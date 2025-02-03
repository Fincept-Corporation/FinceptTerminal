from rich.table import Table
from fincept_terminal.FinceptTerminalUtilsModule.themes import console
import click


from dotenv import load_dotenv

def load_environment():
    """Load environment variables from .env file."""
    load_dotenv()  # Load variables from .env into the environment

def display_options_in_columns(options, title):
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
    table = Table(title="Available Symbols", title_justify="left", header_style="highlight", show_lines=True)
    table.add_column("Symbol", style="highlight", justify="left", width=15)
    table.add_column("Name", style="highlight", justify="left", width=50)
    table.add_column("Market Cap", style="highlight", justify="left", width=20)

    for _, row in results.iterrows():
        table.add_row(
            str(row['symbol']),
            str(row['name']),
            str(row['market_cap'])
        )

    console.print(table, justify="left")
    
def display_fii_dii_table(fii_dii_data):
    # Display data in a colorful table
    table = Table(title="FII/DII DATA - INDIA (values in CR)", title_justify="left", header_style="bold magenta")

    table.add_column("DATE", style="cyan", no_wrap=True)
    table.add_column("FII BUY VALUE", style="yellow")
    table.add_column("FII BUY PCT CHANGE", style="blue")
    table.add_column("FII SELL VALUE", style="yellow")
    table.add_column("FII SELL PCT CHANGE", style="blue")
    table.add_column("FII NET VALUE", style="green")
    table.add_column("FII NET PCT CHANGE", style="blue")
    table.add_column("DII BUY VALUE", style="yellow")
    table.add_column("DII BUY PCT CHANGE", style="blue")
    table.add_column("DII SELL VALUE", style="yellow")
    table.add_column("DII SELL PCT CHANGE", style="blue")
    table.add_column("DII NET VALUE", style="green")
    table.add_column("DII NET PCT CHANGE", style="blue")

    if len(fii_dii_data) < 2:
        console.print(f"[bold red]Not enough data to perform percentage change calculations.[/bold red]", justify="left")
        return

    second_entry = fii_dii_data[1]
    previous_entry = None

    for entry in fii_dii_data:
        # Initialize percentage change strings
        fii_buy_pct_change = fii_sell_pct_change = fii_net_pct_change = "0.00%"
        dii_buy_pct_change = dii_sell_pct_change = dii_net_pct_change = "0.00%"

        # Calculate percentage changes using the second entry if previous_entry is not available
        if previous_entry:
            compare_entry = previous_entry
        else:
            compare_entry = second_entry  # Use the second available data if no previous entry

        # Perform percentage change calculation
        if compare_entry:
            fii_buy_pct_change = ((entry['fii_buy_value'] - compare_entry['fii_buy_value']) / abs(compare_entry['fii_buy_value'])) * 100
            fii_sell_pct_change = ((entry['fii_sell_value'] - compare_entry['fii_sell_value']) / abs(compare_entry['fii_sell_value'])) * 100
            fii_net_pct_change = ((entry['fii_net_value'] - compare_entry['fii_net_value']) / abs(compare_entry['fii_net_value'])) * 100
            dii_buy_pct_change = ((entry['dii_buy_value'] - compare_entry['dii_buy_value']) / abs(compare_entry['dii_buy_value'])) * 100
            dii_sell_pct_change = ((entry['dii_sell_value'] - compare_entry['dii_sell_value']) / abs(compare_entry['dii_sell_value'])) * 100
            dii_net_pct_change = ((entry['dii_net_value'] - compare_entry['dii_net_value']) / abs(compare_entry['dii_net_value'])) * 100

            # Format the percentage change strings
            fii_buy_pct_change = f"{fii_buy_pct_change:+.2f}%"
            fii_sell_pct_change = f"{fii_sell_pct_change:+.2f}%"
            fii_net_pct_change = f"{fii_net_pct_change:+.2f}%"
            dii_buy_pct_change = f"{dii_buy_pct_change:+.2f}%"
            dii_sell_pct_change = f"{dii_sell_pct_change:+.2f}%"
            dii_net_pct_change = f"{dii_net_pct_change:+.2f}%"

        # Add row to the table
        table.add_row(
            entry["date"],
            f"{entry['fii_buy_value']:,}", fii_buy_pct_change,
            f"{entry['fii_sell_value']:,}", fii_sell_pct_change,
            f"{entry['fii_net_value']:,}", fii_net_pct_change,
            f"{entry['dii_buy_value']:,}", dii_buy_pct_change,
            f"{entry['dii_sell_value']:,}", dii_sell_pct_change,
            f"{entry['dii_net_value']:,}", dii_net_pct_change
        )

        # Update previous_entry for next iteration
        previous_entry = entry

    console.print(table, justify="left")


def fetch_detailed_data(symbol):
    import yfinance as yf
    from rich.panel import Panel
    
    console.print(f"\n[highlight]FETCHING DATA FOR: {symbol}[/highlight]\n")

    stock = yf.Ticker(symbol)
    info = stock.info

    market_cap = info.get('marketCap', 'N/A')

    info_panel = Panel(
        f"""
        [highlight]Symbol:[/highlight] {info.get('symbol', 'N/A')}
        [highlight]Name:[/highlight] {info.get('longName', 'N/A')}
        [highlight]Sector:[/highlight] {info.get('sector', 'N/A')}
        [highlight]Industry:[/highlight] {info.get('industry', 'N/A')}
        [highlight]Market Cap:[/highlight] {market_cap if market_cap != "N/A" else "N/A"}
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
