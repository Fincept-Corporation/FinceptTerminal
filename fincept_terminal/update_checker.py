# fincept_terminal/update_checker.py
import requests
import os
import zipfile
import sys

# Constants for version checking
CURRENT_VERSION = "1.0.2"
VERSION_URL = "https://product.fincept.in/version.txt"
UPDATE_URL = "https://product.fincept.in/fincept.exe"

def check_for_update():
    """Check if an update is available and prompt the user to update."""
    try:
        response = requests.get(VERSION_URL)
        response.raise_for_status()

        latest_version = response.text.strip()
        if latest_version > CURRENT_VERSION:
            print(f"Update available: {latest_version} (Current: {CURRENT_VERSION})")
            user_input = input("Do you want to update to the latest version? (y/n): ").strip().lower()
            if user_input == 'y':
                download_and_update(latest_version)
        else:
            print(f"You are using the latest version: {CURRENT_VERSION}.")
    except Exception as e:
        print(f"Failed to check for updates: {e}")

def download_and_update(latest_version):
    """Download the update and replace the current files."""
    try:
        response = requests.get(UPDATE_URL, stream=True)
        response.raise_for_status()

        update_file = "latest_build.zip"
        with open(update_file, "wb") as f:
            for chunk in response.iter_content(1024):
                if chunk:
                    f.write(chunk)

        print("Downloaded update successfully!")
        with zipfile.ZipFile(update_file, 'r') as zip_ref:
            zip_ref.extractall(os.path.dirname(sys.argv[0]))  # Extract into current directory

        print(f"Updated to version {latest_version}. Restarting...")
        restart_application()
    except Exception as e:
        print(f"Update failed: {e}")

def restart_application():
    """Restart the application after an update."""
    os.execv(sys.executable, ['python'] + sys.argv)
