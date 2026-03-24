# 🚤  AlphaBoat Automated Waste Collecting Boat
Autonomous waste-collecting boat using Raspberry Pi 5, ESP32, and a custom YOLO model with PID-based object tracking and dual-mode (auto/manual) control.

📌 Overview
This project is an autonomous waste collection boat designed to detect and collect floating waste in water bodies. It combines computer vision, embedded systems, and wireless control to operate in both automatic and manual modes.

The system uses a custom-trained YOLO object detection model running on a Raspberry Pi to identify waste and coordinate collection using an ESP32-based control system.

⚙️ System Architecture
Processing Unit: Raspberry Pi 5
Microcontroller: ESP32
Communication: USB Serial (Pi ↔ ESP32), HC-12 (Remote Control)
Vision System: USB Webcam + Custom YOLO Model
Navigation: Dual Paddle Wheel Mechanism
Body Material: PVC Structure

🚀 Features
🤖 Autonomous Mode
Detects floating waste using a custom-trained YOLO model
Processes real-time video from USB webcam
Sends detection data to ESP32 via serial communication
Automatically navigates towards detected objects

🎮 Manual Mode
Controlled via wireless remote controller
Uses HC-12 module for long-range communication
Joystick-based navigation control

🚧 Obstacle & Boundary Detection
Uses 3 ToF (Time-of-Flight) sensors:
Front
Left
Right
Helps avoid collisions and maintain safe navigation

🧭 Navigation System
Two paddle wheels for differential steering
Enables forward, backward, and turning maneuvers

🗑️ Waste Collection Mechanism
Dual MG996 Servo Motors
Operate gate mechanism for waste collection
Rotate camera for better field of view

🔌 Hardware Components
Raspberry Pi 5
ESP32 (Boat Control)
ESP32 (Remote Controller)
USB Webcam
HC-12 Wireless Modules (x2)
ToF Distance Sensors (x3)
MG996 Servo Motors (x2)
Paddle Wheel Motors (x2)
Joystick Module (Remote)
PVC Body Structure

🔄 Communication Flow
Camera captures live video
Raspberry Pi runs YOLO model for object detection
Detection results sent to ESP32 via USB Serial
ESP32 processes data and controls:
Motors (navigation)
Servo gates (waste collection)
Manual override available via HC-12 remote controller

🧠 Software Stack
Python (Raspberry Pi)
OpenCV
YOLO (Custom Model)
Arduino / Embedded C (ESP32)
Motor control
Sensor integration
Serial communication

🤖 Autonomous Navigation Logic

When the system enters Autonomous Mode, the boat follows a structured scanning and navigation algorithm to efficiently detect and collect waste.

🔍 Scanning Strategy
The camera starts at 0° (center position)
Scans for waste objects for 5 seconds
If no object is detected:
Camera rotates to -60°
Scans for 5 seconds
If still no object:
Camera rotates to +60°
Scans for 5 seconds
If no waste is found in all directions:
Boat moves forward for 3 seconds
The scanning cycle repeats

🎯 Object Detection & Tracking
Waste is detected using a custom-trained YOLO model
Once an object is detected:
The system switches from scanning to tracking mode

🧭 Orientation Adjustment
If the object is detected at an angle (-60° or +60°):
The boat uses the MPU6050 IMU sensor
Rotates to align with the detected object’s direction before moving forward

🎮 Object Following (PID Control)
Object tracking is based on pixel error:
Difference between object position and image center
A PID controller is used to:
Adjust motor speeds
Maintain alignment with the target
This ensures:
Smooth navigation
Accurate object following
Reduced oscillations

🗑️ Waste Collection
Once the boat reaches the object:
The servo-driven gate mechanism is activated
Waste is collected into the storage area

🔁 Continuous Operation
After collection:
The system resumes the scanning cycle
Continues until manually stopped or switched to manual mode

📷 Future Improvements
GPS-based navigation
Solar power integration
Improved waste classification
Mobile app control interface
📌 Project Highlights
Integration of AI + Embedded Systems
Real-time object detection on edge device
Hybrid autonomous + manual control system
Practical solution for environmental cleanup
