//
//  ContentView.swift
//  tape_sim
//
//  Created by Julian ranieri on 4/5/24.
//

import SwiftUI

struct ContentView: View {

	@State private var isRecording = false
	
    var body: some View {
        VStack {
            Button(action: toggleRecording) {
                Text(isRecording ? "Stop Recording" : "Start Recording")
                    .padding()
                    .background(isRecording ? Color.red : Color.blue)
                    .foregroundColor(.white)
                    .clipShape(Capsule())
            }
        }
        .padding()
    }
    
    func toggleRecording() {
		if isRecording {
			onStopRecording()
		} else {
			onStartRecording()
		}
		isRecording.toggle()
	}
}
