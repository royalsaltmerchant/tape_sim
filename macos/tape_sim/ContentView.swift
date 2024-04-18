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
    private var startTimeTimer = Timer.publish(every: 0.1, on: .main, in: .common).autoconnect()
	// timer for rewind and fast forward
    private var rewFasTimer = Timer.publish(every: 0.05, on: .main, in: .common).autoconnect()
    // timer for updating amplitude levels
	private var ampTimer = Timer.publish(every: 0.05, on: .main, in: .common).autoconnect()
    
    init() {
		// init all input track record enabled states
		_inputTrackRecordEnabledStates = State(initialValue: Array(repeating: false, count: Int(getInputTrackCount())))
		// init amplitudes
		_amplitudes = State(initialValue: Array(repeating: -400, count: Int(getInputTrackCount())))

	}
    
    var body: some View {
        VStack {
            Text("\(startTimeInSeconds, specifier: "%.2f") seconds")
                .onReceive(startTimeTimer) { _ in
                    startTimeInSeconds = getUpdatedStartTime() // Fetches the current start time
                }
            
            HStack {
				Button(action: rtz) {
                    Label("RTZ", systemImage: "return")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .gray))
                .keyboardShortcut("z", modifiers: [])
                
                Button(action: rewind) {
                    Label("REW", systemImage: "gobackward")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .blue))
                .keyboardShortcut(",", modifiers: [])
                
                Button(action: playOrRecord) {
                    Label("PLAY", systemImage: "play.circle")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .green))
                
				Button(action: {
					isRecordingEnabled.toggle()
				}) {
                    Label("REC", systemImage: "record.circle")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: isRecordingEnabled ? .red : .gray))
                .keyboardShortcut("r", modifiers: [])
                
                Button(action: stop) {
                    Label("STOP", systemImage: "stop.circle")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .gray))
                
                Button(action: fastForward) {
                    Label("FWD", systemImage: "goforward")
                }
                .buttonStyle(ActionButtonStyle(backgroundColor: .blue))
                .keyboardShortcut(".", modifiers: [])
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
									let rawAmplitude: Float = getCurrentAmplitude(UInt32(index))
									print("Channel: ", amplitudes[index], " Raw from c to swift: ", rawAmplitude)
									let normalizedAmplitude = decibelToHeight(decibel: rawAmplitude)
									print("Channel: ", amplitudes[index], " Normalized amplitude: ", normalizedAmplitude)
									amplitudes[index] = CGFloat(normalizedAmplitude)
								}

							Toggle(isOn: $inputTrackRecordEnabledStates[index]) {
							
							}
							.padding()
							.disabled(isPlaying)
							.onChange(of: inputTrackRecordEnabledStates[index]) { newValue in
								onSetInputTrackRecordEnabled(UInt32(index), newValue)
							}
							Text("\(index + 1)")
						}
					}
				}.frame(height: 400)
			}
        }
        .padding()
        .onAppear() {
			NSEvent.addLocalMonitorForEvents(matching: .keyDown) { (event) -> NSEvent? in
				if event.keyCode == 49 { // 49 is the keycode for space
					if (isRewinding) {
						stopRewind()
						return nil
					}
					if (isFastForwarding) {
						stopFastForward()
						return nil
					}
					if isPlaying {
						stop()
						return nil
					} else {
						playOrRecord()
						return nil
					}
				}
				return event
			}
		}
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
		
		if !isPlaying {
			// Map Swift Bool to C 'bool' (UInt8)
			let recordEnabledStates = inputTrackRecordEnabledStates.map { $0 ? 1 : 0 }.map(UInt32.init)
			recordEnabledStates.withUnsafeBufferPointer { bufferPointer in
				if let baseAddress = bufferPointer.baseAddress {
					onStart(baseAddress, isRecordingEnabled)
					isPlaying = true
				}
			}
		} else {
			onStop()
			isPlaying = false
		}
        
    }
    
    func stop() {
		if (isRewinding) {
			stopRewind()
		}
		if (isFastForwarding) {
			stopFastForward()
		}
		
        if isPlaying {
			onStop()
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
		let normalizedValue: Float = (decibel + 400) / 400
		let uiHeight = 10 + (normalizedValue * (300 - 10))
		print(uiHeight, "UI HEIGHT")
		return uiHeight - 200 // compensate as db never usually goes below 134
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
