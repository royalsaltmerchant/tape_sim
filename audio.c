// gcc -o audio audio.c -lportaudio

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <portaudio.h>

// SETUP
float startTimeInSeconds = 0;
int sampleRate = 48000;
int bitDepth = 24;

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

// Function to check if a key was pressed
bool keyPressed()
{
  struct timeval tv = {0L, 0L};
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(0, &fds); // STDIN_FILENO is 0
  return select(1, &fds, NULL, NULL, &tv) > 0;
}

// Function to get the pressed key without waiting for Enter key
int getCharNonBlocking()
{
  struct termios oldt, newt;
  int ch;
  tcgetattr(STDIN_FILENO, &oldt); // Get current terminal attributes
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);        // Disable buffering and echo
  tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Apply new settings

  if (keyPressed())
  { // Check if a key was pressed
    ch = getchar();
  }
  else
  {
    ch = -1; // No key was pressed
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore terminal attributes
  return ch;
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

// END UTILITY FUNCTIONS

void updateStartTime(float time)
{
  startTimeInSeconds = time;
}

void inputStartTime()
{
  float time;
  printf("Enter new start time in seconds (current: %f): ", startTimeInSeconds);
  if (scanf("%f", &time) == 1)
  { // Ensure scanf successfully reads a float
    // Validate the input time range
    if (time >= 0 && time <= 3600)
    { // 0 to 3600 seconds range
      updateStartTime(time);
      printf("Start time updated to %f seconds.\n", time);
    }
    else
    {
      printf("Invalid input: start time must be between 0 and 3600 seconds.\n");
    }
  }
  else
  {
    // Clear the input buffer to prevent infinite loops in case of invalid input
    while (getchar() != '\n')
      ;
    printf("Invalid input: please enter a numeric value.\n");
  }
}

WavFile *openWavFile(const char *filename)
{
  // allocate the memory
  WavFile *wav = (WavFile *)malloc(sizeof(WavFile));
  if (!wav)
    return NULL;

  // Check if the file already exists
  bool fileExists = access(filename, F_OK) != -1;

  size_t headerSize = 44;
  int subchunk1Size = 16; // For PCM
  short audioFormat = 1;  // PCM = 1 means no compression
  short numChannels = 1;
  int byteRate = sampleRate * numChannels * (bitDepth / 8);
  short blockAlign = numChannels * (bitDepth / 8);

  // Open or create the file
  wav->file = fopen(filename, fileExists ? "r+b" : "wb");
  if (!wav->file)
  {
    free(wav);
    return NULL;
  }

  if (!fileExists)
  {
    wav->dataSize = 0;
    // Write RIFF header with placeholders for sizes
    fwrite("RIFF", 1, 4, wav->file);     // "RIFF"
    fwrite("----", 1, 4, wav->file);     // Placeholder for RIFF chunk size
    fwrite("WAVEfmt ", 1, 8, wav->file); // "WAVEfmt "
    fwrite(&subchunk1Size, sizeof(int), 1, wav->file);
    fwrite(&audioFormat, sizeof(short), 1, wav->file);
    fwrite(&numChannels, sizeof(short), 1, wav->file);
    fwrite(&sampleRate, sizeof(int), 1, wav->file);
    fwrite(&byteRate, sizeof(int), 1, wav->file);
    fwrite(&blockAlign, sizeof(short), 1, wav->file);
    fwrite(&bitDepth, sizeof(short), 1, wav->file);

    // Write "data" subchunk header with a placeholder for the data size
    fwrite("data", 1, 4, wav->file);
    fwrite("----", 1, 4, wav->file); // Placeholder for data chunk size

    return wav;
  }
  else
  {
    fseek(wav->file, 0, SEEK_END);
    size_t currentFileSize = ftell(wav->file);
    wav->dataSize = currentFileSize - headerSize; // Retain original length
    // Prepare for overwriting by seeking to the intended start position
    size_t bytesPerSample = (bitDepth / 8) * numChannels;
    // Calculate the exact sample offset
    size_t sampleOffset = (size_t)(sampleRate * startTimeInSeconds);
    // Convert sample offset to byte offset, ensuring it's aligned to start of a frame
    size_t byteOffset = sampleOffset * bytesPerSample;
    // Adjust for header size
    size_t overwriteStartPos = headerSize + byteOffset;
    fseek(wav->file, overwriteStartPos, SEEK_SET);

    return wav;
  }
}

void initRecordingTracks(MultiTrackRecorder *recorder)
{
  for (size_t i = 0; i < recorder->trackCount; i++)
  {
    char filename[256];
    snprintf(filename, sizeof(filename), "track%zu.wav", i + 1);
    WavFile *wav = openWavFile(filename);
    recorder->tracks[i] = *wav;
    free(wav);
  }
}

void writeWavData(WavFile *wav, const void *data, size_t dataSize)
{
  if (!wav || !wav->file)
    return;

  // Write the new data
  fwrite(data, 1, dataSize, wav->file);

  // Determine the position after writing
  size_t newPosition = ftell(wav->file);

  // Update dataSize based on whether the new data extends beyond the original dataSize
  size_t newDataSize = newPosition - 44; // Subtract header size to get audio data size
  if (newDataSize > wav->dataSize)
  {
    wav->dataSize = newDataSize;
  }

  // Note: wav->dataSize is initially set based on the file's original dataSize when opened
}

void closeWavFiles(MultiTrackRecorder *recorder)
{
  for (size_t i = 0; i < recorder->trackCount; i++)
  {

    size_t finalDataSize = recorder->tracks[i].dataSize;

    // Move to the start of the file size field
    fseek(recorder->tracks[i].file, 4, SEEK_SET);
    uint32_t fileSizeMinus8 = finalDataSize + 36; // Size of 'WAVEfmt ' and 'data' headers plus dataSize
    fwrite(&fileSizeMinus8, sizeof(fileSizeMinus8), 1, recorder->tracks[i].file);

    // Move to the start of the data size field
    fseek(recorder->tracks[i].file, 40, SEEK_SET);
    fwrite(&finalDataSize, sizeof(finalDataSize), 1, recorder->tracks[i].file);

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
  bool isRecording = false;

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
  // handle input count error
  if (inputChannelCount <= 0 || inputChannelCount > 64)
  {
    fprintf(stderr, "Error: Invalid number of input channels (%d). Must be between 1 and 64.\n", inputChannelCount);
    return 1;
  }
  printf("max input channels: %d\n", inputChannelCount);
  recorder.trackCount = inputChannelCount;                                    // we want to make as many tracks as there are input channels on the default input device
  recorder.tracks = (WavFile *)malloc(sizeof(WavFile) * recorder.trackCount); // make room for as many wav files as needed tracks

  int frames = 256;
  // Stream parameters
  PaStreamParameters inputParameters,
      outputParameters;
  inputParameters.channelCount = recorder.trackCount;
  inputParameters.device = Pa_GetDefaultInputDevice();                                                 // or another specific device
  inputParameters.sampleFormat = paInt24 | paNonInterleaved;                                           // Correct way to combine flags
  inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency; // lowest latency
  inputParameters.hostApiSpecificStreamInfo = NULL;                                                    // Typically NULL

  // start PA audio stream
  err = Pa_OpenStream(
      &stream,
      &inputParameters,
      NULL, // NULL if no output is needed
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

  // start applicatin loop
  printf("COMMANDS:\n");
  printf("- Press 'r' to start/stop recording.\n- Press 't' to input start time.\n- Press 'q' to quit.\n");
  while (true)
  {
    int ch = getCharNonBlocking();
    if (ch == 'r')
    { // Toggle recording
      if (isRecording)
      {
        // Stop recording
        Pa_StopStream(stream);
        // clean up wav files and track  memory
        closeWavFiles(&recorder); // updates the WAV headers.
        isRecording = false;
        // set start time to end of current recording
        float totalAudioDataBytes = recorder.tracks[0].dataSize; // assuming that track 0 has data...
        float bytesPerSecond = sampleRate * bitDepth / 8;
        float newStartTime = totalAudioDataBytes / bytesPerSecond;
        updateStartTime(newStartTime);
        printf("Recording stopped.\n");
      }
      else
      {
        // init or re-init all wav track files with header data
        initRecordingTracks(&recorder);
        // Start recording
        // Make sure to reinitialize the stream if needed
        err = Pa_StartStream(stream);
        if (err != paNoError)
        {
          printf("PortAudio error: %s\n", Pa_GetErrorText(err));
          return 1;
        }
        isRecording = true;
        printf("Recording started.\n");
      }
    }
    else if (ch == 't')
    {
      // allow user to input start time
      inputStartTime();
    }
    else if (ch == 'q')
    { // Quit
      if (isRecording)
      {
        Pa_StopStream(stream);
        // clean up wav files and track  memory
        closeWavFiles(&recorder); // updates the WAV headers.
      }
      // close the stream finally
      Pa_CloseStream(stream);

      break;
    }

    // Minimal delay to prevent tight looping
    usleep(100000); // Sleep for 100ms
  }

  // free track memory
  free(recorder.tracks);
  recorder.tracks = NULL;

  // close out PA
  err = Pa_Terminate();
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
  }

  return 0;
}