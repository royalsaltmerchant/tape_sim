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
                // Rectangle animates from the bottom
                Rectangle()
                    .frame(width: 20, height: max(0, min(amplitude, geometry.size.height)))
                    .foregroundColor(.green)
                    // Ensuring the animation responds to amplitude changes
                    .animation(.linear(duration: 0.1), value: amplitude)
            }
            // Aligning the VStack to the bottom to let the rectangle grow upwards
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottom)
            .background(Color.gray.opacity(0.3))  // Background color of the meter
        }
        .frame(width: 20)  // Fixed width of the meter
    }
}

