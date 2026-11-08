// The jacket's looks -- the same numbers and order as ../../ledlink/src/looks.h
// (the firmware draws them; the numbers go over the air). Add new looks at the
// end, never renumber.
import SwiftUI

enum LookCategory: Int, CaseIterable, Identifiable {
    case arcade, themes, holidays, bike, live  // the tab order
    var id: Int { rawValue }
    var title: String {
        switch self {
        case .themes: "Themes"
        case .holidays: "Holidays"
        case .bike: "Bike"
        case .live: "Live"
        case .arcade: "Arcade"
        }
    }
    var symbol: String {
        switch self {
        case .themes: "sparkles"
        case .holidays: "gift"
        case .bike: "bicycle"
        case .live: "waveform"
        case .arcade: "gamecontroller"
        }
    }
}

struct Look: Identifiable {
    let id: Int
    let name: String
    let category: LookCategory
    let colors: [UInt32]       // for the card's preview gradient
    var usesColor: Bool { (Look.solid...Look.comet).contains(id) }

    static let rogueAI = 0, solid = 10, comet = 13, bike = 26, dayFlash = 27, music = 28, musicBeat = 29, heartbeat = 30
    /// ARCADE: the Pixelcade LED firmware's console and game effects (ledlink/src/arcade.h), MAME and the
    /// consoles first, then the games. The games have descriptive names here (the firmware and the
    /// armband use the real titles): game names are other companies' trademarks, and the App Store
    /// won't take them.
    static let arcadeFirst = 31, arcadeConsoles = 45
    var isConsole: Bool { category == .arcade && id < Look.arcadeFirst + Look.arcadeConsoles }

    static let all: [Look] = [
        Look(id: 0, name: "Rogue AI", category: .themes, colors: [0x28FF5A, 0x00823A, 0xFF1E28]),
        Look(id: 1, name: "Light Cycle", category: .themes, colors: [0x001830, 0x00C8FF, 0xFF5000]),
        Look(id: 2, name: "Cyberpunk", category: .themes, colors: [0xFF00A0, 0x8000FF, 0x00E0FF]),
        Look(id: 3, name: "Code Rain", category: .themes, colors: [0x003000, 0x00FF40, 0xD0FFD0]),
        Look(id: 4, name: "Synthwave", category: .themes, colors: [0xFF2080, 0x9020FF, 0xFF6020]),
        Look(id: 5, name: "Rainbow", category: .themes, colors: [0xFF0000, 0xFFE000, 0x00FF00, 0x00C0FF, 0x8000FF]),
        Look(id: 6, name: "Fire", category: .themes, colors: [0x600000, 0xFF2000, 0xFF8000, 0xFFD000]),
        Look(id: 7, name: "Aurora", category: .themes, colors: [0x00FF60, 0x00C0A0, 0x6020FF]),
        Look(id: 8, name: "Ocean", category: .themes, colors: [0x001060, 0x0090FF, 0xC0F0FF]),
        Look(id: 9, name: "Scanner", category: .themes, colors: [0x200000, 0xFF0000, 0x200000]),
        Look(id: 10, name: "Solid", category: .themes, colors: []),
        Look(id: 11, name: "Breathe", category: .themes, colors: []),
        Look(id: 12, name: "Sparkle", category: .themes, colors: []),
        Look(id: 13, name: "Comet", category: .themes, colors: []),
        Look(id: 14, name: "Christmas", category: .holidays, colors: [0xFF0000, 0x00C000, 0xFFE0A0]),
        Look(id: 15, name: "Candy Cane", category: .holidays, colors: [0xFF0000, 0xFFFFFF, 0xFF0000, 0xFFFFFF]),
        Look(id: 16, name: "Winter", category: .holidays, colors: [0x0020A0, 0x40C0FF, 0xFFFFFF]),
        Look(id: 17, name: "Hanukkah", category: .holidays, colors: [0x0040FF, 0xFFFFFF, 0x0040FF]),
        Look(id: 18, name: "New Year", category: .holidays, colors: [0xFFB000, 0xC0C0C0, 0xFF40C0]),
        Look(id: 19, name: "Valentine", category: .holidays, colors: [0xFF1060, 0xFF0000, 0xFF80C0]),
        Look(id: 20, name: "St Patrick", category: .holidays, colors: [0x00FF00, 0x008020, 0xFFC000]),
        Look(id: 21, name: "Easter", category: .holidays, colors: [0xFF80C0, 0xA0FFA0, 0x80C0FF, 0xFFFF80]),
        Look(id: 22, name: "July 4th", category: .holidays, colors: [0xFF0000, 0xFFFFFF, 0x0020FF]),
        Look(id: 23, name: "Halloween", category: .holidays, colors: [0xFF5000, 0x8000FF, 0xFF5000]),
        Look(id: 24, name: "Thanksgiving", category: .holidays, colors: [0xFF6000, 0xC02000, 0xFFA000]),
        Look(id: 25, name: "Pride", category: .holidays, colors: [0xFF0000, 0xFF6000, 0xFFE000, 0x00C000, 0x0040FF, 0x8000C0]),
        Look(id: 26, name: "Bike", category: .bike, colors: [0xFF8000, 0xFFFFFF, 0xFF8000]),
        Look(id: 27, name: "Day Flash", category: .bike, colors: [0xFFA000, 0xFFFFFF, 0x303030]),
        Look(id: 28, name: "Music", category: .live, colors: [0x00FF40, 0xFFE000, 0xFF0000]),
        Look(id: 29, name: "Music Beat", category: .live, colors: [0xFF00FF, 0x00FFFF, 0xFFFF00]),
        Look(id: 30, name: "Heartbeat", category: .live, colors: [0x00A0FF, 0x00FF40, 0xFF0000]),
        Look(id: 31, name: "MAME", category: .arcade, colors: [0xFFDC78, 0x3C2D0A, 0xFFDC78]),
        Look(id: 32, name: "NES", category: .arcade, colors: [0xC80000, 0xB4B4B4, 0xC80000]),
        Look(id: 33, name: "SNES", category: .arcade, colors: [0x7800B4, 0xFFFF00, 0x7800B4]),
        Look(id: 34, name: "N64", category: .arcade, colors: [0xFFDC00, 0x005AFF, 0x00C83C, 0xE6001E]),
        Look(id: 35, name: "GameCube", category: .arcade, colors: [0x8A2BE2, 0x660099, 0x46006E]),
        Look(id: 36, name: "Wii", category: .arcade, colors: [0x145ADC, 0x78BEFF, 0x145ADC]),
        Look(id: 37, name: "Wii U", category: .arcade, colors: [0x00C8DC, 0x0A1E50, 0x00C8DC]),
        Look(id: 38, name: "Famicom Disk", category: .arcade, colors: [0xFFDC00, 0xDC0000, 0xFFDC00]),
        Look(id: 39, name: "Virtual Boy", category: .arcade, colors: [0x280000, 0xFF0000, 0x280000]),
        Look(id: 40, name: "SG-1000", category: .arcade, colors: [0xC80000, 0x000000, 0xC80000]),
        Look(id: 41, name: "Master System", category: .arcade, colors: [0xDC0000, 0x008CFF, 0xDC0000]),
        Look(id: 42, name: "Genesis", category: .arcade, colors: [0x008CFF, 0x000000, 0x008CFF]),
        Look(id: 43, name: "Sega CD", category: .arcade, colors: [0x003C32, 0x00FFD2, 0x003C32]),
        Look(id: 44, name: "32X", category: .arcade, colors: [0xFF8C00, 0x000000, 0xFF8C00]),
        Look(id: 45, name: "Saturn", category: .arcade, colors: [0xE61E3C, 0xFFDC00, 0x008CFF, 0xFF8C00, 0x00C85A, 0xA03CDC]),
        Look(id: 46, name: "Dreamcast", category: .arcade, colors: [0xFF6600, 0x000000, 0xFF6600]),
        Look(id: 47, name: "PlayStation", category: .arcade, colors: [0x00C85A, 0xE6003C, 0x006EFF, 0xE600C8]),
        Look(id: 48, name: "PS2", category: .arcade, colors: [0x0000FF, 0x000000, 0x0000FF]),
        Look(id: 49, name: "PS3", category: .arcade, colors: [0x00C85A, 0xFFFFFF, 0xE6003C, 0x006EFF]),
        Look(id: 50, name: "PSP", category: .arcade, colors: [0x00143C, 0xFFFFFF, 0x00143C]),
        Look(id: 51, name: "Xbox", category: .arcade, colors: [0x147814, 0x28C828, 0x50FF50]),
        Look(id: 52, name: "Xbox 360", category: .arcade, colors: [0x001400, 0x00C800, 0x001400]),
        Look(id: 53, name: "Atari 2600", category: .arcade, colors: [0x8B4513, 0xFF8C00, 0x8B4513]),
        Look(id: 54, name: "Atari 5200", category: .arcade, colors: [0x28282D, 0xFFAA00, 0xE61E1E]),
        Look(id: 55, name: "Atari 7800", category: .arcade, colors: [0xC80000, 0x000000, 0xC80000]),
        Look(id: 56, name: "Jaguar", category: .arcade, colors: [0xFF0000, 0x640000, 0xFF0000]),
        Look(id: 57, name: "Lynx", category: .arcade, colors: [0x5A5A5F, 0xFF0000, 0x5A5A5F]),
        Look(id: 58, name: "PC Engine", category: .arcade, colors: [0xFFFFFF, 0xFF8C00, 0xDC0000]),
        Look(id: 59, name: "PC Engine CD", category: .arcade, colors: [0xFF8C00, 0xFFFFFF, 0xFF8C00]),
        Look(id: 60, name: "SuperGrafx", category: .arcade, colors: [0xFFFFFF, 0xFF8C00, 0xC80000]),
        Look(id: 61, name: "3DO", category: .arcade, colors: [0xE6001E, 0xFFD200, 0x00C83C, 0x006EFF]),
        Look(id: 62, name: "CD-i", category: .arcade, colors: [0x0078C8, 0xFFFFFF, 0x0078C8]),
        Look(id: 63, name: "Vectrex", category: .arcade, colors: [0x0F6423, 0x28FF5A, 0x0F6423]),
        Look(id: 64, name: "ColecoVision", category: .arcade, colors: [0xDC0000, 0xFF7800, 0xFFDC00, 0x00AA3C, 0x006EDC]),
        Look(id: 65, name: "Intellivision", category: .arcade, colors: [0x8B4513, 0xFFD700, 0x8B4513]),
        Look(id: 66, name: "Amiga CD32", category: .arcade, colors: [0xFF0000, 0xFFFFFF, 0xFF0000]),
        Look(id: 67, name: "Channel F", category: .arcade, colors: [0xFF8C00, 0x000000, 0xFF8C00]),
        Look(id: 68, name: "Astrocade", category: .arcade, colors: [0xFF7800, 0xDC0000, 0xFF7800]),
        Look(id: 69, name: "Game Boy", category: .arcade, colors: [0x0F380F, 0x8BAC0F, 0x9BBC0F]),
        Look(id: 70, name: "Game Boy Color", category: .arcade, colors: [0xFF0000, 0x00FF00, 0x0000FF, 0xFF00FF]),
        Look(id: 71, name: "Game Boy Advance", category: .arcade, colors: [0x5A288C, 0xDCDCDC, 0x5A288C]),
        Look(id: 72, name: "Nintendo DS", category: .arcade, colors: [0xC8C8D2, 0x0078FF, 0xC8C8D2]),
        Look(id: 73, name: "3DS", category: .arcade, colors: [0x00D2C8, 0xFFFFFF, 0x00D2C8]),
        Look(id: 74, name: "Game Gear", category: .arcade, colors: [0xC80000, 0x000000, 0xC80000]),
        Look(id: 75, name: "Neo Geo Pocket", category: .arcade, colors: [0xFFC800, 0x232326, 0xFFC800]),
        Look(id: 76, name: "Maze Chomper", category: .arcade, colors: [0x000000, 0xFFFF00, 0x646464]),
        Look(id: 77, name: "Barrel Climb", category: .arcade, colors: [0x960000, 0x8B4513, 0xFF0000]),
        Look(id: 78, name: "Starfield Shooter", category: .arcade, colors: [0xFFDC00, 0x00DCFF, 0xE61E28, 0x00DC5A]),
        Look(id: 79, name: "Road Hopper", category: .arcade, colors: [0x006400, 0x323232, 0x003264]),
        Look(id: 80, name: "Maze Chomper II", category: .arcade, colors: [0xFFFF00, 0xFF6496, 0x966496]),
        Look(id: 81, name: "Dig & Pump", category: .arcade, colors: [0x3C1E0F, 0xFFFFFF, 0xFF0000]),
        Look(id: 82, name: "Cube Hopper", category: .arcade, colors: [0xFF8C00, 0xFFFF00, 0x800080]),
        Look(id: 83, name: "Grand Prix", category: .arcade, colors: [0x646464, 0xFF0000, 0xC8C8C8]),
        Look(id: 84, name: "Burger Stack", category: .arcade, colors: [0xFFA500, 0x00FF00, 0xFF0000]),
        Look(id: 85, name: "Lance Flyers", category: .arcade, colors: [0xFFFF00, 0xFF0000, 0xFF6400]),
        Look(id: 86, name: "Robot Swarm", category: .arcade, colors: [0x00FFFF, 0xFF0000, 0xFF00FF]),
        Look(id: 87, name: "Fortress Flyer", category: .arcade, colors: [0x646496, 0xFFFFFF, 0xFF0000]),
        Look(id: 88, name: "Grid Racers", category: .arcade, colors: [0x003250, 0x00FFFF, 0xFF6400]),
        Look(id: 89, name: "Time Fighter", category: .arcade, colors: [0x001A3C, 0xC8C8C8, 0x648000]),
        Look(id: 90, name: "Pacific Dogfight", category: .arcade, colors: [0x000050, 0xFF0000, 0x96B400]),
        Look(id: 91, name: "Paper Route", category: .arcade, colors: [0xC86432, 0xFF0000, 0x6496C8]),
        Look(id: 92, name: "Graveyard Knight", category: .arcade, colors: [0x1E0032, 0xFF8C00, 0x00C800]),
        Look(id: 93, name: "Dungeon Quartet", category: .arcade, colors: [0xFF0000, 0x0000FF, 0xFFFF00, 0x00FF00]),
        Look(id: 94, name: "Jungle Soldier", category: .arcade, colors: [0x003200, 0x648000, 0xC80000]),
        Look(id: 95, name: "City Smash", category: .arcade, colors: [0x646464, 0x8B4513, 0x00C800]),
        Look(id: 96, name: "Bubble Dragons", category: .arcade, colors: [0x00FF00, 0xC8C8FF, 0x0000FF]),
        Look(id: 97, name: "Brick Breaker", category: .arcade, colors: [0xFF0000, 0xFFFF00, 0x00FF00]),
        Look(id: 98, name: "Sunset Drive", category: .arcade, colors: [0xFF9600, 0xFF0000, 0x9B6464]),
        Look(id: 99, name: "Ruin Warrior", category: .arcade, colors: [0x646450, 0x8B4513, 0xFFD700]),
        Look(id: 100, name: "Street Brawl", category: .arcade, colors: [0x3C3C3C, 0x0000FF, 0xFF0000]),
        Look(id: 101, name: "Wave Cannon", category: .arcade, colors: [0x280028, 0x0096FF, 0xFF8C00]),
        Look(id: 102, name: "Shadow Ninja", category: .arcade, colors: [0x14003C, 0xFFFFFF, 0xC80000]),
        Look(id: 103, name: "Spread Shot", category: .arcade, colors: [0x002800, 0x0000FF, 0xFF0000]),
        Look(id: 104, name: "Dojo Fighter", category: .arcade, colors: [0x8B4513, 0xFFFFFF, 0xFF0000]),
        Look(id: 105, name: "Jungle Gunner", category: .arcade, colors: [0x007800, 0xFF0000, 0xFFFF00]),
        Look(id: 106, name: "Jet Afterburn", category: .arcade, colors: [0x0064C8, 0x969696, 0xFF6400]),
        Look(id: 107, name: "Siren Chase", category: .arcade, colors: [0xFF0000, 0x14003C, 0x0000FF]),
        Look(id: 108, name: "Horror House", category: .arcade, colors: [0x1E0000, 0xFFFFFF, 0x009600]),
        Look(id: 109, name: "Barbarian Trio", category: .arcade, colors: [0x64461E, 0x005000, 0xFFD700]),
        Look(id: 110, name: "Metro Brawlers", category: .arcade, colors: [0x464646, 0x0000C8, 0x009600]),
        Look(id: 111, name: "Sewer Heroes", category: .arcade, colors: [0x0000FF, 0x800080, 0xFF8C00, 0xFF0000]),
        Look(id: 112, name: "Space Fleet", category: .arcade, colors: [0xFFFF00, 0xFF0000, 0x00FFFF]),
        Look(id: 113, name: "Mushroom Bugs", category: .arcade, colors: [0x00FF00, 0xFFFF00, 0xD2B48C]),
        Look(id: 114, name: "Garden Bugs", category: .arcade, colors: [0xFF00FF, 0xFFFF00, 0x00FF00]),
        Look(id: 115, name: "Space Rocks", category: .arcade, colors: [0x64503C, 0xFFFFFF, 0x00FFFF]),
        Look(id: 116, name: "Planet Guard", category: .arcade, colors: [0x00FF00, 0xFF0000, 0x503C28]),
        Look(id: 117, name: "Warp Gate", category: .arcade, colors: [0x1A2A64, 0x00FFFF, 0xFFFFFF]),
        Look(id: 118, name: "Neon Tube", category: .arcade, colors: [0xFF0000, 0x0064FF, 0xFFFF00]),
        Look(id: 119, name: "Dungeon Wizard", category: .arcade, colors: [0x0000C8, 0xFFFF00, 0xFF0000]),
    ]

    static func of(_ id: Int) -> Look { all.indices.contains(id) ? all[id] : all[0] }
    static func `in`(_ c: LookCategory) -> [Look] { all.filter { $0.category == c } }
}

extension Color {
    init(rgb: UInt32) {
        self.init(red: Double((rgb >> 16) & 255) / 255, green: Double((rgb >> 8) & 255) / 255, blue: Double(rgb & 255) / 255)
    }
}
