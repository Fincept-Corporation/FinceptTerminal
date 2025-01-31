import ujson as json
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # Current file's directory
SETTINGS_FILE = os.path.join(BASE_DIR, "..", "settings", "settings.json")


def save_portfolios():
    """Save the portfolios to the settings.json file."""
    from fincept_terminal.utils.themes import console

    # Load existing settings
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as file:
            settings = json.load(file)
    else:
        settings = {}

    # Update the portfolios key in the settings
    settings["portfolios"] = {
        name: [stock.info.get('symbol', stock.ticker) for stock in stocks]
        for name, stocks in portfolios.items()
    }

    # Save updated settings back to the file
    with open(SETTINGS_FILE, "w") as file:
        json.dump(settings, file, indent=4)

    console.print("[bold green]Portfolios saved successfully to settings.json![/bold green]")


def load_portfolios():
    """Load portfolios from the settings.json file."""
    from os import path
    import yfinance as yf
    from fincept_terminal.utils.themes import console

    if path.exists(SETTINGS_FILE):
        try:
            with open(SETTINGS_FILE, "r") as file:
                settings = json.load(file)
                saved_portfolios = settings.get("portfolios", {})
                return {
                    name: [yf.Ticker(symbol) for symbol in stocks]
                    for name, stocks in saved_portfolios.items()
                }
        except (json.JSONDecodeError, ValueError):
            console.print(
                "[bold red]Error: The settings file is corrupted or invalid. Starting with an empty portfolio.[/bold red]"
            )
            return {}  # Return empty dictionary if JSON is invalid
    return {}

# Load the portfolios at startup
portfolios = load_portfolios()

def show_portfolio_menu():
    """Portfolio menu with options to create, modify, delete, and manage portfolios."""
    while True:
        from fincept_terminal.utils.themes import console
        console.print("[bold cyan]PORTFOLIO MENU[/bold cyan]")
        menu_options = [
            "CREATE NEW PORTFOLIO",
            "SELECT AND MANAGE EXISTING PORTFOLIO",
            "VIEW ALL PORTFOLIOS",
            "MODIFY PORTFOLIO NAME",
            "DELETE PORTFOLIO",
            "BACK TO MAIN MENU"
        ]
        from fincept_terminal.utils.const import display_in_columns
        display_in_columns("Select an Option", menu_options)

        from rich.prompt import Prompt
        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            create_new_portfolio()
        elif choice == "2":
            select_portfolio_to_manage()  # New function to select and manage portfolio
        elif choice == "3":
            console.print(list(portfolios.keys()))
        elif choice == "4":
            modify_portfolio_name()
        elif choice == "5":
            delete_portfolio()
        elif choice == "6":
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
            return  # Exit to main menu
        else:
            console.print("[bold red]Invalid choice, please select a valid option.[/bold red]")

def select_portfolio_to_manage():
    """Display all portfolios and allow the user to select and manage one."""
    from fincept_terminal.utils.themes import console
    if not portfolios:
        console.print("[bold red]No portfolios available. Create a portfolio first.[/bold red]")
        return

    console.print("[bold cyan]Select an existing portfolio to manage:[/bold cyan]\n")
    portfolio_names = list(portfolios.keys())
    from fincept_terminal.utils.const import display_in_columns
    display_in_columns("Available Portfolios", portfolio_names)

    from rich.prompt import Prompt
    portfolio_choice = Prompt.ask("Enter the portfolio number to select")

    try:
        selected_portfolio_name = portfolio_names[int(portfolio_choice) - 1]
        country = Prompt.ask("Enter the country for this portfolio")
        manage_selected_portfolio(selected_portfolio_name, country)  # Pass the selected portfolio for management
    except (ValueError, IndexError):
        console.print("[bold red]Invalid choice, please select a valid portfolio number.[/bold red]")

def create_new_portfolio():
    from fincept_terminal.utils.themes import console
    from rich.prompt import Prompt
    """Create a new portfolio and manage it."""
    portfolio_name = Prompt.ask("Enter the new portfolio name")

    # Ask the user to input the country for the portfolio
    country = Prompt.ask("Enter the country for this portfolio (e.g., India, United States, etc.)")

    portfolios[portfolio_name] = []  # Initialize an empty portfolio
    console.print(f"Portfolio '{portfolio_name}' created successfully!")

    # Automatically go to manage the newly created portfolio
    manage_selected_portfolio(portfolio_name, country)


def select_portfolio():
    """Allow users to select an existing portfolio for adding stocks or analysis."""
    from fincept_terminal.utils.themes import console
    if not portfolios:
        console.print("[bold red]No portfolios available. Create a portfolio first.[/bold red]")
        return

    console.print("[bold cyan]Select an existing portfolio:[/bold cyan]\n")
    portfolio_names = list(portfolios.keys())
    from fincept_terminal.utils.const import display_in_columns
    display_in_columns("Available Portfolios", portfolio_names)

    from rich.prompt import Prompt
    portfolio_choice = Prompt.ask("Enter the portfolio number to select")
    selected_portfolio_name = portfolio_names[int(portfolio_choice) - 1]

    manage_selected_portfolio(selected_portfolio_name)


def manage_selected_portfolio(portfolio_name, country):
    """Manage the selected portfolio."""
    while True:
        from fincept_terminal.utils.themes import console
        console.print(f"\n[bold cyan]MANAGE PORTFOLIO: {portfolio_name}[/bold cyan]")
        manage_menu = [
            "ADD STOCK TO PORTFOLIO",
            "VIEW CURRENT PORTFOLIO",
            "ANALYZE PORTFOLIO PERFORMANCE",
            "BACKTEST PORTFOLIO",  # New option for backtesting
            "BACK TO PREVIOUS MENU"
        ]

        from fincept_terminal.utils.const import display_in_columns
        from rich.prompt import Prompt
        display_in_columns(f"Portfolio: {portfolio_name}", manage_menu)
        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            add_stock_to_portfolio(portfolio_name)
        elif choice == "2":
            view_portfolio(portfolio_name)
        elif choice == "3":
            analyze_portfolio(portfolio_name, country)  # Pass both portfolio_name and country
        elif choice == "4":
            backtest_portfolio(portfolio_name)  # Call the backtest function here
        elif choice == "5":
            return  # Go back to the previous menu
        else:
            console.print("[bold red]Invalid choice, please select a valid option.[/bold red]")


def add_stock_to_portfolio(portfolio_name=None):
    """Add stocks to an existing portfolio."""
    from rich.prompt import Prompt
    from fincept_terminal.utils.themes import console
    import yfinance as yf

    if portfolio_name is None:
        if not portfolios:
            console.print("[bold red]No portfolios available. Please create one first.[/bold red]")
            return

        portfolio_names = list(portfolios.keys())
        portfolio_name = Prompt.ask("Enter the portfolio name")

    while True:
        ticker = Prompt.ask("Enter the stock symbol (or type 'back' to return)")
        if ticker.lower() == 'back':
            break

        try:
            stock = yf.Ticker(ticker)
            stock_info = stock.history(period="1y")

            if not stock_info.empty:
                portfolios[portfolio_name].append(stock)
                console.print(f"[bold green]{ticker} added to portfolio '{portfolio_name}'![/bold green]")

                # Save updated portfolios to settings.json
                save_portfolios()
            else:
                console.print(f"[bold red]No data found for {ticker}.[/bold red]")
        except Exception as e:
            console.print(f"[bold red]Error adding stock: {e}[/bold red]")


def view_portfolio(portfolio_name):
    """Display the current portfolio."""
    portfolio = portfolios.get(portfolio_name, [])

    from fincept_terminal.utils.themes import console
    if not portfolio:
        console.print(f"[bold red]Portfolio '{portfolio_name}' is empty.[/bold red]")
        return

    from rich.table import Table
    table = Table(title=f"Portfolio: {portfolio_name}", header_style="bold", show_lines=True)
    table.add_column("Symbol", style="cyan", width=15)
    table.add_column("Name", style="green", width=50)

    for stock in portfolio:
        stock_info = stock.info
        table.add_row(stock_info.get('symbol', 'N/A'), stock_info.get('shortName', 'N/A'))

    console.print(table)


def detect_market_index(stock_symbol, country):
    """Detect the appropriate market index based on the stock's country or exchange."""
    from fincept_terminal.utils.const import MARKET_INDEX_MAP
    market_index = MARKET_INDEX_MAP.get(country, None)

    if market_index:
        return market_index  # Return the market index if available for the country
    else:
        from fincept_terminal.utils.themes import console
        # If no market index is available, prompt the user for input
        console.print \
            (f"[bold yellow]No default market index found for {country}. Please enter a valid market index:[/bold yellow]")
        from rich.prompt import Prompt
        index = Prompt.ask("Enter the index symbol (e.g., ^NSEI for Nifty 50)", default="^NSEI")
        return index  # If the user doesn't enter anything, use ^NSEI as the default


def analyze_portfolio(portfolio_name, country):
    """Analyze portfolio performance with dynamic market index selection."""
    portfolio = portfolios.get(portfolio_name, [])
    from fincept_terminal.utils.themes import console

    if not portfolio:
        console.print(f"[bold red]Portfolio '{portfolio_name}' is empty.[/bold red]")
        return

    import pandas as pd
    portfolio_returns = pd.DataFrame()

    results = {}  # To store analysis results for export

    # Collect returns for each stock in the portfolio
    for stock in portfolio:
        ticker = stock.info['symbol']
        stock_history = stock.history(period="1y")['Close']  # Get the stock's closing prices
        stock_returns = stock_history.pct_change().dropna()  # Calculate daily returns
        portfolio_returns[ticker] = stock_returns

    try:
        mean_returns = portfolio_returns.mean(axis=1)
        from empyrical import (
            aggregate_returns, alpha, alpha_beta, alpha_beta_aligned, annual_return,
            annual_volatility, beta, beta_aligned, cum_returns, cum_returns_final,
            downside_risk, excess_sharpe, max_drawdown, omega_ratio, sharpe_ratio,
            sortino_ratio, tail_ratio, value_at_risk, calmar_ratio, conditional_value_at_risk
        )

        # Compute key metrics
        cumulative_returns = cum_returns(mean_returns)
        cumulative_returns_final = cum_returns_final(mean_returns)
        annual_vol = annual_volatility(mean_returns)
        sharpe = sharpe_ratio(mean_returns)
        max_dd = max_drawdown(mean_returns)
        calmar = calmar_ratio(mean_returns)
        sortino = sortino_ratio(mean_returns)
        omega = omega_ratio(mean_returns)
        downside = downside_risk(mean_returns)
        tail = tail_ratio(mean_returns)
        var = value_at_risk(mean_returns)
        cvar = conditional_value_at_risk(mean_returns)
        ann_return = annual_return(mean_returns)
        monthly_returns = aggregate_returns(mean_returns, convert_to='monthly')

        # Get the market index dynamically based on the country
        market_index = detect_market_index(portfolio[0].info['symbol'], country)

        # Fetch market returns from the detected index
        import yfinance as yf
        market_ticker = yf.Ticker(market_index)
        market_history = market_ticker.history(period="1y")['Close'].pct_change().dropna()

        # Align the dates of market returns and portfolio returns
        aligned_returns = pd.concat([mean_returns, market_history], axis=1).dropna()
        aligned_portfolio_returns = aligned_returns.iloc[:, 0]
        aligned_market_returns = aligned_returns.iloc[:, 1]

        # Alpha and Beta
        alpha_value, beta_value = alpha_beta_aligned(aligned_portfolio_returns, aligned_market_returns)
        alpha_standalone = alpha(aligned_portfolio_returns, aligned_market_returns)
        beta_standalone = beta(aligned_portfolio_returns, aligned_market_returns)
        excess_sharpe_value = excess_sharpe(aligned_portfolio_returns, aligned_market_returns)

        # Store results in dictionary for export
        results.update({
            "Cumulative Returns": f"{cumulative_returns[-1]:.2%}",
            "Cumulative Returns Final": f"{cumulative_returns_final:.2%}",
            "Annual Return": f"{ann_return:.2%}",
            "Monthly Aggregated Returns": f"{monthly_returns.mean():.2%}",
            "Annual Volatility": f"{annual_vol:.2%}",
            "Sharpe Ratio": f"{sharpe:.2f}",
            "Max Drawdown": f"{max_dd:.2%}",
            "Calmar Ratio": f"{calmar:.2f}",
            "Sortino Ratio": f"{sortino:.2f}",
            "Omega Ratio": f"{omega:.2f}",
            "Downside Risk": f"{downside:.2%}",
            "Tail Ratio": f"{tail:.2f}",
            "Value at Risk (VaR)": f"{var:.2%}",
            "Conditional VaR (CVaR)": f"{cvar:.2%}",
            "Alpha (Standalone)": f"{alpha_standalone:.2f}",
            "Beta (Standalone)": f"{beta_standalone:.2f}",
            "Alpha (Aligned)": f"{alpha_value:.2f}",
            "Beta (Aligned)": f"{beta_value:.2f}",
            "Excess Sharpe": f"{excess_sharpe_value:.2f}"
        })

        # Display the analysis
        from rich.table import Table
        analysis_table = Table(title=f"Portfolio Performance Analysis: {portfolio_name}", header_style="bold", show_lines=True)
        analysis_table.add_column("Metric", style="cyan")
        analysis_table.add_column("Value", style="green")

        # Add the metrics to the table
        for metric, value in results.items():
            analysis_table.add_row(metric, value)

        console.print(analysis_table)

        # Export the analysis
        export_portfolio_results(results, portfolio_name)

    except Exception as e:
        console.print(f"[bold red]Error in analyzing portfolio: {e}[/bold red]")


def view_all_portfolios():
    """Display a list of all existing portfolios."""
    from fincept_terminal.utils.themes import console
    if not portfolios:
        console.print("[bold red]No portfolios available. Create a portfolio first.[/bold red]")
        return

    from rich.table import Table
    table = Table(title="All Portfolios", header_style="bold", show_lines=True)
    table.add_column("Portfolio Name", style="cyan", width=30)
    table.add_column("Number of Stocks", style="green", width=20)

    for portfolio_name, stocks in portfolios.items():
        table.add_row(portfolio_name, str(len(stocks)))

    console.print(table)

def modify_portfolio_name():
    """Modify the name of an existing portfolio."""
    from rich.prompt import Prompt
    from fincept_terminal.utils.themes import console

    if not portfolios:
        console.print("[bold yellow]No portfolios available to modify.[/bold yellow]")
        return

    portfolio_name = Prompt.ask("Enter the portfolio name you want to modify")

    if portfolio_name not in portfolios:
        console.print(f"[bold red]Portfolio '{portfolio_name}' does not exist.[/bold red]")
        return

    new_name = Prompt.ask("Enter the new portfolio name")

    if new_name in portfolios:
        console.print(f"[bold red]A portfolio with the name '{new_name}' already exists.[/bold red]")
        return

    # Rename the portfolio
    portfolios[new_name] = portfolios.pop(portfolio_name)

    # Save updated portfolios to settings.json
    save_portfolios()

    console.print(f"[bold green]Portfolio '{portfolio_name}' renamed to '{new_name}' successfully![/bold green]")



def delete_portfolio():
    """Delete an existing portfolio."""
    from fincept_terminal.utils.themes import console
    from rich.prompt import Prompt

    if not portfolios:
        console.print("[bold yellow]No portfolios available to delete.[/bold yellow]")
        return

    portfolio_name = Prompt.ask("Enter the portfolio name you want to delete")

    if portfolio_name not in portfolios:
        console.print(f"[bold red]Portfolio '{portfolio_name}' does not exist.[/bold red]")
        return

    confirm = Prompt.ask(f"Are you sure you want to delete the portfolio '{portfolio_name}'? (yes/no)", default="no")

    if confirm.lower() == "yes":
        portfolios.pop(portfolio_name)
        console.print(f"[bold green]Portfolio '{portfolio_name}' deleted successfully![/bold green]")

        # Save updated portfolios to settings.json
        save_portfolios()



def export_portfolio_results(results, portfolio_name):
    """Export portfolio results to CSV or Excel."""
    from rich.prompt import Prompt
    choice = Prompt.ask("Do you want to export the results? (yes/no)", default="no")

    if choice.lower() == "yes":
        export_format = Prompt.ask("Choose export format: CSV or Excel", choices=["csv", "excel"], default="csv")
        file_name = f"{portfolio_name}_analysis.{export_format}"

        import pandas as pd
        df = pd.DataFrame.from_dict(results, orient='index', columns=["Value"])

        from fincept_terminal.utils.themes import console
        try:
            if export_format == "csv":
                df.to_csv(file_name, index_label="Metric")
            elif export_format == "excel":
                df.to_excel(file_name, index_label="Metric")
            console.print(f"[bold green]Portfolio analysis exported to {file_name} successfully![/bold green]")
        except Exception as e:
            console.print(f"[bold red]Failed to export data: {e}[/bold red]")

def export_backtest_results(pf, portfolio_name):
    """Export backtest results to CSV or Excel."""
    from rich.prompt import Prompt
    choice = Prompt.ask("Do you want to export the backtest results? (yes/no)", default="no")

    if choice.lower() == "yes":
        from fincept_terminal.utils.themes import console
        export_format = Prompt.ask("Choose export format: CSV or Excel", choices=["csv", "excel"], default="csv")
        file_name = f"{portfolio_name}_backtest_results.{export_format}"

        df = pf.to_dataframe()

        try:
            if export_format == "csv":
                df.to_csv(file_name, index=True)
            elif export_format == "excel":
                df.to_excel(file_name, index=True)
            console.print(f"[bold green]Backtest results exported to {file_name} successfully![/bold green]")
        except Exception as e:
            console.print(f"[bold red]Failed to export data: {e}[/bold red]")


def backtest_portfolio(portfolio_name):
    from fincept_terminal.utils.themes import console
    import pandas as pd
    """Perform a backtest on the entire portfolio by combining all stock prices."""
    portfolio = portfolios.get(portfolio_name, [])

    if not portfolio:
        console.print(f"[bold red]Portfolio '{portfolio_name}' is empty.[/bold red]")
        return

    portfolio_returns = pd.DataFrame()

    # Collect historical data for each stock and combine returns into a single DataFrame
    for stock in portfolio:
        ticker = stock.info['symbol']
        stock_history = stock.history(period="1y")['Close']

        if stock_history.empty:
            console.print(f"[bold red]No data available for {ticker}.[/bold red]")
            continue

        # Resample to daily frequency and fill any missing data
        stock_history = stock_history.asfreq('D').fillna(method='ffill')

        # Debugging: Output the stock's historical data to check for issues
        console.print(f"[yellow]Stock History for {ticker}:[/yellow]")
        console.print(stock_history.head(10))  # Display first 10 rows for inspection

        # Calculate daily returns
        stock_returns = stock_history.pct_change().dropna()

        # Store the returns in the portfolio dataframe
        portfolio_returns[ticker] = stock_returns

    # Check if there are enough stocks in the portfolio
    if portfolio_returns.empty:
        console.print(f"[bold red]No valid stock data found in the portfolio '{portfolio_name}'[/bold red]")
        return

    # Handle zero and non-finite values before backtesting
    import numpy as np
    portfolio_returns = portfolio_returns.replace([np.inf, -np.inf], np.nan).dropna()
    portfolio_returns = portfolio_returns[portfolio_returns > 0].dropna()

    # Validate the combined portfolio data
    if not validate_stock_data(portfolio_returns):
        console.print(f"[bold red]Invalid data detected in the portfolio '{portfolio_name}'[/bold red]")
        return

    # Create a portfolio-wide return series by averaging individual returns
    portfolio_returns['Portfolio'] = portfolio_returns.mean(axis=1)

    # Debugging: Output the portfolio returns to check for issues
    console.print(f"[yellow]Portfolio Returns:[/yellow]")
    console.print(portfolio_returns.head(10))  # Display first 10 rows for inspection

    console.print(f"\nGenerating backtest charts for {portfolio_name} (combined portfolio)...")

    try:
        # Use vectorbt to run the backtest on the combined portfolio
        import vectorbt as vbt
        pf = vbt.Portfolio.from_holding(portfolio_returns['Portfolio'], freq='D')

        # Plot the backtest charts for the entire portfolio
        pf.plot().show()

        # Display portfolio-wide performance metrics in table format
        display_backtest_metrics(pf, portfolio_name)

    except Exception as e:
        console.print(f"[bold red]Error in backtesting portfolio: {e}[/bold red]")
        console.print(f"[bold red]Please ensure all data is correct and finite before backtesting.[/bold red]")


def validate_stock_data(portfolio_returns):
    """Ensure that all data in the portfolio returns DataFrame is valid and finite."""
    # Check for missing values
    from fincept_terminal.utils.themes import console
    if portfolio_returns.isnull().values.any():
        console.print("[bold red]Error: Portfolio data contains missing (NaN) values.[/bold red]")
        return False

    # Check for non-finite values
    import numpy as np
    from fincept_terminal.utils.themes import console
    if not np.isfinite(portfolio_returns.values).all():
        console.print("[bold red]Error: Portfolio data contains non-finite values (e.g., inf, -inf).[/bold red]")
        return False

    # Check for non-positive values
    if (portfolio_returns <= 0).any().any():
        console.print("[bold red]Error: Portfolio data contains non-positive values.[/bold red]")
        return False

    return True  # Allow only valid, finite, and positive returns

def display_backtest_metrics(pf, portfolio_name):
    """Display backtest performance metrics in a table for the entire portfolio."""
    from fincept_terminal.utils.themes import console
    console.print(f"\nBacktesting results for {portfolio_name}")

    # Create a rich table to display the backtest metrics
    from rich.table import Table
    table = Table(title=f"Backtest Performance for {portfolio_name}", header_style="bold", show_lines=True)
    table.add_column("Metric", style="cyan", justify="left")
    table.add_column("Value", style="green", justify="right")

    try:
        # Calculate metrics
        total_return = pf.total_return()
        annualized_return = pf.annualized_return()
        max_drawdown = pf.max_drawdown()
        sharpe_ratio = pf.sharpe_ratio()
        sortino_ratio = pf.sortino_ratio()
        import numpy as np

        # Safeguard: If the annualized return is unreasonably high, flag it
        if annualized_return > 1000:  # Example threshold, adjust based on context
            console.print("[bold red]Warning: Annualized Return seems unreasonably high![/bold red]")
            annualized_return = np.nan  # Or handle appropriately

        # Add the metrics to the table
        table.add_row("Total Return", f"{total_return:.2%}" if isinstance(total_return, (int, float, np.float64)) else f"{total_return.iloc[-1]:.2%}")
        table.add_row("Annualized Return", f"{annualized_return:.2%}" if isinstance(annualized_return, (int, float, np.float64)) else f"{annualized_return.iloc[-1]:.2%}")
        table.add_row("Max Drawdown", f"{max_drawdown:.2%}" if isinstance(max_drawdown, (int, float, np.float64)) else f"{max_drawdown.iloc[-1]:.2%}")
        table.add_row("Sharpe Ratio", f"{sharpe_ratio:.2f}" if isinstance(sharpe_ratio, (int, float, np.float64)) else f"{sharpe_ratio.iloc[-1]:.2f}")
        table.add_row("Sortino Ratio", f"{sortino_ratio:.2f}" if isinstance(sortino_ratio, (int, float, np.float64)) else f"{sortino_ratio.iloc[-1]:.2f}")

    except Exception as e:
        console.print(f"[bold red]Error in calculating metrics: {e}[/bold red]")

    console.print(table)



def show_backtesting_menu():
    """Backtesting menu where users can run backtests on their portfolios."""
    from fincept_terminal.utils.themes import console
    console.print("[bold cyan]BACKTESTING MENU[/bold cyan]\n", style="info")
    console.print("1. Backtest Portfolio\n2. Back to Main Menu")

    from rich.prompt import Prompt
    choice = Prompt.ask("Enter your choice")

    if choice == "1":
        portfolio_name = Prompt.ask("Enter the portfolio name to backtest")
        if portfolio_name not in portfolios:
            console.print(f"[bold red]Portfolio '{portfolio_name}' not found![/bold red]")
            return

        # Perform backtest
        backtest_portfolio(portfolio_name)
    elif choice == "2":
        return  # Go back to the main menu
