"""Generate current TOTP code from a base32 secret.

The secret is the broker's PERMANENT TOTP seed, not a one-time code — anyone
who reads it owns the second factor forever. So it must never appear in argv:
on Windows any process running as the same user can read another process's full
command line (Win32_Process.CommandLine via WMI, no elevation required), on
Linux via /proc/<pid>/cmdline, and crash dumps / EDR telemetry capture it too.

Input precedence (most to least secure):
  1. --stdin flag   — read one line from stdin. PREFERRED. Opt-in via the flag
                      because the caller must also close the write channel;
                      reading stdin unconditionally would hang whenever the
                      parent leaves the pipe open (which PythonRunner does).
  2. FINCEPT_TOTP_SECRET environment variable.
  3. sys.argv[1]    — DEPRECATED, kept only so an older caller keeps working.
                      Emits a warning on stderr when used.

Usage (preferred):
  echo <base32_secret> | totp_gen.py --stdin

Prints JSON: {"code": "123456", "valid_for": 15}
"""
import os
import sys
import json
import time


def read_secret():
    """Resolve the base32 secret from stdin, then env, then argv."""
    argv = sys.argv[1:]

    # 1. stdin — explicit opt-in. The caller must write one line and close the
    #    write channel; without the flag we must NOT touch stdin, because a
    #    parent that keeps the pipe open would make readline() block forever.
    if "--stdin" in argv:
        try:
            line = sys.stdin.readline()
        except Exception:
            line = ""
        return line.strip()

    # 2. Environment.
    env_secret = os.environ.get("FINCEPT_TOTP_SECRET", "").strip()
    if env_secret:
        return env_secret

    # 3. Legacy argv path.
    positional = [a for a in argv if not a.startswith("--")]
    if positional and positional[0].strip():
        print("WARNING: TOTP secret passed via argv — it is readable by any "
              "process running as this user. Pipe it on stdin with --stdin "
              "instead.", file=sys.stderr)
        return positional[0].strip()

    return ""


def main():
    secret = read_secret()
    if not secret:
        print(json.dumps({"error": "No secret provided"}))
        sys.exit(1)

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
