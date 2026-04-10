#!/usr/bin/env python3
"""
speech_to_text.py — Microphone → Google Speech Recognition → JSON stdout.

Protocol (one JSON object per line on stdout):
    {"status": "calibrating"}          — adjusting for ambient noise
    {"status": "listening"}            — ready, waiting for speech
    {"text": "hello world"}            — transcription result
    {"error": "..."}                   — recoverable error (keeps listening)
    {"fatal": "..."}                   — unrecoverable error (process exits)

The process runs until stdin is closed or it receives SIGTERM.
"""

import json
import sys
import signal


def emit(obj: dict) -> None:
    """Write a JSON line to stdout and flush immediately."""
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def main() -> None:
    # Fail fast if speech_recognition is not installed
    try:
        import speech_recognition as sr
    except ImportError:
        emit({"fatal": "speech_recognition not installed — run: pip install SpeechRecognition"})
        sys.exit(1)

    # Fail fast if PyAudio is not available (needed for microphone access)
    try:
        import pyaudio  # noqa: F401
    except ImportError:
        emit({"fatal": "PyAudio not installed — run: pip install PyAudio"})
        sys.exit(1)

    # Graceful shutdown on SIGTERM (Windows doesn't have SIGTERM via signal,
    # but QProcess::kill sends taskkill which terminates the process anyway).
    running = True

    def on_signal(signum, frame):
        nonlocal running
        running = False

    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, on_signal)

    recognizer = sr.Recognizer()
    recognizer.energy_threshold = 300
    recognizer.dynamic_energy_threshold = True
    recognizer.pause_threshold = 0.8

    try:
        mic = sr.Microphone()
    except (OSError, AttributeError) as e:
        emit({"fatal": f"No microphone available: {e}"})
        sys.exit(1)

    # Calibrate for ambient noise
    emit({"status": "calibrating"})
    try:
        with mic as source:
            recognizer.adjust_for_ambient_noise(source, duration=1)
    except Exception as e:
        emit({"fatal": f"Microphone calibration failed: {e}"})
        sys.exit(1)

    # Main listening loop
    while running:
        emit({"status": "listening"})
        try:
            with mic as source:
                audio = recognizer.listen(source, timeout=10, phrase_time_limit=15)

            text = recognizer.recognize_google(audio)
            if text and text.strip():
                emit({"text": text.strip()})

        except sr.WaitTimeoutError:
            # No speech detected within timeout — loop and try again
            pass
        except sr.UnknownValueError:
            # Could not understand audio — not an error, just try again
            pass
        except sr.RequestError as e:
            emit({"error": f"Google API request failed: {e}"})
        except KeyboardInterrupt:
            break
        except Exception as e:
            emit({"error": f"Unexpected error: {e}"})

    emit({"status": "stopped"})


if __name__ == "__main__":
    main()
