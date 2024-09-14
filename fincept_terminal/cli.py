import click
import logging
logging.getLogger('numexpr').setLevel(logging.ERROR)
import warnings
warnings.filterwarnings("ignore")

@click.group(invoke_without_command=True)
@click.version_option(version="0.1.2", prog_name="Fincept Investments")
@click.pass_context
def cli(ctx):
    """Fincept Investments CLI - Your professional financial terminal."""
    if ctx.invoked_subcommand is None:
        start()

# Start Command
@cli.command()
def start():
    """Start the Fincept Investments terminal"""
    from fincept_terminal import update_checker
    update_checker.check_for_update()
    from fincept_terminal.display import display_art
    display_art()
    show_main_menu()


def show_main_menu():
    """Main menu with navigation options."""
    from fincept_terminal.themes import console
    console.print("\n")
    console.print("[bold cyan]MAIN MENU[/bold cyan]\n", style="info")

    main_menu_options = [
        "Global Indices", #1
        "World Economic Trends", #2
        "Global News & Sentiment", #3
        "Global Stocks (Equities)", #4
        "Global Funds", #5
        "Exchange Traded Funds", #6
        "BONDS & FIXED INCOME", #7
        "Options & Derivatives", #8
        "CryptoCurrency", #9
        "PORTFOLIO & INVESTMENT TOOLS", #10
        "Global Stock Scanner", #11
        "Currency Markets (Forex)", #12
        "COMMODITIES", #13
        "Money Market", #14
        "BACKTESTING STOCKS", #15
        "Advance Technicals",  # 16
        "GenAI Query", #17
        "Education & Resources", #18
        "SETTINGS & CUSTOMIZATION", #19
        "Terminal Documentation", #20
        "EXIT", #21
    ]

    # Display main menu in columns
    from fincept_terminal.const import display_in_columns
    display_in_columns("Select an Option", main_menu_options)

    console.print("\n")
    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")
    console.print("\n")

    if choice == "1":
        from fincept_terminal.indices import show_global_indices_menu
        show_global_indices_menu()
    elif choice == "2":
        console.print("[bold yellow] Feature still in development [/bold yellow]", style="warning")
    elif choice == "3":
        from fincept_terminal.newsfetch import show_news_and_sentiment_menu
        show_news_and_sentiment_menu()
    elif choice == "4":
        from fincept_terminal.menuList import show_equities_menu
        show_equities_menu() 
    elif choice == "5":
        from fincept_terminal.funds import show_global_funds_menu
        show_global_funds_menu()
    elif choice == "6":
        from fincept_terminal.etfdata import show_etf_market_menu
        show_etf_market_menu()
    elif choice == "7":
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
    elif choice == "8":
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
    elif choice == "9":
        from fincept_terminal.cryptodata import crypto_main_menu
        crypto_main_menu()
    elif choice == "10":
        from fincept_terminal.portfolio import show_portfolio_menu
        show_portfolio_menu()
    elif choice == "11":
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
    elif choice == "12":
        from fincept_terminal.currencyforex import show_currency_market_menu
        show_currency_market_menu()
    elif choice == "13":
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
    elif choice == "14":
        from fincept_terminal.moneymarket import show_money_market_menu
        show_money_market_menu()
    elif choice == "15":
        from fincept_terminal.portfolio import show_backtesting_menu
        show_backtesting_menu()
    elif choice == "16":
        from fincept_terminal.advancetechnicals import show_technical_main_menu
        show_technical_main_menu()
    elif choice == "17":
        from fincept_terminal.GenAI import show_genai_query
        show_genai_query()
    elif choice == "18":
        console.print("[bold red] Feature still in development [/bold red]", style="danger")
    elif choice == "19":
        console.print("[bold red]Feature still in development [/bold red]", style="danger")
    elif choice == "20":
        import webbrowser
        webbrowser.open("https://docs.fincept.in")
        console.print("[bold green]Redirecting to Fincept Documentation...[/bold green]")
        show_main_menu()
    elif choice == "21":
        print("[bold green]Exiting Fincept Terminal...[/bold green]")
        import sys
        sys.exit(0)


if __name__ == '__main__':
    cli()
