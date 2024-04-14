#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include "../portaudio/include/portaudio.h"

// SETUP
float startTimeInSeconds = 0;
int sampleRate = 48000;
int bitDepth = 24;
PaStream *stream;
int frames = 256;
bool isRecording;

typedef struct
{
  FILE *file;
  size_t dataSize;
  float currentAmplitudeLevel;
  bool recordEnabled;
} WavFile;

typedef struct
{
  WavFile *tracks;
  size_t trackCount;
  int playbackPosition;      // Current playback position in the buffer
} Recorder;

Recorder recorder;

// functions
void initAudio();
void cleanupAudio();
void onStop();
void onStart(const uint32_t *inputTrackRecordEnabledStates, bool isRecordingFromUI);
void onRewind();
void onFastForward();
void onRtz();
float getCurrentStartTimeInSeconds();
unsigned int getInputTrackCount();
float getCurrentAmplitude(unsigned int index);
void onSetInputTrackRecordEnabled(unsigned int index, bool state);

#endif
