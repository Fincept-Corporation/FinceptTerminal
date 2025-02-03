import yfinance as yf
import pandas as pd
import json, os, asyncio, requests, re
from textual.containers import Container, Vertical, VerticalScroll
from textual.widgets import Static, Input, Button, TabbedContent, DataTable, TabPane, OptionList
from textual.app import ComposeResult
from scholarly import scholarly
import google.generativeai as genai
from empyrical import cum_returns, annual_volatility, sharpe_ratio, max_drawdown, calmar_ratio, alpha_beta
from io import BytesIO
from PyPDF2 import PdfReader
import plotly.offline as pyo
import plotly.graph_objects as go

BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # Current file's directory
SETTINGS_FILE = os.path.join(BASE_DIR, "settings", "settings.json")

class ConsumerBehaviorTab(Container):
    """Consumer Behavior Analysis Tab"""

    def compose(self) -> ComposeResult:
        """Compose the layout of the Consumer Behavior tab."""
        yield Static("Consumer Behavior Analysis", id="header")

        # Input for city selection
        with Vertical(classes="input-section"):
            yield Input(placeholder="Enter city to analyze...", id="city_input")
            yield Button("Analyze", id="analyze_button")

        with VerticalScroll(classes="consumer_behaviour_content_section", id="summary_section"):
            yield Static("Summary Report will appear here...", id="summary_display")


    async def on_button_pressed(self, event):
        """Handle button press events."""
        if event.button.id == "analyze_button":
            await self.process_consumer_behavior_analysis()

    async def process_consumer_behavior_analysis(self):
        """Fetch research papers, generate insights, and display only the summary report."""
        city_input = self.query_one("#city_input", Input).value.strip()
        if not city_input:
            self.app.notify("⚠ Please enter a city to analyze!", severity="warning")
            return

        # Notify: Starting analysis
        self.app.notify("🔍 Starting consumer behavior analysis...")

        # Stage 1: Fetch research papers
        self.app.notify(f"📚 Fetching research papers for {city_input}...")
        papers = await asyncio.to_thread(self.fetch_research_papers, city_input)
        if not papers:
            self.app.notify("❌ No research papers found. Analysis stopped.", severity="error")
            return

        # Notify: Found research papers
        self.app.notify(f"✅ Found {len(papers)} research papers. Consolidating content...")

        # Stage 2: Consolidate content
        consolidated_content = await asyncio.to_thread(self.consolidate_paper_content, papers)
        if not consolidated_content.strip():
            self.app.notify("❌ No accessible content found in the papers. Analysis stopped.", severity="error")
            return

        # Notify: Consolidation complete
        self.app.notify("📄 Consolidation complete. Generating summary report...")

        # Stage 3: Generate summary report
        summary_report = await asyncio.to_thread(self.generate_summary_report, consolidated_content)
        if summary_report:
            self.app.notify("✅ Summary report generated successfully!")
            self.query_one("#summary_display", Static).update(summary_report)
        else:
            self.app.notify("❌ Error generating the summary report.", severity="error")

    def fetch_research_papers(self, city, max_papers=10):
        """Fetch research papers related to consumer behavior in a given city."""
        query = f"consumer behavior in {city}"
        search_results = scholarly.search_pubs(query)
        papers = []
        for result in search_results:
            title = result.get("title", "Unknown Title")
            url = result.get("eprint_url", result.get("pub_url", ""))
            if title and url:
                papers.append({"title": title, "url": url})
            if len(papers) >= max_papers:
                break
        return papers

    def consolidate_paper_content(self, papers, max_papers=5):
        """Consolidate content from multiple research papers."""
        combined_content = ""
        for i, paper in enumerate(papers):
            if i >= max_papers:
                break
            content = self.fetch_paper_content(paper["url"])
            combined_content += f"Paper Title: {paper['title']}\nContent:\n{content}\n\n"
        return combined_content

    def fetch_paper_content(self, url):
        """Fetch the content of the paper from the URL."""
        try:
            response = requests.get(url, stream=True)
            response.raise_for_status()
            if "application/pdf" in response.headers.get("Content-Type", ""):
                pdf_file = BytesIO(response.content)
                reader = PdfReader(pdf_file)
                text = ""
                for page in reader.pages:
                    text += page.extract_text()
                return text or "Unable to extract text from PDF."
            return response.text
        except Exception as e:
            return f"Error fetching content: {e}"

    def extract_facts_dynamically(self, content):
        """Dynamically extract factual data from the consolidated content."""
        facts = []
        patterns = [
            r"(\d+%)",  # Percentages
            r"₹[\d,]+",  # Currency values in Indian Rupees
            r"\d+\s?(times|years|days)",  # Time-related data
            r"\d+\s?(respondents|samples|people|users|shoppers)",  # Sample size
        ]
        for pattern in patterns:
            matches = re.findall(pattern, content)
            facts.extend(matches)
        return list(set(facts))  # Remove duplicates

    def load_settings(self):
        """
        Load user settings from settings.json.

        Returns:
            dict: A dictionary containing the settings data.
        """
        if not os.path.exists(SETTINGS_FILE):
            self.app.notify("⚠ settings.json not found! Using default settings.", severity="warning")
            return {}  # Return an empty dictionary if the file doesn't exist

        try:
            with open(SETTINGS_FILE, "r") as file:
                settings = json.load(file)
                self.app.notify("✅ Successfully loaded settings.")
                return settings
        except json.JSONDecodeError as e:
            self.app.notify(f"❌ Error parsing settings.json: {e}", severity="error")
            return {}

    def generate_summary_report(self, content):
        """Generate a brief summary report based on the consolidated content."""
        settings = self.load_settings()
        api_key = settings.get("data_sources", {}).get("genai_model", {}).get("apikey")
        genai.configure(api_key=api_key)
        gemini_model = genai.GenerativeModel("gemini-1.5-flash")
        try:
            response = gemini_model.generate_content(
                f"Summarize the following research content into a structured consumer behavior report:\n\n{content}"
            )
            return response.text
        except Exception as e:
            return f"Error generating summary report: {e}"


class ComparisonAnalysisTab(Container):
    """Comparison Analysis Tab with Sub-Tabs for Each Comparison Type."""

    def compose(self):
        """Compose the layout for the Comparison Analysis tab."""
        yield Static("Comparison Analysis", id="header")
        with TabbedContent():
            with TabPane("Portfolio Comparison", id="portfolio_tab"):
                yield Static("Portfolio Comparison", id="portfolio_header")
                yield Input(placeholder="Enter portfolio names (comma-separated)...", id="portfolio_input")
                yield Button("Analyze Portfolios", id="portfolio_analyze_button")
                with VerticalScroll():
                    yield DataTable(id="portfolio_results_table")
                    yield DataTable(id="portfolio_risk_metrics_table")
                    yield DataTable(id="portfolio_sector_allocation_table")


            with TabPane("Index Comparison", id="index_tab"):
                yield Static("Index Comparison", id="index_header")
                yield Input(placeholder="Enter index symbols (comma-separated)...", id="index_input")
                yield Button("Analyze Indices", id="index_analyze_button")
                with VerticalScroll():
                    yield DataTable(id="index_results_table")
                    yield DataTable(id="index_trend_table", cursor_type="cell")
                    yield Button("Visualize",id="index_trend_visualize")

            with TabPane("Stock Performance", id="stock_tab"):
                yield Static("Stock Performance Comparison", id="stock_header")
                yield Input(placeholder="Enter stock symbols (comma-separated)...", id="stock_input")
                yield Button("Analyze Stocks", id="stock_analyze_button")
                with VerticalScroll():
                    yield DataTable(id="basic_analysis_table")
                    yield DataTable(id="intermediate_analysis_table")
                    yield DataTable(id="advanced_analysis_table")
                    yield DataTable(id="performance_over_time_table")


    async def on_button_pressed(self, event):
        """Handle button press events for each analysis type."""
        button_id = event.button.id

        if button_id == "portfolio_analyze_button":
            await self.process_portfolio_comparison()
        elif button_id == "index_analyze_button":
            await self.process_index_comparison()
        elif button_id == "index_trend_visualize":
            await self.visualize_index_trend("^GSPC")
        elif button_id == "stock_analyze_button":
            await self.process_stock_performance_comparison()
        elif button_id == "economic_analyze_button":
            await self.process_economic_indicator_comparison()

    async def process_portfolio_comparison(self):
        """Enhanced portfolio comparison with full table updates and error handling."""
        input_box = self.query_one("#portfolio_input", Input)
        portfolio_names = [name.strip().lower() for name in input_box.value.strip().split(",")]
        input_box.value = ""

        result_table = self.query_one("#portfolio_results_table", DataTable)
        if not portfolio_names:
            result_table.clear()
            result_table.add_row(
                ["No portfolios provided!"]
            )
            return

        result_table.clear()
        if not result_table.columns:
            result_table.add_columns(
                "Portfolio", "Total Invested", "Current Value", "Profit/Loss", "Total Return",
                "Annual Volatility", "Sharpe Ratio", "Max Drawdown", "Calmar Ratio"
            )

        risk_table = self.query_one("#portfolio_risk_metrics_table", DataTable)
        # Additional tables for risk and sector allocation
        risk_table.clear()
        if not risk_table.columns:
            risk_table.add_columns(
                "Portfolio", "Alpha", "Beta", "Sortino Ratio", "Treynor Ratio"
            )

        sector_table = self.query_one("#portfolio_sector_allocation_table", DataTable)
        sector_table.clear()
        if not sector_table.columns:
            sector_table.add_columns(
                "Portfolio", "Technology", "Healthcare", "Financials", "Energy", "Consumer Discretionary", "Others"
            )

        # Load only the specified portfolios from settings
        portfolios = self.load_portfolios_from_settings(portfolio_names)
        portfolios = {key.lower(): value for key, value in portfolios.items()}

        for name in portfolio_names:
            if name not in portfolios:
                # If portfolio not found, add empty row for that portfolio
                self.query_one("#portfolio_results_table", DataTable).add_row(
                    name, "No Data", "No Data", "No Data", "No Data", "No Data", "No Data", "No Data", "No Data"
                )
                continue

            portfolio_data = portfolios[name]
            individual_returns = pd.DataFrame()
            portfolio_weights = []
            total_invested = 0
            current_value = 0
            sector_allocation = {
                "Technology": 0, "Healthcare": 0, "Financials": 0,
                "Energy": 0, "Consumer Discretionary": 0, "Others": 0
            }

            # Process each stock in the portfolio
            for symbol, stock_info in portfolio_data.items():
                try:
                    quantity = stock_info.get("quantity", 0)
                    avg_price = stock_info.get("avg_price", 0)
                    if quantity <= 0 or avg_price <= 0:
                        continue

                    total_invested += quantity * avg_price

                    # Fetch stock historical data
                    stock = yf.Ticker(symbol)
                    stock_history = stock.history(period="1y")["Close"]
                    if stock_history.empty:
                        continue

                    # Calculate current value and returns
                    current_price = stock_history.iloc[-1]
                    current_value += quantity * current_price

                    stock_returns = stock_history.pct_change().dropna()
                    individual_returns[symbol] = stock_returns
                    portfolio_weights.append(quantity * avg_price)

                    # Sector allocation
                    stock_info = stock.info
                    sector = stock_info.get("sector", "Others")
                    sector_allocation[sector] = sector_allocation.get(sector, 0) + quantity * avg_price
                except Exception as e:
                    print(f"Error processing {symbol}: {e}")
                    continue

            if individual_returns.empty or not portfolio_weights:
                # If no valid data, add default row
                self.query_one("#portfolio_results_table", DataTable).add_row(
                    name, f"₹{total_invested:.2f}", "No Data", "No Data", "No Data", "No Data", "No Data", "No Data",
                    "No Data"
                )
                continue

            # Normalize portfolio weights
            total_weight = sum(portfolio_weights)
            portfolio_weights = [weight / total_weight for weight in portfolio_weights]

            # Portfolio metrics
            try:
                portfolio_mean_returns = (individual_returns * portfolio_weights).sum(axis=1)
                total_return = ((current_value - total_invested) / total_invested) * 100
                annual_vol = annual_volatility(portfolio_mean_returns)
                sharpe = sharpe_ratio(portfolio_mean_returns, risk_free=0.01)
                max_dd = max_drawdown(portfolio_mean_returns)
                calmar = calmar_ratio(portfolio_mean_returns)

                # Profit/Loss
                profit_loss = current_value - total_invested

                # Add to main results table
                self.query_one("#portfolio_results_table", DataTable).add_row(
                    name,
                    f"₹{total_invested:.2f}",
                    f"₹{current_value:.2f}",
                    f"₹{profit_loss:.2f}",
                    f"{total_return:.2f}%",
                    f"{annual_vol:.2%}",
                    f"{sharpe:.2f}",
                    f"{max_dd:.2%}",
                    f"{calmar:.2f}",
                )

                # Add risk metrics
                # Fetch Benchmark Data
                try:
                    benchmark = yf.Ticker("^GSPC").history(period="1y")["Close"].pct_change().dropna()
                    if benchmark.empty:
                        print("Benchmark data is empty.")
                        alpha, beta = "N/A", "N/A"
                    else:
                        # Calculate Alpha and Beta
                        alpha, beta = alpha_beta(portfolio_mean_returns, benchmark)
                except Exception as e:
                    print(f"Error fetching benchmark data or calculating alpha/beta: {e}")
                    alpha, beta = "N/A", "N/A"

                # Sortino Ratio
                try:
                    downside_deviation = portfolio_mean_returns[portfolio_mean_returns < 0].std() * (252 ** 0.5)
                    sortino = (
                                          portfolio_mean_returns.mean() - 0.01) / downside_deviation if downside_deviation > 0 else "N/A"
                except Exception as e:
                    print(f"Error calculating Sortino Ratio: {e}")
                    sortino = "N/A"

                # Treynor Ratio
                try:
                    treynor = total_return / beta if beta != 0 and beta != "N/A" else "N/A"
                except Exception as e:
                    print(f"Error calculating Treynor Ratio: {e}")
                    treynor = "N/A"

                # Add Row to Risk Metrics Table
                self.query_one("#portfolio_risk_metrics_table", DataTable).add_row(
                    name,
                    f"{alpha:.2f}" if alpha != "N/A" else alpha,
                    f"{beta:.2f}" if beta != "N/A" else beta,
                    f"{sortino:.2f}" if sortino != "N/A" else sortino,
                    f"{treynor:.2f}" if treynor != "N/A" else treynor
                )

                # Add sector allocation
                total_sector = sum(sector_allocation.values())
                if total_sector > 0:
                    for sector in sector_allocation:
                        sector_allocation[sector] = (sector_allocation[sector] / total_sector) * 100

                self.query_one("#portfolio_sector_allocation_table", DataTable).add_row(
                    name,
                    f"{sector_allocation['Technology']:.2f}%",
                    f"{sector_allocation['Healthcare']:.2f}%",
                    f"{sector_allocation['Financials']:.2f}%",
                    f"{sector_allocation['Energy']:.2f}%",
                    f"{sector_allocation['Consumer Discretionary']:.2f}%",
                    f"{sector_allocation['Others']:.2f}%"
                )
            except Exception as e:
                self.query_one("#portfolio_results_table", DataTable).add_row(
                    name, f"₹{total_invested:.2f}", "Error", "Error", "Error", "Error", "Error", "Error", "Error"
                )
                print(f"Error calculating metrics for {name}: {e}")

    def load_portfolios_from_settings(self, portfolio_names=None):
        """
        Load specific portfolios from the settings.json file.

        Args:
            portfolio_names (list): List of portfolio names to fetch. If None, fetches all portfolios.

        Returns:
            dict: A dictionary of the requested portfolio(s). If portfolio_names is None, returns all portfolios.
        """
        if not os.path.exists(SETTINGS_FILE):
            raise FileNotFoundError(f"Settings file not found at {SETTINGS_FILE}.")

        try:
            with open(SETTINGS_FILE, "r") as file:
                settings = json.load(file)

            portfolios = settings.get("portfolios", {})

            # If no specific portfolio names are provided, return all portfolios
            if portfolio_names is None:
                return portfolios

            # Filter and return only the requested portfolios
            requested_portfolios = {}
            for name in portfolio_names:
                portfolio_name = name.strip().lower()
                if portfolio_name in portfolios:
                    requested_portfolios[portfolio_name] = portfolios[portfolio_name]
                else:
                    print(f"Portfolio '{name}' not found in settings.")

            return requested_portfolios

        except json.JSONDecodeError as e:
            raise ValueError(f"Error decoding JSON from settings file: {e}")
        except Exception as e:
            raise RuntimeError(f"Unexpected error loading portfolios: {e}")


    async def visualize_index_trend(self, index_name: str):
        """
        Visualize the trend of an index and generate an interactive graph in HTML format.

        Args:
            index_name (str): The name of the index to visualize (e.g., "^GSPC").
        """
        try:
            # ✅ Fetch historical data for the index
            index_ticker = yf.Ticker(index_name)
            index_data = index_ticker.history(period="1y")

            if index_data.empty:
                self.app.notify(f"No data found for index '{index_name}'.", severity="error")
                return

            # ✅ Extract relevant data
            dates = index_data.index
            closing_prices = index_data["Close"]

            # ✅ Create a line chart using Plotly
            fig = go.Figure()

            fig.add_trace(
                go.Scatter(
                    x=dates,
                    y=closing_prices,
                    mode="lines",
                    name=f"{index_name} Trend",
                    line=dict(color="#1f77b4", width=2)
                )
            )

            # ✅ Update chart layout
            fig.update_layout(
                title=f"{index_name} - 1-Year Trend",
                title_x=0.5,
                xaxis_title="Date",
                yaxis_title="Closing Price",
                paper_bgcolor="#282828",
                plot_bgcolor="#282828",
                font=dict(color="white"),
                xaxis=dict(showgrid=True, gridcolor="#444"),
                yaxis=dict(showgrid=True, gridcolor="#444"),
                margin=dict(t=50, l=25, r=25, b=25),
            )

            # ✅ Save the chart as an interactive HTML file
            filename = f"{index_name}_trend.html"
            pyo.plot(fig, filename=filename, auto_open=True)

            self.app.notify(f"Trend visualization for '{index_name}' saved as {filename}.")

        except Exception as e:
            print(f"⚠️ Error visualizing trend for {index_name}: {e}")
            self.app.notify(f"Error visualizing trend for '{index_name}'.", severity="error")

    async def process_index_comparison(self):
        """Perform index comparison analysis with key metrics and trends."""
        input_box = self.query_one("#index_input", Input)
        indices = [index.strip().upper() for index in input_box.value.strip().split(",")]
        input_box.value = ""

        if not indices:
            self.query_one("#index_results_table", DataTable).clear()
            self.query_one("#index_results_table", DataTable).add_row(["No indices provided!"])
            return

        # Clear and initialize all DataTables
        results_table = self.query_one("#index_results_table", DataTable)
        results_table.clear()
        if not results_table.columns:
            results_table.add_columns(
                "Index", "Total Return (%)", "Annual Volatility (%)", "Max Drawdown (%)"
            )

        trend_table = self.query_one("#index_trend_table", DataTable)
        trend_table.clear()
        if not trend_table.columns:
            trend_table.add_columns("Index", "1-Year Trend")

        # Iterate through the indices for analysis
        for index in indices:
            try:
                # Fetch historical data for the index
                index_ticker = yf.Ticker(index)
                index_data = index_ticker.history(period="1y")

                # Skip if no data
                if index_data.empty:
                    results_table.add_row(index, "No Data", "No Data", "No Data")
                    trend_table.add_row(index, "No Trend")
                    continue

                # Calculate metrics
                total_return = ((index_data["Close"][-1] - index_data["Close"][0]) / index_data["Close"][0]) * 100
                annual_vol = annual_volatility(index_data["Close"].pct_change().dropna()) * 100
                max_dd = max_drawdown(index_data["Close"].pct_change().dropna()) * 100

                # Add metrics to the results table
                results_table.add_row(
                    index,
                    f"{total_return:.2f}%",
                    f"{annual_vol:.2f}%",
                    f"{max_dd:.2f}%"
                )

                # Add trend to the trend table (placeholder for visualization)
                trend_table.add_row(index, "Trend Available")

                # Optional: Plot the trend (you can integrate matplotlib or plotly for this)
                # Example:
                # import matplotlib.pyplot as plt
                # plt.plot(index_data["Close"])
                # plt.title(f"{index} - 1-Year Trend")
                # plt.savefig(f"{index}_trend.png")

            except Exception as e:
                # Handle errors gracefully and continue processing
                results_table.add_row(index, "Error", "Error", "Error")
                trend_table.add_row(index, "Error")
                print(f"Error processing index {index}: {e}")
                continue

    async def process_stock_performance_comparison(self):
        """Perform stock performance comparison analysis."""
        input_box = self.query_one("#stock_input", Input)
        stocks = [stock.strip() for stock in input_box.value.split(",") if stock.strip()]
        input_box.value = ""

        if not stocks:
            self.query_one("#basic_analysis_table", DataTable).clear()
            self.query_one("#basic_analysis_table", DataTable).add_row(["No stocks provided!"])
            return

        # Clear previous results
        for table_id in [
            "basic_analysis_table",
            "intermediate_analysis_table",
            "advanced_analysis_table",
            "performance_over_time_table",
        ]:
            self.query_one(f"#{table_id}", DataTable).clear()

        # Analyze stocks
        await self.basic_analysis(stocks)
        await self.intermediate_analysis(stocks)
        await self.advanced_analysis(stocks)
        await self.performance_over_time_analysis(stocks)

    async def basic_analysis(self, stocks):
        """Perform detailed basic stock analysis."""
        table = self.query_one("#basic_analysis_table", DataTable)
        if not table.columns:
            table.add_columns(
                "Stock",
                "Current Price",
                "52-Week High",
                "52-Week Low",
                "Dividend Yield",
                "Beta",
                "Volume",
                "Sector"
            )

        for stock in stocks:
            try:
                ticker = yf.Ticker(stock)
                info = ticker.info
                table.add_row(
                    stock,
                    f"{info.get('currentPrice', 'N/A')}",
                    f"{info.get('fiftyTwoWeekHigh', 'N/A')}",
                    f"{info.get('fiftyTwoWeekLow', 'N/A')}",
                    f"{info.get('dividendYield', 'N/A'):.2%}" if info.get("dividendYield") else "N/A",
                    f"{info.get('beta', 'N/A')}",
                    f"{info.get('volume', 'N/A')}",
                    f"{info.get('sector', 'N/A')}"
                )
            except Exception as e:
                table.add_row(stock, "Error", "Error", "Error", "Error", "Error", "Error", "Error")

    async def intermediate_analysis(self, stocks):
        """Perform detailed intermediate stock analysis."""
        table = self.query_one("#intermediate_analysis_table", DataTable)
        if not table.columns:
            table.add_columns(
                "Stock",
                "P/E Ratio",
                "EPS",
                "Market Cap",
                "Revenue",
                "Profit Margin",
                "Operating Margin",
                "Debt-to-Equity Ratio"
            )

        for stock in stocks:
            try:
                ticker = yf.Ticker(stock)
                info = ticker.info
                table.add_row(
                    stock,
                    f"{info.get('trailingPE', 'N/A')}",
                    f"{info.get('trailingEps', 'N/A')}",
                    f"{info.get('marketCap', 'N/A')}",
                    f"{info.get('totalRevenue', 'N/A')}",
                    f"{info.get('profitMargins', 'N/A'):.2%}" if info.get("profitMargins") else "N/A",
                    f"{info.get('operatingMargins', 'N/A'):.2%}" if info.get("operatingMargins") else "N/A",
                    f"{info.get('debtToEquity', 'N/A')}"
                )
            except Exception as e:
                table.add_row(stock, "Error", "Error", "Error", "Error", "Error", "Error", "Error")

    async def advanced_analysis(self, stocks):
        """Perform detailed advanced stock analysis."""
        table = self.query_one("#advanced_analysis_table", DataTable)
        table.clear()  # Clear previous data
        if not table.columns:
            table.add_columns(
                "Stock",
                "Sharpe Ratio",
                "Volatility",
                "Max Drawdown",
                "Calmar Ratio",
                "Sortino Ratio",
                "Alpha",
                "Beta"
            )

        for stock in stocks:
            try:
                # Fetch stock data
                stockticker = yf.Ticker(stock)
                stock_data = stockticker.history(period="2y")
                if stock_data.empty or "Close" not in stock_data.columns:
                    table.add_row(stock, "No Data", "No Data", "No Data", "No Data", "No Data", "No Data", "No Data")
                    continue

                # Calculate daily returns
                stock_data["Daily Return"] = stock_data["Close"].pct_change().dropna()
                daily_returns = stock_data["Daily Return"]

                if daily_returns.empty:
                    table.add_row(stock, "No Data", "No Data", "No Data", "No Data", "No Data", "No Data", "No Data")
                    continue

                # Use empyrical to calculate financial metrics
                calculated_sharpe_ratio = sharpe_ratio(daily_returns, risk_free=0.01)  # Assume 1% risk-free rate
                calculated_volatility = annual_volatility(daily_returns)
                calculated_max_drawdown = max_drawdown(daily_returns)
                calculated_calmar_ratio = calmar_ratio(daily_returns)
                calculated_sortino_ratio = sharpe_ratio(
                    daily_returns, risk_free=0.01
                )  # Assume 2% minimum return for Sortino

                # Estimate Alpha and Beta (requires a market benchmark, e.g., S&P 500)
                benchmark_data_ticker = yf.Ticker("^GSPC")
                benchmark_data = benchmark_data_ticker.history(period="1y")  # S&P 500 as benchmark
                benchmark_data["Daily Return"] = benchmark_data["Close"].pct_change().dropna()

                if not benchmark_data["Daily Return"].empty:
                    covariance_matrix = pd.DataFrame(
                        {
                            "Stock": daily_returns,
                            "Benchmark": benchmark_data["Daily Return"]
                        }
                    ).cov()
                    beta = covariance_matrix.iloc[0, 1] / covariance_matrix.iloc[1, 1]
                    alpha = daily_returns.mean() - beta * benchmark_data["Daily Return"].mean()
                else:
                    beta = "N/A"
                    alpha = "N/A"

                # Add results to the table
                table.add_row(
                    stock,
                    f"{calculated_sharpe_ratio:.2f}",
                    f"{calculated_volatility:.2%}",
                    f"{calculated_max_drawdown:.2%}",
                    f"{calculated_calmar_ratio:.2f}",
                    f"{calculated_sortino_ratio:.2f}",
                    f"{alpha:.2f}" if isinstance(alpha, float) else alpha,
                    f"{beta:.2f}" if isinstance(beta, float) else beta,
                )
            except Exception as e:
                table.add_row(stock, "Error", "Error", "Error", "Error", "Error", "Error", "Error")
                print(f"Error processing {stock}: {e}")  # Log error details.

    async def performance_over_time_analysis(self, stocks):
        """Analyze stock performance over time using the expected return formula."""
        table = self.query_one("#performance_over_time_table", DataTable)
        table.clear()  # Clear previous data
        if not table.columns:
            table.add_columns("Stock", "1-Month Return", "6-Month Return", "1-Year Return")

        for stock in stocks:
            try:
                # Fetch stock data
                stockticker = yf.Ticker(stock)
                stock_data = stockticker.history(period="2y")
                if stock_data.empty or "Close" not in stock_data.columns:
                    table.add_row(stock, "No Data", "No Data", "No Data")
                    continue

                # Ensure stock_data has a DateTime index
                stock_data.index = pd.to_datetime(stock_data.index)

                # Resample to get the last closing price of each month
                monthly_data = stock_data["Close"].resample("M").last()

                # Default values for insufficient data
                one_month_return = "Insufficient Data"
                six_month_return = "Insufficient Data"
                one_year_return = "Insufficient Data"

                # Calculate returns using the Expected Return formula
                if len(monthly_data) >= 2:  # Ensure at least two months of data
                    final_price = monthly_data.iloc[-1]
                    initial_price = monthly_data.iloc[-2]
                    one_month_return = ((final_price - initial_price) / initial_price) * 100

                if len(monthly_data) >= 7:  # Ensure at least seven months of data
                    final_price = monthly_data.iloc[-1]
                    initial_price = monthly_data.iloc[-7]
                    six_month_return = ((final_price - initial_price) / initial_price) * 100

                if len(monthly_data) >= 13:  # Ensure at least thirteen months of data
                    final_price = monthly_data.iloc[-1]
                    initial_price = monthly_data.iloc[-13]
                    one_year_return = ((final_price - initial_price) / initial_price) * 100

                # Add row to the table
                table.add_row(
                    stock,
                    f"{one_month_return:.2f}%" if isinstance(one_month_return, float) else one_month_return,
                    f"{six_month_return:.2f}%" if isinstance(six_month_return, float) else six_month_return,
                    f"{one_year_return:.2f}%" if isinstance(one_year_return, float) else one_year_return,
                )
            except Exception as e:
                table.add_row(stock, "Error", "Error", "Error")
                print(f"Error processing {stock}: {e}")  # Log error details


