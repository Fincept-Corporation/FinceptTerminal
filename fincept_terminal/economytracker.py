import requests
from rich.prompt import Prompt
from rich.panel import Panel
from fincept_terminal.themes import console
from fincept_terminal.const import display_in_columns


def show_world_economy_tracker_menu():
    """World Economy Tracker menu."""
    while True:
        console.print("[highlight]WORLD ECONOMY TRACKER[/highlight]\n", style="info")

        menu_options = [
            "List of Countries",
            "Search by Index",
            "Back to Main Menu"
        ]

        display_in_columns("WORLD ECONOMY TRACKER MENU", menu_options)

        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            show_list_of_countries()
        elif choice == "2":
            search_by_index()
        elif choice == "3":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
            break
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")


def show_list_of_countries():
    """Display the list of countries."""
    try:
        url = "https://fincept.share.zrok.io/LargeEconomicDatabase/countries/data"
        response = requests.get(url)
        response.raise_for_status()
        countries_data = response.json()

        countries = [item['country_name'] for item in countries_data]
        paginate_display_in_columns("List of Countries", countries)

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching countries data: {e}[/bold red]", style="danger")

def show_list_of_countries():
    """Display the list of countries."""
    try:
        url = "https://fincept.share.zrok.io/LargeEconomicDatabase/countries/data"
        response = requests.get(url)
        response.raise_for_status()
        countries_data = response.json()

        countries = [item['country_name'] for item in countries_data]
        paginate_display_in_columns("List of Countries", countries)

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching countries data: {e}[/bold red]", style="danger")


def paginate_display_in_columns(title, items, items_per_page=None):
    """
    Paginate and display items in a table with multiple columns.

    Parameters:
    - title: The title of the table.
    - items: A list of items to display.
    - items_per_page: Number of items to display per page. If None, display all on one page.
    """
    total_items = len(items)

    if items_per_page is None:
        items_per_page = total_items

    start = 0

    while start < total_items:
        end = min(start + items_per_page, total_items)
        display_items = items[start:end]

        # Display the current page of items
        from rich.table import Table
        table = Table(title=title, header_style="bold green on #282828", show_lines=True)
        max_rows = 7
        num_columns = (len(display_items) + max_rows - 1) // max_rows

        for _ in range(num_columns):
            table.add_column("", style="highlight", justify="left")

        rows = [[] for _ in range(max_rows)]
        for index, item in enumerate(display_items):
            row_index = index % max_rows
            rows[row_index].append(f"{start + index + 1}. {item}")

        for row in rows:
            row += [""] * (num_columns - len(row))
            table.add_row(*row)

        console.print(table)

        if end < total_items:
            proceed = Prompt.ask("\nMore results available. Would you like to see more? (yes/no)", default="yes")
            if proceed.lower() != "yes":
                break

        start += items_per_page

def search_by_index():
    """Search and display economic data by selected index."""
    try:
        url = "https://fincept.share.zrok.io/LargeEconomicDatabase/tables"
        response = requests.get(url)
        response.raise_for_status()
        indices = response.json()

        # Filter out "countries" from the list
        indices = [index for index in indices if index != "countries"]
        indices.append("Back to Main Menu")  # Add the back option

        display_in_columns("Select an Index", indices)

        index_choice = Prompt.ask("Enter your choice")
        selected_index = indices[int(index_choice) - 1]

        if int(index_choice) == len(indices):  # Check if the user chose the "Back to Main Menu" option
            from fincept_terminal.cli import show_main_menu
            return show_main_menu()

        # Fetch data for the selected index
        index_data_url = f"https://fincept.share.zrok.io/LargeEconomicDatabase/{selected_index}/data"
        response = requests.get(index_data_url)
        response.raise_for_status()
        index_data = response.json()

        index_items = [item['column_name'] for item in index_data]
        paginate_display_in_columns(f"Data for {selected_index.replace('_', ' ').title()}", index_items)

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching index data: {e}[/bold red]", style="danger")


def paginate_display_in_columns(title, items, items_per_page=21):
    """Paginate and display items in columns."""
    total_items = len(items)
    for start in range(0, total_items, items_per_page):
        end = start + items_per_page
        display_in_columns(title, items[start:end])

        if end < total_items:
            proceed = Prompt.ask("\nMore results available. Would you like to see more? (yes/no)", default="yes")
            if proceed.lower() != "yes":
                break
