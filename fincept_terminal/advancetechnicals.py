from fincept_terminal.themes import console
from fincept_terminal.const import display_in_columns
from rich.prompt import Prompt
import yfinance as yf
import pandas as pd
from datetime import datetime

# Placeholder: use the actual function from your codebase
def show_equities_menu():
    """Equities submenu that allows selection of stocks based on continent and country."""
    continents = ["Asia", "Europe", "Africa", "North America", "South America", "Oceania", "Middle East", "Main Menu"]

    while True:
        console.print("[bold cyan]EQUITIES MENU[/bold cyan]\n", style="info")
        display_in_columns("Select a Continent", continents)

        console.print("\n")
        choice = Prompt.ask("Enter your choice")
        console.print("\n")

        if choice == "8":  # Exit to Main Menu
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
            return

        selected_continent = continents[int(choice) - 1]

        if not show_country_menu(selected_continent):
            continue  # Loop back to continent selection if no valid data is found


# Placeholder for show_country_menu and get_countries_by_continent
def show_country_menu(continent):
    """Display available countries for a given continent and allow the user to select one."""
    countries = get_countries_by_continent(continent)
    if not countries:
        console.print(f"[bold red]No countries available for {continent}[/bold red]", style="danger")
        return False

    console.print(f"[bold cyan]Countries in {continent}[/bold cyan]\n", style="info")
    display_in_columns(f"Select a Country in {continent}", countries)

    console.print("\n")
    choice = Prompt.ask("Enter your choice")
    selected_country = countries[int(choice) - 1]
    console.print("\n")

    from fincept_terminal.stock import show_sectors_in_country
    return show_sectors_in_country(selected_country)


# Placeholder for fetching countries by continent
def get_countries_by_continent(continent):
    """Returns the list of countries for the given continent."""
    # This dictionary should contain actual continent-country mappings
    continent_countries = {
        "Asia": ["India", "China", "Japan"],
        "Europe": ["Germany", "France", "UK"],
        # Add all other continents with respective countries
    }
    return continent_countries.get(continent, [])


# Step 1: Technical Main Menu
def show_technical_main_menu():
    """Technical analysis main menu that allows global or direct stock symbol selection."""
    console.print("[bold cyan]TECHNICAL ANALYSIS MENU[/bold cyan]\n", style="info")

    # Step 1.1: List the main options
    options = ["Global Stocks", "Search Stock Symbol", "Main Menu"]
    display_in_columns("Select an Option", options)

    choice = Prompt.ask("Enter your choice")

    if choice == "1":  # Global Stocks
        show_equities_menu()  # This will handle stock selection through continents/countries

        # You need to store the selected stock symbol in this process
        stock_symbol = "AAPL"  # Placeholder for selected stock after equities menu

        show_technical_analysis_menu(stock_symbol)

    elif choice == "2":  # Search Stock Symbol
        stock_symbol = Prompt.ask("Enter the stock symbol")
        show_technical_analysis_menu(stock_symbol)

    elif choice == "3":  # Main Menu
        from fincept_terminal.cli import show_main_menu
        show_main_menu()
    else:
        console.print("[bold red]Invalid selection, please try again.[/bold red]")


# Step 2: Show technical analysis options for the selected stock
def show_technical_analysis_menu(stock_symbol):
    """Menu for selecting technical analysis for the given stock."""
    console.print(f"[bold cyan]TECHNICAL ANALYSIS FOR {stock_symbol.upper()}[/bold cyan]\n", style="info")

    # Step 2.1: Time frame selection
    time_frames = ["5d", "1wk", "1mo", "3mo", "1y", "3y", "5y"]
    display_in_columns("Select a Time Frame", time_frames)
    time_frame = Prompt.ask("Enter the time frame")

    stock_data = fetch_stock_data(stock_symbol, time_frame)

    if stock_data is None or stock_data.empty:
        console.print(f"[bold red]No data found for {stock_symbol} for the selected period {time_frame}.[/bold red]")
        return  # Exit if no data is available

    # Step 2.2: Technical analysis options
    technical_options = ["Stage Analysis", "Camarilla", "Gann", "Elliott Wave", "Greed/Fear Index"]
    display_in_columns("Select Technical Analysis", technical_options)
    technical_choice = Prompt.ask("Enter the number corresponding to the analysis")

    if technical_choice == "1":  # Stage Analysis
        results = calculate_stage_analysis(stock_data)
    elif technical_choice == "2":  # Camarilla
        results = calculate_camarilla_levels(stock_data)
    elif technical_choice == "3":  # Gann
        results = calculate_gann(stock_data)
    elif technical_choice == "4":  # Elliott Wave
        results = calculate_elliott_wave(stock_data)
    elif technical_choice == "5":  # Greed/Fear Index
        results = calculate_greed_fear_index(stock_data)
    else:
        console.print("[bold red]Invalid selection.[/bold red]")
        return

    display_technical_analysis_results(results)

    # Ask if the user wants to query another analysis or exit to the main menu
    continue_query = Prompt.ask("Do you want to perform another analysis? (yes/no)")
    if continue_query.lower() == 'yes':
        show_technical_analysis_menu(stock_symbol)
    else:
        console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
        from fincept_terminal.cli import show_main_menu
        show_main_menu()


# Placeholder for fetching stock data
def fetch_stock_data(stock_symbol, period):
    """Fetch stock data using yfinance for the given symbol and period."""
    try:
        # Force minimum period of 1 year for Stage Analysis to calculate 200-SMA
        if period in ["5d", "1wk", "1mo", "3mo"]:
            console.print(
                "[bold yellow]Fetching 1 year of data for Stage Analysis to calculate 200-day SMA.[/bold yellow]")
            period = "1y"  # Override shorter time frames to at least 1 year

        stock = yf.Ticker(stock_symbol)
        stock_data = stock.history(period=period)

        if stock_data.empty:
            console.print(f"[bold red]No data found for {stock_symbol} for the selected period {period}.[/bold red]")
            return None

        return stock_data
    except Exception as e:
        console.print(f"[bold red]Error fetching data: {e}[/bold red]")
        return None


# Placeholder for technical analysis calculations
def calculate_stage_analysis(stock_data):
    """Perform Stage Analysis and return the current stage, trend, and when the stage started."""
    try:
        # Calculate the 200-day simple moving average (SMA)
        stock_data['200_SMA'] = stock_data['Close'].rolling(window=200).mean()

        # Fetch the last closing price and the corresponding 200-SMA
        last_row = stock_data.iloc[-1]
        last_close = last_row['Close']
        last_sma = last_row['200_SMA']

        # Determine the stage based on the relationship between price and the 200-SMA
        if last_close > last_sma:
            # Stage 2 (Uptrend): Price is above the 200-SMA
            stage = "Stage 2 (Uptrend)"
        elif last_close < last_sma:
            # Stage 4 (Downtrend): Price is below the 200-SMA
            stage = "Stage 4 (Downtrend)"
        else:
            # Price is close to the 200-SMA, could be Accumulation or Distribution
            stage = "Stage 3 (Distribution)" if last_close < last_sma else "Stage 1 (Accumulation)"

        # Find the starting point of the current trend (when it crossed the 200-SMA)
        stock_data['Trend'] = stock_data['Close'] > stock_data['200_SMA']
        trend_changes = stock_data['Trend'].ne(stock_data['Trend'].shift())

        # Identify when the current trend started
        trend_start = stock_data[trend_changes].iloc[-1].name if not stock_data[trend_changes].empty else None

        # Convert the trend start date to a readable format
        trend_start_date = pd.to_datetime(trend_start).strftime("%Y-%m-%d") if trend_start else "Unknown"

        # Return stage, trend, and trend start date
        return {
            "Stage": stage,
            "Current Trend": "Uptrend" if last_close > last_sma else "Downtrend",
            "Trend Start Date": trend_start_date
        }

    except Exception as e:
        console.print(f"[bold red]Error calculating Stage Analysis: {e}[/bold red]")
        return {}



def calculate_camarilla_levels(stock_data):
    """Calculate Camarilla pivot points based on the last trading day's high, low, and close."""
    try:
        # Fetch the last trading day's data
        last_day = stock_data.iloc[-1]

        high = last_day['High']
        low = last_day['Low']
        close = last_day['Close']

        # Camarilla pivot point formulas
        r4 = (high - low) * 1.1 / 2 + close
        r3 = (high - low) * 1.1 / 4 + close
        r2 = (high - low) * 1.1 / 6 + close
        r1 = (high - low) * 1.1 / 12 + close

        s1 = close - (high - low) * 1.1 / 12
        s2 = close - (high - low) * 1.1 / 6
        s3 = close - (high - low) * 1.1 / 4
        s4 = close - (high - low) * 1.1 / 2

        # Return calculated levels as a dictionary
        return {
            "R4 (Resistance 4)": round(r4, 4),
            "R3 (Resistance 3)": round(r3, 4),
            "R2 (Resistance 2)": round(r2, 4),
            "R1 (Resistance 1)": round(r1, 4),
            "S1 (Support 1)": round(s1, 4),
            "S2 (Support 2)": round(s2, 4),
            "S3 (Support 3)": round(s3, 4),
            "S4 (Support 4)": round(s4, 4)
        }

    except Exception as e:
        console.print(f"[bold red]Error calculating Camarilla levels: {e}[/bold red]")
        return {}


def calculate_gann(stock_data):
    console.print("Calculating Gann levels...")
    return {"Gann Levels": "Calculated"}


def calculate_elliott_wave(stock_data):
    console.print("Calculating Elliott Wave...")
    return {"Elliott Wave": "Wave 1"}


def calculate_greed_fear_index(stock_data):
    console.print("Calculating Greed/Fear Index...")
    return {"Greed/Fear": "Neutral"}


# Placeholder for displaying results
def display_technical_analysis_results(results):
    from rich.table import Table
    table = Table(show_lines=True, header_style="bold white on #282828")

    table.add_column("Technical Indicator", style="cyan on #282828", width=35)
    table.add_column("Result", style="green on #282828", width=45)

    for key, value in results.items():
        table.add_row(key, str(value))

    console.print(table)

