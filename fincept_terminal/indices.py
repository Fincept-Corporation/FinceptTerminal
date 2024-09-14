import requests
import yfinance as yf
from fincept_terminal.themes import console
from fincept_terminal.const import display_in_columns,display_info_in_three_columns

# Step 1: Fetch the list of exchanges for global indices
def fetch_indices_exchanges():
    url = "https://fincept.share.zrok.io/FinanceDB/indices/exchange/data?unique=true"
    response = requests.get(url)
    if response.status_code == 200:
        data = response.json()
        # Filter out null values
        return [exchange for exchange in data['exchange'] if exchange]
    else:
        console.print(f"[bold red]Failed to fetch exchanges: {response.status_code}[/bold red]")
        return []


# Step 2: Display the full list of exchanges (no pagination)
def display_indices_exchanges_full(exchanges):
    # Display all exchanges in one go
    display_in_columns("Select an Exchange", exchanges)

    # Ask the user to select an exchange
    from rich.prompt import Prompt
    action = Prompt.ask("Select an Exchange (Enter the number corresponding to the exchange)")

    try:
        selected_index = int(action) - 1
        if 0 <= selected_index < len(exchanges):
            return exchanges[selected_index]
        else:
            console.print("[bold red]Invalid selection. Please try again.[/bold red]")
            return display_indices_exchanges_full(exchanges)
    except ValueError:
        console.print("[bold red]Invalid input. Please enter a number.[/bold red]")
        return display_indices_exchanges_full(exchanges)


# Step 3: Fetch indices by exchange
def fetch_indices_by_exchange(exchange):
    url = f"https://fincept.share.zrok.io/FinanceDB/indices/exchange/filter?value={exchange}"
    response = requests.get(url)
    if response.status_code == 200:
        return response.json()
    else:
        console.print(f"[bold red]Failed to fetch indices for {exchange}: {response.status_code}[/bold red]")
        return []


# Step 4: Paginate the list of indices (21 per page)
def display_indices_paginated(indices, start_index=0, page_size=21):
    end_index = min(start_index + page_size, len(indices))
    indices_page = indices[start_index:end_index]

    # Extract only the names of the indices to display
    index_names = [index['name'] for index in indices_page]

    # Display the current page of indices
    display_in_columns(f"Indices (Showing {start_index + 1} - {end_index})", index_names)

    # Ask the user to navigate or select an index
    from rich.prompt import Prompt
    action = Prompt.ask(
        "\n[bold cyan]Next Page (N) / Previous Page (P) / Select an Index (1-{0})[/bold cyan]".format(len(index_names)))

    if action.lower() == 'n':
        if end_index < len(indices):
            return display_indices_paginated(indices, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_indices_paginated(indices, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_indices_paginated(indices, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_indices_paginated(indices, start_index)
    else:
        try:
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(indices):
                return indices[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_indices_paginated(indices, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number or 'N'/'P'.[/bold red]")
            return display_indices_paginated(indices, start_index)


# Step 5: Fetch index data by symbol using yfinance and historical data
def fetch_index_data_by_symbol_with_history(symbol):
    ticker = yf.Ticker(symbol)

    # Fetch general info
    info = ticker.info
    # Filter out unnecessary fields
    filtered_info = {k: v for k, v in info.items() if
                     k not in ['maxAge', 'uuid', 'messageBoardId', 'gmtOffSetMilliseconds']}

    # Fetch last 7 days of historical data
    history_data = ticker.history(period='7d')  # Fetch last 7 days of historical data

    # Extract important columns from historical data
    history_table = history_data[['Open', 'High', 'Low', 'Close', 'Volume']].tail(7)  # Limit to last 7 records

    return filtered_info, history_table


# Step 8: Display details in three columns and price history
def display_info_with_history(info, history_table):
    """Display key-value pairs in three columns, and display the last 7 days of price data."""
    from rich.table import Table

    # Create the main info table (three columns)
    table = Table(show_lines=True, style="info", header_style="bold white on #282828")
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

    # Create the historical price data table
    history_table_display = Table(show_lines=True, header_style="bold white on #282828")
    history_table_display.add_column("Date", style="cyan on #282828", width=15)
    history_table_display.add_column("Open", style="green on #282828", width=15)
    history_table_display.add_column("High", style="green on #282828", width=15)
    history_table_display.add_column("Low", style="green on #282828", width=15)
    history_table_display.add_column("Close", style="green on #282828", width=15)

    # Add rows for the last 7 days of price data
    for idx, row in history_table.iterrows():
        history_table_display.add_row(str(idx.date()), f"{row['Open']:.2f}", f"{row['High']:.2f}", f"{row['Low']:.2f}",
                                      f"{row['Close']:.2f}")

    console.print("\n[bold yellow]Last 7 Days Price Data[/bold yellow]")
    console.print(history_table_display)


# Step 6: Main function to show the Global Indices menu
def show_global_indices_menu():
    console.print("[bold cyan]GLOBAL INDICES MENU[/bold cyan]\n", style="info")

    from rich.prompt import Prompt

    # Display two options: Global Indices or Search by Symbol
    options = ["Global Indices", "Search Indices Symbol"]
    display_in_columns("Select an Option", options)

    choice = Prompt.ask("Enter the number corresponding to your choice")

    if choice == "1":  # Global Indices
        exchanges = fetch_indices_exchanges()
        if not exchanges:
            return
        selected_exchange = display_indices_exchanges_full(exchanges)  # This will display the full list of exchanges
        if selected_exchange:
            indices = fetch_indices_by_exchange(selected_exchange)
            if not indices:
                return

            # Display the indices and let user select one
            selected_index = display_indices_paginated(indices)

            # Fetch and display the selected index details and the last 7 days of price history
            if selected_index:
                display_index_info(selected_index)
                index_data, history_table = fetch_index_data_by_symbol_with_history(selected_index['symbol'])
                display_info_with_history(index_data, history_table)  # Correctly passing index data and history table

    elif choice == "2":  # Search Indices Symbol
        symbol = Prompt.ask("Enter the index symbol")
        index_data, history_table = fetch_index_data_by_symbol_with_history(symbol)
        display_info_with_history(index_data, history_table)

    # Ask if the user wants to query another index or exit to the main menu
    continue_query = Prompt.ask("Do you want to query another index? (yes/no)")
    if continue_query.lower() == 'yes':
        show_global_indices_menu()  # Redirect back to the Global Indices menu
    else:
        console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
        from fincept_terminal.cli import show_main_menu
        show_main_menu()


# Step 7: Display additional index info (like currency, market, etc.)
def display_index_info(index):
    details = [
        f"Name: {index.get('name', 'N/A')}",
        f"Currency: {index.get('currency', 'N/A')}",
        f"Market: {index.get('market', 'N/A')}",
        f"Exchange: {index.get('exchange', 'N/A')}",
        f"Exchange Timezone: {index.get('exchange timezone', 'N/A')}"
    ]
    display_in_columns("Index Details", details)

