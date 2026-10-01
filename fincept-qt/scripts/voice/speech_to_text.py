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
import time


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

    # We bridge sounddevice to SpeechRecognition by subclassing sr.AudioSource —
    # recognizer.listen() only touches source.stream.read(CHUNK), so a minimal
    # wrapper is enough.
    try:
        import sounddevice as sd
    except ImportError:
        emit({"fatal": "sounddevice not installed — run: pip install sounddevice"})
        sys.exit(1)

    SAMPLE_RATE  = 16000
    CHANNELS     = 1
    SAMPLE_WIDTH = 2     # int16 = 2 bytes per sample
    CHUNK_FRAMES = 1024  # matches SpeechRecognition's default chunk size

    class _SoundDeviceStream:
        """Implements the .read(size) and .close() API that
        sr.Recognizer.listen() expects on AudioSource.stream."""

        def __init__(self):
            self._stream = sd.RawInputStream(
                samplerate=SAMPLE_RATE,
                channels=CHANNELS,
                dtype="int16",
                blocksize=CHUNK_FRAMES,
            )
            self._stream.start()

        def read(self, size: int) -> bytes:
            # SpeechRecognition asks for `size` BYTES; sounddevice reads frames.
            frames = max(1, size // (SAMPLE_WIDTH * CHANNELS))
            data, _overflowed = self._stream.read(frames)
            return bytes(data)

        def close(self):
            try:
                self._stream.stop()
                self._stream.close()
            except Exception:
                pass

    class SoundDeviceAudioSource(sr.AudioSource):
        def __init__(self):
            self.SAMPLE_RATE  = SAMPLE_RATE
            self.SAMPLE_WIDTH = SAMPLE_WIDTH
            self.CHUNK        = CHUNK_FRAMES * SAMPLE_WIDTH * CHANNELS
            self.stream = None

        def __enter__(self):
            self.stream = _SoundDeviceStream()
            return self

        def __exit__(self, exc_type, exc, tb):
            if self.stream is not None:
                self.stream.close()
                self.stream = None
            return False

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
        mic = SoundDeviceAudioSource()
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
    #
    # `consecutive_errors` counts loop iterations that failed OUTSIDE the speech
    # round-trip (opening/reading the mic: unplugged, device busy, permission
    # revoked). Those fail instantly, so without a bound the loop spun at full speed
    # printing an {"error"} line per turn forever — a CPU burn and a flood of UI
    # error events. A few in a row means the mic is gone: report it and exit.
    consecutive_errors = 0
    while running:
        emit({"status": "listening"})
        try:
            with mic as source:
                audio = recognizer.listen(source, timeout=10, phrase_time_limit=15)
            consecutive_errors = 0  # the mic opened and read — failures below are not its fault

            text = recognizer.recognize_google(audio)
            if text and text.strip():
                emit({"text": text.strip()})

        except sr.WaitTimeoutError:
            # No speech detected within timeout — the mic is fine; loop and try again
            consecutive_errors = 0
        except sr.UnknownValueError:
            # Could not understand audio — not an error, just try again
            pass
        except sr.RequestError as e:
            emit({"error": f"Google API request failed: {e}"})
        except KeyboardInterrupt:
            break
        except Exception as e:
            consecutive_errors += 1
            emit({"error": f"Unexpected error: {e}"})
            if consecutive_errors >= 3:
                emit({"fatal": f"Microphone unavailable: {e}"})
                sys.exit(1)
            time.sleep(0.5)

    emit({"status": "stopped"})


if __name__ == "__main__":
    main()
