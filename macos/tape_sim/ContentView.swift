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
    @State private var isRewinding = false
    @State private var isFastForwarding = false
    @State private var startTimeInSeconds: Float = 0.0
    @State private var inputTrackRecordEnabledStates: [Bool]
	@State private var amplitudes: [CGFloat]
    // timer for updating start timer
    private var timer = Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()
	// timer for rewind and fast forward
    private var rewFasTimer = Timer.publish(every: 0.05, on: .main, in: .common).autoconnect()
    // timer for updating amplitude levels
	private var ampTimer = Timer.publish(every: 0.05, on: .main, in: .common).autoconnect()
    
    init() {
		// init all input track record enabled states
		_inputTrackRecordEnabledStates = State(initialValue: Array(repeating: true, count: Int(getInputTrackCount())))
		// init amplitudes
		_amplitudes = State(initialValue: Array(repeating: 0.0, count: Int(getInputTrackCount())))

	}
    
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
            
            HStack {
				HStack {
					ForEach(0..<amplitudes.count, id: \.self) { index in
						VStack {
							AmpMeter(amplitude: amplitudes[index])
								.frame(width: 20, height: 300)
								.padding()
								.onReceive(ampTimer) { _ in
									if (!isPlaying) {
										amplitudes[index] = 0
										return
									}
									let recordingOrPlaying = inputTrackRecordEnabledStates[index] && isRecordingEnabled ? true : false
									let rawAmplitude: Float = getCurrentAmplitude(UInt32(index), recordingOrPlaying)
									print("raw amplitude", rawAmplitude)
									let normalizedAmplitude = decibelToHeight(decibel: rawAmplitude)
									print("normalized amplitude", normalizedAmplitude)
									amplitudes[index] = CGFloat(normalizedAmplitude)
								}

							Toggle(isOn: $inputTrackRecordEnabledStates[index]) {
								Text("Track \(index + 1)")
							}.padding()
						}
					}
				}.frame(height: 400)
			}
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
    
	func decibelToHeight(decibel: Float) -> Float {
		let normalizedValue: Float = (decibel + 120) / 120
		let uiHeight = 10 + (normalizedValue * (300 - 10)) // UI range 10px - 300px
		
		return uiHeight
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
