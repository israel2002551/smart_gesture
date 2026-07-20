import cv2
import mediapipe as mp
import pyttsx3
import requests
import time
import threading

# Network IPs
CAM_IP = "http://192.168.1.100"   # ESP32-CAM Board
C3_IP = "http://192.168.1.101"    # ESP32-C3 Board
RTSP_URL = f"rtsp://192.168.1.100:8554/mjpeg"

engine = pyttsx3.init()
engine.setProperty('rate', 150)

def speak(text):
    def _speak():
        engine.say(text)
        engine.runAndWait()
    threading.Thread(target=_speak, daemon=True).start()

def route_hardware(device, state):
    """Routes calls to their distinct target boards while forcing complete mutual exclusivity."""
    try:
        if device == "bulb" and state == "on":
            # Force close all fan relays first
            requests.get(f"{CAM_IP}/fan", params={"state": "off"}, timeout=1)
            # Turn on bulb
            requests.get(f"{C3_IP}/control", params={"state": "on"}, timeout=1)
            
        elif device == "bulb" and state == "off":
            requests.get(f"{C3_IP}/control", params={"state": "off"}, timeout=1)
            
        elif device == "fan":
            # Force close light bulb relay first
            requests.get(f"{C3_IP}/control", params={"state": "off"}, timeout=1)
            # Turn on specific fan speed
            requests.get(f"{CAM_IP}/fan", params={"state": state}, timeout=1)
            
    except requests.exceptions.RequestException as e:
        print(f"Network routing failed: {e}")

# MediaPipe Initialization
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(max_num_hands=1, min_detection_confidence=0.7, min_tracking_confidence=0.7)

bulb_state = "off"
fan_state = "off"
last_action_time = 0
gesture_cooldown_time = 2.0

cap = cv2.VideoCapture(RTSP_URL)

while cap.isOpened():
    success, frame = cap.read()
    if not success: continue

    frame = cv2.flip(frame, 1)
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = hands.process(rgb_frame)

    if results.multi_hand_landmarks:
        for hand_landmarks in results.multi_hand_landmarks:
            landmarks = hand_landmarks.landmark
            fingers_up = []
            
            # Thumb
            if landmarks[4].x < landmarks[3].x: fingers_up.append(1)
            else: fingers_up.append(0)
            
            # Others
            for tip in [8, 12, 16, 20]:
                if landmarks[tip].y < landmarks[tip - 2].y: fingers_up.append(1)
                else: fingers_up.append(0)

            total_fingers = sum(fingers_up)
            current_time = time.time()

            if (current_time - last_action_time) > gesture_cooldown_time:
                # ✊ Fist: Turn off Fan
                if total_fingers == 0 and fan_state != "off":
                    fan_state = "off"
                    speak("fan off")
                    route_hardware("fan", "off")
                    last_action_time = current_time

                # ☝️ 1 Finger: Fan Speed 1
                elif total_fingers == 1 and fingers_up[1] == 1 and fan_state != "1":
                    fan_state = "1"
                    bulb_state = "off" # Soft track state sync
                    speak("fan set speed 1")
                    route_hardware("fan", "1")
                    last_action_time = current_time

                # ✌️ 2 Fingers: Fan Speed 2
                elif total_fingers == 2 and fingers_up[1] == 1 and fingers_up[2] == 1 and fan_state != "2":
                    fan_state = "2"
                    bulb_state = "off"
                    speak("fan set at speed 2")
                    route_hardware("fan", "2")
                    last_action_time = current_time

                # 3 Fingers: Fan Speed 3
                elif total_fingers == 3 and fingers_up[4] == 0 and fan_state != "3":
                    fan_state = "3"
                    bulb_state = "off"
                    speak("fan set speed 3")
                    route_hardware("fan", "3")
                    last_action_time = current_time

                # ✋ 5 Fingers: Toggle Bulb
                elif total_fingers == 5:
                    if bulb_state == "off":
                        bulb_state = "on"
                        fan_state = "off"
                        speak("light on")
                        route_hardware("bulb", "on")
                    else:
                        bulb_state = "off"
                        speak("light off")
                        route_hardware("bulb", "off")
                    last_action_time = current_time

    cv2.imshow("Smart Home", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'): break

cap.release()
cv2.destroyAllWindows()
