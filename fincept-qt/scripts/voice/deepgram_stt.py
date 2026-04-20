#!/usr/bin/env python3
"""
deepgram_stt.py — Microphone → Deepgram live WebSocket STT → JSON stdout.

Matches the same JSON-lines protocol as speech_to_text.py so SpeechService can
parse either transparently:
    {"status": "calibrating"}          — adjusting for ambient noise (no-op here)
    {"status": "listening"}            — ready, waiting for speech
    {"text": "hello world"}            — finalized transcription
    {"error": "..."}                   — recoverable error (keeps listening)
    {"fatal": "..."}                   — unrecoverable error (process exits)

Environment variables (injected by DeepgramSttProvider in C++):
    DEEPGRAM_API_KEY       — required
    FINCEPT_STT_MODEL      — default "nova-3"
    FINCEPT_STT_LANGUAGE   — default "en"
    FINCEPT_STT_KEYTERMS   — comma-separated boosted terms (optional)

The process runs until stdin is closed, a fatal error occurs, or it receives
SIGTERM (Windows: QProcess::kill sends taskkill which terminates anyway).
"""

import json
import os
import signal
import sys
import threading


def emit(obj: dict) -> None:
    """Write a JSON line to stdout and flush immediately."""
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def main() -> None:
    api_key = os.environ.get("DEEPGRAM_API_KEY", "").strip()
    if not api_key:
        emit({"fatal": "DEEPGRAM_API_KEY not set — configure it in Settings → Voice"})
        sys.exit(1)

    model = os.environ.get("FINCEPT_STT_MODEL", "nova-3").strip() or "nova-3"
    language = os.environ.get("FINCEPT_STT_LANGUAGE", "en").strip() or "en"
    keyterms_raw = os.environ.get("FINCEPT_STT_KEYTERMS", "").strip()
    keyterms = [t.strip() for t in keyterms_raw.split(",") if t.strip()]

    # Required deps — fail fast with clear install hints
    try:
        from deepgram import (
            DeepgramClient,
            DeepgramClientOptions,
            LiveOptions,
            LiveTranscriptionEvents,
            Microphone,
        )
    except ImportError as e:
        emit({"fatal": f"deepgram-sdk not installed — run: pip install deepgram-sdk ({e})"})
        sys.exit(1)

    try:
        import pyaudio  # noqa: F401  — required by Microphone helper
    except ImportError:
        emit({"fatal": "PyAudio not installed — run: pip install PyAudio"})
        sys.exit(1)

    # Graceful shutdown flag
    stop_flag = threading.Event()

    def on_signal(signum, frame):
        stop_flag.set()

    if hasattr(signal, "SIGTERM"):
        try:
            signal.signal(signal.SIGTERM, on_signal)
        except (ValueError, OSError):
            pass  # Not on main thread / not supported

    # Build Deepgram client — quiet the SDK's own stdout/stderr spam, which
    # would otherwise pollute our JSON-lines protocol.
    config = DeepgramClientOptions(options={"keepalive": "true"})
    client = DeepgramClient(api_key, config)
    dg_conn = client.listen.websocket.v("1")

    def on_open(_self, _open, **_kw):
        emit({"status": "listening"})

    def on_message(_self, result, **_kw):
        try:
            alt = result.channel.alternatives[0]
            transcript = (alt.transcript or "").strip()
            if not transcript:
                return
            # Only emit finalized utterances — interim partials would flood the
            # consumer which expects completed commands.
            is_final = bool(getattr(result, "is_final", False))
            speech_final = bool(getattr(result, "speech_final", False))
            if is_final and speech_final:
                emit({"text": transcript})
        except Exception as ex:
            emit({"error": f"Transcript parse failed: {ex}"})

    def on_error(_self, err, **_kw):
        emit({"error": f"Deepgram error: {err}"})

    def on_close(_self, _close, **_kw):
        stop_flag.set()

    def on_unhandled(_self, _unhandled, **_kw):
        # Deepgram emits several event types we don't care about (Metadata,
        # SpeechStarted, UtteranceEnd). Swallow them silently.
        pass

    dg_conn.on(LiveTranscriptionEvents.Open, on_open)
    dg_conn.on(LiveTranscriptionEvents.Transcript, on_message)
    dg_conn.on(LiveTranscriptionEvents.Error, on_error)
    dg_conn.on(LiveTranscriptionEvents.Close, on_close)
    dg_conn.on(LiveTranscriptionEvents.Unhandled, on_unhandled)

    live_opts = LiveOptions(
        model=model,
        language=language,
        encoding="linear16",
        sample_rate=16000,
        channels=1,
        punctuate=True,
        smart_format=True,
        interim_results=False,
        endpointing=300,
        vad_events=True,
        utterance_end_ms="1000",
    )

    # Keyword boosting — API surface depends on model.
    # Nova-3 uses `keyterm` (no weights, English-only).
    # Older models use `keywords` with optional weight.
    if keyterms:
        try:
            if model.startswith("nova-3"):
                live_opts.keyterm = keyterms
            else:
                live_opts.keywords = keyterms
        except Exception as ex:  # non-fatal — continue without boosting
            emit({"error": f"Keyterm setup failed: {ex}"})

    emit({"status": "calibrating"})
    if not dg_conn.start(live_opts):
        emit({"fatal": "Failed to open Deepgram WebSocket connection"})
        sys.exit(1)

    # Start microphone → feeds binary audio to dg_conn
    try:
        microphone = Microphone(dg_conn.send)
    except Exception as ex:
        dg_conn.finish()
        emit({"fatal": f"No microphone available: {ex}"})
        sys.exit(1)

    if not microphone.start():
        dg_conn.finish()
        emit({"fatal": "Failed to start microphone"})
        sys.exit(1)

    # Block until signalled or parent closes stdin
    try:
        while not stop_flag.is_set():
            # Wake up periodically to check the flag; also exit if stdin is
            # closed (parent QProcess was killed).
            if stop_flag.wait(timeout=0.5):
                break
            if sys.stdin.closed:
                break
    except KeyboardInterrupt:
        pass
    finally:
        try:
            microphone.finish()
        except Exception:
            pass
        try:
            dg_conn.finish()
        except Exception:
            pass
        emit({"status": "stopped"})


if __name__ == "__main__":
    main()
