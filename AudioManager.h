/**
 * @file AudioManager.h
 * @brief Real-time audio capture and shared buffer management for voice recognition
 * @author Kethy
 */
#pragma once
#include <portaudio.h>
#include <atomic>
#include <vector>
#include <mutex>

/**
 * @struct SharedAudioBuffer
 * @brief Shared buffer for microphone audio samples.
 *
 * This structure stores audio samples captured from PortAudio
 * and provides a mutex to protect access when the buffer is
 * shared between threads.
 *
 * The audio input thread writes samples into this buffer,
 * while the speech recognition component reads from it.
 */
struct SharedAudioBuffer {
    std::vector<float> samples; /**< Captured audio samples. */
    std::mutex         mtx; /**< Mutex protecting access to the sample buffer. */
};

/**
 * @brief Global volume level used by the assistant.
 */
extern std::atomic<float> g_volume;


/**
 * @brief Global flag indicating whether the microphone is muted.
 */
extern std::atomic<bool>  g_micMuted;

/**
 * @brief Global shared audio buffer for microphone input.
 */
extern SharedAudioBuffer  g_audioBuf;


/**
 * @class AudioManager
 * @brief Manages microphone audio input using PortAudio.
 *
 * This class wraps the lifecycle of a PortAudio input stream.
 * It is responsible for initializing the selected microphone,
 * starting the input stream, and stopping and cleaning up
 * audio resources when finished.
 */

class AudioManager {
public:

    /**
     * @brief Constructs an AudioManager object.
     */
    AudioManager() = default;


    /**
     * @brief Destroys the AudioManager object.
     *
     * Ensures that the PortAudio stream is stopped and cleaned up.
     */
    ~AudioManager();

    /**
     * @brief Initializes and starts the PortAudio input stream.
     *
     * Opens the microphone stream for the given input device index
     * and begins capturing audio samples.
     *
     * @param micIndex The index of the microphone input device to use.
     * @return true if initialization succeeded, false if no valid input
     *         device was available or the stream could not be opened.
     */
    bool init(int micIndex);

    /**
     * @brief Stops the active PortAudio stream.
     *
     * Closes the microphone input stream if it is currently running.
     */
    void stop();

private:
    PaStream* m_stream = nullptr; /**< Pointer to the active PortAudio stream. */
};