#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include "portaudio.h"

// SETUP
float startTimeInSeconds = 0;
int sampleRate = 48000;
int bitDepth = 24;
PaStream *recordingStream;
PaStream *playingStream;
bool isRecording = false;
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
} MultiTrackRecorder;

MultiTrackRecorder recorder;

// functions
void initAudio();
void initRecordingStream();
void cleanupAudio();
void checkDeviceCountAndGetAudioDeviceInfo();
void onStopRecording();
void onStartRecording();

#endif
