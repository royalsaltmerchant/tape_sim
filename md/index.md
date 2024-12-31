# Tape Sim
![Logo](/logo.png)

Tape Sim intends to simulate a simple tape machine experience by recording and playing audio with a simple UI.

![Image of current UI in MacOS](/macosscreenshot.png)

### Program Details

The program can record as many tracks as there are inputs in your <b>default audio device</b> and allows you to rewind, fast-forward and re-record over a section of time.
Your Recordings will be saved to the directory `/Users/<username>/Music/tape_sim`
They will be named as such: `track1.wav track2.wav` etc... for as many inputs as are available.

By selecting the checkbox next to the track name you record enable the track. All record-enabled tracks will be written to upon recording.

Playback is mapped 1-1 input to output. For example: if there are 4 inputs, 4 wav files are created and playedback on outputs 1-4. <b>If there are not as many outputs as inputs then the program will crash.</b>

Recordings and playback are in mono at <b>48khz 24bit</b> quality.

Current UI available for this program is for <b>MacOS(Intel x86_64)</b> made with SwiftUI.

### SwiftUI Key Commands:
- `Space` play/stop/rec or stop rew/fwd
- `r` enable record mode (REC)
- `,` rewind
- `.` fast forward
- `z` RTZ

### Stereo Bounce

The program currently offers a stereo bounce feature which allos the user to select two tracks and create a single stereo wav file in a selected directory. To use this feature you must be using at least a two-track I/O setup. You can select this feature from `Actions -> Stereo Bounce`

## DEV Setup

### C Program logic layer
<b>GCC version - Apple clang version 15.0.0 (clang-1500.1.0.2.5)</b>

If you want to work on the logic layer alone you can compile the program with gcc and use the right flags to link the dependencies including port audio. Make sure to download and compile port audio on your machine. You may need to compile targeting a certain mac version to be consistent with the SwiftUI app.

Here is what I used to compile PA using cmake and setting target osx 11. 

```
cd portaudio        
mkdir build && cd build
cmake -DCMAKE_OSX_DEPLOYMENT_TARGET=11  .. -DBUILD_SHARED_LIBS=OFF
make
```

Then to compile the c program here is an example. Make sure to reference the correct directory that your PA build is in.
```
gcc -o audio audio.c -I../portaudio/include -L../portaudio/build -lportaudio -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework CoreServices
```
### SwiftUI on Xcode
<b>Version 15.2</b>

You need to make sure that you are also linking all the correct binaries in xcode. In Build Phases, it should look like this:

![Image of current UI in MacOS](/xcodelinkbinaries.png)


Also, for search paths, you should have something like this for header and library:


![Image of current UI in MacOS](/xcodesearchpaths.png)

## Other Resources

#### Wav Spec: https://docs.fileformat.com/audio/wav/

#### Port Audio: https://portaudio.com/docs/v19-doxydocs/index.html