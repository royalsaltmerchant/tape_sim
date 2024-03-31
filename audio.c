// gcc -o myapp myapp.c -lportaudio

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <portaudio.h>

volatile sig_atomic_t terminateFlag = 0;

void getAudioDeviceInfo()
{
  int numDevices = Pa_GetDeviceCount();
  for (int i = 0; i < numDevices; i++)
  {
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
    printf("Device %d: %s\n", i, deviceInfo->name);
  }
}

typedef struct
{
  FILE *file;
  size_t dataSize;
} WavFile;

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

void closeWavFile(WavFile *wav)
{
  if (!wav || !wav->file)
    return;

  // Update RIFF chunk size at position 4
  fseek(wav->file, 4, SEEK_SET);
  int riffChunkSize = 4 + (8 + 16) + (8 + wav->dataSize); // "WAVE" + ("fmt " chunk and size) + ("data" header and size)
  fwrite(&riffChunkSize, 4, 1, wav->file);

  // Update data chunk size at position 40
  fseek(wav->file, 40, SEEK_SET);
  fwrite(&wav->dataSize, 4, 1, wav->file);

  // Close the file
  fclose(wav->file);
  free(wav);
}

static int recordCallback(const void *inputBuffer,
                          void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
  WavFile *wav = (WavFile *)userData;
  int bytesPerSample = 3; // 24 bit depth ***********************
  writeWavData(wav, inputBuffer, framesPerBuffer * 3);
  return paContinue;
}

void handleSignal(int signal)
{
  terminateFlag = 1;
}

int main(void)
{
  // SETUP
  PaError err;
  PaStream *stream = NULL;
  WavFile *wav = NULL;

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
  // init wav
  wav = openWavFile("output.wav", sampleRate, bitsPerSample, inputChannels);
  // stream
  err = Pa_OpenDefaultStream(&stream, inputChannels, outputChannels, paInt24, sampleRate, frames, recordCallback, wav);
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

  if (wav)
  {
    closeWavFile(wav); // Make sure this updates the WAV header correctly.
  }

  err = Pa_Terminate();
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
  }

  return 0;
}