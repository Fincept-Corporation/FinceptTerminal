import requests
from rich.prompt import Prompt
from rich.panel import Panel
from fincept_terminal.themes import console
from fincept_terminal.const import display_in_columns
from statsmodels.tsa.arima.model import ARIMA

# Your FRED API key
FRED_API_KEY = "186968c7ec98984b5d08393e93aedf2b"

def show_world_economy_tracker_menu():
    """World Economy Tracker menu."""
    while True:
        console.print("[highlight]WORLD ECONOMY TRACKER[/highlight]\n", style="info")

        menu_options = [
            "LIST OF COUNTRIES",
            "SEARCH BY INDEX",
            "QUERY FRED DATA",
            "BACK TO MAIN MENU"
        ]

        display_in_columns("WORLD ECONOMY TRACKER MENU", menu_options)

        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            show_list_of_countries()
        elif choice == "2":
            search_by_index()
        elif choice == "3":
            query_fred_data()
        elif choice == "4":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")


def show_list_of_countries():
    """Display the list of countries."""
    try:
        url = "https://fincept.share.zrok.io/metaData/world_country_list/data"
        response = requests.get(url)
        response.raise_for_status()
        countries_data = response.json()

        countries = [item['name'] for item in countries_data]
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

def display_series_id_paginated(indices, start_index=0, page_size=21):
    end_index = min(start_index + page_size, len(indices))
    indices_page = indices[start_index:end_index]

    # Extract only the names of the indices to display
    index_names = [index['id'] for index in indices_page]

    # Display the current page of indices
    display_in_columns(f"Indices (Showing {start_index + 1} - {end_index})", index_names)

    # Ask the user to navigate or select an index
    from rich.prompt import Prompt
    action = Prompt.ask(
        "\n[bold cyan]Next Page (N) / Previous Page (P) / Select an Index (1-{0})[/bold cyan]".format(len(index_names)))

    if action.lower() == 'n':
        if end_index < len(indices):
            return display_series_id_paginated(indices, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_series_id_paginated(indices, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            return display_series_id_paginated(indices, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_series_id_paginated(indices, start_index)
    else:
        try:
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(indices):
                return indices[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_series_id_paginated(indices, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number or 'N'/'P'.[/bold red]")
            return display_series_id_paginated(indices, start_index)

def query_fred_data():
    """Query and display FRED data, with an option to perform ARIMA analysis."""
    try:
        fredCategoryData_url = "https://fincept.share.zrok.io/metaData/fredCategoryData/data?limit=30"
        response = requests.get(fredCategoryData_url)
        response.raise_for_status()
        fredcategorydata = response.json()

        #series_id = [item['id'] for item in fredcategorydata]
        selected_series = display_series_id_paginated(fredcategorydata)
        selected_id = selected_series['id']

        # Prompt the user to enter a FRED series ID
        #series_id = Prompt.ask("Enter the FRED Series ID (e.g., GDP, UNRATE)")

        # Build the FRED API URL
        fred_url = f"https://api.stlouisfed.org/fred/series/observations?series_id={selected_id}&api_key={FRED_API_KEY}&file_type=json"
        response = requests.get(fred_url)
        response.raise_for_status()
        fred_data = response.json()

        if "observations" in fred_data:
            observations = fred_data["observations"]
            if observations:
                import pandas as pd
                # Convert observations to a DataFrame
                df = pd.DataFrame(observations)
                df['date'] = pd.to_datetime(df['date'])
                df['value'] = pd.to_numeric(df['value'], errors='coerce')

                console.print(f"[bold green]FRED Data for {selected_id.upper()} retrieved successfully.[/bold green]")

                display_items = [f"Date: {row['date'].strftime('%Y-%m-%d')} | Value: {row['value']}" for _, row in df.iterrows()]
                paginate_display_in_columns(f"FRED Data for {selected_id.upper()}", display_items)

                # Ask the user if they want to perform ARIMA analysis
                perform_arima = Prompt.ask("Would you like to perform ARIMA analysis on this data? (yes/no)", default="yes")

                if perform_arima.lower() == 'yes':
                    arima_analysis(df, selected_id)

                # Return to the World Economy Tracker menu
                show_world_economy_tracker_menu()

            else:
                console.print(f"[bold red]No data found for the FRED Series ID: {selected_id}[/bold red]", style="danger")
        else:
            console.print(f"[bold red]Error retrieving FRED data: {fred_data.get('error_message', 'Unknown error')}[/bold red]", style="danger")

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching FRED data: {e}[/bold red]", style="danger")

def arima_analysis(df, series_id):
    """Perform ARIMA analysis on the FRED data."""
    try:
        # Set the date as the index
        df.set_index('date', inplace=True)

        # Drop NaN values
        df.dropna(inplace=True)

        # Prompt the user for ARIMA order (p, d, q)
        p = Prompt.ask("Enter the ARIMA order p (AutoRegressive part)", default="1")
        d = Prompt.ask("Enter the ARIMA order d (Differencing part)", default="1")
        q = Prompt.ask("Enter the ARIMA order q (Moving Average part)", default="1")

        # Convert input to integers
        p, d, q = int(p), int(d), int(q)

        # Fit the ARIMA model
        model = ARIMA(df['value'], order=(p, d, q))
        fitted_model = model.fit()

        # Display the model summary
        console.print(f"[bold cyan]ARIMA Model Summary for {series_id.upper()}[/bold cyan]")
        console.print(f"[bold yellow]{fitted_model.summary()}[/bold yellow]")

        # Forecast the next 10 periods
        import pandas as pd
        import matplotlib.pyplot as plt
        forecast = fitted_model.forecast(steps=10)
        forecast_df = pd.DataFrame({
            'date': pd.date_range(start=df.index[-1] + pd.DateOffset(1), periods=10, freq='M'),
            'forecast': forecast
        })

        # Plot the original data and the forecast
        plt.figure(figsize=(10, 6))
        plt.plot(df['value'], label='Original Data')
        plt.plot(forecast_df['date'], forecast_df['forecast'], label='Forecast', color='red')
        plt.title(f"ARIMA Forecast for {series_id.upper()}")
        plt.legend()
        plt.show()

        # Display forecasted values
        forecast_items = [f"Date: {row['date'].strftime('%Y-%m-%d')} | Forecast: {row['forecast']}" for _, row in forecast_df.iterrows()]
        paginate_display_in_columns(f"ARIMA Forecast for {series_id.upper()}", forecast_items)

    except Exception as e:
        console.print(f"[bold red]Error during ARIMA analysis: {e}[/bold red]", style="danger")
