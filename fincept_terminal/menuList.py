def show_market_tracker_menu():
    """Market Tracker submenu."""
    while True:
        from fincept_terminal.themes import console
        console.print("[highlight]MARKET TRACKER[/highlight]\n", style="info")

        tracker_text = """
1. FII/DII DATA INDIA
2. NIFTY 50 LIST
3. SEARCH ASSETS
4. BACK TO MAIN MENU
        """

        from rich.panel import Panel
        tracker_panel = Panel(tracker_text, title="MARKET TRACKER MENU", title_align="center",
                              style="bold green on #282828", padding=(1, 2))
        console.print(tracker_panel)

        from rich.prompt import Prompt
        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            from fincept_terminal.data_fetcher import display_fii_dii_data
            display_fii_dii_data()
        elif choice == "2":
            console.print("[bold yellow]Nifty 50 list under development[/bold yellow]", style="warning")
        elif choice == "3":
            from fincept_terminal.assets import search_assets
            search_assets()
        elif choice == "4":
            break


def show_country_menu(continent):
    """Display available countries for a given continent and allow the user to select one."""
    from fincept_terminal.data import get_countries_by_continent
    countries = get_countries_by_continent(continent)
    from fincept_terminal.themes import console

    if not countries:
        console.print(f"[bold red]No countries available for {continent}[/bold red]", style="danger")
        return False  # Indicate that there are no countries and return to the main menu

    console.print(f"[bold cyan]Countries in {continent}[/bold cyan]\n", style="info")
    from fincept_terminal.const import display_in_columns
    display_in_columns(f"Select a Country in {continent}", countries)

    console.print("\n")
    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")
    selected_country = countries[int(choice) - 1]
    console.print("\n")

    # Check if sectors are available, otherwise return to country menu
    from fincept_terminal.stock import show_sectors_in_country
    return show_sectors_in_country(selected_country)


# Equities submenu
def show_equities_menu():
    """Equities submenu that allows selection of stocks based on continent and country."""
    continents = ["Asia", "Europe", "Africa", "North America", "South America", "Oceania", "Middle East", "Main Menu"]

    while True:
        from fincept_terminal.themes import console
        console.print("[bold cyan]EQUITIES MENU[/bold cyan]\n", style="info")
        from fincept_terminal.const import display_in_columns
        display_in_columns("Select a Continent", continents)

        console.print("\n")
        from rich.prompt import Prompt
        choice = Prompt.ask("Enter your choice")
        console.print("\n")

        if choice == "8":
            from fincept_terminal.cli import show_main_menu
            show_main_menu()
            return  # Exit equities menu and return to main menu

        selected_continent = continents[int(choice) - 1]

        # If country/sector fetching fails, return to the continent selection
        if not show_country_menu(selected_continent):
            continue  # Loop back to continent selection if no valid data is found


