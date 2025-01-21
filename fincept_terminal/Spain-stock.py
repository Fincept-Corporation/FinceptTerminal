def configure_spanish_market_data():
    """
    Configure and display financial market data for Spain.
    """
    import yfinance as yf
    from rich.table import Table
    from fincept_terminal.themes import console

    # Example ticker for a Spanish index
    ticker = "IBEX.MC"
    stock = yf.Ticker(ticker)
    stock_info = stock.info

    console.print(f"\n[bold cyan]Financial market data for {ticker}:[/bold cyan]", style="info")
    metrics = {
        "P/E Ratio": stock_info.get('trailingPE', 'N/A'),
        "Forward P/E": stock_info.get('forwardPE', 'N/A'),
        "Price/Book Ratio": stock_info.get('priceToBook', 'N/A'),
        "Earnings Per Share (EPS)": stock_info.get('trailingEps', 'N/A'),
        "Return on Equity (ROE)": stock_info.get('returnOnEquity', 'N/A'),
        "Debt/Equity Ratio": stock_info.get('debtToEquity', 'N/A'),
        "Gross Margin": stock_info.get('grossMargins', 'N/A'),
        "Dividend Yield": stock_info.get('dividendYield', 'N/A'),
        "Market Cap": stock_info.get('marketCap', 'N/A')
    }

    # Display metrics in a table
    fundamentals_table = Table(title="Fundamental Analysis", show_lines=True, header_style="bold cyan")
    fundamentals_table.add_column("Metric")
    fundamentals_table.add_column("Value")

    for metric, value in metrics.items():
        fundamentals_table.add_row(metric, str(value))

    console.print(fundamentals_table)
