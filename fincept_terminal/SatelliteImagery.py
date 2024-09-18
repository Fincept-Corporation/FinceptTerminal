import requests
import boto3
import os

# Define the API URL and query parameters for New York City (adjusted bounding box)
url = "https://api.openaerialmap.org/meta?bbox=-74.00597,40.712776,-73.935242,40.730610"

# Send the request to OpenAerialMap
response = requests.get(url)

if response.status_code == 200:
    print("Aerial image metadata retrieved successfully!")
    metadata = response.json()

    # Check if any imagery is available in the response
    if metadata['results']:
        # Get the first result (you can loop through results if needed)
        first_result = metadata['results'][0]

        # Extract the URL for the image (this could be an S3 link)
        image_url = first_result['properties']['url']

        if image_url.startswith('s3://'):
            # Parse the S3 URL to get the bucket and key
            s3_parts = image_url.replace("s3://", "").split('/')

            bucket_name = s3_parts[0]
            key = '/'.join(s3_parts[1:])

            # Use boto3 to download the image from S3
            s3 = boto3.client('s3', config=boto3.session.Config(signature_version='unsigned'))
            local_filename = 'nyc_aerial_image.tif'
            try:
                s3.download_file(bucket_name, key, local_filename)
                print(f"Image downloaded and saved successfully as '{local_filename}'!")
            except Exception as e:
                print(f"Error downloading the image from S3: {e}")
        else:
            # If the URL is not an S3 link, handle it with requests
            image_response = requests.get(image_url)

            if image_response.status_code == 200:
                with open('nyc_aerial_image.jpg', 'wb') as f:
                    f.write(image_response.content)
                print("Image downloaded and saved successfully as 'nyc_aerial_image.jpg'!")
            else:
                print(f"Error downloading the image: {image_response.status_code}")
    else:
        print("No imagery available for the specified area.")
else:
    print(f"Error: {response.status_code}")
