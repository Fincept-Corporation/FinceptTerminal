import requests
import yfinance as yf
from fincept_terminal.utils.themes import console


# Step 1: Fetch and display the list of base currencies
def fetch_base_currencies():
    url = "https://fincept.share.zrok.io/FinanceDB/currencies/base_currency/data?unique=true"
    response = requests.get(url)
    if response.status_code == 200:
        data = response.json()
        return data['base_currency']
    else:
        console.print(f"[bold red]Failed to fetch base currencies: {response.status_code}[/bold red]")
        return []


# Step 2: Fetch currency pairs based on the base currency selected by the user
def fetch_currency_pairs(base_currency):
    url = f"https://fincept.share.zrok.io/FinanceDB/currencies/base_currency/filter?value={base_currency}"
    response = requests.get(url)
    if response.status_code == 200:
        return response.json()
    else:
        console.print(f"[bold red]Failed to fetch currency pairs: {response.status_code}[/bold red]")
        return []


# Step 3: Use yfinance to fetch details about a selected pair
def fetch_currency_pair_details(symbol):
    ticker = yf.Ticker(symbol)
    info = ticker.info
    # Filter out unnecessary fields
    relevant_info = {k: v for k, v in info.items() if k not in [
        'messageBoardId', 'gmtOffSetMilliseconds', 'firstTradeDateEpochUtc', 'maxAge']}
    return relevant_info


# Step 4: Main function to display the menu and interact with the user
def show_currency_market_menu():
    console.print("[bold cyan]CURRENCY MARKET MENU[/bold cyan]\n", style="info")

    while True:
        # Step 1: Fetch and display base currencies
        base_currencies = fetch_base_currencies()
        if not base_currencies:
            return

        from fincept_terminal.utils.const import display_in_columns
        console.print("[bold green]Available Base Currencies:[/bold green]\n", style="info")
        display_in_columns("Select a Base Currency", base_currencies)

        # User selects a base currency
        from rich.prompt import Prompt
        choice = Prompt.ask("Enter the number corresponding to the base currency")
        selected_base_currency = base_currencies[int(choice) - 1]

        # Step 2: Fetch and display currency pairs
        currency_pairs = fetch_currency_pairs(selected_base_currency)
        if not currency_pairs:
            return

        currency_pair_names = [pair['name'] for pair in currency_pairs]
        display_in_columns(f"Currency Pairs for {selected_base_currency}", currency_pair_names)

        # User selects a currency pair
        choice = Prompt.ask("Enter the number corresponding to the currency pair")
        selected_currency_pair = currency_pairs[int(choice) - 1]['symbol']

        # Step 3: Fetch and display details for the selected currency pair
        currency_details = fetch_currency_pair_details(selected_currency_pair)
        if currency_details:
            from fincept_terminal.utils.const import display_info_in_three_columns
            display_info_in_three_columns(currency_details)

        # Ask if the user wants to query another pair or exit
        continue_query = Prompt.ask("Do you want to query another currency pair? (yes/no)")
        if continue_query.lower() == 'no':
            console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            break  # Exit the loop and redirect to the main menu

# Step 5: Helper function to display currency details
def display_currency_details(details):
    from rich.table import Table
    table = Table(title="Currency Pair Details", header_style="bold green on #282828", show_lines=True)
    table.add_column("Attribute", style="highlight", justify="left")
    table.add_column("Value", style="highlight", justify="left")

    for key, value in details.items():
        table.add_row(str(key), str(value))

    console.print(table)

