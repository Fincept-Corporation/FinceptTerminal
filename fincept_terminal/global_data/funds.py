import yfinance as yf
from fincept_terminal.utils.themes import console
from fincept_terminal.utils.const import display_in_columns, display_info_in_three_columns
import financedatabase as fd


# Step 1: Fetch the list of fund exchanges
def fetch_fund_exchanges():
    """
    Fetch a list of unique fund exchanges using financedatabase.
    """
    try:
        funds = fd.Funds()
        funds_data = funds.select()
        exchanges = funds_data["exchange"].dropna().unique().tolist()
        console.print("[bold green]Successfully fetched fund exchanges.[/bold green]")
        return exchanges
    except Exception as e:
        console.print(f"[bold red]Failed to fetch fund exchanges: {e}[/bold red]")
        return []


# Step 2: Paginate the exchange list (21 per page)
def display_fund_exchanges_paginated(exchanges, start_index=0, page_size=21):
    """
    Paginate and display a list of fund exchanges for user selection.
    """
    end_index = min(start_index + page_size, len(exchanges))
    exchanges_page = exchanges[start_index:end_index]

    display_in_columns(f"Select an Exchange (Showing {start_index + 1} - {end_index})", exchanges_page)

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
    """
    Fetch funds filtered by the selected exchange using financedatabase.
    """
    try:
        funds = fd.Funds()
        funds_data = funds.select()
        filtered_funds = funds_data[funds_data["exchange"] == exchange]
        if filtered_funds.empty:
            console.print(f"[bold yellow]No funds found for exchange: {exchange}[/bold yellow]")
            return []
        console.print(f"[bold green]Successfully fetched funds for exchange: {exchange}[/bold green]")
        filtered_funds = filtered_funds.reset_index()
        return filtered_funds.to_dict("records")
    except Exception as e:
        console.print(f"[bold red]Failed to fetch funds for exchange {exchange}: {e}[/bold red]")
        return []


# Step 4: Paginate the list of funds (21 per page)
def display_funds_paginated(funds, start_index=0, page_size=21):
    """
    Paginate and display a list of funds for user selection.
    """
    end_index = min(start_index + page_size, len(funds))
    funds_page = funds[start_index:end_index]

    # Extract only the names of the funds to display
    fund_names = [fund['name'] for fund in funds_page]

    # Display the current page of funds
    display_in_columns(f"Funds (Showing {start_index + 1} - {end_index})", fund_names)

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
    """
    Fetch detailed fund data from Yahoo Finance by symbol.
    """
    ticker = yf.Ticker(symbol)
    info = ticker.info
    filtered_info = {k: v for k, v in info.items() if k not in ['gmtOffSetMilliseconds', 'uuid', 'maxAge']}
    return filtered_info


# Step 6: Main function to show the Global Funds menu
def show_global_funds_menu():
    """
    Show the Global Funds menu and handle user interactions.
    """
    console.print("[bold cyan]GLOBAL FUNDS MENU[/bold cyan]\n")

    while True:
        options = ["Global Funds", "Search Fund With Symbol", "Back to Main Menu"]
        display_in_columns("Select an Option", options)

        from rich.prompt import Prompt
        choice = Prompt.ask("Enter the number corresponding to your choice")

        if choice == "1":  # Global Funds
            exchanges = fetch_fund_exchanges()
            if not exchanges:
                continue

            selected_exchange = display_fund_exchanges_paginated(exchanges)
            if selected_exchange:
                funds = fetch_funds_by_exchange(selected_exchange)
                if not funds:
                    continue

                selected_fund = display_funds_paginated(funds)
                if selected_fund:
                    display_fund_info(selected_fund)
                    fund_data = fetch_fund_data_by_symbol(selected_fund.get("symbol", ""))
                    display_info_in_three_columns(fund_data)

        elif choice == "2":  # Search Fund With Symbol
            symbol = Prompt.ask("Enter the fund symbol")
            fund_data = fetch_fund_data_by_symbol(symbol)
            if fund_data:
                display_info_in_three_columns(fund_data)

        elif choice == "3":  # Back to Main Menu
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]")
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            break


# Step 7: Display additional fund info (summary, manager, etc.)
def display_fund_info(fund):
    """
    Display additional details of a selected fund.
    """
    details = [
        f"Name: {fund.get('name', 'N/A')}",
        f"Summary: {fund.get('summary', 'N/A')}",
        f"Category Group: {fund.get('category_group', 'N/A')}",
        f"Category: {fund.get('category', 'N/A')}",
        f"Family: {fund.get('family', 'N/A')}",
        f"Market: {fund.get('market', 'N/A')}",
    ]
    display_in_columns("Fund Details", details)
