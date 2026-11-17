"""Reference checks for merger_models/accretion_engine.py (run: python -m unittest discover -s <this dir>).

Hand-computed reference case (the M&A tab's default deal, all-debt-funded cash part):
    acquirer NI 150,000,000 / 200,000,000 shares        -> standalone EPS 0.75
    price 500,000,000, 60% cash -> cash 300,000,000 (new debt at 6% -> interest 18,000,000),
                                   stock 200,000,000 at 15.00 -> 13,333,333.33 new shares
    target NI 40,000,000, tax 21%, no synergies
    pro forma NI = 150,000,000 + 40,000,000 - 18,000,000 x 0.79 = 175,780,000
    pro forma EPS = 175,780,000 / 213,333,333.33 = 0.823968750
    accretion = 0.823968750 / 0.75 - 1 = +9.86250%
Breakeven check (synergies phased 100% in year 1): with a 25x P/E price on a 0.75 EPS acquirer the deal dilutes;
the closed-form breakeven synergies must make year-1 EPS change exactly zero.
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..", "..")))  # scripts/Analytics

from corporateFinance.merger_models.accretion_engine import (  # noqa: E402
    AccretionInputError, analyze, normalize, _year)

BASE = {
    "acquirer": {"revenue": 1e9, "ebitda": 250e6, "net_income": 150e6, "shares": 200e6, "share_price": 15.0},
    "target": {"revenue": 300e6, "net_income": 40e6},
    "deal_value": 500e6, "premium": 0.30, "cash_pct": 0.60,
    "cost_of_debt": 0.06, "cash_yield": 0.0, "tax_rate": 0.21,
    "synergies": 0.0, "synergy_phase": [1.0, 1.0, 1.0],
}


class AccretionEngineTest(unittest.TestCase):
    def test_reference_case(self):
        r = analyze(BASE)
        ad = r["accretion_dilution"]
        self.assertAlmostEqual(ad["standalone_eps"], 0.75, places=12)
        self.assertAlmostEqual(r["deal_overview"]["new_shares_issued"], 200e6 / 15.0, places=4)
        self.assertAlmostEqual(ad["pro_forma_eps"], 175_780_000 / (200e6 + 200e6 / 15.0), places=12)
        self.assertAlmostEqual(ad["accretion_dilution_pct"], 9.8625, places=6)
        self.assertEqual(ad["status"], "Accretive")

    def test_bridge_reconciles(self):
        p = dict(BASE, synergies=30e6, synergy_phase=[0.5, 1.0, 1.0], integration_costs_y1=12e6,
                 amortization=5e6, cash_from_balance_sheet=100e6, cash_yield=0.04)
        r = analyze(p)
        steps = r["eps_bridge"]
        total = steps[0]["value"] + sum(s["value"] for s in steps[1:-1])
        self.assertAlmostEqual(total, steps[-1]["value"], places=12)
        self.assertAlmostEqual(steps[-1]["value"], r["accretion_dilution"]["pro_forma_eps"], places=12)
        # Hand figures for this case: synergies after tax per standalone share = 15e6 x 0.79 / 200e6
        self.assertAlmostEqual(steps[2]["value"], 15e6 * 0.79 / 200e6, places=12)
        # interest: (300e6 - 100e6) x 6% = 12e6 ; forgone on cash: 100e6 x 4% = 4e6
        self.assertAlmostEqual(steps[5]["value"], -12e6 * 0.79 / 200e6, places=12)
        self.assertAlmostEqual(steps[6]["value"], -4e6 * 0.79 / 200e6, places=12)

    def test_breakeven_is_eps_neutral(self):
        p = dict(BASE, deal_value=25 * 40e6, cash_pct=0.0)  # all stock at 25x target earnings vs 20x acquirer P/E
        r = analyze(p)
        self.assertLess(r["accretion_dilution"]["eps_change"], 0)
        be = r["breakeven_synergies"]
        self.assertIsNotNone(be)
        x = normalize(p)
        y = _year(x, 1, x["price"], x["cash_pct"], be)
        self.assertAlmostEqual(y["eps_change"], 0.0, places=12)

    def test_sensitivity_base_cell_matches_headline(self):
        p = dict(BASE, synergies=20e6)
        r = analyze(p)
        s = r["sensitivity"]
        self.assertAlmostEqual(s["accretion_premium_synergies"][s["base_row"]][s["base_col"]],
                               r["accretion_dilution"]["accretion_dilution_pct"], places=9)
        self.assertAlmostEqual(s["accretion_premium_cash"][s["base_row"]][s["base_col_cash"]],
                               r["accretion_dilution"]["accretion_dilution_pct"], places=9)
        self.assertAlmostEqual(s["row_values"][s["base_row"]], 30.0, places=9)
        # Premium 40% (one row down): price = 500e6 / 1.3 x 1.4
        price = 500e6 / 1.3 * 1.4
        x = normalize(p)
        self.assertAlmostEqual(s["accretion_premium_synergies"][s["base_row"] + 1][s["base_col"]],
                               _year(x, 1, price, 0.6, 20e6)["accretion_dilution_pct"], places=9)

    def test_missing_inputs_named(self):
        p = dict(BASE, acquirer={"net_income": 150e6, "shares": 200e6})  # no share price, 40% stock
        with self.assertRaises(AccretionInputError) as cm:
            analyze(p)
        self.assertEqual(cm.exception.missing, ["acquirer_share_price"])
        # All cash: the share price is not needed.
        r = analyze(dict(p, cash_pct=1.0))
        self.assertEqual(r["deal_overview"]["new_shares_issued"], 0.0)

    def test_three_years_grow(self):
        r = analyze(dict(BASE, earnings_growth=0.10))
        self.assertAlmostEqual(r["years"][2]["acquirer_net_income"], 150e6 * 1.21, places=4)


if __name__ == "__main__":
    unittest.main()
