#!/usr/bin/env python3
"""
deepgram_stt.py — Mic (raw pyaudio) -> Deepgram live WebSocket -> JSON stdout.

This rewrite replaces deepgram-sdk's opaque `Microphone` helper with a manual
pyaudio capture loop so we can:
    * pick a specific input device by name substring,
    * apply a software gain multiplier (laptop mics are often very quiet),
    * log periodic RMS levels so we can tell whether the mic is actually
      capturing anything or whether the user just needs to speak louder /
      switch device.

Stdout protocol (one JSON object per line, identical to before):
    {"status": "calibrating"|"listening"|"stopped"}
    {"text":   "..."}                       finalized transcription
    {"error":  "..."}                       recoverable
    {"fatal":  "..."}                       unrecoverable, process exits

Environment variables (set by DeepgramSttProvider in C++):
    DEEPGRAM_API_KEY     required
    FINCEPT_STT_MODEL    default "nova-3"
    FINCEPT_STT_LANGUAGE default "en"
    FINCEPT_STT_KEYTERMS comma-separated boosted terms (optional)
    FINCEPT_STT_GAIN     audio gain multiplier (default "3.0").  Try 5-10 if
                         the mic is far-field; leave at 1.0 for a headset.
    FINCEPT_STT_DEVICE   case-insensitive substring of input-device name.  If
                         empty, the system default device is used.
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
    sys.stderr.write(f"[deepgram_stt] {msg}\n")
    sys.stderr.flush()


# ── Audio constants ───────────────────────────────────────────────────────────
SAMPLE_RATE = 16000
CHANNELS    = 1
SAMPLE_WIDTH = 2          # int16 = 2 bytes/sample
CHUNK_SAMPLES = 1600      # 100 ms at 16 kHz — Deepgram recommended cadence
LEVEL_LOG_EVERY_S = 2.0   # how often to log RMS / peak to stderr


def list_input_devices(pa) -> List[dict]:
    """Returns a list of {index, name, channels, default_rate} for inputs."""
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
            "channels": int(info.get("maxInputChannels", 0)),
            "default_rate": int(info.get("defaultSampleRate", 0)),
            "is_default": (i == default_input_index),
        })
    return out


def pick_device(pa, pref: str) -> Optional[int]:
    """Return device index matching `pref` substring, else default device.
    Logs choices to stderr so the user can see what was selected."""
    inputs = list_input_devices(pa)
    diag(f"available input devices ({len(inputs)}):")
    for d in inputs:
        marker = " [default]" if d["is_default"] else ""
        diag(f"  #{d['index']}  {d['name']}  "
             f"ch={d['channels']} rate={d['default_rate']}{marker}")
    if not inputs:
        return None
    if pref:
        pref_l = pref.lower()
        for d in inputs:
            if pref_l in d["name"].lower():
                diag(f"selected input by FINCEPT_STT_DEVICE='{pref}': "
                     f"#{d['index']} {d['name']}")
                return d["index"]
        diag(f"no device matched FINCEPT_STT_DEVICE='{pref}', "
             f"falling back to system default")
    for d in inputs:
        if d["is_default"]:
            diag(f"selected system default input: #{d['index']} {d['name']}")
            return d["index"]
    diag(f"no default flagged, using first input: #{inputs[0]['index']} "
         f"{inputs[0]['name']}")
    return inputs[0]["index"]


def apply_gain_int16(buf: bytes, gain: float) -> bytes:
    """Multiply each int16 sample by `gain`, clipping to int16 range.
    Used to compensate for low-sensitivity laptop mics."""
    if gain <= 0 or abs(gain - 1.0) < 1e-3:
        return buf
    n = len(buf) // SAMPLE_WIDTH
    if n == 0:
        return buf
    samples = struct.unpack("<" + "h" * n, buf)
    out = []
    for s in samples:
        v = int(s * gain)
        if v > 32767:
            v = 32767
        elif v < -32768:
            v = -32768
        out.append(v)
    return struct.pack("<" + "h" * n, *out)


def rms_int16(buf: bytes) -> float:
    """Root-mean-square in 0..32768 of an int16 PCM buffer."""
    n = len(buf) // SAMPLE_WIDTH
    if n == 0:
        return 0.0
    samples = struct.unpack("<" + "h" * n, buf)
    s = 0
    for v in samples:
        s += v * v
    return math.sqrt(s / n)


def peak_int16(buf: bytes) -> int:
    """Max absolute value in 0..32768 of an int16 PCM buffer."""
    n = len(buf) // SAMPLE_WIDTH
    if n == 0:
        return 0
    samples = struct.unpack("<" + "h" * n, buf)
    p = 0
    for v in samples:
        a = -v if v < 0 else v
        if a > p:
            p = a
    return p


def main() -> None:
    diag("script started — pid=" + str(os.getpid()))

    api_key = os.environ.get("DEEPGRAM_API_KEY", "").strip()
    diag(f"env DEEPGRAM_API_KEY len={len(api_key)}")
    if not api_key:
        emit({"fatal": "DEEPGRAM_API_KEY not set — configure it in Settings -> Voice"})
        sys.exit(1)

    model       = os.environ.get("FINCEPT_STT_MODEL", "nova-3").strip() or "nova-3"
    language    = os.environ.get("FINCEPT_STT_LANGUAGE", "en").strip() or "en"
    keyterms_raw = os.environ.get("FINCEPT_STT_KEYTERMS", "").strip()
    keyterms    = [t.strip() for t in keyterms_raw.split(",") if t.strip()]
    device_pref = os.environ.get("FINCEPT_STT_DEVICE", "").strip()
    try:
        gain = float(os.environ.get("FINCEPT_STT_GAIN", "3.0"))
    except ValueError:
        gain = 3.0
    diag(f"config model={model} language={language} keyterms={keyterms} "
         f"gain={gain} device='{device_pref}'")

    # ── Required deps ────────────────────────────────────────────────────────
    try:
        from deepgram import (
            DeepgramClient,
            DeepgramClientOptions,
            LiveOptions,
            LiveTranscriptionEvents,
        )
        import deepgram as _dg_mod
        diag(f"deepgram-sdk imported, version={getattr(_dg_mod, '__version__', '?')}")
    except ImportError as e:
        diag(f"deepgram import failed: {e}")
        emit({"fatal": (
            "Deepgram Python SDK not available in venv-numpy2 "
            f"({e}). Open Settings -> Python Env -> Reinstall packages."
        )})
        sys.exit(1)

    try:
        import pyaudio
        diag(f"pyaudio imported, version={getattr(pyaudio, '__version__', '?')}")
    except ImportError as e:
        diag(f"pyaudio import failed: {e}")
        emit({"fatal": (
            "PyAudio not available in venv-numpy2. "
            "Open Settings -> Python Env -> Reinstall packages."
        )})
        sys.exit(1)

    # ── Shutdown wiring ──────────────────────────────────────────────────────
    stop_flag = threading.Event()

    def on_signal(_signum, _frame):
        stop_flag.set()
    if hasattr(signal, "SIGTERM"):
        try:
            signal.signal(signal.SIGTERM, on_signal)
        except (ValueError, OSError):
            pass

    # ── Deepgram client ──────────────────────────────────────────────────────
    config = DeepgramClientOptions(options={"keepalive": "true"})
    client = DeepgramClient(api_key, config)
    dg_conn = client.listen.websocket.v("1")

    # ── Transcript buffering ─────────────────────────────────────────────────
    # Accumulate every is_final segment and flush on speech_final / UtteranceEnd.
    # Requiring `is_final && speech_final` per-message dropped mid-utterance
    # words on machines where speech_final fires sparingly.
    finalised_segments: List[str] = []
    finalised_lock = threading.Lock()

    def flush_buffered_text(why: str) -> None:
        with finalised_lock:
            if not finalised_segments:
                return
            text = " ".join(finalised_segments).strip()
            finalised_segments.clear()
        if text:
            diag(f"flushing buffered transcript ({why}): \"{text[:120]}\"")
            emit({"text": text})

    def on_open(_self, _open, **_kw):
        diag("deepgram event: Open")
        emit({"status": "listening"})

    def on_message(_self, result, **_kw):
        try:
            alt = result.channel.alternatives[0]
            transcript = (alt.transcript or "").strip()
            is_final     = bool(getattr(result, "is_final", False))
            speech_final = bool(getattr(result, "speech_final", False))
            if not transcript:
                return
            diag(f"transcript: is_final={is_final} speech_final={speech_final} "
                 f"text=\"{transcript[:80]}\"")
            if is_final:
                with finalised_lock:
                    finalised_segments.append(transcript)
                if speech_final:
                    flush_buffered_text("speech_final")
        except Exception as ex:
            diag(f"on_message exception: {ex}")
            emit({"error": f"Transcript parse failed: {ex}"})

    def on_utterance_end(_self, _payload, **_kw):
        diag("deepgram event: UtteranceEnd")
        flush_buffered_text("utterance_end")

    def on_error(_self, err, **_kw):
        diag(f"deepgram event: Error — {err}")
        emit({"error": f"Deepgram error: {err}"})

    def on_close(_self, _close, **_kw):
        diag("deepgram event: Close — stopping")
        flush_buffered_text("close")
        stop_flag.set()

    def on_unhandled(_self, _unhandled, **_kw):
        pass

    dg_conn.on(LiveTranscriptionEvents.Open, on_open)
    dg_conn.on(LiveTranscriptionEvents.Transcript, on_message)
    # UtteranceEnd is its own event in deepgram-sdk v3 — register it explicitly
    # so we always flush, even when speech_final is sporadic.
    if hasattr(LiveTranscriptionEvents, "UtteranceEnd"):
        dg_conn.on(LiveTranscriptionEvents.UtteranceEnd, on_utterance_end)
    dg_conn.on(LiveTranscriptionEvents.Error, on_error)
    dg_conn.on(LiveTranscriptionEvents.Close, on_close)
    dg_conn.on(LiveTranscriptionEvents.Unhandled, on_unhandled)

    # endpointing of 800 ms (was 300) — gives natural-pace speech room to
    # breathe between phrases without prematurely closing the utterance.
    live_opts = LiveOptions(
        model=model,
        language=language,
        encoding="linear16",
        sample_rate=SAMPLE_RATE,
        channels=CHANNELS,
        punctuate=True,
        smart_format=True,
        interim_results=True,
        endpointing=800,
        vad_events=True,
        utterance_end_ms="1200",
    )
    if keyterms:
        try:
            if model.startswith("nova-3"):
                live_opts.keyterm = keyterms
            else:
                live_opts.keywords = keyterms
        except Exception as ex:
            emit({"error": f"Keyterm setup failed: {ex}"})

    emit({"status": "calibrating"})
    diag("calling dg_conn.start() …")
    if not dg_conn.start(live_opts):
        diag("dg_conn.start() returned False — websocket failed")
        emit({"fatal": "Failed to open Deepgram WebSocket connection"})
        sys.exit(1)
    diag("dg_conn.start() returned True — websocket open")

    # ── Mic capture (raw pyaudio) ────────────────────────────────────────────
    pa = pyaudio.PyAudio()
    device_index = pick_device(pa, device_pref)
    if device_index is None:
        emit({"fatal": "No input audio device available"})
        try:
            dg_conn.finish()
        finally:
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
        dg_conn.finish()
        pa.terminate()
        emit({"fatal": f"Could not open microphone stream: {ex}"})
        sys.exit(1)
    diag(f"audio stream opened — device={device_index} rate={SAMPLE_RATE} "
         f"chunk={CHUNK_SAMPLES} samples ({CHUNK_SAMPLES * 1000 / SAMPLE_RATE:.0f} ms)")

    last_level_log = 0.0
    chunk_count = 0
    sent_bytes  = 0

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

            chunk_count += 1

            # Periodic level diagnostic — RMS and peak (both 0..32768).  Lets
            # the user see whether the mic is even picking up sound.
            now = time.time()
            if now - last_level_log >= LEVEL_LOG_EVERY_S:
                last_level_log = now
                rms = rms_int16(buf)
                peak = peak_int16(buf)
                # rms ~50 = near-silence, ~500-2000 = soft speech, >5000 = loud.
                diag(f"audio level (pre-gain): rms={rms:.0f} peak={peak} "
                     f"chunks_sent={chunk_count} bytes_sent={sent_bytes}")

            # Apply software gain — int16 with clipping.
            if gain != 1.0:
                buf = apply_gain_int16(buf, gain)

            try:
                dg_conn.send(buf)
                sent_bytes += len(buf)
            except Exception as ex:
                diag(f"dg_conn.send failed: {ex}")
                emit({"error": f"Send to Deepgram failed: {ex}"})
                break
    except KeyboardInterrupt:
        pass
    finally:
        diag(f"shutting down — chunks_sent={chunk_count} bytes_sent={sent_bytes}")
        try:
            stream.stop_stream()
            stream.close()
        except Exception:
            pass
        try:
            pa.terminate()
        except Exception:
            pass
        flush_buffered_text("shutdown")
        try:
            dg_conn.finish()
        except Exception:
            pass
        emit({"status": "stopped"})


if __name__ == "__main__":
    main()
