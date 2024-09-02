from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.theme import Theme
import click
import requests
import random
from fincept.utils.data_fetching import equities_df, fetch_fii_dii_data, match_symbol, fetch_detailed_data


# Custom theme
custom_theme = Theme({
    "info": "bold white on #282828",
    "warning": "bold yellow on #282828",
    "danger": "bold red on #282828",
    "success": "bold green on #282828",
    "highlight": "bold cyan on #282828",
})

console = Console(theme=custom_theme, style="white on #282828")

# API base URL
API_BASE_URL = "https://fincept.share.zrok.io"

def show_main_menu():
    while True:
        console.print("[highlight]MAIN MENU[/highlight]\n")

        menu_text = """
1. MARKET TRACKER
2. ECONOMICS
3. NEWS
4. DATABASES
5. EXIT
        """

        menu_panel = Panel(menu_text, title="SELECT AN OPTION", title_align="center", padding=(1, 2))

        console.print(menu_panel)
        
        choice = click.prompt("Please select an option", type=int)

        if choice == 1:
            show_market_tracker_menu()
        elif choice == 2:
            console.print("\n[success]ECONOMICS SELECTED.[/success] (This feature is under development)\n")
        elif choice == 3:
            search_assets()
        elif choice == 4:
            show_databases()
        elif choice == 5:
            console.print("\n[danger]EXITING FINCEPT INVESTMENTS.[/danger]")
            break
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")

def show_market_tracker_menu():
    while True:
        console.print("[highlight]MARKET TRACKER[/highlight]\n")

        tracker_text = """
1. FII/DII DATA INDIA
2. SEARCH ASSETS
3. BACK TO MAIN MENU
        """

        tracker_panel = Panel(tracker_text, title="SELECT AN OPTION", title_align="center", style="highlight", padding=(1, 2))
        console.print(tracker_panel)

        choice = click.prompt("Please select an option", type=int)

        if choice == 1:
            display_fii_dii_data()
        elif choice == 2:
            search_assets()
        elif choice == 3:
            break
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")

def show_databases():
    response = requests.get(f"{API_BASE_URL}/databases")
    databases = response.json()

    # Filter out "postgres" and prepare a list of other databases
    databases = [db for db in databases if db.lower() != "postgres"]

    # Display databases
    display_options_in_columns(databases, "Available Databases")
    database_choice = select_option_from_list(databases, "database")
    
    show_tables(database_choice)

def show_tables(database_choice):
    response = requests.get(f"{API_BASE_URL}/{database_choice}/tables?limit=30")
    tables = response.json()

    # Display tables
    display_options_in_columns(tables, f"Available Tables in {database_choice}")
    table_choice = select_option_from_list(tables, "table")
    
    display_table_data(database_choice, table_choice)

def display_table_data(database_choice, table_choice):
    response = requests.get(f"{API_BASE_URL}/{database_choice}/{table_choice}/data?limit=25")
    data = response.json()

    if not data:
        console.print(f"[bold red]No data available for table {table_choice} in database {database_choice}.[/bold red]", justify="left")
        return

    # Display the data
    table = Table(title=f"Data from {table_choice} - {database_choice}", title_justify="left", header_style="bold magenta", show_lines=True)

    # Dynamically add columns based on keys in the data
    for key in data[0].keys():
        table.add_column(key, style="cyan", justify="left")

    for entry in data:
        table.add_row(*[str(value) for value in entry.values()])

    console.print(table, justify="left")

def search_assets():
    console.print("[highlight]SEARCH ASSETS[/highlight]\n")

    countries = sorted(equities_df['country'].dropna().unique())

    display_options_in_columns(countries, "Available Countries")
    country_choice = select_option_from_list(countries, "country")
    
    if country_choice == '' or country_choice.lower() == 'worldwide':
        country_choice = 'Worldwide'
        filtered_df = equities_df
    else:
        filtered_df = equities_df[equities_df['country'] == country_choice]

    sectors = sorted(filtered_df['sector'].dropna().unique())
    display_options_in_columns(sectors, f"Available Sectors in {country_choice}")
    sector_choice = select_option_from_list(sectors, "sector")

    industries = sorted(filtered_df[filtered_df['sector'] == sector_choice]['industry'].dropna().unique())
    display_options_in_columns(industries, f"Available Industries in {sector_choice}, {country_choice}")
    industry_choice = select_option_from_list(industries, "industry")

    console.print(f"\n[highlight]LISTING ALL SYMBOLS IN {industry_choice} - {sector_choice}, {country_choice}[/highlight]\n")

    search_results = filtered_df[(filtered_df['sector'] == sector_choice) & (filtered_df['industry'] == industry_choice)]

    if search_results.empty:
        console.print(f"[danger]No symbols found in '{industry_choice}' industry.[/danger]")
        return

    display_search_results(search_results)

    fetch_data = click.prompt("Would you like to fetch detailed data for any symbol using yfinance? (y/n)", type=str)
    if fetch_data.lower() == 'y':
        input_name = click.prompt("Enter the symbol or name to fetch data for", type=str)
        closest_symbol = match_symbol(input_name, search_results)
        if closest_symbol:
            display_stock_info(closest_symbol)
        else:
            console.print(f"[danger]No matching symbol found for '{input_name}'.[/danger]")

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
    table.add_column("Market Cap (₹ Cr)", style="highlight", justify="left", width=20)

    for _, row in results.iterrows():
        market_cap_crore = convert_to_crore(row['market_cap'])
        table.add_row(
            str(row['symbol']),
            str(row['name']),
            f"{market_cap_crore:,.2f}" if market_cap_crore != "N/A" else "N/A"
        )

    console.print(table, justify="left")

def convert_to_crore(market_cap):
    try:
        market_cap_value = float(market_cap)
        return market_cap_value / 10**7  # 1 crore = 10 million
    except (ValueError, TypeError):
        return "N/A"

def display_fii_dii_data():
    # API endpoint for FII/DII data
    fii_dii_url = "https://fincept.share.zrok.io/IndiaStockExchange/fii_dii_data/data"
    
    try:
        # Fetch data from the API
        response = requests.get(fii_dii_url)
        response.raise_for_status()
        fii_dii_data = response.json()

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

        for index, entry in enumerate(fii_dii_data):
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

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching FII/DII data: {e}[/bold red]", justify="left")
    data = fetch_fii_dii_data()
    if not data:
        console.print(f"[bold red]Error fetching FII/DII data.[/bold red]", justify="left")
        return

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

    second_entry = data[1] if len(data) > 1 else None
    previous_entry = None

    for index, entry in enumerate(data):
        fii_buy_pct_change = fii_sell_pct_change = fii_net_pct_change = "0.00%"
        dii_buy_pct_change = dii_sell_pct_change = dii_net_pct_change = "0.00%"

        if previous_entry:
            compare_entry = previous_entry
        else:
            compare_entry = second_entry

        if compare_entry:
            fii_buy_pct_change = ((entry['fii_buy_value'] - compare_entry['fii_buy_value']) / abs(compare_entry['fii_buy_value'])) * 100
            fii_sell_pct_change = ((entry['fii_sell_value'] - compare_entry['fii_sell_value']) / abs(compare_entry['fii_sell_value'])) * 100
            fii_net_pct_change = ((entry['fii_net_value'] - compare_entry['fii_net_value']) / abs(compare_entry['fii_net_value'])) * 100
            dii_buy_pct_change = ((entry['dii_buy_value'] - compare_entry['dii_buy_value']) / abs(compare_entry['dii_buy_value'])) * 100
            dii_sell_pct_change = ((entry['dii_sell_value'] - compare_entry['dii_sell_value']) / abs(compare_entry['dii_sell_value'])) * 100
            dii_net_pct_change = ((entry['dii_net_value'] - compare_entry['dii_net_value']) / abs(compare_entry['dii_net_value'])) * 100

            fii_buy_pct_change = f"{fii_buy_pct_change:+.2f}%"
            fii_sell_pct_change = f"{fii_sell_pct_change:+.2f}%"
            fii_net_pct_change = f"{fii_net_pct_change:+.2f}%"
            dii_buy_pct_change = f"{dii_buy_pct_change:+.2f}%"
            dii_sell_pct_change = f"{dii_sell_pct_change:+.2f}%"
            dii_net_pct_change = f"{dii_net_pct_change:+.2f}%"

        table.add_row(
            entry["date"],
            f"{entry['fii_buy_value']:,}", fii_buy_pct_change,
            f"{entry['fii_sell_value']:,}", fii_sell_pct_change,
            f"{entry['fii_net_value']:,}", fii_net_pct_change,
            f"{entry['dii_buy_value']:,}", dii_buy_pct_change,
            f"{entry['dii_sell_value']:,}", dii_sell_pct_change,
            f"{entry['dii_net_value']:,}", dii_net_pct_change
        )

        previous_entry = entry

    console.print(table, justify="left")

def display_stock_info(symbol):
    info = fetch_detailed_data(symbol)
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

if __name__ == "__main__":
    show_main_menu()
