import yfinance as yf
import json
import subprocess
import logging
import pandas as pd

# Configure logging
logging.basicConfig(level=logging.INFO)

# List of financial metrics to fetch
METRICS = [
    "Total Revenue", "EBITDA", "Net Income", "Earnings Per Share",
    "Operating Cash Flow", "Total Debt", "Return on Equity", "Dividend Yield"
]


def fetch_stock_data(ticker):
    """
    Fetch stock information and financial data (Annual & Quarterly metrics).
    Now supports EBITDA, Revenue, Net Income, EPS, Operating Cash Flow, Debt, ROE, and Dividend Yield.
    """
    stock = yf.Ticker(ticker)
    info = stock.info

    # Fetch annual & quarterly financials
    annual_financials = stock.financials
    quarterly_financials = stock.quarterly_financials

    # Initialize dictionaries for storing metrics
    financial_data = {
        metric: {"Annual": {}, "Quarterly": {}} for metric in METRICS
    }

    try:
        for metric in METRICS:
            # Extract Annual Data
            if metric in annual_financials.index:
                annual_series = annual_financials.loc[metric]
                if isinstance(annual_series, pd.Series):
                    for col, value in annual_series.items():
                        if pd.notna(value):
                            financial_data[metric]["Annual"][col.strftime("%Y-%m-%d")] = value

            # Extract Quarterly Data
            if metric in quarterly_financials.index:
                quarterly_series = quarterly_financials.loc[metric]
                if isinstance(quarterly_series, pd.Series):
                    for col, value in quarterly_series.items():
                        if pd.notna(value):
                            financial_data[metric]["Quarterly"][col.strftime("%Y-%m-%d")] = value

    except Exception as e:
        logging.error(f"Error fetching financial data: {e}")

    # Additional key financial ratios (from Yahoo Finance "info" dictionary)
    financial_data["Total Debt"] = info.get("totalDebt", "N/A")
    financial_data["Return on Equity"] = info.get("returnOnEquity", "N/A")
    financial_data["Dividend Yield"] = info.get("dividendYield", "N/A")

    stock_data = {
        "Ticker Info": {
            "symbol": info.get("symbol", ticker.upper()),
            "name": info.get("shortName", "N/A"),
            "industry": info.get("industry", "N/A"),
            "sector": info.get("sector", "N/A"),
            "market_cap": info.get("marketCap", "N/A"),
            "website": info.get("website", "N/A"),
        },
        "Financials": financial_data,
    }

    return stock_data


def build_prompt(stock_data):
    """
    Dynamically builds a structured prompt with all available financial metrics.
    Now includes Debt, ROE, and Dividend Yield for deeper analysis.
    """
    financials = stock_data["Financials"]

    prompt_parts = ["Based on the following key stock data:"]

    for metric, values in financials.items():
        if metric in ["Total Debt", "Return on Equity", "Dividend Yield"]:  # Single-value metrics
            prompt_parts.append(f"- {metric}: {values}")
        else:  # Annual & Quarterly values
            annual = values["Annual"]
            quarterly = values["Quarterly"]

            annual_str = ", ".join(f"{date}: {value}" for date, value in sorted(annual.items())) if annual else "N/A"
            quarterly_str = ", ".join(
                f"{date}: {value}" for date, value in sorted(quarterly.items())) if quarterly else "N/A"

            prompt_parts.append(f"- {metric}: (Annual: {annual_str}), (Quarterly: {quarterly_str})")

    prompt_parts.append(
        "Ignore any missing or incomplete data. Provide a concise performance analysis, highlighting trends, possible causes for fluctuations, "
        "investment risks, and potential opportunities in one precise paragraph."
    )

    return "\n".join(prompt_parts)


def get_deepseek_analysis(prompt):
    """
    Calls the DeepSeek AI model via API to generate a one-paragraph analysis.
    """
    if "No financial data available" in prompt:
        return "No financial data available for analysis."

    payload = {
        "model": "deepseek-r1:8b",
        "prompt": prompt,
        "stream": False
    }
    command = [
        "curl",
        "-s",
        "http://localhost:11434/api/generate",
        "-d", json.dumps(payload)
    ]
    logging.debug("DeepSeek payload: " + json.dumps(payload))
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        logging.error("DeepSeek command failed: " + result.stderr)
        return "Error in DeepSeek analysis."
    return result.stdout.strip()


def main():
    ticker_symbol = input("Enter ticker symbol: ").strip()
    logging.info(f"Fetching stock data for {ticker_symbol}...")
    stock_data = fetch_stock_data(ticker_symbol)

    print("\n=== Stock Data ===")
    print(json.dumps(stock_data, indent=2))

    prompt = build_prompt(stock_data)
    print("\nBuilt prompt:")
    print(prompt)

    analysis = get_deepseek_analysis(prompt)
    print("\n=== DeepSeek Analysis Result ===")
    print(analysis)


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
