import requests
import re
from rich.table import Table
from rich.prompt import Prompt
from rich.console import Console
from rich.theme import Theme

# Define a custom theme with a dark background and bright white text
custom_theme = Theme({
    "info": "bright_white on black",
    "danger": "bold bright_red on black",
    "warning": "bold yellow on black",
    "success": "bold green on black",
    "highlight": "bold bright_white on black",
})

# Create a console with the custom theme
console = Console(theme=custom_theme)

def clean_html(raw_html):
    """Remove HTML tags and return clean text."""
    clean_text = re.sub('<.*?>', '', raw_html)
    return clean_text

def fetch_json_data(url):
    """Fetch JSON data from the given URL."""
    response = requests.get(url)
    response.raise_for_status()
    return response.json()

def paginate_display_in_columns(title, items, items_per_page=21):
    """Paginate and display items in columns with numeric selection."""
    total_items = len(items)
    item_numbers = list(range(1, total_items + 1))

    for start in range(0, total_items, items_per_page):
        end = start + items_per_page
        numbered_items = [f"{num}. {item}" for num, item in zip(item_numbers[start:end], items[start:end])]
        display_in_columns(title, numbered_items)

        if end < total_items:
            proceed = Prompt.ask("\nMore results available. Would you like to see more? (yes/no)", default="yes")
            if proceed.lower() != "yes":
                break

def display_in_columns(title, items):
    """Display items in columns."""
    table = Table(title=title, style="highlight", title_style="bright_white on black", border_style="bright_white")
    columns = 3
    rows = [items[i:i + columns] for i in range(0, len(items), columns)]
    max_cols = max(len(row) for row in rows)
    for i in range(max_cols):
        table.add_column(f"Column {i + 1}", style="bright_white")
    for row in rows:
        table.add_row(*[item for item in row])
    console.print(table)

def display_json_data(data):
    """Display a single entry of JSON data in a table format."""
    table = Table(show_header=True, header_style="bright_white on black", border_style="bright_white")
    table.add_column("Field", style="bright_white")
    table.add_column("Value", style="bright_white")

    for key, value in data.items():
        if isinstance(value, str):
            clean_value = clean_html(value)
        else:
            clean_value = str(value)
        table.add_row(key, clean_value)

    console.print(table)

def display_table_list(tables):
    """Display the list of tables in a paginated format, showing only 'resource_info' items."""
    info_tables = [table for table in tables if "resource_info" in table]
    paginate_display_in_columns("Select a Data Table", info_tables)

def query_table_data(table_name):
    """Fetch and display data from the selected table."""
    info_url = f"https://datos.gob.es/en/apidata#/{table_name}/data"
    data_url = info_url.replace("resource_info", "resource_data")

    try:
        # Fetch and display information about the resource
        info_data = fetch_json_data(info_url)
        if info_data:
            console.print("\n[highlight]Resource Information:[/highlight]", style="highlight")
            display_json_data(info_data[0])

            # Ask if the user wants to see the data of the resource
            query_data = Prompt.ask("\nWould you like to see the data of this resource? (yes/no)", default="no")
            if query_data.lower() == "yes":
                # Fetch and display the data
                data = fetch_json_data(data_url)
                if data:
                    console.print("\n[highlight]Resource Data:[/highlight]", style="highlight")
                    display_static_table(data)
                else:
                    console.print(f"[danger]No data available for {data_url}[/danger]", style="danger")
        else:
            console.print(f"[danger]No information available for {table_name}[/danger]", style="danger")
    except requests.exceptions.RequestException as e:
        console.print(f"[danger]An error occurred while fetching data: {e}[/danger]", style="danger")

    another_query = Prompt.ask("\nWould you like to query another table? (yes/no)", default="no")
    if another_query.lower() == "yes":
        show_spain_macro_micro_menu()
    else:
        console.print("[highlight]Returning to the main menu...[/highlight]", style="highlight")
        from fincept_terminal.oldTerminal.cli import show_main_menu
        show_main_menu()

def display_static_table(data):
    """Display data in a static table format with all columns."""
    # Define the table structure based on the first entry's keys
    table = Table(show_header=True, header_style="bright_white on black", border_style="bright_white")
    for key in data[0].keys():
        table.add_column(key, style="bright_white")

    # Add rows to the table
    for entry in data:
        row = [str(entry.get(key, 'N/A')) for key in data[0].keys()]
        table.add_row(*row)

    console.print(table)

def show_spain_macro_micro_menu():
    """Show the Spain Macro & Micro Data menu."""
    console.print("[highlight]Spain Macro & Micro Data[/highlight]\n", style="highlight")

    # Fetch the list of available tables
    url = "https://datos.gob.es/en/apidata#/tables"
    try:
        tables = fetch_json_data(url)
        display_table_list(tables)

        selected_table_num = Prompt.ask("Enter the table number to query")
        selected_table = tables[int(selected_table_num) - 1]
        query_table_data(selected_table)

    except requests.exceptions.RequestException as e:
        console.print(f"[danger]An error occurred while fetching data: {e}[/danger]", style="danger")
    except (IndexError, ValueError):
        console.print("[danger]Invalid selection. Please enter a valid table number.[/danger]", style="danger")
        show_spain_macro_micro_menu()
