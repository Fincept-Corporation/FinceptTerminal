"""Generate current TOTP code from a base32 secret.
Usage: totp_gen.py <base32_secret>
Prints JSON: {"code": "123456", "valid_for": 15}
"""
import sys, json, time

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "No secret provided"}))
        sys.exit(1)

    secret = sys.argv[1].strip()
    try:
        import pyotp
        totp = pyotp.TOTP(secret)
        code = totp.now()
        # Seconds remaining in this 30s window
        valid_for = 30 - (int(time.time()) % 30)
        print(json.dumps({"code": code, "valid_for": valid_for}))
    except ImportError:
        # Fallback: implement TOTP manually (RFC 6238)
        import hmac, hashlib, base64, struct
        key = base64.b32decode(secret.upper() + '=' * (-len(secret) % 8))
        t = int(time.time()) // 30
        msg = struct.pack('>Q', t)
        h = hmac.new(key, msg, hashlib.sha1).digest()
        offset = h[-1] & 0x0f
        code = str((struct.unpack('>I', h[offset:offset+4])[0] & 0x7fffffff) % 1000000).zfill(6)
        valid_for = 30 - (int(time.time()) % 30)
        print(json.dumps({"code": code, "valid_for": valid_for}))

if __name__ == "__main__":
    main()
