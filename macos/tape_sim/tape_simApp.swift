//
//  tape_simApp.swift
//  tape_sim
//
//  Created by Julian ranieri on 4/5/24.
//

import SwiftUI
import AppKit

@main
struct tape_simApp: App {
    @StateObject private var directoryPicker = DirectoryPickerViewModel()
    
    init() {
        initAudio()
    }
    
    var body: some Scene {
        WindowGroup {
            ContentView()
                .onAppear {
                    directoryPicker.promptUserForDirectory()
                }
        }
        .commands {
            CommandMenu("Actions") {
                Button("Stereo Bounce") {
                    showBounceWindow()
                }
                Button("Change Working Directory") {
					directoryPicker.promptUserForDirectory()
				}
            }
        }
    }

    func showBounceWindow() {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 480, height: 300),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered, defer: false)
        window.center()
        window.setFrameAutosaveName("Bounce Window")
        window.isReleasedWhenClosed = false
        window.contentView = NSHostingController(rootView: BounceView()).view
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
    
    func applicationWillTerminate(_ aNotification: Notification) {
        cleanupAudio()
    }
}

