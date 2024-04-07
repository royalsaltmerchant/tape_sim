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
    }
    
	// Use this if you want to perform some cleanup before the app is terminated
    func applicationWillTerminate(_ aNotification: Notification) {
		cleanupAudio()
    }
}
