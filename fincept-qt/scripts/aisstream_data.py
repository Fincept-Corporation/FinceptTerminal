"""
AISStream Data Fetcher
Live AIS vessel positions and ship static data (name, type, destination, ETA) from the AISStream.io WebSocket feed, collected for a few seconds into a snapshot (free key: AISSTREAM_API_KEY).
"""
import sys
import json
import os
import time
from typing import Dict, Any, List

API_KEY = os.environ.get('AISSTREAM_API_KEY', '')
# AISStream has no REST API (api.aisstream.io never resolved); data is only served over this WebSocket.
STREAM_URL = "wss://stream.aisstream.io/v0/stream"
MAX_SECONDS = 60


def _collect(bboxes: List, message_types: List[str], seconds: float, mmsi: List[str] = None,
             max_messages: int = 2000) -> Any:
    if not API_KEY:
        return {"error": "AISStream needs a free API key: sign in at https://aisstream.io/apikeys and save it as "
                         "AISSTREAM_API_KEY in Settings > Credentials"}
    try:
        import websocket  # websocket-client
    except ImportError:
        return {"error": "Missing dependency: websocket-client"}
    seconds = max(1.0, min(float(seconds), MAX_SECONDS))
    sub = {"APIKey": API_KEY, "BoundingBoxes": bboxes, "FilterMessageTypes": message_types}
    if mmsi:
        sub["FiltersShipMMSI"] = mmsi
    messages = []
    try:
        ws = websocket.create_connection(STREAM_URL, timeout=10)
    except Exception as e:  # network / TLS failures
        return {"error": f"Could not connect to AISStream: {e}"}
    try:
        ws.send(json.dumps(sub))
        deadline = time.time() + seconds
        while time.time() < deadline and len(messages) < max_messages:
            ws.settimeout(max(0.1, deadline - time.time()))
            try:
                raw = ws.recv()
            except websocket.WebSocketTimeoutException:
                break
            except websocket.WebSocketConnectionClosedException:
                if not messages:
                    return {"error": "AISStream closed the connection without data - check AISSTREAM_API_KEY "
                                     "(an invalid key is rejected this way)"}
                break
            try:
                msg = json.loads(raw)
            except (TypeError, ValueError):
                continue
            if isinstance(msg, dict) and msg.get("error"):
                return {"error": f"AISStream: {msg['error']}"}
            messages.append(msg)
    finally:
        try:
            ws.close()
        except Exception:
            pass
    return messages


def _vessels(messages: List[Dict]) -> List[Dict]:
    out: Dict[str, Dict] = {}
    for m in messages:
        meta = m.get("MetaData", {})
        key = str(meta.get("MMSI"))
        v = out.setdefault(key, {"mmsi": meta.get("MMSI")})
        if meta.get("ShipName", "").strip():
            v["name"] = meta["ShipName"].strip()
        body = m.get("Message", {}).get(m.get("MessageType", ""), {})
        if m.get("MessageType") in ("PositionReport", "StandardClassBPositionReport"):
            v.update({"lat": body.get("Latitude", meta.get("latitude")), "lon": body.get("Longitude", meta.get("longitude")),
                      "sog_knots": body.get("Sog"), "cog": body.get("Cog"), "heading": body.get("TrueHeading"),
                      "nav_status": body.get("NavigationalStatus"), "time_utc": meta.get("time_utc")})
        elif m.get("MessageType") == "ShipStaticData":
            eta = body.get("Eta") or {}
            v.update({"imo": body.get("ImoNumber"), "call_sign": (body.get("CallSign") or "").strip(),
                      "ship_type": body.get("Type"), "destination": (body.get("Destination") or "").strip(),
                      "draught_m": body.get("MaximumStaticDraught"),
                      "eta": f"{eta.get('Month', 0):02d}-{eta.get('Day', 0):02d} {eta.get('Hour', 0):02d}:{eta.get('Minute', 0):02d}" if eta else None})
    return list(out.values())


def get_vessels_in_area(lat1: float, lon1: float, lat2: float, lon2: float, seconds: float = 15) -> Any:
    """Vessels reporting positions inside a bounding box during a short listening window."""
    msgs = _collect([[[float(lat1), float(lon1)], [float(lat2), float(lon2)]]],
                    ["PositionReport", "StandardClassBPositionReport", "ShipStaticData"], seconds)
    if isinstance(msgs, dict):
        return msgs
    vessels = _vessels(msgs)
    return {"bbox": [[lat1, lon1], [lat2, lon2]], "seconds": seconds, "messages": len(msgs),
            "count": len(vessels), "vessels": vessels, "source": "AISStream.io (live)"}


def get_vessel_by_mmsi(mmsi: str, seconds: float = 30) -> Any:
    """Latest position/static data for one MMSI heard during the listening window (worldwide)."""
    msgs = _collect([[[-90, -180], [90, 180]]], ["PositionReport", "StandardClassBPositionReport", "ShipStaticData"],
                    seconds, mmsi=[str(mmsi)])
    if isinstance(msgs, dict):
        return msgs
    vessels = _vessels(msgs)
    if not vessels:
        return {"error": f"No AIS messages from MMSI {mmsi} within {seconds}s (vessel may be out of coverage or silent)"}
    return {"vessel": vessels[0], "messages": len(msgs), "source": "AISStream.io (live)"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    try:
        if command == "area":
            # default: approaches to Rotterdam
            lat1 = args[1] if len(args) > 1 else "51.8"
            lon1 = args[2] if len(args) > 2 else "3.8"
            lat2 = args[3] if len(args) > 3 else "52.1"
            lon2 = args[4] if len(args) > 4 else "4.4"
            seconds = float(args[5]) if len(args) > 5 else 15
            result = get_vessels_in_area(lat1, lon1, lat2, lon2, seconds)
        elif command == "vessel":
            if len(args) < 2:
                result = {"error": "Usage: vessel <mmsi> [seconds]"}
            else:
                seconds = float(args[2]) if len(args) > 2 else 30
                result = get_vessel_by_mmsi(args[1], seconds)
    except ValueError as e:
        result = {"error": f"Invalid argument: {e}"}
    print(json.dumps(result))


if __name__ == "__main__":
    main()
