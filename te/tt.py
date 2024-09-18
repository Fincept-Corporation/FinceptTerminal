import pandas as pd
from fredapi import Fred
import os
import time  # Import the time module to introduce a delay

# Initialize FRED API with your API key
fred = Fred(api_key='186968c7ec98984b5d08393e93aedf2b')

# Create a directory to store the CSV files
output_dir = 'fred_category_data'
if not os.path.exists(output_dir):
    os.makedirs(output_dir)

# Start looping from category_id=1, and you can set a max value (e.g., 50000)
start_id = 39305

## DONE 32991-39304
end_id = 50000  # You can adjust this value based on how many categories you want to check

for category_id in range(start_id, end_id + 1):
    try:
        # Fetch data for the category
        data = fred.search_by_category(category_id=category_id)

        # Convert the data to a DataFrame
        df = pd.DataFrame(data)

        # Check if DataFrame is not empty
        if not df.empty:
            # Save the DataFrame to CSV
            output_file = os.path.join(output_dir, f'category_{category_id}.csv')
            df.to_csv(output_file, index=False)
            print(f"Data for category {category_id} saved to {output_file}")
            time.sleep(5)
        else:
            print(f"No data for category {category_id}, skipping...")

        # Add a 1-second delay between API requests
        time.sleep(5)

    except Exception as e:
        print(f"Error fetching data for category {category_id}: {e}")
