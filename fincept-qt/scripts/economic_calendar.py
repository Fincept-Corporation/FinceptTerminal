"""Forex Factory economic calendar for one day of the current week.

Source: Forex Factory's public weekly calendar export
(https://nfs.faireconomy.media/ff_calendar_thisweek.json) - a plain JSON file, so no browser, driver
or API key is needed. The export covers the current week only and publishes forecast and previous
values, not actuals.

Usage:
    python economic_calendar.py [date]
        date: Forex Factory day format ("nov9.2026") or ISO ("2026-11-09"); default: today.
        The day and the times are this computer's local time zone.

Output (JSON on stdout; {"error": ...} on failure):
    {"date", "url", "events_count",
     "events": [{"time", "currency", "impact", "impact_label", "event", "forecast", "previous",
                 "datetime"}],
     "week_start", "week_end", "timezone", "source", "note"}
    impact: 3 = High, 2 = Medium, 1 = Low, 0 = Holiday / non-economic
"""

import json
import os
import re
import sys
import tempfile
import time
from datetime import date, datetime

import requests

FEED_URL = "https://nfs.faireconomy.media/ff_calendar_thisweek.json"
CALENDAR_URL = "https://www.forexfactory.com/calendar"
IMPACT_LEVELS = {"high": 3, "medium": 2, "low": 1}
MONTHS = ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"]

# The export is one file for the whole week, refreshed about hourly, and it answers repeated
# downloads with HTTP 429 - so every day of the week is served from one cached copy.
CACHE_PATH = os.path.join(tempfile.gettempdir(), "fincept_ff_calendar_thisweek.json")
CACHE_FRESH_SECONDS = 15 * 60
CACHE_FALLBACK_SECONDS = 6 * 60 * 60  # rate-limited / offline: a few hours old is still this week


def _read_cache(max_age: float):
    try:
        age = time.time() - os.path.getmtime(CACHE_PATH)
        if age <= max_age:
            with open(CACHE_PATH, encoding="utf-8") as f:
                return json.load(f), age
    except (OSError, ValueError):
        pass
    return None, None


def load_feed():
    """-> (feed list, age of the copy in seconds)."""
    feed, age = _read_cache(CACHE_FRESH_SECONDS)
    if feed is not None:
        return feed, age
    try:
        resp = requests.get(FEED_URL, timeout=20, headers={"User-Agent": "Fincept-Terminal/1.0"})
        resp.raise_for_status()
        feed = resp.json()
    except requests.exceptions.RequestException:
        feed, age = _read_cache(CACHE_FALLBACK_SECONDS)
        if feed is not None:
            return feed, age
        raise
    try:
        tmp = CACHE_PATH + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(feed, f)
        os.replace(tmp, CACHE_PATH)
    except OSError:
        pass  # caching is an optimisation only
    return feed, 0.0


def parse_day(text: str) -> date:
    """'nov9.2026' (Forex Factory day format) or '2026-11-09' -> date."""
    t = text.strip().lower()
    m = re.fullmatch(r"([a-z]{3})(\d{1,2})\.(\d{4})", t)
    if m and m.group(1) in MONTHS:
        return date(int(m.group(3)), MONTHS.index(m.group(1)) + 1, int(m.group(2)))
    try:
        return date.fromisoformat(t)
    except ValueError:
        raise ValueError(f"Unrecognised date '{text}': use e.g. nov9.2026 or 2026-11-09") from None


def utc_offset_label(dt: datetime) -> str:
    off = dt.utcoffset()
    minutes = int(off.total_seconds() // 60) if off is not None else 0
    sign = "+" if minutes >= 0 else "-"
    return f"UTC{sign}{abs(minutes) // 60:02d}:{abs(minutes) % 60:02d}"


def calendar_for_day(day: date) -> dict:
    feed, feed_age = load_feed()
    if not isinstance(feed, list):
        raise ValueError("Unexpected Forex Factory feed format")

    events, local_days = [], []
    for item in feed:
        if not isinstance(item, dict) or not item.get("date"):
            continue
        # e.g. "2026-11-09T08:30:00-05:00" (US Eastern) -> this computer's local time
        local = datetime.fromisoformat(item["date"]).astimezone()
        local_days.append(local.date())
        if local.date() != day:
            continue
        label = str(item.get("impact") or "")
        events.append({
            "time": local.strftime("%H:%M"),
            "currency": item.get("country", ""),
            "impact": IMPACT_LEVELS.get(label.lower(), 0),
            "impact_label": label,
            "event": item.get("title", ""),
            "forecast": item.get("forecast", ""),
            "previous": item.get("previous", ""),
            "datetime": local.isoformat(timespec="minutes"),
        })
    if not local_days:
        raise ValueError("The Forex Factory weekly feed is empty")
    week_start, week_end = min(local_days), max(local_days)
    if not week_start <= day <= week_end:
        raise ValueError(f"Forex Factory's public calendar feed covers this week only "
                         f"({week_start.isoformat()} to {week_end.isoformat()})")
    events.sort(key=lambda e: e["datetime"])
    return {
        "date": day.isoformat(),
        "url": f"{CALENDAR_URL}?day={MONTHS[day.month - 1]}{day.day}.{day.year}",
        "events_count": len(events),
        "events": events,
        "week_start": week_start.isoformat(),
        "week_end": week_end.isoformat(),
        "timezone": utc_offset_label(datetime.now().astimezone()),
        "source": FEED_URL,
        "feed_age_seconds": int(feed_age),
        "note": "Forex Factory weekly export: current week only; forecast and previous, no actuals",
    }


def main(argv) -> dict:
    try:
        day = parse_day(argv[1]) if len(argv) > 1 else date.today()
        return calendar_for_day(day)
    except requests.exceptions.RequestException as e:
        return {"error": f"Forex Factory calendar feed unavailable: {str(e)[:300]}"}
    except (ValueError, KeyError, TypeError) as e:
        return {"error": str(e)[:300]}


if __name__ == "__main__":
    print(json.dumps(main(sys.argv), indent=2))
