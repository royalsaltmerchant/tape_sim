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

// MACOS
AudioDeviceID getDefaultMacOSInputDeviceID()
{
  AudioDeviceID deviceID = kAudioObjectUnknown;
  UInt32 dataSize = sizeof(deviceID);
  AudioObjectPropertyAddress propertyAddress = {
      kAudioHardwarePropertyDefaultInputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster};

  OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &propertyAddress,
                                               0,
                                               NULL,
                                               &dataSize,
                                               &deviceID);
  if (status != noErr)
  {
    fprintf(stderr, "Failed to get the default macos input device ID\n");
    exit(1);
  }

  return deviceID; // Return the default input device ID
}

AudioDeviceID getDefaultMacOSOutputDeviceID()
{
  AudioDeviceID deviceID = kAudioObjectUnknown;
  UInt32 dataSize = sizeof(deviceID);
  AudioObjectPropertyAddress propertyAddress = {
      kAudioHardwarePropertyDefaultOutputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster};

  OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &propertyAddress,
                                               0,
                                               NULL,
                                               &dataSize,
                                               &deviceID);
  if (status != noErr)
  {
    fprintf(stderr, "Failed to get the default macos output device ID\n");
    exit(1);
  }

  return deviceID; // Return the default output device ID
}

void checkDeviceCountAndGetAudioDeviceInfo()
{
  short numDevices = Pa_GetDeviceCount();
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
// END UTILITY FUNCTIONS

int checkPAIOAndGetChannelCount()
{
  // max track count for default input device
  int inputDevice = Pa_GetDefaultInputDevice();
  int outputDevice = Pa_GetDefaultOutputDevice();
  // check that we have both IO devices from default setup
  if (inputDevice == -1 || outputDevice == -1)
  {
    exit(1);
  }
  // get IO device info
  const PaDeviceInfo *inputDeviceInfo = Pa_GetDeviceInfo(inputDevice);
  const PaDeviceInfo *outputDeviceInfo = Pa_GetDeviceInfo(outputDevice);
  // set channel count
  int inputChannelCount = inputDeviceInfo->maxInputChannels;
  int outputChannelCount = outputDeviceInfo->maxOutputChannels;

  // handle input count error
  if (inputChannelCount <= 0)
  {
    printf("Error: Invalid number of input channels (%d). Must be greater than 0.\n", inputChannelCount);
    exit(1);
  }

  if (outputChannelCount < inputChannelCount)
  {
    printf("Error: There are not enough output channels (%d) to match input channels\n", outputChannelCount);
    exit(1);
  }

  // we can return size_t because channel count should always be above 0 otherwise we crash
  return inputChannelCount;
}

void updateStartTime(float time)
{
  startTimeInSeconds = time;
}

float getCurrentStartTimeInSeconds()
{
  return startTimeInSeconds;
}

void setInputTrackCount(int inputChannelCount)
{
  if (recorder.tracks != NULL)
  {
    free(recorder.tracks);
    recorder.tracks = NULL;
  }
  recorder.trackCount = inputChannelCount;
  recorder.tracks = calloc(sizeof(WavFile) * recorder.trackCount, sizeof(WavFile)); // make room for as many wav files as needed tracks
  // handle allocation error
  if (recorder.tracks == NULL && recorder.trackCount > 0)
  {
    // Handle allocation failure; possibly set trackCount to 0 or take other corrective actions
    printf("Failed to allocate memory for tracks.\n");
    exit(1);
  }
}

int getInputTrackCount()
{
  // if macos device defaults change
  AudioDeviceID defaultMacOSInputDevice = getDefaultMacOSInputDeviceID();
  AudioDeviceID defaultMacOSOutputDevice = getDefaultMacOSOutputDeviceID();
  if (defaultMacOSInputDevice != currentDefaultMacOSInputDevice || defaultMacOSOutputDevice != currentDefaultMacOSOutputDevice)
  {
    cleanupAudio();
    initAudio();
  }
  // after we re-init and set the recorder.trackCount we should now be returning the latest state update
  return recorder.trackCount;
}

float getCurrentAmplitude(unsigned int index)
{
  return recorder.tracks[index].currentAmplitudeLevel;
}

void onSetInputTrackRecordEnabled(unsigned int index, bool state)
{
  printf("STATE: %d", state);
  if (state == 1)
  {
    recorder.tracks[index].recordEnabled = true;
  }
  else
  {
    recorder.tracks[index].recordEnabled = false;
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

void initTracks(const uint32_t *inputTrackRecordEnabledStates)
{
  for (size_t i = 0; i < recorder.trackCount; i++)
  {
    char filename[256];
    snprintf(filename, sizeof(filename), "track%zu.wav", i + 1);
    WavFile *wav = openWavFile(filename);
    recorder.tracks[i] = *wav;
    recorder.tracks[i].currentAmplitudeLevel = -400; //  minimum signal level in dap
    free(wav);

    // logic for record enabled setting
    if (inputTrackRecordEnabledStates[i] == 1)
    {
      recorder.tracks[i].recordEnabled = true;
    }
    else
    {
      recorder.tracks[i].recordEnabled = false;
    }
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
    int fileSizeMinus8 = finalDataSize + 36; // Size of 'WAVEfmt ' and 'data' headers plus dataSize
    fwrite(&fileSizeMinus8, sizeof(fileSizeMinus8), 1, recorder.tracks[i].file);

    // Move to the start of the data size field
    fseek(recorder.tracks[i].file, 40, SEEK_SET);
    fwrite(&finalDataSize, sizeof(finalDataSize), 1, recorder.tracks[i].file);

    fclose(recorder.tracks[i].file);
  }
}

static int streamCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
  const unsigned char **inputBuffers = (const unsigned char **)inputBuffer;
  unsigned char **outputBuffers = (unsigned char **)outputBuffer;
  size_t minReadFrames = framesPerBuffer;                   // Initialize with the maximum possible
  unsigned char *writeBuffer = malloc(framesPerBuffer * 3); // Allocate buffer

  for (size_t channel = 0; channel < recorder.trackCount; ++channel)
  {
    if (isRecording)
    {
      if (recorder.tracks[channel].recordEnabled)
      {
        // set db amplitude levels
        float rms = calculateRMS(inputBuffers[channel], framesPerBuffer);
        float dbLevel = rmsToDb(rms);
        printf("Recording - Channel %d: dB Level = %f\n", channel, dbLevel);
        recorder.tracks[channel].currentAmplitudeLevel = dbLevel;
        // channel data and write buffer for wav
        const unsigned char *channelData = inputBuffers[channel];
        // Accumulate the samples for the current channel into writeBuffer
        for (unsigned long frame = 0; frame < framesPerBuffer; ++frame)
        {
          int byteIndex = frame * 3;
          memcpy(&writeBuffer[frame * 3], &channelData[byteIndex], 3);
        }
        // Now, writeBuffer contains all the frames for the current channel, so write it all at once
        writeWavData(&recorder.tracks[channel], writeBuffer, framesPerBuffer * 3);
      }
    }

    // Handle Playback for non record enabled tracks
    if (!recorder.tracks[channel].recordEnabled)
    {
      float dbLevel = -400; // Default to a very low dB level if no data is read
      size_t readFrames = 0;

      for (size_t frame = 0; frame < framesPerBuffer; ++frame)
      {
        if (recorder.playbackPosition + frame < recorder.tracks[channel].dataSize / 3)
        {
          uint8_t sampleBytes[3];
          if (fread(sampleBytes, sizeof(uint8_t), 3, recorder.tracks[channel].file) == 3)
          {
            memcpy(&outputBuffers[channel][frame * 3], sampleBytes, 3);
            readFrames++;
          }
          else
          {
            memset(&outputBuffers[channel][frame * 3], 0, 3); // Fail-safe clear in case of read failure
          }
        }
        else
        {
          memset(&outputBuffers[channel][frame * 3], 0, 3); // Zero out the buffer beyond data size
        }
      }

      if (readFrames > 0)
      {
        float rms = calculateRMS(outputBuffers[channel], readFrames);
        dbLevel = rmsToDb(rms);
      }
      printf("Playback - Channel %d: dB Level = %f\n", channel, dbLevel);
      recorder.tracks[channel].currentAmplitudeLevel = dbLevel;

      // Determine the minimum readFrames across all channels for playback tracking
      if (readFrames < minReadFrames)
      {
        minReadFrames = readFrames;
      }
    }
  }

  // free the write buffer
  free(writeBuffer);

  // Update the start time to reflect the current playback position
  float timeIncrement = (float)framesPerBuffer / sampleRate;
  startTimeInSeconds += timeIncrement;

  return paContinue;
}

void initStream()
{
  // stop if no input device
  int inputDevice = Pa_GetDefaultInputDevice();
  if (inputDevice == -1)
  {
    printf("WARNING: No default input device found!");
    exit(1);
  }

  PaError err;
  // Stream parameters
  PaStreamParameters inputParameters;
  inputParameters.channelCount = recorder.trackCount;
  inputParameters.device = inputDevice;                                                                // or another specific device
  inputParameters.sampleFormat = paInt24 | paNonInterleaved;                                           // Correct way to combine flags
  inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency; // lowest latency
  inputParameters.hostApiSpecificStreamInfo = NULL;

  // Setup output parameters
  PaStreamParameters outputParameters;
  outputParameters.device = Pa_GetDefaultOutputDevice();
  outputParameters.channelCount = recorder.trackCount;                                                   // Number of tracks
  outputParameters.sampleFormat = paInt24 | paNonInterleaved;                                            // Correct way to combine flags
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency; // lowest latency
  outputParameters.hostApiSpecificStreamInfo = NULL;

  printf("Initializing stream with %d channels.\n", recorder.trackCount); // Typically NULL

  // start PA audio stream
  err = Pa_OpenStream(
      &stream,
      &inputParameters,
      &outputParameters,
      sampleRate,
      frames,
      paClipOff, // or other relevant flags. Note: paNonInterleaved is NOT set here
      streamCallback,
      NULL // User data passed to callback
  );
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }

  // Calculate playback start position based on startTimeInSeconds
  // Assuming each sample in the buffer corresponds to a frame of audio
  size_t startPosition = (size_t)(sampleRate * startTimeInSeconds);
  recorder.playbackPosition = startPosition;
}

void initAudio()
{
  currentDefaultMacOSInputDevice = getDefaultMacOSInputDeviceID();
  currentDefaultMacOSOutputDevice = getDefaultMacOSOutputDeviceID();

  PaError err;
  // init PA
  err = Pa_Initialize();
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }

  size_t inputChannelCount = checkPAIOAndGetChannelCount();

  setInputTrackCount(inputChannelCount);
}

void cleanupAudio()
{
  PaError err;
  // safety close streams
  Pa_StopStream(stream);
  Pa_CloseStream(stream);

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

void onStop()
{
  // Stop recording
  Pa_StopStream(stream);
  Pa_CloseStream(stream);
  // clean up wav files and track  memory
  closeWavFiles(); // updates the WAV headers.

  printf("Recording stopped.\n");
}

void onStart(const uint32_t *inputTrackRecordEnabledStates, bool isRecordingFromUI)
{
  // handle setting is recording first
  isRecording = isRecordingFromUI;

  PaError err;
  // init or re-init all wav track files with header data
  initTracks(inputTrackRecordEnabledStates);
  // init stream
  initStream();
  // Start recording
  err = Pa_StartStream(stream);
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }

  printf("Recording started.\n");
}

void onRtz()
{
  updateStartTime(0.00);
}

// TESTING
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