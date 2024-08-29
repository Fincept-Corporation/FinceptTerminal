import click
import requests
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from gnews import GNews
from time import sleep
import threading

from .art import display_art

console = Console()
gnews = GNews(language='en', max_results=15)

@click.group()
def cli():
    """Fincept Investments CLI"""
    pass

@cli.command()
def start():
    """Start the Fincept Investments terminal"""
    display_art()
    show_main_menu()

def show_main_menu():
    while True:
        console.print("[bold cyan]MAIN MENU[/bold cyan]\n", justify="left")

        # Menu options with colors and padding
        menu_text = """
[bold]1. MARKET TRACKER[/bold]
[bold]2. ECONOMICS[/bold]
[bold]3. NEWS[/bold]
[bold]4. EXIT[/bold]
        """

        # Create a panel for the menu without centering
        menu_panel = Panel(menu_text, title="[bold]SELECT AN OPTION[/bold]", title_align="center", style="cyan")

        console.print(menu_panel, justify="left")
        
        # Prompt for user choice
        choice = click.prompt("Please select an option", type=int)

        if choice == 1:
            show_market_tracker_menu()
        elif choice == 2:
            console.print("\n[bold green]ECONOMICS SELECTED.[/bold green] (This feature is under development)\n", justify="left")
        elif choice == 3:
            show_news_menu()
        elif choice == 4:
            console.print("\n[bold red]EXITING FINCEPT INVESTMENTS.[/bold red]", justify="left")
            break
        else:
            console.print("\n[bold red]INVALID OPTION. PLEASE TRY AGAIN.[/bold red]", justify="left")

def show_market_tracker_menu():
    while True:
        console.print("[bold cyan]MARKET TRACKER[/bold cyan]\n", justify="left")

        # Market Tracker options
        tracker_text = """
[bold]1. FII/DII DATA INDIA[/bold]
[bold]2. NIFTY 50 LIST[/bold]
[bold]3. BACK TO MAIN MENU[/bold]
        """

        tracker_panel = Panel(tracker_text, title="[bold]SELECT AN OPTION[/bold]", title_align="center", style="cyan")
        console.print(tracker_panel, justify="left")

        # Prompt for user choice
        choice = click.prompt("Please select an option", type=int)

        if choice == 1:
            display_fii_dii_data()
        elif choice == 2:
            console.print("\n[bold yellow]NIFTY 50 LIST SELECTED.[/bold yellow] (This feature is under development)\n", justify="left")
        elif choice == 3:
            break
        else:
            console.print("\n[bold red]INVALID OPTION. PLEASE TRY AGAIN.[/bold red]", justify="left")

def display_fii_dii_data():
    # API endpoint for FII/DII data
    fii_dii_url = "https://fincept.share.zrok.io/IndiaStockExchange/fii_dii_data/data"
    
    try:
        # Fetch data from the API
        response = requests.get(fii_dii_url)
        response.raise_for_status()
        fii_dii_data = response.json()

        # Display data in a colorful table
        table = Table(title="FII/DII DATA - INDIA (values in CR)", title_justify="left", header_style="bold magenta")

        table.add_column("DATE", style="cyan", no_wrap=True)
        table.add_column("FII BUY VALUE", style="yellow")
        table.add_column("FII BUY PCT CHANGE", style="blue")
        table.add_column("FII SELL VALUE", style="yellow")
        table.add_column("FII SELL PCT CHANGE", style="blue")
        table.add_column("FII NET VALUE", style="green")
        table.add_column("FII NET PCT CHANGE", style="blue")
        table.add_column("DII BUY VALUE", style="yellow")
        table.add_column("DII BUY PCT CHANGE", style="blue")
        table.add_column("DII SELL VALUE", style="yellow")
        table.add_column("DII SELL PCT CHANGE", style="blue")
        table.add_column("DII NET VALUE", style="green")
        table.add_column("DII NET PCT CHANGE", style="blue")

        if len(fii_dii_data) < 2:
            console.print(f"[bold red]Not enough data to perform percentage change calculations.[/bold red]", justify="left")
            return

        second_entry = fii_dii_data[1]
        previous_entry = None

        for index, entry in enumerate(fii_dii_data):
            # Initialize percentage change strings
            fii_buy_pct_change = fii_sell_pct_change = fii_net_pct_change = "0.00%"
            dii_buy_pct_change = dii_sell_pct_change = dii_net_pct_change = "0.00%"

            # Calculate percentage changes using the second entry if previous_entry is not available
            if previous_entry:
                compare_entry = previous_entry
            else:
                compare_entry = second_entry  # Use the second available data if no previous entry

            # Perform percentage change calculation
            if compare_entry:
                fii_buy_pct_change = ((entry['fii_buy_value'] - compare_entry['fii_buy_value']) / abs(compare_entry['fii_buy_value'])) * 100
                fii_sell_pct_change = ((entry['fii_sell_value'] - compare_entry['fii_sell_value']) / abs(compare_entry['fii_sell_value'])) * 100
                fii_net_pct_change = ((entry['fii_net_value'] - compare_entry['fii_net_value']) / abs(compare_entry['fii_net_value'])) * 100
                dii_buy_pct_change = ((entry['dii_buy_value'] - compare_entry['dii_buy_value']) / abs(compare_entry['dii_buy_value'])) * 100
                dii_sell_pct_change = ((entry['dii_sell_value'] - compare_entry['dii_sell_value']) / abs(compare_entry['dii_sell_value'])) * 100
                dii_net_pct_change = ((entry['dii_net_value'] - compare_entry['dii_net_value']) / abs(compare_entry['dii_net_value'])) * 100

                # Format the percentage change strings
                fii_buy_pct_change = f"{fii_buy_pct_change:+.2f}%"
                fii_sell_pct_change = f"{fii_sell_pct_change:+.2f}%"
                fii_net_pct_change = f"{fii_net_pct_change:+.2f}%"
                dii_buy_pct_change = f"{dii_buy_pct_change:+.2f}%"
                dii_sell_pct_change = f"{dii_sell_pct_change:+.2f}%"
                dii_net_pct_change = f"{dii_net_pct_change:+.2f}%"

            # Add row to the table
            table.add_row(
                entry["date"],
                f"{entry['fii_buy_value']:,}", fii_buy_pct_change,
                f"{entry['fii_sell_value']:,}", fii_sell_pct_change,
                f"{entry['fii_net_value']:,}", fii_net_pct_change,
                f"{entry['dii_buy_value']:,}", dii_buy_pct_change,
                f"{entry['dii_sell_value']:,}", dii_sell_pct_change,
                f"{entry['dii_net_value']:,}", dii_net_pct_change
            )

            # Update previous_entry for next iteration
            previous_entry = entry

        console.print(table, justify="left")

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching FII/DII data: {e}[/bold red]", justify="left")

def show_news_menu():
    countries = [
        "United States", "India", "Canada", "United Kingdom", 
        "Australia", "Germany", "France", "Japan", 
        "South Korea", "Brazil"
    ]
    
    console.print("[bold cyan]NEWS[/bold cyan]\n", justify="left")
    console.print("[bold]SELECT A COUNTRY[/bold]\n", justify="left")

    for i, country in enumerate(countries, start=1):
        console.print(f"[bold]{i}. {country}[/bold]", justify="left")
    
    choice = click.prompt("Please select a country", type=int)
    
    if 1 <= choice <= len(countries):
        selected_country = countries[choice - 1]
        display_news_headlines(selected_country)
    else:
        console.print("\n[bold red]INVALID OPTION. PLEASE TRY AGAIN.[/bold red]", justify="left")

def display_news_headlines(country):
    console.print(f"\n[bold cyan]TOP NEWS HEADLINES IN {country.upper()}[/bold cyan]\n", justify="left")
    
    def loading_indicator():
        total_length = 50  # Total length of the loading bar
        for i in range(1, 101):
            filled_length = int(total_length * i // 100)
            bar = "$" * filled_length + "-" * (total_length - filled_length)
            console.print(f"\r[bold cyan]Fetching news... [{i}%] |{bar}|[/bold cyan]", end="", justify="left")
            sleep(0.05)
        console.print("\n")

    # Start the loading indicator
    loading_thread = threading.Thread(target=loading_indicator)
    loading_thread.start()

    # Fetch news articles
    try:
        news_results = gnews.get_news(country)
        loading_thread.join()

        table = Table(title=f"Top News Headlines in {country}", title_justify="left", header_style="bold magenta")
        
        table.add_column("Title", style="yellow")
        table.add_column("Published Date", style="cyan")
        table.add_column("Source", style="green")
        
        for article in news_results:
            table.add_row(
                article['title'], 
                article['published date'], 
                article.get('publisher', {}).get('title', 'Unknown')  # Fetch source correctly
            )
        
        console.print(table, justify="left")
        display_live_news_ticker(news_results)

    except Exception as e:
        loading_thread.join()
        console.print(f"[bold red]Error fetching news for {country}: {e}[/bold red]", justify="left")

def display_live_news_ticker(news_results):
    console.print("\n[bold cyan]LIVE NEWS TICKER[/bold cyan]\n", justify="left")
    
    try:
        while True:
            for article in news_results:
                console.print(f"[bold yellow]{article['title']}[/bold yellow] - [bold green]{article.get('publisher', {}).get('title', 'Unknown')}[/bold green]", justify="left")
                sleep(3)
    except KeyboardInterrupt:
        console.print("\n[bold red]Exiting live news ticker...[/bold red]", justify="left")

if __name__ == "__main__":
    cli()
