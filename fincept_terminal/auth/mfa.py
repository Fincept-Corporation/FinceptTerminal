import requests

BASE_URL = "https://finceptapi.share.zrok.io"  # Replace with your API URL

def stest_mfa_secondary():
    email = "testuser@example.com"
    secondary_email = "testuser_secondary@example.com"
    password = "TestPass123!"

    # 1️⃣ Add Secondary Email
    r = requests.post(f"{BASE_URL}/add-secondary-email", json={"secondary_email": secondary_email})
    print("Add Secondary Email:", r.status_code, r.json())

    otp = input("Enter the OTP received on secondary email: ")

    # 2️⃣ Verify Secondary Email
    r = requests.post(f"{BASE_URL}/verify-secondary-email", json={"email": secondary_email, "otp": int(otp)})
    print("Verify Secondary Email:", r.status_code, r.json())

    # 3️⃣ Set MFA to use Secondary Email
    r = requests.post(f"{BASE_URL}/set-mfa-email", json={"email_type": "secondary"})
    print("Set MFA Email:", r.status_code, r.json())

    # 4️⃣ Login and Trigger MFA
    r = requests.post(f"{BASE_URL}/login", json={"email": email, "password": password})
    print("Login:", r.status_code, r.json())

if __name__ == "__main__":
    stest_mfa_secondary()
