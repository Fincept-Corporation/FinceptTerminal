import click
import logging
import warnings
from rich.prompt import Prompt
from rich.console import Console
import webbrowser
import sys
import os
import json
from fincept_terminal.utils import update_checker
from fincept_terminal.utils.display import display_art
from fincept_terminal.utils.themes import console
from fincept_terminal.utils.const import display_in_columns
from fincept_terminal.auth.authentication import register_user, verify_otp

# Suppress warnings and loggers
logging.getLogger('numexpr').setLevel(logging.ERROR)
warnings.filterwarnings("ignore")
console = Console()

# Determine the base directory of the script
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SETTINGS_FOLDER = os.path.join(BASE_DIR, "settings")
SETTINGS_FILE = os.path.join(SETTINGS_FOLDER, "settings.json")

@click.group(invoke_without_command=True)
@click.version_option(version="1.0.0", prog_name="Fincept Investments Terminal")
@click.pass_context
def cli(ctx):
    """Fincept Investments CLI - Your professional financial terminal."""
    if ctx.invoked_subcommand is None:
        start()


@cli.command()
def start():
    """Start the Fincept Investments terminal."""
    # Check for updates on startup
    update_checker.check_for_update()

    # Display ASCII art
    display_art()

    # Authentication flow
    console.print("\n[bold cyan]Welcome to the Fincept Investments Terminal![/bold cyan]")
    console.print("[green]Please choose how you would like to proceed:[/green]")
    options = ["Guest User", "Register"]
    display_in_columns("Select an Option", options)

    choice = Prompt.ask("Enter your choice (1 for Guest, 2 for Register)")

    if choice == "1":
        console.print("\n[green]Continuing as Guest User...[/green]")
        show_main_menu()
    elif choice == "2":
        console.print("\n[cyan]Starting Registration Process...[/cyan]")
        handle_registration()
    else:
        console.print("\n[red]Invalid choice. Please restart the terminal and try again.[/red]")


def handle_registration():
    """Handles user registration and authentication."""
    os.makedirs(SETTINGS_FOLDER, exist_ok=True)

    # Step 1: Prompt user for inputs
    try:
        username = input("Enter your username: ")
        email = input("Enter your email address: ")
        password = input("Enter your password: ")
    except KeyboardInterrupt:
        console.print("[red]Input process aborted by user. Exiting registration.[/red]")
        return

    # Step 2: Register User
    console.print("[cyan]Registering user...[/cyan]")
    registration_data = register_user(username, email, password)  # Pass inputs to register_user
    if "api_key" not in registration_data:
        console.print("[red]Registration failed. Cannot proceed.[/red]")
        return

    # Step 3: Verify OTP
    console.print("[cyan]Verifying OTP...[/cyan]")
    otp_response = verify_otp(email)  # Pass email for OTP verification
    if otp_response.get("message") != "Email verified successfully!":
        console.print("[red]OTP verification failed. Cannot proceed.[/red]")
        return

    console.print("[green]OTP verification successful![/green]")

    api_key = registration_data["api_key"]
    console.print(f"[green]Registration successful! Your API Key: {api_key}[/green]")

    # Step 4: Save user settings to file
    console.print("[cyan]Saving user settings...[/cyan]")
    save_user_settings(api_key, email, username)

    console.print("[green]Registration and setup complete![/green]")

    # Display the main menu
    show_main_menu()


def save_user_settings(api_key, email, username):
    """Save user details to a JSON file."""
    settings = {
        "api_key": api_key,
        "email": email,
        "username": username,
    }

    with open(SETTINGS_FILE, "w") as file:
        json.dump(settings, file, indent=4)

    console.print(f"[green]User settings saved to {SETTINGS_FILE}[/green]")


def show_main_menu():
    """Main menu with navigation options and other details."""
    console.print("\n")
    console.print("[bold cyan]MAIN MENU[/bold cyan]\n")

    main_menu_options = [
        "GLOBAL INDICES",  # 1
        "WORLD ECONOMY TRACKER",  # 2
        "GLOBAL NEWS & SENTIMENT",  # 3
        "GLOBAL STOCKS (EQUITIES)",  # 4
        "GLOBAL FUNDS",  # 5
        "EXCHANGE TRADED FUNDS",  # 6
        "GLOBAL BOND MARKET",  # 7 fixed income to add
        "OPTIONS & DERIVATIVES",  # 8
        "CRYPTOCURRENCY",  # 9
        "PORTFOLIO & INVESTMENT TOOLS",  # 10
        "GLOBAL STOCK SCANNER",  # 11
        "CURRENCY MARKETS (FOREX)",  # 12
        "COMMODITIES",  # 13
        "MONEY MARKET",  # 14
        "BACKTESTING STOCKS",  # 15
        "ROBO ADVISOR",  # 16
        "ADVANCE TECHNICALS",  # 17
        "GENAI QUERY",  # 18
        "FINSCRIPT",  # 19
        "EDUCATION & RESOURCES",  # 20
        "SETTINGS & CUSTOMIZATION",  # 21
        "TERMINAL DOCUMENTATION",  # 22
        "CONSUMER BEHAVIOUR",  # 23
        "REPORTS AND ANALYTICS",  # 24
        "SATELLITE IMAGERY ANALYSIS",  # 25
        "MARINE TRADE DATA",  # 26
        "INDIA MACRO & MICRO DATA",  # 27
        "EXIT",
        "COMPARISON ANALYSIS"
    ]

    # Display main menu in columns
    display_in_columns("Select an Option", main_menu_options)

    console.print("\n")
    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")

    try:
        choice = int(choice)  # Convert input to an integer
    except ValueError:
        console.print("[bold red]Invalid input. Please enter a valid option number.[/bold red]")
        return show_main_menu()  # Show the menu again on invalid input

    console.print("\n")

    # Call corresponding function based on the choice
    if choice == 1:
        from fincept_terminal.global_data.indices import show_global_indices_menu
        show_global_indices_menu()
    elif choice == 2:
        from fincept_terminal.world_economy_tracker.economytracker import show_world_economy_tracker_menu
        show_world_economy_tracker_menu()
    elif choice == 3:
        from fincept_terminal.global_data.newsfetch import show_news_and_sentiment_menu
        show_news_and_sentiment_menu()
    elif choice == 4:
        from fincept_terminal.global_data.stock import show_equities_menu
        show_equities_menu()
    elif choice == 5:
        from fincept_terminal.global_data.funds import show_global_funds_menu
        show_global_funds_menu()
    elif choice == 6:
        from fincept_terminal.exchange_traded_funds.etfdata import show_etf_market_menu
        show_etf_market_menu()
    elif choice == 7:
        from fincept_terminal.global_data.bonds import show_bond_market_menu
        show_bond_market_menu()
    elif choice == 8:
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
        show_main_menu()
    elif choice == 9:
        from fincept_terminal.cryptocurrency.cryptodata import crypto_main_menu
        crypto_main_menu()
    elif choice == 10:
        from fincept_terminal.portfolio_and_investement_tool.portfolio import show_portfolio_menu
        show_portfolio_menu()
    elif choice == 11:
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
        show_main_menu()
    elif choice == 12:
        from fincept_terminal.currency_markets.currencyforex import show_currency_market_menu
        show_currency_market_menu()
    elif choice == 13:
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
        show_main_menu()
    elif choice == 14:
        from fincept_terminal.money_market.moneymarket import show_money_market_menu
        show_money_market_menu()
    elif choice == 15:
        from fincept_terminal.portfolio_and_investement_tool.portfolio import show_backtesting_menu
        show_backtesting_menu()
    elif choice == 16:
        from fincept_terminal.robo_advisory.roboadvisor import show_robo_advisor_menu
        show_robo_advisor_menu()
    elif choice == 17:
        from fincept_terminal.advance_technicals.advancetechnicals import show_technical_main_menu
        show_technical_main_menu()
    elif choice == 18:
        from fincept_terminal.genai_query.GenAI import show_genai_query
        show_genai_query()
    elif choice == 19:
        from fincept_terminal.finscript import show_finscript_menu
        show_finscript_menu()
    elif choice == 20:
        # Call the settings menu
        console.print("[bold green]Under Construction...[/bold green]")
        show_main_menu()
    elif choice == 21:
        show_settings_menu()  # Call this function instead of calling a Click command
    elif choice == 22:
        webbrowser.open("https://docs.fincept.in")
        console.print("[bold green]Redirecting to Fincept Documentation...[/bold green]")
        show_main_menu()
    elif choice == 23:
        from fincept_terminal.consumer_behaviour.consumer_behavior import show_consumer_behavior_menu
        show_consumer_behavior_menu()
    elif choice == 24:
        from fincept_terminal.analytics_reports.analytics import daily_report_menu
        daily_report_menu()
    elif choice == 25:
        from fincept_terminal.satellite_imagery_analysis.SatelliteImagery import show_car_analysis_menu
        show_car_analysis_menu()
    elif choice == 26:
        from fincept_terminal.marine_trade_data.marine import marine_menu
        marine_menu()
    elif choice == 27:
        from fincept_terminal.WorldEconomyData.EcoIndia.DataGovIN_Economy import show_india_macro_micro_menu
        show_india_macro_micro_menu()
    elif choice == 28:
        console.print("[bold green]Exiting Fincept Terminal...[/bold green]")
        sys.exit(0)
    elif choice == 29:
        from fincept_terminal.comparison_analysis.comparison_analysis import show_comparison_analysis
        show_comparison_analysis()
    else:
        console.print("[bold red]Invalid option. Please select a valid menu option.[/bold red]")
        show_main_menu()  # Show the menu again if an invalid option is chosen

def show_settings_menu():
    """Settings menu with options displayed in a table."""
    while True:  # Use a loop to keep returning to the settings menu after each operation
        console.print("\n[bold cyan]SETTINGS MENU[/bold cyan]\n")

        settings_options = [
            "Configure Sources",#1
            "Change Theme",  # 2
            "Configure Display Options",  # 3
            "Toggle Notifications",  # 4
            "Toggle Auto-Update",  # 5
            "Display Current Configurations",  # 6
            "Back to Main Menu",  # 7
        ]

        # Display settings options in columns
        display_in_columns("Settings Options", settings_options)

        console.print("\n")
        from rich.prompt import Prompt
        choice = Prompt.ask("Enter your choice")

        try:
            choice = int(choice)  # Convert input to an integer
        except ValueError:
            console.print("[bold red]Invalid input. Please enter a valid option number.[/bold red]")
            continue  # Show the settings menu again on invalid input

        console.print("\n")

        # Handle settings menu choices
        if choice == 1:
            from fincept_terminal.settings.settings import configure_sources
            configure_sources()
        elif choice == 2:
            from fincept_terminal.settings.settings import change_theme
            change_theme()
        elif choice == 3:
            from fincept_terminal.settings.settings import configure_display
            configure_display()
        elif choice == 4:
            from fincept_terminal.settings.settings import toggle_notifications
            toggle_notifications()
        elif choice == 5:
            from fincept_terminal.settings.settings import toggle_auto_update
            toggle_auto_update()
        elif choice == 6:
            display_current_configurations()
        elif choice == 7:
            show_main_menu()  # Go back to the main menu
            break
        else:
            console.print("[bold red]Invalid option. Please select a valid menu option.[/bold red]")


def display_current_configurations():
    """Fetch and display the current user configurations."""
    from fincept_terminal.settings.settings import load_settings

    # Load the current settings from the settings file
    settings = load_settings()

    # Create a table to display settings in a structured way
    from rich.table import Table
    table = Table(title="Current Configurations", title_justify="center", header_style="bold cyan")

    table.add_column("Setting", style="bold yellow", justify="left")
    table.add_column("Value", style="bold white", justify="left")

    # Add settings and their values to the table
    table.add_row("Theme", settings.get("theme", "Not set"))
    table.add_row("Display Rows", str(settings.get("display_rows", "Not set")))
    table.add_row("Notifications", "Enabled" if settings.get("notifications", False) else "Disabled")
    table.add_row("Auto-Update", "Enabled" if settings.get("auto_update", False) else "Disabled")

    # Display the table
    console.print(table)

if __name__ == '__main__':
    cli()
