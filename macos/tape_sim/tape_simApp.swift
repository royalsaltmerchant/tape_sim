//
//  tape_simApp.swift
//  tape_sim
//
//  Created by Julian ranieri on 4/5/24.
//

import SwiftUI

@main
struct tape_simApp: App {
	init() {
		initAudio()
	}
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
		.commands {
            CommandMenu("Actions") {
                Button("Stereo Bounce") {
                    showBounceWindow()
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
    
	// Use this if you want to perform some cleanup before the app is terminated
    func applicationWillTerminate(_ aNotification: Notification) {
		cleanupAudio()
    }
}
