# 🚤  AlphaBoat Automated Waste Collecting Boat
Autonomous waste-collecting boat using Raspberry Pi 5, ESP32, and a custom YOLO model with PID-based object tracking and dual-mode (auto/manual) control.

📌 Overview <br>
This project is an autonomous waste collection boat designed to detect and collect floating waste in water bodies. It combines computer vision, embedded systems, and wireless control to operate in both automatic and manual modes.<br>

The system uses a custom-trained YOLO object detection model running on a Raspberry Pi to identify waste and coordinate collection using an ESP32-based control system.

⚙️ System Architecture
Processing Unit: Raspberry Pi 5<br>
Microcontroller: ESP32<br>
Communication: USB Serial (Pi ↔ ESP32), HC-12 (Remote Control)<br>
Vision System: USB Webcam + Custom YOLO Model<br>
Navigation: Dual Paddle Wheel Mechanism<br>
Body Material: PVC Structure<br>

🚀 Features<br>
🤖 Autonomous Mode<br>
Detects floating waste using a custom-trained YOLO model<br>
Processes real-time video from USB webcam<br>
Sends detection data to ESP32 via serial communication<br>
Automatically navigates towards detected objects<br>

🎮 Manual Mode<br>
Controlled via wireless remote controller<br>
Uses HC-12 module for long-range communication<br>
Joystick-based navigation control<br>

🚧 Obstacle & Boundary Detection<br>
Uses 3 ToF (Time-of-Flight) sensors:<br>
Front<br>
Left<br>
Right<br>
Helps avoid collisions and maintain safe navigation<br>

🧭 Navigation System<br>
Two paddle wheels for differential steering<br>
Enables forward, backward, and turning maneuvers<br>

🗑️ Waste Collection Mechanism<br>
Dual MG996 Servo Motors<br>
Operate gate mechanism for waste collection<br>
Rotate camera for better field of view<br>

🔌 Hardware Components<br>
Raspberry Pi 5<br>
ESP32 (Boat Control)<br>
ESP32 (Remote Controller)<br>
USB Webcam<br>
HC-12 Wireless Modules (x2)<br>
ToF Distance Sensors (x3)<br>
MG996 Servo Motors (x2)<br>
Paddle Wheel Motors (x2)<br>
Joystick Module (Remote)<br>
PVC Body Structure<br>

🔄 Communication Flow<br>
Camera captures live video<br>
Raspberry Pi runs YOLO model for object detection<br>
Detection results sent to ESP32 via USB Serial<br>
ESP32 processes data and controls:<br>
Motors (navigation)<br>
Servo gates (waste collection)<br>
Manual override available via HC-12 remote controller<br>

🧠 Software Stack<br>
Python (Raspberry Pi)<br>
OpenCV<br>
YOLO (Custom Model)<br>
Arduino / Embedded C (ESP32)<br>
Motor control<br>
Sensor integration<br>
Serial communication<br>

🤖 Autonomous Navigation Logic<br>

When the system enters Autonomous Mode, the boat follows a structured scanning and navigation algorithm to efficiently detect and collect waste.<br>

🔍 Scanning Strategy<br>
The camera starts at 0° (center position)<br>
Scans for waste objects for 5 seconds<br>
If no object is detected:<br>
Camera rotates to -60°<br>
Scans for 5 seconds<br>
If still no object:<br>
Camera rotates to +60°<br>
Scans for 5 seconds<br>
If no waste is found in all directions:<br>
Boat moves forward for 3 seconds<br>
The scanning cycle repeats<br>

🎯 Object Detection & Tracking<br>
Waste is detected using a custom-trained YOLO model<br>
Once an object is detected:<br>
The system switches from scanning to tracking mode<br>

🧭 Orientation Adjustment<br>
If the object is detected at an angle (-60° or +60°):<br>
The boat uses the MPU6050 IMU sensor<br>
Rotates to align with the detected object’s direction before moving forward<br>

🎮 Object Following (PID Control)<br>
Object tracking is based on pixel error:<br>
Difference between object position and image center<br>
A PID controller is used to:<br>
Adjust motor speeds<br>
Maintain alignment with the target<br>
This ensures:<br>
Smooth navigation<br>
Accurate object following<br>
Reduced oscillations<br>

🗑️ Waste Collection<br>
Once the boat reaches the object:<br>
The servo-driven gate mechanism is activated<br>
Waste is collected into the storage area<br>

🔁 Continuous Operation<br>
After collection:<br>
The system resumes the scanning cycle<br>
Continues until manually stopped or switched to manual mode<br>

📷 Future Improvements<br>
GPS-based navigation<br>
Solar power integration<br>
Improved waste classification<br>
Mobile app control interface<br>

📌 Project Highlights<br>
Integration of AI + Embedded Systems<br>
Real-time object detection on edge device<br>
Hybrid autonomous + manual control system
Practical solution for environmental cleanup
