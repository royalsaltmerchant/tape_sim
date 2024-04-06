// gcc -o audio audio.c -lportaudio
#include "audio.h"

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
    printf("Device %zu: %s\n", i, deviceInfo->name);
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
  WavFile *wav = malloc(sizeof(WavFile));
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

static int playbackCallback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo *timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData)
{
  Player *player = (Player *)userData;
  float *out = (float *)outputBuffer;

  for (unsigned long i = 0; i < framesPerBuffer; i++)
  {
    if (player->playbackPosition < player->bufferLength)
    {
      // Interleave left and right channel data for true stereo playback
      *out++ = player->leftChannelBuffer[player->playbackPosition];  // Left channel
      *out++ = player->rightChannelBuffer[player->playbackPosition]; // Right channel
      player->playbackPosition++;
    }
    else
    {
      *out++ = 0; // Fill remainder with silence if buffer end reached
      *out++ = 0;
    }
  }
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
  inputParameters.hostApiSpecificStreamInfo = NULL;                                                    // Typically NULL

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

void readTrackDataToBuffer(WavFile *track, float *buffer, size_t totalSamples)
{
  fseek(track->file, 44, SEEK_SET);

  uint8_t sampleBytes[3];
  int32_t sample24bit;
  for (size_t i = 0; i < totalSamples; i++)
  {
    // Read 3 bytes per sample
    if (fread(sampleBytes, sizeof(uint8_t), 3, track->file) == 3)
    {
      // Convert bytes to a 24-bit signed int
      // Note: Assuming little-endian byte order
      sample24bit = (int32_t)(sampleBytes[0] << 8 | sampleBytes[1] << 16 | sampleBytes[2] << 24) >> 8;

      // Convert to floating point in the range of -1.0 to 1.0
      buffer[i] = sample24bit / (float)(1 << 23);
    }
    else
    {
      // End of file or read error; fill the rest with zeros
      for (; i < totalSamples; i++)
      {
        buffer[i] = 0.0f;
      }
      break;
    }
  }
}

void mixTracksIntoStereoBuffer()
{
  // Calculate the total length of the buffers required
  // For simplicity, assume all tracks are the same length and format
  size_t totalSamples = player.tracks[0].dataSize / (bitDepth / 8);

  // Allocate memory for the stereo buffers
  player.leftChannelBuffer = malloc(totalSamples * sizeof(float));
  player.rightChannelBuffer = malloc(totalSamples * sizeof(float));
  player.bufferLength = totalSamples;
  // Initialize the stereo buffers to zero
  memset(player.leftChannelBuffer, 0, totalSamples * sizeof(float));
  memset(player.rightChannelBuffer, 0, totalSamples * sizeof(float));

  // Temporary buffer for reading track data
  float *tempBuffer = malloc(totalSamples * sizeof(float));

  // Mix each track into the stereo buffers
  for (size_t i = 0; i < player.trackCount; i++)
  {
    // that reads track data into a buffer
    readTrackDataToBuffer(&player.tracks[i], tempBuffer, totalSamples);

    // Mix this track into the stereo buffers
    for (size_t j = 0; j < totalSamples; j++)
    {
      // For simplicity, mix equally into both channels
      player.leftChannelBuffer[j] += tempBuffer[j] / player.trackCount;
      player.rightChannelBuffer[j] += tempBuffer[j] / player.trackCount;
    }
  }

  // Cleanup
  free(tempBuffer);
}

void initPlayerStream()
{
  initPlayerTracks();
  // Mix the tracks into a single stereo buffer
  mixTracksIntoStereoBuffer();

  player.playbackPosition = 0;

  PaError err = Pa_OpenDefaultStream(&playingStream, 0, 2, paFloat32, sampleRate,
                                     frames, playbackCallback, &player);
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
  if (inputDevice == -1)
  {
    inputDevice = 0;
  }
  const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(inputDevice);

  int inputChannelCount = deviceInfo->maxInputChannels;
  // handle input count error
  if (inputChannelCount <= 0 || inputChannelCount > 64)
  {
    fprintf(stderr, "Error: Invalid number of input channels (%d). Must be between 1 and 64.\n", inputChannelCount);
    exit(1);
  }
  printf("max input channels: %d\n", inputChannelCount);

  // init tracks and track count for recording and playback
  recorder.trackCount = inputChannelCount;
  recorder.tracks = malloc(sizeof(WavFile) * recorder.trackCount); // make room for as many wav files as needed tracks
  player.trackCount = inputChannelCount;
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

void onStopRecording()
{
  // Stop recording
  Pa_StopStream(recordingStream);
  Pa_CloseStream(recordingStream);
  // clean up wav files and track  memory
  closeWavFiles(); // updates the WAV headers.

  // set start time to end of current recording
  float totalAudioDataBytes = recorder.tracks[0].dataSize; // assuming that track 0 has data...
  float bytesPerSecond = sampleRate * bitDepth / 8;
  float newStartTime = totalAudioDataBytes / bytesPerSecond;
  updateStartTime(newStartTime);
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

// int main() {
//   initAudio();
//   onStartRecording();
//   sleep(3);
//   onStopRecording();
//   onStartPlaying();
//   sleep(5);
//   cleanupAudio();
// }