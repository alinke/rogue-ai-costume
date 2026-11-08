package org.pixelcade.arcadejacket

// Bluetooth to the jacket: LEDLINK (the Gledopto driving the LED strips),
// ledlink/src/jacket_link.h -- the same protocol as the iPhone app's JacketLink.swift.
// The app finds it by its GATT service and connects:
//   COMMAND (write):                cmd a b c
//   LIVE (write without response):  "M" bass mid treble level beat  |  "H" bpm
//   STATUS (read, notify):          look bright speed flags ackSeq r g b turn bpm
//   LAYOUT (read, write):           the LED layout (settings): see LedLayout
// Android runs one GATT operation at a time, so they're queued; live data is
// dropped rather than queued when the link is busy.
//
// No controller connected = demo mode: every command still goes to the on-screen
// preview (PreviewEngine), which runs the jacket's own looks code.

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.util.ArrayDeque
import java.util.UUID

enum class JacketCommand(val code: Int) { LOOK(1), BRIGHT(2), SPEED(3), COLOR(4), TURN(5), BRAKE(6), POWER(7), NEXT(8) }

enum class Turn(val code: Int) {
    OFF(0), LEFT(1), RIGHT(2), HAZARD(3);
    companion object { fun of(v: Int) = entries.firstOrNull { it.code == v } ?: OFF }
}

data class JacketStatus(
    val look: Int = 0,
    val bright: Int = 60,
    val speed: Int = 100,
    val flags: Int = 1,          // on, until the jacket says otherwise
    val color: Int = 0x00DCFF,
    val turn: Turn = Turn.OFF,
    val bpm: Int = 0,
) {
    val on get() = flags and 1 != 0
    val followingCostume get() = flags and 2 != 0
    val musicLive get() = flags and 4 != 0
    val heartLive get() = flags and 8 != 0
    val braking get() = flags and 16 != 0
}

/** One stretch of the LED chain, in wiring order (jacket_link.h LAYOUT). */
data class LedZone(val kind: Kind, val side: Side, val reversed: Boolean, val count: Int, val key: Long = nextKey++) {
    enum class Kind(val code: Int, val title: String) {
        SLEEVE(0, "Sleeve"), FRONT(1, "Front"), EYE(2, "Eye pod"), MATRIX(3, "8x8 matrix"), DARK(4, "Dark");
        val lit get() = this != DARK
        companion object { fun of(v: Int) = entries.firstOrNull { it.code == v } ?: DARK }
    }
    enum class Side(val code: Int, val title: String) {
        LEFT(-1, "Left"), NONE(0, "None"), RIGHT(1, "Right");
        companion object { fun of(v: Int) = entries.firstOrNull { it.code == v } ?: NONE }
    }
    /** The same zone, ignoring which list item it is. */
    fun sameAs(o: LedZone) = kind == o.kind && side == o.side && reversed == o.reversed && count == o.count

    companion object { private var nextKey = 1L }
}

/** The controller's LED layout and what it says about itself. */
data class LedLayout(
    val zones: List<LedZone> = emptyList(),
    val saved: Boolean = false,         // the app's layout, not the firmware's built-in one
    val matrixSkipped: Boolean = false, // an 8x8 matrix zone isn't wired in right now (no costume)
    val maxLeds: Int = 0,
    val dataPin: Int = 0,
    val maxMilliamps: Int = 0,
    val address: String = "",           // Wi-Fi address ("" without Wi-Fi)
) {
    val total get() = zones.sumOf { it.count }
    val lit get() = zones.filter { it.kind.lit && !(matrixSkipped && it.kind == LedZone.Kind.MATRIX) }.sumOf { it.count }

    /** Where zone i starts on the wire: an 8x8 matrix that isn't wired in takes no room. */
    fun start(i: Int) = zones.take(i).filter { !(matrixSkipped && it.kind == LedZone.Kind.MATRIX) }.sumOf { it.count }

    fun sameZones(o: List<LedZone>) = zones.size == o.size && zones.zip(o).all { (a, b) -> a.sameAs(b) }

    val bytes: ByteArray
        get() {
            val b = ArrayList<Byte>()
            b += 'L'.code.toByte(); b += 1; b += zones.size.toByte()
            for (z in zones) {
                b += z.kind.code.toByte(); b += z.side.code.toByte(); b += (if (z.reversed) 1 else 0).toByte()
                b += (z.count and 255).toByte(); b += (z.count shr 8).toByte()
            }
            return b.toByteArray()
        }

    companion object {
        fun parse(d: ByteArray): LedLayout? {
            val b = d.map { it.toInt() and 255 }
            if (b.size < 11 || b[0] != 'L'.code || b[1] != 1) return null
            val n = b[3]
            if (b.size < 11 + n * 5) return null
            val zones = (0 until n).map { k ->
                val p = 11 + k * 5
                LedZone(LedZone.Kind.of(b[p]), LedZone.Side.of(d[p + 1].toInt()), b[p + 2] != 0, b[p + 3] or (b[p + 4] shl 8))
            }
            return LedLayout(
                zones = zones, saved = b[2] and 1 != 0, matrixSkipped = b[2] and 2 != 0,
                maxLeds = b[6] or (b[7] shl 8), dataPin = b[8], maxMilliamps = b[9] or (b[10] shl 8),
                address = String(d, 11 + n * 5, d.size - 11 - n * 5, Charsets.UTF_8),
            )
        }

        /** Demo mode's layout (no controller): the model jacket the preview draws. */
        val demo = LedLayout(
            zones = listOf(
                LedZone(LedZone.Kind.FRONT, LedZone.Side.RIGHT, false, 78),
                LedZone(LedZone.Kind.FRONT, LedZone.Side.LEFT, true, 77),
                LedZone(LedZone.Kind.DARK, LedZone.Side.NONE, false, 8),
                LedZone(LedZone.Kind.SLEEVE, LedZone.Side.LEFT, true, 113),
                LedZone(LedZone.Kind.DARK, LedZone.Side.NONE, false, 30),
                LedZone(LedZone.Kind.SLEEVE, LedZone.Side.RIGHT, true, 113),
            ),
            maxLeds = 1200, dataPin = 16, maxMilliamps = 2000,
        )
    }
}

enum class TestPattern(val code: Int) { OFF(0), NUMBERED(1), RANGE(2), LIT(3) }

@SuppressLint("MissingPermission")  // every call is behind Permissions.bluetooth (MainActivity)
class JacketLink(private val context: Context) {
    enum class State { BLUETOOTH_OFF, NO_PERMISSION, SEARCHING, CONNECTING, CONNECTED }

    var state by mutableStateOf(State.SEARCHING); private set
    var status by mutableStateOf(JacketStatus())
    /** The controller's once read; null = its firmware has no settings. */
    var layout by mutableStateOf<LedLayout?>(LedLayout.demo); private set
    var controllerName by mutableStateOf(""); private set
    /** No controller connected: the app runs on the on-screen preview, with a sample layout. */
    val demo get() = state != State.CONNECTED

    private val main = Handler(Looper.getMainLooper())
    private val adapter: BluetoothAdapter? = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    private var gatt: BluetoothGatt? = null
    private var commandChar: BluetoothGattCharacteristic? = null
    private var liveChar: BluetoothGattCharacteristic? = null
    private var statusChar: BluetoothGattCharacteristic? = null
    private var layoutChar: BluetoothGattCharacteristic? = null
    private var scanning = false
    private var started = false

    // One GATT operation at a time.
    private val queue = ArrayDeque<() -> Boolean>()
    private var busy = false

    init {
        PreviewEngine.command(JacketCommand.LOOK.code, status.look, 0, 0)
    }

    /** Begins looking for the jacket (once the Bluetooth permissions are granted); scan = false: demo only (screenshots). */
    fun start(scan: Boolean = true) {
        started = scan
        if (scan) search() else state = State.SEARCHING
    }

    // ---- Sending ------------------------------------------------------------------------

    /** A command; the shown status changes at once (the jacket's own status follows). */
    fun send(cmd: JacketCommand, a: Int = 0, b: Int = 0, c: Int = 0) {
        status = when (cmd) {
            JacketCommand.LOOK -> status.copy(look = a, flags = status.flags or 1)
            JacketCommand.BRIGHT -> status.copy(bright = a)
            JacketCommand.SPEED -> status.copy(speed = a)
            JacketCommand.COLOR -> status.copy(color = (a shl 16) or (b shl 8) or c)
            JacketCommand.TURN -> status.copy(turn = Turn.of(a))
            JacketCommand.POWER -> status.copy(flags = if (a != 0) status.flags or 1 else status.flags and 1.inv())
            else -> status
        }
        PreviewEngine.command(cmd.code, a, b, c)  // the preview follows along (it's what demo mode shows)
        val ch = commandChar ?: return
        if (state != State.CONNECTED) return
        write(ch, byteArrayOf(cmd.code.toByte(), a.toByte(), b.toByte(), c.toByte()), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    fun setLook(id: Int) = send(JacketCommand.LOOK, id)

    fun setColor(rgb: Int) = send(JacketCommand.COLOR, (rgb shr 16) and 255, (rgb shr 8) and 255, rgb and 255)

    /** Music levels or a heart rate: dropped rather than queued if the link is busy. Any thread. */
    fun sendLive(bytes: ByteArray) {
        main.post {
            if (bytes.size >= 6 && bytes[0] == 'M'.code.toByte()) {
                PreviewEngine.music(bytes[1].u, bytes[2].u, bytes[3].u, bytes[4].u, bytes[5].u)
            }
            if (bytes.size >= 2 && bytes[0] == 'H'.code.toByte()) PreviewEngine.heart(bytes[1].u)
            val ch = liveChar ?: return@post
            if (state != State.CONNECTED || busy) return@post
            write(ch, bytes, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)
        }
    }

    // ---- Settings -----------------------------------------------------------------------

    fun readLayout() {
        val ch = layoutChar ?: return
        enqueue { gatt?.readCharacteristic(ch) == true }
    }

    /** Saves the layout on the controller (it keeps it), then reads it back. */
    fun saveLayout(l: LedLayout) {
        if (demo) { layout = l; return }
        val ch = layoutChar ?: return
        write(ch, l.bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        readLayout()
    }

    /** Back to the layout built into the firmware. */
    fun resetLayout() {
        if (demo) { layout = LedLayout.demo; return }
        val ch = layoutChar ?: return
        write(ch, byteArrayOf('D'.code.toByte()), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        readLayout()
    }

    /** A test pattern on the LEDs (wire positions a..b for RANGE); OFF goes back to the looks. */
    fun test(t: TestPattern, a: Int = 0, b: Int = 0) {
        val ch = layoutChar ?: return
        if (state != State.CONNECTED) return
        write(ch, byteArrayOf('T'.code.toByte(), t.code.toByte(), (a and 255).toByte(), (a shr 8).toByte(),
            (b and 255).toByte(), (b shr 8).toByte()), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
    }

    // ---- Connecting ---------------------------------------------------------------------

    private fun search() {
        if (!started) return
        if (!Permissions.bluetooth(context)) { state = State.NO_PERMISSION; return }
        val a = adapter
        if (a == null || !a.isEnabled) { state = State.BLUETOOTH_OFF; main.postDelayed({ search() }, 3000); return }
        state = State.SEARCHING
        if (scanning) return
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE)).build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        a.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)
        scanning = true
    }

    private fun stopScan() {
        if (!scanning) return
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
        scanning = false
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            main.post { if (gatt == null) connect(result.device) }
        }
    }

    private fun connect(d: BluetoothDevice) {
        stopScan()
        controllerName = d.name ?: ""
        state = State.CONNECTING
        gatt = d.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    private fun lost() {
        gatt?.close()
        gatt = null
        commandChar = null; liveChar = null; statusChar = null; layoutChar = null
        queue.clear(); busy = false
        layout = LedLayout.demo
        // Walked out of range or the jacket's power went off: look again.
        search()
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            main.post {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    g.requestMtu(185)  // the layout is ~75 bytes
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    lost()
                }
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            main.post { g.discoverServices() }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, st: Int) {
            main.post {
                val svc = g.getService(SERVICE) ?: run { g.disconnect(); return@post }
                commandChar = svc.getCharacteristic(COMMAND)
                liveChar = svc.getCharacteristic(LIVE)
                statusChar = svc.getCharacteristic(STATUS)
                layoutChar = svc.getCharacteristic(LAYOUT)
                if (commandChar == null) { g.disconnect(); return@post }
                statusChar?.let { ch ->
                    g.setCharacteristicNotification(ch, true)
                    ch.getDescriptor(CCCD)?.let { d ->
                        enqueue { writeDescriptor(g, d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) }
                    }
                    enqueue { g.readCharacteristic(ch) }
                }
                if (layoutChar == null) layout = null else readLayout()  // older LEDLINK: no settings
                state = State.CONNECTED
            }
        }

        @Deprecated("Android 12 and older")
        override fun onCharacteristicRead(g: BluetoothGatt, ch: BluetoothGattCharacteristic, st: Int) {
            @Suppress("DEPRECATION") val v = ch.value ?: ByteArray(0)
            main.post { received(ch, v); next() }
        }

        override fun onCharacteristicRead(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray, st: Int) {
            main.post { received(ch, value); next() }
        }

        @Deprecated("Android 12 and older")
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION") val v = ch.value ?: ByteArray(0)
            main.post { received(ch, v) }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray) {
            main.post { received(ch, value) }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, ch: BluetoothGattCharacteristic, st: Int) {
            main.post { next() }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, st: Int) {
            main.post { next() }
        }
    }

    private fun received(ch: BluetoothGattCharacteristic, d: ByteArray) {
        when (ch.uuid) {
            STATUS -> if (d.size >= 10) {
                val b = d.map { it.toInt() and 255 }
                status = JacketStatus(b[0], b[1], b[2], b[3], (b[5] shl 16) or (b[6] shl 8) or b[7], Turn.of(b[8]), b[9])
            }
            LAYOUT -> layout = LedLayout.parse(d)
        }
    }

    // ---- The GATT queue -----------------------------------------------------------------

    private fun write(ch: BluetoothGattCharacteristic, v: ByteArray, type: Int) {
        if (type == BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) {
            // Not queued: fire and forget (the stack calls back when it's sent).
            busy = true
            if (!writeChar(ch, v, type)) busy = false
            return
        }
        enqueue { writeChar(ch, v, type) }
    }

    private fun writeChar(ch: BluetoothGattCharacteristic, v: ByteArray, type: Int): Boolean {
        val g = gatt ?: return false
        return if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(ch, v, type) == BluetoothGatt.GATT_SUCCESS
        } else {
            @Suppress("DEPRECATION")
            ch.writeType = type
            @Suppress("DEPRECATION")
            ch.value = v
            @Suppress("DEPRECATION")
            g.writeCharacteristic(ch)
        }
    }

    private fun writeDescriptor(g: BluetoothGatt, d: BluetoothGattDescriptor, v: ByteArray): Boolean =
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeDescriptor(d, v) == BluetoothGatt.GATT_SUCCESS
        } else {
            @Suppress("DEPRECATION")
            d.value = v
            @Suppress("DEPRECATION")
            g.writeDescriptor(d)
        }

    private fun enqueue(op: () -> Boolean) {
        queue.add(op)
        if (!busy) next()
    }

    private fun next() {
        busy = false
        while (queue.isNotEmpty()) {
            val op = queue.poll() ?: break
            if (op()) { busy = true; return }  // wait for its callback
        }
    }

    companion object {
        val SERVICE: UUID = UUID.fromString("524A4B31-0000-4C41-4237-000000000000")
        val COMMAND: UUID = UUID.fromString("524A4B31-0001-4C41-4237-000000000000")
        val LIVE: UUID = UUID.fromString("524A4B31-0002-4C41-4237-000000000000")
        val STATUS: UUID = UUID.fromString("524A4B31-0003-4C41-4237-000000000000")
        val LAYOUT: UUID = UUID.fromString("524A4B31-0004-4C41-4237-000000000000")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}

private val Byte.u get() = toInt() and 255
