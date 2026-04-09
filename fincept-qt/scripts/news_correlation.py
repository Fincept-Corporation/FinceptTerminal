"""
News Correlation Engine — Signal detection, Country Instability Index,
baseline tracking, and deviation analysis.

Commands:
  detect_signals <json_articles> <json_market_data>
  compute_instability <country_code> <json_signals>
  baseline_update <json_category_counts> <json_existing_baseline>
  detect_deviations <json_current_counts> <json_baseline>
  focal_points <json_geolocated_articles>
"""
import sys
import json
import math
import time
from collections import Counter, defaultdict


# ── Signal Types ─────────────────────────────────────────────────────────────

SIGNAL_TYPES = [
    "velocity_spike",        # 3x+ normal volume in category
    "keyword_spike",         # trending term above baseline
    "news_leads_markets",    # negative news before price drop
    "silent_divergence",     # market moves without news
    "convergence",           # multiple signals in same location
    "triangulation",         # entity in 3+ sources
    "sector_cascade",        # sector-wide market move + news
    "hotspot_escalation",    # conflict zone activity surge
    "military_surge",        # military keyword spike
    "prediction_leads_news", # prediction market odds shift before news
    "geo_convergence",       # multiple events in same geography
    "flow_divergence",       # capital flow vs price divergence
]

# ── Threat keywords by category ──────────────────────────────────────────────

CATEGORY_KEYWORDS = {
    "conflict": ["war", "attack", "missile", "strike", "troops", "invasion",
                 "airstrike", "bombing", "military", "combat", "weapon",
                 "artillery", "drone", "casualties", "killed", "wounded"],
    "cyber": ["cyberattack", "hack", "breach", "ransomware", "malware",
              "vulnerability", "exploit", "data leak", "phishing"],
    "natural": ["earthquake", "tsunami", "hurricane", "typhoon", "flood",
                "wildfire", "drought", "tornado", "volcanic", "pandemic"],
    "market": ["crash", "selloff", "sell-off", "circuit breaker", "trading halt",
               "flash crash", "margin call", "bankruptcy", "default", "downgrade"],
    "regulatory": ["sanction", "tariff", "ban", "regulation", "antitrust",
                   "investigation", "fine", "penalty", "embargo", "restriction"],
}


def detect_signals(articles_json, market_json=None):
    """Detect correlation signals from news articles and optional market data."""
    try:
        articles = json.loads(articles_json) if isinstance(articles_json, str) else articles_json
        market_data = json.loads(market_json) if market_json and isinstance(market_json, str) else (market_json or {})
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    signals = []
    now = int(time.time())
    hour_ago = now - 3600
    two_hours_ago = now - 7200

    # Count articles per category in recent window
    recent_counts = Counter()
    older_counts = Counter()
    source_per_headline = defaultdict(set)
    location_events = defaultdict(list)
    keyword_counts = Counter()

    for a in articles:
        ts = a.get("sort_ts", 0)
        cat = a.get("category", "GENERAL")
        headline = a.get("headline", "").lower()

        if ts >= hour_ago:
            recent_counts[cat] += 1
        elif ts >= two_hours_ago:
            older_counts[cat] += 1

        # Track sources per headline (for triangulation)
        key = headline[:50]
        source_per_headline[key].add(a.get("source", ""))

        # Track locations (for geo_convergence)
        region = a.get("region", "")
        if region:
            location_events[region].append(a)

        # Keyword counting
        for cat_name, keywords in CATEGORY_KEYWORDS.items():
            for kw in keywords:
                if kw in headline:
                    keyword_counts[kw] += 1

    # Signal 1: velocity_spike — category has 3x+ recent vs older volume
    for cat in recent_counts:
        recent = recent_counts[cat]
        older = max(older_counts.get(cat, 0), 1)
        ratio = recent / older
        if ratio >= 3.0 and recent >= 5:
            signals.append({
                "type": "velocity_spike",
                "category": cat,
                "detail": f"{cat}: {recent} articles in 1h vs {older} in prior hour ({ratio:.1f}x)",
                "severity": "high" if ratio >= 5 else "medium",
                "value": round(ratio, 1),
            })

    # Signal 2: triangulation — same story in 3+ sources
    for headline_key, sources in source_per_headline.items():
        if len(sources) >= 3:
            signals.append({
                "type": "triangulation",
                "detail": f"'{headline_key[:40]}...' reported by {len(sources)} sources",
                "severity": "medium",
                "sources": list(sources)[:5],
                "value": len(sources),
            })

    # Signal 3: keyword_spike — any threat keyword appearing 5+ times
    for kw, count in keyword_counts.most_common(10):
        if count >= 5:
            # Find which category this keyword belongs to
            kw_cat = "general"
            for cat_name, keywords in CATEGORY_KEYWORDS.items():
                if kw in keywords:
                    kw_cat = cat_name
                    break
            signals.append({
                "type": "keyword_spike",
                "keyword": kw,
                "category": kw_cat,
                "detail": f"'{kw}' mentioned {count} times in current feed",
                "severity": "high" if count >= 10 else "medium",
                "value": count,
            })

    # Signal 4: geo_convergence — 5+ events in same region
    for region, events in location_events.items():
        if len(events) >= 5:
            categories = Counter(e.get("category", "") for e in events)
            if len(categories) >= 2:  # Multiple categories = convergence
                signals.append({
                    "type": "geo_convergence",
                    "region": region,
                    "detail": f"{region}: {len(events)} events across {len(categories)} categories",
                    "severity": "high",
                    "categories": dict(categories),
                    "value": len(events),
                })

    # Signal 5: hotspot_escalation — conflict keywords surge
    conflict_count = sum(keyword_counts.get(kw, 0) for kw in CATEGORY_KEYWORDS["conflict"])
    if conflict_count >= 10:
        signals.append({
            "type": "hotspot_escalation",
            "detail": f"{conflict_count} conflict-related mentions in feed",
            "severity": "critical" if conflict_count >= 20 else "high",
            "value": conflict_count,
        })

    # Signal 6: military_surge
    mil_keywords = ["military", "troops", "missile", "weapon", "artillery", "drone"]
    mil_count = sum(keyword_counts.get(kw, 0) for kw in mil_keywords)
    if mil_count >= 8:
        signals.append({
            "type": "military_surge",
            "detail": f"{mil_count} military-related mentions",
            "severity": "high",
            "value": mil_count,
        })

    # Sort by severity
    severity_order = {"critical": 0, "high": 1, "medium": 2, "low": 3}
    signals.sort(key=lambda s: severity_order.get(s.get("severity", "low"), 3))

    return {
        "success": True,
        "signals": signals,
        "signal_count": len(signals),
        "categories_active": dict(recent_counts),
    }


def compute_instability(country_code, signals_json):
    """Compute Country Instability Index (0-100) from multi-signal blend."""
    try:
        signals = json.loads(signals_json) if isinstance(signals_json, str) else signals_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    # Baseline weights per signal type
    weights = {
        "conflict": 25,      # max 25 points from conflict signals
        "cyber": 10,         # max 10 from cyber threats
        "natural": 15,       # max 15 from natural disasters
        "market": 15,        # max 15 from market stress
        "regulatory": 10,    # max 10 from regulatory action
        "velocity_spike": 10,  # max 10 from velocity spikes
        "triangulation": 5,   # max 5 from multi-source confirmation
        "hotspot_escalation": 10, # max 10 from hotspot escalation
    }

    # Known baseline adjustments (tier-1 nations have higher baseline stability)
    tier1_nations = {"US", "GB", "DE", "FR", "JP", "CA", "AU", "KR", "SG", "CH",
                     "NL", "SE", "NO", "DK", "FI", "NZ", "AT", "BE", "IE", "LU"}
    baseline = 10 if country_code in tier1_nations else 25  # starting instability

    # Accumulate from signals
    signal_scores = defaultdict(float)
    for signal in signals:
        s_type = signal.get("type", "")
        severity = signal.get("severity", "low")
        value = signal.get("value", 1)

        severity_mult = {"critical": 1.0, "high": 0.7, "medium": 0.4, "low": 0.2}.get(severity, 0.1)

        if s_type in weights:
            max_w = weights[s_type]
            score = min(max_w, value * severity_mult * 5)
            signal_scores[s_type] = max(signal_scores[s_type], score)

        # Category-based scoring
        category = signal.get("category", "")
        if category in weights:
            max_w = weights[category]
            score = min(max_w, value * severity_mult * 3)
            signal_scores[category] = max(signal_scores[category], score)

    total = baseline + sum(signal_scores.values())
    cii = min(100, max(0, int(total)))

    # Risk level
    if cii >= 75:
        level = "CRITICAL"
    elif cii >= 50:
        level = "HIGH"
    elif cii >= 30:
        level = "ELEVATED"
    else:
        level = "STABLE"

    return {
        "success": True,
        "country": country_code,
        "cii_score": cii,
        "level": level,
        "baseline": baseline,
        "signal_contributions": dict(signal_scores),
        "total_signals": len(signals),
    }


def baseline_update(current_json, existing_json):
    """Update rolling 7-day hourly baseline with current counts."""
    try:
        current = json.loads(current_json) if isinstance(current_json, str) else current_json
        existing = json.loads(existing_json) if isinstance(existing_json, str) else existing_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    updated = {}
    for category, count in current.items():
        prev = existing.get(category, {"hourly_counts": [], "mean": 0, "stddev": 0})
        counts = prev.get("hourly_counts", [])
        counts.append(count)

        # Keep last 168 hours (7 days)
        if len(counts) > 168:
            counts = counts[-168:]

        n = len(counts)
        mean = sum(counts) / max(n, 1)
        variance = sum((c - mean) ** 2 for c in counts) / max(n, 1)
        stddev = math.sqrt(variance)

        updated[category] = {
            "hourly_counts": counts,
            "mean": round(mean, 2),
            "stddev": round(stddev, 2),
            "sample_size": n,
        }

    return {"success": True, "baselines": updated}


def detect_deviations(current_json, baseline_json):
    """Detect z-score deviations from baseline."""
    try:
        current = json.loads(current_json) if isinstance(current_json, str) else current_json
        baseline = json.loads(baseline_json) if isinstance(baseline_json, str) else baseline_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    deviations = []
    for category, count in current.items():
        bl = baseline.get(category, {})
        mean = bl.get("mean", 0)
        stddev = bl.get("stddev", 0)
        sample_size = bl.get("sample_size", 0)

        if sample_size < 24 or stddev < 0.5:
            continue

        z_score = (count - mean) / stddev
        if z_score >= 2.0:
            deviations.append({
                "category": category,
                "current": count,
                "baseline_mean": mean,
                "z_score": round(z_score, 2),
                "multiplier": round(count / max(mean, 0.1), 1),
                "severity": "critical" if z_score >= 4 else ("high" if z_score >= 3 else "medium"),
            })

    deviations.sort(key=lambda d: d["z_score"], reverse=True)

    return {"success": True, "deviations": deviations, "count": len(deviations)}


def focal_points(articles_json):
    """Detect geographic focal points — convergence of events in same area."""
    try:
        articles = json.loads(articles_json) if isinstance(articles_json, str) else articles_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    # Group by approximate grid (1 degree = ~111km)
    grid = defaultdict(list)
    for a in articles:
        lat = a.get("primary_lat")
        lon = a.get("primary_lon")
        if lat is not None and lon is not None:
            key = (round(float(lat)), round(float(lon)))
            grid[key].append(a)

    focal = []
    for (lat, lon), events in grid.items():
        if len(events) >= 3:
            categories = Counter()
            sources = set()
            for e in events:
                categories[e.get("category", "GENERAL")] += 1
                sources.add(e.get("source", ""))

            focal.append({
                "lat": lat,
                "lon": lon,
                "event_count": len(events),
                "categories": dict(categories),
                "source_count": len(sources),
                "severity": "critical" if len(events) >= 10 else ("high" if len(events) >= 5 else "medium"),
                "headlines": [e.get("headline", "")[:60] for e in events[:5]],
            })

    focal.sort(key=lambda f: f["event_count"], reverse=True)

    return {"success": True, "focal_points": focal[:10], "count": len(focal)}


def resolve_arg(arg):
    """If arg starts with '@', read content from that file path and delete it."""
    if arg and arg.startswith("@"):
        path = arg[1:]
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = f.read()
            try:
                import os
                os.remove(path)
            except Exception:
                pass
            return data
        except Exception:
            return arg  # fallback: return as-is
    return arg


def main(args=None):
    if args is None:
        args = sys.argv[1:]

    if len(args) < 2:
        print(json.dumps({"success": False, "error": "Usage: news_correlation.py <command> <args...>"}))
        return

    command = args[0]

    if command == "detect_signals":
        market = resolve_arg(args[2]) if len(args) > 2 else None
        result = detect_signals(resolve_arg(args[1]), market)
    elif command == "compute_instability":
        if len(args) < 3:
            result = {"success": False, "error": "Usage: compute_instability <country_code> <json_signals>"}
        else:
            result = compute_instability(args[1], resolve_arg(args[2]))
    elif command == "baseline_update":
        if len(args) < 3:
            result = {"success": False, "error": "Usage: baseline_update <current> <existing>"}
        else:
            result = baseline_update(resolve_arg(args[1]), resolve_arg(args[2]))
    elif command == "detect_deviations":
        if len(args) < 3:
            result = {"success": False, "error": "Usage: detect_deviations <current> <baseline>"}
        else:
            result = detect_deviations(resolve_arg(args[1]), resolve_arg(args[2]))
    elif command == "focal_points":
        result = focal_points(resolve_arg(args[1]))
    else:
        result = {"success": False, "error": f"Unknown command: {command}"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
