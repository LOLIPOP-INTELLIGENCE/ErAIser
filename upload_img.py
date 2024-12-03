import base64
import requests
import os
from dotenv import load_dotenv

load_dotenv()
from typing import Optional, Dict, Union

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

# Example usage:
if __name__ == "__main__":
    client_id = os.getenv('IMGUR_CLIENT_ID')

    # Example upload
    try:
        result = upload_to_imgur(
            image_path='/Users/blackhole/Desktop/Code.nosync/ChatGPTEraser/output/best_frame_5_score_1.73.jpg',
            client_id=client_id,
            title='My Uploaded Image',
            description='A test upload using the Imgur API'
        )
        
        if result['success']:
            print(f"Upload successful!")
            print(f"Image URL: {result['image_data']['link']}")
            print(f"Delete hash: {result['image_data']['deletehash']}")
        else:
            print(f"Upload failed: {result.get('error', 'Unknown error')}")
            
    except Exception as e:
        print(f"Error: {str(e)}")