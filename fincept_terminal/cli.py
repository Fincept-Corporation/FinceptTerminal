import click
from rich.console import Console
from rich.panel import Panel
from rich.prompt import Prompt
from rich.table import Table
from rich.text import Text
from .market import show_market_tracker_menu
from .data_fetcher import fetch_sectors_by_country, fetch_industries_by_sector, fetch_stocks_by_industry, display_fii_dii_data
from .assets import search_assets
from .data import get_countries_by_continent
from .themes import console
import yfinance as yf
from fuzzywuzzy import process 
import datetime
import time
from empyrical import cum_returns, max_drawdown, sharpe_ratio, annual_volatility
import requests
import json
import pandas as pd

import warnings
warnings.filterwarnings("ignore")

# Debugging helper function to print messages
def debug(msg):
    console.print(f"[bold yellow][DEBUG] {msg}[/bold yellow]", style="info")

     
@click.group(invoke_without_command=True)
@click.version_option(version="0.1.1", prog_name="Fincept Investments")
@click.pass_context
def cli(ctx):
    """Fincept Investments CLI - Your professional financial terminal."""
    if ctx.invoked_subcommand is None:
        click.echo(ctx.get_help())  # Display help if no command is given

# Start Command
@cli.command()
def start():
    """Start the Fincept Investments terminal"""
    from .display import display_art
    display_art()
    show_main_menu()
    

def show_main_menu():
    """Main menu with navigation options."""
    debug("Displaying Main Menu...")
    console.print("\n")
    console.print("[bold cyan]MAIN MENU[/bold cyan]\n", style="info")
    
    main_menu_options = [
        "MARKET TRACKER",
        "ECONOMICS & MACRO TRENDS",
        "NEWS & ANALYSIS",
        "STOCKS (Equities)",
        "FUNDS & ETFs",
        "BONDS & FIXED INCOME",
        "OPTIONS & DERIVATIVES",
        "CRYPTOCURRENCIES",
        "PORTFOLIO & INVESTMENT TOOLS",
        "CURRENCY MARKETS (Forex)",
        "COMMODITIES",
        "GenAI Query",
        "EDUCATION & RESOURCES",
        "SETTINGS & CUSTOMIZATION",
        "Terminal Documentation",
        "EXIT"
    ]

    # Display main menu in columns
    display_in_columns("Select an Option", main_menu_options)

    console.print("\n")
    choice = Prompt.ask("Enter your choice")
    debug(f"User selected menu option: {choice}")
    console.print("\n")

    if choice == "1":
        show_market_tracker_menu()  # Market Tracker submenu
    elif choice == "2":
        console.print("[bold yellow]Economics section under development[/bold yellow]", style="warning")
    elif choice == "3":
        console.print("[bold yellow]News section under development[/bold yellow]", style="warning")
    elif choice == "4":
        show_equities_menu()  # Equities submenu for continent and country selection
    elif choice == "5":
        console.print("[bold yellow]Funds section under development[/bold yellow]", style="warning")
    elif choice == "6":
        console.print("[bold yellow]Bonds section under development[/bold yellow]", style="warning")
    elif choice == "7":
        console.print("[bold red]Exiting the Fincept terminal...[/bold red]", style="danger")
    elif choice == "9":
        show_portfolio_menu()
    elif choice == "12":  # GenAI Query option
        show_genai_query()
        
        
# Initialize an empty dictionary to hold multiple portfolios
portfolios = {}

def show_portfolio_menu():
    """Submenu for Portfolio & Investment Tools with multiple portfolios management."""
    
    while True:
        console.print("[highlight]PORTFOLIO & INVESTMENT TOOLS[/highlight]\n", style="info")

        portfolio_text = """
1. CREATE NEW PORTFOLIO
2. SELECT EXISTING PORTFOLIO
3. VIEW ALL PORTFOLIOS
4. BACK TO MAIN MENU
        """

        portfolio_panel = Panel(portfolio_text, title="PORTFOLIO MENU", title_align="center", style="bold green on #282828", padding=(1, 2))
        console.print(portfolio_panel)

        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            create_new_portfolio()
        elif choice == "2":
            select_portfolio()
        elif choice == "3":
            view_all_portfolios()
        elif choice == "4":
            break  # Return to main menu


def create_new_portfolio():
    """Allow users to create a new portfolio and directly manage it after creation."""
    portfolio_name = Prompt.ask("Enter the new portfolio name")
    if portfolio_name in portfolios:
        console.print(f"[bold red]Portfolio '{portfolio_name}' already exists![/bold red]")
    else:
        portfolios[portfolio_name] = []
        console.print(f"[bold green]Portfolio '{portfolio_name}' created successfully![/bold green]")
        manage_selected_portfolio(portfolio_name)   # Automatically go to manage the newly created portfolio



def select_portfolio():
    """Allow users to select an existing portfolio for adding stocks or analysis."""
    if not portfolios:
        console.print("[bold red]No portfolios available. Create a portfolio first.[/bold red]")
        return

    console.print("[bold cyan]Select an existing portfolio:[/bold cyan]\n")
    portfolio_names = list(portfolios.keys())
    display_in_columns("Available Portfolios", portfolio_names)

    portfolio_choice = Prompt.ask("Enter the portfolio number to select")
    selected_portfolio_name = portfolio_names[int(portfolio_choice) - 1]

    manage_selected_portfolio(selected_portfolio_name)


def manage_selected_portfolio(portfolio_name):
    """Manage the selected portfolio (add stocks, view stocks, analyze)."""
    
    while True:
        console.print(f"[highlight]MANAGE PORTFOLIO: {portfolio_name}[/highlight]\n", style="info")

        portfolio_menu_text = """
1. ADD STOCK TO PORTFOLIO
2. VIEW CURRENT PORTFOLIO
3. ANALYZE PORTFOLIO PERFORMANCE
4. BACK TO PREVIOUS MENU
        """
        
        portfolio_panel = Panel(portfolio_menu_text, title=f"PORTFOLIO: {portfolio_name}", title_align="center", style="bold green on #282828", padding=(1, 2))
        console.print(portfolio_panel)

        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            add_stock_to_portfolio(portfolio_name)
        elif choice == "2":
            view_portfolio(portfolio_name)
        elif choice == "3":
            analyze_portfolio(portfolio_name)
        elif choice == "4":
            break  # Go back to the previous menu


def add_stock_to_portfolio(portfolio_name=None):
    """Allow users to add multiple stocks to the selected portfolio until they choose to return."""
    if portfolio_name is None:
        # Ask user to select a portfolio if none provided
        if not portfolios:
            console.print("[bold red]No portfolios available. Please create a portfolio first.[/bold red]")
            return
        portfolio_names = list(portfolios.keys())
        display_in_columns("Select a Portfolio", portfolio_names)
        portfolio_choice = Prompt.ask("Enter the portfolio number to select")
        portfolio_name = portfolio_names[int(portfolio_choice) - 1]
    
    while True:
        ticker = Prompt.ask("Enter the stock symbol (or type 'back' to return to the portfolio menu)")
        if ticker.lower() == 'back':
            break  # Exit the loop and return to the portfolio menu
        try:
            stock = yf.Ticker(ticker)
            stock_info = stock.history(period="1y")  # Fetch 1-year historical data
            if not stock_info.empty:
                portfolios[portfolio_name].append(stock)
                console.print(f"[bold green]{ticker} added to portfolio '{portfolio_name}'![/bold green]")
            else:
                console.print(f"[bold red]No data found for {ticker}.[/bold red]")
        except Exception as e:
            console.print(f"[bold red]Error: {e}[/bold red]")


def view_portfolio(portfolio_name):
    """Display the current portfolio."""
    portfolio = portfolios.get(portfolio_name, [])
    
    if not portfolio:
        console.print(f"[bold red]Portfolio '{portfolio_name}' is empty.[/bold red]")
        return

    table = Table(title=f"Portfolio: {portfolio_name}", header_style="bold", show_lines=True)
    table.add_column("Symbol", style="cyan", width=15)
    table.add_column("Name", style="green", width=50)
    
    for stock in portfolio:
        stock_info = stock.info
        table.add_row(stock_info.get('symbol', 'N/A'), stock_info.get('shortName', 'N/A'))

    console.print(table)


def analyze_portfolio(portfolio_name):
    """Analyze portfolio performance using empyrical-reloaded."""
    portfolio = portfolios.get(portfolio_name, [])

    if not portfolio:
        console.print(f"[bold red]Portfolio '{portfolio_name}' is empty.[/bold red]")
        return

    portfolio_returns = pd.DataFrame()

    # Collect returns for each stock in the portfolio
    for stock in portfolio:
        ticker = stock.info['symbol']
        stock_history = stock.history(period="1y")['Close']  # Get the stock's closing prices
        stock_returns = stock_history.pct_change().dropna()  # Calculate daily returns
        portfolio_returns[ticker] = stock_returns

    # Calculate cumulative returns, Sharpe ratio, max drawdown, and annual volatility
    try:
        cumulative_returns = cum_returns(portfolio_returns.mean(axis=1))
        sharpe = sharpe_ratio(portfolio_returns.mean(axis=1))
        max_dd = max_drawdown(portfolio_returns.mean(axis=1))
        annual_vol = annual_volatility(portfolio_returns.mean(axis=1))

        # Display the analysis in a table
        analysis_table = Table(title=f"Portfolio Performance Analysis: {portfolio_name}", header_style="bold", show_lines=True)
        analysis_table.add_column("Metric", style="cyan")
        analysis_table.add_column("Value", style="green")

        analysis_table.add_row("Cumulative Returns", f"{cumulative_returns[-1]:.2%}")
        analysis_table.add_row("Sharpe Ratio", f"{sharpe:.2f}")
        analysis_table.add_row("Max Drawdown", f"{max_dd:.2%}")
        analysis_table.add_row("Annual Volatility", f"{annual_vol:.2%}")

        console.print(analysis_table)

    except Exception as e:
        console.print(f"[bold red]Error in analyzing portfolio: {e}[/bold red]")


def view_all_portfolios():
    """Display a list of all existing portfolios."""
    if not portfolios:
        console.print("[bold red]No portfolios available. Create a portfolio first.[/bold red]")
        return
    
    table = Table(title="All Portfolios", header_style="bold", show_lines=True)
    table.add_column("Portfolio Name", style="cyan", width=30)
    table.add_column("Number of Stocks", style="green", width=20)
    
    for portfolio_name, stocks in portfolios.items():
        table.add_row(portfolio_name, str(len(stocks)))

    console.print(table)

# Equities submenu
def show_equities_menu():
    """Equities submenu that allows selection of stocks based on continent and country."""
    continents = ["Asia", "Europe", "Africa", "North America", "South America", "Oceania", "Middle East", "Main Menu"]

    while True:
        console.print("[bold cyan]EQUITIES MENU[/bold cyan]\n", style="info")
        display_in_columns("Select a Continent", continents)
        
        console.print("\n")
        choice = Prompt.ask("Enter your choice")
        console.print("\n")
        
        if choice == "8":
            show_main_menu()
            return  # Exit equities menu and return to main menu

        selected_continent = continents[int(choice) - 1]

        # If country/sector fetching fails, return to the continent selection
        if not show_country_menu(selected_continent):
            continue  # Loop back to continent selection if no valid data is found
        
def genai_query(user_input):
    """
    Send the user's query to the GenAI API and return the response.
    
    Parameters:
    user_input (str): The query/question entered by the user.
    
    Returns:
    str: The formatted response from the GenAI API.
    """
    api_url = "https://fincept.share.zrok.io/process-gemini/"
    headers = {
        "Content-Type": "application/json"
    }
    data = {
        "user_input": user_input
    }

    try:
        # Send POST request
        response = requests.post(api_url, headers=headers, json=data)
        response.raise_for_status()  # Check for HTTP errors
        
        # Parse the JSON response and extract the 'gemini_response'
        api_response = response.json()
        raw_text = api_response.get("gemini_response", "No response received from the server.")
        
        # Handle any Markdown-like syntax and format the text dynamically
        formatted_response = format_genai_response(raw_text)
        return formatted_response
    
    except requests.exceptions.RequestException as e:
        return f"Error processing query: {str(e)}"


def format_genai_response(response):
    """
    Dynamically format the response text: remove unnecessary symbols, handle Markdown-like syntax, and apply styling.
    
    Parameters:
    response (str): The raw response text from the API.
    
    Returns:
    Text: A Rich Text object with formatted response.
    """
    # Remove Markdown-like symbols (e.g., **, ##) and apply rich formatting
    response = response.replace("**", "").replace("##", "").strip()

    # Create a Rich Text object with cyan color and bold style
    formatted_text = Text(response, style="bold cyan")

    return formatted_text


def show_genai_query():
    """Prompt the user for a finance-related query and send it to the GenAI API."""
    console.print("[bold cyan]GENAI QUERY[/bold cyan]\n", style="info")

    while True:
        query = Prompt.ask("Enter your finance-related query (or type 'back' to return to main menu)")

        if query.lower() == 'back':
            return  # Exit back to the main menu if user types 'back'

        # Send the query to the GenAI API
        console.print("\n[bold yellow]Processing your query...[/bold yellow]\n", style="info")
        response = genai_query(query)

        # Display the formatted response in a panel
        console.print(Panel(response, title="GenAI Query Response", style="cyan on #282828"))

        # Ask if the user wants to make another query
        another_query = Prompt.ask("\nWould you like to make another query? (yes/no)")

        if another_query.lower() == 'no':
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
            show_main_menu()
            return  # Redirect back to the main menu if user types 'no'

def show_country_menu(continent):
    """Display available countries for a given continent and allow the user to select one."""
    countries = get_countries_by_continent(continent)

    if not countries:
        console.print(f"[bold red]No countries available for {continent}[/bold red]", style="danger")
        return

    console.print(f"[bold cyan]Countries in {continent}[/bold cyan]\n", style="info")
    display_in_columns(f"Select a Country in {continent}", countries)
    
    console.print("\n")
    choice = Prompt.ask("Enter your choice")
    selected_country = countries[int(choice) - 1]
    console.print("\n")
    
    show_sectors_in_country(selected_country)
    
def show_country_menu(continent):
    """Display available countries for a given continent and allow the user to select one."""
    countries = get_countries_by_continent(continent)

    if not countries:
        console.print(f"[bold red]No countries available for {continent}[/bold red]", style="danger")
        return False  # Indicate that there are no countries and return to the main menu

    console.print(f"[bold cyan]Countries in {continent}[/bold cyan]\n", style="info")
    display_in_columns(f"Select a Country in {continent}", countries)
    
    console.print("\n")
    choice = Prompt.ask("Enter your choice")
    selected_country = countries[int(choice) - 1]
    console.print("\n")
    
    # Check if sectors are available, otherwise return to country menu
    return show_sectors_in_country(selected_country)

def fetch_sectors_with_retry(country, max_retries=3, retry_delay=2):
    """Fetch sectors with retry logic."""
    sectors_url = f"https://fincept.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks?filter_column=country&filter_value={country}"
    
    retries = 0
    while retries < max_retries:
        try:
            # Simulate fetching sectors (replace this with actual request logic)
            response = requests.get(sectors_url)
            response.raise_for_status()
            sectors = response.json().get('sectors', [])
            return sectors
        except requests.exceptions.RequestException:
            retries += 1
            time.sleep(retry_delay)  # Wait before retrying
            if retries >= max_retries:
                return None  # Return None after max retries


def show_sectors_in_country(country):
    """Fetch sectors for the selected country and allow the user to select one."""
    console.print(f"[bold cyan]Fetching sectors for {country}...[/bold cyan]\n", style="info")
    
    # Fetch sectors with retries
    sectors = fetch_sectors_with_retry(country)
    
    if not sectors:
        # Display user-friendly error after retries
        console.print(f"[bold red]Data temporarily unavailable for {country}. Redirecting to the main menu...[/bold red]", style="danger")
        return False  # Indicate failure to fetch sectors, return to main menu

    console.print(f"[bold cyan]Sectors in {country}[/bold cyan]\n", style="info")
    display_in_columns(f"Select a Sector in {country}", sectors)

    console.print("\n")
    choice = Prompt.ask("Enter your choice")
    selected_sector = sectors[int(choice) - 1]

    show_industries_in_sector(country, selected_sector)
    return True  # Continue normally if sectors were fetched successfully

def show_industries_in_sector(country, sector):
    """Fetch industries for the selected sector and allow the user to select one."""
    industries = fetch_industries_by_sector(country, sector)

    if not industries:
        console.print(f"[bold red]No industries available for {sector} in {country}.[/bold red]", style="danger")
        return

    console.print(f"[bold cyan]Industries in {sector}, {country}[/bold cyan]\n", style="info")
    display_in_columns(f"Select an Industry in {sector}", industries)
    
    choice = Prompt.ask("Enter your choice")
    selected_industry = industries[int(choice) - 1]

    show_stocks_in_industry(country, sector, selected_industry)
    
# After displaying the stocks, ask the user if they want to search more information
def show_stocks_in_industry(country, sector, industry):
    """Display stocks available in the selected industry."""
    stock_data = fetch_stocks_by_industry(country, sector, industry)

    if stock_data.empty:
        console.print(f"[bold red]No stocks available for {industry} in {sector}, {country}.[/bold red]", style="danger")
    else:
        display_equities(stock_data)

        while True:
            console.print("\n")
            choice = Prompt.ask("Would you like to search for more information on a specific stock? (yes/no)")
            if choice.lower() == 'yes':
                ticker_name = Prompt.ask("Please enter the stock symbol or company name (partial or full)")
                closest_ticker = find_closest_ticker(ticker_name, stock_data)
                if closest_ticker:
                    display_stock_info(closest_ticker)
                else:
                    console.print(f"[bold red]No matching ticker found for '{ticker_name}'.[/bold red]", style="danger")
            else:
                return  # Return to the previous menu instead of directly to the main menu

def find_closest_ticker(user_input, stock_data):
    """Find the closest matching ticker or company name from the displayed stocks."""
    stock_symbols = stock_data['symbol'].tolist()
    stock_names = stock_data['name'].tolist()

    # Combine symbols and names into a list for fuzzy matching
    stock_list = stock_symbols + stock_names

    # Use fuzzy matching to find the closest match
    closest_match, score = process.extractOne(user_input, stock_list)
    
    if score > 70:
        if closest_match in stock_names:
            return stock_data.loc[stock_data['name'] == closest_match, 'symbol'].values[0]
        return closest_match
    return None


def display_stock_info(ticker):
    """Fetch and display detailed stock information using yfinance."""
    stock = yf.Ticker(ticker)
    stock_info = stock.info

    if not stock_info:
        console.print(f"[bold red]No information found for {ticker}.[/bold red]", style="danger")
        return

    # Filter out null values and unwanted keys
    filtered_info = {k: v for k, v in stock_info.items() if v is not None and k not in [
        'uuid', 'gmtOffSetMilliseconds', 'messageBoardId', 'compensationAsOfEpochDate', 'maxAge'
    ]}

    # Display `longBusinessSummary` in one row (full width)
    console.print(f"\n[highlight]Business Summary[/highlight]: {filtered_info.get('longBusinessSummary', 'N/A')}", style="info")

    # Display `companyOfficers` in a structured way
    if 'companyOfficers' in filtered_info:
        console.print("\n[highlight]Company Officers:[/highlight]", style="highlight")
        officers_table = Table(show_lines=True, style="info", header_style="bold white on #282828")

        officers_table.add_column("Name", style="cyan on #282828")
        officers_table.add_column("Title", style="green on #282828")
        officers_table.add_column("Total Pay", style="magenta on #282828")
        officers_table.add_column("Age", style="yellow on #282828")
        
        for officer in filtered_info['companyOfficers']:
            name = officer.get('name', 'N/A')
            title = officer.get('title', 'N/A')
            total_pay = officer.get('totalPay', 'N/A')
            age = officer.get('age', 'N/A')
            officers_table.add_row(name, title, str(total_pay), str(age))
        console.print(officers_table)
        console.print("\n")

    # Remove `longBusinessSummary` and `companyOfficers` from the filtered info as we already displayed them
    filtered_info.pop('longBusinessSummary', None)
    filtered_info.pop('companyOfficers', None)

    # Display the remaining data in three columns
    display_info_in_three_columns(filtered_info)

    # Ask if the user wants to export the data
    choice = Prompt.ask("\nWould you like to export the data to CSV or Excel? (yes/no)", default="no")
    
    if choice.lower() == "yes":
        export_choice = Prompt.ask("Choose export format: CSV or Excel?", choices=["csv", "excel"], default="csv")
        export_stock_info(filtered_info, ticker, export_choice)

def display_info_in_three_columns(info):
    """Display key-value pairs in three columns, skipping long values."""
    table = Table(show_lines=True, style="info", header_style="bold white on #282828")

    # Add columns for three attributes and values
    table.add_column("Attribute 1", style="cyan on #282828", width=25)
    table.add_column("Value 1", style="green on #282828", width=35)
    table.add_column("Attribute 2", style="cyan on #282828", width=25)
    table.add_column("Value 2", style="green on #282828", width=35)
    table.add_column("Attribute 3", style="cyan on #282828", width=25)
    table.add_column("Value 3", style="green on #282828", width=35)

    max_value_length = 40  # Set a maximum length for displayed values
    keys = list(info.keys())
    values = list(info.values())

    for i in range(0, len(keys), 3):
        row_data = []
        for j in range(3):
            if i + j < len(keys):
                key = keys[i + j]
                value = values[i + j]
                # Skip long values and add a placeholder
                if isinstance(value, str) and len(value) > max_value_length:
                    row_data.append(str(key))
                    row_data.append("[value too long]")
                else:
                    row_data.append(str(key))
                    row_data.append(str(value))
            else:
                row_data.append("")
                row_data.append("")
        table.add_row(*row_data)

    console.print(table)

def export_stock_info(info, ticker, export_format):
    """Export stock information to CSV or Excel."""
    # Convert the info dictionary to a pandas DataFrame
    df = pd.DataFrame(list(info.items()), columns=["Attribute", "Value"])

    # Define the file name
    file_name = f"{ticker}_stock_info.{export_format}"

    try:
        if export_format == "csv":
            df.to_csv(file_name, index=False)
        elif export_format == "excel":
            df.to_excel(file_name, index=False)
        console.print(f"[bold green]Stock information successfully exported to {file_name}![/bold green]", style="success")
    except Exception as e:
        console.print(f"[bold red]Failed to export data: {e}[/bold red]", style="danger")

def display_equities(stock_data):
    """Display stock data in a tabular format."""
    table = Table(title="Available Stocks", title_justify="left", header_style="bold", show_lines=True)
    table.add_column("Symbol", style="cyan", justify="left", width=15)
    table.add_column("Name", style="green", justify="left", width=50)
    table.add_column("Market Cap", style="yellow", justify="left", width=20)

    for _, row in stock_data.iterrows():
        table.add_row(str(row['symbol']), str(row['name']), str(row['market_cap']))

    console.print("\n")
    console.print(table)

# Function to display lists in columns with max 7 rows per column (no "Column 1" heading)
def display_in_columns(title, items):
    """Display the items in a table with multiple columns if they exceed 7 rows."""
    table = Table(title=title, header_style="bold green on #282828", show_lines=True)  # show_lines=True adds spacing between rows
    max_rows = 7  # Maximum number of rows per column
    num_columns = (len(items) + max_rows - 1) // max_rows  # Calculate the required number of columns

    # Add the columns (empty headers to remove column titles)
    for _ in range(num_columns):
        table.add_column("", style="highlight", justify="left")

    # Add rows in columns
    rows = [[] for _ in range(max_rows)]  # Empty rows to hold the items
    for index, item in enumerate(items):
        row_index = index % max_rows
        rows[row_index].append(f"{index+1}. {item}")

    # Fill the table
    for row in rows:
        # If the row has fewer elements than the number of columns, fill the rest with empty strings
        row += [""] * (num_columns - len(row))
        table.add_row(*row)

    console.print(table)


# Submenu handling for Market Tracker in `market.py`
def show_market_tracker_menu():
    """Market Tracker submenu."""
    while True:
        console.print("[highlight]MARKET TRACKER[/highlight]\n", style="info")

        tracker_text = """
1. FII/DII DATA INDIA
2. NIFTY 50 LIST
3. SEARCH ASSETS
4. BACK TO MAIN MENU
        """

        tracker_panel = Panel(tracker_text, title="MARKET TRACKER MENU", title_align="center", style="bold green on #282828", padding=(1, 2))
        console.print(tracker_panel)

        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            display_fii_dii_data()  
        elif choice == "2":
            console.print("[bold yellow]Nifty 50 list under development[/bold yellow]", style="warning")
        elif choice == "3":
            search_assets() 
        elif choice == "4":
            break

if __name__ == '__main__':
    cli()
