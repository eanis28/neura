# detect.py
#
# Runs hand gesture detection for Neura.
# Opens the camera, checks for a hand each frame using MediaPipe,
# figures out what gesture is being made, and prints one line per frame:
#
#   POINTING 0.4821 0.3910
#   (gesture name, then x and y of the index fingertip)
#
# VisionInput.cpp on the C++ side reads those lines through a pipe.
#
# Gestures we recognize:
#   POINTING    - index finger up, others curled (moves the cursor)
#   TWO_FINGERS - index + middle up (click and drag)
#   OPEN_PALM   - all fingers up (cancel / let go)
#   THUMBS_UP   - fist with thumb pointing up (yes)
#   THUMBS_DOWN - fist with thumb pointing down (no)

import cv2
import mediapipe as mp
from collections import deque
import numpy as np

mp_hands = mp.solutions.hands

# Short history buffers — we average the last few values to smooth out
# the jitter that MediaPipe produces frame to frame
thumb_dist_history = deque(maxlen=5)
mouse_x_history    = deque(maxlen=5)
mouse_y_history    = deque(maxlen=5)

# Gesture debouncing — each gesture needs to be seen for a few frames in a
# row before we confirm it, and stays confirmed for a few frames after it
# stops so the output doesn't flicker while the hand transitions
two_finger_counter         = 0
two_finger_confirmed       = False
two_finger_release_counter = 0
TWO_FINGER_THRESHOLD         = 3   # frames in a row needed to confirm
TWO_FINGER_RELEASE_THRESHOLD = 4   # frames to hold it after it stops

pointing_counter         = 0
pointing_confirmed       = False
pointing_release_counter = 0
POINTING_THRESHOLD         = 3
POINTING_RELEASE_THRESHOLD = 5

open_palm_counter   = 0
open_palm_confirmed = False
OPEN_PALM_THRESHOLD = 3

last_gesture = "NONE"


# ── Finger state helpers ──────────────────────────────────────────────────────

def is_thumb_curled(landmarks):
    """Checks if the thumb is folded in toward the palm.

    Measures the distance between the thumb tip and the base knuckle of
    the index finger. Averages the last 5 frames to avoid flickering.

    Args:
        landmarks: The 21 hand landmark points from MediaPipe.

    Returns:
        True if the thumb looks tucked in, False if it's sticking out.
    """
    thumb_tip = landmarks[4]
    index_mcp = landmarks[5]  # base knuckle of the index finger
    dist = ((thumb_tip.x - index_mcp.x)**2 + (thumb_tip.y - index_mcp.y)**2) ** 0.5
    thumb_dist_history.append(dist)
    avg_dist = np.mean(thumb_dist_history)
    return avg_dist < 0.08


def is_finger_curled(landmarks, tip_id, mcp_id):
    """Checks if a finger is bent/curled toward the palm.

    Looks at how close the fingertip is to its own base knuckle.
    tip_id and mcp_id are MediaPipe landmark numbers (e.g. 8 and 5 for index finger).

    Args:
        landmarks: The 21 hand landmark points from MediaPipe.
        tip_id:    Landmark number of the fingertip.
        mcp_id:    Landmark number of the base knuckle (MCP joint).

    Returns:
        True if the finger looks curled, False if it looks extended.
    """
    tip  = landmarks[tip_id]
    mcp  = landmarks[mcp_id]
    dist = ((tip.x - mcp.x)**2 + (tip.y - mcp.y)**2) ** 0.5
    return dist < 0.18


def is_finger_extended_from_wrist(landmarks, tip_id, mcp_id):
    """Checks if a finger is clearly extended (pointing outward).

    Instead of just measuring tip-to-knuckle distance (which breaks when
    the hand is far from the camera and everything looks tiny), this compares
    how far the tip and knuckle each are from the wrist. If the tip is more
    than 1.6x further from the wrist than the knuckle, the finger is extended.
    This gives consistent results no matter how close the hand is to the camera.

    Args:
        landmarks: The 21 hand landmark points from MediaPipe.
        tip_id:    Landmark number of the fingertip.
        mcp_id:    Landmark number of the base knuckle (MCP joint).

    Returns:
        True if the finger is clearly extended, False otherwise.
    """
    tip   = landmarks[tip_id]
    mcp   = landmarks[mcp_id]
    wrist = landmarks[0]

    tip_d = ((tip.x - wrist.x)**2 + (tip.y - wrist.y)**2) ** 0.5
    mcp_d = ((mcp.x - wrist.x)**2 + (mcp.y - wrist.y)**2) ** 0.5

    # mcp_d > 0.005 avoids a divide-by-zero in weird edge cases
    return mcp_d > 0.005 and tip_d > mcp_d * 1.6


# ── Gesture checks ────────────────────────────────────────────────────────────

def is_open_palm(landmarks):
    """Checks if all four fingers are extended (open hand).

    Args:
        landmarks: The 21 hand landmark points from MediaPipe.

    Returns:
        True if all four fingers look extended.
    """
    return (
        is_finger_extended_from_wrist(landmarks, 8,  5) and   # index
        is_finger_extended_from_wrist(landmarks, 12, 9) and   # middle
        is_finger_extended_from_wrist(landmarks, 16, 13) and  # ring
        is_finger_extended_from_wrist(landmarks, 20, 17)      # pinky
    )


def is_pointing(landmarks):
    """Checks if only the index finger is up, others curled.

    Uses the wrist-ratio check for the index finger so it works at any
    distance. The other fingers just need to pass the simple curl check.

    Args:
        landmarks: The 21 hand landmark points from MediaPipe.

    Returns:
        True if it looks like a pointing gesture.
    """
    return (
        is_finger_extended_from_wrist(landmarks, 8,  5) and  # index up
        is_finger_curled(landmarks, 12, 9) and                # middle down
        is_finger_curled(landmarks, 16, 13) and               # ring down
        is_finger_curled(landmarks, 20, 17)                   # pinky down
    )


def is_two_fingers(landmarks):
    """Checks if index and middle fingers are both up, ring and pinky curled.

    Both extended fingers use the wrist-ratio check. This can't accidentally
    overlap with is_pointing because a middle finger that passes the wrist-ratio
    extension check can't also pass the curl check at the same time.

    Args:
        landmarks: The 21 hand landmark points from MediaPipe.

    Returns:
        True if it looks like a two-finger gesture.
    """
    return (
        is_finger_extended_from_wrist(landmarks, 8,  5) and  # index up
        is_finger_extended_from_wrist(landmarks, 12, 9) and  # middle up
        is_finger_curled(landmarks, 16, 13) and               # ring down
        is_finger_curled(landmarks, 20, 17)                   # pinky down
    )


# ── Position smoother ─────────────────────────────────────────────────────────

def smooth_position(x, y, reset=False):
    """Averages the last few fingertip positions to keep the cursor steady.

    When the gesture changes we wipe the history (reset=True) so positions
    from the old gesture don't bleed into the new one.

    Args:
        x:     Raw x position of the fingertip from MediaPipe (0.0 to 1.0).
        y:     Raw y position of the fingertip from MediaPipe (0.0 to 1.0).
        reset: If True, clears the history before adding the new position.

    Returns:
        A (x, y) tuple with the smoothed position.
    """
    if reset:
        mouse_x_history.clear()
        mouse_y_history.clear()
    mouse_x_history.append(x)
    mouse_y_history.append(y)
    return float(np.mean(mouse_x_history)), float(np.mean(mouse_y_history))


# ── Main gesture detector ─────────────────────────────────────────────────────

def detect_gesture(landmarks):
    """Looks at the hand and decides what gesture is being made.

    Checks gestures in priority order (open palm first, thumbs last).
    Each gesture needs to be held for a few frames to confirm, and stays
    active for a few frames after it stops to avoid flickering.

    Args:
        landmarks: The 21 hand landmark points from MediaPipe.

    Returns:
        A string — one of: OPEN_PALM, TWO_FINGERS, POINTING,
        THUMBS_UP, THUMBS_DOWN, or NONE.
    """
    global two_finger_counter, two_finger_confirmed, two_finger_release_counter
    global pointing_counter, pointing_confirmed, pointing_release_counter
    global open_palm_counter, open_palm_confirmed

    thumb_tip = landmarks[4]
    wrist     = landmarks[0]

    # Open palm overrides everything — if the hand is open, cancel whatever was happening
    if is_open_palm(landmarks):
        open_palm_counter += 1
        if open_palm_counter >= OPEN_PALM_THRESHOLD:
            open_palm_confirmed = True
        if open_palm_confirmed:
            two_finger_counter   = 0
            two_finger_confirmed = False
            pointing_counter     = 0
            pointing_confirmed   = False
            return "OPEN_PALM"
        return "NONE"
    else:
        open_palm_counter   = 0
        open_palm_confirmed = False

    # Two fingers beats pointing since it's a superset (index + middle vs just index)
    if is_two_fingers(landmarks):
        pointing_counter         = 0
        pointing_confirmed       = False
        pointing_release_counter = 0
        two_finger_release_counter = 0
        two_finger_counter        += 1
        if two_finger_counter >= TWO_FINGER_THRESHOLD:
            two_finger_confirmed = True
        if two_finger_confirmed:
            return "TWO_FINGERS"
        return "NONE"
    else:
        two_finger_counter = 0
        if two_finger_confirmed:
            # Hold it for a bit so it doesn't cut off abruptly when fingers lower
            two_finger_release_counter += 1
            if two_finger_release_counter >= TWO_FINGER_RELEASE_THRESHOLD:
                two_finger_confirmed       = False
                two_finger_release_counter = 0
            else:
                return "TWO_FINGERS"

    # Pointing — just the index finger
    if is_pointing(landmarks):
        pointing_release_counter = 0
        pointing_counter        += 1
        if pointing_counter >= POINTING_THRESHOLD:
            pointing_confirmed = True
        if pointing_confirmed:
            return "POINTING"
        return "NONE"
    else:
        pointing_counter = 0
        if pointing_confirmed:
            pointing_release_counter += 1
            if pointing_release_counter >= POINTING_RELEASE_THRESHOLD:
                pointing_confirmed       = False
                pointing_release_counter = 0
            else:
                return "POINTING"

    # Thumbs up/down — hand needs to be a fist first (all fingers curled in)
    fingers_curled = (
        is_finger_curled(landmarks, 12, 9) and
        is_finger_curled(landmarks, 16, 13) and
        is_finger_curled(landmarks, 20, 17)
    )
    if not fingers_curled:
        return "NONE"

    # Extra check on the index finger — a pointing finger angled toward the camera
    # can fool the simple curl check, so we also verify the wrist ratio is below 1.4
    idx_tip   = landmarks[8]
    idx_mcp   = landmarks[5]
    idx_tip_d = ((idx_tip.x - wrist.x)**2 + (idx_tip.y - wrist.y)**2) ** 0.5
    idx_mcp_d = ((idx_mcp.x - wrist.x)**2 + (idx_mcp.y - wrist.y)**2) ** 0.5
    idx_truly_curled = (
        is_finger_curled(landmarks, 8, 5) and
        (idx_mcp_d < 0.005 or idx_tip_d <= idx_mcp_d * 1.4)
    )
    if not idx_truly_curled:
        return "NONE"

    # We call is_thumb_curled every frame so it can keep its rolling history updated
    thumb_tucked = is_thumb_curled(landmarks)
    if thumb_tucked:
        return "NONE"  # thumb is folded in = not a thumbs gesture

    # Thumb above the wrist = thumbs up, below = thumbs down
    if thumb_tip.y < wrist.y - 0.08:
        return "THUMBS_UP"
    elif thumb_tip.y > wrist.y + 0.08:
        return "THUMBS_DOWN"

    return "NONE"


# ── Camera setup ──────────────────────────────────────────────────────────────

def _load_camera_index():
    """Reads the user's preferred camera from settings.ini.

    Returns:
        The saved camera index number, or -1 if nothing was saved.
    """
    try:
        with open("settings.ini") as f:
            for line in f:
                if line.startswith("camera="):
                    return int(line.strip().split("=", 1)[1])
    except Exception:
        pass
    return -1


# Try the camera the user picked in settings first.
# If that doesn't work (or nothing was saved), scan 0-3 until one opens.
_pref_idx = _load_camera_index()
cap = None

if _pref_idx != -1:
    candidate = cv2.VideoCapture(_pref_idx)
    if candidate.isOpened():
        cap = candidate

if cap is None:
    for idx in range(4):
        candidate = cv2.VideoCapture(idx)
        if candidate.isOpened():
            cap = candidate
            break

if cap is None:
    raise RuntimeError("No camera found")

cap.set(cv2.CAP_PROP_FPS, 15)  # 15fps is plenty for gesture detection


# ── Main loop ─────────────────────────────────────────────────────────────────

with mp_hands.Hands(max_num_hands=1) as hands:
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        # MediaPipe needs RGB but OpenCV gives BGR by default
        rgb     = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(rgb)

        if results.multi_hand_landmarks:
            landmarks = results.multi_hand_landmarks[0].landmark

            gesture   = detect_gesture(landmarks)
            index_tip = landmarks[8]  # landmark 8 = index fingertip

            # Clear position history when the gesture changes so the cursor
            # doesn't snap from wherever the hand was in the previous gesture
            reset = (gesture != last_gesture)
            last_gesture = gesture

            sx, sy = smooth_position(index_tip.x, index_tip.y, reset=reset)

            # One line per frame — VisionInput.cpp reads this
            print(f"{gesture} {sx:.4f} {sy:.4f}", flush=True)
        else:
            # No hand visible
            last_gesture = "NONE"
            print("NONE 0 0", flush=True)

cap.release()