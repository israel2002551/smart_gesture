from dataclasses import dataclass
import os
import threading
import time
import warnings

os.environ.setdefault("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp")
os.environ.setdefault("OPENCV_LOG_LEVEL", "ERROR")
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")
warnings.filterwarnings(
    "ignore",
    message=r"SymbolDatabase\.GetPrototype\(\) is deprecated.*",
    category=UserWarning,
)

import cv2
import mediapipe as mp
import pyttsx3
import requests

CAM_IP = "http://10.95.238.216"
C3_IP = "http://10.95.238.93"
RTSP_URL = "rtsp://10.60.1.77:554/"

REQUEST_TIMEOUT_SECONDS = 1.5
GESTURE_COOLDOWN_SECONDS = 2.0
CAMERA_RETRY_SECONDS = 3.0
PROCESS_EVERY_N_FRAMES = 2
MAX_PROCESSING_WIDTH = 480

VOICE_RATE = 150
HAND_DETECTION_CONFIDENCE = 0.7
HAND_TRACKING_CONFIDENCE = 0.7

if not hasattr(mp, "solutions"):
    raise RuntimeError(
        "This project needs the classic MediaPipe solutions API. "
        "Create the virtual environment with Python 3.10, 3.11, or 3.12, "
        "then install mediapipe 0.10.x."
    )


GESTURE_ACTIONS = {
    "fist": ("fan", "off", "fan off"),
    "one": ("fan", "1", "fan set speed 1"),
    "two": ("fan", "2", "fan set speed 2"),
    "three": ("fan", "3", "fan set speed 3"),
    "open_palm": ("bulb", "toggle", None),
}


@dataclass
class SmartHomeState:
    bulb: str = "off"
    fan: str = "off"


class Speaker:
    def __init__(self):
        self.engine = pyttsx3.init()
        self.engine.setProperty("rate", VOICE_RATE)
        self.lock = threading.Lock()

    def say(self, text):
        def _speak():
            with self.lock:
                self.engine.say(text)
                self.engine.runAndWait()

        threading.Thread(target=_speak, daemon=True).start()


class HardwareClient:
    def __init__(self, state):
        self.state = state

    def _get(self, base_url, path, params=None):
        response = requests.get(
            f"{base_url}{path}",
            params=params,
            timeout=REQUEST_TIMEOUT_SECONDS,
        )
        response.raise_for_status()
        return response

    def set_bulb(self, state):
        if state not in {"on", "off"}:
            raise ValueError(f"Invalid bulb state: {state}")

        if state == "on" and self.state.fan != "off":
            self.set_fan("off", sync_only=True)

        self._get(C3_IP, "/control", {"state": state})
        self.state.bulb = state
        return self.get_bulb_status()

    def set_fan(self, state, sync_only=False):
        if state not in {"off", "1", "2", "3"}:
            raise ValueError(f"Invalid fan state: {state}")

        if state != "off" and not sync_only and self.state.bulb != "off":
            self.set_bulb("off")

        self._get(CAM_IP, "/fan", {"state": state})
        self.state.fan = state
        return self.get_fan_status()

    def get_bulb_status(self):
        try:
            return self._get(C3_IP, "/status").json()
        except requests.RequestException as exc:
            print(f"Bulb status unavailable: {exc}")
            return None

    def get_fan_status(self):
        try:
            return self._get(CAM_IP, "/status").json()
        except requests.RequestException as exc:
            print(f"Fan status unavailable: {exc}")
            return None

    def apply(self, device, desired_state):
        if device == "bulb":
            next_state = "off" if self.state.bulb == "on" else "on"
            if desired_state != "toggle":
                next_state = desired_state
            return self.set_bulb(next_state), f"light {next_state}"

        if device == "fan":
            return self.set_fan(desired_state), None

        raise ValueError(f"Unknown device: {device}")


def classify_gesture(landmarks):
    fingers = []

    fingers.append(1 if landmarks[4].x < landmarks[3].x else 0)
    for tip in [8, 12, 16, 20]:
        fingers.append(1 if landmarks[tip].y < landmarks[tip - 2].y else 0)

    total = sum(fingers)

    if total == 0:
        return "fist"
    if total == 1 and fingers[1] == 1:
        return "one"
    if total == 2 and fingers[1] == 1 and fingers[2] == 1:
        return "two"
    if total == 3 and fingers[1] == 1 and fingers[2] == 1 and fingers[3] == 1:
        return "three"
    if total == 5:
        return "open_palm"

    return None


def draw_status(frame, state, raw_gesture, last_action):
    text = (
        f"Fan: {state.fan} | Light: {state.bulb} | "
        f"Gesture: {raw_gesture or '-'} | Last action: {last_action or '-'}"
    )
    cv2.putText(frame, text, (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)


def resize_for_processing(frame):
    height, width = frame.shape[:2]
    if width == 0 or height == 0:
        return None

    if width <= MAX_PROCESSING_WIDTH:
        return frame

    scale = MAX_PROCESSING_WIDTH / width
    size = (MAX_PROCESSING_WIDTH, int(height * scale))
    return cv2.resize(frame, size, interpolation=cv2.INTER_AREA)


def open_camera():
    while True:
        cap = cv2.VideoCapture(RTSP_URL)
        if cap.isOpened():
            cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
            return cap

        print(f"Camera unavailable. Retrying in {CAMERA_RETRY_SECONDS} seconds...")
        time.sleep(CAMERA_RETRY_SECONDS)


def main():
    state = SmartHomeState()
    speaker = Speaker()
    hardware = HardwareClient(state)
    last_action_time = 0

    mp_hands = mp.solutions.hands
    mp_draw = mp.solutions.drawing_utils
    hands = mp_hands.Hands(
        model_complexity=0,
        max_num_hands=1,
        min_detection_confidence=HAND_DETECTION_CONFIDENCE,
        min_tracking_confidence=HAND_TRACKING_CONFIDENCE,
    )

    cap = open_camera()
    frame_count = 0
    raw_gesture = None
    last_action_gesture = None

    while True:
        success, frame = cap.read()
        if not success or frame is None or frame.size == 0:
            cap.release()
            cap = open_camera()
            continue

        height, width = frame.shape[:2]
        if width == 0 or height == 0:
            continue

        frame = cv2.flip(frame, 1)
        frame_count += 1

        detected_gesture = None
        if frame_count % PROCESS_EVERY_N_FRAMES == 0:
            process_frame = resize_for_processing(frame)
            if process_frame is None:
                continue

            rgb_frame = cv2.cvtColor(process_frame, cv2.COLOR_BGR2RGB)
            rgb_frame.flags.writeable = False
            results = hands.process(rgb_frame)
            raw_gesture = None

            if results.multi_hand_landmarks:
                raw_gesture = classify_gesture(results.multi_hand_landmarks[0].landmark)
                for hand_landmarks in results.multi_hand_landmarks:
                    mp_draw.draw_landmarks(
                        frame,
                        hand_landmarks,
                        mp_hands.HAND_CONNECTIONS,
                    )

            detected_gesture = raw_gesture

        now = time.time()

        if detected_gesture and now - last_action_time > GESTURE_COOLDOWN_SECONDS:
            device, desired_state, message = GESTURE_ACTIONS[detected_gesture]

            try:
                _, dynamic_message = hardware.apply(device, desired_state)
                speaker.say(message or dynamic_message)
                last_action_time = now
                last_action_gesture = detected_gesture
            except (ValueError, requests.RequestException) as exc:
                print(f"Command failed: {exc}")
                speaker.say("device connection failed")

        draw_status(frame, state, raw_gesture, last_action_gesture)
        cv2.imshow("Smart Home", frame)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    hands.close()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
