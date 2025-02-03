import yfinance as yf
from rich.prompt import Prompt
from fincept_terminal.FinceptTerminalUtils.themes import console
import financedatabase as fd

# Step 1: Fetch the list of exchanges for money markets
def fetch_money_market_exchanges():
    """
    Fetch the list of unique money market exchanges using financedatabase.
    """
    try:
        moneymarkets = fd.Moneymarkets()
        data = moneymarkets.select()
        exchanges = data['exchange'].dropna().unique().tolist()  # Extract unique exchanges
        return sorted(exchanges)
    except Exception as e:
        console.print(f"[bold red]Failed to fetch exchanges: {e}[/bold red]")
        return []

# Step 2: Display the exchanges with pagination
def display_money_market_exchanges_paginated(exchanges, title="Select an Exchange", start_index=0, page_size=21):
    """
    Paginate and display a list of exchanges for user selection.

    Args:
        exchanges (list): List of exchanges.
        title (str): Title for the displayed list.
        start_index (int): Starting index for pagination.
        page_size (int): Number of items per page.

    Returns:
        str: Selected exchange or None if the user exits.
    """
    from fincept_terminal.FinceptTerminalUtils.const import display_in_columns
    while True:
        end_index = min(start_index + page_size, len(exchanges))
        current_page = exchanges[start_index:end_index]

        display_in_columns(f"Select an Exchange (Showing {start_index + 1} - {end_index})", current_page)

        # Prompt for navigation or selection
        action = Prompt.ask(
            "\n[bold cyan]Next Page (N) / Previous Page (P) / Select (1-{0}) / Exit (E)[/bold cyan]".format(len(current_page))
        )

        if action.lower() == 'n':
            if end_index < len(exchanges):
                start_index += page_size
            else:
                console.print("[bold red]No more pages available.[/bold red]")
        elif action.lower() == 'p':
            if start_index > 0:
                start_index -= page_size
            else:
                console.print("[bold red]Already on the first page.[/bold red]")
        elif action.lower() == 'e':
            console.print("[bold yellow]Exiting selection...[/bold yellow]")
            return None
        else:
            try:
                selected_index = int(action) - 1
                if 0 <= selected_index < len(exchanges):
                    return exchanges[selected_index]
                else:
                    console.print("[bold red]Invalid selection. Please select a valid number.[/bold red]")
            except ValueError:
                console.print("[bold red]Invalid input. Please enter a number, 'N', 'P', or 'E'.[/bold red]")

# Step 3: Fetch symbols by exchange for money markets
def fetch_money_market_symbols_by_exchange(exchange):
    """
    Fetch symbols for a specific exchange using financedatabase.
    """
    try:
        moneymarkets = fd.Moneymarkets()
        data = moneymarkets.select()
        symbols = data[data['exchange'] == exchange].reset_index()
        if symbols.empty:
            console.print(f"[bold yellow]No symbols found for exchange: {exchange}[/bold yellow]")
            return []
        return symbols.to_dict('records')
    except Exception as e:
        console.print(f"[bold red]Failed to fetch symbols for {exchange}: {e}[/bold red]")
        return []

# Step 4: Display symbols with pagination
def display_money_market_symbols_paginated(symbols, title="Select a Symbol", start_index=0, page_size=21):
    """
    Paginate and display a list of money market symbols using the display_in_columns function.

    Args:
        symbols (list): List of money market symbols (dictionaries with 'symbol' and 'name' keys).
        title (str): Title for the displayed list.
        start_index (int): Starting index for pagination.
        page_size (int): Number of items per page.

    Returns:
        dict: The selected symbol dictionary or None if the user exits.
    """
    from fincept_terminal.FinceptTerminalUtils.const import display_in_columns

    while True:
        # Calculate the range of items to display
        end_index = min(start_index + page_size, len(symbols))
        current_page = symbols[start_index:end_index]

        # Format the symbols for display
        formatted_symbols = [
            f"{item['symbol']} - {item['name']}"
            for i, item in enumerate(current_page, start=start_index)
        ]

        # Display the current page using display_in_columns
        display_in_columns(f"{title} (Showing {start_index + 1}-{end_index} of {len(symbols)})", formatted_symbols)

        # Prompt the user for navigation or selection
        action = Prompt.ask(
            "\n[bold cyan]Next Page (N) / Previous Page (P) / Select (1-{0}) / Exit (E)[/bold cyan]".format(len(current_page))
        )

        if action.lower() == 'n':
            # Move to the next page
            if end_index < len(symbols):
                start_index += page_size
            else:
                console.print("[bold red]No more pages available.[/bold red]")
        elif action.lower() == 'p':
            # Move to the previous page
            if start_index > 0:
                start_index -= page_size
            else:
                console.print("[bold red]Already on the first page.[/bold red]")
        elif action.lower() == 'e':
            # Exit the selection
            console.print("[bold yellow]Exiting selection...[/bold yellow]")
            return None
        else:
            # Attempt to select an item
            try:
                selected_index = int(action) - 1
                if start_index <= selected_index < end_index:
                    return symbols[selected_index]
                else:
                    console.print("[bold red]Invalid selection. Please select a valid number.[/bold red]")
            except ValueError:
                console.print("[bold red]Invalid input. Please enter a number, 'N', 'P', or 'E'.[/bold red]")


# Step 5: Fetch money market data by symbol using yfinance
def fetch_money_market_data_by_symbol(symbol):
    """
    Fetch detailed data for a money market symbol using yfinance.

    Args:
        symbol (str): The symbol of the money market to fetch data for.

    Returns:
        dict: Filtered information about the money market symbol.
    """
    # Validate the symbol
    if not symbol or not isinstance(symbol, str):
        console.print("[bold red]Invalid symbol provided. Please select a valid money market symbol.[/bold red]")
        return {}

    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info
        # Filter out unnecessary fields
        filtered_info = {
            k: v for k, v in info.items() if k not in ['maxAge', 'messageBoardId', 'gmtOffSetMilliseconds']
        }
        return filtered_info
    except Exception as e:
        console.print(f"[bold red]Failed to fetch data for symbol '{symbol}': {e}[/bold red]")
        return {}

# Step 6: Main function to show the Money Market menu
def show_money_market_menu():
    """
    Show the Money Market menu and handle user interactions.
    """
    from fincept_terminal.FinceptTerminalUtils.const import display_in_columns, display_info_in_three_columns
    console.print("[bold cyan]MONEY MARKET MENU[/bold cyan]\n")

    while True:  # Loop to keep showing the menu until the user selects "Back to Main Menu"
        # Define menu options
        options = ["Global Money Market", "Search Market Symbol", "Back to Main Menu"]

        # Display options using display_in_columns
        display_in_columns("Money Market Menu", [f"{option}" for i, option in enumerate(options)])

        from rich.prompt import Prompt
        choice = Prompt.ask("\nEnter the number corresponding to your choice")

        # Validate user input and handle each option
        if choice == "1":  # Global Money Market
            exchanges = fetch_money_market_exchanges()
            if not exchanges:
                console.print("[bold yellow]No exchanges available.[/bold yellow]")
                continue
            selected_exchange = display_money_market_exchanges_paginated(exchanges)
            if selected_exchange:
                symbols = fetch_money_market_symbols_by_exchange(selected_exchange)
                if not symbols:
                    console.print("[bold yellow]No symbols found for the selected exchange.[/bold yellow]")
                    continue
                selected_symbol = display_money_market_symbols_paginated(symbols)
                if selected_symbol:
                    display_symbol_info(selected_symbol)
                    market_data = fetch_money_market_data_by_symbol(selected_symbol['symbol'])
                    display_info_in_three_columns(market_data)

        elif choice == "2":  # Search Market Symbol
            symbol = Prompt.ask("Enter the market symbol")
            market_data = fetch_money_market_data_by_symbol(symbol)
            if market_data:
                display_info_in_three_columns(market_data)
            else:
                console.print("[bold red]No data found for the provided symbol.[/bold red]")

        elif choice == "3":  # Back to Main Menu
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            break  # Exit the money market menu loop

        else:
            console.print("[bold red]Invalid selection. Please try again.[/bold red]")



def display_symbol_info(symbol):
    from fincept_terminal.FinceptTerminalUtils.const import display_in_columns
    details = [
        f"Name: {symbol.get('name', 'N/A')}",
        f"Currency: {symbol.get('currency', 'N/A')}",
        f"Market: {symbol.get('market', 'N/A')}",
        f"Exchange: {symbol.get('exchange', 'N/A')}"
    ]
    display_in_columns("Symbol Details", details)