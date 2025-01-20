import requests
from rich.prompt import Prompt
from fincept_terminal.utils.themes import console
from fincept_terminal.utils.const import display_in_columns , display_info_in_three_columns

def show_bond_market_menu():
    """Bond Market Menu"""
    while True:
        console.print("[highlight]BOND MARKET[/highlight]\n", style="info")

        menu_options = [
            "global_data Bonds",
            "Back to Main Menu"
        ]

        display_in_columns("BOND MARKET MENU", menu_options)

        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            show_global_bonds()
        elif choice == "2":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")


def fetch_with_retry(url, retries=3, delay=2):
    """
    Fetch data from the given URL with retry logic.

    Parameters:
    - url: The URL to fetch data from.
    - retries: Number of retries before giving up.
    - delay: Delay between retries in seconds.

    Returns:
    - JSON response if successful, None otherwise.
    """
    for attempt in range(retries):
        try:
            response = requests.get(url)
            response.raise_for_status()  # Raise an error for bad responses
            return response.json()  # Return JSON data
        except requests.exceptions.RequestException as e:
            if attempt < retries - 1:
                console.print(
                    f"\n[bold yellow]Attempt {attempt + 1} failed: {e}. Retrying in {delay} seconds...[/bold yellow]")
                import time
                time.sleep(delay)  # Wait before retrying
            else:
                console.print(f"\n[bold red]Error fetching data: {e}. All retry attempts failed.[/bold red]")
                return None  # Return None if all retries fail


def show_global_bonds():
    """Display bond regions globally with retry logic."""
    url = "https://fincept.share.zrok.io/WorldBondData/countries_with_bonds/region/data?unique=true"
    regions_data = fetch_with_retry(url)

    if regions_data is None:
        return  # Exit if data couldn't be fetched

    regions = regions_data.get('region', [])
    regions.append("Back to Bond Market Menu")
    display_in_columns("Select a Region", regions)

    region_choice = Prompt.ask("Enter your choice")

    if int(region_choice) == len(regions):  # Back to Bond Market Menu
        return

    selected_region = regions[int(region_choice) - 1]
    show_countries_in_region(selected_region)

def show_countries_in_region(region):
    """Display countries in the selected region and handle India-specific ISIN lookup."""
    try:
        # Fetch countries by region
        region_url = f"https://fincept.share.zrok.io/WorldBondData/countries_with_bonds/region/filter?value={region}"
        response = requests.get(region_url)
        response.raise_for_status()
        countries = [item['country_name'] for item in response.json()]

        countries.append("Back to global_data Bonds Menu")
        display_in_columns(f"Countries in {region}", countries)

        country_choice = Prompt.ask("Enter your choice")

        if int(country_choice) == len(countries):  # Back to global_data Bonds Menu
            return

        selected_country = countries[int(country_choice) - 1]

        if selected_country == "India":
            handle_india_bond_isin()
        else:
            console.print(f"\n[bold cyan]Currently, we only have bond market data for India.[/bold cyan]\n")
            return

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching countries: {e}[/bold red]", style="danger")


def handle_india_bond_isin():
    """Handle ISIN input for India and fetch relevant bond data tables with retry logic."""
    while True:
        isin = Prompt.ask("Enter the Indian bond ISIN number (e.g., INE002A08690) or type 'back' to return")

        if isin.lower() == "back":
            return  # Return to the previous menu

        try:
            # Fetch bond data tables containing the ISIN with retry logic
            url = "https://fincept.share.zrok.io/IndiaBondData/tables"
            tables_data = fetch_with_retry(url)

            if tables_data is None:
                return  # Exit if data couldn't be fetched

            # Filter tables based on the provided ISIN
            tables = [table for table in tables_data if isin in table]

            if tables:
                while True:
                    display_all_tables(tables, isin)

                    # Prompt user to select a specific table or view all tables
                    table_choice = Prompt.ask(
                        "Enter the number of the table to view, 'all' to view all tables, or 'back' to return")
                    if table_choice.lower() == "back":
                        return
                    elif table_choice.lower() == "all":
                        for table in tables:
                            show_table_data(table)
                    else:
                        try:
                            table_index = int(table_choice) - 1
                            if 0 <= table_index < len(tables):
                                show_table_data(tables[table_index])
                            else:
                                console.print("\n[bold red]Invalid selection.[/bold red]\n")
                        except ValueError:
                            console.print(
                                "\n[bold red]Invalid input. Please enter a valid number or 'all'.[/bold red]\n")

                    another_query = Prompt.ask("Do you want to query another table for the same ISIN? (yes/no)")
                    if another_query.lower() != 'yes':
                        break
            else:
                console.print(f"\n[bold red]No data found for ISIN: {isin}[/bold red]\n")
        except requests.exceptions.RequestException as e:
            console.print(f"[bold red]Error fetching bond data tables: {e}[/bold red]", style="danger")


def display_all_tables(tables, isin):
    """Display all the fetched tables related to the provided ISIN."""
    from rich.table import Table
    table_display = Table(title=f"Tables for ISIN {isin}", header_style="bold green on #282828")

    # Display table numbers with the names
    table_display.add_column("Number", justify="center", style="cyan")
    table_display.add_column("Table Name", justify="left", style="cyan")

    for i, table in enumerate(tables, start=1):
        table_display.add_row(str(i), table)

    console.print(table_display)


def show_table_data(table_name):
    """Fetch and display data from the specified table."""
    url = f"https://fincept.share.zrok.io/IndiaBondData/{table_name}/data"
    data = fetch_with_retry(url)

    if data is None:
        return

    if isinstance(data, list) and data:
        if len(data) > 1 or len(data[0]) > 10:  # If there's a lot of data, use the three-column display
            display_info_in_three_columns(data[0])
        else:
            from rich.table import Table
            table = Table(title=f"Data from {table_name}", header_style="bold green on #282828", show_lines=True)
            # Dynamically add columns based on the data keys
            for key in data[0].keys():
                table.add_column(key, style="cyan", justify="left")

            # Add rows
            for row in data:
                table.add_row(*[str(value) for value in row.values()])

            console.print(table)
    else:
        console.print(f"\n[bold red]No data available for table {table_name}.[/bold red]\n")
