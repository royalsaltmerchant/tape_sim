//
//  Bounce.swift
//  tape_sim
//
//  Created by Julian ranieri on 4/26/24.
//

import SwiftUI

struct BounceView: View {
    @State private var selectedTracks: [Bool]
    
    init() {
        _selectedTracks = State(initialValue: [Bool](repeating: false, count: Int(getInputTrackCount())))
    }

    var body: some View {
		Text("Please select two tracks to mix into a stereo output wav file")
		.padding()
		
        List(0..<selectedTracks.count, id: \.self) { index in
            Toggle("Track \(index + 1)", isOn: $selectedTracks[index])
				.onChange(of: selectedTracks[index]) { newValue in
					if newValue {
						if selectedTracks.filter({ $0 }).count > 2 {
							selectedTracks[index] = false // Automatically unselect if more than 2
						}
					}
				}
        }
        
        Button("Create Stereo Bounce") {
            showSavePanelAndBounce()
        }
        .padding()
        .disabled(selectedTracks.filter({ $0 }).count != 2)
    }
    
    private func showSavePanelAndBounce() {
		let panel = NSSavePanel()
		panel.prompt = "Save"
		panel.allowedFileTypes = ["wav"]
		panel.canCreateDirectories = true
		panel.nameFieldStringValue = "StereoBounce.wav"

		panel.begin { response in
			if response == .OK {
				if let selectedPath = panel.url?.path {
					print(selectedPath)
					let selectedTracksAsInt = selectedTracks.map { $0 ? 1 : 0 }.map(UInt32.init)
					selectedTracksAsInt.withUnsafeBufferPointer { bufferPointer in
						if let baseAddress = bufferPointer.baseAddress {
							// Create a mutable copy of the selectedPath string for the C function.
							let cPath = strdup(selectedPath)
							defer { free(cPath) }  // Ensure to free the allocated memory after use.
							if let cPath = cPath {
								bounceTracks(baseAddress, cPath)
							}
						}
					}
				}
			}
		}
	}
}

