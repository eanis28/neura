#include "CameraThread.h"
#include "Settings.h"
#include <chrono>

/**
 * @file CameraThread.cpp
 * @brief Implementation of background camera capture thread
 * @author Kethy
 */

/**
 * @brief Opens camera device with platform-specific backend
 * 
 * Attempts to open the camera using the index from g_settings.cameraIndex.
 * If that fails or is -1, falls back to probing indices 1 and 0.
 * Uses platform-specific backends: AVFoundation (macOS), MSMF (Windows),
 * V4L2 (Linux).
 * 
 * @return Opened VideoCapture object, or empty capture if all attempts fail
 */
static cv::VideoCapture openCamera() {
    int idx = g_settings.cameraIndex;

#ifdef __APPLE__
    if (idx != -1) return cv::VideoCapture(idx, cv::CAP_AVFOUNDATION);
    // Fallback: try indices 1 then 0 to find any available camera
    for (int i = 1; i >= 0; --i) {
        cv::VideoCapture cap(i, cv::CAP_AVFOUNDATION);
        if (cap.isOpened()) return cap;
    }
#else
    if (idx != -1) {
        cv::VideoCapture cap("/dev/video" + std::to_string(idx), cv::CAP_V4L2);
        if (cap.isOpened()) return cap;
    }
    for (int i = 1; i >= 0; --i) {
        cv::VideoCapture cap("/dev/video" + std::to_string(i), cv::CAP_V4L2);
        if (cap.isOpened()) return cap;
    }
#endif
    return cv::VideoCapture();
}

CameraThread::~CameraThread() {
    // Ensure thread is stopped before object destruction
    stop();
}

void CameraThread::start(unsigned panelW, unsigned panelH) {
    m_running = true;
    // Spawn background thread that runs the capture loop
    m_thread  = std::thread(&CameraThread::loop, this, panelW, panelH);
}

void CameraThread::stop() {
    // Signal the loop to exit
    m_running = false;
    // Wait for thread to finish its current iteration and terminate
    if (m_thread.joinable()) m_thread.join();
}

bool CameraThread::copyLatest(cv::Mat& dst) {
    // Lock mutex to prevent reading while loop() is updating m_latest
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_latest.empty()) return false;
    dst = m_latest.clone();
    return true;
}

void CameraThread::loop(unsigned panelW, unsigned panelH) {
    cv::VideoCapture cap = openCamera();
    if (!cap.isOpened()) return;
    cap.set(cv::CAP_PROP_FPS, 15);

    cv::Mat frame, flipped, resized, rgba;

    while (m_running.load()) {
        // Attempt to read a frame from the camera
        if (!cap.read(frame) || frame.empty()) {
            // Camera disconnected or frame read failed - wait and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Mirror the frame horizontally for natural selfie-view
        cv::flip(frame, flipped, 1);

        // Calculate scaling to fit panel while maintaining aspect ratio
        float scaleW = (float)panelW / flipped.cols;
        float scaleH = (float)panelH / flipped.rows;
        float scale  = std::min(scaleW, scaleH);
        int fitW = (int)(flipped.cols * scale);
        int fitH = (int)(flipped.rows * scale);
        
        // Center the frame within the panel
        int xOff = ((int)panelW - fitW) / 2;
        int yOff = ((int)panelH - fitH) / 2;

        cv::resize(flipped, resized, cv::Size(fitW, fitH));
        // Convert to RGBA for SFML rendering
        cv::cvtColor(resized, rgba, cv::COLOR_BGR2RGBA);

        // Create white background panel
        cv::Mat panel((int)panelH, (int)panelW,
                    CV_8UC4, cv::Scalar(255, 255, 255, 255));
        // Copy resized frame into center of panel
        rgba.copyTo(panel(cv::Rect(xOff, yOff, fitW, fitH)));

        // Lock mutex to safely update m_latest without race conditions
        std::lock_guard<std::mutex> lock(m_mtx);
        m_latest = panel.clone();
    }
}