// gcc -o audio audio.c -lportaudio
#include "audio.h"

// END SETUP

// UTILITY FUNCTIONS
// Function to build a directory path for your app within the user's home directory
char *buildAppDirectoryPath()
{
  const char *homeDir = getenv("HOME");
  if (!homeDir)
  {
    // Fallback if HOME isn't set
    homeDir = ".";
  }

  const char *appSubDir = "/Documents/tape_sim";

  // Allocate memory for the full path
  char *appDirPath = malloc(strlen(homeDir) + strlen(appSubDir) + 1);
  if (appDirPath)
  {
    strcpy(appDirPath, homeDir);
    strcat(appDirPath, appSubDir);
  }

  // Optionally, create the directory if it doesn't exist
  mkdir(appDirPath, 0777);

  return appDirPath; // Remember to free this memory after use!
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
    printf("Device %zu: %s\n", i, deviceInfo->name);
  }
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

float getCurrentStartTimeInSeconds()
{
  return startTimeInSeconds;
}

unsigned int getInputTrackCount()
{
  return (unsigned int)recorder.trackCount;
}

float getCurrentAmplitude(unsigned int index, bool isRecording)
{
  if (isRecording)
  {
    return recorder.tracks[index].currentAmplitudeLevel;
  }
  else
  {
    return player.tracks[index].currentAmplitudeLevel;
  }
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
  char *directoryPath = buildAppDirectoryPath(); // Build or get the directory path
  if (!directoryPath)
  {
    exit(1);
  }

  char *filePath = malloc(strlen(directoryPath) + strlen(filename) + 2); // +2 for '/' and null terminator
  if (!filePath)
  {
    free(directoryPath);
    exit(1);
  }

  sprintf(filePath, "%s/%s", directoryPath, filename);

  WavFile *wav = malloc(sizeof(WavFile));
  if (!wav)
    exit(1);

  // Check if the file already exists
  bool fileExists = access(filePath, F_OK) != -1;

  size_t headerSize = 44;
  int subchunk1Size = 16; // For PCM
  short audioFormat = 1;  // PCM = 1 means no compression
  short numChannels = 1;
  int byteRate = sampleRate * numChannels * (bitDepth / 8);
  short blockAlign = numChannels * (bitDepth / 8);

  // Open or create the file
  wav->file = fopen(filePath, fileExists ? "r+b" : "wb");
  if (!wav->file)
  {
    free(wav);
    exit(1);
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

void initRecordingTracks()
{
  for (size_t i = 0; i < recorder.trackCount; i++)
  {
    char filename[256];
    snprintf(filename, sizeof(filename), "track%zu.wav", i + 1);
    WavFile *wav = openWavFile(filename);
    recorder.tracks[i] = *wav;
    free(wav);
  }
}

void initPlayerTracks()
{
  for (size_t i = 0; i < player.trackCount; i++)
  {
    char filename[256];
    snprintf(filename, sizeof(filename), "track%zu.wav", i + 1);
    WavFile *wav = openWavFile(filename);
    player.tracks[i] = *wav;
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

void closeWavFiles()
{
  for (size_t i = 0; i < recorder.trackCount; i++)
  {

    size_t finalDataSize = recorder.tracks[i].dataSize;

    // Move to the start of the file size field
    fseek(recorder.tracks[i].file, 4, SEEK_SET);
    uint32_t fileSizeMinus8 = finalDataSize + 36; // Size of 'WAVEfmt ' and 'data' headers plus dataSize
    fwrite(&fileSizeMinus8, sizeof(fileSizeMinus8), 1, recorder.tracks[i].file);

    // Move to the start of the data size field
    fseek(recorder.tracks[i].file, 40, SEEK_SET);
    fwrite(&finalDataSize, sizeof(finalDataSize), 1, recorder.tracks[i].file);

    fclose(recorder.tracks[i].file);
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
  Recorder *recorder = (Recorder *)userData;

  // Assuming you have a suitable buffer allocated for accumulating the samples
  // The buffer size needs to be at least framesPerBuffer * 3 bytes for each channel

  for (int channel = 0; channel < recorder->trackCount; ++channel)
  {
    // calculate db levels
    float rms = calculateRMS(buffers[channel], framesPerBuffer);
    float dbLevel = rmsToDb(rms);
    printf("Channel %d: dB Level = %f\n", channel, dbLevel);
    recorder->tracks[channel].currentAmplitudeLevel = dbLevel;

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

  // Update the start time to reflect the current playback position
  float timeIncrement = (float)framesPerBuffer / sampleRate;
  startTimeInSeconds += timeIncrement;

  return paContinue;
}

static int playbackCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
  unsigned char **outputBuffers = (unsigned char **)outputBuffer;
  size_t minReadFrames = framesPerBuffer; // Initialize with the maximum possible, will find the minimum

  for (int channel = 0; channel < player.trackCount; ++channel)
  {
    // calculate db levels
    float rms = calculateRMS(outputBuffers[channel], framesPerBuffer);
    float dbLevel = rmsToDb(rms);
    printf("Channel %d: dB Level = %f\n", channel, dbLevel);
    player.tracks[channel].currentAmplitudeLevel = dbLevel;

    size_t readFrames = 0;
    for (size_t frame = 0; frame < framesPerBuffer; ++frame)
    {
      if (player.playbackPosition + frame < player.tracks[channel].dataSize / 3)
      {
        uint8_t sampleBytes[3];
        if (fread(sampleBytes, sizeof(uint8_t), 3, player.tracks[channel].file) == 3)
        {
          // We read 3 bytes successfully
          memcpy(&outputBuffers[channel][frame * 3], sampleBytes, 3);
          readFrames++;
        }
      }
      else
      {
        // If no more data, fill with zeros
        memset(&outputBuffers[channel][frame * 3], 0, 3);
      }
    }
    // Determine the minimum readFrames across all channels
    if (readFrames < minReadFrames)
    {
      minReadFrames = readFrames;
    }
  }

  // Update the playback position after processing all channels
  player.playbackPosition += minReadFrames;

  // Update the global time based on frames processed
  startTimeInSeconds += (float)framesPerBuffer / (float)sampleRate;

  return paContinue;
}

void initRecordingStream()
{
  int inputDevice = Pa_GetDefaultInputDevice();
  if (inputDevice == -1)
  {
    inputDevice = 0;
  }

  PaError err;
  // Stream parameters
  PaStreamParameters inputParameters,
      outputParameters;
  inputParameters.channelCount = recorder.trackCount;
  inputParameters.device = inputDevice;                                                                // or another specific device
  inputParameters.sampleFormat = paInt24 | paNonInterleaved;                                           // Correct way to combine flags
  inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency; // lowest latency
  inputParameters.hostApiSpecificStreamInfo = NULL;

  printf("Initializing stream with %d channels.\n", recorder.trackCount); // Typically NULL

  // start PA audio stream
  err = Pa_OpenStream(
      &recordingStream,
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
    exit(1);
  }
}

void initPlayerStream()
{
  PaError err;

  initPlayerTracks();

  // Calculate playback start position based on startTimeInSeconds
  // Assuming each sample in the buffer corresponds to a frame of audio
  size_t startPosition = (size_t)(sampleRate * startTimeInSeconds);
  player.playbackPosition = startPosition;

  // Setup output parameters
  PaStreamParameters outputParameters;
  outputParameters.device = Pa_GetDefaultOutputDevice();
  outputParameters.channelCount = player.trackCount;                                                     // Number of tracks
  outputParameters.sampleFormat = paInt24 | paNonInterleaved;                                            // Correct way to combine flags
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency; // lowest latency
  outputParameters.hostApiSpecificStreamInfo = NULL;

  printf("Initializing stream with %d channels.\n", player.trackCount);

  // Open stream
  err = Pa_OpenStream(
      &playingStream,
      NULL, // No input parameters for playback only
      &outputParameters,
      sampleRate,
      frames,
      paClipOff, // Don't clip output
      playbackCallback,
      NULL // No callback data needed
  );
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }
}

void initAudio()
{
  PaError err;
  // init PA
  err = Pa_Initialize();
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }

  // get device info and check device count to ensure it is not negative
  checkDeviceCountAndGetAudioDeviceInfo();

  // setup recorder multi tracks
  // max track count for default input device
  int inputDevice = Pa_GetDefaultInputDevice();
  int outputDevice = Pa_GetDefaultOutputDevice();
  if (inputDevice == -1)
  {
    inputDevice = 0;
  }
  if (outputDevice == -1)
  {
    outputDevice = 0;
  }

  const PaDeviceInfo *inputDeviceInfo = Pa_GetDeviceInfo(inputDevice);
  const PaDeviceInfo *outputDeviceInfo = Pa_GetDeviceInfo(outputDevice);

  int inputChannelCount = inputDeviceInfo->maxInputChannels;
  int outputChannelCount = outputDeviceInfo->maxOutputChannels;

  // handle input count error
  if (inputChannelCount <= 0 || inputChannelCount > 64)
  {
    printf("Error: Invalid number of input channels (%d). Must be between 1 and 64.\n", inputChannelCount);
    exit(1);
  }

  // handle output count error
  if (outputChannelCount <= 0)
  {
    printf("Error: There are no output channels.\n", outputChannelCount);
    exit(1);
  }
  if (outputChannelCount < inputChannelCount)
  {
    printf("Error: There are not enough output channels to match input channels\n", outputChannelCount);
    exit(1);
  }
  // If there are more output channels than input, make them equal 1-1
  if (outputChannelCount > inputChannelCount)
  {
    outputChannelCount = inputChannelCount;
  }

  printf("max input channels: %d\n", inputChannelCount);
  printf("max output channels %d\n", outputChannelCount);

  // init tracks and track count for recording and playback
  recorder.trackCount = inputChannelCount;
  recorder.tracks = malloc(sizeof(WavFile) * recorder.trackCount); // make room for as many wav files as needed tracks
  player.trackCount = outputChannelCount;                          // this gets used as the source of truth for later when we init the stream
  player.tracks = malloc(sizeof(WavFile) * player.trackCount);
}

void cleanupAudio()
{
  PaError err;
  // safety close streams
  Pa_StopStream(recordingStream);
  Pa_CloseStream(recordingStream);
  Pa_StopStream(playingStream);
  Pa_CloseStream(playingStream);

  // clean up wav files
  closeWavFiles();

  // free track memory
  free(recorder.tracks);
  free(player.tracks);

  // close out PA
  err = Pa_Terminate();
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
  }
}

void onRewind()
{
  // Adjust time backwards by 0.1 seconds as an example
  if (startTimeInSeconds > 0.1)
  {
    startTimeInSeconds -= 0.1;
  }
  else
  {
    startTimeInSeconds = 0.0;
  }
}

void onFastForward()
{
  // Adjust time forwards by 0.1 seconds, with 216000 seconds as 60 hours
  if (startTimeInSeconds < 216000 - 0.1)
  {
    startTimeInSeconds += 0.1;
  }
  else
  {
    // Optionally loop around or cap at 216000
    startTimeInSeconds = 216000;
  }
}

void onStopRecording()
{
  // Stop recording
  Pa_StopStream(recordingStream);
  Pa_CloseStream(recordingStream);
  // clean up wav files and track  memory
  closeWavFiles(); // updates the WAV headers.

  printf("Recording stopped.\n");
}

void onStartRecording()
{
  PaError err;
  // init recording stream
  initRecordingStream();
  // init or re-init all wav track files with header data
  initRecordingTracks();
  // Start recording
  err = Pa_StartStream(recordingStream);
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }

  printf("Recording started.\n");
}

void onStartPlaying()
{
  PaError err;
  // init playback stream
  initPlayerStream();
  // start playback
  err = Pa_StartStream(playingStream);
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }

  printf("Playing started.\n");
}

void onStopPlaying()
{
  // Stop recording
  Pa_StopStream(playingStream);
  Pa_CloseStream(playingStream);

  printf("Playing stopped.\n");
}

void onRtz()
{
  updateStartTime(0.00);
}

// int main() {
//   initAudio();
//   onStartRecording();
//   sleep(3);
//   onStopRecording();
//   onStartPlaying();
//   sleep(3);
//   onStopPlaying();

//   cleanupAudio();
// }