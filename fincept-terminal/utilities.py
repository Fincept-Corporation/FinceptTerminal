from rich.table import Table
from rich.panel import Panel
from themes import console
import yfinance as yf
import click

def display_options_in_columns(options, title):
    """Display options in a two-column format with numbering."""
    num_columns = 4
    table = Table(title=title, title_justify="left", header_style="highlight")

    for _ in range(num_columns):
        table.add_column("Option", style="highlight", justify="left", width=50)

    rows = []
    for i, option in enumerate(options, start=1):
        if i % num_columns == 1:
            rows.append([f"{i}. {option}"])
        else:
            rows[-1].append(f"{i}. {option}")

    for row in rows:
        if len(row) < num_columns:
            row.append("")
        table.add_row(*row)

    console.print(table)

def select_option_from_list(options, option_type):
    """Select an option from a list based on user input."""
    while True:
        try:
            choice = int(click.prompt(f"Please select a {option_type} by number"))
            if 1 <= choice <= len(options):
                return options[choice - 1]
            else:
                console.print(f"[danger]Invalid {option_type} number. Please try again.[/danger]")
        except ValueError:
            console.print(f"[danger]Invalid input. Please enter a number corresponding to the {option_type}.[/danger]")

def display_search_results(results):
    """Display search results in a formatted table."""
    table = Table(title="Available Symbols", title_justify="left", header_style="highlight", show_lines=True)
    table.add_column("Symbol", style="highlight", justify="left", width=15)
    table.add_column("Name", style="highlight", justify="left", width=50)
    table.add_column("Market Cap (₹ Cr)", style="highlight", justify="left", width=20)

    for _, row in results.iterrows():
        market_cap_crore = convert_to_crore(row['market_cap'])
        table.add_row(
            str(row['symbol']),
            str(row['name']),
            f"{market_cap_crore:,.2f}" if market_cap_crore != "N/A" else "N/A"
        )

    console.print(table, justify="left")

def fetch_detailed_data(symbol):
    """Fetch and display detailed data for a given symbol."""
    stock = yf.Ticker(symbol)
    info = stock.info

    market_cap_crore = convert_to_crore(info.get('marketCap'))

    info_panel = Panel(
        f"""
        [highlight]Symbol:[/highlight] {info.get('symbol', 'N/A')}
        [highlight]Name:[/highlight] {info.get('longName', 'N/A')}
        [highlight]Sector:[/highlight] {info.get('sector', 'N/A')}
        [highlight]Industry:[/highlight] {info.get('industry', 'N/A')}
        [highlight]Market Cap (₹ Cr):[/highlight] {market_cap_crore if market_cap_crore != "N/A" else "N/A"}
        [highlight]Currency:[/highlight] {info.get('currency', 'N/A')}
        [highlight]Exchange:[/highlight] {info.get('exchange', 'N/A')}
        [highlight]Website:[/highlight] {info.get('website', 'N/A')}
        [highlight]52 Week High:[/highlight] {info.get('fiftyTwoWeekHigh', 'N/A')}
        [highlight]52 Week Low:[/highlight] {info.get('fiftyTwoWeekLow', 'N/A')}
        [highlight]Previous Close:[/highlight] {info.get('previousClose', 'N/A')}
        [highlight]Open:[/highlight] {info.get('open', 'N/A')}
        [highlight]Volume:[/highlight] {info.get('volume', 'N/A')}
        """,
        title=f"Detailed Data for {symbol}",
        title_align="center",
        style="highlight",
        padding=(1, 2)
    )

    console.print(info_panel, justify="left")

def convert_to_crore(market_cap):
    """Convert the market cap to Indian Crores."""
    try:
        market_cap_value = float(market_cap)
        return market_cap_value / 10**7  # 1 crore = 10 million
    except (ValueError, TypeError):
        return "N/A"
