import requests
import yfinance as yf

# Step 1: Fetch the list of ETF families
def fetch_etf_families():
    url = "https://fincept.share.zrok.io/FinanceDB/etfs/family/data?unique=true"
    response = requests.get(url)
    if response.status_code == 200:
        data = response.json()
        # Filter out null values from the family list
        return [family for family in data['family'] if family]
    else:
        from fincept_terminal.utils.themes import console
        console.print(f"[bold red]Failed to fetch ETF families: {response.status_code}[/bold red]")
        return []


# Step 2: Paginate the ETF families list
def display_etf_families_paginated(families, start_index=0, page_size=21):
    # Determine end index and slice the list
    end_index = min(start_index + page_size, len(families))
    families_page = families[start_index:end_index]

    # Display the current page of ETF families
    from fincept_terminal.utils.const import display_in_columns
    display_in_columns(f"Select an ETF Family (Showing {start_index + 1} - {end_index})", families_page)

    # Ask the user to navigate or select an ETF family
    from rich.prompt import Prompt
    action = Prompt.ask("\n[bold cyan]Next Page (N) / Previous Page (P) / Select a Family (1-{0})[/bold cyan]".format(
        len(families_page)))

    from fincept_terminal.utils.themes import console
    if action.lower() == 'n':
        if end_index < len(families):
            # If there's a next page, display it
            return display_etf_families_paginated(families, start_index + page_size)
        else:
            console.print("[bold red]No more pages.[/bold red]")
            return display_etf_families_paginated(families, start_index)
    elif action.lower() == 'p':
        if start_index > 0:
            # If there's a previous page, display it
            return display_etf_families_paginated(families, start_index - page_size)
        else:
            console.print("[bold red]You are on the first page.[/bold red]")
            return display_etf_families_paginated(families, start_index)
    else:
        try:
            # Convert action to an integer to select a family
            selected_index = int(action) - 1 + start_index
            if 0 <= selected_index < len(families):
                return families[selected_index]
            else:
                console.print("[bold red]Invalid selection. Please try again.[/bold red]")
                return display_etf_families_paginated(families, start_index)
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a number or 'N'/'P'.[/bold red]")
            return display_etf_families_paginated(families, start_index)


# Step 3: Fetch ETF details by family
def fetch_etfs_by_family(family_name):
    url = f"https://fincept.share.zrok.io/FinanceDB/etfs/family/filter?value={family_name}"
    response = requests.get(url)
    if response.status_code == 200:
        return response.json()
    else:
        from fincept_terminal.utils.themes import console
        console.print(f"[bold red]Failed to fetch ETFs for {family_name}: {response.status_code}[/bold red]")
        return []


# Step 4: Fetch ETF details by symbol using yfinance
def fetch_etf_data_by_symbol(symbol):
    ticker = yf.Ticker(symbol)
    info = ticker.info
    # Filter out unnecessary fields
    filtered_info = {k: v for k, v in info.items() if
                     k not in ['messageBoardId', 'gmtOffSetMilliseconds', 'uuid', 'maxAge']}
    return filtered_info


# Step 5: Main function to show the ETF menu
def show_etf_market_menu():
    from fincept_terminal.utils.themes import console
    console.print("[bold cyan]EXCHANGE TRADED FUNDS MENU[/bold cyan]\n", style="info")

    from rich.prompt import Prompt

    while True:
        from fincept_terminal.utils.const import display_in_columns
        # Display two options: Global Search or Search by Symbol
        options = ["Global Search", "Search by Symbol"]
        display_in_columns("Select an Option", options)

        choice = Prompt.ask("Enter the number corresponding to your choice")

        from fincept_terminal.utils.const import display_info_in_three_columns
        if choice == "1":  # Global Search
            families = fetch_etf_families()
            if not families:
                return
            selected_family = display_etf_families_paginated(families)
            if selected_family:
                etfs = fetch_etfs_by_family(selected_family)
                etf_names = [etf['name'] for etf in etfs]
                display_in_columns(f"ETFs in {selected_family}", etf_names)

                # Ask the user to select an ETF
                etf_choice = int(Prompt.ask("Enter the number corresponding to the ETF")) - 1
                selected_etf = etfs[etf_choice]

                # Display the ETF summary before the detailed data
                console.print(f"\n[bold yellow]{selected_etf['summary']}[/bold yellow]\n")

                # Fetch and display the ETF details from yfinance
                etf_data = fetch_etf_data_by_symbol(selected_etf['symbol'])
                display_info_in_three_columns(etf_data)

                # Ask if the user wants to query another ETF or exit
                continue_query = Prompt.ask("Do you want to query another ETF? (yes/no)")
                if continue_query.lower() != "yes":
                    console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
                    from fincept_terminal.cli import show_main_menu
                    show_main_menu()
                    break  # Exit the loop and return to the main menu

        elif choice == "2":  # Search by Symbol
            symbol = Prompt.ask("Enter the ETF symbol")
            etf_data = fetch_etf_data_by_symbol(symbol)
            display_info_in_three_columns(etf_data)

            # Ask if the user wants to query another ETF or exit
            continue_query = Prompt.ask("Do you want to query another ETF? (yes/no)")
            if continue_query.lower() != "yes":
                console.print("\n[bold yellow]Redirecting to the main menu...[/bold yellow]", style="info")
                from fincept_terminal.cli import show_main_menu
                show_main_menu()
                break  # Exit the loop and return to the main menu
