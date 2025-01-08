#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include "../portaudio/include/portaudio.h"
// macos specific
#include <CoreAudio/CoreAudio.h>

// SETUP
char *appDirPath = NULL;
float startTimeInSeconds = 0;
int sampleRate = 48000;
short bitDepth = 24;
PaStream *stream;
int frames = 256;
bool isRecording;
AudioDeviceID currentDefaultMacOSInputDevice;
AudioDeviceID currentDefaultMacOSOutputDevice;

typedef struct
{
  FILE *file;
  size_t dataSize; // not including header
  float currentAmplitudeLevel;
  bool recordEnabled;
} WavFile;

typedef struct
{
  WavFile *tracks;
  int trackCount;
  int playbackPosition;      // Current playback position in the buffer
} Recorder;

Recorder recorder = {NULL, 0, 0}; // Initializer ensures tracks is NULL

// functions
void initAudio();
void cleanupAudio();
void onStop();
void onStart(const uint32_t *inputTrackRecordEnabledStates, bool isRecordingFromUI);
void onRewind();
void onFastForward();
void onRtz();
float getCurrentStartTimeInSeconds();
int getInputTrackCount();
float getCurrentAmplitude(unsigned int index);
void onSetInputTrackRecordEnabled(unsigned int index, bool state);
int bounceTracks(const uint32_t *tracksToBounce, char *selectedPath);
void onSetAppDirPath(const char *selectedPath);

#endif
