#include "AudioManager.h"
#include <cmath>
#include <iostream>


/**
 * @def PA_SAMPLE_RATE
 * @brief Sample rate used for microphone input.
 *
 * This constant defines the number of audio samples captured
 * per second from the microphone.
 */

#define PA_SAMPLE_RATE    44100


/**
 * @def FRAMES_PER_BUFFER
 * @brief Number of audio frames processed in each callback.
 *
 * This value determines how many audio frames PortAudio passes
 * to the callback function at one time.
 */
#define FRAMES_PER_BUFFER 256

// ── Globals ───────────────────────────────────────────────────────────────────

/**
 * @brief Global microphone input volume level.
 *
 * Stores the most recently computed audio level from the input stream.
 */
std::atomic<float> g_volume(0.0f);

/**
 * @brief Global microphone mute flag.
 *
 * When true, incoming microphone audio is ignored and the shared
 * audio buffer is cleared.
 */
std::atomic<bool>  g_micMuted(false);

/**
 * @brief Global shared audio buffer containing microphone samples.
 *
 * This buffer is filled by the PortAudio callback and consumed
 * by other components such as speech recognition.
 */
SharedAudioBuffer  g_audioBuf;

// ── PortAudio callback ────────────────────────────────────────────────────────

/**
 * @brief PortAudio input callback function.
 *
 * This function is called repeatedly by PortAudio whenever a new block
 * of microphone input samples is available. It updates the global
 * volume level, handles microphone muting, and appends audio data
 * into the shared audio buffer for later processing.
 *
 * @param inputBuffer Pointer to the incoming audio sample buffer.
 * @param Unused output buffer parameter.
 * @param framesPerBuffer Number of frames in the input buffer.
 * @param Unused timing information provided by PortAudio.
 * @param Unused callback status flags.
 * @param Unused user data pointer.
 * @return `paContinue` to continue streaming audio.
 */

static int audioCallback(
    const void* inputBuffer,
    void*,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void*)
{
    const float* in = static_cast<const float*>(inputBuffer);
    if (!in) return paContinue;

    if (g_micMuted.load()) {
        g_volume.store(0.0f);

        std::lock_guard<std::mutex> lock(g_audioBuf.mtx);
        g_audioBuf.samples.clear();
        return paContinue;
    }

    float sum = 0.0f;
    for (unsigned int i = 0; i < framesPerBuffer; i++)
        sum += in[i] * in[i];
    g_volume.store(std::sqrt(sum / framesPerBuffer) * 10.0f);

    {
        std::lock_guard<std::mutex> lock(g_audioBuf.mtx);
        constexpr size_t MAX_SAMPLES = PA_SAMPLE_RATE * 10;
        if (g_audioBuf.samples.size() < MAX_SAMPLES)
            g_audioBuf.samples.insert(
                g_audioBuf.samples.end(), in, in + framesPerBuffer);
    }

    return paContinue;
}

// ── AudioManager ──────────────────────────────────────────────────────────────

/**
 * @brief Initializes and starts the PortAudio microphone input stream.
 *
 * This function initializes PortAudio, selects either the requested
 * microphone device or the default input device, configures the stream
 * parameters, opens the input stream, and starts audio capture.
 *
 * @param micIndex The index of the microphone device to use.
 *                 If `-1`, the default input device is used.
 * @return true if the input stream was started successfully, false otherwise.
 */

bool AudioManager::init(int micIndex) {
    Pa_Initialize();

    int devIdx = (micIndex == -1)
                 ? Pa_GetDefaultInputDevice()
                 : micIndex;
    if (devIdx == paNoDevice) devIdx = Pa_GetDefaultInputDevice();
    if (devIdx == paNoDevice) {
        std::cerr << "[AudioManager] No input device available.\n";
        Pa_Terminate();
        return false;
    }

    PaStreamParameters inParams;
    inParams.device                    = devIdx;
    inParams.channelCount              = 1;
    inParams.sampleFormat              = paFloat32;
    inParams.suggestedLatency          = Pa_GetDeviceInfo(devIdx)->defaultLowInputLatency;
    inParams.hostApiSpecificStreamInfo = nullptr;

    Pa_OpenStream(&m_stream, &inParams, nullptr,
                  PA_SAMPLE_RATE, FRAMES_PER_BUFFER,
                  paNoFlag, audioCallback, nullptr);
    Pa_StartStream(m_stream);
    return true;
}


/**
 * @brief Stops and closes the active PortAudio stream.
 *
 * If an audio stream is currently open, this function stops it,
 * closes it, resets the stream pointer, and terminates PortAudio.
 */

void AudioManager::stop() {
    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }
    Pa_Terminate();
}

/**
 * @brief Destroys the AudioManager object.
 *
 * Ensures that the PortAudio stream is properly stopped and cleaned up.
 */

AudioManager::~AudioManager() {
    stop();
}