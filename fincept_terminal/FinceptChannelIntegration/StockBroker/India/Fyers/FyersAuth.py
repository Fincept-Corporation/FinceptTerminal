import base64
import hmac
import os
import struct
import time
from urllib.parse import urlparse, parse_qs
import requests
from fyers_apiv3 import fyersModel

totp_key = ""
username = ""
pin = 1234
client_id = ""
secret_key = ""
redirect_uri = "https://google.com"

def read_file():
    try:
        with open("access_token.log", "r") as f:
            token_line = f.readline().strip()
            if token_line.startswith("Access Token:"):
                token = token_line.split("Access Token:")[1].strip()
                return token
        return None
    except FileNotFoundError:
        return None

def write_file(token):
    with open("access_token.log", "w") as f:
        f.write(f"Access Token: {token}")

def totp(key, time_step=30, digits=6, digest="sha1"):
    key = base64.b32decode(key.upper() + "=" * ((8 - len(key)) % 8))
    counter = struct.pack(">Q", int(time.time() / time_step))
    mac = hmac.new(key, counter, digest).digest()
    offset = mac[-1] & 0x0F
    binary = struct.unpack(">L", mac[offset : offset + 4])[0] & 0x7FFFFFFF
    return str(binary)[-digits:].zfill(digits)

def get_token():
    headers = {
        "Accept": "application/json",
        "Accept-Language": "en-US,en;q=0.9",
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/112.0.0.0 Safari/537.36",
    }

    s = requests.Session()
    s.headers.update(headers)

    data1 = f'{{"fy_id":"{base64.b64encode(f"{username}".encode()).decode()}","app_id":"2"}}'
    r1 = s.post("https://api-t2.fyers.in/vagator/v2/send_login_otp_v2", data=data1)

    request_key = r1.json()["request_key"]
    data2 = f'{{"request_key":"{request_key}","otp":{totp(totp_key)}}}'
    r2 = s.post("https://api-t2.fyers.in/vagator/v2/verify_otp", data=data2)
    assert r2.status_code == 200, f"Error in r2:\n {r2.text}"

    request_key = r2.json()["request_key"]
    data3 = f'{{"request_key":"{request_key}","identity_type":"pin","identifier":"{base64.b64encode(f"{pin}".encode()).decode()}"}}'
    r3 = s.post("https://api-t2.fyers.in/vagator/v2/verify_pin_v2", data=data3)
    assert r3.status_code == 200, f"Error in r3:\n {r3.json()}"

    headers = {"authorization": f"Bearer {r3.json()['data']['access_token']}", "content-type": "application/json; charset=UTF-8"}
    data4 = f'{{"fyers_id":"{username}","app_id":"{client_id[:-4]}","redirect_uri":"{redirect_uri}","appType":"100","code_challenge":"","state":"abcdefg","scope":"","nonce":"","response_type":"code","create_cookie":true}}'
    r4 = s.post("https://api.fyers.in/api/v2/token", headers=headers, data=data4)
    assert r4.status_code == 308, f"Error in r4:\n {r4.json()}"

    parsed = urlparse(r4.json()["Url"])
    auth_code = parse_qs(parsed.query)["auth_code"][0]

    session = fyersModel.SessionModel(client_id=client_id, secret_key=secret_key, redirect_uri=redirect_uri, response_type="code", grant_type="authorization_code")
    session.set_token(auth_code)
    response = session.generate_token()
    return response["access_token"]

def get_profile(token):
    fyers = fyersModel.FyersModel(client_id=client_id, token=token, log_path=os.getcwd())
    return fyers.get_profile()

def main():
    token = read_file()
    if token is None:
        token = get_token()

    resp = get_profile(token)

    if "error" in resp["s"] or "error" in resp["message"] or "expired" in resp["message"]:
        token = get_token()
        resp = get_profile(token)

    write_file(token)
    print("Fyers access token is saved in `access_token.log` file.")
    print(resp)

if __name__ == "__main__":
    main()
