#!/usr/bin/env python3
"""fincept_mcp_stdio.py - stdio <-> Streamable-HTTP bridge for Fincept Terminal's MCP server.

Lets MCP clients that only speak stdio (Claude Desktop, some IDE plugins) use the
MCP server built into a running Fincept Terminal:

    python fincept_mcp_stdio.py --url http://127.0.0.1:41720/mcp

The bearer token comes from the FINCEPT_MCP_TOKEN environment variable, or from
--token-file. Both are shown in Fincept Terminal -> Agent Workbench -> Connect.

Protocol: newline-delimited JSON-RPC messages (or batches) are read from stdin,
each is POSTed to the terminal, and every JSON-RPC message that comes back is
written to stdout as one line. Notifications get no reply (HTTP 202). When the
terminal cannot be reached, every request in the message is answered with a
JSON-RPC error (-32000) right away, so the client never hangs. Requests are
forwarded concurrently (a slow tool call does not block a ping). Diagnostics go
to stderr only: stdout carries protocol messages and nothing else.

Standard library only; Python 3.8+.
"""

import argparse
import http.client
import json
import os
import socket
import sys
import threading
import time
import urllib.error
import urllib.request

VERSION = "1.0"
DEFAULT_URL = "http://127.0.0.1:41720/mcp"
TOKEN_ENV = "FINCEPT_MCP_TOKEN"
URL_ENV = "FINCEPT_MCP_URL"
MAX_IN_FLIGHT = 8          # concurrent requests to the terminal
EOF_GRACE_SECONDS = 3.0    # after stdin closes, let in-flight replies land
SERVER_ERROR = -32000
INTERNAL_ERROR = -32603


def log(message):
    sys.stderr.write("[fincept-mcp-stdio] %s\n" % message)
    sys.stderr.flush()


def ascii_header_value(text, limit=64):
    """http.client encodes header values as latin-1; keep client names plain ASCII."""
    cleaned = "".join(ch for ch in text if 32 <= ord(ch) < 127)
    return cleaned.strip()[:limit]


class Bridge(object):
    def __init__(self, url, token, timeout, verbose):
        self.url = url
        self.token = token
        self.timeout = timeout
        self.verbose = verbose
        self.protocol_version = None
        self.client_name = None
        self._out_lock = threading.Lock()
        self._state_lock = threading.Lock()
        self._slots = threading.BoundedSemaphore(MAX_IN_FLIGHT)
        self._threads = []
        # Never send loopback traffic (and the bearer token) through a proxy
        # picked up from HTTP_PROXY / system settings.
        self._opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    # -- output ---------------------------------------------------------------

    def debug(self, message):
        if self.verbose:
            log(message)

    def emit(self, message):
        line = json.dumps(message, ensure_ascii=False, separators=(",", ":"))
        data = (line + "\n").encode("utf-8")
        with self._out_lock:
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()

    @staticmethod
    def request_ids(message):
        """ids of the requests (not notifications / responses) in a message or batch."""
        items = message if isinstance(message, list) else [message]
        return [m["id"] for m in items if isinstance(m, dict) and "method" in m and "id" in m]

    def fail(self, ids, text, code=SERVER_ERROR):
        for request_id in ids:
            self.emit({"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": text}})

    # -- protocol bookkeeping -------------------------------------------------

    def note_outgoing(self, message):
        """Remember the client's name (from initialize) for the User-Agent."""
        items = message if isinstance(message, list) else [message]
        for m in items:
            if isinstance(m, dict) and m.get("method") == "initialize":
                info = (m.get("params") or {}).get("clientInfo") or {}
                name = info.get("name")
                if isinstance(name, str) and name.strip():
                    with self._state_lock:
                        self.client_name = ascii_header_value(name)

    def note_incoming(self, message):
        """Remember the negotiated protocol version (sent as MCP-Protocol-Version)."""
        items = message if isinstance(message, list) else [message]
        for m in items:
            result = m.get("result") if isinstance(m, dict) else None
            if isinstance(result, dict) and "serverInfo" in result:
                version = result.get("protocolVersion")
                if isinstance(version, str) and version:
                    with self._state_lock:
                        self.protocol_version = ascii_header_value(version)

    def headers(self):
        with self._state_lock:
            client, protocol = self.client_name, self.protocol_version
        h = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "User-Agent": "fincept-mcp-stdio/%s%s" % (VERSION, " (%s)" % client if client else ""),
        }
        if self.token:
            h["Authorization"] = "Bearer " + self.token
        if protocol:
            h["MCP-Protocol-Version"] = protocol
        return h

    # -- HTTP -----------------------------------------------------------------

    @staticmethod
    def parse_sse(data):
        """JSON-RPC messages from a text/event-stream body (data: lines, blank-line separated)."""
        messages, chunk = [], []

        def flush():
            if chunk:
                try:
                    messages.append(json.loads("\n".join(chunk)))
                except ValueError:
                    pass
                del chunk[:]

        for line in data.decode("utf-8", "replace").splitlines():
            if not line:
                flush()
                continue
            if line.startswith(":"):
                continue  # SSE comment / keep-alive
            field, _, value = line.partition(":")
            if value.startswith(" "):
                value = value[1:]
            if field == "data":
                chunk.append(value)
        flush()
        return messages

    def deliver(self, data, content_type, ids):
        if "text/event-stream" in content_type.lower():
            messages = self.parse_sse(data)
        else:
            try:
                messages = [json.loads(data.decode("utf-8"))]
            except (UnicodeDecodeError, ValueError) as exc:
                self.fail(ids, "Fincept Terminal returned an unreadable response: %s" % exc)
                return
        for m in messages:
            self.note_incoming(m)
            self.emit(m)
        if not messages and ids:
            self.fail(ids, "Fincept Terminal returned an empty response")

    def http_failure(self, status, data, ids):
        try:
            payload = json.loads(data.decode("utf-8")) if data else None
        except (UnicodeDecodeError, ValueError):
            payload = None
        # A JSON-RPC answer that arrived with an error status (e.g. a 400 for an
        # empty batch) is still the server's answer: pass it through when it
        # answers this message. Otherwise every request gets a synthesized error.
        if isinstance(payload, list):
            self.emit(payload)
            return
        if isinstance(payload, dict) and payload.get("jsonrpc") == "2.0":
            answered_id = payload.get("id")
            if answered_id in ids or (answered_id is None and not ids):
                self.emit(payload)
                return
        if status == 401:
            text = ("Fincept Terminal rejected the MCP token (HTTP 401). Copy the current token from Fincept "
                    "Terminal -> Agent Workbench -> Connect into %s." % TOKEN_ENV)
        elif status == 403:
            text = "Fincept Terminal refused the request (HTTP 403, origin policy)."
        else:
            detail = ""
            if isinstance(payload, dict):
                detail = str(payload.get("message") or payload.get("error") or "")
            text = "Fincept Terminal returned HTTP %d%s" % (status, (": " + detail[:300]) if detail else "")
        log(text)
        self.fail(ids, text)

    def forward(self, raw_line, message):
        ids = self.request_ids(message)
        self.note_outgoing(message)
        request = urllib.request.Request(self.url, data=raw_line.encode("utf-8"), headers=self.headers(),
                                         method="POST")
        started = time.monotonic()
        try:
            with self._opener.open(request, timeout=self.timeout) as response:
                status = response.status
                content_type = response.headers.get("Content-Type", "") or ""
                data = response.read()
        except urllib.error.HTTPError as exc:
            try:
                data = exc.read()
            except Exception:  # noqa: BLE001 - the body is best-effort context only
                data = b""
            self.http_failure(exc.code, data, ids)
            return
        except (socket.timeout, TimeoutError) as exc:
            self.fail(ids, "Fincept Terminal did not answer within %d s: %s" % (int(self.timeout), exc))
            return
        except (urllib.error.URLError, http.client.HTTPException, OSError, ValueError) as exc:
            reason = getattr(exc, "reason", None) or exc
            log("cannot reach %s: %s" % (self.url, reason))
            self.fail(ids, "Fincept Terminal is not running or its MCP server is disabled: %s" % reason)
            return
        self.debug("HTTP %d in %.0f ms" % (status, (time.monotonic() - started) * 1000.0))
        if status == 202 or not data.strip():
            if ids:  # a request must be answered; an empty 200 is a server bug
                self.fail(ids, "Fincept Terminal returned no response (HTTP %d)" % status)
            return
        self.deliver(data, content_type, ids)

    # -- main loop ------------------------------------------------------------

    def _worker(self, raw_line, message):
        try:
            self.forward(raw_line, message)
        except Exception as exc:  # noqa: BLE001 - never leave the client waiting
            log("internal error: %r" % (exc,))
            self.fail(self.request_ids(message), "fincept-mcp-stdio internal error: %s" % exc, INTERNAL_ERROR)
        finally:
            self._slots.release()

    def run(self):
        stdin = sys.stdin.buffer
        while True:
            raw = stdin.readline()
            if not raw:
                break  # EOF: the client is gone
            line = raw.decode("utf-8", "replace").strip()
            if not line:
                continue
            try:
                message = json.loads(line)
            except ValueError as exc:
                self.emit({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "Parse error: %s" % exc}})
                continue
            if not isinstance(message, (dict, list)):
                self.emit({"jsonrpc": "2.0", "id": None,
                           "error": {"code": -32600, "message": "Invalid Request: expected an object or array"}})
                continue
            self._slots.acquire()
            worker = threading.Thread(target=self._worker, args=(line, message), daemon=True)
            worker.start()
            self._threads = [t for t in self._threads if t.is_alive()]
            self._threads.append(worker)

        deadline = time.monotonic() + EOF_GRACE_SECONDS
        for worker in self._threads:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            worker.join(remaining)
        return 0


def read_token(token_file):
    if token_file:
        try:
            with open(token_file, "r", encoding="utf-8") as handle:
                return handle.read().strip()
        except OSError as exc:
            log("cannot read --token-file: %s" % exc)
            return ""
    return os.environ.get(TOKEN_ENV, "").strip()


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Bridge a stdio MCP client to Fincept Terminal's MCP server (Streamable HTTP).")
    parser.add_argument("--url", default=os.environ.get(URL_ENV, DEFAULT_URL),
                        help="server endpoint (default: %s)" % DEFAULT_URL)
    parser.add_argument("--token-file", help="file holding the bearer token (default: $%s)" % TOKEN_ENV)
    parser.add_argument("--timeout", type=float, default=600.0, help="per-request timeout in seconds (default 600)")
    parser.add_argument("--verbose", action="store_true", help="log every exchange to stderr")
    args = parser.parse_args(argv)

    token = read_token(args.token_file)
    if not token:
        log("no token: set %s or pass --token-file (the terminal will answer 401)" % TOKEN_ENV)
    bridge = Bridge(args.url, token, max(1.0, args.timeout), args.verbose)
    try:
        return bridge.run()
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
