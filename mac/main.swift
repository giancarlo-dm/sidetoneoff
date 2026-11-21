// Sidetone Off: turns off the Logitech G635's sidetone whenever the headset is connected.
// Lives in the menu bar and starts at login. Uninstall it from its menu, or drag the app to the Trash.

import AppKit
import IOKit
import ServiceManagement

enum Config {
    static let vendorID = 0x046d   // Logitech
    static let productID = 0x0a89  // G635
    static let headsetcontrol = "/opt/homebrew/bin/headsetcontrol"
}

enum Headset {
    static var connectedCount = 0
    static let worker = DispatchQueue(label: "com.giancarlo.sidetone-off.worker")
}

// MARK: - Menu bar

final class MenuBar: NSObject {
    enum State { case disconnected, applying, sidetoneOff, failed }

    static let shared = MenuBar()

    private let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
    private let statusLine = NSMenuItem(title: "", action: nil, keyEquivalent: "")
    private let reapply = NSMenuItem(title: "Turn Off Sidetone Again",
                                     action: #selector(MenuBar.reapplyClicked), keyEquivalent: "")

    private override init() {
        super.init()
        let icon = NSImage(systemSymbolName: "headphones", accessibilityDescription: "Sidetone Off")
        icon?.isTemplate = true
        item.button?.image = icon
        item.button?.toolTip = "Sidetone Off"

        let uninstall = NSMenuItem(title: "Uninstall Sidetone Off…",
                                   action: #selector(MenuBar.uninstallClicked), keyEquivalent: "")
        let quit = NSMenuItem(title: "Quit Sidetone Off",
                              action: #selector(MenuBar.quitClicked), keyEquivalent: "q")
        for target in [reapply, uninstall, quit] { target.target = self }
        statusLine.isEnabled = false

        let menu = NSMenu()
        menu.autoenablesItems = false
        for entry in [statusLine, .separator(), reapply, .separator(), uninstall, quit] {
            menu.addItem(entry)
        }
        item.menu = menu
        show(.disconnected)
    }

    func show(_ state: State) {
        switch state {
        case .disconnected: statusLine.title = "Headset not connected"
        case .applying:     statusLine.title = "G635 connected, turning off sidetone…"
        case .sidetoneOff:  statusLine.title = "G635 connected, sidetone off"
        case .failed:       statusLine.title = "G635 connected, couldn't turn off sidetone"
        }
        reapply.isEnabled = state == .sidetoneOff || state == .failed
        item.button?.appearsDisabled = state == .disconnected
    }

    @objc private func reapplyClicked() {
        turnOffSidetone()
    }

    @objc private func uninstallClicked() {
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = "Uninstall Sidetone Off?"
        alert.informativeText = "It will be removed from your login items and moved to the Trash. "
            + "Your headset's sidetone won't be turned off automatically anymore."
        let uninstall = alert.addButton(withTitle: "Uninstall")
        uninstall.hasDestructiveAction = true
        uninstall.keyEquivalent = ""          // must be clicked; Return won't confirm by accident
        alert.addButton(withTitle: "Cancel")  // Esc cancels
        NSApp.activate(ignoringOtherApps: true)
        guard alert.runModal() == .alertFirstButtonReturn else { return }

        try? SMAppService.mainApp.unregister()
        do {
            try FileManager.default.trashItem(at: Bundle.main.bundleURL, resultingItemURL: nil)
        } catch {
            try? SMAppService.mainApp.register()
            showAlert("Couldn't uninstall Sidetone Off", error.localizedDescription)
            return
        }
        exit(0)
    }

    @objc private func quitClicked() {
        NSApp.terminate(nil)
    }
}

// MARK: - Sidetone

/// Retries briefly because the headset's HID interface can come up a moment after the USB device.
/// Blocks, so it runs on `Headset.worker`.
func runHeadsetcontrol() -> Bool {
    for attempt in 1...5 {
        Thread.sleep(forTimeInterval: 1)
        let process = Process()
        process.executableURL = URL(fileURLWithPath: Config.headsetcontrol)
        process.arguments = ["-s", "0"]
        process.standardOutput = FileHandle.nullDevice
        process.standardError = FileHandle.nullDevice
        do {
            try process.run()
        } catch {
            NSLog("Sidetone Off: can't run \(Config.headsetcontrol): \(error)")
            return false
        }
        process.waitUntilExit()
        if process.terminationStatus == 0 {
            NSLog("Sidetone Off: sidetone disabled (attempt \(attempt))")
            return true
        }
    }
    NSLog("Sidetone Off: headsetcontrol failed after 5 attempts")
    return false
}

func turnOffSidetone() {
    MenuBar.shared.show(.applying)
    Headset.worker.async {
        let succeeded = runHeadsetcontrol()
        DispatchQueue.main.async {
            guard Headset.connectedCount > 0 else { return }  // unplugged in the meantime
            MenuBar.shared.show(succeeded ? .sidetoneOff : .failed)
        }
    }
}

// MARK: - USB notifications

/// Releases every device in the iterator. Draining is also what re-arms the notification.
func drain(_ iterator: io_iterator_t) -> Int {
    var count = 0
    var device = IOIteratorNext(iterator)
    while device != 0 {
        count += 1
        IOObjectRelease(device)
        device = IOIteratorNext(iterator)
    }
    return count
}

func headsetArrived(_ refcon: UnsafeMutableRawPointer?, _ iterator: io_iterator_t) {
    let arrived = drain(iterator)
    guard arrived > 0 else { return }
    Headset.connectedCount += arrived
    turnOffSidetone()
}

func headsetRemoved(_ refcon: UnsafeMutableRawPointer?, _ iterator: io_iterator_t) {
    Headset.connectedCount = max(0, Headset.connectedCount - drain(iterator))
    if Headset.connectedCount == 0 { MenuBar.shared.show(.disconnected) }
}

/// The kernel filters on vendor/product ID, so this process is only woken for the G635.
func watchHeadset(_ port: IONotificationPortRef?, _ notification: String,
                  _ callback: IOServiceMatchingCallback) -> io_iterator_t {
    let matching = IOServiceMatching("IOUSBHostDevice") as NSMutableDictionary
    matching["idVendor"] = Config.vendorID
    matching["idProduct"] = Config.productID

    var iterator: io_iterator_t = 0
    let result = IOServiceAddMatchingNotification(
        port, notification, matching as CFDictionary, callback, nil, &iterator)
    guard result == KERN_SUCCESS else {
        NSLog("Sidetone Off: couldn't register for USB notifications (\(result))")
        exit(1)
    }
    return iterator
}

// MARK: - Login item

func showAlert(_ title: String, _ message: String) {
    let alert = NSAlert()
    alert.messageText = title
    alert.informativeText = message
    NSApp.activate(ignoringOtherApps: true)
    alert.runModal()
}

func registerAtLogin() {
    let service = SMAppService.mainApp
    guard service.status != .enabled else { return }
    do {
        try service.register()
        showAlert("Sidetone Off is running",
                  "It starts automatically when you log in and turns off your headset's sidetone whenever it's connected.\n\n"
                  + "Look for the headphones icon in the menu bar. From there you can check its status, quit it, or uninstall it.")
    } catch {
        showAlert("Couldn't add Sidetone Off to login items", error.localizedDescription)
    }
}

// MARK: - Startup

let app = NSApplication.shared
app.setActivationPolicy(.accessory)
_ = MenuBar.shared

// Remove the login item and quit as soon as the app bundle is moved (e.g. dragged to the Trash) or deleted.
let bundleFD = open(Bundle.main.bundlePath, O_EVTONLY)
let bundleWatcher = DispatchSource.makeFileSystemObjectSource(
    fileDescriptor: bundleFD, eventMask: [.rename, .delete], queue: .main)
bundleWatcher.setEventHandler {
    try? SMAppService.mainApp.unregister()
    exit(0)
}
bundleWatcher.resume()

let port = IONotificationPortCreate(kIOMainPortDefault)
CFRunLoopAddSource(CFRunLoopGetMain(),
                   IONotificationPortGetRunLoopSource(port).takeUnretainedValue(),
                   .commonModes)
let arrivals = watchHeadset(port, kIOFirstMatchNotification, headsetArrived)
let removals = watchHeadset(port, kIOTerminatedNotification, headsetRemoved)

DispatchQueue.main.async {
    _ = drain(removals)              // arm the unplug notification
    headsetArrived(nil, arrivals)    // handle a headset that's already connected, and arm the plug-in notification
    registerAtLogin()
}

app.run()
