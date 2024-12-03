import os

from openai import AzureOpenAI
from dotenv import load_dotenv

load_dotenv()


api_base = os.getenv("AZURE_OPENAI_ENDPOINT")
api_key= os.getenv("AZURE_OPENAI_API_KEY")
deployment_name = 'NewOmni'
api_version = '2023-12-01-preview' # this might change in the future

client = AzureOpenAI(
    api_key=api_key,  
    api_version=api_version,
    base_url=f"{api_base}/openai/deployments/{deployment_name}"
)

response = client.chat.completions.create(
    model=deployment_name,
    messages=[
        { "role": "system", "content": "You are a helpful and truthful assistant" },
        { "role": "user", "content": [  
            { 
                "type": "text", 
                "text": "Answer this question for me. Do chain of thought to explain your thought process" 
            },
            { 
                "type": "image_url",
                "image_url": {
                    "url": "https://i.ytimg.com/vi/CBDVKCywCYY/hq720.jpg?sqp=-oaymwEhCK4FEIIDSFryq4qpAxMIARUAAAAAGAElAADIQj0AgKJD&rs=AOn4CLCeDlUOwuYzqlitBja3-QwpTDZzpg"
                }
            }
        ] } 
    ],
    max_tokens=2000 
)

import pdb; pdb.set_trace()
print(response)