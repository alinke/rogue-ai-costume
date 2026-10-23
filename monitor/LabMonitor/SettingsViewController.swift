// The settings panel (hold the screen 3 s): every field labelled, in the
// monitor's colours. iOS 12-compatible UIKit, no storyboard.
import UIKit

final class SettingsViewController: UIViewController, UITextFieldDelegate {
    /// Called after Save, with what changed in role or photos (so the monitor can
    /// start or stop the camera).
    var onSave: (() -> Void)?
    /// The costume's signal right now, refreshed while the panel is open.
    var liveSignal: (() -> String)?

    private let cfg = Config.shared
    private let room = UITextField(), dbm = UITextField(), length = UITextField(), cooldown = UITextField()
    private let role = UISegmentedControl(items: ["Checkpoint", "Evidence Wall"])
    private let photos = UISwitch()
    private let signal = UILabel()
    private var timer: Timer?

    private let bg = UIColor(hex: 0x0B1520), panel = UIColor(hex: 0x12233A), steel = UIColor(hex: 0x1F4E79)
    private let pale = UIColor(hex: 0x7FB3D5), white = UIColor(hex: 0xEEF2F5)

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = bg
        preferredContentSize = CGSize(width: 560, height: 640)

        let title = label("LAB-7 MONITOR SETTINGS", size: 22, weight: .heavy, color: white)
        signal.font = UIFont(name: "Menlo-Bold", size: 15) ?? .systemFont(ofSize: 15)
        signal.textColor = UIColor(hex: 0xF5B041)
        signal.numberOfLines = 0

        role.selectedSegmentIndex = cfg.isWall ? 1 : 0
        role.tintColor = pale
        role.backgroundColor = bg
        role.setTitleTextAttributes([.foregroundColor: white, .font: UIFont.systemFont(ofSize: 14, weight: .semibold)], for: .normal)
        role.setTitleTextAttributes([.foregroundColor: bg, .font: UIFont.systemFont(ofSize: 14, weight: .bold)], for: .selected)
        if #available(iOS 13.0, *) { role.selectedSegmentTintColor = pale }
        photos.isOn = cfg.photosEnabled
        photos.onTintColor = UIColor(hex: 0x2ECC71)

        field(room, cfg.sector, "e.g. KITCHEN", caps: true)
        field(dbm, "\(cfg.intrusionDbm)", "-62", numbers: true)
        field(length, "\(cfg.intrusionMaxSeconds)", "5", numbers: true)
        field(cooldown, "\(cfg.intrusionCooldownSeconds)", "120", numbers: true)

        let rows: [UIView] = [
            title,
            signal,
            row("THIS IPAD IS", "The checkpoint (camera, ID cards) or the evidence wall (slideshow, tracker).", role),
            row("CHECKPOINT PHOTOS", "Off: camera off, the checkpoint shows as closed.", photos),
            row("ROOM NAME", "Shown on screen and on the cards.", room),
            row("IN-THIS-ROOM SIGNAL (dBm)", "The costume counts as in the room at this strength or stronger. Closer to 0 = it must come closer.", dbm),
            row("TAKEOVER LENGTH (s)", "How long the AI hijacks the screen. 5 or more.", length),
            row("TAKEOVER COOLDOWN (s)", "Wait before it can happen again, unless the costume leaves first. 0 = none.", cooldown),
        ]
        let stack = UIStackView(arrangedSubviews: rows)
        stack.axis = .vertical
        stack.spacing = 14
        stack.setCustomSpacing(6, after: title)

        let cancel = button("Cancel", filled: false, action: #selector(cancelTapped))
        let save = button("Save", filled: true, action: #selector(saveTapped))
        let buttons = UIStackView(arrangedSubviews: [cancel, save])
        buttons.spacing = 12
        buttons.distribution = .fillEqually

        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        stack.translatesAutoresizingMaskIntoConstraints = false
        buttons.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scroll)
        scroll.addSubview(stack)
        view.addSubview(buttons)
        NSLayoutConstraint.activate([
            scroll.topAnchor.constraint(equalTo: view.topAnchor, constant: 20),
            scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scroll.bottomAnchor.constraint(equalTo: buttons.topAnchor, constant: -12),
            stack.topAnchor.constraint(equalTo: scroll.topAnchor),
            stack.bottomAnchor.constraint(equalTo: scroll.bottomAnchor),
            stack.leadingAnchor.constraint(equalTo: scroll.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: scroll.trailingAnchor, constant: -24),
            stack.widthAnchor.constraint(equalTo: scroll.widthAnchor, constant: -48),
            buttons.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            buttons.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24),
            buttons.bottomAnchor.constraint(equalTo: view.bottomAnchor, constant: -20),
            buttons.heightAnchor.constraint(equalToConstant: 48),
        ])
        refreshSignal()
        timer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in self?.refreshSignal() }
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        timer?.invalidate()
    }

    private func refreshSignal() {
        signal.text = "COSTUME SIGNAL NOW: " + (liveSignal?() ?? "--")
            + "\nUPLOAD: " + (cfg.uploadConfigured ? "ON, \(Uploader.shared.pending) WAITING" : "OFF")
    }

    // MARK: - Actions

    @objc private func cancelTapped() { dismiss(animated: true) }

    @objc private func saveTapped() {
        cfg.isWall = role.selectedSegmentIndex == 1
        cfg.photosEnabled = photos.isOn
        if let s = room.text?.trimmingCharacters(in: .whitespaces), !s.isEmpty { cfg.sector = s }
        if let v = Int(dbm.text ?? ""), v < 0 { cfg.intrusionDbm = v }
        if let v = Int(length.text ?? ""), v >= 5 { cfg.intrusionMaxSeconds = v }
        if let v = Int(cooldown.text ?? ""), v >= 0 { cfg.intrusionCooldownSeconds = v }
        dismiss(animated: true) { self.onSave?() }
    }

    func textFieldShouldReturn(_ t: UITextField) -> Bool { t.resignFirstResponder(); return true }

    // MARK: - Pieces

    private func label(_ s: String, size: CGFloat, weight: UIFont.Weight = .regular, color: UIColor) -> UILabel {
        let l = UILabel()
        l.text = s
        l.font = .systemFont(ofSize: size, weight: weight)
        l.textColor = color
        l.numberOfLines = 0
        return l
    }

    /// Label + hint on the left, the control on the right; a panel behind.
    private func row(_ title: String, _ hint: String, _ control: UIView) -> UIView {
        let t = label(title, size: 14, weight: .bold, color: white)
        let h = label(hint, size: 12, color: pale)
        let text = UIStackView(arrangedSubviews: [t, h])
        text.axis = .vertical
        text.spacing = 3
        control.setContentHuggingPriority(.required, for: .horizontal)
        control.setContentCompressionResistancePriority(.required, for: .horizontal)
        if control is UITextField { control.widthAnchor.constraint(equalToConstant: 170).isActive = true }
        let line = UIStackView(arrangedSubviews: [text, control])
        line.spacing = 14
        line.alignment = .center
        line.isLayoutMarginsRelativeArrangement = true
        line.layoutMargins = UIEdgeInsets(top: 10, left: 14, bottom: 10, right: 14)
        let box = UIView()
        box.backgroundColor = panel
        box.layer.borderColor = steel.cgColor
        box.layer.borderWidth = 1
        line.translatesAutoresizingMaskIntoConstraints = false
        box.addSubview(line)
        NSLayoutConstraint.activate([
            line.topAnchor.constraint(equalTo: box.topAnchor), line.bottomAnchor.constraint(equalTo: box.bottomAnchor),
            line.leadingAnchor.constraint(equalTo: box.leadingAnchor), line.trailingAnchor.constraint(equalTo: box.trailingAnchor),
        ])
        return box
    }

    private func field(_ f: UITextField, _ value: String, _ placeholder: String, caps: Bool = false, numbers: Bool = false) {
        f.text = value
        f.attributedPlaceholder = NSAttributedString(string: placeholder, attributes: [.foregroundColor: pale.withAlphaComponent(0.6)])
        f.textColor = white
        f.font = UIFont(name: "Menlo-Bold", size: 17) ?? .systemFont(ofSize: 17)
        f.backgroundColor = bg
        f.layer.borderColor = steel.cgColor
        f.layer.borderWidth = 1
        f.layer.cornerRadius = 6
        f.leftView = UIView(frame: CGRect(x: 0, y: 0, width: 10, height: 10))
        f.leftViewMode = .always
        f.heightAnchor.constraint(equalToConstant: 40).isActive = true
        f.autocapitalizationType = caps ? .allCharacters : .none
        f.autocorrectionType = .no
        f.keyboardType = numbers ? .numbersAndPunctuation : .default
        f.returnKeyType = .done
        f.delegate = self
    }

    private func button(_ title: String, filled: Bool, action: Selector) -> UIButton {
        let b = UIButton(type: .system)
        b.setTitle(title, for: .normal)
        b.titleLabel?.font = .systemFont(ofSize: 18, weight: .bold)
        b.setTitleColor(white, for: .normal)
        b.backgroundColor = filled ? steel : .clear
        b.layer.borderColor = steel.cgColor
        b.layer.borderWidth = 2
        b.layer.cornerRadius = 8
        b.addTarget(self, action: action, for: .touchUpInside)
        return b
    }
}
