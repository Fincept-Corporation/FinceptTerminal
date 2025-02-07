import json
import os

# 🔹 Define a persistent settings directory in the user's home folder
BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # Current file's directory
SETTINGS_DIR = os.path.join(BASE_DIR, "settings")  # Directory for settings files
SETTINGS_FILE = os.path.join(SETTINGS_DIR, "settings.json")  # Path to settings.json

# 🔹 Default settings template (Modify as needed)
DEFAULT_SETTINGS = {
    "username": "",
    "theme": "light",
    "notifications": True
}

def ensure_settings_file():
    """
    Ensures that the settings directory and file exist.
    If the file does not exist, it creates one with default settings.
    """

    # ✅ Ensure the settings directory exists
    if not os.path.exists(SETTINGS_DIR):
        os.makedirs(SETTINGS_DIR, exist_ok=True)
        print(f"📁 Created settings directory: {SETTINGS_DIR}")

    # ✅ Create settings file if missing or invalid
    if not os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
            json.dump(DEFAULT_SETTINGS, f, indent=4)
        print(f"✅ Settings file created: {SETTINGS_FILE}")
    else:
        # ✅ Validate the existing settings file
        try:
            with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                json.load(f)  # Try loading JSON to ensure it's valid
        except json.JSONDecodeError:
            print(f"⚠ Corrupt settings file detected. Resetting to defaults.")
            with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
                json.dump(DEFAULT_SETTINGS, f, indent=4)
            print(f"✅ Settings file reset: {SETTINGS_FILE}")

# 🔹 Call the function to ensure settings exist before use
ensure_settings_file()

def load_settings():
    """
    Load settings from the settings.json file.
    If the file does not exist, create it with default settings.
    """
    ensure_settings_file()  # Ensure settings exist before loading

    with open(SETTINGS_FILE, "r", encoding="utf-8") as file:
        return json.load(file)

def save_user_data(data):
    """
    Save user data to the settings.json file.
    """
    settings = load_settings()  # Load existing settings
    settings.update(data)  # Update settings with new data

    with open(SETTINGS_FILE, "w", encoding="utf-8") as file:
        json.dump(settings, file, indent=4)  # Save in pretty format

def is_registered():
    """
    Check if a user is registered by verifying the presence of 'username' in settings.
    """
    settings = load_settings()
    return bool(settings.get("username"))

def get_user_name():
    """
    Get the registered user's name, or return 'Guest' if no user is registered.
    """
    settings = load_settings()
    return settings.get("username", "Guest")

def clear_user_data():
    """
    Clear all user data by resetting settings.json to default values.
    """
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "w", encoding="utf-8") as file:
            json.dump(DEFAULT_SETTINGS, file, indent=4)  # Reset to default settings
        print(f"🗑 User data cleared. Settings reset to defaults: {SETTINGS_FILE}")