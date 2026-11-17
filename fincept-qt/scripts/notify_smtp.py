"""Send one plain-text e-mail over SMTP for the Email notification provider
(src/services/notifications/providers/EmailProvider.cpp, run via PythonRunner).

Input: one JSON object on stdin -- never argv, it carries the SMTP password:
    {"host", "port", "user", "password", "from", "to", "subject", "body"}
"to" may list several addresses separated by commas or semicolons.

Transport: port 465 uses implicit TLS (SMTP_SSL); any other port is plain SMTP
upgraded with STARTTLS. If the server offers no STARTTLS, the password is only
sent to a server on this machine (127.0.0.1 / ::1 / localhost).

Output: one JSON line on stdout -- {"success": true} or {"error": "<reason>"}.
Standard library only.
"""

import json
import smtplib
import socket
import ssl
import sys
from email.message import EmailMessage
from email.utils import formatdate, make_msgid

_TIMEOUT_S = 30
_LOOPBACK_HOSTS = {"127.0.0.1", "::1", "localhost"}


def _reply(payload, code):
    print(json.dumps(payload))
    return code


def _fail(message):
    return _reply({"error": message}, 1)


def _smtp_text(exc):
    """Server wording of an SMTP response error, decoded."""
    raw = getattr(exc, "smtp_error", b"")
    if isinstance(raw, bytes):
        raw = raw.decode("utf-8", "replace")
    text = str(raw).strip()
    code = getattr(exc, "smtp_code", None)
    if code and text:
        return "%s %s" % (code, text)
    return text or str(exc)


def main():
    try:
        cfg = json.loads(sys.stdin.read() or "{}")
    except ValueError:
        return _fail("Invalid request")
    if not isinstance(cfg, dict):
        return _fail("Invalid request")

    host = str(cfg.get("host") or "").strip()
    port_text = str(cfg.get("port") or "").strip() or "587"
    user = str(cfg.get("user") or "").strip()
    password = str(cfg.get("password") or "")
    sender = str(cfg.get("from") or "").strip() or user
    recipients = [a.strip() for a in str(cfg.get("to") or "").replace(";", ",").split(",") if a.strip()]

    if not host:
        return _fail("SMTP host is not set")
    try:
        port = int(port_text)
    except ValueError:
        return _fail("SMTP port must be a number")
    if not 1 <= port <= 65535:
        return _fail("SMTP port must be between 1 and 65535")
    if not recipients:
        return _fail("To address is not set")
    if not sender:
        return _fail("Set a From address or a username")

    msg = EmailMessage()
    msg["Subject"] = str(cfg.get("subject") or "")
    msg["From"] = sender
    msg["To"] = ", ".join(recipients)
    msg["Date"] = formatdate(localtime=True)
    msg["Message-ID"] = make_msgid()
    msg.set_content(str(cfg.get("body") or ""))

    context = ssl.create_default_context()
    try:
        if port == 465:
            server = smtplib.SMTP_SSL(host, port, timeout=_TIMEOUT_S, context=context)
        else:
            server = smtplib.SMTP(host, port, timeout=_TIMEOUT_S)
        with server:
            server.ehlo()
            if port != 465:
                if server.has_extn("starttls"):
                    server.starttls(context=context)
                    server.ehlo()
                elif user and host.lower() not in _LOOPBACK_HOSTS:
                    return _fail("The server does not offer STARTTLS, so the password would travel "
                                 "unencrypted. Use port 465 (SSL) or a server with STARTTLS.")
            if user:
                server.login(user, password)
            refused = server.send_message(msg, from_addr=sender, to_addrs=recipients)
    except smtplib.SMTPAuthenticationError as exc:
        return _fail("SMTP login failed: " + _smtp_text(exc))
    except smtplib.SMTPRecipientsRefused as exc:
        return _fail("Recipient refused: " + ", ".join(sorted(exc.recipients)))
    except smtplib.SMTPSenderRefused as exc:
        return _fail("Sender refused: " + _smtp_text(exc))
    except smtplib.SMTPResponseException as exc:
        return _fail("SMTP error: " + _smtp_text(exc))
    except smtplib.SMTPException as exc:
        return _fail("SMTP error: " + str(exc))
    except ssl.SSLError as exc:
        return _fail("TLS error: " + (exc.reason or str(exc)))
    except (socket.timeout, TimeoutError):
        return _fail("Timed out talking to %s:%d" % (host, port))
    except OSError as exc:
        return _fail("Could not connect to %s:%d: %s" % (host, port, exc.strerror or exc))

    if refused:
        return _fail("Recipient refused: " + ", ".join(sorted(refused)))
    return _reply({"success": True}, 0)


if __name__ == "__main__":
    sys.exit(main())
