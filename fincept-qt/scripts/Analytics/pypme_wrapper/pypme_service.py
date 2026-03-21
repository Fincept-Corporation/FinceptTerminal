"""
PyPME Worker Handler - Public Market Equivalent calculations.
Usage: python worker_handler.py <operation> <json_data>
"""
import sys
import json

def main(args):
    if len(args) < 2:
        print(json.dumps({"error": "Usage: worker_handler.py <operation> <json_data>"}))
        return

    operation = args[0]
    data = json.loads(args[1])

    result = dispatch(operation, data)
    print(json.dumps(result))

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
