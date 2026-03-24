<h1 align="center">🚤 AlphaBoat – Automated Waste Collecting Boat</h1>
<p align="center">
Autonomous waste-collecting boat using Raspberry Pi 5, ESP32, and YOLO-based object detection
</p>
<p align="center">
  <img src="https://github.com/user-attachments/assets/6599509c-509c-4058-9b40-b8ae3b211204" width="500"/>
</p>

---

## 📌 Overview

This project is an autonomous waste collection boat designed to detect and collect floating waste in water bodies. It combines computer vision, embedded systems, and wireless control to operate in both automatic and manual modes.

The system uses a custom-trained YOLO object detection model running on a Raspberry Pi to identify waste and coordinate collection using an ESP32-based control system.

---

## 🎥 Demo Video
- [Auto Mode Testing - YouTube](https://youtube.com/shorts/W3KBWQQizXY)  

## ⚙️ System Architecture

- **Processing Unit:** Raspberry Pi 5  
- **Microcontroller:** ESP32  
- **Communication:** USB Serial (Pi ↔ ESP32), HC-12 (Remote Control)  
- **Vision System:** USB Webcam + Custom YOLO Model  
- **Navigation:** Dual Paddle Wheel Mechanism  
- **Body Material:** PVC Structure  

---

## 🚀 Features

### 🤖 Autonomous Mode
- Detects floating waste using a custom-trained YOLO model  
- Processes real-time video from USB webcam  
- Sends detection data to ESP32 via serial communication  
- Automatically navigates towards detected objects  

### 🎮 Manual Mode
- Controlled via wireless remote controller  
- Uses HC-12 module for long-range communication  
- Joystick-based navigation control  

### 🚧 Obstacle & Boundary Detection
- Uses 3 ToF (Time-of-Flight) sensors:
  - Front  
  - Left  
  - Right  
- Helps avoid collisions and maintain safe navigation  

### 🧭 Navigation System
- Two paddle wheels for differential steering  
- Enables forward, backward, and turning maneuvers  

### 🗑️ Waste Collection Mechanism
- Dual MG996 Servo Motors  
- Operate gate mechanism for waste collection  
- Rotate camera for better field of view  

---

## 🔌 Hardware Components

- Raspberry Pi 5  
- ESP32 (Boat Control)  
- ESP32 (Remote Controller)  
- USB Webcam  
- HC-12 Wireless Modules (x2)  
- ToF Distance Sensors (x3)  
- MG996 Servo Motors (x2)  
- Paddle Wheel Motors (x2)  
- Joystick Module (Remote)  
- PVC Body Structure  

---

## 🔄 Communication Flow

1. Camera captures live video  
2. Raspberry Pi runs YOLO model for object detection  
3. Detection results sent to ESP32 via USB Serial  
4. ESP32 processes data and controls:
   - Motors (navigation)  
   - Servo gates (waste collection)  
5. Manual override available via HC-12 remote controller  

---

## 🧠 Software Stack

### Raspberry Pi
- Python  
- OpenCV  
- YOLO (Custom Model)  

### ESP32
- Arduino / Embedded C  
- Motor control  
- Sensor integration  
- Serial communication  

---

## 🤖 Autonomous Navigation Logic

When the system enters Autonomous Mode, the boat follows a structured scanning and navigation algorithm to efficiently detect and collect waste.

### 🔍 Scanning Strategy
1. Camera starts at **0° (center position)**  
2. Scans for **5 seconds**  
3. If no object detected:
   - Rotate to **-60°** and scan for 5 seconds  
4. If still no object:
   - Rotate to **+60°** and scan for 5 seconds  
5. If no waste found:
   - Move forward for **3 seconds**  
6. Repeat the scanning cycle  

---

### 🎯 Object Detection & Tracking
- Waste detected using custom YOLO model  
- System switches to **tracking mode** upon detection  

---

### 🧭 Orientation Adjustment
- If object detected at **±60°**:
  - Uses **MPU6050 IMU**
  - Rotates boat to align with object direction  

---

### 🎮 Object Following (PID Control)
- Based on **pixel error** (object vs image center)  
- PID controller adjusts:
  - Motor speeds  
  - Direction alignment  

**Ensures:**
- Smooth navigation  
- Accurate tracking  
- Reduced oscillations  

---

### 🗑️ Waste Collection
- Servo-driven gate mechanism activated  
- Waste collected into storage  

---

### 🔁 Continuous Operation
- After collection, system resumes scanning  
- Continues until manual stop or mode switch  

---

## 📷 Future Improvements

- GPS-based navigation  
- Solar power integration  
- Improved waste classification  
- Mobile app control  

---

## 📌 Project Highlights

- Integration of **AI + Embedded Systems**  
- Real-time **edge-based object detection**  
- Hybrid **autonomous + manual control system**  
- Practical solution for **environmental cleanup**  

---

## References


- [Creating Custom Models - YouTube](https://www.youtube.com/watch?v=r0RspiLG260&t=1085s)  
- [Raspberry Pi Object Detection - YouTube](https://www.youtube.com/watch?v=z70ZrSZNi-8&t=334s)

---
