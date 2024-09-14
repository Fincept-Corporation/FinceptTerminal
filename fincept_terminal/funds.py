import requests
import yfinance as yf
from fincept_terminal.themes import console
from fincept_terminal.const import display_in_columns,display_info_in_three_columns

# Step 1: Fetch the list of exchanges
def fetch_fund_exchanges():
    url = "https://fincept.share.zrok.io/FinanceDB/funds/exchange/data?unique=true"
    response = requests.get(url)
    if response.status_code == 200:
        data = response.json()
        # Filter out null values and invalid entries
        return [exchange for exchange in data['exchange'] if exchange and exchange not in [
            " de 10 % de la volatilit\u001aÉ¬© de l\u001a¢¬Ä¬ôindice.",
            " s\u001a¢¬Ä¬ôil est plus \u001aÉ¬©lev\u001aÉ¬©"]]
    else:
        console.print(f"[bold red]Failed to fetch exchanges: {response.status_code}[/bold red]")
        return []


# Step 2: Paginate the exchange list (21 per page)
def display_fund_exchanges_paginated(exchanges, start_index=0, page_size=21):
    end_index = min(start_index + page_size, len(exchanges))
    exchanges_page = exchanges[start_index:end_index]

    # Display the current page of exchanges
    display_in_columns(f"Select an Exchange (Showing {start_index + 1} - {end_index})", exchanges_page)

    # Ask the user to navigate or select an exchange
    from rich.prompt import Prompt
    action = Prompt.ask(
        "\n[bold cyan]Next Page (N) / Previous Page (P) / Select an Exchange (1-{0})[/bold cyan]".format(
            len(exchanges_page)))

    if action.lower() == 'n':
        if end_index < len(exchanges):
            return display_fund_exchanges_paginated(exchanges, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_fund_exchanges_paginated(exchanges, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_fund_exchanges_paginated(exchanges, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_fund_exchanges_paginated(exchanges, start_index)
    else:
        try:
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(exchanges):
                return exchanges[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_fund_exchanges_paginated(exchanges, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number or 'N'/'P'.[/bold red]")
            return display_fund_exchanges_paginated(exchanges, start_index)


# Step 3: Fetch funds by exchange
def fetch_funds_by_exchange(exchange):
    url = f"https://fincept.share.zrok.io/FinanceDB/funds/exchange/filter?value={exchange}"
    response = requests.get(url)
    if response.status_code == 200:
        return response.json()
    else:
        console.print(f"[bold red]Failed to fetch funds for {exchange}: {response.status_code}[/bold red]")
        return []


# Step 4: Paginate the list of funds (21 per page)
def display_funds_paginated(funds, start_index=0, page_size=21):
    end_index = min(start_index + page_size, len(funds))
    funds_page = funds[start_index:end_index]

    # Extract only the names of the funds to display
    fund_names = [fund['name'] for fund in funds_page]

    # Display the current page of funds
    display_in_columns(f"Funds (Showing {start_index + 1} - {end_index})", fund_names)

    # Ask the user to navigate or select a fund
    from rich.prompt import Prompt
    action = Prompt.ask(
        "\n[bold cyan]Next Page (N) / Previous Page (P) / Select a Fund (1-{0})[/bold cyan]".format(len(fund_names)))

    if action.lower() == 'n':
        if end_index < len(funds):
            return display_funds_paginated(funds, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_funds_paginated(funds, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_funds_paginated(funds, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_funds_paginated(funds, start_index)
    else:
        try:
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(funds):
                return funds[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_funds_paginated(funds, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number or 'N'/'P'.[/bold red]")
            return display_funds_paginated(funds, start_index)


# Step 5: Fetch fund data by symbol using yfinance
def fetch_fund_data_by_symbol(symbol):
    ticker = yf.Ticker(symbol)
    info = ticker.info
    # Filter out unnecessary fields
    filtered_info = {k: v for k, v in info.items() if
                     k not in ['gmtOffSetMilliseconds', 'uuid', 'maxAge', 'firstTradeDateEpochUtc']}
    return filtered_info


# Step 6: Main function to show the Global Funds menu
def show_global_funds_menu():
    console.print("[bold cyan]GLOBAL FUNDS MENU[/bold cyan]\n", style="info")

    from rich.prompt import Prompt

    # Display two options: Global Funds or Search by Symbol
    options = ["Global Funds", "Search Fund With Symbol"]
    display_in_columns("Select an Option", options)

    choice = Prompt.ask("Enter the number corresponding to your choice")

    if choice == "1":  # Global Funds
        exchanges = fetch_fund_exchanges()
        if not exchanges:
            return
        selected_exchange = display_fund_exchanges_paginated(exchanges)
        if selected_exchange:
            funds = fetch_funds_by_exchange(selected_exchange)
            if not funds:
                return

            # Display the funds with pagination (21 per page)
            selected_fund = display_funds_paginated(funds)

            # Display the selected fund details and fetch data from yfinance
            if selected_fund:
                display_fund_info(selected_fund)
                fund_data = fetch_fund_data_by_symbol(selected_fund['symbol'])
                display_info_in_three_columns(fund_data)

    elif choice == "2":  # Search Fund With Symbol
        symbol = Prompt.ask("Enter the fund symbol")
        fund_data = fetch_fund_data_by_symbol(symbol)
        display_info_in_three_columns(fund_data)

    # Ask if the user wants to query another fund or exit to the main menu
    continue_query = Prompt.ask("Do you want to query another fund? (yes/no)")
    if continue_query.lower() == 'yes':
        show_global_funds_menu()  # Redirect back to the Global Funds menu
    else:
        console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
        from fincept_terminal.cli import show_main_menu
        show_main_menu()


# Step 7: Display additional fund info (summary, manager, etc.)
def display_fund_info(fund):
    details = [
        f"Summary: {fund.get('summary', 'N/A')}",
        f"Manager Name: {fund.get('manager_name', 'N/A')}",
        f"Manager Bio: {fund.get('manager_bio', 'N/A')}",
        f"Category Group: {fund.get('category_group', 'N/A')}",
        f"Category: {fund.get('category', 'N/A')}",
        f"Family: {fund.get('family', 'N/A')}",
        f"Market: {fund.get('market', 'N/A')}",
    ]
    display_in_columns("Fund Details", details)

