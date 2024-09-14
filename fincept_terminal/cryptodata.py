import requests
from rich.prompt import Prompt
from rich.progress import Progress
import yfinance as yf
from fincept_terminal.themes import console  # Using existing console

# API Endpoints
CURRENCY_API = "https://fincept.share.zrok.io/FinanceDB/cryptos/currency/data?unique=true"
CRYPTO_BY_CURRENCY_API = "https://fincept.share.zrok.io/FinanceDB/cryptos/currency/filter?value={}"

# Constants
ROWS_PER_COLUMN = 7
COLUMNS_PER_PAGE = 9
ITEMS_PER_PAGE = ROWS_PER_COLUMN * COLUMNS_PER_PAGE  # 7 rows per column * 9 columns per page = 63 items per page

# Fetch available currencies from the API
def fetch_available_currencies():
    with Progress() as progress:
        task = progress.add_task("[cyan]Fetching currencies...", total=100)
        response = requests.get(CURRENCY_API)
        data = response.json()
        progress.update(task, advance=100)
        return data['currency']

# Fetch all cryptocurrencies by currency
def fetch_cryptos_by_currency(currency):
    url = CRYPTO_BY_CURRENCY_API.format(currency)
    with Progress() as progress:
        task = progress.add_task(f"[cyan]Fetching cryptocurrencies for {currency}...", total=100)
        response = requests.get(url)
        progress.update(task, advance=100)
    return response.json()

# Display information in three columns, skipping long values
def display_info_in_three_columns(info):
    """Display key-value pairs in three columns, skipping long values."""
    from rich.table import Table
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

# Display the items in a paginated table with 7 rows and up to 9 columns per page, allowing index selection
def display_paginated_columns(title, items, current_page=1, allow_selection=False):
    """Display items in paginated table format (7 rows per column, up to 9 columns per page), with optional index selection."""
    from rich.table import Table

    start_index = (current_page - 1) * ITEMS_PER_PAGE
    end_index = start_index + ITEMS_PER_PAGE

    # Get the items for the current page
    page_items = items[start_index:end_index]
    total_pages = (len(items) + ITEMS_PER_PAGE - 1) // ITEMS_PER_PAGE

    # Calculate the number of columns to display dynamically
    num_columns = (len(page_items) + ROWS_PER_COLUMN - 1) // ROWS_PER_COLUMN

    table = Table(title=f"{title} (Page {current_page}/{total_pages})", header_style="bold green on #282828", show_lines=True)

    # Add only the necessary columns based on data length
    for _ in range(min(num_columns, COLUMNS_PER_PAGE)):
        table.add_column("", style="highlight", justify="left")

    # Add rows in columns (7 rows per column)
    rows = [[] for _ in range(ROWS_PER_COLUMN)]
    for index, item in enumerate(page_items):
        row_index = index % ROWS_PER_COLUMN
        rows[row_index].append(f"{start_index + index + 1}. {item}")

    # Fill the table
    for row in rows:
        row += [""] * (num_columns - len(row))  # Only add empty strings for missing columns
        table.add_row(*row)

    console.print(table)

    # Check if more pages are available
    if allow_selection:
        return Prompt.ask("\nSelect an index to view details, or 'n' for next page, 'p' for previous, or 'q' to quit")

    if current_page < total_pages:
        next_page = Prompt.ask(f"Page {current_page} of {total_pages}. Enter 'n' for next page, 'p' for previous, or 'q' to quit", default="n")
        if next_page == "n":
            display_paginated_columns(title, items, current_page + 1)
        elif next_page == "p" and current_page > 1:
            display_paginated_columns(title, items, current_page - 1)
        elif next_page != "q":
            console.print("[bold red]Invalid input. Returning to the main menu.[/bold red]")

# Main menu for Crypto operations
def crypto_main_menu():
    while True:
        console.print("\n[bold cyan]CRYPTO MENU[/bold cyan]\n")
        menu_items = ["1. Search Cryptocurrency by Symbol (Using yfinance)",
                      "2. View All Cryptocurrencies (Select Currency First)",
                      "3. Exit"]
        display_paginated_columns("CRYPTO MENU", menu_items)

        choice = Prompt.ask("\nEnter your choice")

        if choice == "1":
            symbol = Prompt.ask("Enter cryptocurrency symbol (e.g., BTC, ETH)").upper()
            fetch_and_display_yfinance_data(symbol)  # Fetch and display yfinance data
        elif choice == "2":
            view_all_cryptocurrencies()  # View all cryptocurrencies
        elif choice == "3":
            console.print("[bold green]Exiting Crypto Menu...[/bold green]")
            break
        else:
            console.print("[bold red]Invalid choice. Please try again.[/bold red]")

# Crypto Menu - Option 2: View All Cryptocurrencies
def view_all_cryptocurrencies():
    # Fetch and display available currencies
    currencies = fetch_available_currencies()
    console.print(f"\n[bold cyan]Total Currencies Available: {len(currencies)}[/bold cyan]")
    display_paginated_columns("Available Currencies", currencies)

    # Ask the user to select a currency
    selected_currency_index = Prompt.ask("Select a currency by entering the index")
    try:
        selected_currency = currencies[int(selected_currency_index) - 1]
    except (ValueError, IndexError):
        console.print("[bold red]Invalid currency selection! Returning to main menu.[/bold red]")
        return

    console.print(f"\n[bold green]Fetching cryptocurrencies for {selected_currency}...[/bold green]")

    # Fetch and display cryptocurrencies for the selected currency
    cryptos = fetch_cryptos_by_currency(selected_currency)
    if not cryptos:
        console.print(f"[bold red]No cryptocurrencies found for {selected_currency}.[/bold red]")
        return

    # Display the available cryptocurrencies with pagination and allow index selection
    selected_crypto_index = display_paginated_columns(f"Cryptocurrencies for {selected_currency}", [crypto['symbol'] for crypto in cryptos], allow_selection=True)

    if selected_crypto_index.isdigit():
        try:
            selected_crypto = cryptos[int(selected_crypto_index) - 1]
        except (ValueError, IndexError):
            console.print("[bold red]Invalid cryptocurrency selection! Returning to main menu.[/bold red]")
            return

        # Display the selected cryptocurrency details
        fetch_and_display_yfinance_data(selected_crypto['symbol'])
        prompt_for_another_query()
    elif selected_crypto_index.lower() not in ['n', 'p', 'q']:
        console.print("[bold red]Invalid input. Returning to the main menu.[/bold red]")

# Prompt the user if they want to query another cryptocurrency
def prompt_for_another_query():
    choice = Prompt.ask("\nWould you like to query another cryptocurrency? (yes/no)", default="no").lower()
    if choice == "yes":
        from fincept_terminal.cli import show_main_menu
        show_main_menu()
    else:
        console.print("[bold green]Returning to main menu...[/bold green]")

# Fetch and display yfinance data for the given symbol
def fetch_and_display_yfinance_data(symbol):
    ticker = yf.Ticker(symbol)
    data = ticker.info  # Get the JSON data

    # Define the keys to exclude
    excluded_keys = {'uuid', 'messageBoardId', 'maxAge'}

    # Check if description or summary exists and remove it from the table
    description = data.pop('description', None)
    summary = data.pop('summary', None)

    # Filter the data
    filtered_data = {k: v for k, v in data.items() if k not in excluded_keys}

    # Display the data in three columns
    display_info_in_three_columns(filtered_data)

    # Display the summary/description separately
    if description:
        console.print(f"\n[bold cyan]Description[/bold cyan]: {description}")
    if summary:
        console.print(f"\n[bold cyan]Summary[/bold cyan]: {summary}")
