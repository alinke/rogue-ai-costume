package org.pixelcade.arcadejacket.ui

// Demo mode: with no controller connected, a drawn jacket shows the look -- run by the
// jacket's own looks code (PreviewEngine), so every button does what it would on the
// real thing -- and "What you need" says what it takes to light a real one.

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameMillis
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.pixelcade.arcadejacket.JacketLink
import org.pixelcade.arcadejacket.PreviewEngine
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min

/** The banner above the tabs while no controller is connected. */
@Composable
fun DemoBanner(onHelp: () -> Unit, link: JacketLink) {
    Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp).padding(bottom = 8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("DEMO", color = Color.Black, fontSize = 11.sp, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace,
                modifier = Modifier.background(Orange, CircleShape).padding(horizontal = 6.dp, vertical = 1.dp))
            Text("No jacket connected: a preview", color = Dim, fontSize = 12.sp, modifier = Modifier.weight(1f))
            TextButton(onClick = onHelp) { Text("What you need", color = Neon, fontSize = 12.sp, fontWeight = FontWeight.SemiBold) }
        }
        JacketPreview(link, Modifier.fillMaxWidth().height(190.dp))
    }
}

/** The model jacket, front view, its LEDs lit by the looks code every frame. */
@Composable
fun JacketPreview(link: JacketLink, modifier: Modifier = Modifier) {
    var frame by remember { mutableLongStateOf(0L) }
    LaunchedEffect(Unit) { while (true) withFrameMillis { frame = it } }
    val model = PreviewModel
    Canvas(modifier) {
        frame  // redraw every frame
        val colors = model.frame()
        val side = min(size.width, size.height)
        val ox = (size.width - side) / 2
        val oy = (size.height - side) / 2
        fun at(x: Float, y: Float) = Offset(ox + x * side, oy + y * side)

        // The jacket.
        val body = Path()
        SILHOUETTE.forEachIndexed { i, p -> val q = at(p.first, p.second); if (i == 0) body.moveTo(q.x, q.y) else body.lineTo(q.x, q.y) }
        body.close()
        drawPath(body, Color.White.copy(alpha = 0.07f))
        drawPath(body, Color.White.copy(alpha = 0.18f), style = Stroke(1.dp.toPx()))

        // The LEDs: a soft glow, then the dots.
        val k = 0.6f + 0.4f * link.status.bright / 100f
        val r = max(1.5f, side / 190f)
        for (i in colors.indices) {
            val c = colors[i]
            val q = at(model.xs[i], model.ys[i])
            if (c == 0) {  // an unlit LED: just the strip
                drawCircle(Color.White.copy(alpha = 0.08f), r / 2, q)
                continue
            }
            val color = Color(0xFF000000.toInt() or c).copy(alpha = k)
            drawCircle(color.copy(alpha = k * 0.25f), r * 2.6f, q)
            drawCircle(color.copy(alpha = k * 0.45f), r * 1.6f, q)
            drawCircle(color, r, q)
        }
    }
}

/** Where the model's LEDs sit on the drawing (0..1, front view: the wearer's right is on the left). */
private object PreviewModel {
    val xs: FloatArray
    val ys: FloatArray
    private val buffer: IntArray

    // Hem up to the collar (the wearer's right edge of the zipper), and shoulder down
    // to the cuff (their right sleeve); the left side mirrors them.
    private val FRONT = listOf(0.47f to 0.96f, 0.47f to 0.34f, 0.44f to 0.2f, 0.39f to 0.09f)
    private val SLEEVE = listOf(0.29f to 0.13f, 0.2f to 0.42f, 0.11f to 0.86f)

    init {
        val n = PreviewEngine.count()
        buffer = IntArray(n)
        xs = FloatArray(n)
        ys = FloatArray(n)
        val v = FloatArray(3)
        for (i in 0 until n) {
            PreviewEngine.pixel(i, v)
            val p = along(if (v[0] == 1f) SLEEVE else FRONT, v[2])
            xs[i] = if (v[1] < 0) 1 - p.first else p.first  // the wearer's left is on the viewer's right
            ys[i] = p.second
        }
    }

    fun frame(): IntArray {
        PreviewEngine.render(buffer)
        return buffer
    }

    /** The point a fraction t (0..1) of the way along a polyline. */
    private fun along(pts: List<Pair<Float, Float>>, t: Float): Pair<Float, Float> {
        val lens = pts.zipWithNext { a, b -> hypot(b.first - a.first, b.second - a.second) }
        var d = t.coerceIn(0f, 1f) * lens.sum()
        for ((k, len) in lens.withIndex()) {
            if (d <= len || k == lens.size - 1) {
                val f = if (len > 0) min(d / len, 1f) else 0f
                val a = pts[k]; val b = pts[k + 1]
                return (a.first + (b.first - a.first) * f) to (a.second + (b.second - a.second) * f)
            }
            d -= len
        }
        return pts.last()
    }
}

private val SILHOUETTE = listOf(
    0.38f to 0.07f, 0.27f to 0.11f, 0.22f to 0.2f, 0.12f to 0.5f, 0.04f to 0.86f, 0.16f to 0.9f, 0.24f to 0.56f,
    0.27f to 0.4f, 0.28f to 0.98f, 0.72f to 0.98f, 0.73f to 0.4f, 0.76f to 0.56f, 0.84f to 0.9f, 0.96f to 0.86f,
    0.88f to 0.5f, 0.78f to 0.2f, 0.73f to 0.11f, 0.62f to 0.07f, 0.5f to 0.15f,
)
