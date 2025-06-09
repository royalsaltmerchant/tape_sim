#include "audio.h"

// UTILITY FUNCTIONS
void separatePathFromTitle(const char *selectedPath, char **dirPath, char **trackTitle)
{
  // Find the last occurrence of '/' which separates directory path and file name
  const char *lastSlash = strrchr(selectedPath, '/');
  if (lastSlash == NULL)
  {
    // Handle the case where there's no slash; assume current directory
    *dirPath = strdup(".");
    *trackTitle = strdup(selectedPath);
  }
  else
  {
    // Allocate memory and copy directory path
    *dirPath = (char *)malloc(lastSlash - selectedPath + 2); // +1 for null terminator, +1 to include '/'
    strncpy(*dirPath, selectedPath, lastSlash - selectedPath + 1);
    (*dirPath)[lastSlash - selectedPath + 1] = '\0'; // Ensure null-terminated

    // Allocate memory and copy the track title (filename)
    *trackTitle = strdup(lastSlash + 1);
  }
}

// For calculating db levels
float calculateRMS(const unsigned char *buffer, size_t framesPerBuffer)
{
  float sum = 0.0;
  for (size_t i = 0; i < framesPerBuffer; i++)
  {
    int sample24bit = (buffer[3 * i + 2] << 16) | (buffer[3 * i + 1] << 8) | buffer[3 * i];
    if (sample24bit & 0x800000)
    {                           // if the highest bit is set, the sample is negative
      sample24bit |= ~0xFFFFFF; // extend the sign to 32 bits
    }
    float sampleFloat = sample24bit / (float)0x800000; // Normalize by the maximum possible value of a 24-bit int
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

int calculateAudioDuration(size_t audioDataSize, int channels)
{
  size_t bytesPerSample = bitDepth / 8;
  int durationInSeconds = (int)audioDataSize / (bytesPerSample * sampleRate * channels);
  return durationInSeconds;
}

size_t calculateBufferSize(int channels, int durationInSeconds)
{
  size_t bytesPerSample = bitDepth / 8;
  size_t samplesPerChannel = (size_t)(sampleRate * durationInSeconds);
  size_t totalBufferSize = samplesPerChannel * bytesPerSample * channels;
  return totalBufferSize;
}

void mixMonoTrackToStereoBuffer(WavFile *monoTrack, unsigned char *stereoBuffer, size_t bufferSize, int channel)
{
  fseek(monoTrack->file, 0, SEEK_END);
  long fileSize = ftell(monoTrack->file);
  rewind(monoTrack->file);

  long dataLength = fileSize - 44;
  long numSamples = dataLength / 3;

  fseek(monoTrack->file, 44, SEEK_SET);
  unsigned char sample[3];

  for (long i = 0; i < numSamples; i++)
  {
    size_t offset = 6 * i + 3 * channel;
    if (offset + 3 > bufferSize)
    {
      break;
    }

    fread(sample, 1, 3, monoTrack->file);
    memcpy(stereoBuffer + offset, sample, 3);
  }
}

float getCurrentStartTimeInSeconds()
{
  return startTimeInSeconds;
}

void updateStartTime(float time)
{
  startTimeInSeconds = time;
}

float getCurrentAmplitude(unsigned int index)
{
  return recorder.tracks[index].currentAmplitudeLevel;
}

void onSetInputTrackRecordEnabled(unsigned int index, bool state)
{
  if (state == 1)
  {
    recorder.tracks[index].recordEnabled = true;
  }
  else
  {
    recorder.tracks[index].recordEnabled = false;
  }
}

void openWavFile(WavFile *wav, char *filename, char *directoryPath, short numChannels)
{
  char *filePath = malloc(strlen(directoryPath) + strlen(filename) + 2); // +2 for '/' and '\0'
  sprintf(filePath, "%s/%s", directoryPath, filename);

  bool fileExists = access(filePath, F_OK) != -1;

  int headerSize = 44;
  int subchunk1Size = 16; // PCM
  short audioFormat = 1;  // No compression
  int byteRate = sampleRate * numChannels * (bitDepth / 8);
  short blockAlign = numChannels * (bitDepth / 8);
  int zero = 0;

  wav->file = fopen(filePath, fileExists ? "r+b" : "wb");
  if (!wav->file)
  {
    perror("Failed to open file");
    free(filePath);
    return;
  }

  if (!fileExists)
  {
    wav->dataSize = 0;

    // Write proper RIFF header
    fwrite("RIFF", 1, 4, wav->file); // ChunkID
    fwrite(&zero, 4, 1, wav->file);  // ChunkSize (placeholder)
    fwrite("WAVE", 1, 4, wav->file); // Format

    fwrite("fmt ", 1, 4, wav->file);         // Subchunk1ID
    fwrite(&subchunk1Size, 4, 1, wav->file); // Subchunk1Size
    fwrite(&audioFormat, 2, 1, wav->file);   // AudioFormat
    fwrite(&numChannels, 2, 1, wav->file);   // NumChannels
    fwrite(&sampleRate, 4, 1, wav->file);    // SampleRate
    fwrite(&byteRate, 4, 1, wav->file);      // ByteRate
    fwrite(&blockAlign, 2, 1, wav->file);    // BlockAlign
    fwrite(&bitDepth, 2, 1, wav->file);      // BitsPerSample

    fwrite("data", 1, 4, wav->file); // Subchunk2ID
    fwrite(&zero, 4, 1, wav->file);  // Subchunk2Size (placeholder)
  }
  else
  {
    fseek(wav->file, 0, SEEK_END);
    size_t currentFileSize = ftell(wav->file);
    wav->dataSize = currentFileSize - headerSize;

    // Seek to overwrite position
    size_t bytesPerSample = (bitDepth / 8) * numChannels;
    size_t sampleOffset = (size_t)(sampleRate * startTimeInSeconds);
    size_t byteOffset = sampleOffset * bytesPerSample;
    size_t overwriteStartPos = headerSize + byteOffset;

    fseek(wav->file, overwriteStartPos, SEEK_SET);
  }

  free(filePath);
}

void initTracks(const uint32_t *inputTrackRecordEnabledStates)
{
  for (size_t i = 0; i < recorder.trackCount; i++)
  {
    char filename[20];
    snprintf(filename, sizeof(filename), "track%zu.wav", i + 1);
    openWavFile(&recorder.tracks[i], filename, appDirPath, 1);

    if (inputTrackRecordEnabledStates && inputTrackRecordEnabledStates[i] == 1)
    {
      recorder.tracks[i].recordEnabled = true;
    }
    else
    {
      recorder.tracks[i].recordEnabled = false;
    }

    recorder.tracks[i].currentAmplitudeLevel = -100; //  a minimum signal level for now
  }
}

void writeWavData(WavFile *wav, const void *data, size_t dataSize)
{
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

void closeWavFile(WavFile *wav)
{
  size_t finalDataSize = wav->dataSize;

  // Move to the start of the file size field
  fseek(wav->file, 4, SEEK_SET);
  int fileSizeMinus8 = finalDataSize + 36; // Size of 'WAVEfmt ' and 'data' headers plus dataSize
  fwrite(&fileSizeMinus8, sizeof(fileSizeMinus8), 1, wav->file);

  // Move to the start of the data size field
  fseek(wav->file, 40, SEEK_SET);
  fwrite(&finalDataSize, sizeof(finalDataSize), 1, wav->file);

  fclose(wav->file);
}

void closeWavFiles()
{
  for (size_t i = 0; i < recorder.trackCount; i++)
  {
    closeWavFile(&recorder.tracks[i]);
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
    else // Handle Playback for non record enabled tracks
    {
      float dbLevel = -100; // Default to a very low dB level if no data is read
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
    printf("Error: No default input device found!");
    exit(EXIT_FAILURE);
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
    exit(EXIT_FAILURE);
  }

  // Calculate playback start position based on startTimeInSeconds
  // Assuming each sample in the buffer corresponds to a frame of audio
  size_t startPosition = (size_t)(sampleRate * startTimeInSeconds);
  recorder.playbackPosition = startPosition;
}

void onRewind()
{
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

void onRtz()
{
  updateStartTime(0.00);
}

void onStop()
{
  Pa_StopStream(stream);
  Pa_CloseStream(stream);
  closeWavFiles();
}

void onStart(const uint32_t *inputTrackRecordEnabledStates, bool isRecordingFromUI)
{
  PaError err;

  isRecording = isRecordingFromUI;

  initTracks(inputTrackRecordEnabledStates);
  initStream();

  err = Pa_StartStream(stream);
  if (err != paNoError)
  {
    printf("PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }
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
    exit(EXIT_FAILURE);
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
    exit(EXIT_FAILURE);
  }

  return deviceID; // Return the default output device ID
}

void setupInputTracks(int inputChannelCount)
{
  if (recorder.tracks != NULL)
  {
    free(recorder.tracks);
    recorder.tracks = NULL;
  }
  recorder.trackCount = inputChannelCount;
  recorder.tracks = calloc(sizeof(WavFile) * recorder.trackCount, sizeof(WavFile)); // make room for as many wav files as needed tracks

  if (recorder.tracks == NULL)
  {
    printf("Error: Failed to allocate memory for tracks.\n");
    exit(EXIT_FAILURE);
  }
}

int checkPAIOAndGetChannelCount()
{
  int inputDevice = Pa_GetDefaultInputDevice();
  int outputDevice = Pa_GetDefaultOutputDevice();
  // check that we have both IO devices from default setup
  if (inputDevice == -1 || outputDevice == -1)
  {
    printf("Error: We are missing either an input or an output default device\n");
    exit(EXIT_FAILURE);
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
    exit(EXIT_FAILURE);
  }

  // for now, we are enforcing that devices must comply with 1:1 inputs to outputs
  if (outputChannelCount < inputChannelCount)
  {
    printf("Error: There are not enough output channels (%d) to match input channels\n", outputChannelCount);
    exit(EXIT_FAILURE);
  }

  return inputChannelCount;
}

void initAudio()
{

  PaError err;
  // init PA
  err = Pa_Initialize();
  if (err != paNoError)
  {
    printf("PortAudio error during initialization: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  // establish the current input setup
  size_t inputChannelCount = checkPAIOAndGetChannelCount();
  setupInputTracks(inputChannelCount);
}

void cleanupAudio()
{
  PaError err;

  Pa_StopStream(stream);
  Pa_CloseStream(stream);

  err = Pa_Terminate();
  if (err != paNoError)
  {
    printf("PortAudio error during termination: %s\n", Pa_GetErrorText(err));
  }
}

// also monitors the default devices on macos to adjust audio setup if a change has occured
int getInputTrackCount()
{
  // if macos device defaults change
  AudioDeviceID defaultMacOSInputDevice = getDefaultMacOSInputDeviceID();
  AudioDeviceID defaultMacOSOutputDevice = getDefaultMacOSOutputDeviceID();
  if (defaultMacOSInputDevice != currentDefaultMacOSInputDevice || defaultMacOSOutputDevice != currentDefaultMacOSOutputDevice)
  {
    // reset our PA audio setup
    cleanupAudio();
    initAudio();
  }
  // after we re-init and set the recorder.trackCount we should now be returning the latest state update
  return recorder.trackCount;
}

int bounceTracks(const uint32_t *tracksToBounce, char *selectedPath)
{
  // reset time in seconds
  startTimeInSeconds = 0;

  initTracks(NULL); // ###############################

  // Parse the selected path into directory and track title
  char *dirPath, *trackTitle;
  separatePathFromTitle(selectedPath, &dirPath, &trackTitle);

  // Create the new stereo file
  WavFile *bouncedTrack = malloc(sizeof(WavFile));
  openWavFile(bouncedTrack, trackTitle, dirPath, 2);

  // Find the two tracks marked for bouncing
  int selectedIndices[2] = {-1, -1};
  int found = 0;
  for (size_t i = 0; i < recorder.trackCount; i++)
  {
    if (tracksToBounce[i] == 1)
    {
      selectedIndices[found++] = i;
    }
  }

  if (found != 2)
  {
    printf("Error: Less than two tracks marked for bouncing.\n");
    free(bouncedTrack);
    return 1;
  }

  // Determine the duration of the longest track among the two
  int duration = 0;
  for (int i = 0; i < 2; i++)
  {
    fseek(recorder.tracks[selectedIndices[i]].file, 0, SEEK_END);
    size_t fileSize = ftell(recorder.tracks[selectedIndices[i]].file);
    size_t audioDataSize = fileSize - 44; // Subtract the header size
    int trackDuration = calculateAudioDuration(audioDataSize, 1);
    if (duration < trackDuration)
    {
      duration = trackDuration;
    }
  }

  if (duration == 0)
  {
    printf("ERROR: One or both tracks are empty.\n");
    free(bouncedTrack);
    return 1;
  }

  // Allocate and initialize a stereo buffer based on the longest track duration
  size_t stereoBufferSize = calculateBufferSize(2, duration);
  unsigned char *stereoBuffer = malloc(stereoBufferSize);
  if (!stereoBuffer)
  {
    printf("Memory allocation failed for stereo buffer.\n");
    free(bouncedTrack);
    return 1;
  }
  memset(stereoBuffer, 0, stereoBufferSize);

  // Mix each track into the appropriate channel of the stereo buffer
  for (int channel = 0; channel < 2; channel++)
  {
    mixMonoTrackToStereoBuffer(&recorder.tracks[selectedIndices[channel]], stereoBuffer, stereoBufferSize, channel);
  }

  // Write and close the stereo WAV file
  writeWavData(bouncedTrack, stereoBuffer, stereoBufferSize);
  closeWavFile(bouncedTrack);

  // Cleanup
  free(stereoBuffer);
  free(bouncedTrack);

  return 0;
}

void onSetAppDirPath(const char *selectedPath)
{
  if (appDirPath != NULL)
  {
    free(appDirPath);
  }

  appDirPath = malloc(strlen(selectedPath) + 1);
  if (appDirPath == NULL)
  {
    fprintf(stderr, "Failed to allocate memory for appDirPath\n");
    return;
  }
  strcpy(appDirPath, selectedPath);
  printf("App directory path set to: %s\n", appDirPath);
  // reset time in seconds
  startTimeInSeconds = 0;
  // re-init tracks
  initTracks(NULL);
}