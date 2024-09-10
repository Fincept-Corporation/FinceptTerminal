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
        click.echo(ctx.get_help())  # Display help if no command is given

# Start Command
@cli.command()
def start():
    """Start the Fincept Investments terminal"""
    from fincept_terminal.display import display_art
    display_art()
    show_main_menu()


def show_main_menu():
    """Main menu with navigation options."""
    from fincept_terminal.themes import console
    console.print("\n")
    console.print("[bold cyan]MAIN MENU[/bold cyan]\n", style="info")

    main_menu_options = [
        "MARKET TRACKER", #1
        "ECONOMICS & MACRO TRENDS", #2
        "Global News & Sentiment", #3
        "STOCKS (Equities)", #4
        "FUNDS & ETFs", #5
        "BONDS & FIXED INCOME", #6
        "OPTIONS & DERIVATIVES", #7
        "CryptoCurrency", #8
        "PORTFOLIO & INVESTMENT TOOLS", #9
        "CURRENCY MARKETS (Forex)", #10
        "COMMODITIES", #11
        "BACKTESTING STOCKS", #12
        "GenAI Query", #13
        "EDUCATION & RESOURCES", #14
        "SETTINGS & CUSTOMIZATION", #15
        "Terminal Documentation", #16
        "EXIT", #17
    ]

    # Display main menu in columns
    from fincept_terminal.const import display_in_columns
    display_in_columns("Select an Option", main_menu_options)

    console.print("\n")
    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")
    console.print("\n")

    if choice == "1":
        from fincept_terminal.menuList import show_market_tracker_menu
        show_market_tracker_menu()  # Market Tracker submenu
    elif choice == "2":
        console.print("[bold yellow]Economics section under development[/bold yellow]", style="warning")
    elif choice == "3":
        show_news_and_sentiment_menu()
    elif choice == "4":
        from fincept_terminal.menuList import show_equities_menu
        show_equities_menu()  # Equities submenu for continent and country selection
    elif choice == "5":
        console.print("[bold yellow]Funds section under development[/bold yellow]", style="warning")
    elif choice == "6":
        console.print("[bold yellow]Bonds section under development[/bold yellow]", style="warning")
    elif choice == "7":
        console.print("[bold red]Exiting the Fincept terminal...[/bold red]", style="danger")
    elif choice == "8":
        crypto_main_menu()
    elif choice == "9":
        from fincept_terminal.portfolio import show_portfolio_menu
        show_portfolio_menu()
    elif choice == "12":
        from fincept_terminal.portfolio import show_backtesting_menu
        show_backtesting_menu()
    elif choice == "13":  # GenAI Query option
        from fincept_terminal.GenAI import show_genai_query
        show_genai_query()


import warnings
from gnews import GNews

# Suppress warnings
warnings.filterwarnings("ignore")

# Initialize GNews with default settings
google_news = GNews(language='en', max_results=25)

# Country code mapping based on GNews documentation
COUNTRY_CODES = {
    'Australia': 'AU', 'Botswana': 'BW', 'Canada': 'CA', 'Ethiopia': 'ET', 'Ghana': 'GH', 'India': 'IN', 'Indonesia': 'ID',
    'Ireland': 'IE', 'Israel': 'IL', 'Kenya': 'KE', 'Latvia': 'LV', 'Malaysia': 'MY', 'Namibia': 'NA', 'New Zealand': 'NZ',
    'Nigeria': 'NG', 'Pakistan': 'PK', 'Philippines': 'PH', 'Singapore': 'SG', 'South Africa': 'ZA', 'Tanzania': 'TZ',
    'Uganda': 'UG', 'United Kingdom': 'GB', 'United States': 'US', 'Zimbabwe': 'ZW', 'Czech Republic': 'CZ',
    'Germany': 'DE', 'Austria': 'AT', 'Switzerland': 'CH', 'Argentina': 'AR', 'Chile': 'CL', 'Colombia': 'CO',
    'Cuba': 'CU', 'Mexico': 'MX', 'Peru': 'PE', 'Venezuela': 'VE', 'Belgium': 'BE', 'France': 'FR', 'Morocco': 'MA',
    'Senegal': 'SN', 'Italy': 'IT', 'Lithuania': 'LT', 'Hungary': 'HU', 'Netherlands': 'NL', 'Norway': 'NO',
    'Poland': 'PL', 'Brazil': 'BR', 'Portugal': 'PT', 'Romania': 'RO', 'Slovakia': 'SK', 'Slovenia': 'SI',
    'Sweden': 'SE', 'Vietnam': 'VN', 'Turkey': 'TR', 'Greece': 'GR', 'Bulgaria': 'BG', 'Russia': 'RU', 'Ukraine': 'UA',
    'Serbia': 'RS', 'United Arab Emirates': 'AE', 'Saudi Arabia': 'SA', 'Lebanon': 'LB', 'Egypt': 'EG',
    'Bangladesh': 'BD', 'Thailand': 'TH', 'China': 'CN', 'Taiwan': 'TW', 'Hong Kong': 'HK', 'Japan': 'JP',
    'Republic of Korea': 'KR'
}


def set_gnews_country(country):
    """Set the GNews country dynamically based on user selection."""
    if country == "WORLD":
        google_news.country = None  # No specific country, fetch global news
    else:
        google_news.country = COUNTRY_CODES.get(country, None)


def show_news_and_sentiment_menu():
    """Display Global News and Sentiment analysis by continent and country."""
    from fincept_terminal.themes import console
    from fincept_terminal.data import get_countries_by_continent
    from fincept_terminal.const import display_in_columns
    from rich.prompt import Prompt

    while True:  # Loop to keep asking the user for news until they choose to exit
        # Step 1: Prompt user to select a continent
        console.print("[bold cyan]GLOBAL NEWS AND SENTIMENT[/bold cyan]\n", style="info")

        continents = ["Asia", "Europe", "Africa", "North America", "South America", "Oceania", "Middle East", "Main Menu"]
        display_in_columns("Select a Continent", continents)
        console.print("\n")
        continent_choice = Prompt.ask("Enter your continent choice (Press enter to default to WORLD)")

        if not continent_choice or continent_choice.lower() == 'main menu':
            selected_continent = "WORLD"  # Default to global news
        else:
            try:
                selected_continent = continents[int(continent_choice) - 1]
            except (ValueError, IndexError):
                selected_continent = "WORLD"  # Default to global news if input is invalid

        # Step 2: Show available countries for the selected continent
        if selected_continent == "WORLD":
            selected_country = "WORLD"
            set_gnews_country(selected_country)
            fetch_and_display_global_news()  # Fetch global news directly without category
        else:
            countries = get_countries_by_continent(selected_continent)
            if not countries:
                console.print(f"[bold red]No countries available in {selected_continent}[/bold red]")
                continue  # Restart the loop to select a new continent

            display_in_columns(f"Select a Country in {selected_continent} (or press Enter for WORLD)", countries)
            country_choice = Prompt.ask("Enter your country choice or press Enter for WORLD")
            if not country_choice:
                selected_country = "WORLD"
                fetch_and_display_global_news()  # Fetch global news directly without category
            else:
                try:
                    selected_country = countries[int(country_choice) - 1]
                    set_gnews_country(selected_country)
                    # Step 3: Prompt for news category (NATION, BUSINESS, TECHNOLOGY)
                    topics = ["NATION", "BUSINESS", "TECHNOLOGY"]
                    display_in_columns("Select a Topic", topics)
                    topic_choice = Prompt.ask("Enter your topic choice")
                    try:
                        selected_topic = topics[int(topic_choice) - 1]
                    except (ValueError, IndexError):
                        console.print("[bold red]Invalid topic choice. Defaulting to NATION.[/bold red]")
                        selected_topic = "NATION"  # Default to NATION if the user input is invalid

                    # Fetch and display news based on country and topic
                    fetch_and_display_news(selected_country, selected_topic)
                except (ValueError, IndexError):
                    selected_country = "WORLD"
                    fetch_and_display_global_news()  # Fetch global news directly without category

        # Ask user if they want to fetch more news
        another_news = Prompt.ask("\nWould you like to fetch more news? (yes/no)").lower()
        if another_news == "no":
            break


def fetch_and_display_global_news():
    """Fetch and display global news directly without category."""
    from fincept_terminal.themes import console

    # Fetch global news
    console.print(f"[bold cyan]Fetching Global News...[/bold cyan]")
    articles = google_news.get_top_news()

    if not articles:
        console.print(f"[bold red]No global news found.[/bold red]")
        return

    # Display articles and perform sentiment analysis
    from rich.table import Table
    table = Table(title="Global News", title_justify="center", header_style="bold green on #282828")
    table.add_column("Headline", style="cyan", justify="left", no_wrap=True)
    table.add_column("Sentiment", style="magenta", justify="left")

    positive_count = 0
    neutral_count = 0
    negative_count = 0

    # Loop over the articles, perform sentiment analysis, and display the news
    for article in articles:
        headline = article['title']

        # Perform sentiment analysis on the headline
        from textblob import TextBlob
        sentiment_score = TextBlob(headline).sentiment.polarity
        if sentiment_score > 0:
            sentiment = "Positive"
            positive_count += 1
        elif sentiment_score < 0:
            sentiment = "Negative"
            negative_count += 1
        else:
            sentiment = "Neutral"
            neutral_count += 1

        table.add_row(headline, sentiment)

    console.print(table)

    # Step 4: Display sentiment analysis as ASCII bars in one line
    display_sentiment_bar(positive_count, neutral_count, negative_count)


def fetch_and_display_news(country, topic):
    from fincept_terminal.themes import console
    """Fetch and display news articles along with sentiment analysis for a selected country and topic."""

    # Fetch news articles based on the topic
    console.print(f"[bold cyan]Fetching {topic} news for {country}...[/bold cyan]")
    articles = google_news.get_news_by_topic(topic.lower())

    if not articles:
        console.print(f"[bold red]No news articles found for {topic} in {country}[/bold red]")
        return

    # Display articles and perform sentiment analysis
    from rich.table import Table
    table = Table(title=f"{topic} News for {country}", title_justify="center", header_style="bold green on #282828")
    table.add_column("Headline", style="cyan", justify="left", no_wrap=True)
    table.add_column("Sentiment", style="magenta", justify="left")

    positive_count = 0
    neutral_count = 0
    negative_count = 0

    # Loop over the articles, perform sentiment analysis, and display the news
    for article in articles:
        headline = article['title']

        # Perform sentiment analysis on the headline
        from textblob import TextBlob
        sentiment_score = TextBlob(headline).sentiment.polarity
        if sentiment_score > 0:
            sentiment = "Positive"
            positive_count += 1
        elif sentiment_score < 0:
            sentiment = "Negative"
            negative_count += 1
        else:
            sentiment = "Neutral"
            neutral_count += 1

        table.add_row(headline, sentiment)

    console.print(table)

    # Step 4: Display sentiment analysis as ASCII bars in one line
    display_sentiment_bar(positive_count, neutral_count, negative_count)


def display_sentiment_bar(positive, neutral, negative):
    """Display sentiment analysis as ASCII bar chart in one line."""
    from fincept_terminal.themes import console

    # Calculate the total number of articles
    total = positive + neutral + negative
    if total == 0:
        console.print("[bold red]No sentiment data available.[/bold red]")
        return

    # Calculate the percentage for each sentiment
    positive_percent = (positive / total) * 100
    neutral_percent = (neutral / total) * 100
    negative_percent = (negative / total) * 100

    # Display sentiment as ASCII bar
    bar_length = 40  # Length of the bar
    positive_bar = '█' * int((positive_percent / 100) * bar_length)
    neutral_bar = '█' * int((neutral_percent / 100) * bar_length)
    negative_bar = '█' * int((negative_percent / 100) * bar_length)

    # Create labels for each sentiment category
    positive_label = f"Positive: {positive} ({positive_percent:.2f}%)"
    neutral_label = f"Neutral: {neutral} ({neutral_percent:.2f}%)"
    negative_label = f"Negative: {negative} ({negative_percent:.2f}%)"

    # Display the bars in a single line
    console.print(f"\n[bold green]{positive_label}[/bold green] {positive_bar}  [bold yellow]{neutral_label}[/bold yellow] {neutral_bar}  [bold red]{negative_label}[/bold red] {negative_bar}")

import requests
from rich.prompt import Prompt
from rich.progress import Progress
import yfinance as yf
from fincept_terminal.themes import console  # Using existing console

# API Endpoints
CURRENCY_API = "https://fincept.share.zrok.io/FinanceDB/cryptos/currency/data?unique=true"
CRYPTO_BY_CURRENCY_API = "https://fincept.share.zrok.io/FinanceDB/cryptos/currency/filter?value={}"

# Constants
ROWS_PER_COLUMN = 7
COLUMNS_PER_PAGE = 9
ITEMS_PER_PAGE = ROWS_PER_COLUMN * COLUMNS_PER_PAGE  # 7 rows per column * 9 columns per page = 63 items per page

# Fetch available currencies from the API
def fetch_available_currencies():
    with Progress() as progress:
        task = progress.add_task("[cyan]Fetching currencies...", total=100)
        response = requests.get(CURRENCY_API)
        data = response.json()
        progress.update(task, advance=100)
        return data['currency']

# Fetch all cryptocurrencies by currency
def fetch_cryptos_by_currency(currency):
    url = CRYPTO_BY_CURRENCY_API.format(currency)
    with Progress() as progress:
        task = progress.add_task(f"[cyan]Fetching cryptocurrencies for {currency}...", total=100)
        response = requests.get(url)
        progress.update(task, advance=100)
    return response.json()

# Display information in three columns, skipping long values
def display_info_in_three_columns(info):
    """Display key-value pairs in three columns, skipping long values."""
    from rich.table import Table
    table = Table(show_lines=True, style="info", header_style="bold white on #282828")

    # Add columns for three attributes and values
    table.add_column("Attribute 1", style="cyan on #282828", width=25)
    table.add_column("Value 1", style="green on #282828", width=35)
    table.add_column("Attribute 2", style="cyan on #282828", width=25)
    table.add_column("Value 2", style="green on #282828", width=35)
    table.add_column("Attribute 3", style="cyan on #282828", width=25)
    table.add_column("Value 3", style="green on #282828", width=35)

    max_value_length = 40  # Set a maximum length for displayed values
    keys = list(info.keys())
    values = list(info.values())

    for i in range(0, len(keys), 3):
        row_data = []
        for j in range(3):
            if i + j < len(keys):
                key = keys[i + j]
                value = values[i + j]
                # Skip long values and add a placeholder
                if isinstance(value, str) and len(value) > max_value_length:
                    row_data.append(str(key))
                    row_data.append("[value too long]")
                else:
                    row_data.append(str(key))
                    row_data.append(str(value))
            else:
                row_data.append("")
                row_data.append("")
        table.add_row(*row_data)

    console.print(table)

# Display the items in a paginated table with 7 rows and up to 9 columns per page, allowing index selection
def display_paginated_columns(title, items, current_page=1, allow_selection=False):
    """Display items in paginated table format (7 rows per column, up to 9 columns per page), with optional index selection."""
    from rich.table import Table

    start_index = (current_page - 1) * ITEMS_PER_PAGE
    end_index = start_index + ITEMS_PER_PAGE

    # Get the items for the current page
    page_items = items[start_index:end_index]
    total_pages = (len(items) + ITEMS_PER_PAGE - 1) // ITEMS_PER_PAGE

    # Calculate the number of columns to display dynamically
    num_columns = (len(page_items) + ROWS_PER_COLUMN - 1) // ROWS_PER_COLUMN

    table = Table(title=f"{title} (Page {current_page}/{total_pages})", header_style="bold green on #282828", show_lines=True)

    # Add only the necessary columns based on data length
    for _ in range(min(num_columns, COLUMNS_PER_PAGE)):
        table.add_column("", style="highlight", justify="left")

    # Add rows in columns (7 rows per column)
    rows = [[] for _ in range(ROWS_PER_COLUMN)]
    for index, item in enumerate(page_items):
        row_index = index % ROWS_PER_COLUMN
        rows[row_index].append(f"{start_index + index + 1}. {item}")

    # Fill the table
    for row in rows:
        row += [""] * (num_columns - len(row))  # Only add empty strings for missing columns
        table.add_row(*row)

    console.print(table)

    # Check if more pages are available
    if allow_selection:
        return Prompt.ask("\nSelect an index to view details, or 'n' for next page, 'p' for previous, or 'q' to quit")

    if current_page < total_pages:
        next_page = Prompt.ask(f"Page {current_page} of {total_pages}. Enter 'n' for next page, 'p' for previous, or 'q' to quit", default="n")
        if next_page == "n":
            display_paginated_columns(title, items, current_page + 1)
        elif next_page == "p" and current_page > 1:
            display_paginated_columns(title, items, current_page - 1)
        elif next_page != "q":
            console.print("[bold red]Invalid input. Returning to the main menu.[/bold red]")

# Main menu for Crypto operations
def crypto_main_menu():
    while True:
        console.print("\n[bold cyan]CRYPTO MENU[/bold cyan]\n")
        menu_items = ["1. Search Cryptocurrency by Symbol (Using yfinance)",
                      "2. View All Cryptocurrencies (Select Currency First)",
                      "3. Exit"]
        display_paginated_columns("CRYPTO MENU", menu_items)

        choice = Prompt.ask("\nEnter your choice")

        if choice == "1":
            symbol = Prompt.ask("Enter cryptocurrency symbol (e.g., BTC, ETH)").upper()
            fetch_and_display_yfinance_data(symbol)  # Fetch and display yfinance data
        elif choice == "2":
            view_all_cryptocurrencies()  # View all cryptocurrencies
        elif choice == "3":
            console.print("[bold green]Exiting Crypto Menu...[/bold green]")
            break
        else:
            console.print("[bold red]Invalid choice. Please try again.[/bold red]")

# Crypto Menu - Option 2: View All Cryptocurrencies
def view_all_cryptocurrencies():
    # Fetch and display available currencies
    currencies = fetch_available_currencies()
    console.print(f"\n[bold cyan]Total Currencies Available: {len(currencies)}[/bold cyan]")
    display_paginated_columns("Available Currencies", currencies)

    # Ask the user to select a currency
    selected_currency_index = Prompt.ask("Select a currency by entering the index")
    try:
        selected_currency = currencies[int(selected_currency_index) - 1]
    except (ValueError, IndexError):
        console.print("[bold red]Invalid currency selection! Returning to main menu.[/bold red]")
        return

    console.print(f"\n[bold green]Fetching cryptocurrencies for {selected_currency}...[/bold green]")

    # Fetch and display cryptocurrencies for the selected currency
    cryptos = fetch_cryptos_by_currency(selected_currency)
    if not cryptos:
        console.print(f"[bold red]No cryptocurrencies found for {selected_currency}.[/bold red]")
        return

    # Display the available cryptocurrencies with pagination and allow index selection
    selected_crypto_index = display_paginated_columns(f"Cryptocurrencies for {selected_currency}", [crypto['symbol'] for crypto in cryptos], allow_selection=True)

    if selected_crypto_index.isdigit():
        try:
            selected_crypto = cryptos[int(selected_crypto_index) - 1]
        except (ValueError, IndexError):
            console.print("[bold red]Invalid cryptocurrency selection! Returning to main menu.[/bold red]")
            return

        # Display the selected cryptocurrency details
        fetch_and_display_yfinance_data(selected_crypto['symbol'])
        prompt_for_another_query()
    elif selected_crypto_index.lower() not in ['n', 'p', 'q']:
        console.print("[bold red]Invalid input. Returning to the main menu.[/bold red]")

# Prompt the user if they want to query another cryptocurrency
def prompt_for_another_query():
    choice = Prompt.ask("\nWould you like to query another cryptocurrency? (yes/no)", default="no").lower()
    if choice == "yes":
        show_main_menu()
    else:
        console.print("[bold green]Returning to main menu...[/bold green]")

# Fetch and display yfinance data for the given symbol
def fetch_and_display_yfinance_data(symbol):
    ticker = yf.Ticker(symbol)
    data = ticker.info  # Get the JSON data

    # Define the keys to exclude
    excluded_keys = {'uuid', 'messageBoardId', 'maxAge'}

    # Check if description or summary exists and remove it from the table
    description = data.pop('description', None)
    summary = data.pop('summary', None)

    # Filter the data
    filtered_data = {k: v for k, v in data.items() if k not in excluded_keys}

    # Display the data in three columns
    display_info_in_three_columns(filtered_data)

    # Display the summary/description separately
    if description:
        console.print(f"\n[bold cyan]Description[/bold cyan]: {description}")
    if summary:
        console.print(f"\n[bold cyan]Summary[/bold cyan]: {summary}")


if __name__ == '__main__':
    cli()
