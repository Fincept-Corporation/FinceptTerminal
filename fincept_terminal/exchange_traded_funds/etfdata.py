import yfinance as yf
from rich.prompt import Prompt
from rich.console import Console
from fincept_terminal.FinceptTerminalUtils.themes import console
from fincept_terminal.FinceptTerminalUtils.const import display_in_columns, display_info_in_three_columns
import financedatabase as fd

console = Console()

# Step 1: Fetch the list of ETF families
def fetch_etf_families():
    """
    Fetch the list of ETF families using the financedatabase library.
    """
    try:
        etfs = fd.ETFs()
        etf_data = etfs.select()
        families = etf_data['family'].dropna().unique().tolist()
        console.print("[bold green]ETF families fetched successfully.[/bold green]")
        return families
    except Exception as e:
        console.print(f"[bold red]Failed to fetch ETF families: {e}[/bold red]")
        return []

# Step 2: Paginate the ETF families list
def display_etf_families_paginated(families, start_index=0, page_size=21):
    """
    Paginate and display a list of ETF families for user selection.
    """
    end_index = min(start_index + page_size, len(families))
    families_page = families[start_index:end_index]

    display_in_columns(f"Select an ETF Family (Showing {start_index + 1} - {end_index})", families_page)

    action = Prompt.ask(
        "\n[bold cyan]Next Page (N) / Previous Page (P) / Select a Family (1-{0}) / Exit (E)[/bold cyan]".format(len(families_page))
    )

    if action.lower() == 'n':
        if end_index < len(families):
            return display_etf_families_paginated(families, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_etf_families_paginated(families, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_etf_families_paginated(families, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_etf_families_paginated(families, start_index)
    elif action.lower() == 'e':
        console.print("[bold yellow]Exiting ETF Family selection...[/bold yellow]")
        return None
    else:
        try:
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(families):
                return families[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_etf_families_paginated(families, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number, 'N', 'P', or 'E'.[/bold red]")
            return display_etf_families_paginated(families, start_index)

def display_etf_paginated(families, start_index=0, page_size=21):
    """
    Paginate and display a list of ETF families for user selection.
    """
    end_index = min(start_index + page_size, len(families))
    families_page = families[start_index:end_index]

    display_in_columns(f"Select an ETF (Showing {start_index + 1} - {end_index})", families_page)

    action = Prompt.ask(
        "\n[bold cyan]Next Page (N) / Previous Page (P) / Select a Family (1-{0}) / Exit (E)[/bold cyan]".format(len(families_page))
    )

    if action.lower() == 'n':
        if end_index < len(families):
            return display_etf_families_paginated(families, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_etf_families_paginated(families, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_etf_families_paginated(families, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_etf_families_paginated(families, start_index)
    elif action.lower() == 'e':
        console.print("[bold yellow]Exiting ETF Family selection...[/bold yellow]")
        return None
    else:
        try:
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(families):
                return families[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_etf_families_paginated(families, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number, 'N', 'P', or 'E'.[/bold red]")
            return display_etf_families_paginated(families, start_index)

# Step 3: Fetch ETFs by family
def fetch_etfs_by_family(family_name):
    """
    Fetch ETF details by family name using the financedatabase library.
    """
    try:
        etfs = fd.ETFs()
        etf_data = etfs.select(family=family_name)
        if etf_data.empty:
            console.print(f"[bold yellow]No ETFs found for family: {family_name}[/bold yellow]")
            return []
        console.print(f"[bold green]Successfully fetched ETFs for family: {family_name}[/bold green]")

        etf_data = etf_data.reset_index()

        return etf_data.to_dict('records')  # Convert DataFrame to a list of dictionaries
    except Exception as e:
        console.print(f"[bold red]Failed to fetch ETFs for family {family_name}: {e}[/bold red]")
        return []

# Step 4: Fetch ETF details by symbol using yfinance
def fetch_etf_data_by_symbol(symbol):
    """
    Fetch ETF details and historical data using yfinance.
    """
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info
        filtered_info = {k: v for k, v in info.items() if k not in ['messageBoardId', 'gmtOffSetMilliseconds', 'uuid', 'maxAge']}
        return filtered_info
    except Exception as e:
        console.print(f"[bold red]Failed to fetch ETF data for symbol {symbol}: {e}[/bold red]")
        return {}

# Step 5: Main function to show the ETF menu
def show_etf_market_menu():
    """
    Show the ETF market menu and handle user interactions.
    """
    console.print("[bold cyan]EXCHANGE TRADED FUNDS MENU[/bold cyan]\n")

    while True:
        options = ["ETF Search", "Search by Symbol", "Back to Main Menu"]
        display_in_columns("Select an Option", options)

        choice = Prompt.ask("Enter the number corresponding to your choice")

        if choice == "1":  # ETF Search
            families = fetch_etf_families()
            if not families:
                console.print("[bold red]No ETF families available.[/bold red]")
                continue

            # Paginate and display ETF families
            selected_family = display_etf_families_paginated(families)
            if selected_family:
                etfs = fetch_etfs_by_family(selected_family)
                if not etfs:
                    continue

                # Paginate and display ETFs in the selected family
                etf_names = [etf["name"] for etf in etfs]
                selected_etf_name = display_etf_paginated(etf_names)
                if selected_etf_name:
                    # Extract the ETF symbol and find the matching ETF
                    selected_etf_data = next((etf for etf in etfs if etf["name"] == selected_etf_name), None)

                    console.print(f"\n[bold yellow]ETF Summary: {selected_etf_data['name']}[/bold yellow]\n")

                    # Fetch ETF data from yfinance using its symbol
                    etf_data = fetch_etf_data_by_symbol(selected_etf_data['symbol'])
                    display_info_in_three_columns(etf_data)

        elif choice == "2":  # Search by Symbol
            symbol = Prompt.ask("Enter the ETF symbol")
            etf_data = fetch_etf_data_by_symbol(symbol)
            if etf_data:
                display_info_in_three_columns(etf_data)

        elif choice == "3":  # Back to Main Menu
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]")
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            break

