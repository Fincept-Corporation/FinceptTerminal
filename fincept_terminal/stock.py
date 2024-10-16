import yfinance as yf

def show_country_menu(continent):
    """Display available countries for a given continent and allow the user to select one."""
    from fincept_terminal.data import get_countries_by_continent
    countries = get_countries_by_continent(continent)

    if not countries:
        console.print(f"[bold red]No countries available for {continent}[/bold red]", style="danger")
        return False  # Indicate that there are no countries and return to the main menu

    from fincept_terminal.const import display_in_columns
    console.print(f"[bold cyan]Countries in {continent}[/bold cyan]\n", style="info")
    display_in_columns(f"Select a Country in {continent}", countries)

    console.print("\n")
    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")
    selected_country = countries[int(choice) - 1]
    console.print("\n")

    from fincept_terminal.stock import show_sectors_in_country
    return show_sectors_in_country(selected_country)

def show_equities_menu():
    """Equities submenu that allows selection of stocks based on continent, country, or direct query."""
    continents = ["Asia", "Europe", "Africa", "North America", "South America", "Oceania", "Middle East", "Main Menu", "Query Stock Ticker"]

    while True:
        console.print("[bold cyan]EQUITIES MENU[/bold cyan]\n", style="info")
        from fincept_terminal.const import display_in_columns
        display_in_columns("Select a Continent or Option", continents)

        console.print("\n")
        from rich.prompt import Prompt
        choice = Prompt.ask("Enter your choice")
        console.print("\n")

        if choice == "8":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
            return  # Exit equities menu and return to main menu

        elif choice == "9":
            query_direct_from_yahoo()
            return  # After querying, return to the menu

        selected_continent = continents[int(choice) - 1]

        if not show_country_menu(selected_continent):
            continue  # Loop back to continent selection if no valid data is found


def query_direct_from_yahoo():
    """Directly query data from Yahoo Finance and display it."""
    while True:
        from rich.prompt import Prompt
        ticker = Prompt.ask("Enter the stock symbol (e.g., AAPL)")

        try:
            display_stock_info(ticker)
        except Exception as e:
            console.print(f"[bold red]An error occurred while fetching data for '{ticker}': {str(e)}[/bold red]",
                          style="danger")
            continue

        another_query = Prompt.ask("\nWould you like to query another ticker? (yes/no)", default="no")
        if another_query.lower() != "yes":
            break

    console.print("[bold yellow]Returning to the main menu...[/bold yellow]", style="info")
    from fincept_terminal.cli import show_main_menu
    show_main_menu()


def fetch_sectors_with_retry(country, max_retries=3, retry_delay=2):
    """Fetch sectors with retry logic."""
    sectors_url = f"https://fincept.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks?filter_column=country&filter_value={country}"

    retries = 0
    while retries < max_retries:
        import requests
        try:
            # Simulate fetching sectors (replace this with actual request logic)
            response = requests.get(sectors_url)
            response.raise_for_status()
            sectors = response.json().get('sectors', [])
            return sectors
        except requests.exceptions.RequestException:
            retries += 1
            import time
            time.sleep(retry_delay)  # Wait before retrying
            if retries >= max_retries:
                return None  # Return None after max retries


def show_sectors_in_country(country):
    """Fetch sectors for the selected country and allow the user to select one."""
    from fincept_terminal.themes import console
    console.print(f"[bold cyan]Fetching sectors for {country}...[/bold cyan]\n", style="info")

    # Fetch sectors with retries
    sectors = fetch_sectors_with_retry(country)

    if not sectors:
        # Display user-friendly error after retries
        console.print(
            f"[bold red]Data temporarily unavailable for {country}. Redirecting to the main menu...[/bold red]",
            style="danger")
        return False  # Indicate failure to fetch sectors, return to main menu

    console.print(f"[bold cyan]Sectors in {country}[/bold cyan]\n", style="info")
    from fincept_terminal.const import display_in_columns
    display_in_columns(f"Select a Sector in {country}", sectors)

    console.print("\n")
    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")
    selected_sector = sectors[int(choice) - 1]

    show_industries_in_sector(country, selected_sector)
    return True  # Continue normally if sectors were fetched successfully


import urllib.parse
import requests
from fincept_terminal.themes import console

# Define the base URL in a configuration or a constant
BASE_API_URL = "https://fincept.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks"


def build_url(base_url, params):
    """Dynamically build a URL with the given base URL and parameters."""
    query_string = urllib.parse.urlencode(params)
    return f"{base_url}?{query_string}"


def fetch_industries_by_sector(country, sector):
    """Fetch industries for the selected sector with dynamic URL construction."""
    # URL encode the sector and country as part of query parameters
    params = {
        "filter_column": "country",
        "filter_value": country,
        "sector": sector
    }

    # Build the full URL dynamically
    industries_url = build_url(BASE_API_URL, params)

    try:
        response = requests.get(industries_url)
        response.raise_for_status()
        industries_data = response.json()
        return industries_data.get('industries', [])
    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching industries for {sector} in {country}: {e}[/bold red]", style="danger")
        return None


def show_industries_in_sector(country, sector):
    """Fetch industries for the selected sector and allow the user to select one."""
    industries = fetch_industries_by_sector(country, sector)

    if not industries:
        console.print(f"[bold red]No industries available for {sector} in {country}.[/bold red]", style="danger")
        return

    console.print(f"[bold cyan]Industries in {sector}, {country}[/bold cyan]\n", style="info")
    from fincept_terminal.const import display_in_columns
    display_in_columns(f"Select an Industry in {sector}", industries)

    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")
    selected_industry = industries[int(choice) - 1]

    show_stocks_in_industry(country, sector, selected_industry)



# After displaying the stocks, ask the user if they want to search more information
def show_stocks_in_industry(country, sector, industry):
    """Display stocks available in the selected industry."""
    from fincept_terminal.data_fetcher import fetch_stocks_by_industry
    stock_data = fetch_stocks_by_industry(country, sector, industry)

    from fincept_terminal.themes import console
    if stock_data.empty:
        console.print(f"[bold red]No stocks available for {industry} in {sector}, {country}.[/bold red]",
                      style="danger")
    else:
        display_equities(stock_data)

        while True:
            console.print("\n")
            from rich.prompt import Prompt
            choice = Prompt.ask("Would you like to search for more information on a specific stock? (yes/no)")
            if choice.lower() == 'yes':
                ticker_name = Prompt.ask("Please enter the stock symbol or company name (partial or full)")
                from fincept_terminal.const import find_closest_ticker
                matches = find_closest_ticker(ticker_name, stock_data)
                if not matches:
                    console.print(f"[bold red]No matching tickers found for '{ticker_name}'.[/bold red]", style="danger")
                elif len(matches) == 1:
                    display_stock_info(matches[0]['symbol'])
                else:
                    # If multiple matches are found, ask the user to select one
                    console.print(f"[bold yellow]Multiple matches found for '{ticker_name}':[/bold yellow]\n")
                    from fincept_terminal.const import display_in_columns
                    display_in_columns("Select a Stock", [f"{m['symbol']} - {m['name']}" for m in matches])
                    user_choice = Prompt.ask("Please select the stock by entering the number")
                    selected_ticker = matches[int(user_choice) - 1]['symbol']
                    display_stock_info(selected_ticker)
            else:
                return  # Return to the previous menu instead of directly to the main menu


def display_company_financials(ticker):
    """Fetch and display the financials of the company using yfinance."""
    stock = yf.Ticker(ticker)
    # Get financials (e.g., Income Statement, Balance Sheet, Cash Flow)
    console.print(f"\n[highlight]Fetching Financials for {ticker}...[/highlight]\n", style="info")

    # Display Income Statement
    financials = stock.financials
    if not financials.empty:
        console.print("[highlight]Income Statement:[/highlight]", style="highlight")
        display_financial_table(financials, "Income Statement")
    else:
        console.print("[bold red]No income statement data available.[/bold red]", style="danger")

    # Display Balance Sheet
    balance_sheet = stock.balance_sheet
    if not balance_sheet.empty:
        console.print("\n[highlight]Balance Sheet:[/highlight]", style="highlight")
        display_financial_table(balance_sheet, "Balance Sheet")
    else:
        console.print("[bold red]No balance sheet data available.[/bold red]", style="danger")

    # Display Cash Flow
    cashflow = stock.cashflow
    if not cashflow.empty:
        console.print("\n[highlight]Cash Flow:[/highlight]", style="highlight")
        display_financial_table(cashflow, "Cash Flow")
    else:
        console.print("[bold red]No cash flow data available.[/bold red]", style="danger")

    console.print("\n[bold green]Financials displayed successfully![/bold green]", style="success")


def display_financial_table(financial_data, title):
    """Helper function to display financial data in a table format."""
    if financial_data.empty:
        return  # Exit if there's no data to display

    from rich.table import Table
    table = Table(title=title, show_lines=True, header_style="bold white on #282828")

    # Add columns (years) to the table
    table.add_column("Item", style="cyan on #282828")
    for col in financial_data.columns:
        table.add_column(str(col.date()), style="yellow on #282828")

    # Add rows (financial items) to the table
    for index, row in financial_data.iterrows():
        import pandas as pd
        table.add_row(index, *[f"{value:,.0f}" if not pd.isna(value) else "N/A" for value in row])

    # Print the table
    console.print(table)

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
    console.print(f"\n[highlight]Business Summary[/highlight]: {filtered_info.get('longBusinessSummary', 'N/A')}",
                  style="info")

    # Display `companyOfficers` in a separate table if it exists
    if 'companyOfficers' in stock_info:
        console.print("\n[highlight]Company Officers:[/highlight]", style="highlight")
        from rich.table import Table
        officers_table = Table(show_lines=True, style="info", header_style="bold white on #282828")

        officers_table.add_column("Name", style="cyan on #282828")
        officers_table.add_column("Title", style="green on #282828")
        officers_table.add_column("Age", style="yellow on #282828")
        officers_table.add_column("Total Pay", style="magenta on #282828")
        officers_table.add_column("Exercised Value", style="blue on #282828")
        officers_table.add_column("Unexercised Value", style="red on #282828")

        for officer in stock_info['companyOfficers']:
            name = officer.get('name', 'N/A')
            title = officer.get('title', 'N/A')
            age = str(officer.get('age', 'N/A'))
            total_pay = str(officer.get('totalPay', 'N/A'))
            exercised_value = str(officer.get('exercisedValue', 'N/A'))
            unexercised_value = str(officer.get('unexercisedValue', 'N/A'))

            officers_table.add_row(name, title, age, total_pay, exercised_value, unexercised_value)

        console.print(officers_table)
        console.print("\n")

    # Remove `longBusinessSummary` and `companyOfficers` from the filtered info as we already displayed them
    filtered_info.pop('longBusinessSummary', None)
    filtered_info.pop('companyOfficers', None)

    from fincept_terminal.const import display_info_in_three_columns
    # Display the remaining data in three columns
    display_info_in_three_columns(filtered_info)

    # Ask if the user wants to export the data
    from rich.prompt import Prompt
    choice = Prompt.ask("\nWould you like to export the data to CSV or Excel? (yes/no)", default="no")

    if choice.lower() == "yes":
        export_choice = Prompt.ask("Choose export format: CSV or Excel?", choices=["csv", "excel"], default="csv")
        export_stock_info(filtered_info, ticker, export_choice)
        console.print(f"[bold green]Data has been exported successfully![/bold green]", style="success")

    # Ask if the user wants to query financials
    query_financials = Prompt.ask("\nWould you like to query the financials of this company? (yes/no)", default="no")
    if query_financials.lower() == "yes":
        display_company_financials(ticker)
    else:
        console.print("[bold yellow]Returning to the previous menu...[/bold yellow]", style="info")


def export_stock_info(info, ticker, export_format):
    """Export stock information to CSV or Excel."""
    # Convert the info dictionary to a pandas DataFrame
    import pandas as pd
    df = pd.DataFrame(list(info.items()), columns=["Attribute", "Value"])

    # Define the file name
    file_name = f"{ticker}_stock_info.{export_format}"

    from fincept_terminal.themes import console
    try:
        if export_format == "csv":
            df.to_csv(file_name, index=False)
        elif export_format == "excel":
            df.to_excel(file_name, index=False)
        console.print(f"[bold green]Stock information successfully exported to {file_name}![/bold green]",
                      style="success")
    except Exception as e:
        console.print(f"[bold red]Failed to export data: {e}[/bold red]", style="danger")


def display_equities(stock_data):
    """Display stock data in a tabular format."""
    from rich.table import Table
    table = Table(title="Available Stocks", title_justify="left", header_style="bold", show_lines=True)
    table.add_column("Symbol", style="cyan", justify="left", width=15)
    table.add_column("Name", style="green", justify="left", width=50)
    table.add_column("Market Cap", style="yellow", justify="left", width=20)

    for _, row in stock_data.iterrows():
        table.add_row(str(row['symbol']), str(row['name']), str(row['market_cap']))

    from fincept_terminal.themes import console
    console.print("\n")
    console.print(table)