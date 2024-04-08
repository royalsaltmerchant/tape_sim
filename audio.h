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
PaStream *recordingStream;
PaStream *playingStream;
int frames = 256;

typedef struct
{
  FILE *file;
  size_t dataSize;
} WavFile;

typedef struct
{
  WavFile *tracks;
  size_t trackCount;
} Recorder;

typedef struct
{
  WavFile *tracks;
  size_t trackCount;
  float *leftChannelBuffer;  // Buffer for left channel audio data
  float *rightChannelBuffer; // Buffer for right channel audio data
  size_t bufferLength;       // Length of the buffer
  int playbackPosition;      // Current playback position in the buffer
} Player;

Recorder recorder;
Player player;

// functions
void initAudio();
void cleanupAudio();
void onStopRecording();
void onStartRecording();
void onStartPlaying();
void onStopPlaying();
void onRewind();
void onFastForward();
void onRtz();
float getCurrentStartTimeInSeconds();

#endif
