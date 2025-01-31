from fincept_terminal.utils.themes import console
from rich.prompt import Prompt
from rich.panel import Panel
import yfinance as yf
import pandas as pd
from fincept_terminal.portfolio_and_investement_tool.portfolio import portfolios, save_portfolios
from fincept_terminal.oldTerminal.cli import show_main_menu


def show_robo_advisor_menu():
    """Robo-Advisor: Create a custom portfolio based on user input."""
    console.print("[bold cyan]ROBO ADVISOR: Create Your Portfolio[/bold cyan]\n", style="info")

    # Fetch the list of available countries
    from fincept_terminal.data.data import continent_countries
    console.print("[bold cyan]Select a country to start:[/bold cyan]")
    countries = [country for sublist in continent_countries.values() for country in sublist]

    display_options_in_columns(countries, "Available Countries")
    country_choice = select_option_from_list(countries, "country")

    # Fetch the sectors in the chosen country
    from fincept_terminal.data.data_fetcher import fetch_sectors_by_country
    sectors = fetch_sectors_by_country(country_choice)
    if not sectors:
        console.print(f"[bold red]No sectors available for {country_choice}. Exiting...[/bold red]")
        return

    # Step 2: Select Sector and Industry
    console.print("[bold cyan]Do you want to create a portfolio in one sector or multiple sectors?[/bold cyan]")
    sector_choice_type = Prompt.ask("Enter your choice (single/multiple)", choices=["single", "multiple"])

    selected_sectors = []
    selected_industries = []

    if sector_choice_type == "single":
        display_options_in_columns(sectors, "Available Sectors")
        sector_choice = select_option_from_list(sectors, "sector")
        selected_sectors.append(sector_choice)

        # Fetch industries in the selected sector
        from fincept_terminal.data.data_fetcher import fetch_industries_by_sector
        industries = fetch_industries_by_sector(country_choice, sector_choice)

        # Allow multiple industry selection within the selected sector
        selected_industries = select_multiple_industries(industries, sector_choice)

    else:
        # Allow multi-sector selection
        console.print(
            "[bold cyan]Select industries across multiple sectors. Type 'next' to move to industry selection after picking sectors.[/bold cyan]")
        while True:
            display_options_in_columns(sectors, "Available Sectors")
            sector_choice = Prompt.ask("Enter sector number or 'next' to finish",
                                       choices=[str(i + 1) for i in range(len(sectors))] + ["next"])
            if sector_choice == "next":
                break
            else:
                selected_sectors.append(sectors[int(sector_choice) - 1])

        # Fetch industries for each selected sector
        for sector in selected_sectors:
            from fincept_terminal.data.data_fetcher import fetch_industries_by_sector
            industries = fetch_industries_by_sector(country_choice, sector)
            if industries:
                selected_industries += select_multiple_industries(industries, sector)

    # Step 3: Gather List of Stocks from Selected Industries
    console.print("[bold cyan]Gathering stocks from selected industries...[/bold cyan]")
    from fincept_terminal.data.data_fetcher import fetch_stocks_by_industry
    all_stocks = []

    for sector, industry in selected_industries:
        stock_data = fetch_stocks_by_industry(country_choice, sector, industry)
        if not stock_data.empty:
            all_stocks.append(stock_data)

    if not all_stocks:
        console.print("[bold red]No stocks available in the selected industries.[/bold red]")
        return

    # Combine all the stock DataFrames into one
    all_stocks_df = pd.concat(all_stocks, ignore_index=True)

    console.print(
        f"[bold green]Collected {len(all_stocks_df)} stocks from {len(selected_industries)} industries![/bold green]")

    # Step 4: Run Technical and Fundamental Analysis using yfinance
    console.print("[bold cyan]Running technical and fundamental analysis using yfinance...[/bold cyan]")
    selected_portfolio = run_technical_fundamental_analysis(all_stocks_df)

    # Display the selected portfolio
    console.print("[bold green]Here are the top stocks based on analysis:[/bold green]")
    display_search_results(selected_portfolio)

    # Step 5: Ask for GenAI Feedback
    genai_feedback = Prompt.ask("Do you want to get feedback on this portfolio from GenAI? (yes/no)",
                                choices=["yes", "no"])
    if genai_feedback == "yes":
        stock_list = selected_portfolio['symbol'].tolist()
        stock_query = " ".join(stock_list)
        from fincept_terminal.genai_query.GenAI import genai_query
        genai_recommendation = genai_query(
            f"Review this stock portfolio and suggest additions or removals: {stock_query}")
        console.print(Panel(genai_recommendation, title="GenAI Recommendations", style="cyan"))

    # Step 6: Save Portfolio Option
    finalize = Prompt.ask("Do you want to finalize and save this portfolio? (yes/no)", choices=["yes", "no"])

    if finalize == "yes":
        portfolio_name = Prompt.ask("Enter a name for this portfolio")
        # Add the selected portfolio to the portfolio system
        add_robo_advisor_portfolio(portfolio_name, selected_portfolio)
        # After saving, display a success message and redirect to the main menu
        console.print(f"[bold green]Portfolio '{portfolio_name}' saved successfully to your portfolios![/bold green]")
        show_main_menu()  # Redirect to main menu
    else:
        console.print("[bold red]Portfolio creation cancelled.[/bold red]")
        show_main_menu()  # Redirect to main menu


def add_robo_advisor_portfolio(portfolio_name, selected_portfolio):
    """Add the Robo-Advisor generated portfolio to the user's portfolio system."""
    # Create a list of yfinance.Ticker objects based on the selected portfolio
    portfolio_stocks = [yf.Ticker(symbol) for symbol in selected_portfolio['symbol'].tolist()]
    # Add the new portfolio to the portfolios dictionary
    portfolios[portfolio_name] = portfolio_stocks
    # Save the updated portfolios to the JSON file
    save_portfolios()


def select_multiple_industries(industries, sector_name):
    """Allow user to select multiple industries from a given sector."""
    console.print(
        f"[bold cyan]Select industries in the sector: {sector_name}. Type 'next' to finish industry selection.[/bold cyan]")
    selected_industries = []

    while True:
        display_options_in_columns(industries, f"Industries in {sector_name}")
        industry_choice = Prompt.ask("Enter industry number or 'next' to finish",
                                     choices=[str(i + 1) for i in range(len(industries))] + ["next"])

        if industry_choice == "next":
            break
        else:
            selected_industries.append((sector_name, industries[int(industry_choice) - 1]))

    return selected_industries


# Supporting functions

def run_technical_fundamental_analysis(stocks):
    """Perform fundamental and technical analysis using yfinance for the stock list and return top picks."""
    stock_analysis = []

    for symbol in stocks["symbol"]:
        try:
            # Fetch stock data using yfinance
            stock = yf.Ticker(symbol)
            info = stock.info

            # Fetch historical data for technical analysis
            hist = stock.history(period="6mo")  # Fetch 6 months of data
            if hist.empty:
                console.print(f"[bold red]Warning: No historical data for {symbol}. Skipping stock.")
                continue

            # Calculate 50-day SMA
            sma_50 = hist['Close'].rolling(window=50).mean().iloc[-1] if len(hist) >= 50 else None
            # Calculate 200-day SMA if enough data is available, or fallback to 100-day SMA
            sma_200 = hist['Close'].rolling(window=200).mean().iloc[-1] if len(hist) >= 200 else \
            hist['Close'].rolling(window=100).mean().iloc[-1] if len(hist) >= 100 else None
            current_price = hist['Close'].iloc[-1]

            # Calculate basic fundamentals (like P/E ratio and market cap)
            market_cap = info.get("marketCap", None)
            pe_ratio = info.get("trailingPE", None)

            # Ensure values are not None before comparison or use default values
            if market_cap is None:
                console.print(f"[bold red]Warning: No market cap data for {symbol}. Skipping stock.")
                continue

            if sma_50 is None:
                console.print(f"[bold red]Warning: No sufficient SMA 50 data for {symbol}. Skipping stock.")
                continue

            if sma_200 is None:
                console.print(
                    f"[bold yellow]Warning: No sufficient SMA 200 data for {symbol}. Using SMA 100 as fallback.")

            # Scoring logic
            fundamental_score = 10 if market_cap > 1e10 else (7 if market_cap > 1e9 else 5)
            technical_score = 10 if sma_50 and sma_200 and sma_50 > sma_200 else (7 if sma_50 else 5)

            # Add this stock's analysis to the list
            stock_analysis.append({
                "symbol": symbol,
                "name": info.get("longName", symbol),
                "market_cap": market_cap,
                "pe_ratio": pe_ratio,
                "sma_50": sma_50,
                "sma_200": sma_200,
                "current_price": current_price,
                "fundamental_score": fundamental_score,
                "technical_score": technical_score
            })
        except Exception as e:
            console.print(f"[bold red]Error fetching data for {symbol}: {e}[/bold red]")
            continue

    # Convert to DataFrame for further processing
    analyzed_df = pd.DataFrame(stock_analysis)

    # Calculate a combined score and select top 10 stocks
    analyzed_df["combined_score"] = analyzed_df["fundamental_score"] + analyzed_df["technical_score"]
    selected_stocks = analyzed_df.sort_values("combined_score", ascending=False).head(10)

    return selected_stocks


def display_search_results(results):
    """Display the stock data in a formatted table using rich."""
    from rich.table import Table
    table = Table(title="Top Stocks", header_style="bold green on black")

    table.add_column("Symbol", style="cyan", width=10)
    table.add_column("Name", style="green", width=30)
    table.add_column("Market Cap", style="yellow", justify="right")
    table.add_column("P/E Ratio", style="magenta", justify="right")
    table.add_column("SMA 50", style="blue", justify="right")
    table.add_column("SMA 200", style="blue", justify="right")
    table.add_column("Current Price", style="blue", justify="right")
    table.add_column("Fundamental Score (out of 10)", style="magenta", justify="right")
    table.add_column("Technical Score (out of 10)", style="magenta", justify="right")

    for _, row in results.iterrows():
        table.add_row(
            str(row["symbol"]),
            str(row["name"]),
            f"{row['market_cap']:,}",  # Removed dollar signs for generic currency
            f"{row['pe_ratio']:.2f}" if row['pe_ratio'] else "N/A",
            f"{row['sma_50']:.2f}" if row['sma_50'] else "N/A",
            f"{row['sma_200']:.2f}" if row['sma_200'] else "N/A",
            f"{row['current_price']:.2f}",
            str(row["fundamental_score"]),
            str(row["technical_score"]),
        )

    console.print(table)


def select_option_from_list(options, option_type):
    """Utility function to select an option from a list."""
    while True:
        try:
            choice = int(Prompt.ask(f"Please select a {option_type} by number"))
            if 1 <= choice <= len(options):
                return options[choice - 1]
            else:
                console.print(f"[danger]Invalid {option_type} number. Please try again.[/danger]")
        except ValueError:
            console.print(f"[danger]Invalid input. Please enter a number corresponding to the {option_type}.[/danger]")


def display_options_in_columns(options, title):
    """Utility function to display options in columns."""
    from rich.table import Table
    num_columns = 4
    table = Table(title=title, header_style="bold green on black")

    for _ in range(num_columns):
        table.add_column("", style="bold cyan")

    rows = []
    for i, option in enumerate(options, start=1):
        if i % num_columns == 1:
            rows.append([f"{i}. {option}"])
        else:
            rows[-1].append(f"{i}. {option}")

    for row in rows:
        while len(row) < num_columns:
            row.append("")

    for row in rows:
        table.add_row(*row)

    console.print(table)
