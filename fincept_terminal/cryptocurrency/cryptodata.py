import financedatabase as fd
from rich.prompt import Prompt
import yfinance as yf
from fincept_terminal.FinceptTerminalUtils.themes import console

# Fetch available currencies using financedatabase
def fetch_available_currencies():
    """
    Fetch the list of available currencies from financedatabase.
    """
    try:
        cryptos = fd.Cryptos()
        # Select the required data, including the symbol
        crypto_data = cryptos.select()
        if crypto_data.empty:
            console.print("[bold red]No cryptocurrency data available.[/bold red]")
            return []
        crypto_data = crypto_data.reset_index()
        # Extract unique currencies and ensure valid values
        currencies = crypto_data['currency'].dropna().unique()
        return sorted(currencies)  # Return sorted unique currencies
    except Exception as e:
        console.print(f"[bold red]Failed to fetch currencies: {e}[/bold red]")
        return []


# Fetch cryptocurrencies by currency using financedatabase

def fetch_cryptos_by_currency(currency):
    """
    Fetch cryptocurrencies for a specific base currency from financedatabase.
    """
    try:
        cryptos = fd.Cryptos()
        # Filter by the selected currency
        crypto_data = cryptos.select(currency=currency)
        if crypto_data.empty:
            console.print(f"[bold red]No cryptocurrencies found for currency: {currency}[/bold red]")
            return []

        # Ensure the 'symbol' field is included
        crypto_data = crypto_data.reset_index()  # Reset index to include 'symbol'
        cryptos_list = crypto_data.to_dict('records')

        # Ensure 'symbol' exists for each entry
        for crypto in cryptos_list:
            crypto['symbol'] = crypto.get('symbol', 'Unknown')  # Fallback to 'Unknown' if missing

        return cryptos_list
    except Exception as e:
        console.print(f"[bold red]Failed to fetch cryptocurrencies: {e}[/bold red]")
        return []


# Paginate and display a list of items
def display_cryptos_paginated(cryptos, title="Select a Cryptocurrency", start_index=0, page_size=21):
    """
    Paginate and display a list of cryptocurrencies for user selection.

    Args:
        cryptos (list): List of cryptocurrencies (dictionaries with 'symbol' and 'name').
        title (str): Title for the displayed list.
        start_index (int): Starting index for pagination.
        page_size (int): Number of items to display per page.

    Returns:
        dict: The selected cryptocurrency or None if the user exits.
    """
    from fincept_terminal.FinceptTerminalUtils.const import display_in_columns

    while True:
        end_index = min(start_index + page_size, len(cryptos))
        current_page = cryptos[start_index:end_index]

        # Display the current page
        display_in_columns(f"Select a Base currency (Showing {start_index + 1} - {end_index})", current_page)

        # Prompt the user for navigation or selection
        action = Prompt.ask(
            "\n[bold cyan]Next Page (N) / Previous Page (P) / Select (1-{0}) / Exit (E)[/bold cyan]".format(len(current_page))
        )

        if action.lower() == 'n':
            if end_index < len(cryptos):
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
                if 0 <= selected_index < len(cryptos):
                    return cryptos[selected_index]
                else:
                    console.print("[bold red]Invalid selection. Please select a valid number.[/bold red]")
            except ValueError:
                console.print("[bold red]Invalid input. Please enter a number, 'N', 'P', or 'E'.[/bold red]")



# Main Crypto Menu
def crypto_main_menu():
    """
    Main menu for Crypto operations, displaying options using the display_in_columns method.
    """
    from fincept_terminal.FinceptTerminalUtils.const import display_in_columns
    while True:
        console.print("\n[bold cyan]CRYPTO MENU[/bold cyan]\n")

        # Menu items to display
        menu_items = [
            "Search Cryptocurrency by Symbol (Using yfinance)",
            "View All Cryptocurrencies (Select Currency First)",
            "Exit"
        ]

        # Display the menu using display_in_columns
        display_in_columns("CRYPTO MENU", menu_items)

        # User selects an option
        from rich.prompt import Prompt
        choice = Prompt.ask("Enter your choice", choices=[str(i + 1) for i in range(len(menu_items))])

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


# View All Cryptocurrencies by Currency

def view_all_cryptocurrencies():
    """
    Display all cryptocurrencies after selecting a base currency.
    """
    currencies = fetch_available_currencies()
    if not currencies:
        return

    console.print(f"\n[bold cyan]Total Currencies Available: {len(currencies)}[/bold cyan]")
    # currencies_name = [{"name": currency}for currency in currencies]
    selected_currency = display_cryptos_paginated(currencies, title="Available Currencies")

    if selected_currency:
        console.print(f"\n[bold green]Fetching cryptocurrencies for {selected_currency}...[/bold green]")
        cryptos = fetch_cryptos_by_currency(selected_currency)
        if not cryptos:
            console.print(f"[bold red]No cryptocurrencies found for {selected_currency}.[/bold red]")
            return
        crypto_symbol = [crypto["symbol"] for crypto in cryptos]

        selected_crypto = display_cryptos_paginated(crypto_symbol, title=f"Cryptocurrencies for {crypto_symbol}")
        if selected_crypto:
            fetch_and_display_yfinance_data(selected_crypto)



# Fetch and display cryptocurrency details using yfinance
def fetch_and_display_yfinance_data(symbol):
    """
    Fetch and display cryptocurrency data using yfinance.
    """
    from fincept_terminal.FinceptTerminalUtils.const import display_info_in_three_columns
    try:
        ticker = yf.Ticker(symbol)
        data = ticker.info

        excluded_keys = {'uuid', 'messageBoardId', 'maxAge'}
        description = data.pop("description", None)
        summary = data.pop("summary", None)

        filtered_data = {k: v for k, v in data.items() if k not in excluded_keys}
        display_info_in_three_columns(filtered_data)

        if description:
            console.print(f"\n[bold cyan]Description[/bold cyan]: {description}")
        if summary:
            console.print(f"\n[bold cyan]Summary[/bold cyan]: {summary}")
    except Exception as e:
        console.print(f"[bold red]Error fetching data for {symbol}: {e}[/bold red]")
