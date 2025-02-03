import pandas as pd
import os
import json
import matplotlib.pyplot as plt
from rich.console import Console
from rich.prompt import Prompt
from rich.table import Table
import yfinance as yf
from fincept_terminal.FinceptTerminalUtilsModule.themes import console
from fincept_terminal.FinceptTerminalUtilsModule.const import display_in_columns
from empyrical import (
    cum_returns,
    annual_volatility,
    sharpe_ratio,
    max_drawdown,
    calmar_ratio
)


SETTINGS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "FinceptTerminalSettingModule", "FinceptTerminalSettingModule.json")
console = Console()

def load_portfolios_from_settings():
    """Load portfolios from the FinceptTerminalSettingModule.json file."""
    if not os.path.exists(SETTINGS_FILE):
        console.print("[bold red]Settings file not found. Please configure your portfolios first.[/bold red]")
        return {}

    try:
        with open(SETTINGS_FILE, "r") as file:
            settings = json.load(file)

            # Extract portfolios in the specified format
            portfolios = settings.get("portfolios", {})
            formatted_portfolios = {
                name: {
                    "country": "Unknown",  # Default country since it's not provided in this format
                    "stocks": stocks
                }
                for name, stocks in portfolios.items()
            }

            return formatted_portfolios

    except (json.JSONDecodeError, ValueError):
        console.print("[bold red]Error: The FinceptTerminalSettingModule file is corrupted or invalid.[/bold red]")
        return {}

def show_comparison_analysis():
    """Display the main menu for Comparison Analysis."""
    while True:  # Loop to keep returning to the menu until the user exits
        console.print("[bold cyan]COMPARISON ANALYSIS[/bold cyan]\n")
        comparison_options = [
            "Portfolio Comparison",
            "Index Comparison",
            "Stock Performance",
            "Economic Indicators",
            "Back to Main Menu"
        ]

        display_in_columns("Select a Comparison Type", comparison_options)

        choice = Prompt.ask("Enter your choice", choices=[str(i) for i in range(1, len(comparison_options) + 1)])

        if choice == "1":
            portfolio_comparison()
        elif choice == "2":
            index_comparison()
        elif choice == "3":
            stock_performance_comparison()
        elif choice == "4":
            economic_indicator_comparison()
        elif choice == "5":
            # Exit to the main menu
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            return  # Exit this menu

# 1. Portfolio Comparison
def portfolio_comparison():
    """Compare performance metrics of multiple portfolios."""
    console.print("[bold cyan]PORTFOLIO COMPARISON[/bold cyan]")

    # Load portfolios from FinceptTerminalSettingModule.json
    portfolios = load_portfolios_from_settings()
    if not portfolios:
        console.print("[bold red]No portfolios available for comparison. Please create portfolios first.[/bold red]")
        return

    portfolio_names = list(portfolios.keys())
    display_in_columns("Available Portfolios", portfolio_names)

    selected_portfolios = Prompt.ask(
        "Enter portfolio names separated by commas for comparison"
    ).split(",")

    # Filter valid portfolio names
    selected_portfolios = [name.strip() for name in selected_portfolios if name.strip() in portfolio_names]
    if not selected_portfolios:
        console.print("[bold red]No valid portfolios selected.[/bold red]")
        return

    # Collect returns and calculate metrics
    metrics = []
    for name in selected_portfolios:
        portfolio_data = portfolios[name]
        portfolio_country = portfolio_data.get("country", "Unknown")
        stock_symbols = portfolio_data.get("stocks", [])
        portfolio_returns = pd.DataFrame()

        for symbol in stock_symbols:
            try:
                import yfinance as yf
                stock = yf.Ticker(symbol)
                stock_history = stock.history(period="1y")["Close"]
                stock_returns = stock_history.pct_change().dropna()
                portfolio_returns[symbol] = stock_returns
            except Exception as e:
                console.print(f"[bold red]Error fetching data for {symbol}: {e}[/bold red]")

        if portfolio_returns.empty:
            console.print(f"[bold red]No valid data for portfolio: {name}. Skipping.[/bold red]")
            continue

        portfolio_mean_returns = portfolio_returns.mean(axis=1)
        metrics.append({
            "Portfolio": name,
            "Country": portfolio_country,
            "Total Return": f"{cum_returns(portfolio_mean_returns).iloc[-1]:.2%}",
            "Annual Volatility": f"{annual_volatility(portfolio_mean_returns):.2%}",
            "Sharpe Ratio": f"{sharpe_ratio(portfolio_mean_returns):.2f}",
            "Max Drawdown": f"{max_drawdown(portfolio_mean_returns):.2%}",
            "Calmar Ratio": f"{calmar_ratio(portfolio_mean_returns):.2f}"
        })

    # Display metrics
    table = Table(title="Portfolio Comparison", header_style="bold", show_lines=True)
    table.add_column("Portfolio", style="cyan")
    table.add_column("Country", style="blue")
    table.add_column("Total Return", style="green")
    table.add_column("Annual Volatility", style="magenta")
    table.add_column("Sharpe Ratio", style="yellow")
    table.add_column("Max Drawdown", style="red")
    table.add_column("Calmar Ratio", style="blue")

    for metric in metrics:
        table.add_row(
            metric["Portfolio"], metric["Country"], metric["Total Return"], metric["Annual Volatility"],
            metric["Sharpe Ratio"], metric["Max Drawdown"], metric["Calmar Ratio"]
        )

    console.print(table)


# 2. Index Comparison
def index_comparison():
    """Compare index trends (e.g., NASDAQ vs. S&P 500)."""
    console.print("[bold cyan]INDEX COMPARISON[/bold cyan]")

    indices = ["^GSPC (S&P 500)", "^IXIC (NASDAQ)", "^DJI (Dow Jones)", "^NSEI (Nifty 50)"]
    display_in_columns("Available Indices", indices)

    selected_indices = Prompt.ask(
        "Enter indices symbols separated by commas for comparison"
    ).split(",")

    selected_indices = [index.strip() for index in selected_indices]
    if not selected_indices:
        console.print("[bold red]No valid indices selected.[/bold red]")
        return

    # Fetch index data
    data = {}
    for index in selected_indices:
        try:
            index_data = yf.download(index, period="1y")["Close"]
            data[index] = index_data
        except Exception as e:
            console.print(f"[bold red]Error fetching data for {index}: {e}[/bold red]")

    if not data:
        console.print("[bold red]No data available for selected indices.[/bold red]")
        return

    # Plot index trends
    plt.figure(figsize=(10, 6))
    for index, index_data in data.items():
        plt.plot(index_data, label=index)
    plt.title("Index Comparison")
    plt.legend()
    plt.show()


# 3. Stock Performance Comparison
def stock_performance_comparison():
    """Compare stock prices, returns, and trading volumes."""
    console.print("[bold cyan]STOCK PERFORMANCE COMPARISON[/bold cyan]")

    stock_symbols = Prompt.ask(
        "Enter stock symbols separated by commas for comparison (e.g., AAPL, MSFT)"
    ).split(",")

    stock_symbols = [symbol.strip() for symbol in stock_symbols]
    if not stock_symbols:
        console.print("[bold red]No valid stock symbols selected.[/bold red]")
        return

    # Fetch stock data
    data = {}
    for symbol in stock_symbols:
        try:
            stock_data = yf.download(symbol, period="1y")
            data[symbol] = stock_data
        except Exception as e:
            console.print(f"[bold red]Error fetching data for {symbol}: {e}[/bold red]")

    if not data:
        console.print("[bold red]No data available for selected stocks.[/bold red]")
        return

    # Plot stock trends
    plt.figure(figsize=(10, 6))
    for symbol, stock_data in data.items():
        plt.plot(stock_data["Close"], label=symbol)
    plt.title("Stock Performance Comparison")
    plt.legend()
    plt.show()


# 4. Economic Indicators Comparison
def economic_indicator_comparison():
    """Compare economic indicators like GDP, unemployment, inflation."""
    console.print("[bold cyan]ECONOMIC INDICATOR COMPARISON[/bold cyan]")

    # Example economic indicators
    indicators = ["GDP", "Unemployment Rate", "Inflation Rate"]
    display_in_columns("Available Economic Indicators", indicators)

    indicator_choice = Prompt.ask(
        "Enter the indicator name for comparison (e.g., GDP, Unemployment Rate)"
    )

    if indicator_choice not in indicators:
        console.print("[bold red]Invalid indicator selected.[/bold red]")
        return

    # Fetch data from FRED or other APIs
    try:
        # Example: Fetch data for GDP
        data = fetch_economic_data(indicator_choice)
        plt.figure(figsize=(10, 6))
        for country, country_data in data.items():
            plt.plot(country_data["date"], country_data["value"], label=country)
        plt.title(f"{indicator_choice} Comparison")
        plt.legend()
        plt.show()
    except Exception as e:
        console.print(f"[bold red]Error fetching data for {indicator_choice}: {e}[/bold red]")


# Fetch economic data (example placeholder)
def fetch_economic_data(indicator):
    """Fetch economic data for comparison (placeholder)."""
    # Replace this with actual API calls to FRED, IMF, or World Bank
    return {
        "USA": {"date": pd.date_range("2022-01-01", "2023-01-01", freq="M"), "value": range(12)},
        "India": {"date": pd.date_range("2022-01-01", "2023-01-01", freq="M"), "value": range(12, 24)}
    }
