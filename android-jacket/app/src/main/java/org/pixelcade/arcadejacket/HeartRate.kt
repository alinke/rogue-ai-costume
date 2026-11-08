package org.pixelcade.arcadejacket

// Your heart rate, for the HEARTBEAT look, from a Bluetooth heart-rate sensor:
// anything with the standard Heart Rate service (0x180D) -- a Fitbit Charge 6 or
// Pixel Watch 2-4 with heart-rate sharing on (older Fitbits can't), Polar / Wahoo /
// Garmin chest straps and arm bands, many watches' "broadcast heart rate".

import android.annotation.SuppressLint
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
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.util.UUID

data class HeartSensor(val address: String, val name: String)

@SuppressLint("MissingPermission")  // used only once the Bluetooth permissions are granted
class HeartRate(private val context: Context) {
    var bpm by mutableStateOf<Int?>(null); private set
    var source by mutableStateOf(""); private set
    val sensors = mutableStateListOf<HeartSensor>()
    var scanning by mutableStateOf(false); private set
    var connectedName by mutableStateOf<String?>(null); private set

    /** Each reading, to the jacket. */
    var onBpm: ((Int) -> Unit)? = null

    private val main = Handler(Looper.getMainLooper())
    private val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    private val prefs = context.getSharedPreferences("heart", Context.MODE_PRIVATE)
    private val found = HashMap<String, BluetoothDevice>()
    private var gatt: BluetoothGatt? = null
    private var lastReading = 0L

    init {
        // No reading for 10 s: show "no heart rate" again.
        main.postDelayed(object : Runnable {
            override fun run() {
                if (bpm != null && System.currentTimeMillis() - lastReading > 10_000) bpm = null
                main.postDelayed(this, 2000)
            }
        }, 2000)
    }

    /** Reconnects to the sensor used last time (once the Bluetooth permissions are granted). */
    fun start() {
        val address = prefs.getString("sensor", null) ?: return
        if (gatt != null || adapter == null) return
        runCatching { adapter.getRemoteDevice(address) }.getOrNull()?.let { open(it, auto = true) }
    }

    fun startScan() {
        val s = adapter?.bluetoothLeScanner ?: return
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE)).build()
        s.startScan(listOf(filter), ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCallback)
        scanning = true
    }

    fun stopScan() {
        if (!scanning) return
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
        scanning = false
    }

    fun connect(s: HeartSensor) {
        val d = found[s.address] ?: return
        stopScan()
        gatt?.close()
        prefs.edit().putString("sensor", s.address).apply()
        open(d, auto = false)
    }

    fun forget() {
        gatt?.close()
        gatt = null
        connectedName = null
        prefs.edit().remove("sensor").apply()
    }

    private fun open(d: BluetoothDevice, auto: Boolean) {
        // autoConnect: back as soon as it's in range again, for as long as it's remembered.
        gatt = d.connectGatt(context, auto, callback, BluetoothDevice.TRANSPORT_LE)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val d = result.device
            main.post {
                found[d.address] = d
                if (sensors.none { it.address == d.address }) sensors += HeartSensor(d.address, d.name ?: "Heart-rate sensor")
            }
        }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            main.post {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    connectedName = g.device.name ?: "Heart-rate sensor"
                    g.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    connectedName = null
                    // Remembered: reconnect when it's back in range.
                    if (gatt == g && prefs.getString("sensor", null) == g.device.address) {
                        g.close(); open(g.device, auto = true)
                    }
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val ch = g.getService(SERVICE)?.getCharacteristic(MEASUREMENT) ?: return
            g.setCharacteristicNotification(ch, true)
            val d = ch.getDescriptor(CCCD) ?: return
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeDescriptor(d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION")
                d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                g.writeDescriptor(d)
            }
        }

        @Deprecated("Android 12 and older")
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION") measured(g, ch.value ?: return)
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray) {
            measured(g, value)
        }
    }

    /** Heart Rate Measurement: flags, then the rate as 8 bits (flag bit 0 clear) or 16 bits little-endian (set). */
    private fun measured(g: BluetoothGatt, d: ByteArray) {
        if (d.size < 2) return
        val b = d.map { it.toInt() and 255 }
        val v = if (b[0] and 1 == 0) b[1] else if (d.size >= 3) b[1] or (b[2] shl 8) else 0
        if (v !in 30..230) return
        val name = g.device.name ?: "Sensor"
        main.post {
            lastReading = System.currentTimeMillis()
            bpm = v
            source = name
            onBpm?.invoke(v)
        }
    }

    companion object {
        val SERVICE: UUID = UUID.fromString("0000180d-0000-1000-8000-00805f9b34fb")
        val MEASUREMENT: UUID = UUID.fromString("00002a37-0000-1000-8000-00805f9b34fb")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}
