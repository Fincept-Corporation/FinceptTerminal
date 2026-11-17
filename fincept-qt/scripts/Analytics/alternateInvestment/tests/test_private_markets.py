"""Tests for private_markets.py with cited / hand-computed references.

Run:  python -m pytest -q scripts/Analytics/alternateInvestment/tests/test_private_markets.py
"""
import json
import math
import os
import subprocess
import sys
from datetime import date

HERE = os.path.dirname(os.path.abspath(__file__))
ALT = os.path.dirname(HERE)
sys.path.insert(0, ALT)

import private_markets as pm  # noqa: E402


def _flows(rows):
    flows, ignored = pm.parse_flows(rows)
    assert ignored == 0
    return flows


# Microsoft Excel XIRR documentation example (support.microsoft.com, "XIRR function"):
# -10000 on 1 Jan 2008, 2750 on 1 Mar 2008, 4250 on 30 Oct 2008, 3250 on 15 Feb 2009, 2750 on 1 Apr 2009
# -> 0.373362535 (37.34 %).
def test_xirr_excel_reference():
    dates = [date(2008, 1, 1), date(2008, 3, 1), date(2008, 10, 30), date(2009, 2, 15), date(2009, 4, 1)]
    amounts = [-10000, 2750, 4250, 3250, 2750]
    rate, note = pm.xirr(dates, amounts)
    assert abs(rate - 0.373362535) < 1e-8
    assert note == ""


def test_xirr_no_sign_change():
    rate, note = pm.xirr([date(2020, 1, 1), date(2021, 1, 1)], [-100, -50])
    assert rate is None and "money going in and money coming back" in note


def test_xirr_same_day():
    rate, note = pm.xirr([date(2020, 1, 1), date(2020, 1, 1)], [-100, 110])
    assert rate is None and "same day" in note


def test_xirr_same_day_flows_are_netted_in_npv():
    # Two flows on one day plus a later one: -100 and -100 on day 0, +242 one year (365 days) later -> 10 %.
    rate, _ = pm.xirr([date(2021, 1, 1), date(2021, 1, 1), date(2022, 1, 1)], [-100, -100, 220])
    assert abs(rate - 0.10) < 1e-10


def test_periodic_irr_multiple_roots():
    # -100, +230, -132: NPV = 0 at 10 % and 20 % (classic two-root example; (1+r)^2*100 - 230(1+r) + 132 = 0).
    rate, note = pm.periodic_irr([-100, 230, -132])
    assert abs(rate - 0.10) < 1e-10
    assert "More than one rate" in note


def test_private_capital_default_example_matches_analyzer():
    # The Alt Investments "private-capital" analyzer's default example (AltInvestmentsScreen_Forms.cpp).
    # The 2026-11-16 assessment recorded IRR 5.57 %, TVPI 1.33 — the analyzer then valued the NAV "today"
    # (2026-11-16, 365.25-day year: 5.5670 %). Valued on that day here (actual/365) it is 5.563 %; the
    # difference is the day count only:
    flows_then = _flows([
        {"date": "2020-03-31", "kind": "call", "amount": 1000000},
        {"date": "2021-06-30", "kind": "call", "amount": 500000},
        {"date": "2023-12-31", "kind": "distribution", "amount": 600000},
        {"date": "2026-11-16", "kind": "nav", "amount": 1400000},
    ])
    m_then = pm.fund_metrics(flows_then, 2000000)
    assert abs(m_then["irr"] - 0.055669746672335256) < 1e-4
    assert round(m_then["irr"] * 100, 1) == 5.6
    assert round(m_then["tvpi"], 2) == 1.33
    # Today the analyzer values the NAV on its own statement date (2026-06-30). cli.py gives 5.8568 % with a
    # 365.25-day year (base_analytics.py); this module uses Excel's actual/365 (5.8527 %): within 0.01 pp.
    flows = _flows([
        {"date": "2020-03-31", "kind": "call", "amount": 1000000},
        {"date": "2021-06-30", "kind": "call", "amount": 500000},
        {"date": "2023-12-31", "kind": "distribution", "amount": 600000},
        {"date": "2026-06-30", "kind": "nav", "amount": 1400000},
    ])
    m = pm.fund_metrics(flows, 2000000)
    assert abs(m["irr"] - 0.05856840323094921) < 1e-4
    assert round(m["tvpi"], 2) == 1.33
    assert abs(m["dpi"] - 0.4) < 1e-12
    assert abs(m["rvpi"] - 1400000 / 1500000) < 1e-12
    assert m["unfunded"] == 500000 and abs(m["paid_in_pct"] - 0.75) < 1e-12
    assert m["valuation_date"] == "2026-06-30"


def test_private_capital_matches_cli_py():
    # Same example through the existing cli.py analyzer (run as the terminal runs it).
    data = {"cash_flows": [
        {"date": "2020-03-31", "type": "call", "amount": 1000000},
        {"date": "2021-06-30", "type": "call", "amount": 500000},
        {"date": "2023-12-31", "type": "distribution", "amount": 600000},
        {"date": "2026-06-30", "type": "nav", "amount": 1400000}]}
    out = subprocess.run([sys.executable, os.path.join(ALT, "cli.py"), "private-capital", "--data", json.dumps(data),
                          "--method", "metrics"], capture_output=True, text=True, cwd=ALT, timeout=120)
    res = json.loads(out.stdout[out.stdout.find("{"):])
    metrics = res["metrics"]
    flows = _flows([{"date": r["date"], "kind": r["type"], "amount": r["amount"]} for r in data["cash_flows"]])
    mine = pm.fund_metrics(flows)
    assert abs(metrics["irr"] - mine["irr"]) < 1e-4  # 365.25 vs 365-day year only
    for k in ("tvpi", "dpi", "rvpi"):
        assert abs(metrics[k] - mine[k]) < 1e-12


def test_nav_roll_forward():
    flows = _flows([
        {"date": "2020-01-01", "kind": "call", "amount": 100},
        {"date": "2021-12-31", "kind": "nav", "amount": 120},
        {"date": "2022-03-31", "kind": "call", "amount": 50},
        {"date": "2022-06-30", "kind": "distribution", "amount": 30},
    ])
    m = pm.fund_metrics(flows)
    assert m["nav"] == 140  # 120 + 50 - 30
    assert m["valuation_date"] == "2022-06-30"
    assert any("rolled forward for 2" in n for n in m["notes"])


def test_kind_synonyms_and_bad_rows():
    flows, ignored = pm.parse_flows([
        {"date": "2020-01-01", "kind": "Capital Call", "amount": "-100"},
        {"date": "2020-13-01", "kind": "call", "amount": 1},
        {"date": "2020-02-01", "kind": "bonus", "amount": 1},
        {"date": "2021-01-01", "kind": "Fair value", "amount": 110},
    ])
    assert ignored == 2
    assert flows[0] == (date(2020, 1, 1), "call", 100.0)
    assert flows[1][1] == "nav"


# Hand-computed 3-flow PME case: index 100 / 120 / 150 on 2020-01-01 / 2021-01-01 / 2022-01-01;
# call 100, distribution 60, NAV 80.
#   KS-PME = (60*150/120 + 80) / (100*150/100) = (75 + 80) / 150 = 1.033333...
#   Long-Nickels index NAV = 150 - 75 = 75; LN flows -100, +60, +75.
#   PME+ lambda = (150 - 80) / 75 = 0.93333...; flows -100, +56, +80.
#   Direct Alpha flows -150, +75, +80 (compounded to T).
PME_FLOWS = [
    {"date": "2020-01-01", "kind": "call", "amount": 100},
    {"date": "2021-01-01", "kind": "distribution", "amount": 60},
    {"date": "2022-01-01", "kind": "nav", "amount": 80},
]
PME_PRICES = [{"date": "2020-01-01", "close": 100}, {"date": "2021-01-01", "close": 120},
              {"date": "2022-01-01", "close": 150}]


def test_ks_pme_hand_case():
    out = pm.pme(_flows(PME_FLOWS), "TEST", PME_PRICES)
    assert abs(out["ks_pme"] - 155.0 / 150.0) < 1e-12
    assert abs(out["ln_index_nav"] - 75.0) < 1e-12
    assert abs(out["pme_plus_lambda"] - 70.0 / 75.0) < 1e-12
    t1, t2 = 366 / 365.0, 731 / 365.0
    # Each IRR solves its own NPV equation.
    r = out["ln_pme_irr"]
    assert abs(-100 + 60 / (1 + r) ** t1 + 75 / (1 + r) ** t2) < 1e-8
    a = out["direct_alpha"]
    assert abs(-150 + 75 / (1 + a) ** t1 + 80 / (1 + a) ** t2) < 1e-8
    p = out["pme_plus_irr"]
    assert abs(-100 + 56 / (1 + p) ** t1 + 80 / (1 + p) ** t2) < 1e-8
    # The fund's own IRR and the spread over the Long-Nickels PME IRR.
    f = out["fund_irr"]
    assert abs(-100 + 60 / (1 + f) ** t1 + 80 / (1 + f) ** t2) < 1e-8
    assert abs(out["irr_spread_ln"] - (f - r)) < 1e-12
    assert out["series"] and out["series"][-1]["index_equivalent"] == 75.0


def test_pme_index_starts_too_late():
    prices = [{"date": "2021-06-01", "close": 100}, {"date": "2022-01-01", "close": 110}]
    try:
        pm.pme(_flows(PME_FLOWS), "TEST", prices)
        assert False, "expected an InputError"
    except pm.InputError as e:
        assert "starts on" in str(e)


# pypme's documented example (pypme 0.6.2 package description): verbose_xpme(dates=[2015-01-01, 2015-06-12,
# 2016-02-15], cashflows=[-10000, 7500], prices=[100, 120, 100], pme_prices=[100, 150, 100])
# -> 0.5525698793027238 (PME IRR) and 0.19495150355969598 (asset IRR).
def test_pypme_documented_example():
    pme_irr, asset_irr = pm.mpme_pypme([date(2015, 1, 1), date(2015, 6, 12), date(2016, 2, 15)], [-10000, 7500],
                                      [100, 120, 100], [100, 150, 100])
    assert abs(pme_irr - 0.5525698793027238) < 1e-9
    assert abs(asset_irr - 0.19495150355969598) < 1e-9


def test_pacing_takahashi_alexander_by_hand():
    out = pm.pacing({"commitment": 100.0, "life_years": 10, "call_rates": [0.25, 0.5], "bow": 2.0,
                     "growth": 0.10, "yield": 0.0})
    y = out["yearly"]
    # Year 1: C = 0.25 * 100 = 25, D = 0, NAV = 25.
    assert abs(y[0]["calls"] - 25) < 1e-12 and y[0]["distributions"] == 0 and abs(y[0]["nav"] - 25) < 1e-12
    # Year 2: C = 0.5 * 75 = 37.5; RD = (2/10)^2 = 0.04; D = 0.04 * 25 * 1.1 = 1.1; NAV = 27.5 + 37.5 - 1.1 = 63.9.
    assert abs(y[1]["calls"] - 37.5) < 1e-12
    assert abs(y[1]["distributions"] - 1.1) < 1e-12
    assert abs(y[1]["nav"] - 63.9) < 1e-12
    # Year L: RD = 1 -> all grown NAV distributed; NAV = that year's call only.
    last = y[9]
    assert abs(last["nav"] - last["calls"]) < 1e-9
    assert abs(y[0]["unfunded"] - 75) < 1e-12
    assert out["peak_net_outflow"] > 0 and out["peak_year"] is not None


def test_pacing_vintages_add_up():
    one = pm.pacing({"commitment": 100.0, "life_years": 8, "vintages": 1, "years": 10})
    two = pm.pacing({"commitment": 100.0, "life_years": 8, "vintages": 2, "years": 10})
    # Year 2 of a two-vintage programme = fund year 2 + fund year 1.
    assert abs(two["yearly"][1]["calls"] - (one["yearly"][1]["calls"] + one["yearly"][0]["calls"])) < 1e-9


def test_book_pools_and_groups():
    f1 = {"id": "a", "name": "A", "strategy": "buyout", "vintage": 2020, "commitment": 2000000, "currency": "USD",
          "flows": [{"date": "2020-03-31", "kind": "call", "amount": 1000000},
                    {"date": "2026-06-30", "kind": "nav", "amount": 1400000}]}
    f2 = {"id": "b", "name": "B", "strategy": "venture", "vintage": 2021, "commitment": 1000000, "currency": "USD",
          "flows": [{"date": "2021-01-31", "kind": "call", "amount": 500000},
                    {"date": "2025-01-31", "kind": "distribution", "amount": 700000}]}
    out = pm.book([f1, f2])
    p = out["pooled"][0]
    assert p["funds"] == 2 and abs(p["tvpi"] - (1400000 + 700000) / 1500000) < 1e-12
    assert p["commitment"] == 3000000 and p["unfunded"] == 1000000 + 500000
    assert {r["strategy"] for r in out["by_strategy"]} == {"buyout", "venture"}
    assert p["irr"] is not None


def test_timeline_year_end_rows():
    flows = _flows(PME_FLOWS)
    t = pm.timeline(flows)
    years = [r["year"] for r in t["year_end"]]
    assert years == [2020, 2021, 2022]
    assert t["year_end"][0]["nav"] is None  # no statement yet in 2020
    assert abs(t["year_end"][-1]["tvpi"] - 1.4) < 1e-12
    assert t["series"][0]["net_cash"] == -100


def test_cli_stdin_roundtrip():
    inp = json.dumps({"flows": PME_FLOWS, "commitment": 200})
    out = subprocess.run([sys.executable, os.path.join(ALT, "private_markets.py"), "metrics", "--stdin"], input=inp,
                         capture_output=True, text=True, timeout=60)
    res = json.loads(out.stdout)
    assert res["success"] is True and abs(res["data"]["tvpi"] - 1.4) < 1e-12
    bad = subprocess.run([sys.executable, os.path.join(ALT, "private_markets.py"), "metrics", "--stdin"],
                         input=json.dumps({"flows": []}), capture_output=True, text=True, timeout=60)
    res = json.loads(bad.stdout)
    assert res["success"] is False and "Add the fund" in res["error"]
