import requests
import yfinance as yf
from fincept_terminal.themes import console
from fincept_terminal.const import display_in_columns,display_info_in_three_columns


# Step 1: Fetch the list of exchanges for money markets
def fetch_money_market_exchanges():
    url = "https://fincept.share.zrok.io/FinanceDB/moneymarkets/exchange/data?unique=true"
    response = requests.get(url)
    if response.status_code == 200:
        data = response.json()
        # Filter out null values
        return [exchange for exchange in data['exchange'] if exchange]
    else:
        console.print(f"[bold red]Failed to fetch exchanges: {response.status_code}[/bold red]")
        return []


# Step 2: Display the full list of exchanges (no pagination)
def display_money_market_exchanges_full(exchanges):
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
            return display_money_market_exchanges_full(exchanges)
    except ValueError:
        console.print("[bold red]Invalid input. Please enter a number.[/bold red]")
        return display_money_market_exchanges_full(exchanges)


# Step 3: Fetch symbols by exchange for money markets
def fetch_money_market_symbols_by_exchange(exchange):
    url = f"https://fincept.share.zrok.io/FinanceDB/moneymarkets/exchange/filter?value={exchange}"
    response = requests.get(url)
    if response.status_code == 200:
        return response.json()
    else:
        console.print(f"[bold red]Failed to fetch symbols for {exchange}: {response.status_code}[/bold red]")
        return []


# Step 4: Paginate the list of symbols (21 per page)
def display_money_market_symbols_paginated(symbols, start_index=0, page_size=21):
    end_index = min(start_index + page_size, len(symbols))
    symbols_page = symbols[start_index:end_index]

    # Extract only the names of the symbols to display
    symbol_names = [symbol['name'] for symbol in symbols_page]

    # Display the current page of symbols
    display_in_columns(f"Money Market Symbols (Showing {start_index + 1} - {end_index})", symbol_names)

    # Ask the user to navigate or select a symbol
    from rich.prompt import Prompt
    action = Prompt.ask("\n[bold cyan]Next Page (N) / Previous Page (P) / Select a Symbol (1-{0})[/bold cyan]".format(
        len(symbol_names)))

    if action.lower() == 'n':
        if end_index < len(symbols):
            return display_money_market_symbols_paginated(symbols, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_money_market_symbols_paginated(symbols, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_money_market_symbols_paginated(symbols, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_money_market_symbols_paginated(symbols, start_index)
    else:
        try:
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(symbols):
                return symbols[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_money_market_symbols_paginated(symbols, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number or 'N'/'P'.[/bold red]")
            return display_money_market_symbols_paginated(symbols, start_index)


# Step 5: Fetch money market data by symbol using yfinance
def fetch_money_market_data_by_symbol(symbol):
    ticker = yf.Ticker(symbol)
    info = ticker.info
    # Filter out unnecessary fields
    filtered_info = {k: v for k, v in info.items() if k not in ['maxAge', 'messageBoardId', 'gmtOffSetMilliseconds']}
    return filtered_info


# Step 6: Main function to show the Money Market menu
def show_money_market_menu():
    console.print("[bold cyan]MONEY MARKET MENU[/bold cyan]\n", style="info")

    from rich.prompt import Prompt

    # Display two options: Global Market or Search by Symbol
    options = ["Global Market", "Search Market Symbol"]
    display_in_columns("Select an Option", options)

    choice = Prompt.ask("Enter the number corresponding to your choice")

    if choice == "1":  # Global Market
        exchanges = fetch_money_market_exchanges()
        if not exchanges:
            return
        selected_exchange = display_money_market_exchanges_full(exchanges)
        if selected_exchange:
            symbols = fetch_money_market_symbols_by_exchange(selected_exchange)
            if not symbols:
                return

            # Display the symbols with pagination (21 per page)
            selected_symbol = display_money_market_symbols_paginated(symbols)

            # Display the selected symbol details and fetch data from yfinance
            if selected_symbol:
                display_symbol_info(selected_symbol)
                market_data = fetch_money_market_data_by_symbol(selected_symbol['symbol'])
                display_info_in_three_columns(market_data)

    elif choice == "2":  # Search Market Symbol
        symbol = Prompt.ask("Enter the market symbol")
        market_data = fetch_money_market_data_by_symbol(symbol)
        display_info_in_three_columns(market_data)

    # Ask if the user wants to query another data or exit to the main menu
    continue_query = Prompt.ask("Do you want to query another market data? (yes/no)")
    if continue_query.lower() == 'yes':
        show_money_market_menu()  # Redirect back to the Money Market menu
    else:
        console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
        from fincept_terminal.cli import show_main_menu
        show_main_menu()


# Step 7: Display additional symbol info
def display_symbol_info(symbol):
    details = [
        f"Name: {symbol.get('name', 'N/A')}",
        f"Currency: {symbol.get('currency', 'N/A')}",
        f"Market: {symbol.get('market', 'N/A')}",
        f"Exchange: {symbol.get('exchange', 'N/A')}"
    ]
    display_in_columns("Symbol Details", details)

