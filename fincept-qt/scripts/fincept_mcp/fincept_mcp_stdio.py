#!/usr/bin/env python3
"""fincept_mcp_stdio.py - stdio <-> Streamable-HTTP bridge for Fincept Terminal's MCP server.

Lets MCP clients that only speak stdio (Claude Desktop, some IDE plugins) use the
MCP server built into a running Fincept Terminal:

    python fincept_mcp_stdio.py --url http://127.0.0.1:41720/mcp

The bearer token comes from the FINCEPT_COMMUNITY_MCP_TOKEN environment variable,
or from --token-file. Both are shown in Fincept Terminal -> Settings -> AI Agents
(MCP). The variable is edition-specific on purpose: the separate private build of
Fincept Terminal sets its own token in a different variable, which this bridge
never reads. Before it sends the token anywhere, the bridge asks the server
(GET /health, no token) which edition it is, and refuses to talk to any other.

Protocol: newline-delimited JSON-RPC messages (or batches) are read from stdin,
each is POSTed to the terminal, and every JSON-RPC message that comes back is
written to stdout as one line - including notifications/progress streamed while
a long tool call runs. Notifications get no reply (HTTP 202) and are forwarded
at once, never queued behind running calls, so a notifications/cancelled always
reaches the terminal. When the terminal cannot be reached, every request in the
message is answered with a JSON-RPC error (-32000) right away, so the client
never hangs. Requests are forwarded concurrently (a slow tool call does not block
a ping). The session id the terminal issues is passed along; when stdin closes
the session is ended and the bridge exits. Diagnostics go to stderr only:
stdout carries protocol messages and nothing else.

Standard library only; Python 3.8+.
"""

import argparse
import collections
import http.client
import json
import os
import socket
import sys
import threading
import time
import urllib.parse

VERSION = "1.2"
DEFAULT_URL = "http://127.0.0.1:41720/mcp"
EDITION = "community"                       # the edition this bridge ships with
TOKEN_ENV = "FINCEPT_COMMUNITY_MCP_TOKEN"   # edition-specific (EditionNames.h)
URL_ENV = "FINCEPT_COMMUNITY_MCP_URL"
MAX_IN_FLIGHT = 16         # concurrent requests to the terminal (notifications never wait for a slot)
EOF_GRACE_SECONDS = 3.0    # after stdin closes, let in-flight replies land
CONNECT_TIMEOUT = 5.0      # loopback: a terminal that is up accepts at once
RETRY_REFUSED_AFTER = 0.4  # one retry when the terminal is restarting
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
    def __init__(self, url, token, timeout, verbose, edition=EDITION):
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme != "http" or not parsed.hostname:
            raise ValueError("--url must be http://host:port/path (got %r)" % url)
        self.url = url
        self.host = parsed.hostname
        self.port = parsed.port or 80
        self.path = parsed.path or "/mcp"
        self.token = token
        self.timeout = timeout
        self.verbose = verbose
        self.protocol_version = None
        self.client_name = None
        self.session_id = None
        self.edition = edition
        self._verified = False          # /health said this is our edition
        self._verify_lock = threading.Lock()
        self._verify_cv = threading.Condition(self._verify_lock)
        self._probing = False
        self._verify_failed_why = ""
        self._out_lock = threading.Lock()
        self._state_lock = threading.Lock()
        self._slots = threading.BoundedSemaphore(MAX_IN_FLIGHT)
        self._threads = []

    # -- output ---------------------------------------------------------------

    def debug(self, message):
        if self.verbose:
            log(message)

    def emit(self, message):
        line = json.dumps(message, ensure_ascii=False, separators=(",", ":"))
        data = (line + "\n").encode("utf-8")
        with self._out_lock:
            try:
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            except (BrokenPipeError, OSError):
                pass  # the client went away; stdin EOF ends the bridge

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

    def headers(self, has_body=True):
        with self._state_lock:
            client, protocol, session = self.client_name, self.protocol_version, self.session_id
        h = {
            "Accept": "application/json, text/event-stream",
            "User-Agent": "fincept-mcp-stdio/%s%s" % (VERSION, " (%s)" % client if client else ""),
            "Host": "%s:%d" % (self.host, self.port),
        }
        if has_body:
            h["Content-Type"] = "application/json"
        if self.token and self._verified:
            h["Authorization"] = "Bearer " + self.token
        if protocol:
            h["MCP-Protocol-Version"] = protocol
        if session:
            h["Mcp-Session-Id"] = session
        return h

    # -- HTTP -----------------------------------------------------------------

    def ensure_verified(self):
        """(True, "") once GET /health - sent WITHOUT a token - reports this
        bridge's edition; else (False, plain reason). The token never reaches a
        server that is not this edition of Fincept Terminal (the other edition
        may be listening on the port). Re-checked after a connection failure.
        Single flight: messages that arrive while a check runs share its answer
        instead of each probing in turn (a refused loopback connect alone takes
        about 2 s on Windows)."""
        with self._verify_cv:
            if self._verified:
                return True, ""
            if self._probing:
                self._verify_cv.wait_for(lambda: not self._probing, timeout=CONNECT_TIMEOUT + 2.0)
                if self._verified:
                    return True, ""
                return False, self._verify_failed_why or "Fincept Terminal is not reachable"
            self._probing = True
        ok, why = False, "Fincept Terminal is not reachable"
        try:
            ok, why = self._probe_edition()
        finally:
            with self._verify_cv:
                self._probing = False
                self._verified = ok
                self._verify_failed_why = "" if ok else why
                self._verify_cv.notify_all()
        return ok, why

    def _probe_edition(self):
        """GET /health without a token; (True, "") when it is this edition."""
        conn = http.client.HTTPConnection(self.host, self.port, timeout=CONNECT_TIMEOUT)
        try:
            conn.request("GET", "/health", headers={"Host": "%s:%d" % (self.host, self.port),
                                                    "Accept": "application/json"})
            response = conn.getresponse()
            data = response.read(65536)
            status = response.status
        except (OSError, http.client.HTTPException) as exc:
            return False, "Fincept Terminal is not running or its MCP server is disabled: %s" % exc
        finally:
            conn.close()
        try:
            info = json.loads(data.decode("utf-8")) if status == 200 else None
        except (UnicodeDecodeError, ValueError):
            info = None
        edition = info.get("edition") if isinstance(info, dict) else None
        if edition == self.edition:
            return True, ""
        if isinstance(edition, str) and edition:
            what = "the %s edition" % edition
        else:
            what = "not Fincept Terminal (%s edition)" % self.edition
        text = ("The MCP server on port %d is %s; this bridge belongs to the %s edition and did not send "
                "it a token. Check the port in Fincept Terminal -> Settings -> AI Agents (MCP)."
                % (self.port, what, self.edition))
        log(text)
        return False, text

    def open(self, method, body, read_timeout):
        """One request on a fresh loopback connection. Returns the response
        (caller reads and closes it). Connect is bounded separately from the
        read, so a dead port fails in seconds, not after the tool timeout."""
        conn = http.client.HTTPConnection(self.host, self.port, timeout=CONNECT_TIMEOUT)
        try:
            conn.connect()
            conn.sock.settimeout(read_timeout)
            conn.request(method, self.path, body=body, headers=self.headers(body is not None))
            return conn, conn.getresponse()
        except Exception:
            conn.close()
            raise

    def open_verified(self, method, body, read_timeout):
        """open(), retried once when the connection was refused QUICKLY (the
        terminal restarting). The retry re-checks the edition first: whatever
        listens on the port after a restart must prove it is this edition before
        the token goes out. (On Windows a refused loopback connect itself takes
        about 2 s, so no retry is made there — the client is answered at once.)"""
        started = time.monotonic()
        try:
            return self.open(method, body, read_timeout)
        except ConnectionRefusedError:
            with self._verify_lock:
                self._verified = False
            if time.monotonic() - started > 1.0:
                raise
        time.sleep(RETRY_REFUSED_AFTER)
        ok, why = self.ensure_verified()
        if not ok:
            raise ConnectionRefusedError(why)
        return self.open(method, body, read_timeout)

    def deliver_sse(self, response, ids):
        """Emit every JSON-RPC message of a text/event-stream body AS IT ARRIVES
        (progress notifications reach the client while the tool still runs)."""
        answered = set()
        data_lines = []
        while True:
            raw = response.readline()
            if not raw:
                break
            line = raw.decode("utf-8", "replace").rstrip("\r\n")
            if line == "":
                if data_lines:
                    try:
                        message = json.loads("\n".join(data_lines))
                    except ValueError:
                        message = None
                    data_lines = []
                    if isinstance(message, (dict, list)):
                        self.note_incoming(message)
                        self.emit(message)
                        for m in (message if isinstance(message, list) else [message]):
                            if isinstance(m, dict) and "id" in m and ("result" in m or "error" in m):
                                answered.add(json.dumps(m["id"]))
                continue
            if line.startswith(":"):
                continue  # SSE comment / keep-alive
            field, _, value = line.partition(":")
            if value.startswith(" "):
                value = value[1:]
            if field == "data":
                data_lines.append(value)
        missing = [i for i in ids if json.dumps(i) not in answered]
        if missing:
            self.fail(missing, "Fincept Terminal closed the stream before answering")

    def deliver_json(self, data, ids):
        try:
            message = json.loads(data.decode("utf-8"))
        except (UnicodeDecodeError, ValueError) as exc:
            self.fail(ids, "Fincept Terminal returned an unreadable response: %s" % exc)
            return
        self.note_incoming(message)
        self.emit(message)

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
                    "Terminal -> Settings -> AI Agents (MCP) into %s." % TOKEN_ENV)
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
        ok, why = self.ensure_verified()
        if not ok:
            self.fail(ids, why)
            return
        started = time.monotonic()
        conn = None
        try:
            conn, response = self.open_verified("POST", raw_line.encode("utf-8"), self.timeout)
            status = response.status
            content_type = (response.getheader("Content-Type") or "").lower()
            session = response.getheader("Mcp-Session-Id")
            if session and 0 < len(session) <= 128:
                with self._state_lock:
                    self.session_id = session
            if status >= 400:
                self.http_failure(status, response.read(), ids)
                return
            if "text/event-stream" in content_type:
                self.deliver_sse(response, ids)
                return
            data = response.read()
        except socket.timeout as exc:
            self.fail(ids, "Fincept Terminal did not answer within %d s: %s" % (int(self.timeout), exc))
            return
        except (OSError, http.client.HTTPException, ValueError) as exc:
            text = "Fincept Terminal is not running or its MCP server is disabled: %s" % exc
            with self._verify_lock:
                self._verified = False  # a restart may have put another program on the port
            log("cannot reach %s: %s" % (self.url, exc))
            self.fail(ids, text)
            return
        finally:
            if conn is not None:
                conn.close()
        self.debug("HTTP %d in %.0f ms" % (status, (time.monotonic() - started) * 1000.0))
        if status == 202 or not data.strip():
            if ids:  # a request must be answered; an empty 200 is a server bug
                self.fail(ids, "Fincept Terminal returned no response (HTTP %d)" % status)
            return
        self.deliver_json(data, ids)

    def end_session(self):
        with self._state_lock:
            session = self.session_id
        if not session or not self._verified:
            return
        conn = None
        try:
            conn, response = self.open("DELETE", None, 2.0)
            response.read()
        except Exception:  # noqa: BLE001 - best effort on the way out
            pass
        finally:
            if conn is not None:
                conn.close()

    # -- main loop ------------------------------------------------------------

    def _worker(self, raw_line, message, slot):
        try:
            self.forward(raw_line, message)
        except Exception as exc:  # noqa: BLE001 - never leave the client waiting
            log("internal error: %r" % (exc,))
            self.fail(self.request_ids(message), "fincept-mcp-stdio internal error: %s" % exc, INTERNAL_ERROR)
        finally:
            if slot:
                self._slots.release()

    def _spawn(self, line, message, slot):
        worker = threading.Thread(target=self._worker, args=(line, message, slot), daemon=True)
        worker.start()
        with self._state_lock:
            self._threads = [t for t in self._threads if t.is_alive()]
            self._threads.append(worker)

    def _dispatch(self):
        """Start queued requests as slots free up. Runs on its own thread so the
        stdin reader never blocks: a notification read after a request that is
        still waiting for a slot is forwarded at once."""
        while True:
            with self._queue_cv:
                while not self._queue and not self._closed:
                    self._queue_cv.wait()
                if not self._queue:
                    return  # closed and drained
                line, message = self._queue.popleft()
            self._slots.acquire()
            self._spawn(line, message, True)

    def _drop_queued(self, request_id):
        """A cancel for a request that never left the queue: drop it (the spec
        says a cancelled request gets no response)."""
        with self._queue_cv:
            for item in list(self._queue):
                ids = self.request_ids(item[1])
                if ids == [request_id]:
                    self._queue.remove(item)

    def run(self):

        self._queue = collections.deque()
        self._queue_cv = threading.Condition()
        self._closed = False
        dispatcher = threading.Thread(target=self._dispatch, daemon=True)
        dispatcher.start()
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
            if self.request_ids(message):
                # Requests wait for a slot (bounded concurrency) on the dispatcher.
                with self._queue_cv:
                    self._queue.append((line, message))
                    self._queue_cv.notify()
                continue
            # Notifications and responses never wait: a notifications/cancelled
            # must get through even while every slot is held by the calls it
            # is cancelling.
            if isinstance(message, dict) and message.get("method") == "notifications/cancelled":
                params = message.get("params") or {}
                if "requestId" in params:
                    self._drop_queued(params["requestId"])
            self._spawn(line, message, False)

        with self._queue_cv:
            self._closed = True
            self._queue.clear()  # the client is gone: nobody reads these answers
            self._queue_cv.notify_all()
        deadline = time.monotonic() + EOF_GRACE_SECONDS
        with self._state_lock:
            threads = list(self._threads)
        for worker in threads:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            worker.join(remaining)
        self.end_session()
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
    parser.add_argument("--edition", default=EDITION,
                        help="the Fincept Terminal edition to talk to (default: %s)" % EDITION)
    args = parser.parse_args(argv)

    token = read_token(args.token_file)
    if not token:
        log("no token: set %s or pass --token-file (the terminal will answer 401)" % TOKEN_ENV)
    try:
        bridge = Bridge(args.url, token, max(1.0, args.timeout), args.verbose, args.edition)
    except ValueError as exc:
        log(str(exc))
        return 2
    try:
        return bridge.run()
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
