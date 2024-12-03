from flask import Flask, request, jsonify
import datetime
import os
import cv2
from natsort import natsorted
import base64
import requests
from typing import Optional, Dict, Union
from openai import AzureOpenAI
from dotenv import load_dotenv

latest_response = None
load_dotenv()
client_id = os.getenv('IMGUR_CLIENT_ID')
app = Flask(__name__)

api_base = os.getenv("AZURE_OPENAI_ENDPOINT")
api_key= os.getenv("AZURE_OPENAI_API_KEY")
deployment_name = 'NewOmni'
api_version = '2023-12-01-preview' # this might change in the future

client = AzureOpenAI(
    api_key=api_key,  
    api_version=api_version,
    base_url=f"{api_base}/openai/deployments/{deployment_name}"
)

# Create necessary directories
UPLOAD_FOLDER = 'uploads'
OUTPUT_FOLDER = 'output'
for folder in [UPLOAD_FOLDER, OUTPUT_FOLDER]:
    if not os.path.exists(folder):
        os.makedirs(folder)

# Counter for frame numbering
frame_counter = 0

def getResponse(img_url):
    response = client.chat.completions.create(
        model=deployment_name,
        messages=[
            { "role": "system", "content": "You are a helpful and truthful assistant" },
            { "role": "user", "content": [  
                { 
                    "type": "text", 
                    "text": "Answer this question for me. Do chain of thought to explain your thought process. The text response you return will be printed on a single line OLED display so make sure your answer is super concise - super concise! Also make sure you don't use any weird symbols. And don't use any lines. And don't do markdown formatting. Super concise is key! Your answer should be literally one line. It's ok if it's not as explained but it should be within a line." 
                },
                { 
                    "type": "image_url",
                    "image_url": {
                        "url": img_url
                    }
                }
            ] } 
        ],
        max_tokens=2000 
    )    
    return response.choices[0].message.content

def upload_to_imgur(
    image_path: str,
    client_id: str,
    title: Optional[str] = None,
    description: Optional[str] = None,
    album_id: Optional[str] = None
) -> Dict[str, Union[bool, str, dict]]:
    # Check if file exists
    if not os.path.exists(image_path):
        raise FileNotFoundError(f"Image file not found: {image_path}")
        
    # Check file size (Imgur limit is 10MB)
    file_size = os.path.getsize(image_path)
    if file_size > 10 * 1024 * 1024:  # 10MB in bytes
        raise ValueError("File size exceeds Imgur's 10MB limit")
        
    # Read and encode the image
    with open(image_path, 'rb') as image_file:
        image_data = base64.b64encode(image_file.read())
        
    # Prepare headers and payload
    headers = {
        'Authorization': f'Client-ID {client_id}'
    }
    
    payload = {
        'image': image_data,
        'type': 'base64'
    }
    
    # Add optional parameters if provided
    if title:
        payload['title'] = title
    if description:
        payload['description'] = description
    if album_id:
        payload['album'] = album_id
        
    try:
        # Make the upload request
        response = requests.post(
            'https://api.imgur.com/3/image',
            headers=headers,
            data=payload
        )
        response.raise_for_status()
        
        # Parse and return the response
        json_response = response.json()
        return {
            'success': json_response['success'],
            'status': response.status_code,
            'image_data': json_response['data']
        }
        
    except requests.RequestException as e:
        return {
            'success': False,
            'status': getattr(e.response, 'status_code', None),
            'error': str(e)
        }

def frames_to_video(frame_folder='uploads', output_name='output.mp4', fps=5):
    """
    Convert a folder of frames into a video file.
    """
    # Get list of frames
    frames = [f for f in os.listdir(frame_folder) if f.endswith('.jpg')]
    if not frames:
        print("No frames found in the specified folder")
        return None
    
    # Sort frames naturally
    frames = natsorted(frames)
    
    # Read first frame to get dimensions
    first_frame = cv2.imread(os.path.join(frame_folder, frames[0]))
    height, width, _ = first_frame.shape
    
    # Initialize video writer
    output_path = os.path.join(OUTPUT_FOLDER, output_name)
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))
    
    # Add frames to video
    frame_count = 0
    for frame_name in frames:
        frame_path = os.path.join(frame_folder, frame_name)
        frame = cv2.imread(frame_path)
        out.write(frame)
        frame_count += 1
        
        if frame_count % 50 == 0:
            print(f"Processed {frame_count} frames")
    
    # Release video writer
    out.release()
    print(f"Video created successfully with {frame_count} frames")
    return output_path

def variance_of_laplacian(image):
    """
    Compute the Laplacian variance of the image.
    """
    return cv2.Laplacian(image, cv2.CV_64F).var()

def find_least_blurry_frame(video_path, output_dir, sample_interval=1):
    """
    Process video to find the least blurry frame.
    """
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
    
    # Save the best frame
    if best_frame is not None:
        output_path = os.path.join(output_dir, f"best_frame_{best_frame_number}_score_{max_score:.2f}.jpg")
        cv2.imwrite(output_path, best_frame)
        print(f"Saved best frame to: {output_path}")
    
    return best_frame_number, max_score, output_path

@app.route('/upload', methods=['POST'])
def upload_file():
    global frame_counter
    try:
        # Get the image data from the request
        image_data = request.data
        
        # Generate filename with frame number
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"frame_{timestamp}_{frame_counter:04d}.jpg"
        filepath = os.path.join(UPLOAD_FOLDER, filename)
        
        # Save the image
        with open(filepath, 'wb') as f:
            f.write(image_data)
            
        frame_counter += 1
        print(f"Received frame {frame_counter}")
            
        return 'Frame received', 200
    except Exception as e:
        return f'Error: {str(e)}', 500

@app.route('/finished', methods=['POST'])
def process_video():
    global latest_response
    try:
        print("Processing video from frames...")
        latest_response = None
        
        # Create video from frames
        video_path = frames_to_video(
            frame_folder=UPLOAD_FOLDER,
            output_name='output.mp4',
            fps=5
        )
        
        if video_path is None:
            return 'No frames found to process', 400
        
        print("Finding least blurry frame...")
        # Find and save the least blurry frame
        frame_num, score, best_frame_path = find_least_blurry_frame(
            video_path,
            OUTPUT_FOLDER,
            sample_interval=1
        )
        
        print(f"Processing complete:")
        print(f"- Video saved to: {video_path}")
        print(f"- Best frame ({frame_num}) saved to: {best_frame_path}")
        print(f"- Blur score: {score:.2f}")
        result = upload_to_imgur(
            image_path=best_frame_path,
            client_id=client_id,
        )        

        if result['success']:
            img_url = result['image_data']['link']
            print(f"Upload successful!")
            print(f"Image URL: {img_url}")
            print(f"Delete hash: {result['image_data']['deletehash']}")

            latest_response = getResponse(img_url=img_url)
            print("GPT Response:", latest_response)
            return jsonify({'status': 'Processing complete'}), 200
        else:
            print(f"Upload failed: {result.get('error', 'Unknown error')}")        
        
        return 'Processing complete', 200
    except Exception as e:
        print(f"Error during processing: {str(e)}")
        return f'Error: {str(e)}', 500

@app.route('/get_response', methods=['GET'])
def get_response():
    global latest_response
    if latest_response is None:
        return jsonify({'response': 'Processing not complete yet'}), 202  # 202 means "accepted but processing"
    return jsonify({'response': latest_response}), 200


if __name__ == '__main__':
    # Clear or create new directories
    for folder in [UPLOAD_FOLDER, OUTPUT_FOLDER]:
        if os.path.exists(folder):
            # Clear existing files
            for file in os.listdir(folder):
                os.remove(os.path.join(folder, file))
        else:
            os.makedirs(folder)
        
    print("Server started. Waiting for frames...")
    app.run(host='0.0.0.0', port=5002, threaded=True)