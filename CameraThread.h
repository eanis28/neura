/**
 * @file CameraThread.h
 * @brief Background camera capture thread for real-time video processing
 * @author Kethy
 */
#pragma once
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <mutex>

/**
 * @class CameraThread
 * @brief Manages background thread for camera frame capture and processing
 * 
 * This class runs a separate thread that continuously captures frames from
 * a camera device, performs transformations (flip, resize, color conversion),
 * and makes the latest processed frame available for rendering. The processing
 * includes fitting frames to a target panel size while maintaining aspect ratio.
 */
class CameraThread {
public:
    /**
     * @brief Default constructor
     */
    CameraThread() = default;

    /**
     * @brief Destructor - automatically stops the capture thread
     */
    ~CameraThread();

    /**
     * @brief Starts the camera capture thread
     * 
     * Initializes and starts a background thread that continuously captures
     * and processes camera frames to fit the specified panel dimensions.
     * 
     * @param panelW Target panel width in pixels
     * @param panelH Target panel height in pixels
     */
    void start(unsigned panelW, unsigned panelH);

    /**
     * @brief Stops the capture thread and waits for completion
     * 
     * Signals the background thread to stop and blocks until it has
     * fully terminated. Safe to call multiple times.
     */
    void stop();

    /**
     * @brief Retrieves the most recent processed frame
     * 
     * Thread-safe method to copy the latest RGBA frame captured by the
     * background thread. The frame is fitted to the panel size specified
     * in start() with white borders if aspect ratios don't match.
     * 
     * @param dst Destination matrix to receive the frame copy
     * @return true if a frame was successfully copied, false if no frame available yet
     */
    bool copyLatest(cv::Mat& dst);

private:
    /**
     * @brief Main capture loop running in background thread
     * 
     * Continuously captures frames, applies transformations (flip, resize,
     * color conversion to RGBA), and stores the result with thread-safe access.
     * 
     * @param panelW Target panel width in pixels
     * @param panelH Target panel height in pixels
     */
    void loop(unsigned panelW, unsigned panelH);

    cv::Mat           m_latest;      ///< Most recent processed frame (RGBA)
    std::mutex        m_mtx;         ///< Protects access to m_latest
    std::thread       m_thread;      ///< Background capture thread
    std::atomic<bool> m_running{false}; ///< Thread running state
};