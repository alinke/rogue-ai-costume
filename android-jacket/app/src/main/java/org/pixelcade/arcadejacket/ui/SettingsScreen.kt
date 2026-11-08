package org.pixelcade.arcadejacket.ui

// Settings: the LED controller (LEDLINK on a Gledopto), and its LED layout -- the zones
// along the chain in wiring order, which are lit and which stay dark -- with test patterns
// to check it against the real strips. The layout comes from the controller itself (the
// one built into its firmware until it's saved here), so a working setup shows up as it
// is. The same page as the iPhone app's SettingsView.swift; also "What you need" and the
// layout help.

import android.content.Intent
import android.net.Uri
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.AddCircle
import androidx.compose.material.icons.filled.ArrowDownward
import androidx.compose.material.icons.filled.ArrowUpward
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.ErrorOutline
import androidx.compose.material.icons.filled.HelpOutline
import androidx.compose.material.icons.filled.Lightbulb
import androidx.compose.material.icons.filled.Numbers
import androidx.compose.material.icons.filled.StopCircle
import androidx.compose.material.icons.outlined.Lightbulb
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.pixelcade.arcadejacket.JacketLink
import org.pixelcade.arcadejacket.LedLayout
import org.pixelcade.arcadejacket.LedZone
import org.pixelcade.arcadejacket.TestPattern

const val PRIVACY_URL = "https://pixelcade.org/app-privacy/"
const val GUIDE_URL = "https://alinke.github.io/rogue-ai-costume/"
const val SOURCE_URL = "https://github.com/alinke/rogue-ai-costume"

@Composable
fun SettingsScreen(link: JacketLink, close: () -> Unit) {
    var help by remember { mutableStateOf(false) }
    if (help) {
        LayoutHelp { help = false }
        return
    }
    BackHandler(onBack = close)
    val zones = remember { mutableStateListOf<LedZone>() }
    var loadedFrom by remember { mutableStateOf<LedLayout?>(null) }
    var testing by remember { mutableStateOf<String?>(null) }
    var rangeFrom by remember { mutableStateOf("0") }
    var rangeTo by remember { mutableStateOf("9") }
    var confirmReset by remember { mutableStateOf(false) }

    // Takes the controller's layout, unless there are unsaved edits to keep.
    LaunchedEffect(link.layout) {
        val l = link.layout ?: return@LaunchedEffect
        val from = loadedFrom
        if (from == null || from.sameZones(zones)) { zones.clear(); zones.addAll(l.zones) }
        loadedFrom = l
    }
    LaunchedEffect(Unit) { link.readLayout() }
    DisposableEffect(Unit) { onDispose { if (testing != null) link.test(TestPattern.OFF) } }
    val edited = loadedFrom?.let { !it.sameZones(zones) } ?: false
    val layout = link.layout

    Column(Modifier.fillMaxSize().background(Color.Black)) {
        TopBar("Settings", close)
        LazyColumn(Modifier.weight(1f), contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            // ---- Controller
            item { SectionTitle("Controller") }
            item {
                Card {
                    val detected = link.state == JacketLink.State.CONNECTED
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(if (detected) Icons.Filled.CheckCircle else Icons.Filled.ErrorOutline, null,
                            tint = if (detected) Color.Green else Orange, modifier = Modifier.size(28.dp))
                        Spacer(Modifier.width(12.dp))
                        Column {
                            Text(if (detected) "Gledopto controller detected" else "No controller found", fontWeight = FontWeight.SemiBold)
                            Text(detail(link), color = Dim, fontSize = 12.sp)
                        }
                    }
                    if (link.demo) Text("Demo: the zones below are a sample. Edits and tests do nothing until a controller is connected.", color = Orange, fontSize = 12.sp)
                    if (layout != null && !link.demo) {
                        Info("Name", link.controllerName.ifEmpty { "LEDLINK" })
                        Info("Zones", "${layout.zones.size}")
                        Info("LEDs", "${layout.total} on the chain, ${layout.lit} lit")
                        Info("Data pin", "GPIO${layout.dataPin}")
                        Info("Current limit", String.format("%.1f A", layout.maxMilliamps / 1000.0))
                        Info("Wi-Fi", layout.address.ifEmpty { "not connected" })
                        Info("Layout", if (layout.saved) "set in this app" else "built into the firmware")
                    }
                }
            }
            item { NavRow(Icons.Filled.HelpOutline, "How the LED layout works") { help = true } }
            if (layout != null) {
                // ---- Test
                item { SectionTitle("Test") }
                item {
                    Card {
                        TestRow(Icons.Filled.Numbers, "Number every LED", testing == "numbered") { testing = "numbered"; link.test(TestPattern.NUMBERED) }
                        TestRow(Icons.Filled.Lightbulb, "Show the lit LEDs", testing == "lit") { testing = "lit"; link.test(TestPattern.LIT) }
                        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Text("LEDs")
                            NumberField(rangeFrom, { rangeFrom = it }, Modifier.width(76.dp))
                            Text("to")
                            NumberField(rangeTo, { rangeTo = it }, Modifier.width(76.dp))
                            Spacer(Modifier.weight(1f))
                            OutlinedButton(onClick = {
                                val a = rangeFrom.toIntOrNull()?.coerceAtLeast(0) ?: 0
                                testing = "range"
                                link.test(TestPattern.RANGE, a, maxOf(a, rangeTo.toIntOrNull() ?: a))
                            }) { Text("Light") }
                        }
                        if (testing != null) {
                            TextButton(onClick = { testing = null; link.test(TestPattern.OFF) }) {
                                Icon(Icons.Filled.StopCircle, null, tint = Color.Red); Spacer(Modifier.width(6.dp)); Text("Stop the test", color = Color.Red)
                            }
                        }
                    }
                }
                item {
                    Footnote("Numbered: the first LED green, every 10th white, every 50th red, every 100th blue. Lit: exactly what the zones below light, in dim white. LED numbers count along the wire from 0. The test stops when you leave this page.")
                }

                // ---- Zones
                item { SectionTitle("Zones, in wiring order") }
                val shown = layout.copy(zones = zones.toList())
                itemsIndexed(zones, key = { _, z -> z.key }) { i, z ->
                    ZoneRow(z, i + 1, shown.start(i), layout.matrixSkipped && z.kind == LedZone.Kind.MATRIX, testing == "zone$i",
                        change = { zones[i] = it },
                        light = {
                            testing = "zone$i"
                            val a = shown.start(i)
                            link.test(TestPattern.RANGE, a, a + maxOf(z.count, 1) - 1)
                        },
                        moveUp = if (i > 0) ({ zones.add(i - 1, zones.removeAt(i)) }) else null,
                        moveDown = if (i < zones.size - 1) ({ zones.add(i + 1, zones.removeAt(i)) }) else null,
                        delete = { zones.removeAt(i) })
                }
                if (zones.size < 16) item {
                    TextButton(onClick = { zones.add(LedZone(LedZone.Kind.FRONT, LedZone.Side.NONE, false, 10)) }) {
                        Icon(Icons.Filled.AddCircle, null); Spacer(Modifier.width(6.dp)); Text("Add a zone")
                    }
                }
                item {
                    Footnote("Each zone is a stretch of the chain, starting from the controller. Front and Sleeve are lit (the looks run up the front and down the sleeves); Dark stays off -- strip tucked out of sight or past the end of a run. Left and Right pick which turn signal a zone shows. Light a zone to check where it really is, then save.")
                }

                // ---- Save
                val total = zones.sumOf { it.count }
                val tooMany = total > maxOf(layout.maxLeds, layout.total)
                item {
                    Card {
                        TextButton(onClick = { link.saveLayout(layout.copy(zones = zones.toList())); loadedFrom = null },
                            enabled = edited && zones.isNotEmpty() && !tooMany) { Text("Save to the controller") }
                        if (edited) TextButton(onClick = { zones.clear(); zones.addAll(layout.zones) }) { Text("Undo changes") }
                        if (layout.saved) TextButton(onClick = { confirmReset = true }) { Text("Reset to the firmware's layout", color = Color.Red) }
                    }
                }
                if (tooMany) item { Text("Too many LEDs: this controller takes up to ${layout.maxLeds}.", color = Color.Red, fontSize = 12.sp) }
            }

            // ---- About
            item { SectionTitle("About") }
            item {
                Card {
                    LinkRow("Privacy policy", PRIVACY_URL)
                    LinkRow("Source code and build guide", SOURCE_URL)
                }
            }
        }
    }
    if (confirmReset) {
        AlertDialog(onDismissRequest = { confirmReset = false },
            title = { Text("Reset layout?") },
            text = { Text("Go back to the layout built into the controller's firmware?") },
            confirmButton = { TextButton(onClick = { confirmReset = false; link.resetLayout() }) { Text("Reset layout", color = Color.Red) } },
            dismissButton = { TextButton(onClick = { confirmReset = false }) { Text("Cancel") } })
    }
}

private fun detail(link: JacketLink) = when (link.state) {
    JacketLink.State.BLUETOOTH_OFF -> "Bluetooth is off"
    JacketLink.State.NO_PERMISSION -> "Allow Bluetooth (Nearby devices) to find it"
    JacketLink.State.SEARCHING -> "Looking over Bluetooth -- is it powered and running LEDLINK? Below: a sample layout (demo)"
    JacketLink.State.CONNECTING -> "Found it, connecting..."
    JacketLink.State.CONNECTED -> if (link.layout == null) "Connected, but its firmware has no settings: update LEDLINK" else "Connected over Bluetooth"
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ZoneRow(z: LedZone, number: Int, start: Int, skipped: Boolean, testing: Boolean, change: (LedZone) -> Unit,
                    light: () -> Unit, moveUp: (() -> Unit)?, moveDown: (() -> Unit)?, delete: () -> Unit) {
    var count by remember(z.key) { mutableStateOf(z.count.toString()) }
    LaunchedEffect(z.count) { if (count.toIntOrNull() != z.count) count = z.count.toString() }
    Card {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Box(Modifier.size(24.dp).background(if (z.kind.lit) Neon.copy(alpha = 0.3f) else Color.Gray.copy(alpha = 0.3f), CircleShape),
                contentAlignment = Alignment.Center) { Text("$number", fontSize = 11.sp, fontWeight = FontWeight.Bold) }
            Menu(z.kind.title, LedZone.Kind.entries.map { it.title }) { change(z.copy(kind = LedZone.Kind.entries[it])) }
            if (z.kind == LedZone.Kind.FRONT || z.kind == LedZone.Kind.SLEEVE) {
                Menu(z.side.title, LedZone.Side.entries.map { it.title }) { change(z.copy(side = LedZone.Side.entries[it])) }
            }
            Spacer(Modifier.weight(1f))
            IconButton(onClick = light, enabled = !skipped && z.count > 0) {
                Icon(if (testing) Icons.Filled.Lightbulb else Icons.Outlined.Lightbulb, "Light zone $number", tint = if (testing) Color.Green else Neon)
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            NumberField(count, { count = it; it.toIntOrNull()?.let { n -> change(z.copy(count = n.coerceIn(0, 1200))) } }, Modifier.width(84.dp))
            OutlinedButton(onClick = { change(z.copy(count = maxOf(0, z.count - 1))) }) { Text("-") }
            OutlinedButton(onClick = { change(z.copy(count = minOf(1200, z.count + 1))) }) { Text("+") }
            Spacer(Modifier.weight(1f))
            Text(if (skipped) "not wired now" else if (z.count > 0) "$start-${start + z.count - 1}" else "empty",
                color = Dim, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
        }
        // Where the zone's first LED is, not reversed / reversed (leds.cpp: a sleeve runs cuff -> shoulder unless reversed).
        val ends = when (z.kind) {
            LedZone.Kind.FRONT -> "Starts at the hem" to "Starts at the collar"
            LedZone.Kind.SLEEVE -> "Starts at the cuff" to "Starts at the shoulder"
            else -> null
        }
        if (ends != null) {
            SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                SegmentedButton(!z.reversed, { change(z.copy(reversed = false)) }, SegmentedButtonDefaults.itemShape(0, 2)) { Text(ends.first, fontSize = 12.sp) }
                SegmentedButton(z.reversed, { change(z.copy(reversed = true)) }, SegmentedButtonDefaults.itemShape(1, 2)) { Text(ends.second, fontSize = 12.sp) }
            }
        }
        Row {
            moveUp?.let { IconButton(onClick = it) { Icon(Icons.Filled.ArrowUpward, "Move up", tint = Dim) } }
            moveDown?.let { IconButton(onClick = it) { Icon(Icons.Filled.ArrowDownward, "Move down", tint = Dim) } }
            Spacer(Modifier.weight(1f))
            IconButton(onClick = delete) { Icon(Icons.Filled.Delete, "Delete zone $number", tint = Dim) }
        }
    }
}

// ---- Help --------------------------------------------------------------------------------

/** Help: what a zone is, lit and dark, sides, direction, and how to check it all with the tests. */
@Composable
private fun LayoutHelp(close: () -> Unit) {
    BackHandler(onBack = close)
    Column(Modifier.fillMaxSize().background(Color.Black)) {
        TopBar("LED layout", close)
        LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            val topics = listOf(
                "One long chain" to "Your strips are wired one after another into one chain, starting at the controller. The LEDs are numbered along the wire from 0: the first LED after the controller is 0, the next 1, and so on to the end of the last strip.",
                "Zones" to "A zone is one stretch of that chain -- a run up the front, a sleeve, or a piece of strip you don't want lit. The zones are listed in wiring order, and each one's count says how many LEDs it has. Together they add up to the whole chain.",
                "Lit and dark" to "Front and Sleeve zones light up: the looks run up the front (hem to collar) and down the sleeves (shoulder to cuff). A Dark zone stays off -- strip tucked inside the jacket, the bit that runs across the back from one sleeve to the next, or strip left over past the end of a run. The 8x8 matrix and Eye pod are for the ROGUE.AI costume; leave them out otherwise.",
                "Left and right" to "Left and Right are the wearer's left and right (as you wear it, not as someone facing you sees it). They decide which turn signal a zone shows; the hazard lights use both.",
                "Which end is first" to "LEDs are numbered from the end where the data wire comes in. \"Starts at the hem\" or \"at the collar\" (Front), and \"at the cuff\" or \"at the shoulder\" (Sleeve), tell the jacket which end that is, so the looks run the right way: flames climb from the cuffs, code rain falls from the shoulders, a turn signal sweeps out to the cuff. If a look runs backwards on one zone, flip that setting.",
                "Checking it" to "Number every LED shows the numbering: LED 0 green, every 10th white, every 50th red, every 100th blue -- count along to find where each zone really starts and ends. Each zone's light button lights just that zone, so you can see it lands exactly on the part it should. Show the lit LEDs lights everything that isn't dark, so stray lit LEDs inside the jacket stand out.",
                "Saving" to "Changes only reach the jacket when you tap Save to the controller; it keeps the layout from then on, even when it's switched off. Reset to the firmware's layout goes back to the one it came with.",
            )
            for ((title, text) in topics) item {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text(title, fontWeight = FontWeight.SemiBold, fontSize = 17.sp)
                    Text(text, color = Dim, fontSize = 14.sp)
                }
            }
        }
    }
}

/** What it takes to light a real jacket. */
@Composable
fun WhatYouNeed(close: () -> Unit) {
    BackHandler(onBack = close)
    Column(Modifier.fillMaxSize().background(Color.Black)) {
        TopBar("What you need", close)
        LazyColumn(contentPadding = PaddingValues(16.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            item {
                Text("Arcade Jacket is the remote for a DIY LED jacket: you wire addressable LED strips into a jacket and drive them with a small Wi-Fi/Bluetooth LED controller. Until one is connected, the app runs this preview.")
            }
            val items = listOf(
                "A jacket with LED strips" to "Addressable strips (WS2811 or WS2812B) along the front and down the sleeves, wired one after another into a single chain.",
                "A Gledopto ESP32 LED controller" to "The small ESP32 controllers sold for WLED, powered from a USB power bank.",
                "The free LEDLINK firmware" to "Open source; install it on the controller from the web flasher (it replaces WLED). A controller still running WLED won't show up in the app.",
                "Then just open the app" to "It finds the controller over Bluetooth by itself, no pairing. Tell it how your strips are laid out in Settings.",
            )
            for ((title, text) in items) item {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(title, fontWeight = FontWeight.SemiBold, fontSize = 17.sp)
                    Text(text, color = Dim, fontSize = 14.sp)
                }
            }
            item {
                Card {
                    LinkRow("Web flasher and build guide", GUIDE_URL)
                    LinkRow("Source code (GitHub)", SOURCE_URL)
                }
            }
        }
    }
}

// ---- Bits --------------------------------------------------------------------------------

@Composable
private fun TopBar(title: String, close: () -> Unit) {
    Row(Modifier.fillMaxWidth().padding(4.dp), verticalAlignment = Alignment.CenterVertically) {
        IconButton(onClick = close) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Back") }
        Text(title, fontSize = 18.sp, fontWeight = FontWeight.SemiBold)
    }
    HorizontalDivider(color = Color.White.copy(alpha = 0.1f))
}

@Composable
private fun SectionTitle(t: String) = Text(t.uppercase(), color = Dim, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)

@Composable
private fun Footnote(t: String) = Text(t, color = Dim, fontSize = 12.sp)

@Composable
private fun Card(content: @Composable () -> Unit) {
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel).padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)) { content() }
}

@Composable
private fun Info(label: String, value: String) {
    Row {
        Text(label, modifier = Modifier.weight(1f))
        Text(value, color = Dim)
    }
}

@Composable
private fun NavRow(icon: ImageVector, title: String, open: () -> Unit) {
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel).clickable(onClick = open).padding(14.dp),
        verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, tint = Neon); Spacer(Modifier.width(10.dp)); Text(title, modifier = Modifier.weight(1f)); Icon(Icons.Filled.ChevronRight, null, tint = Dim)
    }
}

@Composable
private fun TestRow(icon: ImageVector, title: String, on: Boolean, run: () -> Unit) {
    Row(Modifier.fillMaxWidth().clickable(onClick = run).padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, tint = Neon); Spacer(Modifier.width(10.dp)); Text(title, color = Neon, modifier = Modifier.weight(1f))
        if (on) Text("ON", color = Color.Green, fontWeight = FontWeight.Bold, fontSize = 12.sp)
    }
}

@Composable
private fun LinkRow(title: String, url: String) {
    val context = LocalContext.current
    Text(title, color = Neon, modifier = Modifier.fillMaxWidth().clickable {
        context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    }.padding(vertical = 6.dp))
}

@Composable
private fun NumberField(value: String, change: (String) -> Unit, modifier: Modifier) {
    OutlinedTextField(value, { s -> change(s.filter { it.isDigit() }.take(4)) }, modifier, singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number))
}

@Composable
private fun Menu(label: String, options: List<String>, pick: (Int) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box {
        TextButton(onClick = { open = true }) { Text("$label ▾") }
        DropdownMenu(open, { open = false }) {
            options.forEachIndexed { i, o -> DropdownMenuItem(text = { Text(o) }, onClick = { open = false; pick(i) }) }
        }
    }
}
