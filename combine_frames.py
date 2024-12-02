import cv2
import os
from natsort import natsorted  # for natural sorting of filenames

def frames_to_video(frame_folder='uploads', output_name='output.mp4', fps=5):
    """
    Convert a folder of frames into a video file.
    
    Args:
        frame_folder (str): Path to folder containing frames
        output_name (str): Name of output video file
        fps (int): Frames per second for output video
    """
    # Get list of frames
    frames = [f for f in os.listdir(frame_folder) if f.endswith('.jpg')]
    if not frames:
        print("No frames found in the specified folder")
        return
    
    # Sort frames naturally (so frame_10 comes after frame_9, not frame_1)
    frames = natsorted(frames)
    
    # Read first frame to get dimensions
    first_frame = cv2.imread(os.path.join(frame_folder, frames[0]))
    height, width, _ = first_frame.shape
    
    # Initialize video writer
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(output_name, fourcc, fps, (width, height))
    
    # Add frames to video
    frame_count = 0
    for frame_name in frames:
        frame_path = os.path.join(frame_folder, frame_name)
        frame = cv2.imread(frame_path)
        out.write(frame)
        frame_count += 1
        
        # Print progress every 100 frames
        if frame_count % 50 == 0:
            print(f"Processed {frame_count} frames")
    
    # Release video writer
    out.release()
    print(f"Video created successfully with {frame_count} frames")

if __name__ == "__main__":
    frames_to_video()  # Use default parameters or specify your own