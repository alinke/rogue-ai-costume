package org.pixelcade.arcadejacket

// The jacket's looks -- the same numbers and order as ledlink/src/looks.h
// (the firmware draws them; the numbers go over the air) and the iPhone app's Looks.swift.
// Add new looks at the end, never renumber. The arcade games have descriptive names here
// (the firmware and the armband use the real titles): game names are other companies' trademarks.

enum class Category(val title: String) {
    ARCADE("Arcade"), THEMES("Themes"), HOLIDAYS("Holidays"), BIKE("Bike"), LIVE("Live")  // the tab order
}

data class Look(val id: Int, val name: String, val category: Category, val colors: List<Int>) {
    val usesColor: Boolean get() = id in SOLID..COMET
    /** ARCADE: MAME and the consoles first (ledlink/src/arcade.h), then the games. */
    val isConsole: Boolean get() = category == Category.ARCADE && id < ARCADE_FIRST + ARCADE_CONSOLES

    companion object {
        const val ROGUE_AI = 0
        const val SOLID = 10
        const val COMET = 13
        const val BIKE = 26
        const val DAY_FLASH = 27
        const val MUSIC = 28
        const val MUSIC_BEAT = 29
        const val HEARTBEAT = 30
        const val ARCADE_FIRST = 31
        const val ARCADE_CONSOLES = 45

        val all: List<Look> = listOf(
        Look(0, "Rogue AI", Category.THEMES, listOf(0x28FF5A, 0x00823A, 0xFF1E28)),
        Look(1, "Light Cycle", Category.THEMES, listOf(0x001830, 0x00C8FF, 0xFF5000)),
        Look(2, "Cyberpunk", Category.THEMES, listOf(0xFF00A0, 0x8000FF, 0x00E0FF)),
        Look(3, "Code Rain", Category.THEMES, listOf(0x003000, 0x00FF40, 0xD0FFD0)),
        Look(4, "Synthwave", Category.THEMES, listOf(0xFF2080, 0x9020FF, 0xFF6020)),
        Look(5, "Rainbow", Category.THEMES, listOf(0xFF0000, 0xFFE000, 0x00FF00, 0x00C0FF, 0x8000FF)),
        Look(6, "Fire", Category.THEMES, listOf(0x600000, 0xFF2000, 0xFF8000, 0xFFD000)),
        Look(7, "Aurora", Category.THEMES, listOf(0x00FF60, 0x00C0A0, 0x6020FF)),
        Look(8, "Ocean", Category.THEMES, listOf(0x001060, 0x0090FF, 0xC0F0FF)),
        Look(9, "Scanner", Category.THEMES, listOf(0x200000, 0xFF0000, 0x200000)),
        Look(10, "Solid", Category.THEMES, listOf()),
        Look(11, "Breathe", Category.THEMES, listOf()),
        Look(12, "Sparkle", Category.THEMES, listOf()),
        Look(13, "Comet", Category.THEMES, listOf()),
        Look(14, "Christmas", Category.HOLIDAYS, listOf(0xFF0000, 0x00C000, 0xFFE0A0)),
        Look(15, "Candy Cane", Category.HOLIDAYS, listOf(0xFF0000, 0xFFFFFF, 0xFF0000, 0xFFFFFF)),
        Look(16, "Winter", Category.HOLIDAYS, listOf(0x0020A0, 0x40C0FF, 0xFFFFFF)),
        Look(17, "Hanukkah", Category.HOLIDAYS, listOf(0x0040FF, 0xFFFFFF, 0x0040FF)),
        Look(18, "New Year", Category.HOLIDAYS, listOf(0xFFB000, 0xC0C0C0, 0xFF40C0)),
        Look(19, "Valentine", Category.HOLIDAYS, listOf(0xFF1060, 0xFF0000, 0xFF80C0)),
        Look(20, "St Patrick", Category.HOLIDAYS, listOf(0x00FF00, 0x008020, 0xFFC000)),
        Look(21, "Easter", Category.HOLIDAYS, listOf(0xFF80C0, 0xA0FFA0, 0x80C0FF, 0xFFFF80)),
        Look(22, "July 4th", Category.HOLIDAYS, listOf(0xFF0000, 0xFFFFFF, 0x0020FF)),
        Look(23, "Halloween", Category.HOLIDAYS, listOf(0xFF5000, 0x8000FF, 0xFF5000)),
        Look(24, "Thanksgiving", Category.HOLIDAYS, listOf(0xFF6000, 0xC02000, 0xFFA000)),
        Look(25, "Pride", Category.HOLIDAYS, listOf(0xFF0000, 0xFF6000, 0xFFE000, 0x00C000, 0x0040FF, 0x8000C0)),
        Look(26, "Bike", Category.BIKE, listOf(0xFF8000, 0xFFFFFF, 0xFF8000)),
        Look(27, "Day Flash", Category.BIKE, listOf(0xFFA000, 0xFFFFFF, 0x303030)),
        Look(28, "Music", Category.LIVE, listOf(0x00FF40, 0xFFE000, 0xFF0000)),
        Look(29, "Music Beat", Category.LIVE, listOf(0xFF00FF, 0x00FFFF, 0xFFFF00)),
        Look(30, "Heartbeat", Category.LIVE, listOf(0x00A0FF, 0x00FF40, 0xFF0000)),
        Look(31, "MAME", Category.ARCADE, listOf(0xFFDC78, 0x3C2D0A, 0xFFDC78)),
        Look(32, "NES", Category.ARCADE, listOf(0xC80000, 0xB4B4B4, 0xC80000)),
        Look(33, "SNES", Category.ARCADE, listOf(0x7800B4, 0xFFFF00, 0x7800B4)),
        Look(34, "N64", Category.ARCADE, listOf(0xFFDC00, 0x005AFF, 0x00C83C, 0xE6001E)),
        Look(35, "GameCube", Category.ARCADE, listOf(0x8A2BE2, 0x660099, 0x46006E)),
        Look(36, "Wii", Category.ARCADE, listOf(0x145ADC, 0x78BEFF, 0x145ADC)),
        Look(37, "Wii U", Category.ARCADE, listOf(0x00C8DC, 0x0A1E50, 0x00C8DC)),
        Look(38, "Famicom Disk", Category.ARCADE, listOf(0xFFDC00, 0xDC0000, 0xFFDC00)),
        Look(39, "Virtual Boy", Category.ARCADE, listOf(0x280000, 0xFF0000, 0x280000)),
        Look(40, "SG-1000", Category.ARCADE, listOf(0xC80000, 0x000000, 0xC80000)),
        Look(41, "Master System", Category.ARCADE, listOf(0xDC0000, 0x008CFF, 0xDC0000)),
        Look(42, "Genesis", Category.ARCADE, listOf(0x008CFF, 0x000000, 0x008CFF)),
        Look(43, "Sega CD", Category.ARCADE, listOf(0x003C32, 0x00FFD2, 0x003C32)),
        Look(44, "32X", Category.ARCADE, listOf(0xFF8C00, 0x000000, 0xFF8C00)),
        Look(45, "Saturn", Category.ARCADE, listOf(0xE61E3C, 0xFFDC00, 0x008CFF, 0xFF8C00, 0x00C85A, 0xA03CDC)),
        Look(46, "Dreamcast", Category.ARCADE, listOf(0xFF6600, 0x000000, 0xFF6600)),
        Look(47, "PlayStation", Category.ARCADE, listOf(0x00C85A, 0xE6003C, 0x006EFF, 0xE600C8)),
        Look(48, "PS2", Category.ARCADE, listOf(0x0000FF, 0x000000, 0x0000FF)),
        Look(49, "PS3", Category.ARCADE, listOf(0x00C85A, 0xFFFFFF, 0xE6003C, 0x006EFF)),
        Look(50, "PSP", Category.ARCADE, listOf(0x00143C, 0xFFFFFF, 0x00143C)),
        Look(51, "Xbox", Category.ARCADE, listOf(0x147814, 0x28C828, 0x50FF50)),
        Look(52, "Xbox 360", Category.ARCADE, listOf(0x001400, 0x00C800, 0x001400)),
        Look(53, "Atari 2600", Category.ARCADE, listOf(0x8B4513, 0xFF8C00, 0x8B4513)),
        Look(54, "Atari 5200", Category.ARCADE, listOf(0x28282D, 0xFFAA00, 0xE61E1E)),
        Look(55, "Atari 7800", Category.ARCADE, listOf(0xC80000, 0x000000, 0xC80000)),
        Look(56, "Jaguar", Category.ARCADE, listOf(0xFF0000, 0x640000, 0xFF0000)),
        Look(57, "Lynx", Category.ARCADE, listOf(0x5A5A5F, 0xFF0000, 0x5A5A5F)),
        Look(58, "PC Engine", Category.ARCADE, listOf(0xFFFFFF, 0xFF8C00, 0xDC0000)),
        Look(59, "PC Engine CD", Category.ARCADE, listOf(0xFF8C00, 0xFFFFFF, 0xFF8C00)),
        Look(60, "SuperGrafx", Category.ARCADE, listOf(0xFFFFFF, 0xFF8C00, 0xC80000)),
        Look(61, "3DO", Category.ARCADE, listOf(0xE6001E, 0xFFD200, 0x00C83C, 0x006EFF)),
        Look(62, "CD-i", Category.ARCADE, listOf(0x0078C8, 0xFFFFFF, 0x0078C8)),
        Look(63, "Vectrex", Category.ARCADE, listOf(0x0F6423, 0x28FF5A, 0x0F6423)),
        Look(64, "ColecoVision", Category.ARCADE, listOf(0xDC0000, 0xFF7800, 0xFFDC00, 0x00AA3C, 0x006EDC)),
        Look(65, "Intellivision", Category.ARCADE, listOf(0x8B4513, 0xFFD700, 0x8B4513)),
        Look(66, "Amiga CD32", Category.ARCADE, listOf(0xFF0000, 0xFFFFFF, 0xFF0000)),
        Look(67, "Channel F", Category.ARCADE, listOf(0xFF8C00, 0x000000, 0xFF8C00)),
        Look(68, "Astrocade", Category.ARCADE, listOf(0xFF7800, 0xDC0000, 0xFF7800)),
        Look(69, "Game Boy", Category.ARCADE, listOf(0x0F380F, 0x8BAC0F, 0x9BBC0F)),
        Look(70, "Game Boy Color", Category.ARCADE, listOf(0xFF0000, 0x00FF00, 0x0000FF, 0xFF00FF)),
        Look(71, "Game Boy Advance", Category.ARCADE, listOf(0x5A288C, 0xDCDCDC, 0x5A288C)),
        Look(72, "Nintendo DS", Category.ARCADE, listOf(0xC8C8D2, 0x0078FF, 0xC8C8D2)),
        Look(73, "3DS", Category.ARCADE, listOf(0x00D2C8, 0xFFFFFF, 0x00D2C8)),
        Look(74, "Game Gear", Category.ARCADE, listOf(0xC80000, 0x000000, 0xC80000)),
        Look(75, "Neo Geo Pocket", Category.ARCADE, listOf(0xFFC800, 0x232326, 0xFFC800)),
        Look(76, "Maze Chomper", Category.ARCADE, listOf(0x000000, 0xFFFF00, 0x646464)),
        Look(77, "Barrel Climb", Category.ARCADE, listOf(0x960000, 0x8B4513, 0xFF0000)),
        Look(78, "Starfield Shooter", Category.ARCADE, listOf(0xFFDC00, 0x00DCFF, 0xE61E28, 0x00DC5A)),
        Look(79, "Road Hopper", Category.ARCADE, listOf(0x006400, 0x323232, 0x003264)),
        Look(80, "Maze Chomper II", Category.ARCADE, listOf(0xFFFF00, 0xFF6496, 0x966496)),
        Look(81, "Dig & Pump", Category.ARCADE, listOf(0x3C1E0F, 0xFFFFFF, 0xFF0000)),
        Look(82, "Cube Hopper", Category.ARCADE, listOf(0xFF8C00, 0xFFFF00, 0x800080)),
        Look(83, "Grand Prix", Category.ARCADE, listOf(0x646464, 0xFF0000, 0xC8C8C8)),
        Look(84, "Burger Stack", Category.ARCADE, listOf(0xFFA500, 0x00FF00, 0xFF0000)),
        Look(85, "Lance Flyers", Category.ARCADE, listOf(0xFFFF00, 0xFF0000, 0xFF6400)),
        Look(86, "Robot Swarm", Category.ARCADE, listOf(0x00FFFF, 0xFF0000, 0xFF00FF)),
        Look(87, "Fortress Flyer", Category.ARCADE, listOf(0x646496, 0xFFFFFF, 0xFF0000)),
        Look(88, "Grid Racers", Category.ARCADE, listOf(0x003250, 0x00FFFF, 0xFF6400)),
        Look(89, "Time Fighter", Category.ARCADE, listOf(0x001A3C, 0xC8C8C8, 0x648000)),
        Look(90, "Pacific Dogfight", Category.ARCADE, listOf(0x000050, 0xFF0000, 0x96B400)),
        Look(91, "Paper Route", Category.ARCADE, listOf(0xC86432, 0xFF0000, 0x6496C8)),
        Look(92, "Graveyard Knight", Category.ARCADE, listOf(0x1E0032, 0xFF8C00, 0x00C800)),
        Look(93, "Dungeon Quartet", Category.ARCADE, listOf(0xFF0000, 0x0000FF, 0xFFFF00, 0x00FF00)),
        Look(94, "Jungle Soldier", Category.ARCADE, listOf(0x003200, 0x648000, 0xC80000)),
        Look(95, "City Smash", Category.ARCADE, listOf(0x646464, 0x8B4513, 0x00C800)),
        Look(96, "Bubble Dragons", Category.ARCADE, listOf(0x00FF00, 0xC8C8FF, 0x0000FF)),
        Look(97, "Brick Breaker", Category.ARCADE, listOf(0xFF0000, 0xFFFF00, 0x00FF00)),
        Look(98, "Sunset Drive", Category.ARCADE, listOf(0xFF9600, 0xFF0000, 0x9B6464)),
        Look(99, "Ruin Warrior", Category.ARCADE, listOf(0x646450, 0x8B4513, 0xFFD700)),
        Look(100, "Street Brawl", Category.ARCADE, listOf(0x3C3C3C, 0x0000FF, 0xFF0000)),
        Look(101, "Wave Cannon", Category.ARCADE, listOf(0x280028, 0x0096FF, 0xFF8C00)),
        Look(102, "Shadow Ninja", Category.ARCADE, listOf(0x14003C, 0xFFFFFF, 0xC80000)),
        Look(103, "Spread Shot", Category.ARCADE, listOf(0x002800, 0x0000FF, 0xFF0000)),
        Look(104, "Dojo Fighter", Category.ARCADE, listOf(0x8B4513, 0xFFFFFF, 0xFF0000)),
        Look(105, "Jungle Gunner", Category.ARCADE, listOf(0x007800, 0xFF0000, 0xFFFF00)),
        Look(106, "Jet Afterburn", Category.ARCADE, listOf(0x0064C8, 0x969696, 0xFF6400)),
        Look(107, "Siren Chase", Category.ARCADE, listOf(0xFF0000, 0x14003C, 0x0000FF)),
        Look(108, "Horror House", Category.ARCADE, listOf(0x1E0000, 0xFFFFFF, 0x009600)),
        Look(109, "Barbarian Trio", Category.ARCADE, listOf(0x64461E, 0x005000, 0xFFD700)),
        Look(110, "Metro Brawlers", Category.ARCADE, listOf(0x464646, 0x0000C8, 0x009600)),
        Look(111, "Sewer Heroes", Category.ARCADE, listOf(0x0000FF, 0x800080, 0xFF8C00, 0xFF0000)),
        Look(112, "Space Fleet", Category.ARCADE, listOf(0xFFFF00, 0xFF0000, 0x00FFFF)),
        Look(113, "Mushroom Bugs", Category.ARCADE, listOf(0x00FF00, 0xFFFF00, 0xD2B48C)),
        Look(114, "Garden Bugs", Category.ARCADE, listOf(0xFF00FF, 0xFFFF00, 0x00FF00)),
        Look(115, "Space Rocks", Category.ARCADE, listOf(0x64503C, 0xFFFFFF, 0x00FFFF)),
        Look(116, "Planet Guard", Category.ARCADE, listOf(0x00FF00, 0xFF0000, 0x503C28)),
        Look(117, "Warp Gate", Category.ARCADE, listOf(0x1A2A64, 0x00FFFF, 0xFFFFFF)),
        Look(118, "Neon Tube", Category.ARCADE, listOf(0xFF0000, 0x0064FF, 0xFFFF00)),
        Look(119, "Dungeon Wizard", Category.ARCADE, listOf(0x0000C8, 0xFFFF00, 0xFF0000)),
        )

        fun of(id: Int): Look = all.getOrElse(id) { all[0] }
        fun inCategory(c: Category): List<Look> = all.filter { it.category == c }
    }
}
