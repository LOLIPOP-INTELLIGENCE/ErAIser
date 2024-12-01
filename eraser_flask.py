from flask import Flask, request
import datetime
import os

app = Flask(__name__)

# Create an 'uploads' directory if it doesn't exist
UPLOAD_FOLDER = 'uploads'
if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

# Counter for frame numbering
frame_counter = 0

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

if __name__ == '__main__':
    # Clear or create a new uploads directory
    if os.path.exists(UPLOAD_FOLDER):
        # Optional: clear existing files
        for file in os.listdir(UPLOAD_FOLDER):
            os.remove(os.path.join(UPLOAD_FOLDER, file))
    else:
        os.makedirs(UPLOAD_FOLDER)
        
    print("Server started. Waiting for frames...")
    app.run(host='0.0.0.0', port=5000, threaded=True)