import sys
import json
import yfinance as yf


def get_stock_data(symbol):
    try:
        ticker = yf.Ticker(symbol)
        # Get basic info
        info = ticker.info
        # Get recent price data (last 30 days)
        hist = ticker.history(period="1mo")

        return {
            "success": True,
            "symbol": symbol,
            "info": {
                "name": info.get("longName", "N/A"),
                "price": info.get("currentPrice", "N/A"),
                "currency": info.get("currency", "USD")
            },
            "recent_data": hist.tail(5).to_dict("records") if not hist.empty else []
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_multiple_stocks(symbols_str):
    try:
        symbols = symbols_str.split(",")
        results = []
        for symbol in symbols[:5]:  # Limit to 5 stocks
            data = yf.download(symbol.strip(), period="5d", interval="1d")
            if not data.empty:
                latest = data.iloc[-1]
                results.append({
                    "symbol": symbol.strip(),
                    "close": float(latest['Close']),
                    "volume": int(latest['Volume'])
                })

        return {"success": True, "data": results}
    except Exception as e:
        return {"success": False, "error": str(e)}


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(json.dumps({"error": "Usage: python main.py <command> <param>"}))
        sys.exit(1)

    command = sys.argv[1]
    param = sys.argv[2]

    if command == "stock":
        result = get_stock_data(param)
    elif command == "multiple":
        result = get_multiple_stocks(param)
    else:
        result = {"error": "Unknown command"}

    print(json.dumps(result))