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
                Spacer()  // Pushes the rectangle to the bottom of the available space.
                Rectangle()
                    .frame(width: 20, height: min(amplitude - 60, geometry.size.height))
                    .foregroundColor(.green)  // Set the color of the amplitude meter.
                    .animation(.linear(duration: 0.1), value: amplitude)  // Animate changes in amplitude.
            }
        }
        .frame(width: 20)
        .background(Color.gray.opacity(0.3))  // Background color of the entire meter area.
        .cornerRadius(5)  // Optional: Rounded corners for the background.
    }
}
