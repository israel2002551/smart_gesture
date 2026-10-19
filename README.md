# smart_gesture

Gesture-controlled smart home prototype using an ESP32-CAM for video/fan
control, an ESP32-C3 for bulb control, and a Python MediaPipe controller.

## Setup

1. Confirm the WiFi SSID and password in both Arduino sketches.
2. Flash `AI-Thinker.ino` to the ESP32-CAM board.
3. Flash `c3.ino` to the ESP32-C3 board.
4. Update board IPs and RTSP URL in `config.py`.
5. Run:

```bash
python gesture.py
```

Press `q` in the camera window to quit.

## Gestures

- Fist: fan off
- One finger: fan speed 1
- Two fingers: fan speed 2
- Three fingers: fan speed 3
- Open palm: toggle bulb

## Board Endpoints

ESP32-CAM:

- `GET /fan?state=off|1|2|3`
- `GET /status`
- `GET /health`

ESP32-C3:

- `GET /control?state=on|off`
- `GET /status`
- `GET /health`
