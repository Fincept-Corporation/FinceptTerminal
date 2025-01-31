from fincept_terminal.utils.const import display_in_columns
import financedatabase as fd

# Initialize the Equities database
indices = fd.Indices()
print(indices)

# Step 1: Fetch the list of exchanges for global_indices
def fetch_indices_exchanges():
    """
    Fetch the list of exchanges available for indices using the financedatabase library.

    Returns:
        list: A list of unique exchanges.
    """
    try:
        # Initialize the indices object from financedatabase
        indices = fd.Indices()

        # Get all indices data
        indices_data = indices.select()

        # Extract unique exchanges
        exchanges = indices_data['exchange'].dropna().unique().tolist()

        console.print("[bold green]Successfully fetched indices exchanges.[/bold green]")
        return exchanges
    except Exception as e:
        console.print(f"[bold red]Failed to fetch exchanges: {e}[/bold red]")
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
    """
    Fetch indices data for a specific exchange using the financedatabase library.

    Parameters:
        exchange (str): The name of the exchange to filter indices.

    Returns:
        list: A list of dictionaries containing indices for the specified exchange.
              Each dictionary contains the 'symbol' and 'name' keys.
    """
    try:
        # Initialize the indices object from financedatabase
        indices = fd.Indices()

        # Select all indices data
        indices_data = indices.select()

        # Filter indices by the specified exchange
        filtered_indices = indices_data[indices_data['exchange'] == exchange]

        if filtered_indices.empty:
            console.print(f"[bold yellow]No indices found for exchange: {exchange}[/bold yellow]")
            return []

        # Convert DataFrame to a list of dictionaries
        indices_list = [
            {"symbol": symbol, "name": row.get("name", "Unknown")}
            for symbol, row in filtered_indices.iterrows()
        ]

        console.print(f"[bold green]Successfully fetched indices for exchange: {exchange}[/bold green]")
        return indices_list
    except Exception as e:
        console.print(f"[bold red]Failed to fetch indices for exchange {exchange}: {e}[/bold red]")
        return []

# Step 4: Paginate the list of indices (21 per page)
def display_indices_paginated(indices, start_index=0, page_size=21):
    """
    Paginate and display a list of indices for user selection.

    Args:
        indices (list): List of indices (dictionaries with at least a 'name' key or string names).
        start_index (int): Starting index of the current page.
        page_size (int): Number of indices to display per page.

    Returns:
        dict or str: Selected index from the list, or None if the user exits.
    """
    # Ensure indices are in the correct format
    if not isinstance(indices, list) or not all(isinstance(index, (dict, str)) for index in indices):
        console.print("[bold red]Invalid indices data format. Expected a list of dictionaries or strings.[/bold red]")
        return None

    # Normalize indices to a list of dictionaries with a 'name' key
    normalized_indices = [
        {"name": index} if isinstance(index, str) else index for index in indices
    ]

    # Paginate the data
    end_index = min(start_index + page_size, len(normalized_indices))
    indices_page = normalized_indices[start_index:end_index]

    # Display the current page
    index_names = [f"{index['name']}" for i, index in enumerate(indices_page)]
    display_in_columns(f"Indices (Showing {start_index + 1} - {end_index} of {len(normalized_indices)})", index_names)

    # Prompt the user for navigation or selection
    from rich.prompt import Prompt
    action = Prompt.ask(
        "\n[bold cyan]Enter: Next (N) / Previous (P) / Select (1-{0}) / Exit (E)[/bold cyan]".format(len(indices_page))
    )

    # Handle actions
    if action.lower() == 'n':
        if end_index < len(normalized_indices):
            return display_indices_paginated(normalized_indices, start_index + page_size)
        else:
            console.print("[bold red]No more pages available.[/bold red]")
            return display_indices_paginated(normalized_indices, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_indices_paginated(normalized_indices, start_index - page_size)
        else:
            console.print("[bold red]Already on the first page.[/bold red]")
            return display_indices_paginated(normalized_indices, start_index)
    elif action.lower() == 'e':
        console.print("[bold yellow]Exiting selection...[/bold yellow]")
        return None
    else:
        try:
            # Parse user selection
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(normalized_indices):
                return normalized_indices[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_indices_paginated(normalized_indices, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number, 'N', 'P', or 'E'.[/bold red]")
            return display_indices_paginated(normalized_indices, start_index)




# Step 5: Fetch index data by symbol using yfinance and historical data

def fetch_index_data_by_symbol_with_history(symbol):
    """
    Fetch general information and historical data for a given symbol.
    Returns:
        - filtered_info: Dictionary of filtered general information
        - history_table: DataFrame of historical data
    """
    try:
        ticker = yf.Ticker(symbol)

        # Fetch general info
        info = ticker.info

        # Filter out unnecessary fields
        filtered_info = {k: v for k, v in info.items() if
                         k not in ['maxAge', 'uuid', 'messageBoardId', 'gmtOffSetMilliseconds']}

        # Fetch last 7 days of historical data
        history_data = ticker.history(period='7d')

        # Validate if historical data is available
        if history_data.empty:
            raise ValueError(f"No historical data found for the symbol '{symbol}'. Please enter a valid symbol.")

        # Extract important columns from historical data
        history_table = history_data[['Open', 'High', 'Low', 'Close', 'Volume']].tail(7)

        return filtered_info, history_table

    except ValueError as ve:
        return str(ve), None
    except Exception as e:
        return f"An unexpected error occurred: {str(e)}", None


import pandas as pd
import yfinance as yf
from fincept_terminal.utils.themes import console


def fetch_index_data_by_symbol_with_history_1(symbol):
    """
    Fetch general info and last 7 days of historical data for the given symbol.

    Parameters:
        symbol (str): The symbol of the index or financial instrument.

    Returns:
        tuple: General info (dict) and historical data (DataFrame).
    """
    try:
        ticker = yf.Ticker(symbol)

        # Fetch general info
        info = ticker.info
        filtered_info = {k: v for k, v in info.items() if
                         k not in ['maxAge', 'uuid', 'messageBoardId', 'gmtOffSetMilliseconds']}

        # Fetch last 7 days of historical data
        history_data = ticker.history(period='7d')

        if history_data.empty:
            console.print(
                f"[bold yellow]No data for the last 7 days for symbol: {symbol}. Attempting to fetch 1-month data.[/bold yellow]")
            history_data = ticker.history(period='1mo')

        if history_data.empty:
            console.print(f"[bold red]No historical data found for the symbol {symbol}.[/bold red]")
            return filtered_info, pd.DataFrame(columns=['Date', 'Open', 'High', 'Low', 'Close', 'Volume'])

        # Extract and limit to required columns
        required_columns = ['Open', 'High', 'Low', 'Close', 'Volume']
        available_columns = [col for col in required_columns if col in history_data.columns]
        history_table = history_data[available_columns].tail(7)

        return filtered_info, history_table

    except Exception as e:
        console.print(f"[bold red]Error fetching data for {symbol}: {e}[/bold red]")
        return {}, pd.DataFrame(columns=['Date', 'Open', 'High', 'Low', 'Close', 'Volume'])


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

    # Check if `info` is a dictionary
    if not isinstance(info, dict):
        console.print(f"[bold red]Error: {info}[/bold red]")
        return

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


# Step 6: Main function to show the global_data Indices menu
def show_global_indices_menu():
    """Display the Global Indices menu and handle user interactions."""
    console.print("[bold cyan]GLOBAL INDICES MENU[/bold cyan]\n")

    from rich.prompt import Prompt

    # Display the main menu options
    options = ["GLOBAL INDICES", "SEARCH INDICES SYMBOL", "BACK TO MAIN MENU"]
    display_in_columns("Select an Option", options)

    choice = Prompt.ask("Enter the number corresponding to your choice")

    if choice == "1":  # GLOBAL INDICES
        # Fetch exchanges using FinanceDatabase
        exchanges = fetch_indices_exchanges()
        exchanges.append("BACK TO MAIN MENU")

        if not exchanges:
            console.print("[bold red]No exchanges available.[/bold red]")
            return

        # Display the exchanges for user selection
        selected_exchange = display_indices_exchanges_full(exchanges)

        if selected_exchange == "BACK TO MAIN MENU":
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            return

        if selected_exchange:
            # Fetch indices for the selected exchange using FinanceDatabase
            indices = fetch_indices_by_exchange(selected_exchange)
            if not indices:  # Use .empty to check if the DataFrame is empty
                console.print(f"[bold red]No indices available for the exchange: {selected_exchange}[/bold red]")
                return

            # Display the indices and let the user select one
            selected_index = display_indices_paginated(indices)

            if selected_index:
                # Display selected index details and last 7 days of price history
                display_index_info(selected_index)
                index_data, history_table = fetch_index_data_by_symbol_with_history_1(selected_index['symbol'])

                if isinstance(index_data, dict) and "error" in index_data:
                    console.print(f"[bold red]{index_data['error']}[/bold red]")
                else:
                    display_info_with_history(index_data, history_table)

    elif choice == "2":  # SEARCH INDICES SYMBOL
        symbol = Prompt.ask("Enter the index symbol")
        index_data, history_table = fetch_index_data_by_symbol_with_history_1(symbol)

        if history_table is not None:
            display_info_with_history(index_data, history_table)
        else:
            console.print(f"[bold red]{index_data}[/bold red]")

    elif choice == "3":  # BACK TO MAIN MENU
        console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]")
        from fincept_terminal.oldTerminal.cli import show_main_menu
        show_main_menu()
        return

    else:
        console.print("\n[bold red]INVALID OPTION. PLEASE TRY AGAIN.[/bold red]")

    # Ask if the user wants to query another index or return to the main menu
    continue_query = Prompt.ask("Do you want to query another index? (yes/no)", default="no")
    if continue_query.lower() == "yes":
        show_global_indices_menu()
    else:
        console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]")
        from fincept_terminal.oldTerminal.cli import show_main_menu
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

