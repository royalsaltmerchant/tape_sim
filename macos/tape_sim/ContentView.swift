//
//  ContentView.swift
//  tape_sim
//
//  Created by Julian ranieri on 4/5/24.
//

import SwiftUI

struct ContentView: View {
    @State private var isRecordingEnabled = false
    @State private var isPlaying = false
    @State private var isRewinding = false;
    @State private var isFastForwarding = false;
    @State private var startTimeInSeconds: Float = 0.0
    private var timer = Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()
	// High-frequency timer for rewind and fast forward
    private var rewFasTimer = Timer.publish(every: 0.05, on: .main, in: .common).autoconnect()
    
    var body: some View {
        VStack {
            Toggle(isOn: $isRecordingEnabled) {
                Text("Enable Recording")
            }
            .padding()
            
            Text("\(startTimeInSeconds, specifier: "%.2f") seconds")
                .onReceive(timer) { _ in
                    startTimeInSeconds = getUpdatedStartTime() // Fetches the current start time
                }
            
            HStack {
				Button(action: rtz) {
                    Label("RTZ", systemImage: "return")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .gray))
                
                Button(action: rewind) {
                    Label("REW", systemImage: "gobackward")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .blue))
                
                Button(action: playOrRecord) {
                    Label(isRecordingEnabled ? "REC" : "PLAY", systemImage: isRecordingEnabled ? "record.circle" : "play.circle")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: isRecordingEnabled ? .red : .green))
                
                Button(action: stop) {
                    Label("STOP", systemImage: "stop.circle")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .gray))
                
                Button(action: fastForward) {
                    Label("FWD", systemImage: "goforward")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .blue))
            }
            .padding(.horizontal)
        }
        .padding()
		.onReceive(rewFasTimer) { _ in
			if isRewinding {
				rewind()
			}
			if isFastForwarding {
				fastForward()
			}
		}
    }
    
    func playOrRecord() {
		if (isRewinding) {
		stopRewind()
		}
		if (isFastForwarding) {
		stopFastForward()
		}
		
        if isRecordingEnabled {
            if !isPlaying {
                onStartRecording()
                isPlaying = true
            } else {
                onStopRecording()
                isPlaying = false
            }
        } else {
            if !isPlaying {
                onStartPlaying()
                isPlaying = true
            } else {
                onStopPlaying()
                isPlaying = false
            }
        }
    }
    
    func stop() {
		if (isRewinding) {
			stopRewind()
		}
		if (isFastForwarding) {
			stopFastForward()
		}
		
        if isPlaying && isRecordingEnabled {
            onStopRecording()
        } else if isPlaying {
            onStopPlaying()
        }
        isPlaying = false
    }
    
    func rewind() {
		if (isPlaying) {stop()}
		if (isFastForwarding) {stopFastForward()}
        isRewinding = true;
        onRewind()
    }
    
    func stopRewind() {
		isRewinding = false;
	}
    
    func fastForward() {
		if (isPlaying) {stop()}
		if (isRewinding) {stopRewind()}
		isFastForwarding = true;
        onFastForward()
    }
    
    func stopFastForward() {
		isFastForwarding = false;
	}
	
	func rtz() {
		if (isPlaying) {stop()}
		if (isFastForwarding) {stopFastForward()}
		if (isRewinding) {stopRewind()}
		onRtz()
	}
    
    func getUpdatedStartTime() -> Float {
        return getCurrentStartTimeInSeconds()
    }
    
}

struct ActionButtonStyle: ButtonStyle {
    var backgroundColor: Color
    
    func makeBody(configuration: Self.Configuration) -> some View {
        configuration.label
            .padding()
            .background(backgroundColor)
            .foregroundColor(.white)
            .scaleEffect(configuration.isPressed ? 0.95 : 1)
    }
}
