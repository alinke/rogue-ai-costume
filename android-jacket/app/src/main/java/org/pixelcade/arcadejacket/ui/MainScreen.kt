package org.pixelcade.arcadejacket.ui

// The one screen: the jacket's link and power along the top, the five categories
// (Arcade, Themes, Holidays, Bike, Live), and brightness / speed along the bottom.
// What's shown follows the jacket's own status, so it matches the armband too. The
// same layout as the iPhone app's ContentView.swift.

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyGridScope
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.ArrowForward
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.DirectionsBike
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.GraphicEq
import androidx.compose.material.icons.filled.Redeem
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material.icons.filled.PowerSettingsNew
import androidx.compose.material.icons.filled.WarningAmber
import androidx.compose.material.icons.filled.WbSunny
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.pixelcade.arcadejacket.App
import org.pixelcade.arcadejacket.BikeBrake
import org.pixelcade.arcadejacket.Category
import org.pixelcade.arcadejacket.HeartRate
import org.pixelcade.arcadejacket.JacketCommand
import org.pixelcade.arcadejacket.JacketLink
import org.pixelcade.arcadejacket.Look
import org.pixelcade.arcadejacket.MusicListener
import org.pixelcade.arcadejacket.Turn

/** What the main screen asks the activity for (permission prompts, other screens). */
class MainActions(
    val openSettings: () -> Unit,
    val openWhatYouNeed: () -> Unit,
    val needMicrophone: (then: () -> Unit) -> Unit,
    val needLocation: (then: () -> Unit) -> Unit,
    val needBluetooth: () -> Unit,
)

@Composable
fun MainScreen(app: App, actions: MainActions) {
    val link = app.link
    var category by rememberSaveable { mutableStateOf(Look.of(link.status.look).category) }
    var picked by rememberSaveable { mutableStateOf(false) }

    // The music looks listen only while they're on; the tab follows the look until one is picked.
    LaunchedEffect(link.status.look) {
        val look = link.status.look
        if (look == Look.MUSIC || look == Look.MUSIC_BEAT) actions.needMicrophone { app.music.start() } else app.music.stop()
        if (!picked) category = Look.of(look).category
    }

    Column(Modifier.fillMaxSize().background(Color.Black)) {
        Header(link, actions)
        if (link.demo) DemoBanner(actions.openWhatYouNeed, link)
        CategoryBar(category, Look.of(link.status.look).category) { category = it; picked = true }
        Box(Modifier.weight(1f)) {
            when (category) {
                Category.ARCADE -> ArcadePanel(link)
                Category.THEMES, Category.HOLIDAYS -> LookGrid(link, Look.inCategory(category))
                Category.BIKE -> BikePanel(link, app.brake, actions)
                Category.LIVE -> LivePanel(link, app.music, app.heart)
            }
        }
        Controls(link)
    }
}

// ---- Header ------------------------------------------------------------------------------

@Composable
private fun Header(link: JacketLink, actions: MainActions) {
    Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, top = 12.dp, bottom = 10.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text("ARCADE JACKET", color = Neon, fontSize = 24.sp, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace, maxLines = 1)
            Row(verticalAlignment = Alignment.CenterVertically) {
                val dot = when (link.state) {
                    JacketLink.State.CONNECTED -> Color.Green
                    JacketLink.State.CONNECTING, JacketLink.State.SEARCHING -> Orange
                    else -> Color.Red
                }
                Box(Modifier.size(8.dp).background(dot, CircleShape))
                Spacer(Modifier.width(6.dp))
                Text(label(link), color = Dim, fontSize = 12.sp, fontFamily = FontFamily.Monospace, maxLines = 1, overflow = TextOverflow.Ellipsis,
                    modifier = if (link.state == JacketLink.State.NO_PERMISSION) Modifier.clickable { actions.needBluetooth() } else Modifier)
            }
        }
        IconButton(onClick = actions.openSettings) { Icon(Icons.Filled.Settings, "Settings", tint = Dim, modifier = Modifier.size(28.dp)) }
        val on = link.status.on
        Box(
            Modifier.size(52.dp).clip(CircleShape).background(if (on) Neon.copy(alpha = 0.25f) else Panel)
                .border(2.dp, if (on) Neon else Color.Gray, CircleShape)
                .clickable { link.send(JacketCommand.POWER, if (on) 0 else 1) },
            contentAlignment = Alignment.Center,
        ) { Icon(Icons.Filled.PowerSettingsNew, if (on) "Lights off" else "Lights on", tint = if (on) Neon else Color.Gray) }
    }
}

private fun label(link: JacketLink) = when (link.state) {
    JacketLink.State.BLUETOOTH_OFF -> "Bluetooth is off"
    JacketLink.State.NO_PERMISSION -> "Allow Bluetooth to find the jacket"
    JacketLink.State.SEARCHING -> "Looking for the jacket..."
    JacketLink.State.CONNECTING -> "Connecting..."
    JacketLink.State.CONNECTED -> if (link.status.on) "Connected · ${Look.of(link.status.look).name}" else "Connected · lights off"
}

// ---- Categories --------------------------------------------------------------------------

private fun Category.icon(): ImageVector = when (this) {
    Category.ARCADE -> Icons.Filled.SportsEsports
    Category.THEMES -> Icons.Filled.AutoAwesome
    Category.HOLIDAYS -> Icons.Filled.Redeem
    Category.BIKE -> Icons.Filled.DirectionsBike
    Category.LIVE -> Icons.Filled.GraphicEq
}

@Composable
private fun CategoryBar(category: Category, current: Category, pick: (Category) -> Unit) {
    Row(Modifier.fillMaxWidth().padding(horizontal = 16.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        for (c in Category.entries) {
            val sel = c == category
            Box(Modifier.weight(1f).clip(RoundedCornerShape(10.dp)).background(if (sel) Neon.copy(alpha = 0.2f) else Panel).clickable { pick(c) }) {
                Column(Modifier.fillMaxWidth().padding(vertical = 8.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(c.icon(), null, tint = if (sel) Neon else Dim)
                    Text(c.title, color = if (sel) Neon else Dim, fontSize = 11.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
                }
                if (c == current) Box(Modifier.align(Alignment.TopEnd).padding(6.dp).size(7.dp).background(Color.Green, CircleShape))
            }
        }
    }
}

// ---- Looks -------------------------------------------------------------------------------

@Composable
fun LookCard(link: JacketLink, look: Look) {
    val on = link.status.look == look.id && link.status.on
    val swatch = if (look.usesColor) {
        val c = rgb(link.status.color)
        if (look.id == Look.SOLID) listOf(c, c) else listOf(c.copy(alpha = 0.25f), c, c.copy(alpha = 0.25f))
    } else look.colors.map { rgb(it) }.ifEmpty { listOf(Neon, Neon) }
    Column(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel)
            .border(BorderStroke(2.dp, if (on) Neon else Color.Transparent), RoundedCornerShape(14.dp))
            .clickable { link.setLook(look.id) }.padding(10.dp),
    ) {
        Box(Modifier.fillMaxWidth().height(34.dp).clip(RoundedCornerShape(8.dp))
            .background(Brush.horizontalGradient(if (swatch.size == 1) swatch + swatch else swatch)))
        Spacer(Modifier.height(8.dp))
        Text(look.name, color = if (on) Color.White else Dim, fontSize = 16.sp, fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

private fun LazyGridScope.full(content: @Composable () -> Unit) = item(span = { GridItemSpan(maxLineSpan) }) { content() }

@Composable
private fun LookGrid(link: JacketLink, looks: List<Look>) {
    LazyVerticalGrid(GridCells.Fixed(2), Modifier.fillMaxSize(), contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        if (Look.of(link.status.look).usesColor && looks.any { it.id == link.status.look }) full { ColorRow(link) }
        items(looks, key = { it.id }) { LookCard(link, it) }
    }
}

/** Solid / Breathe / Sparkle / Comet: the colour, from presets. */
@Composable
private fun ColorRow(link: JacketLink) {
    val presets = listOf(0x00DCFF, 0xFF0000, 0x00FF40, 0x0040FF, 0x9000FF, 0xFFA000, 0xFFFFFF, 0xFF40A0)
    Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel).padding(12.dp), horizontalArrangement = Arrangement.SpaceEvenly) {
        for (c in presets) {
            Box(Modifier.size(30.dp).clip(CircleShape).background(rgb(c))
                .border(if (link.status.color == c) 3.dp else 0.dp, Color.White, CircleShape).clickable { link.setColor(c) })
        }
    }
}

/** ARCADE: Consoles or Games (each A-Z), a letter bar to jump down the list, and a search across both. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ArcadePanel(link: JacketLink) {
    val current = Look.of(link.status.look)
    var games by rememberSaveable { mutableStateOf(current.category == Category.ARCADE && !current.isConsole) }
    var search by rememberSaveable { mutableStateOf("") }
    val list = if (search.isEmpty()) Look.inCategory(Category.ARCADE).filter { it.isConsole != games }
               else Look.inCategory(Category.ARCADE).filter { it.name.contains(search, ignoreCase = true) }
    val looks = list.sortedBy { it.name.lowercase() }
    val grid = rememberLazyGridState()
    val scope = rememberCoroutineScope()
    fun letter(n: String) = n.firstOrNull()?.uppercaseChar()?.takeIf { it.isLetter() }?.toString() ?: "#"
    val first = LinkedHashMap<String, Int>()
    looks.forEachIndexed { i, l -> first.putIfAbsent(letter(l.name), i) }
    val header = if (search.isEmpty()) 3 else 2  // the items above the cards

    LazyVerticalGrid(GridCells.Fixed(2), Modifier.fillMaxSize(), state = grid, contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        full {
            SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth().alpha(if (search.isEmpty()) 1f else 0.4f)) {
                listOf("Consoles" to false, "Games" to true).forEachIndexed { i, (t, g) ->
                    SegmentedButton(selected = games == g, onClick = { games = g }, shape = SegmentedButtonDefaults.itemShape(i, 2), enabled = search.isEmpty()) { Text(t) }
                }
            }
        }
        full {
            TextField(search, { search = it }, Modifier.fillMaxWidth(), singleLine = true,
                placeholder = { Text("Find a console or game") },
                leadingIcon = { Icon(Icons.Filled.Search, null) },
                trailingIcon = { if (search.isNotEmpty()) IconButton(onClick = { search = "" }) { Icon(Icons.Filled.Close, "Clear") } },
                shape = RoundedCornerShape(12.dp),
                colors = TextFieldDefaults.colors(focusedContainerColor = Panel, unfocusedContainerColor = Panel,
                    focusedIndicatorColor = Color.Transparent, unfocusedIndicatorColor = Color.Transparent))
        }
        if (search.isEmpty()) full {
            // Tap a letter: the list scrolls to the first name starting with it (# for numbers).
            LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                items(first.keys.sortedWith(compareBy({ it != "#" }, { it })).toList()) { l ->
                    Box(Modifier.size(32.dp).clip(RoundedCornerShape(8.dp)).background(Panel)
                        .clickable { scope.launch { grid.animateScrollToItem(header + (first[l] ?: 0)) } },
                        contentAlignment = Alignment.Center) {
                        Text(l, color = Neon, fontWeight = FontWeight.Bold, fontFamily = FontFamily.Monospace)
                    }
                }
            }
        }
        items(looks, key = { it.id }) { LookCard(link, it) }
        if (looks.isEmpty()) full { Text("Nothing called \"$search\"", color = Dim) }
        full {
            Text("The LED strip effects Pixelcade plays on arcade cabinets, one for each console and game.", color = Dim, fontSize = 12.sp)
        }
    }
}

// ---- Bike --------------------------------------------------------------------------------

@Composable
private fun BikePanel(link: JacketLink, brake: BikeBrake, actions: MainActions) {
    var holding by remember { mutableStateOf(false) }
    LaunchedEffect(holding) {
        while (holding) { delay(800); link.send(JacketCommand.BRAKE, 1) }  // the jacket lets go 1.5 s after the last
    }
    LazyVerticalGrid(GridCells.Fixed(2), Modifier.fillMaxSize(), contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
        full {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                TurnButton(link, Turn.LEFT, Icons.Filled.ArrowBack, "Left", Modifier.weight(1f))
                TurnButton(link, Turn.HAZARD, Icons.Filled.WarningAmber, "Hazard", Modifier.weight(1f))
                TurnButton(link, Turn.RIGHT, Icons.Filled.ArrowForward, "Right", Modifier.weight(1f))
            }
        }
        full {
            // Press and hold: the brake light for as long as you hold it.
            val on = link.status.braking || holding
            Box(
                Modifier.fillMaxWidth().height(64.dp).clip(RoundedCornerShape(14.dp)).background(if (on) Color.Red else Color.Red.copy(alpha = 0.18f))
                    .pointerInput(Unit) {
                        detectTapGestures(onPress = {
                            holding = true
                            link.send(JacketCommand.BRAKE, 1)
                            tryAwaitRelease()
                            holding = false
                            link.send(JacketCommand.BRAKE, 0)
                        })
                    },
                contentAlignment = Alignment.Center,
            ) { Text(if (on) "BRAKING" else "HOLD TO BRAKE", color = if (on) Color.White else Color.Red, fontSize = 20.sp, fontWeight = FontWeight.Black, fontFamily = FontFamily.Monospace) }
        }
        full {
            Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel).padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text("Automatic brake light", fontWeight = FontWeight.SemiBold)
                        Text("Lights red when GPS says you're slowing down (about a second behind). Keeps working with the screen off while the jacket is connected.",
                            color = Dim, fontSize = 12.sp)
                    }
                    Switch(brake.enabled, { on -> if (on) actions.needLocation { brake.enable(true) } else brake.enable(false) },
                        colors = SwitchDefaults.colors(checkedTrackColor = Color.Red))
                }
                if (brake.enabled) {
                    Row {
                        Text(brake.speed?.let { String.format("%.0f km/h · %.0f mph", it * 3.6, it * 2.237) } ?: "Waiting for GPS...",
                            fontFamily = FontFamily.Monospace, fontSize = 12.sp, modifier = Modifier.weight(1f))
                        if (brake.braking) Text("BRAKE", color = Color.Red, fontWeight = FontWeight.Bold, fontSize = 12.sp)
                    }
                }
            }
        }
        items(Look.inCategory(Category.BIKE), key = { it.id }) { LookCard(link, it) }
        full {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Turn signals sweep amber down that side's sleeve and front, and cancel themselves after 30 s. The armband has the same buttons, easier to reach while riding.",
                    color = Dim, fontSize = 12.sp)
                Row {
                    Icon(Icons.Filled.WarningAmber, null, tint = Orange, modifier = Modifier.size(16.dp))
                    Spacer(Modifier.width(6.dp))
                    Text("Wear the jacket as well as proper bike lights, never instead of them. The automatic brake light is about a second behind your braking, and check your local rules on flashing lights.",
                        color = Orange, fontSize = 12.sp)
                }
            }
        }
    }
}

@Composable
private fun TurnButton(link: JacketLink, t: Turn, icon: ImageVector, label: String, modifier: Modifier) {
    val on = link.status.turn == t
    Column(
        modifier.height(96.dp).clip(RoundedCornerShape(14.dp)).background(if (on) Orange else Orange.copy(alpha = 0.15f))
            .clickable { link.send(JacketCommand.TURN, if (on) Turn.OFF.code else t.code) },
        horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center,
    ) {
        Icon(icon, null, tint = if (on) Color.Black else Orange, modifier = Modifier.size(34.dp))
        Text(if (on) "Tap: off" else label, color = if (on) Color.Black else Orange, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
    }
}

// ---- Live --------------------------------------------------------------------------------

@Composable
private fun LivePanel(link: JacketLink, music: MusicListener, heart: HeartRate) {
    LazyVerticalGrid(GridCells.Fixed(2), Modifier.fillMaxSize(), contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
        items(Look.inCategory(Category.LIVE), key = { it.id }) { LookCard(link, it) }
        full {
            Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel).padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Filled.MusicNote, null); Spacer(Modifier.width(6.dp)); Text("Music", fontWeight = FontWeight.SemiBold)
                }
                if (music.running) {
                    Row(Modifier.height(70.dp), verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Meter("Bass", music.bass, Color.Green)
                        Meter("Mids", music.mid, Color.Yellow)
                        Meter("Treble", music.treble, Color(0xFFFF2D55))
                        Spacer(Modifier.weight(1f))
                        BeatDot(music.beats)
                    }
                    Text("Listening -- nothing is recorded. Your music can play from this phone too.", color = Dim, fontSize = 12.sp)
                } else {
                    Text("Pick Music or Music Beat: the phone listens to the music around you (a concert, a party, a speaker) and the jacket moves with it.",
                        color = Dim, fontSize = 12.sp)
                }
            }
        }
        full { HeartPanel(heart) }
    }
}

@Composable
private fun Meter(name: String, v: Float, c: Color) {
    val h by animateFloatAsState(v, tween(80), label = name)
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(Modifier.width(28.dp).weight(1f), contentAlignment = Alignment.BottomCenter) {
            Box(Modifier.fillMaxWidth().fillMaxHeight(h.coerceIn(0.02f, 1f)).clip(RoundedCornerShape(4.dp)).background(c))
        }
        Text(name, color = Dim, fontSize = 10.sp)
    }
}

/** Flashes on each beat the listener hears. */
@Composable
private fun BeatDot(beats: Int) {
    var flash by remember { mutableFloatStateOf(0f) }
    LaunchedEffect(beats) { flash = 1f; delay(30); flash = 0f }
    val a by animateFloatAsState(if (flash > 0) 1f else 0.25f, tween(if (flash > 0) 0 else 250), label = "beat")
    Box(Modifier.size(44.dp).alpha(a).background(Color(0xFFAF52DE), CircleShape))
}

@Composable
private fun HeartPanel(heart: HeartRate) {
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Panel).padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Filled.Favorite, null, tint = Color.Red); Spacer(Modifier.width(6.dp))
            Text("Heart rate", color = Color.Red, fontWeight = FontWeight.SemiBold, modifier = Modifier.weight(1f))
            heart.bpm?.let { Text("$it BPM", fontWeight = FontWeight.Bold, fontSize = 18.sp) }
        }
        if (heart.bpm != null) Text("From ${heart.source}", color = Dim, fontSize = 12.sp)
        val name = heart.connectedName
        if (name != null) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Sensor: $name", modifier = Modifier.weight(1f))
                TextButton(onClick = { heart.forget() }) { Text("Forget") }
            }
        } else {
            Button(onClick = { if (heart.scanning) heart.stopScan() else heart.startScan() },
                colors = ButtonDefaults.buttonColors(containerColor = Color.Red, contentColor = Color.White)) {
                Text(if (heart.scanning) "Searching for sensors..." else "Find a heart-rate sensor")
            }
            for (s in heart.sensors) {
                Row(Modifier.fillMaxWidth().clickable { heart.connect(s) }.padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Filled.Bluetooth, null); Spacer(Modifier.width(8.dp)); Text(s.name, Modifier.weight(1f)); Text("Connect", color = Neon)
                }
            }
        }
        Text("Works with anything that shares heart rate over Bluetooth: a Fitbit Charge 6 or Pixel Watch 2-4 (turn on heart-rate sharing on the watch), Polar, Wahoo and Garmin chest straps and arm bands. Older Fitbits (Versa, Sense, Inspire) can't.",
            color = Dim, fontSize = 12.sp)
    }
}

// ---- Brightness / speed ------------------------------------------------------------------

@Composable
private fun Controls(link: JacketLink) {
    // While a slider is dragged (and a moment after), the jacket's status is still catching
    // up with the values just sent: showing those would pull the slider back.
    var bright by remember { mutableFloatStateOf(link.status.bright.toFloat()) }
    var speed by remember { mutableFloatStateOf(link.status.speed.toFloat()) }
    var dragging by remember { mutableStateOf(false) }
    var releasedAt by remember { mutableLongStateOf(0L) }
    fun following() = !dragging && System.currentTimeMillis() - releasedAt > 1500
    LaunchedEffect(link.status.bright) { if (following()) bright = link.status.bright.toFloat() }
    LaunchedEffect(link.status.speed) { if (following()) speed = link.status.speed.toFloat() }

    Column(Modifier.fillMaxWidth().background(Color.White.copy(alpha = 0.04f)).padding(horizontal = 16.dp, vertical = 8.dp)) {
        SliderRow(Icons.Filled.WbSunny, bright, 10f..100f, 17, "${bright.toInt()}%",
            onChange = { dragging = true; bright = it; link.send(JacketCommand.BRIGHT, it.toInt()) },
            onDone = { dragging = false; releasedAt = System.currentTimeMillis(); link.send(JacketCommand.BRIGHT, bright.toInt()) })
        SliderRow(Icons.Filled.Speed, speed, 20f..240f, 21, String.format("%.1fx", speed / 100),
            onChange = { dragging = true; speed = it; link.send(JacketCommand.SPEED, it.toInt()) },
            onDone = { dragging = false; releasedAt = System.currentTimeMillis(); link.send(JacketCommand.SPEED, speed.toInt()) })
    }
}

@Composable
private fun SliderRow(icon: ImageVector, value: Float, range: ClosedFloatingPointRange<Float>, steps: Int, text: String,
                      onChange: (Float) -> Unit, onDone: () -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, null, tint = Neon, modifier = Modifier.size(24.dp))
        Spacer(Modifier.width(12.dp))
        Slider(value, onChange, Modifier.weight(1f), valueRange = range, steps = steps, onValueChangeFinished = onDone,
            colors = SliderDefaults.colors(thumbColor = Color.White, activeTrackColor = Neon, activeTickColor = Color.Transparent, inactiveTickColor = Color.Transparent))
        Text(text, fontSize = 12.sp, modifier = Modifier.width(48.dp).padding(start = 8.dp))
    }
}
