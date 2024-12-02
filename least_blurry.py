import cv2
import numpy as np
from pathlib import Path

def variance_of_laplacian(image):
    """
    Compute the Laplacian variance of the image.
    Returns a measure of image focus/blur - higher values indicate less blur.
    """
    return cv2.Laplacian(image, cv2.CV_64F).var()

def find_least_blurry_frame(video_path, output_dir=None, sample_interval=1):
    """
    Process video to find the least blurry frame.
    
    Args:
        video_path (str): Path to the video file
        output_dir (str, optional): Directory to save the frame image. If None, won't save.
        sample_interval (int): Process every Nth frame (default=1, process all frames)
    
    Returns:
        tuple: (frame_number, blur_score, frame_image)
    """
    # Open the video file
    video = cv2.VideoCapture(video_path)
    if not video.isOpened():
        raise ValueError("Could not open video file")
    
    total_frames = int(video.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = video.get(cv2.CAP_PROP_FPS)
    print(f"Video contains {total_frames} frames at {fps} fps")
    
    max_score = -float('inf')
    best_frame = None
    best_frame_number = -1
    frame_number = 0
    
    while True:
        ret, frame = video.read()
        if not ret:
            break
            
        if frame_number % sample_interval == 0:
            # Convert to grayscale for blur detection
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            score = variance_of_laplacian(gray)
            
            if score > max_score:
                max_score = score
                best_frame = frame.copy()
                best_frame_number = frame_number
                
            if frame_number % 100 == 0:
                print(f"Processed frame {frame_number}/{total_frames}")
        
        frame_number += 1
    
    video.release()
    
    # Save the best frame if output directory is specified
    if output_dir is not None and best_frame is not None:
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        output_path = output_dir / f"frame_{best_frame_number}_score_{max_score:.2f}.jpg"
        cv2.imwrite(str(output_path), best_frame)
        print(f"Saved best frame to: {output_path}")
    
    return best_frame_number, max_score, best_frame

# Example usage
if __name__ == "__main__":
    video_path = "output.mp4"
    output_dir = "output_frames"
    
    # Process every frame (sample_interval=1)
    frame_num, score, frame = find_least_blurry_frame(
        video_path,
        output_dir=output_dir,
        sample_interval=1
    )
    
    print(f"Least blurry frame: {frame_num}")
    print(f"Blur score: {score:.2f}")