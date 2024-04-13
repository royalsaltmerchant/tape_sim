//
//  AmpMeter.swift
//  tape_sim
//
//  Created by Julian ranieri on 4/12/24.
//

import SwiftUI

struct AmpMeter: View {
    var amplitude: CGFloat

    var body: some View {
        GeometryReader { geometry in
            VStack {
				Spacer()
                Rectangle()
                    .frame(width: 20, height: max(0, min(amplitude, geometry.size.height))) // make sure it doesn't go below zero or above max height of the frame
                    .foregroundColor(.green)  // Set the color of the amplitude meter.
                    .animation(.linear(duration: 0.1), value: amplitude)  // Animate changes in amplitude.
            }
            .frame(alignment: .bottom)
        }
        .frame(width: 20)
        .background(Color.gray.opacity(0.3))  // Background color of the entire meter area.
    }
}
