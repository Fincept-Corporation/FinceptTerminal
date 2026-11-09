"""
PyPME service - Public Market Equivalent (PME / xPME) of a private-equity cash-flow stream via the pypme library.

Usage: python pypme_service.py <operation> <json_data>
  pme | verbose_pme          {"cashflows": [...], "prices": [...], "pme_prices": [...]}   (one row per period)
  xpme | verbose_xpme        {"dates": ["YYYY-MM-DD", ...], "cashflows": [...], "prices": [...], "pme_prices": [...]}
  tessa_xpme | tessa_verbose_xpme
                             {"dates": [...], "cashflows": [...], "prices": [...], "pme_ticker": "SPY",
                              "pme_source": "yahoo"}   (index prices fetched by tessa; needs network)
prices / pme_prices / dates: one per point in time, all > 0, the last being the valuation date (prices = the
investment's NAV path, pme_prices = the index); cashflows: one per point except the last (len = len(prices) - 1),
first one negative (paid in), positive = distributions. Output JSON; {"error": ...} on failure.
"""
import os
import sys
import json

# Run as a plain file, only this folder and scripts/ are on sys.path: make `pypme_wrapper.*` importable.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    import fincept_json


def main(args):
    if len(args) < 2:
        print(json.dumps({"error": "Usage: pypme_service.py <operation> <json_data>"}))
        return

    operation = args[0]
    data = json.loads(args[1])

    result = dispatch(operation, data)
    print(fincept_json.dumps(result))

def dispatch(operation, data):
    try:
        if operation == "pme":
            from pypme_wrapper.core import calculate_pme
            return calculate_pme(
                data["cashflows"],
                data["prices"],
                data["pme_prices"]
            )

        elif operation == "verbose_pme":
            from pypme_wrapper.core import calculate_verbose_pme
            return calculate_verbose_pme(
                data["cashflows"],
                data["prices"],
                data["pme_prices"]
            )

        elif operation == "xpme":
            from pypme_wrapper.core import calculate_xpme
            return calculate_xpme(
                data["dates"],
                data["cashflows"],
                data["prices"],
                data["pme_prices"]
            )

        elif operation == "verbose_xpme":
            from pypme_wrapper.core import calculate_verbose_xpme
            return calculate_verbose_xpme(
                data["dates"],
                data["cashflows"],
                data["prices"],
                data["pme_prices"]
            )

        elif operation == "tessa_xpme":
            from pypme_wrapper.core import calculate_tessa_xpme
            return calculate_tessa_xpme(
                data["dates"],
                data["cashflows"],
                data["prices"],
                data["pme_ticker"],
                data.get("pme_source", "yahoo")
            )

        elif operation == "tessa_verbose_xpme":
            from pypme_wrapper.core import calculate_tessa_verbose_xpme
            return calculate_tessa_verbose_xpme(
                data["dates"],
                data["cashflows"],
                data["prices"],
                data["pme_ticker"],
                data.get("pme_source", "yahoo")
            )

        else:
            return {"error": f"Unknown operation: {operation}"}

    except Exception as e:
        return {"error": str(e)}

if __name__ == "__main__":
    main(sys.argv[1:])
