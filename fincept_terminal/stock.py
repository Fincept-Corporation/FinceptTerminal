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


def show_industries_in_sector(country, sector):
    """Fetch industries for the selected sector and allow the user to select one."""
    from fincept_terminal.data_fetcher import fetch_industries_by_sector
    industries = fetch_industries_by_sector(country, sector)
    from fincept_terminal.themes import console

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
                closest_ticker = find_closest_ticker(ticker_name, stock_data)
                if closest_ticker:
                    display_stock_info(closest_ticker)
                else:
                    console.print(f"[bold red]No matching ticker found for '{ticker_name}'.[/bold red]", style="danger")
            else:
                return  # Return to the previous menu instead of directly to the main menu


def display_stock_info(ticker):
    """Fetch and display detailed stock information using yfinance."""
    import yfinance as yf
    stock = yf.Ticker(ticker)
    stock_info = stock.info

    if not stock_info:
        from fincept_terminal.themes import console
        console.print(f"[bold red]No information found for {ticker}.[/bold red]", style="danger")
        return

    # Filter out null values and unwanted keys
    filtered_info = {k: v for k, v in stock_info.items() if v is not None and k not in [
        'uuid', 'gmtOffSetMilliseconds', 'messageBoardId', 'compensationAsOfEpochDate', 'maxAge'
    ]}

    from fincept_terminal.themes import console
    # Display `longBusinessSummary` in one row (full width)
    console.print(f"\n[highlight]Business Summary[/highlight]: {filtered_info.get('longBusinessSummary', 'N/A')}",
                  style="info")

    # Display `companyOfficers` in a structured way
    if 'companyOfficers' in filtered_info:
        console.print("\n[highlight]Company Officers:[/highlight]", style="highlight")
        from rich.table import Table
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

    from fincept_terminal.const import display_info_in_three_columns
    # Display the remaining data in three columns
    display_info_in_three_columns(filtered_info)

    # Ask if the user wants to export the data
    from rich.prompt import Prompt
    choice = Prompt.ask("\nWould you like to export the data to CSV or Excel? (yes/no)", default="no")

    if choice.lower() == "yes":
        export_choice = Prompt.ask("Choose export format: CSV or Excel?", choices=["csv", "excel"], default="csv")
        export_stock_info(filtered_info, ticker, export_choice)


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