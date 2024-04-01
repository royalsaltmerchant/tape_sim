// gcc -o audio audio.c -lportaudio

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <signal.h>
#include <portaudio.h>

// SETUP
volatile sig_atomic_t terminateFlag = 0;
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

// END SETUP

// UTILITY FUNCTIONS

void handleSignal(int signal)
{
  terminateFlag = 1;
}

void checkDeviceCountAndGetAudioDeviceInfo()
{
  int numDevices = Pa_GetDeviceCount();
  if (numDevices < 0)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText((PaError)numDevices));
    exit(1);
  }
  for (size_t i = 0; i < numDevices; i++)
  {
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
    // printf("Device %d: %s\n", i, deviceInfo->name);
  }
}

#include <stdbool.h>

// Checks if at least `interval` milliseconds have passed since last true return
bool canPrintAgain(int interval)
{
  static clock_t lastPrintTime = 0;
  clock_t currentTime = clock();
  clock_t elapsed = ((currentTime - lastPrintTime) * 1000) / CLOCKS_PER_SEC;

  if (elapsed >= interval)
  {
    lastPrintTime = currentTime;
    return true;
  }
  return false;
}

// END UTILIT FUNCTIONS

WavFile *openWavFile(const char *filename, int sampleRate, int bitsPerSample, int channels)
{
  // allocate the memory
  WavFile *wav = (WavFile *)malloc(sizeof(WavFile));
  if (!wav)
    return NULL;

  // open
  wav->file = fopen(filename, "wb");
  if (!wav->file)
  {
    free(wav);
    return NULL;
  }

  // init size
  wav->dataSize = 0;
  // Write RIFF header with placeholders for sizes
  fwrite("RIFF", 1, 4, wav->file);     // "RIFF"
  fwrite("----", 1, 4, wav->file);     // Placeholder for RIFF chunk size
  fwrite("WAVEfmt ", 1, 8, wav->file); // "WAVEfmt "

  int subchunk1Size = 16; // For PCM
  short audioFormat = 1;  // PCM = 1 means no compression
  short numChannels = channels;
  int byteRate = sampleRate * numChannels * (bitsPerSample / 8);
  short blockAlign = numChannels * (bitsPerSample / 8);

  fwrite(&subchunk1Size, sizeof(int), 1, wav->file);
  fwrite(&audioFormat, sizeof(short), 1, wav->file);
  fwrite(&numChannels, sizeof(short), 1, wav->file);
  fwrite(&sampleRate, sizeof(int), 1, wav->file);
  fwrite(&byteRate, sizeof(int), 1, wav->file);
  fwrite(&blockAlign, sizeof(short), 1, wav->file);
  fwrite(&bitsPerSample, sizeof(short), 1, wav->file);

  // Write "data" subchunk header with a placeholder for the data size
  fwrite("data", 1, 4, wav->file);
  fwrite("----", 1, 4, wav->file); // Placeholder for data chunk size

  return wav;
}

void writeWavData(WavFile *wav, const void *data, size_t dataSize)
{
  if (!wav || !wav->file)
    return;

  fwrite(data, 1, dataSize, wav->file);
  wav->dataSize += dataSize;
}

void closeWavFiles(MultiTrackRecorder *recorder)
{
  for (size_t i = 0; i < recorder->trackCount; i++)
  {

    // Update RIFF chunk size at position 4
    fseek(recorder->tracks[i].file, 4, SEEK_SET);
    int riffChunkSize = 4 + (8 + 16) + (8 + recorder->tracks[i].dataSize); // "WAVE" + ("fmt " chunk and size) + ("data" header and size)
    fwrite(&riffChunkSize, 4, 1, recorder->tracks[i].file);

    // Update data chunk size at position 40
    fseek(recorder->tracks[i].file, 40, SEEK_SET);
    fwrite(&recorder->tracks[i].dataSize, 4, 1, recorder->tracks[i].file);

    // Close the file
    fclose(recorder->tracks[i].file);
  }
}

float calculateRMS(const unsigned char *buffer, size_t framesPerBuffer)
{
  float sum = 0.0;
  for (size_t i = 0; i < framesPerBuffer; i++)
  {
    // Convert 3 bytes to one 24-bit int
    int sample24bit = (buffer[3 * i] << 8) | (buffer[3 * i + 1] << 16) | (buffer[3 * i + 2] << 24);
    sample24bit >>= 8; // Convert to signed 32-bit int

    // Now convert to float
    float sampleFloat = sample24bit / (float)INT32_MAX;
    sum += sampleFloat * sampleFloat;
  }
  float rms = sqrt(sum / framesPerBuffer);
  return rms;
}

float rmsToDb(float rms)
{
  // Ensure rms is positive but non-zero to avoid log10(0)
  if (rms <= 0)
    rms = 1e-20;
  float dbFS = 20.0 * log10(rms);
  return dbFS; // This will naturally yield negative values for rms < 1
}

static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
  // Cast inputBuffer to an array of pointers to byte arrays (one per channel)
  const unsigned char **buffers = (const unsigned char **)inputBuffer;
  MultiTrackRecorder *recorder = (MultiTrackRecorder *)userData;

  // Assuming you have a suitable buffer allocated for accumulating the samples
  // The buffer size needs to be at least framesPerBuffer * 3 bytes for each channel

  for (int channel = 0; channel < recorder->trackCount; ++channel)
  {
    // calculate db levels
    float rms = calculateRMS(buffers[channel], framesPerBuffer);
    float dbLevel = rmsToDb(rms);
    // Rate-limiting the print operation
    if (canPrintAgain(1))
    { // 10 milliseconds
      printf("Channel %d: dB Level = %f\n", channel, dbLevel);
    }

    // RECORDING
    // channel data and write buffer for wav
    const unsigned char *channelData = buffers[channel];
    unsigned char *writeBuffer = malloc(framesPerBuffer * 3); // Allocate buffer for the current channel

    // Accumulate the samples for the current channel into writeBuffer
    for (unsigned long frame = 0; frame < framesPerBuffer; ++frame)
    {
      int byteIndex = frame * 3;
      memcpy(&writeBuffer[frame * 3], &channelData[byteIndex], 3);
    }

    // Now, writeBuffer contains all the frames for the current channel, so write it all at once
    writeWavData(&recorder->tracks[channel], writeBuffer, framesPerBuffer * 3);

    free(writeBuffer);
  }

  return paContinue;
}

int main(void)
{
  // SETUP
  PaError err;
  PaStream *stream;
  MultiTrackRecorder recorder;

  // init PA
  err = Pa_Initialize();
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    return 1;
  }

  // get device info and check device count to ensure it is not negative
  checkDeviceCountAndGetAudioDeviceInfo();

  // setup recorder multi tracks
  // max track count for default input device
  const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(Pa_GetDefaultInputDevice());
  int inputChannelCount = deviceInfo->maxInputChannels;
  printf("max input channels: %d\n", inputChannelCount);
  recorder.trackCount = inputChannelCount;
  recorder.tracks = (WavFile *)malloc(sizeof(WavFile) * recorder.trackCount);

  // init signal
  signal(SIGINT, handleSignal);

  // values
  int sampleRate = 48000;
  int bitsPerSample = 24;
  int frames = 256;
  short inputChannels = 1;
  short outputChannels = 0;
  // init wav files
  for (size_t i = 0; i < recorder.trackCount; i++)
  {
    char filename[256];
    snprintf(filename, sizeof(filename), "track%zu.wav", i + 1);
    WavFile *wav = openWavFile(filename, sampleRate, bitsPerSample, inputChannels);
    if (wav == NULL)
    {
      printf("Failed to open WAV file for track %zu\n", i + 1);
      // Cleanup previously allocated resources
      return 1;
    }
    recorder.tracks[i] = *wav;
  }

  // stream and parameters
  PaStreamParameters inputParameters;
  inputParameters.channelCount = recorder.trackCount;
  inputParameters.device = Pa_GetDefaultInputDevice();                                                 // or another specific device
  inputParameters.sampleFormat = paInt24 | paNonInterleaved;                                           // Correct way to combine flags
  inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency; // lowest latency
  inputParameters.hostApiSpecificStreamInfo = NULL;                                                    // Typically NULL

  // OutputParameters setup is similar if needed, otherwise set to NULL in Pa_OpenStream
  PaStreamParameters outputParameters;
  // Setup outputParameters similarly if you need output

  err = Pa_OpenStream(
      &stream,
      &inputParameters,
      outputChannels > 0 ? &outputParameters : NULL, // NULL if no output is needed
      sampleRate,
      frames,
      paClipOff, // or other relevant flags. Note: paNonInterleaved is NOT set here
      recordCallback,
      &recorder // User data passed to callback
  );
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    return 1;
  }
  err = Pa_StartStream(stream);
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    return 1;
  }

  printf("Recording... Press CTRL+C to stop.\n");

  while (!terminateFlag)
  {
    // keep main thread running while background async thread runs audio recording
    Pa_Sleep(1000);
  }

  // Clean up and terminate PortAudio
  if (stream)
  {
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
  }

  // clean up wav files and track  memory
  closeWavFiles(&recorder); // updates the WAV headers.
  free(recorder.tracks);
  recorder.tracks = NULL;

  err = Pa_Terminate();
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
  }

  return 0;
}