def show_market_tracker_menu():
    while True:
        from fincept_terminal.themes import console
        console.print("[highlight]MARKET TRACKER[/highlight]\n")

        tracker_text = """
1. FII/DII DATA INDIA
2. NIFTY 50 LIST
3. SEARCH ASSETS
4. BACK TO MAIN MENU
        """

        from rich.panel import Panel
        tracker_panel = Panel(tracker_text, title="SELECT AN OPTION", title_align="center", style="highlight", padding=(1, 2))
        console.print(tracker_panel)
        import click
        choice = click.prompt("Please select an option", type=int)

        if choice == 1:
            from fincept_terminal.data_fetcher import display_fii_dii_data
            display_fii_dii_data()
        elif choice == 2:
            console.print("\n[warning]NIFTY 50 LIST SELECTED.[/warning] (This feature is under development)\n")
        elif choice == 3:
            from fincept_terminal.assets import search_assets
            search_assets()
        elif choice == 4:
            break
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")
