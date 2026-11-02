// The jacket's looks -- the same numbers and order as ../../ledlink/src/looks.h
// (the firmware draws them; the numbers go over the air). Add new looks at the
// end, never renumber.
import SwiftUI

enum LookCategory: Int, CaseIterable, Identifiable {
    case themes, holidays, bike, live
    var id: Int { rawValue }
    var title: String {
        switch self {
        case .themes: "Themes"
        case .holidays: "Holidays"
        case .bike: "Bike"
        case .live: "Live"
        }
    }
    var symbol: String {
        switch self {
        case .themes: "sparkles"
        case .holidays: "gift"
        case .bike: "bicycle"
        case .live: "waveform"
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
    ]

    static func of(_ id: Int) -> Look { all.indices.contains(id) ? all[id] : all[0] }
    static func `in`(_ c: LookCategory) -> [Look] { all.filter { $0.category == c } }
}

extension Color {
    init(rgb: UInt32) {
        self.init(red: Double((rgb >> 16) & 255) / 255, green: Double((rgb >> 8) & 255) / 255, blue: Double(rgb & 255) / 255)
    }
}
