#!/usr/bin/env python3
"""
deepgram_tts.py — Stdin text -> Deepgram Aura TTS -> pyaudio playback.

JSON-lines stdout protocol matches tts.py so TtsService can swap providers
transparently:
    {"status": "speaking"}
    {"status": "done"}
    {"error":  "..."}
    {"fatal":  "..."}      unrecoverable, process exits

Strategy: POST text to Deepgram's /v1/speak endpoint with linear16 / 24000Hz
mono / container=none. Response body is raw PCM streamed as it's synthesised;
we pipe it directly into a pyaudio output stream so playback starts before
the full clip arrives (low latency, no temp files, no ffmpeg).

Environment variables (set by DeepgramTtsProvider in C++):
    DEEPGRAM_API_KEY      required
    FINCEPT_TTS_DG_MODEL  default "aura-2-thalia-en"
    FINCEPT_TTS_RATE      pyttsx3 leftover, ignored here
"""

import json
import os
import sys
import urllib.parse
from typing import Optional


def emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def diag(msg: str) -> None:
    sys.stderr.write(f"[deepgram_tts] {msg}\n")
    sys.stderr.flush()


SAMPLE_RATE = 24000   # Aura-2 native rate
CHANNELS    = 1
PLAYBACK_CHUNK = 4096 # bytes; ~85 ms at 24 kHz int16 mono


def play_stream(audio_iter, pyaudio_mod) -> None:
    """Write raw PCM bytes from `audio_iter` to a pyaudio output stream."""
    pa = pyaudio_mod.PyAudio()
    try:
        stream = pa.open(
            format=pyaudio_mod.paInt16,
            channels=CHANNELS,
            rate=SAMPLE_RATE,
            output=True,
        )
    except Exception as ex:
        pa.terminate()
        raise RuntimeError(f"pyaudio open output failed: {ex}") from ex

    try:
        emit({"status": "speaking"})
        for chunk in audio_iter:
            if not chunk:
                continue
            stream.write(chunk)
    finally:
        try:
            stream.stop_stream()
            stream.close()
        except Exception:
            pass
        pa.terminate()


def main() -> int:
    text = sys.stdin.read()
    if text is None:
        text = ""
    text = text.strip()
    diag(f"received text len={len(text)}")
    if not text:
        emit({"status": "done"})
        return 0

    api_key = os.environ.get("DEEPGRAM_API_KEY", "").strip()
    if not api_key:
        emit({"fatal": "DEEPGRAM_API_KEY not set — configure it in Settings -> Voice"})
        return 1

    model = os.environ.get("FINCEPT_TTS_DG_MODEL", "aura-2-thalia-en").strip() \
            or "aura-2-thalia-en"
    diag(f"config model={model}")

    # Try `requests` first — already pulled in transitively by deepgram-sdk and
    # most of the analytics stack. urllib fallback keeps the script self-contained
    # if requests is somehow absent.
    requests_mod: Optional[object]
    try:
        import requests as requests_mod  # type: ignore
        diag(f"requests imported, version={requests_mod.__version__}")
    except ImportError:
        requests_mod = None
        diag("requests not available — falling back to urllib")

    try:
        import pyaudio
        diag(f"pyaudio imported, version={getattr(pyaudio, '__version__', '?')}")
    except ImportError as e:
        diag(f"pyaudio import failed: {e}")
        emit({"fatal": (
            "PyAudio not available in venv-numpy2. "
            "Open Settings -> Python Env -> Reinstall packages."
        )})
        return 1

    qs = urllib.parse.urlencode({
        "model":       model,
        "encoding":    "linear16",
        "sample_rate": str(SAMPLE_RATE),
        "container":   "none",
    })
    url = f"https://api.deepgram.com/v1/speak?{qs}"
    headers = {
        "Authorization": f"Token {api_key}",
        "Content-Type":  "application/json",
        "Accept":        "audio/wav,audio/*",
    }
    payload = json.dumps({"text": text[:2000]}).encode("utf-8")

    # Trim text to a reasonable upper bound — Deepgram /v1/speak caps text
    # at 2000 chars per request anyway.

    try:
        if requests_mod is not None:
            diag("POST /v1/speak (requests, stream=True)")
            with requests_mod.post(url, headers=headers, data=payload,
                                   stream=True, timeout=30) as resp:
                if resp.status_code != 200:
                    body = resp.text[:500] if resp.text else ""
                    diag(f"non-200: {resp.status_code} body={body}")
                    if resp.status_code == 401:
                        emit({"fatal": "Deepgram rejected API key (401)"})
                    elif resp.status_code == 402:
                        emit({"fatal": "Deepgram credits exhausted (402)"})
                    elif resp.status_code == 400:
                        emit({"fatal": f"Deepgram bad request (400): {body}"})
                    else:
                        emit({"fatal": f"Deepgram TTS HTTP {resp.status_code}: {body}"})
                    return 1
                resp.raw.decode_content = True
                play_stream(resp.iter_content(chunk_size=PLAYBACK_CHUNK), pyaudio)
        else:
            import urllib.request
            req = urllib.request.Request(url, data=payload, headers=headers, method="POST")
            diag("POST /v1/speak (urllib)")
            with urllib.request.urlopen(req, timeout=30) as resp:
                code = resp.getcode()
                if code != 200:
                    emit({"fatal": f"Deepgram TTS HTTP {code}"})
                    return 1
                def _iter():
                    while True:
                        chunk = resp.read(PLAYBACK_CHUNK)
                        if not chunk:
                            return
                        yield chunk
                play_stream(_iter(), pyaudio)
    except Exception as ex:
        diag(f"runtime exception: {ex}")
        emit({"error": f"Deepgram TTS error: {ex}"})
        return 1

    emit({"status": "done"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
