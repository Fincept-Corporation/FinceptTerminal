"""Alpha Arena LLM-call subprocess.

Single-purpose: take a system prompt + user prompt + model identity, call the
right provider SDK, return the response text + token/cost metadata as JSON on
stdout. That is all this script does. No engine logic, no persistence, no
trading state — those live in the C++ engine.

Wire protocol (input — argv[1] is a path to a request JSON file):
  {
    "system_prompt":  "<text>",
    "user_prompt":    "<text>",
    "model": {
      "provider":     "openai|anthropic|google|deepseek|xai|qwen",
      "id":           "<model id, e.g. gpt-5>",
      "base_url":     "<optional>",
      "api_version":  "<optional>"
    },
    "temperature":    0.1,
    "max_output_tokens": 4096,
    "secret_handle":  "<opaque token>",
    "secret_pipe":    "fincept-aa-<nonce>"
  }

Wire protocol (output — JSON on stdout):
  Success: {"response_text": "...", "latency_ms": int, "tokens_in": int,
            "tokens_out": int, "cost_usd": float}
  Failure: {"error": "<message>", "stage": "<redeem|sdk_call|response>"}

Failure modes are non-fatal — caller logs and treats as a no-op tick.
"""

import json
import os
import socket
import struct
import sys
import time
from pathlib import Path


def fail(stage: str, msg: str) -> None:
    sys.stdout.write(json.dumps({"error": msg, "stage": stage}))
    sys.stdout.flush()
    sys.exit(1)


def redeem_secret(pipe_name: str, handle: str) -> str:
    """Connect to the engine's per-tick local IPC server, exchange the opaque
    handle for the actual API key. Server invalidates the handle after first
    redemption.

    On Windows the pipe is `\\\\.\\pipe\\<pipe_name>` (QLocalServer convention);
    on POSIX it's an abstract or file-based UNIX socket at the same name.
    """
    if not handle:
        return ""
    try:
        if os.name == "nt":
            # Open the named pipe directly. CreateFile-style read/write.
            pipe_path = r"\\.\pipe\\" + pipe_name
            with open(pipe_path, "r+b", buffering=0) as f:
                payload = json.dumps({"handle": handle}).encode("utf-8")
                f.write(struct.pack("<I", len(payload)) + payload)
                # Read length-prefixed reply.
                hdr = f.read(4)
                if len(hdr) != 4:
                    raise RuntimeError("short read on secret pipe header")
                (n,) = struct.unpack("<I", hdr)
                body = f.read(n)
                if len(body) != n:
                    raise RuntimeError("short read on secret pipe body")
                reply = json.loads(body.decode("utf-8"))
        else:
            # POSIX abstract namespace if the engine is using one (Qt
            # QLocalServer uses /tmp/<name> by default on Linux/macOS).
            sock_path = f"/tmp/{pipe_name}"
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.connect(sock_path)
                payload = json.dumps({"handle": handle}).encode("utf-8")
                s.sendall(struct.pack("<I", len(payload)) + payload)
                hdr = s.recv(4)
                if len(hdr) != 4:
                    raise RuntimeError("short read on secret socket header")
                (n,) = struct.unpack("<I", hdr)
                body = b""
                while len(body) < n:
                    chunk = s.recv(n - len(body))
                    if not chunk:
                        break
                    body += chunk
                reply = json.loads(body.decode("utf-8"))

        if "error" in reply:
            raise RuntimeError(reply["error"])
        return reply.get("api_key", "")
    except Exception as exc:
        raise RuntimeError(f"secret redemption failed: {exc}") from exc


def call_openai_compatible(api_key, base_url, model_id, system, user, temperature, max_tokens):
    """OpenAI / DeepSeek / xAI / Qwen-via-DashScope use the OpenAI SDK shape.
    We import lazily so that a missing optional SDK doesn't break the script."""
    try:
        from openai import OpenAI
    except ImportError as exc:
        raise RuntimeError(f"openai SDK not installed: {exc}") from exc

    kwargs = {"api_key": api_key}
    if base_url:
        kwargs["base_url"] = base_url
    client = OpenAI(**kwargs)

    resp = client.chat.completions.create(
        model=model_id,
        messages=[
            {"role": "system", "content": system},
            {"role": "user", "content": user},
        ],
        temperature=temperature,
        max_tokens=max_tokens,
    )
    text = resp.choices[0].message.content or ""
    usage = getattr(resp, "usage", None)
    return text, getattr(usage, "prompt_tokens", 0), getattr(usage, "completion_tokens", 0)


def call_anthropic(api_key, model_id, system, user, temperature, max_tokens):
    try:
        import anthropic
    except ImportError as exc:
        raise RuntimeError(f"anthropic SDK not installed: {exc}") from exc

    client = anthropic.Anthropic(api_key=api_key)
    resp = client.messages.create(
        model=model_id,
        system=system,
        messages=[{"role": "user", "content": user}],
        temperature=temperature,
        max_tokens=max_tokens,
    )
    text = "".join(b.text for b in resp.content if getattr(b, "type", "") == "text")
    return text, getattr(resp.usage, "input_tokens", 0), getattr(resp.usage, "output_tokens", 0)


def call_google(api_key, model_id, system, user, temperature, max_tokens):
    try:
        import google.generativeai as genai
    except ImportError as exc:
        raise RuntimeError(f"google.generativeai SDK not installed: {exc}") from exc

    genai.configure(api_key=api_key)
    # Gemini takes the system prompt as a top-level instruction.
    model = genai.GenerativeModel(model_name=model_id, system_instruction=system)
    resp = model.generate_content(
        user,
        generation_config={"temperature": temperature, "max_output_tokens": max_tokens},
    )
    text = (resp.text or "") if hasattr(resp, "text") else ""
    usage = getattr(resp, "usage_metadata", None)
    in_tok = getattr(usage, "prompt_token_count", 0) if usage else 0
    out_tok = getattr(usage, "candidates_token_count", 0) if usage else 0
    return text, in_tok, out_tok


def estimate_cost_usd(provider: str, model_id: str, in_tok: int, out_tok: int) -> float:
    """Approximate prices ($/1M tokens). The C++ side ultimately treats this
    as a competition signal, not an accounting truth — keep it directional."""
    rates = {
        # provider:model_prefix -> (in_per_m, out_per_m)
        "openai:gpt-5":           (5.0, 15.0),
        "openai:gpt-4o":          (2.5, 10.0),
        "anthropic:claude-sonnet": (3.0, 15.0),
        "anthropic:claude-opus":   (15.0, 75.0),
        "google:gemini-2.5-pro":   (1.25, 5.0),
        "deepseek:deepseek-chat":  (0.14, 0.28),
        "xai:grok-4":              (5.0, 15.0),
        "qwen:qwen3-max":          (1.6, 6.4),
    }
    for key, (in_rate, out_rate) in rates.items():
        prefix_provider, prefix_model = key.split(":", 1)
        if provider == prefix_provider and model_id.startswith(prefix_model):
            return (in_tok * in_rate + out_tok * out_rate) / 1_000_000.0
    return 0.0


def main() -> None:
    if len(sys.argv) < 2:
        fail("usage", "expected request JSON file path on argv[1]")

    request_path = Path(sys.argv[1])
    try:
        request = json.loads(request_path.read_text(encoding="utf-8"))
    except Exception as exc:
        fail("usage", f"failed to read request: {exc}")
    finally:
        # Best-effort delete; secrets aren't in the file, but reduce footprint.
        try:
            request_path.unlink()
        except OSError:
            pass

    model = request.get("model", {})
    provider = model.get("provider", "").lower()
    model_id = model.get("id", "")
    base_url = model.get("base_url", "") or None
    system = request.get("system_prompt", "")
    user = request.get("user_prompt", "")
    temperature = float(request.get("temperature", 0.1))
    max_tokens = int(request.get("max_output_tokens", 4096))
    handle = request.get("secret_handle", "")
    pipe_name = request.get("secret_pipe", "")

    try:
        api_key = redeem_secret(pipe_name, handle)
    except Exception as exc:
        fail("redeem", str(exc))

    if not api_key:
        fail("redeem", "no api_key available after redemption")

    started = time.monotonic()
    try:
        if provider in ("openai", "deepseek", "xai", "qwen"):
            text, in_tok, out_tok = call_openai_compatible(
                api_key, base_url, model_id, system, user, temperature, max_tokens
            )
        elif provider == "anthropic":
            text, in_tok, out_tok = call_anthropic(
                api_key, model_id, system, user, temperature, max_tokens
            )
        elif provider == "google":
            text, in_tok, out_tok = call_google(
                api_key, model_id, system, user, temperature, max_tokens
            )
        else:
            fail("sdk_call", f"unknown provider: {provider}")
    except Exception as exc:
        fail("sdk_call", str(exc))

    latency_ms = int((time.monotonic() - started) * 1000)
    sys.stdout.write(
        json.dumps(
            {
                "response_text": text,
                "latency_ms": latency_ms,
                "tokens_in": int(in_tok or 0),
                "tokens_out": int(out_tok or 0),
                "cost_usd": estimate_cost_usd(provider, model_id, in_tok or 0, out_tok or 0),
            }
        )
    )
    sys.stdout.flush()


if __name__ == "__main__":
    main()
