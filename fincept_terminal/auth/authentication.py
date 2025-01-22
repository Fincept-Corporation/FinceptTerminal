import requests
from datetime import datetime
from rich.prompt import Prompt

# Base URL of your hosted API
BASE_URL = "https://finceptapi.share.zrok.io"

# Endpoints
REGISTER_ENDPOINT = f"{BASE_URL}/register"
VERIFY_OTP_ENDPOINT = f"{BASE_URL}/verify-otp"
DATABASES_ENDPOINT = f"{BASE_URL}/databases"
SUBSCRIBE_ENDPOINT = f"{BASE_URL}/subscribe/IndiaPriceData"
TABLES_ENDPOINT = f"{BASE_URL}/IndiaPriceData/tables"

# Generate unique username
timestamp = datetime.now().strftime("%Y%m%d%H%M%S")

SAMPLE_OTP = 111111  # Fixed OTP for testing
SAMPLE_USER = {
    "username": f"testuser_{timestamp}",
    "email": f"testuser_{timestamp}@example.com",
    "password": "securepassword123"
}

def register_user(username, email, password):
    """Register a new user with the given credentials."""
    # Create the payload for registration
    user_data = {
        "username": username,
        "email": email,
        "password": password
    }

    print(f"Registering user: {username}")

    # Send registration request
    response = requests.post(REGISTER_ENDPOINT, json=user_data)

    print("Register Response Status Code:", response.status_code)
    print("Register Response Text:", response.text)

    try:
        return response.json()
    except requests.exceptions.JSONDecodeError:
        print("Failed to parse JSON response")
        return {}


def verify_otp(email):
    """Verify the OTP for the given email."""
    # Prompt user for the OTP
    # otp = Prompt.ask("Enter the OTP sent to your email")

    # Construct the payload
    payload = {"email": email, "otp": SAMPLE_OTP}

    # Send OTP verification request
    response = requests.post(VERIFY_OTP_ENDPOINT, json=payload)

    print("Verify OTP Response Status Code:", response.status_code)
    print("Verify OTP Response Text:", response.text)

    try:
        return response.json()
    except requests.exceptions.JSONDecodeError:
        print("Failed to parse JSON response")
        return {}


def get_databases(api_key):
    """Test fetching list of databases."""
    headers = {"X-API-Key": api_key}
    response = requests.get(DATABASES_ENDPOINT, headers=headers)
    print("Databases Response Status Code:", response.status_code)
    try:
        print("Databases Response:", response.json())
    except requests.exceptions.JSONDecodeError:
        print("Failed to parse JSON response")


def subscribe_database(api_key):
    """Test subscribing to a database."""
    headers = {"X-API-Key": api_key}
    response = requests.post(SUBSCRIBE_ENDPOINT, headers=headers)
    print("Subscribe Response Status Code:", response.status_code)
    print("Subscribe Response Text:", response.text)
    try:
        return response.json()
    except requests.exceptions.JSONDecodeError:
        print("Failed to parse JSON response")
        return {}


def get_tables(api_key):
    """Test fetching list of tables in a subscribed database."""
    headers = {"X-API-Key": api_key}
    response = requests.get(TABLES_ENDPOINT, headers=headers)
    print("Tables Response Status Code:", response.status_code)
    try:
        print("Tables Response:", response.json())
    except requests.exceptions.JSONDecodeError:
        print("Failed to parse JSON response")


def run_ip_limiting_test(ip, headers=None):
    """
    Simulates requests to test IP-based rate limits.
    """
    print(f"\nTesting rate limiting for IP: {ip}")
    for i in range(12):  # Test with 12 requests to exceed both limits
        response = requests.get(DATABASES_ENDPOINT, headers=headers)
        print(f"Request {i + 1} - Status Code: {response.status_code}")
        if response.status_code == 429:
            print("Rate limit exceeded!")
            break


if __name__ == "__main__":
    # Step 1: Register a user
    registration_data = register_user()
    if "api_key" in registration_data:
        api_key = registration_data["api_key"]
        print(f"API Key: {api_key}")

        # Step 2: Verify OTP
        verify_otp_response = verify_otp()
        if verify_otp_response.get("message") == "Email verified successfully!":
            print("OTP verification successful.")

            # Step 3: Get list of databases
            get_databases(api_key)

            # Step 4: Subscribe to a database
            subscribe_response = subscribe_database(api_key)
            if subscribe_response.get("message"):
                print("Subscription successful.")

                # Step 5: Fetch tables in a subscribed database
                get_tables(api_key)

            # Step 6: Test fetching tables in a non-subscribed database
            print("\nTesting non-subscribed database access...")
            headers = {"X-API-Key": api_key}
            response = requests.get(f"{BASE_URL}/metaData/tables", headers=headers)
            print("Non-Subscribed Database Response Status Code:", response.status_code)
            print("Non-Subscribed Database Response:", response.json())

            # Step 7: Test rate limits for IP
            run_ip_limiting_test("unregistered_ip")  # Unregistered IP test
            run_ip_limiting_test("registered_ip", headers=headers)  # Registered IP test
        else:
            print("OTP verification failed.")
    else:
        print("Registration failed. Cannot proceed with tests.")
