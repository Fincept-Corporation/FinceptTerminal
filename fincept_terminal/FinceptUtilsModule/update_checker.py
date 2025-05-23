import requests
import os
import sys
import subprocess
from tqdm import tqdm
import json
from packaging import version

# Constants for version checking
CURRENT_VERSION = "0.1.4"
CONFIG_URL = "https://product.fincept.in/update_config.json"
SETTINGS_FILE = "user_settings.json"


def load_settings():
    """Load user FinceptSettingModule from a JSON file."""
    default_settings = {
        "theme": "dark",
        "notifications": True,
        "display_rows": 10,
        "auto_update": False
    }
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as f:
            return json.load(f)
    return default_settings


def fetch_update_config():
    """Fetch the update configuration from the server."""
    try:
        response = requests.get(CONFIG_URL)
        response.raise_for_status()
        return response.json()
    except Exception as e:
        print(f"Failed to fetch update configuration: {e}")
        return None


def compare_versions(current_version, latest_version):
    """Compare two versions numerically."""
    return version.parse(latest_version) > version.parse(current_version)


def check_for_update():
    """Check if an update is available and prompt the user to update."""
    config = fetch_update_config()
    if not config:
        print("Failed to retrieve update configuration.")
        return

    latest_version = config.get("version")
    download_url = config.get("download_url")

    if latest_version and compare_versions(CURRENT_VERSION, latest_version):
        print(f"Update available: {latest_version} (Current: {CURRENT_VERSION})")
        user_input = input("Do you want to update to the latest version? (y/n): ").strip().lower()
        if user_input == 'y':
            download_and_update(latest_version, download_url)
    else:
        print(f"You are using the latest version: {CURRENT_VERSION}.")


def download_and_update(latest_version, download_url):
    """Download the update and show progress, then handle installation."""
    try:
        response = requests.get(download_url, stream=True)
        response.raise_for_status()

        total_size = int(response.headers.get('content-length', 0))  # Total file size in bytes
        update_file = os.path.join(os.getcwd(), download_url.split("/")[-1])  # Save with the original file name

        # Progress bar setup
        with open(update_file, "wb") as f, tqdm(
                desc="Downloading Update",
                total=total_size,
                unit='B',
                unit_scale=True,
                unit_divisor=1024,
        ) as bar:
            for chunk in response.iter_content(1024):
                if chunk:
                    f.write(chunk)
                    bar.update(len(chunk))

        print("Downloaded update successfully!")

        # Since we only deal with .exe, run the installer
        if update_file.endswith('.exe'):
            print(f"Running installer {update_file}...")
            run_installer(update_file)

    except Exception as e:
        print(f"Update failed: {e}")


def run_installer(installer_path):
    """Run the downloaded .exe installer and exit the current application."""
    try:
        # Start the .exe installer and close the current application
        subprocess.run([installer_path], check=True)
        print("Installer executed successfully. Exiting current application.")
        sys.exit(0)  # Exit the current application
    except subprocess.CalledProcessError as e:
        print(f"Failed to run the installer: {e}")


def auto_update_check():
    """Automatically check for updates if Auto-Update is enabled."""
    settings = load_settings()
    if settings.get("auto_update", False):
        print("Auto-Update is enabled. Checking for updates...")
        check_for_update()


# Example of how you might call auto_update_check in your main program
if __name__ == "__main__":
    auto_update_check()
