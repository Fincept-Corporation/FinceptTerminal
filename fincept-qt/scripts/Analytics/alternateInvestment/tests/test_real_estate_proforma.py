"""Tests for real_estate_proforma.py (hand-checked spreadsheet cases; no network).

Run:  python -m pytest -q scripts/Analytics/alternateInvestment/tests/test_real_estate_proforma.py
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ALT = os.path.dirname(HERE)
sys.path.insert(0, ALT)

import real_estate_proforma as re_pf  # noqa: E402
from private_markets import InputError  # noqa: E402


def test_annuity_reference():
    # Standard mortgage formula: 1,000,000 at 6 % over 30 years, monthly payment 5,995.51.
    assert round(re_pf.monthly_payment(1000000, 0.06, 30), 2) == 5995.51
    # 200,000 at 5 % over 15 years: 1,581.59 (same formula, a second table value).
    assert round(re_pf.monthly_payment(200000, 0.05, 15), 2) == 1581.59


def test_selftest_passes():
    res = re_pf.selftest()
    assert res["passed"], res


def test_flat_cases_closed_form():
    base = {"price": 1000000, "closing_costs": 0, "gross_rent": 60000, "rent_growth": 0, "vacancy": 0, "opex": 0,
            "opex_growth": 0, "hold_years": 5, "exit_cap": 0.06, "selling_costs": 0, "ltv": 0}
    s = re_pf.proforma(base)["summary"]
    assert abs(s["unlevered_irr"] - 0.06) < 1e-10 and abs(s["going_in_cap"] - 0.06) < 1e-12
    lev = dict(base, ltv=0.5, interest_rate=0.04, amort_years=0)
    out = re_pf.proforma(lev)
    s = out["summary"]
    assert abs(s["levered_irr"] - 0.08) < 1e-10
    assert abs(s["cash_on_cash_y1"] - 0.08) < 1e-12
    assert abs(s["ltv"] - 0.5) < 1e-12
    assert all(abs(r["debt_service"] - 20000) < 1e-6 for r in out["years"])


def test_operating_lines_year_two():
    inp = {"price": 2000000, "closing_costs": 0.01, "gross_rent": 200000, "other_income": 10000, "rent_growth": 0.03,
           "vacancy": 0.05, "opex": 60000, "opex_growth": 0.02, "mgmt_fee": 0.04, "capex_reserve": 5000,
           "ltv": 0.6, "interest_rate": 0.055, "amort_years": 25, "io_years": 1, "hold_years": 7,
           "exit_cap": 0.065, "selling_costs": 0.02}
    out = re_pf.proforma(inp)
    y2 = out["years"][1]
    gpr = 200000 * 1.03
    other = 10000 * 1.03
    egi = (gpr + other) * 0.95
    noi = egi - 60000 * 1.02 - 0.04 * egi
    assert abs(y2["egi"] - egi) < 1e-6 and abs(y2["noi"] - noi) < 1e-6
    # Year 1 interest only: 12 x 1,200,000 x 5.5 % / 12 = 66,000.
    assert abs(out["years"][0]["debt_service"] - 66000) < 1e-6
    # From year 2: the amortising payment of 1,200,000 over 25 years at 5.5 %.
    pay = re_pf.monthly_payment(1200000, 0.055, 25)
    assert abs(y2["debt_service"] - 12 * pay) < 1e-6
    # Exit on year-8 NOI.
    s = out["summary"]
    assert abs(s["sale_price"] - s["exit_noi"] / 0.065) < 1e-6
    sens = out["sensitivity"]
    assert len(sens["values"]) == 5 and len(sens["values"][0]) == 5
    assert abs(sens["values"][sens["base_row"]][sens["base_col"]] - s["levered_irr"]) < 1e-12
    # A lower exit cap (higher price) can only raise the IRR; faster rent growth too.
    assert sens["values"][0][2] > sens["values"][4][2]
    assert sens["values"][2][4] > sens["values"][2][0]


def test_input_errors():
    for bad, text in (({"price": 0, "gross_rent": 1}, "purchase price"),
                      ({"price": 100, "gross_rent": 0}, "gross potential rent"),
                      ({"price": 100, "gross_rent": 10, "ltv": 1.2}, "LTV")):
        try:
            re_pf.proforma(bad)
            assert False
        except InputError as e:
            assert text in str(e)


def test_cli_stdin_and_selftest_flag():
    inp = json.dumps({"price": 1000000, "gross_rent": 90000, "opex": 25000, "ltv": 0.55, "interest_rate": 0.06,
                      "hold_years": 10, "exit_cap": 0.0625})
    out = subprocess.run([sys.executable, os.path.join(ALT, "real_estate_proforma.py"), "proforma", "--stdin"],
                         input=inp, capture_output=True, text=True, timeout=60)
    res = json.loads(out.stdout)
    assert res["success"] and res["data"]["summary"]["levered_irr"] is not None
    st = subprocess.run([sys.executable, os.path.join(ALT, "real_estate_proforma.py"), "--selftest"],
                        capture_output=True, text=True, timeout=60)
    assert st.returncode == 0 and json.loads(st.stdout)["data"]["passed"]
