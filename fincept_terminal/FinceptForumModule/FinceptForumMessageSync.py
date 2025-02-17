import requests
import json

# Set the base URL of your API (adjust if needed)
BASE_URL = "https://finceptapi.share.zrok.io/"

# Replace with your valid API key for testing
API_KEY = "33f51a75-c36b-4003-ac71-8a75351b35f6"

# List of allowed channels (8 endpoints)
channels = [
    "global",
    "africa",
    "antarctica",
    "asia",
    "europe",
    "north_america",
    "oceania",
    "south_america"
]

headers = {
    "X-API-Key": API_KEY,
    "Content-Type": "application/json"
}


def send_message(channel, message_text):
    url = f"{BASE_URL}/chat/{channel}/message"
    payload = {"message": message_text}

    response = requests.post(url, headers=headers, data=json.dumps(payload))
    try:
        resp_json = response.json()
    except Exception as e:
        resp_json = {"error": str(e)}

    print(f"Channel: {channel}")
    print("Status Code:", response.status_code)
    print("Response:", json.dumps(resp_json, indent=2))
    print("-" * 50)


def get_chat_messages(channel):
    """Fetch chat messages for a specific forum channel."""
    url = f"{BASE_URL}/chat/{channel}/messages"  # ✅ Fetch messages from a specific channel

    try:
        response = requests.get(url, headers=headers)

        print(f"GET /chat/{channel}/messages")
        print("Status Code:", response.status_code)

        if response.status_code == 200:
            messages = response.json()
            print("Response:", json.dumps(messages, indent=2))
        else:
            print("⚠ Failed to fetch messages. Error:", response.text)

        print("-" * 50)

    except requests.RequestException as e:
        print(f"❌ Error fetching messages for {channel}: {e}")


if __name__ == "__main__":
    get_chat_messages("global")
    # for channel in channels:
    #
    #     message = f"This is a test message for the '{channel}' chat channel."
    #     send_message(channel, message)

# def get_chat_messages():
#     url = f"{BASE_URL}/chat/messages"
#     response = requests.get(url)
#
#     print("GET /chat/messages")
#     print("Status Code:", response.status_code)
#     print("Response:", json.dumps(response.json(), indent=2))
#     print("-" * 50)
