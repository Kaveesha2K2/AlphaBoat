import os
import sys
import argparse
import glob
import time
import serial
import threading
import queue

import cv2
import numpy as np
from ultralytics import YOLO

# Define and parse user input arguments
parser = argparse.ArgumentParser()
parser.add_argument('--model', help='Path to YOLO model file', required=True)
parser.add_argument('--source', help='Image source (e.g., "0" for webcam)', required=True)
parser.add_argument('--thresh', help='Minimum confidence threshold', default=0.5)
parser.add_argument('--resolution', help='Resolution in WxH', default=None)
parser.add_argument('--record', help='Record results', action='store_true')

args = parser.parse_args()

model_path = args.model
img_source = args.source
min_thresh = float(args.thresh)
user_res = args.resolution
record = args.record

if not os.path.exists(model_path):
    print('ERROR: Model path is invalid or model was not found.')
    sys.exit(0)

model = YOLO(model_path, task='detect')
labels = model.names

# Parse input to determine image source
img_ext_list = ['.jpg', '.JPG', '.jpeg', '.JPEG', '.png', '.PNG', '.bmp', '.BMP']
vid_ext_list = ['.avi', '.mov', '.mp4', '.mkv', '.wmv']

if os.path.isdir(img_source):
    source_type = 'folder'
elif os.path.isfile(img_source):
    _, ext = os.path.splitext(img_source)
    if ext in img_ext_list:
        source_type = 'image'
    elif ext in vid_ext_list:
        source_type = 'video'
    else:
        sys.exit(0)
elif 'usb' in img_source:
    source_type = 'usb'
    usb_idx = int(img_source[3:])
elif 'picamera' in img_source:
    source_type = 'picamera'
    picam_idx = int(img_source[8:])
elif '0' in img_source:
    source_type = 'usb'
    usb_idx = 0
else:
    sys.exit(0)

resize = False
if user_res:
    resize = True
    resW, resH = int(user_res.split('x')[0]), int(user_res.split('x')[1])

if record:
    record_name = 'demo1.avi'
    record_fps = 30
    recorder = cv2.VideoWriter(record_name, cv2.VideoWriter_fourcc(*'MJPG'), record_fps, (resW, resH))

if source_type == 'image':
    imgs_list = [img_source]
elif source_type == 'folder':
    imgs_list = glob.glob(img_source + '/*')
elif source_type in ['video', 'usb']:
    cap_arg = img_source if source_type == 'video' else usb_idx
    cap = cv2.VideoCapture(cap_arg)
    if user_res:
        cap.set(3, resW)
        cap.set(4, resH)
elif source_type == 'picamera':
    from picamera2 import Picamera2
    cap = Picamera2()
    cap.configure(cap.create_video_configuration(
        main={"format": 'XRGB8888', "size": (resW, resH)}))
    cap.start()

bbox_colors = [(164, 120, 87), (68, 148, 228), (93, 97, 209), (178, 182, 133), (88, 159, 106),
               (96, 202, 231), (159, 124, 168), (169, 162, 241), (98, 118, 150), (172, 176, 184)]

# --- SERIAL SETUP ---
try:
    esp32 = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
    print("Successfully connected to ESP32 on /dev/ttyUSB0")
except serial.SerialException:
    print("Warning: ESP32 not connected. Running without serial transmission.")
    esp32 = None

# --- MODE TRACKING ---
current_mode = None
mode_lock = threading.Lock()

# --- AUTO MODE STATE ---
last_detection_time = time.time()
last_seen_for_rotation = time.time()
target_timeout = 2.0
no_target_time = None
close_gate_timeout = 2.0
long_no_target_timeout = 10.0
rotate_check_timeout = 6.0
rotation_state = None
rotation_cycle_start = None
rotation_message_sent = False
gateClosed = False
current_cam_angle = 0


def reset_auto_state():
    global rotation_state, rotation_cycle_start, rotation_message_sent
    global no_target_time, gateClosed, last_detection_time, last_seen_for_rotation
    global current_cam_angle

    rotation_state = None
    rotation_cycle_start = None
    rotation_message_sent = False
    no_target_time = None
    gateClosed = False
    last_detection_time = time.time()
    last_seen_for_rotation = time.time()
    current_cam_angle = 0
    print("[MODE SWITCH] Auto state fully reset")


# --- SERIAL WRITE HELPER ---
# Centralized serial write with flush to avoid buffer lag
def serial_write(msg: str):
    if esp32:
        try:
            esp32.write(msg.encode('utf-8'))
            esp32.flush()  # force immediate send, no buffer lag
        except Exception as e:
            print(f"[Serial Write Error] {e}")


# --- SERIAL READ THREAD ---
def _read_and_print_serial():
    global current_mode

    while True:
        if esp32:
            try:
                line = esp32.readline().decode('utf-8', errors='ignore').strip()

                if line:
                    print(f"[ESP32 RX] {line}")

                    lower = line.lower()

                    # Bug 2 fix: only parse lines prefixed with "mode:"
                    # prevents debug prints like "Left: 150" triggering false switches
                    if lower.startswith("mode:"):
                        mode_str = line[5:].strip()

                        with mode_lock:
                            previous = current_mode  # read inside lock

                            if "RC" in mode_str:
                                current_mode = "RC mode"
                            elif "Auto" in mode_str:
                                current_mode = "Auto mode"

                        if previous != current_mode:
                            print(f"[MODE SWITCH] Switched to {current_mode}")
                            serial_write("Rotate Cam:0\n")
                            reset_auto_state()

            except Exception as e:
                print(f"[Serial Read Error] {e}")

        time.sleep(0.02)


# --- YOLO INFERENCE THREAD ---
# Runs inference in background so main loop never blocks waiting for YOLO
frame_queue = queue.Queue(maxsize=1)
result_queue = queue.Queue(maxsize=s1)


def inference_thread():
    while True:
        try:
            # Block until a frame is available
            frame = frame_queue.get()
            results = model(frame, verbose=False)

            # Drop old result if main loop hasn't consumed it yet
            # This ensures we always have the freshest detection
            if result_queue.full():
                try:
                    result_queue.get_nowait()
                except queue.Empty:
                    pass

            result_queue.put(results)

        except Exception as e:
            print(f"[Inference Error] {e}")


inf_thread = threading.Thread(target=inference_thread, daemon=True)
inf_thread.start()

# --- START SERIAL THREAD ---
try:
    if esp32:
        _serial_thread = threading.Thread(target=_read_and_print_serial, daemon=True)
        _serial_thread.start()

        # Retry handshake until mode received
        start_wait = time.time()
        last_send = 0
        resend_delay = 2.0

        while True:
            if time.time() - last_send > resend_delay:
                serial_write("Pi Started\n")
                print("[ESP32 TX] Pi Started (retry)")
                last_send = time.time()

            with mode_lock:
                mode_snapshot = current_mode

            if mode_snapshot in ('RC mode', 'Auto mode'):
                break

            if time.time() - start_wait > 30:
                print("Warning: mode response timeout, defaulting to Auto mode")
                with mode_lock:
                    current_mode = 'Auto mode'
                break

            time.sleep(0.1)

        with mode_lock:
            print(f"Initial mode set to: {current_mode}")

except Exception as e:
    print(f"Serial startup error: {e}")
    with mode_lock:
        current_mode = 'Auto mode'

# --- MAIN LOOP ---
avg_frame_rate = 0
frame_rate_buffer = []
fps_avg_len = 200
img_count = 0

while True:
    t_start = time.perf_counter()

    # Take a mode snapshot once per frame
    with mode_lock:
        active_mode = current_mode

    if source_type in ['image', 'folder']:
        if img_count >= len(imgs_list):
            break
        frame = cv2.imread(imgs_list[img_count])
        img_count += 1
    elif source_type in ['video', 'usb']:
        ret, frame = cap.read()
        if not ret or frame is None:
            break
    elif source_type == 'picamera':
        frame_bgra = cap.capture_array()
        frame = cv2.cvtColor(np.copy(frame_bgra), cv2.COLOR_BGRA2BGR)
        if frame is None:
            break

    if resize:
        frame = cv2.resize(frame, (resW, resH))

    # --- DETECTION (Auto mode only) ---
    results = None
    detections = []

    if active_mode == "Auto mode":
        # Push frame to inference thread (non-blocking)
        # If queue is full, drop oldest frame — freshness over completeness
        if not frame_queue.full():
            frame_queue.put_nowait(frame.copy())

        # Grab latest result if available (non-blocking)
        if not result_queue.empty():
            results = result_queue.get_nowait()
            detections = results[0].boxes
    else:
        print(f"[RC MODE] Detection skipped.")

    object_count = 0
    target_found = False
    valid_detections = []

    if results is not None:
        for i in range(len(detections)):
            xyxy_tensor = detections[i].xyxy.cpu()
            xyxy = xyxy_tensor.numpy().squeeze()

            if xyxy.ndim == 0 or len(xyxy) < 4:
                continue

            xmin, ymin, xmax, ymax = xyxy.astype(int)
            classidx = int(detections[i].cls.item())
            classname = labels[classidx]
            conf = detections[i].conf.item()

            if conf > min_thresh:
                bbox_area = (xmax - xmin) * (ymax - ymin)
                valid_detections.append({
                    'xmin': xmin, 'ymin': ymin, 'xmax': xmax, 'ymax': ymax,
                    'classidx': classidx, 'classname': classname, 'conf': conf,
                    'area': bbox_area
                })
                object_count += 1
                target_found = True

                if current_cam_angle != 0:
                    msg = f"Rotate Cam:0 &{current_cam_angle}\n"
                    print(msg.strip())
                    serial_write(msg)
                    current_cam_angle = 0

                last_detection_time = time.time()
                last_seen_for_rotation = time.time()
                rotation_state = None
                rotation_cycle_start = None
                rotation_message_sent = False

    # Largest detection
    largest_detection = None
    if len(valid_detections) > 0:
        largest_detection = max(valid_detections, key=lambda x: x['area'])

    # Draw and send serial for detections
    for det in valid_detections:
        xmin, ymin, xmax, ymax = det['xmin'], det['ymin'], det['xmax'], det['ymax']
        classidx = det['classidx']
        classname = det['classname']
        conf = det['conf']

        color = bbox_colors[classidx % 10]
        cv2.rectangle(frame, (xmin, ymin), (xmax, ymax), color, 2)
        label = f'{classname}: {int(conf*100)}%'
        cv2.putText(frame, label, (xmin, ymin - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 2)

        frame_center_x = frame.shape[1] // 2
        box_center_x = int((xmin + xmax) / 2)
        error_x = box_center_x - frame_center_x

        if det is largest_detection:
            # Re-check mode before sending to avoid stale active_mode
            # sending Target Acquired during a mode switch mid-inference
            with mode_lock:
                safe_to_send = (current_mode == "Auto mode")

            if safe_to_send:
                message = f"Target Acquired!: {error_x}\n"
                gateClosed = False
                serial_write(message)

        cv2.line(frame, (frame_center_x, 0), (frame_center_x, frame.shape[0]), (0, 0, 255), 2)
        cv2.circle(frame, (box_center_x, int((ymin + ymax) / 2)), 5, (0, 255, 0), -1)
        cv2.putText(frame, f'Err: {error_x}', (xmin, ymax + 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

    # --- NO TARGET LOGIC (Auto mode only) ---
    if not target_found:
        if active_mode == "Auto mode":
            time_since_last_seen = time.time() - last_detection_time
            time_since_truly_last_seen = time.time() - last_seen_for_rotation

            if time_since_last_seen > target_timeout:
                serial_write("No target\n")
                if no_target_time is None:
                    no_target_time = time.time()
                last_detection_time = time.time()

            if no_target_time is not None:
                if time.time() - no_target_time > close_gate_timeout and not gateClosed:
                    serial_write("Close Gate\n")
                    no_target_time = None
                    gateClosed = True

            if time_since_truly_last_seen > long_no_target_timeout:
                if rotation_state is None:
                    rotation_state = 'rotate_90'
                    rotation_cycle_start = time.time()
                    rotation_message_sent = False

                time_in_state = time.time() - rotation_cycle_start

                if rotation_state == 'rotate_90':
                    if not rotation_message_sent:
                        serial_write("Rotate Cam:60\n")
                        current_cam_angle = 60
                        rotation_message_sent = True
                        rotation_state = 'checking_90'
                        rotation_cycle_start = time.time()

                elif rotation_state == 'checking_90':
                    if time_in_state > rotate_check_timeout:
                        rotation_state = 'rotate_minus_90'
                        rotation_cycle_start = time.time()
                        rotation_message_sent = False

                elif rotation_state == 'rotate_minus_90':
                    if not rotation_message_sent:
                        serial_write("Rotate Cam:-60\n")
                        current_cam_angle = -60
                        rotation_message_sent = True
                        rotation_state = 'checking_minus_90'
                        rotation_cycle_start = time.time()

                elif rotation_state == 'checking_minus_90':
                    if time_in_state > rotate_check_timeout:
                        rotation_state = 'move_forward'
                        rotation_cycle_start = time.time()
                        rotation_message_sent = False

                elif rotation_state == 'move_forward':
                    if not rotation_message_sent:
                        serial_write("Move Forward\n")
                        rotation_message_sent = True
                        rotation_state = 'center_camera'
                        rotation_cycle_start = time.time()
                        rotation_message_sent = False

                elif rotation_state == 'center_camera':
                    if not rotation_message_sent:
                        serial_write("Rotate Cam:0\n")
                        current_cam_angle = 0
                        rotation_message_sent = True
                        rotation_state = 'checking_center'
                        rotation_cycle_start = time.time()

                elif rotation_state == 'checking_center':
                    if time_in_state > rotate_check_timeout:
                        rotation_state = 'rotate_90'
                        rotation_cycle_start = time.time()
                        rotation_message_sent = False

    else:
        no_target_time = None
        last_seen_for_rotation = time.time()
        rotation_state = None
        rotation_cycle_start = None
        rotation_message_sent = False

    # --- HUD ---
    if source_type in ['video', 'usb', 'picamera']:
        cv2.putText(frame, f'FPS: {avg_frame_rate:0.2f}', (10, 20),
                    cv2.FONT_HERSHEY_SIMPLEX, .7, (0, 255, 255), 2)

    cv2.putText(frame, f'Mode: {active_mode}', (10, 60),
                cv2.FONT_HERSHEY_SIMPLEX, .7, (0, 255, 255), 2)
    cv2.putText(frame, f'Objects: {object_count}', (10, 40),
                cv2.FONT_HERSHEY_SIMPLEX, .7, (0, 255, 255), 2)

    cv2.imshow('YOLO detection results', frame)
    if record:
        recorder.write(frame)

    key = cv2.waitKey(1 if source_type not in ['image', 'folder'] else 0)
    if key in [ord('q'), ord('Q')]:
        break

    t_stop = time.perf_counter()
    frame_rate_buffer.append(float(1 / (t_stop - t_start)))
    if len(frame_rate_buffer) >= fps_avg_len:
        frame_rate_buffer.pop(0)
    avg_frame_rate = np.mean(frame_rate_buffer)

# --- CLEANUP ---
print(f'Average pipeline FPS: {avg_frame_rate:.2f}')
if source_type in ['video', 'usb']:
    cap.release()
elif source_type == 'picamera':
    cap.stop()
if record:
    recorder.release()
cv2.destroyAllWindows()