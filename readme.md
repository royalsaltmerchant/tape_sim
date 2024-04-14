# TapeSim DAW
Tape Sim intends to simulate a simple tape machine experience by recording and playing audio with a simple UI.

The program can record as many tracks as there are inputs in your default audio device and allows you to rewind, fast-forward and re-record over a section of time.

By selecting the checkbox next to the track name you record enable that track so that during recording those tracks will write, however they will not playback while in record-enable mode.

Playback is mapped 1-1 input to output. So if there are 4 inputs, 4 wav files are created and playedback on outputs 1-4. If there are not as many outputs as inputs then the program will crash.

Recordings and playback are in mono at 48khz 24bit quality.

Current UI available for this program is for MacOS(Intel x86_64) made with SwiftUI.

![Image of current UI in MacOS](./macosscreenshot.png)

## Resources

wav spec https://docs.fileformat.com/audio/wav/

port audio https://portaudio.com/docs/v19-doxydocs/index.html