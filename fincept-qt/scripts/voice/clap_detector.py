#!/usr/bin/env python3
"""
clap_detector.py — Continuous mic loop -> clap event detection -> JSON stdout.

Privacy: audio is analysed in-process and discarded. Nothing leaves the box.

Stdout protocol (one JSON line per event, flushed):
    {"status": "listening"}              ready, mic open
    {"event":  "clap"}                   clap (or double-clap, depending on mode)
    {"error":  "..."}                    recoverable
    {"fatal":  "..."}                    unrecoverable, process exits

Environment variables (set by ClapDetectorService in C++):
    FINCEPT_CLAP_MODE         "double" (default) | "single"
    FINCEPT_CLAP_PEAK_MIN     int 0..32768  — min peak amplitude to count as a
                              candidate. Default 12000 (~ -8 dBFS). Lower = more
                              sensitive, more false positives.
    FINCEPT_CLAP_PR_RATIO     float — min peak/rms ratio. Claps are sharp; this
                              filters out steady loud noise like music or a fan.
                              Default 4.0.
    FINCEPT_CLAP_MAX_GAP_MS   int — max ms between two candidates to count as a
                              double-clap. Default 1500.
    FINCEPT_CLAP_DEBOUNCE_MS  int — silence window after a fired event. Default 1500.
    FINCEPT_STT_DEVICE        same key as STT — input device name substring.
"""

import json
import math
import os
import signal
import struct
import sys
import threading
import time
from typing import List, Optional


def emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def diag(msg: str) -> None:
    sys.stderr.write(f"[clap_detector] {msg}\n")
    sys.stderr.flush()


SAMPLE_RATE   = 16000
CHANNELS      = 1
SAMPLE_WIDTH  = 2
CHUNK_SAMPLES = 1600   # 100 ms at 16 kHz — short enough to localise transients


def list_input_devices(pa) -> List[dict]:
    out = []
    try:
        host = pa.get_default_host_api_info()
        default_input_index = host.get("defaultInputDevice", -1)
    except Exception:
        default_input_index = -1
    for i in range(pa.get_device_count()):
        try:
            info = pa.get_device_info_by_index(i)
        except Exception:
            continue
        if int(info.get("maxInputChannels", 0)) <= 0:
            continue
        out.append({
            "index": i,
            "name": str(info.get("name", "")),
            "is_default": (i == default_input_index),
        })
    return out


def pick_device(pa, pref: str) -> Optional[int]:
    inputs = list_input_devices(pa)
    if not inputs:
        return None
    if pref:
        pref_l = pref.lower()
        for d in inputs:
            if pref_l in d["name"].lower():
                return d["index"]
    for d in inputs:
        if d["is_default"]:
            return d["index"]
    return inputs[0]["index"]


def rms_peak_int16(buf: bytes):
    """Return (rms, peak) for an int16 PCM buffer; both 0..32768."""
    n = len(buf) // SAMPLE_WIDTH
    if n == 0:
        return 0.0, 0
    samples = struct.unpack("<" + "h" * n, buf)
    s = 0
    p = 0
    for v in samples:
        s += v * v
        a = -v if v < 0 else v
        if a > p:
            p = a
    return math.sqrt(s / n), p


def main() -> None:
    diag("script started — pid=" + str(os.getpid()))

    mode = os.environ.get("FINCEPT_CLAP_MODE", "double").strip().lower() or "double"
    if mode not in ("single", "double"):
        mode = "double"

    try:
        peak_min = int(os.environ.get("FINCEPT_CLAP_PEAK_MIN", "12000"))
    except ValueError:
        peak_min = 12000
    try:
        pr_ratio = float(os.environ.get("FINCEPT_CLAP_PR_RATIO", "4.0"))
    except ValueError:
        pr_ratio = 4.0
    try:
        max_gap_ms = int(os.environ.get("FINCEPT_CLAP_MAX_GAP_MS", "1500"))
    except ValueError:
        max_gap_ms = 1500
    try:
        debounce_ms = int(os.environ.get("FINCEPT_CLAP_DEBOUNCE_MS", "1500"))
    except ValueError:
        debounce_ms = 1500
    device_pref = os.environ.get("FINCEPT_STT_DEVICE", "").strip()

    diag(f"config mode={mode} peak_min={peak_min} pr_ratio={pr_ratio} "
         f"max_gap_ms={max_gap_ms} debounce_ms={debounce_ms} device='{device_pref}'")

    try:
        import pyaudio
    except ImportError as e:
        diag(f"pyaudio import failed: {e}")
        emit({"fatal": (
            "PyAudio not available in venv-numpy2. "
            "Open Settings -> Python Env -> Reinstall packages."
        )})
        sys.exit(1)

    stop_flag = threading.Event()

    def on_signal(_signum, _frame):
        stop_flag.set()
    if hasattr(signal, "SIGTERM"):
        try:
            signal.signal(signal.SIGTERM, on_signal)
        except (ValueError, OSError):
            pass

    pa = pyaudio.PyAudio()
    device_index = pick_device(pa, device_pref)
    if device_index is None:
        emit({"fatal": "No input audio device available"})
        pa.terminate()
        sys.exit(1)

    try:
        stream = pa.open(
            format=pyaudio.paInt16,
            channels=CHANNELS,
            rate=SAMPLE_RATE,
            input=True,
            input_device_index=device_index,
            frames_per_buffer=CHUNK_SAMPLES,
        )
    except Exception as ex:
        diag(f"pa.open failed: {ex}")
        pa.terminate()
        emit({"fatal": f"Could not open microphone stream: {ex}"})
        sys.exit(1)

    diag(f"audio stream opened — device={device_index}")
    emit({"status": "listening"})

    # Detection state machine
    last_candidate_ms: Optional[int] = None
    last_fire_ms: int = 0
    chunks_seen = 0
    candidates_seen = 0

    try:
        while not stop_flag.is_set():
            try:
                buf = stream.read(CHUNK_SAMPLES, exception_on_overflow=False)
            except Exception as ex:
                diag(f"stream.read failed: {ex}")
                emit({"error": f"Audio read failed: {ex}"})
                break

            if not buf:
                continue
            chunks_seen += 1

            now_ms = int(time.time() * 1000)
            if now_ms - last_fire_ms < debounce_ms:
                # Within debounce window — drop everything to avoid double-firing
                continue

            rms, peak = rms_peak_int16(buf)
            if peak < peak_min:
                continue
            ratio = (peak / rms) if rms > 1.0 else 0.0
            if ratio < pr_ratio:
                continue

            # Sharp transient candidate
            candidates_seen += 1
            diag(f"candidate #{candidates_seen} peak={peak} rms={rms:.0f} ratio={ratio:.2f}")

            if mode == "single":
                emit({"event": "clap"})
                last_fire_ms = now_ms
                last_candidate_ms = None
                continue

            # Double-clap mode
            if last_candidate_ms is None or (now_ms - last_candidate_ms) > max_gap_ms:
                last_candidate_ms = now_ms
            else:
                # Second candidate within window
                emit({"event": "clap"})
                last_fire_ms = now_ms
                last_candidate_ms = None

    except KeyboardInterrupt:
        pass
    finally:
        diag(f"shutting down — chunks={chunks_seen} candidates={candidates_seen}")
        try:
            stream.stop_stream()
            stream.close()
        except Exception:
            pass
        try:
            pa.terminate()
        except Exception:
            pass


if __name__ == "__main__":
    main()
