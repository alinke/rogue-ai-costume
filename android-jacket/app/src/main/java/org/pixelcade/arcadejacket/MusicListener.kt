package org.pixelcade.arcadejacket

// The music modes: the phone's microphone hears the music around you, an FFT splits
// it into bass / mids / treble, and a beat is a jump in the bass over its recent
// average. ~43 times a second the levels go to the jacket (JacketLink.sendLive, "M"
// bass mid treble level beat) -- nothing is recorded or kept. The same analysis as the
// iPhone app's MusicListener.swift.
//
// Each band is scaled against its own recent peak (automatic gain), so a quiet room
// and a loud concert both use the jacket's whole range.

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Handler
import android.os.Looper
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

class MusicListener {
    var running by mutableStateOf(false); private set
    var bass by mutableFloatStateOf(0f); private set
    var mid by mutableFloatStateOf(0f); private set
    var treble by mutableFloatStateOf(0f); private set
    var beats by mutableIntStateOf(0); private set

    /** Called on the audio thread with each frame's bytes (bass, mid, treble, level, beat). */
    var onLevels: ((ByteArray) -> Unit)? = null

    private val main = Handler(Looper.getMainLooper())
    @Volatile private var thread: Thread? = null

    private val n = 1024
    private val rate = 44100
    private val window = FloatArray(n) { (0.5 - 0.5 * cos(2 * PI * it / (n - 1))).toFloat() }
    private val peaks = floatArrayOf(1e-4f, 1e-4f, 1e-4f, 1e-4f)  // per band: a slowly falling peak (auto gain)
    private val bassHistory = ArrayDeque<Float>()
    private var lastBeat = 0L
    private var uiTick = 0

    /** Needs the microphone permission (MainActivity asks first). */
    @SuppressLint("MissingPermission")
    fun start() {
        if (running) return
        val min = AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT)
        // UNPROCESSED: no automatic gain or noise suppression on the mic.
        val rec = runCatching {
            AudioRecord(MediaRecorder.AudioSource.UNPROCESSED, rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT, max(min, n * 8))
        }.getOrNull()?.takeIf { it.state == AudioRecord.STATE_INITIALIZED }
            ?: runCatching {
                AudioRecord(MediaRecorder.AudioSource.MIC, rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_FLOAT, max(min, n * 8))
            }.getOrNull()?.takeIf { it.state == AudioRecord.STATE_INITIALIZED }
            ?: return
        running = true
        thread = Thread {
            val buf = FloatArray(n)
            rec.startRecording()
            while (thread === Thread.currentThread()) {
                var got = 0
                while (got < n) {
                    val r = rec.read(buf, got, n - got, AudioRecord.READ_BLOCKING)
                    if (r <= 0) break
                    got += r
                }
                if (got == n) process(buf)
            }
            rec.stop()
            rec.release()
        }.apply { name = "music"; start() }
    }

    fun stop() {
        thread = null
        running = false
    }

    private val re = FloatArray(n)
    private val im = FloatArray(n)
    private val mags = FloatArray(n / 2)

    private fun process(samples: FloatArray) {
        for (i in 0 until n) { re[i] = samples[i] * window[i]; im[i] = 0f }
        fft(re, im)
        for (i in 0 until n / 2) mags[i] = re[i] * re[i] + im[i] * im[i]
        val hz = rate.toFloat() / n
        fun band(lo: Float, hi: Float): Float {
            val a = max(1, (lo / hz).toInt())
            val b = min(n / 2 - 1, (hi / hz).toInt())
            if (b <= a) return 0f
            var s = 0f
            for (i in a..b) s += mags[i]
            return sqrt(s / (b - a + 1))
        }
        val raw = floatArrayOf(band(30f, 160f), band(160f, 2000f), band(2000f, 9000f), band(30f, 9000f))
        val out = FloatArray(4)
        for (i in 0 until 4) {
            peaks[i] = maxOf(raw[i], peaks[i] * 0.997f, 1e-4f)  // falls ~13% a second
            out[i] = min(1f, raw[i] / peaks[i])
            out[i] = out[i] * out[i]                              // more contrast
        }
        // A beat: the bass well above its last ~0.7 s, not too soon after the last.
        bassHistory.addLast(raw[0])
        if (bassHistory.size > 32) bassHistory.removeFirst()
        val avg = bassHistory.sum() / bassHistory.size
        val now = System.currentTimeMillis()
        val beat = raw[0] > avg * 1.45f && raw[0] > peaks[0] * 0.3f && now - lastBeat > 250
        if (beat) lastBeat = now
        onLevels?.invoke(byteArrayOf((out[0] * 255).toInt().toByte(), (out[1] * 255).toInt().toByte(),
            (out[2] * 255).toInt().toByte(), (out[3] * 255).toInt().toByte(), if (beat) 1 else 0))
        uiTick++
        if (beat || uiTick % 3 == 0) {
            val (b, m, t) = Triple(out[0], out[1], out[2])
            main.post {
                bass = b; mid = m; treble = t
                if (beat) beats++
            }
        }
    }

    /** In-place radix-2 FFT (n a power of two). */
    private fun fft(re: FloatArray, im: FloatArray) {
        val n = re.size
        var j = 0
        for (i in 1 until n) {
            var bit = n shr 1
            while (j and bit != 0) { j = j xor bit; bit = bit shr 1 }
            j = j xor bit
            if (i < j) {
                var t = re[i]; re[i] = re[j]; re[j] = t
                t = im[i]; im[i] = im[j]; im[j] = t
            }
        }
        var len = 2
        while (len <= n) {
            val ang = -2 * PI / len
            val wr = cos(ang).toFloat()
            val wi = sin(ang).toFloat()
            var i = 0
            while (i < n) {
                var cr = 1f
                var ci = 0f
                for (k in 0 until len / 2) {
                    val ar = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci
                    val ai = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr
                    re[i + k + len / 2] = re[i + k] - ar
                    im[i + k + len / 2] = im[i + k] - ai
                    re[i + k] += ar
                    im[i + k] += ai
                    val ncr = cr * wr - ci * wi
                    ci = cr * wi + ci * wr
                    cr = ncr
                }
                i += len
            }
            len = len shl 1
        }
    }
}
