import os
import sys
import json
import sqlite3
from fincept_terminal.FinceptTerminalUtilsModule.themes import console
from fincept_terminal.FinceptTerminalUtilsModule.const import display_in_columns
from datagovindia import DataGovIndia, check_api_key


SETTINGS_FILE = os.path.join(os.path.dirname(__file__), "..", "..", "FinceptTerminalSettingModule", "FinceptTerminalSettingModule.json")
DB_PATH = os.path.expanduser("~/datagovindia.db")  # Default database path


def validate_api_key():
    """
    Validate the API key for the OGD platform.
    Fetches the API key for 'World Economy Tracker' from the 'FinceptTerminalSettingModule.json' file in the FinceptTerminalSettingModule folder.
    """
    # Check if the FinceptTerminalSettingModule.json file exists
    if not os.path.exists(SETTINGS_FILE):
        console.print("[bold red]Settings file not found. Please ensure 'FinceptTerminalSettingModule.json' exists in the FinceptTerminalSettingModule folder.[/bold red]")
        sys.exit(1)

    # Load the FinceptTerminalSettingModule.json file
    try:
        with open(SETTINGS_FILE, "r") as file:
            settings = json.load(file)
            world_economy_tracker = settings.get("data_sources", {}).get("World Economy Tracker", {})
            api_key = world_economy_tracker.get("api_key")
    except json.JSONDecodeError:
        console.print("[bold red]Error: 'FinceptTerminalSettingModule.json' file is corrupted or invalid.[/bold red]")
        sys.exit(1)

    # Validate the presence of the API key
    if not api_key:
        console.print("[bold red]API key not found for 'World Economy Tracker' in 'FinceptTerminalSettingModule.json'. Please add it to the file.[/bold red]")
        sys.exit(1)

    # Validate the API key using the check_api_key function
    if not check_api_key(api_key):
        console.print("[bold red]Invalid API key for 'World Economy Tracker'. Please set a valid key in 'FinceptTerminalSettingModule.json'.[/bold red]")
        sys.exit(1)

    console.print("[bold green]API key for 'World Economy Tracker' validated successfully![/bold green]")
    return api_key

def get_all_resources(db_path=DB_PATH):
    """
    Retrieve all resources (title and resource_id) directly from the database.

    Args:
        db_path (str): Path to the SQLite database file.

    Returns:
        list: A list of dictionaries with 'title' and 'resource_id' keys.
    """
    try:
        # Connect to the SQLite database
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()

        # Query to fetch title and resource_id
        query = "SELECT title, resource_id FROM resources"
        cursor.execute(query)
        resources = [{"title": row[0], "resource_id": row[1]} for row in cursor.fetchall()]

        # Close the connection
        conn.close()

        return resources
    except sqlite3.Error as e:
        console.print(f"[bold red]Error fetching resources from the database: {e}[/bold red]")
        return []

def initialize_metadata(api_key):
    """
    Ensure the metadata is initialized in the database.
    If the database or metadata is missing, this will update it and return resources.

    Args:
        api_key (str): API key for authentication.

    Returns:
        list: A list of resources (dictionaries with 'title' and 'resource_id' keys).
    """
    try:
        # Initialize the DataGovIndia wrapper with the provided API key
        datagovin = DataGovIndia(api_key=api_key, db_path=DB_PATH)

        # Check if the database file exists
        if not os.path.exists(DB_PATH):
            console.print("[bold yellow]Database file not found. Creating a new database...[/bold yellow]")

        # Check if metadata is already synced
        try:
            update_info = datagovin.get_update_info()
            if update_info:
                console.print(f"[bold green]Metadata last updated: {update_info}[/bold green]")
            else:
                console.print("[bold cyan]No update info available. Initializing metadata...[/bold cyan]")

            # Fetch resources (title and resource_id)
            resources = get_all_resources(DB_PATH)
            if resources:
                console.print(f"[bold green]Found {len(resources)} resources in the database.[/bold green]")
                return resources
            else:
                console.print("[bold yellow]No resources found in the database. Syncing metadata...[/bold yellow]")

        except Exception as e:
            console.print(f"[bold cyan]Metadata check failed: {e}. Initializing metadata...[/bold cyan]")

        # Sync metadata if not found or invalid
        datagovin.sync_metadata()
        resources = get_all_resources()
        console.print("[bold green]Metadata initialized successfully and resources fetched.[/bold green]")
        return resources

    except Exception as e:
        console.print(f"[bold red]Error initializing metadata: {e}[/bold red]")
        sys.exit(1)

def fetch_resource_info(api_key, resource_id):
    """
    Fetch information about a specific resource using its resource ID.

    Args:
        api_key (str): The API key for the DataGovIndia platform.
        resource_id (str): The ID of the resource to fetch information about.

    Returns:
        dict: The resource information, or None if not found or an error occurs.
    """
    try:
        datagovin = DataGovIndia(api_key=api_key, db_path=DB_PATH)
        console.print(f"[bold cyan]Fetching info for Resource ID: {resource_id}...[/bold cyan]")
        resource_info = datagovin.get_resource_info(resource_id)

        if not resource_info:
            console.print(f"[bold yellow]No information found for Resource ID: {resource_id}[/bold yellow]")
            return None

        console.print(f"[bold green]Resource info fetched successfully for Resource ID: {resource_id}[/bold green]")
        return resource_info

    except Exception as e:
        console.print(f"[bold red]Error fetching resource info: {e}[/bold red]")
        return None


def fetch_resource_data(api_key, resource_id, limit=5):
    """
    Fetch data for a specific resource ID.
    """
    try:
        datagovin = DataGovIndia(api_key=api_key, db_path=DB_PATH)
        console.print(f"[bold cyan]Fetching data for resource ID: {resource_id}[/bold cyan]")
        data = datagovin.get_data(resource_id, limit=limit)
        return data
    except Exception as e:
        console.print(f"[bold red]Error fetching data for resource ID '{resource_id}': {e}[/bold red]")
        return None

def display_resources_paginated(resources, title="Select a Resource", start_index=0, page_size=10):
    """
    Paginate and display a list of resources for user selection.

    Args:
        resources (list): List of resources (dictionaries with 'name' and 'resource_id' keys).
        title (str): Title for the displayed list.
        start_index (int): Starting index for pagination.
        page_size (int): Number of items per page.

    Returns:
        dict: The selected resource or None if the user exits.
    """
    while True:
        end_index = min(start_index + page_size, len(resources))
        current_page = resources[start_index:end_index]

        # Display resources using a table
        from rich.table import Table
        table = Table(title=f"{title} (Showing {start_index + 1}-{end_index} of {len(resources)})", show_lines=True)
        table.add_column("Number", justify="right", style="cyan")
        table.add_column("Name", style="bold white")
        table.add_column("Resource ID", style="green")

        for i, resource in enumerate(current_page, start=start_index + 1):
            table.add_row(str(i), resource["title"], resource["resource_id"])

        console.print(table)

        # Prompt for user action
        from rich.prompt import Prompt
        action = Prompt.ask(
            "\n[bold cyan]Next Page (N) / Previous Page (P) / Select (1-{0}) / Exit (E)[/bold cyan]".format(len(current_page))
        )

        if action.lower() == 'n':
            # Go to the next page
            if end_index < len(resources):
                start_index += page_size
            else:
                console.print("[bold red]No more pages available.[/bold red]")
        elif action.lower() == 'p':
            # Go to the previous page
            if start_index > 0:
                start_index -= page_size
            else:
                console.print("[bold red]Already on the first page.[/bold red]")
        elif action.lower() == 'e':
            # Exit selection
            console.print("[bold yellow]Exiting resource selection...[/bold yellow]")
            return None
        else:
            # Handle selection
            try:
                selected_index = int(action) - 1
                if start_index <= selected_index < end_index:
                    return current_page[selected_index]
                else:
                    console.print("[bold red]Invalid selection. Please select a valid number.[/bold red]")
            except ValueError:
                console.print("[bold red]Invalid input. Please enter a number, 'N', 'P', or 'E'.[/bold red]")

def display_resource_info(resource_info):
    """
    Display the resource information in two tables:
    1. General resource information.
    2. Fields information (separate table for fields).

    Args:
        resource_info (dict): The resource information to display.
    """
    from rich.table import Table
    if not resource_info:
        console.print("[bold red]No resource information available to display.[/bold red]")
        return

    # Table 1: General Resource Information
    general_table = Table(title="Resource Information", header_style="bold green on black", show_lines=True)
    general_table.add_column("Attribute", style="cyan", justify="left")
    general_table.add_column("Value", style="green", justify="left")

    # Exclude fields from the general table
    for key, value in resource_info.items():
        if key == "field":
            continue  # Skip fields for the general table
        # Format values
        if isinstance(value, list):
            value = ", ".join(map(str, value))
        elif isinstance(value, bool):
            value = "True" if value else "False"
        elif value is None:
            value = "N/A"
        general_table.add_row(key, str(value))

    console.print(general_table)

    # Table 2: Fields Information
    fields = resource_info.get("field", [])
    if fields:
        fields_table = Table(title="Fields Information", header_style="bold green on black", show_lines=True)
        fields_table.add_column("ID", style="cyan", justify="left")
        fields_table.add_column("Name", style="green", justify="left")
        fields_table.add_column("Type", style="cyan", justify="left")

        for field in fields:
            fields_table.add_row(field.get("id", "N/A"), field.get("name", "N/A"), field.get("type", "N/A"))

        console.print(fields_table)
    else:
        console.print("[bold yellow]No fields available for this resource.[/bold yellow]")

def display_resource_data(resource_data):
    """
    Display the fetched resource data in a structured format with column headers.

    Args:
        resource_data (DataFrame): The resource data fetched using `get_data`.
    """
    if resource_data.empty:
        console.print("[bold red]No data available for this resource.[/bold red]")
        return

    # Display a maximum of 5 rows for better readability
    max_rows = 5
    data_to_display = resource_data.head(max_rows)

    # Extract column headers and rows
    headers = data_to_display.columns.tolist()
    rows = data_to_display.values.tolist()

    # Format the headers and rows for display
    formatted_data = [" | ".join(headers)]  # Add headers as the first row
    for row in rows:
        formatted_data.append(" | ".join(map(str, row)))  # Add each row as a string

    # Display the formatted data using display_in_columns
    title = f"Resource Data (First {max_rows} Rows)"
    display_in_columns(title, formatted_data)

    if len(resource_data) > max_rows:
        console.print(f"[bold yellow]Only the first {max_rows} rows are displayed.[/bold yellow]")

def download_resource_data(api_key, resource_id, output_file):
    """
    Download resource data in the specified format.

    Args:
        api_key (str): The API key for authentication.
        resource_id (str): The resource ID to fetch data for.
        output_file (str): The output file path with format (e.g., .csv, .json, .xlsx).

    Returns:
        bool: True if the data is successfully downloaded, False otherwise.
    """
    try:
        datagovin = DataGovIndia(api_key=api_key)
        console.print(f"[bold cyan]Downloading data for Resource ID: {resource_id}...[/bold cyan]")
        data = datagovin.get_data(resource_id)

        # Save data based on the specified file extension
        if output_file.endswith(".csv"):
            data.to_csv(output_file, index=False)
        elif output_file.endswith(".json"):
            data.to_json(output_file, orient="records", indent=4)
        elif output_file.endswith(".xlsx"):
            data.to_excel(output_file, index=False, engine="openpyxl")
        else:
            console.print("[bold red]Invalid file format. Supported formats: .csv, .json, .xlsx[/bold red]")
            return False

        return True
    except Exception as e:
        console.print(f"[bold red]Error downloading resource data: {e}[/bold red]")
        return False

def show_datagovindia_menu():
    """
    Show the DataGovIndia module menu.
    """
    console.print("[bold cyan]DATAGOVINDIA MENU[/bold cyan]\n")

    # Step 1: Validate API Key
    api_key = validate_api_key()

    # Step 2: Initialize Metadata
    resources = initialize_metadata(api_key)
    # console.print(resources)
    # Step 3: Fetch all resource IDs and names
    #resources = fetch_resource_ids_with_names(api_key)
    if not resources:
        return

    # Step 4: Paginated Display of Resource IDs and Names
    while True:
        selected_resource = display_resources_paginated(resources, title="Available Resources")
        if not selected_resource:
            console.print("[bold yellow]No selection made. Returning to the main menu...[/bold yellow]")
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            break

        # Extract the selected resource ID
        resource_id = selected_resource["resource_id"]

        # Step 5: Fetch and Display Resource Info
        try:
            resource_info = fetch_resource_info(api_key, resource_id)
            if resource_info:
                console.print(f"[bold green]Resource Info for Resource ID: {resource_id}[/bold green]")
                display_resource_info(resource_info)  # Display resource info

                # Step 6: Fetch and Display Resource Data
                data = fetch_resource_data(api_key, resource_id)
                if data is not None:
                    console.print(f"[bold green]Data for Resource ID: {resource_id}[/bold green]")
                    items = data.head(5).to_dict()
                    display_resource_info(items)  # Display first 5 rows of the data

                    # Step 7: Prompt for Download Option
                    from rich.prompt import Prompt
                    download_option = Prompt.ask(
                        "\nDo you want to download this data? (yes/no)", default="no"
                    ).lower()

                    if download_option == "yes":
                        file_name = Prompt.ask("Enter the file name (e.g., data.csv, data.json, data.xlsx)").strip()
                        download_success = download_resource_data(api_key, resource_id, file_name)
                        if download_success:
                            console.print(f"[bold green]Data successfully downloaded as {file_name}![/bold green]")
                        else:
                            console.print("[bold red]Failed to download the data.[/bold red]")
                else:
                    console.print(f"[bold red]No data found for Resource ID: {resource_id}[/bold red]")
            else:
                console.print(f"[bold red]No resource info found for Resource ID: {resource_id}[/bold red]")

            # Prompt for another query or exit
            continue_query = Prompt.ask("\nDo you want to query another resource ID? (yes/no)", default="no").lower()
            if continue_query != "yes":
                console.print("[bold yellow]Exiting DataGovIndia module...[/bold yellow]")
                from fincept_terminal.oldTerminal.cli import show_main_menu
                show_main_menu()
                break
        except Exception as e:
            console.print(f"[bold red]Error processing resource data: {e}[/bold red]")
