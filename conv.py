import cv2
import numpy as np

# --- CONFIGURATION ---
VIDEO_PATH = 'input.mp4'
OUTPUT_FILE = 'custom_video.bin'
WIDTH = 128
HEIGHT = 64

# --- TIME TRIMMING ---
TARGET_FPS = 30          
START_TIME_SECONDS = 7   
END_TIME_SECONDS = 47    

def convert_video():
    cap = cv2.VideoCapture(VIDEO_PATH)

    if not cap.isOpened():
        print("Error: Could not open video file.")
        return


    original_fps = cap.get(cv2.CAP_PROP_FPS)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    video_duration = total_frames / original_fps


    if START_TIME_SECONDS >= video_duration:
        print(f"Error: Start time ({START_TIME_SECONDS}s) is beyond video length ({video_duration:.2f}s)")
        return
    
    actual_end = min(END_TIME_SECONDS, video_duration)
    requested_duration = actual_end - START_TIME_SECONDS
    max_frames_to_write = int(requested_duration * TARGET_FPS)


    cap.set(cv2.CAP_PROP_POS_MSEC, START_TIME_SECONDS * 1000)

    print(f"Original Video: {video_duration:.2f}s @ {original_fps:.2f} FPS")
    print(f"Trimming: {START_TIME_SECONDS}s to {actual_end}s ({requested_duration:.2f}s total)")
    print(f"Output: {TARGET_FPS} FPS -> {max_frames_to_write} frames total")
    print("---")
    
    frames_written = 0
    start_frame_idx = int(cap.get(cv2.CAP_PROP_POS_FRAMES))

    with open(OUTPUT_FILE, 'wb') as f:
        while cap.isOpened() and frames_written < max_frames_to_write:
            ret, frame = cap.read()
            if not ret:
                break

            current_pos_frames = int(cap.get(cv2.CAP_PROP_POS_FRAMES)) - start_frame_idx
            current_time_rel = current_pos_frames / original_fps
            
            target_frames_expected = int(current_time_rel * TARGET_FPS)

            if frames_written <= target_frames_expected:
                frame_res = cv2.resize(frame, (WIDTH, HEIGHT))
                gray = cv2.cvtColor(frame_res, cv2.COLOR_BGR2GRAY)

                _, binary = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY | cv2.THRESH_OTSU)

                flat = binary.flatten()
                byte_array = bytearray()
                for i in range(0, len(flat), 8):
                    byte_val = 0
                    for bit in range(8):
                        if flat[i + bit] > 0:
                            byte_val |= (1 << (7 - bit))
                    byte_array.append(byte_val)

                while frames_written <= target_frames_expected:
                    if frames_written >= max_frames_to_write:
                        break
                    f.write(byte_array)
                    frames_written += 1

    cap.release()

    print(f"\nDone!")
    print(f"Final File size: {os.path.getsize(OUTPUT_FILE) / 1024:.2f} KB")
    print(f"Saved to: {OUTPUT_FILE}")

import os # Needed for getsize
if __name__ == "__main__":
    convert_video()