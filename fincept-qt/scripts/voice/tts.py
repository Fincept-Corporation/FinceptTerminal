#!/usr/bin/env python3
"""
tts.py — Speak the text on stdin via pyttsx3, then exit.

Used by TtsService as a one-shot subprocess. The C++ side launches one
process per utterance, writes the text to stdin (terminated by EOF), and
listens for {"status": "speaking"|"done"} events on stdout.

Stdin protocol:
    raw UTF-8 text — read until EOF.

Stdout protocol (JSON-lines, one per line, flushed):
    {"status": "speaking"}
    {"status": "done"}
    {"error":  "..."}
    {"fatal":  "..."}        engine could not be initialised

Environment variables (optional, all set by TtsService):
    FINCEPT_TTS_RATE     — words/min override (default = engine default).
    FINCEPT_TTS_VOLUME   — 0.0…1.0 (default 1.0).
    FINCEPT_TTS_VOICE    — substring match against voice id/name; first hit wins.
"""

import json
import os
import sys


def emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def diag(msg: str) -> None:
    sys.stderr.write(f"[tts] {msg}\n")
    sys.stderr.flush()


def main() -> int:
    text = sys.stdin.read()
    if text is None:
        text = ""
    text = text.strip()
    diag(f"received text len={len(text)}")
    if not text:
        emit({"status": "done"})
        return 0

    try:
        import pyttsx3
    except ImportError as e:
        diag(f"pyttsx3 import failed: {e}")
        emit({"fatal": (
            "pyttsx3 not available in venv-numpy2. "
            "Open Settings -> Python Env -> Reinstall packages, then try again."
        )})
        return 1

    try:
        engine = pyttsx3.init()
    except Exception as e:
        diag(f"pyttsx3 init failed: {e}")
        emit({"fatal": f"TTS engine could not be initialised: {e}"})
        return 1

    # Optional tuning from env
    rate_env = os.environ.get("FINCEPT_TTS_RATE", "").strip()
    if rate_env:
        try:
            engine.setProperty("rate", int(rate_env))
        except Exception as e:
            diag(f"set rate failed: {e}")

    vol_env = os.environ.get("FINCEPT_TTS_VOLUME", "").strip()
    if vol_env:
        try:
            engine.setProperty("volume", max(0.0, min(1.0, float(vol_env))))
        except Exception as e:
            diag(f"set volume failed: {e}")

    voice_pref = os.environ.get("FINCEPT_TTS_VOICE", "").strip().lower()
    if voice_pref:
        try:
            for v in engine.getProperty("voices") or []:
                vid = (getattr(v, "id", "") or "").lower()
                vname = (getattr(v, "name", "") or "").lower()
                if voice_pref in vid or voice_pref in vname:
                    engine.setProperty("voice", v.id)
                    diag(f"voice selected: {v.id}")
                    break
        except Exception as e:
            diag(f"voice selection failed: {e}")

    # Trim very long replies — SAPI can lock up on huge buffers, and the
    # caller already truncates around the same threshold.
    speak = text[:2000]

    emit({"status": "speaking"})
    try:
        engine.say(speak)
        engine.runAndWait()
    except Exception as e:
        diag(f"runtime exception: {e}")
        emit({"error": f"TTS runtime error: {e}"})
        return 1

    emit({"status": "done"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
