// gcc -o audio audio.c -lportaudio

#include <stdio.h>
#include <stdlib.h>
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
  int trackCount;
} MultiTrackRecorder;

// END SETUP

// UTILITY FUNCTIONS

void handleSignal(int signal)
{
  terminateFlag = 1;
}

void getAudioDeviceInfo()
{
  int numDevices = Pa_GetDeviceCount();
  for (int i = 0; i < numDevices; i++)
  {
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
    printf("Device %d: %s\n", i, deviceInfo->name);
  }
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
  for (int i = 0; i < recorder->trackCount; i++)
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

static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
  // Cast inputBuffer to an array of pointers to byte arrays (one per channel)
  const unsigned char **buffers = (const unsigned char **)inputBuffer;
  MultiTrackRecorder *recorder = (MultiTrackRecorder *)userData;

  // For each channel
  for (int channel = 0; channel < recorder->trackCount; ++channel)
  {
    // Pointer to the current channel's data
    const unsigned char *channelData = buffers[channel];

    // For each frame, write the 24-bit sample (3 bytes) to the WAV file
    for (unsigned long frame = 0; frame < framesPerBuffer; ++frame)
    {
      // Calculate the starting index for the current sample (3 bytes per sample)
      int byteIndex = frame * 3;
      // Write the sample to the WAV file corresponding to the current channel
      writeWavData(&recorder->tracks[channel], &channelData[byteIndex], 3);
    }
  }
  return paContinue;
}

int main(void)
{
  // SETUP
  PaError err;
  PaStream *stream;
  MultiTrackRecorder recorder;

  // setup recorder multi tracks
  int trackCount = 1;
  recorder.trackCount = trackCount;
  recorder.tracks = (WavFile *)malloc(sizeof(WavFile) * recorder.trackCount);

  // init signal
  signal(SIGINT, handleSignal);

  // init PA
  err = Pa_Initialize();
  if (err != paNoError)
    return 1;

  // get device info
  getAudioDeviceInfo();

  // values
  int sampleRate = 48000;
  int bitsPerSample = 24;
  int frames = 256;
  short inputChannels = 1;
  short outputChannels = 0;
  // init wav files
  for (int i = 0; i < recorder.trackCount; i++)
  {
    char filename[256];
    snprintf(filename, sizeof(filename), "track%d.wav", i + 1);
    WavFile *wav = openWavFile(filename, sampleRate, bitsPerSample, inputChannels);
    if (wav == NULL)
    {
      printf("Failed to open WAV file for track %d\n", i + 1);
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