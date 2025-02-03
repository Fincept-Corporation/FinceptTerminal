from textual.containers import VerticalScroll, Container
from textual.widgets import Static, Input, Button, TabPane, TabbedContent, DataTable
from textual.app import ComposeResult
import yfinance as yf
import pandas as pd

class StockTrackerTab(Container):
    """Stock Tracker Interface with Multiple Analysis Sub-Tabs"""

    def compose(self) -> ComposeResult:
        """Compose the UI layout."""
        yield Static("Stock Tracker", classes="header")
        yield Input(placeholder="Enter Stock Ticker (e.g., AAPL)", id="ticker_input")
        yield Button("Fetch Data", id="fetch_button")

        # ✅ Create Sub-Tabs for Different Analysis
        with TabbedContent():
            with TabPane("Info"):
                with VerticalScroll(id="stock_info"):
                    yield DataTable(id="stock_info_table")

            with TabPane("Technicals"):
                with VerticalScroll(id="technicals"):
                    yield DataTable(id="technical_table")

            with TabPane("Fundamentals"):
                with TabbedContent():
                    with TabPane("financial"):
                        with VerticalScroll():
                            yield DataTable(id="financials_table")
                    with TabPane("Balance Sheet"):
                        with VerticalScroll():
                            yield DataTable(id="balance_sheet_table")
                    with TabPane("Cashflow"):
                        with VerticalScroll():
                            yield DataTable(id="cashflow_table")

            with TabPane("Quant"):
                with VerticalScroll(id="quant"):
                    yield DataTable(id="quant_table")

            with TabPane("News"):
                with VerticalScroll(id="news"):
                    yield DataTable(id="news_table")

    async def on_button_pressed(self, event: Button.Pressed):
        """Handle button press events."""
        if event.button.id == "fetch_button":
            ticker = self.query_one("#ticker_input", Input).value.strip().upper()

            if not ticker:
                self.app.notify("Please enter a valid stock ticker!", severity="error")
                return
            await self._fetch_stock_data(ticker)
            self.app.notify(f"Fetching data for {ticker}...", severity="information")


    async def _fetch_stock_data(self, ticker: str):
        """Fetch stock data and update tables."""
        stock = yf.Ticker(ticker)

        try:
            # ✅ Fetch all data
            info = stock.info
            history = stock.history(period="1y")
            financials = stock.financials
            balance_sheet = stock.balance_sheet
            cashflow = stock.cashflow

            # ✅ Populate all tables
            await self._populate_info_table(info)
            await self._populate_technical_table(history)
            await self._populate_financial_table(financials, balance_sheet, cashflow)
            await self._populate_quant_table(history)
            await self._populate_news_table(ticker)

            self.app.notify(f"Data fetched successfully for {ticker}!")

        except Exception as e:
            self.app.notify(f"Error fetching data: {e}", severity="error")

    async def _populate_info_table(self, info):
        """Populate the stock information table."""
        table = self.query_one("#stock_info_table", DataTable)
        table.clear()
        if not table.columns:
            table.add_column("Attribute")
            table.add_column("Value")

        for key, value in info.items():
            table.add_row(str(key), str(value))

    async def _populate_technical_table(self, history):
        """Calculate and populate technical indicators."""
        if history.empty:
            self.app.notify("No historical data available!", severity="warning")
            return

        table = self.query_one("#technical_table", DataTable)
        table.clear()
        if not table.columns:
            table.add_column("Indicator")
            table.add_column("Value")

        # ✅ Calculate Technical Indicators
        history["50_MA"] = history["Close"].rolling(window=50).mean()
        history["200_MA"] = history["Close"].rolling(window=200).mean()
        history["RSI"] = 100 - (100 / (1 + history["Close"].pct_change().rolling(window=14).mean() /
                                       history["Close"].pct_change().rolling(window=14).std()))
        history["MACD"] = history["Close"].ewm(span=12).mean() - history["Close"].ewm(span=26).mean()
        history["Signal"] = history["MACD"].ewm(span=9).mean()

        # ✅ Add Data to Table
        table.add_row("50-Day Moving Average", f"{history['50_MA'].iloc[-1]:.2f}")
        table.add_row("200-Day Moving Average", f"{history['200_MA'].iloc[-1]:.2f}")
        table.add_row("Latest RSI (14-day)", f"{history['RSI'].iloc[-1]:.2f}")
        table.add_row("MACD", f"{history['MACD'].iloc[-1]:.2f}")
        table.add_row("Signal Line", f"{history['Signal'].iloc[-1]:.2f}")

    async def _populate_financial_table(self, financials, balance_sheet, cashflow):
        """Update the Financials Subtab with data for financials, balance sheet, and cashflow."""

        # Populate Financials Table
        financials_table = self.query_one("#financials_table", DataTable)
        financials_table.clear()
        if not financials.empty:
            # Add columns: Metric and Years
            if not financials_table.columns:
                financials_table.add_column("Metric")
            existing_columns = set(financials_table.columns)

            for year in financials.columns:
                year_str = str(year.year)  # Assuming year is a datetime object
                if year_str not in existing_columns:
                    financials_table.add_column(year_str)
                    existing_columns.add(year_str)

            # Add rows: Metric and corresponding values for each year
            for metric, values in financials.iterrows():
                row = [str(metric)] + [str(values[year]) if not pd.isna(values[year]) else "N/A" for year in
                                       financials.columns]
                financials_table.add_row(*row)
        else:
            self.app.notify("No financial data available.", severity="warning")

        # Populate Balance Sheet Table
        balance_sheet_table = self.query_one("#balance_sheet_table", DataTable)
        balance_sheet_table.clear()
        if not balance_sheet.empty:
            # Add columns: Metric and Years
            if not balance_sheet_table.columns:
                balance_sheet_table.add_column("Metric")
            existing_columns = set(balance_sheet_table.columns)

            for year in balance_sheet.columns:
                year_str = str(year.year)  # Assuming year is a datetime object
                if year_str not in existing_columns:
                    balance_sheet_table.add_column(year_str)
                    existing_columns.add(year_str)

            # Add rows: Metric and corresponding values for each year
            for metric, values in balance_sheet.iterrows():
                row = [str(metric)] + [str(values[year]) if not pd.isna(values[year]) else "N/A" for year in
                                       balance_sheet.columns]
                balance_sheet_table.add_row(*row)
        else:
            self.app.notify("No balance sheet data available.", severity="warning")

        # Populate Cash Flow Table
        cashflow_table = self.query_one("#cashflow_table", DataTable)
        cashflow_table.clear()
        if not cashflow.empty:
            # Add columns: Metric and Years
            if not cashflow_table.columns:
                cashflow_table.add_column("Metric")
            existing_columns = set(cashflow_table.columns)

            for year in cashflow.columns:
                year_str = str(year.year)  # Assuming year is a datetime object
                if year_str not in existing_columns:
                    cashflow_table.add_column(year_str)
                    existing_columns.add(year_str)

            # Add rows: Metric and corresponding values for each year
            for metric, values in cashflow.iterrows():
                row = [str(metric)] + [str(values[year]) if not pd.isna(values[year]) else "N/A" for year in
                                       cashflow.columns]
                cashflow_table.add_row(*row)
        else:
            self.app.notify("No cash flow data available.", severity="warning")

        self.app.notify("Financial data updated successfully!")

    async def _populate_quant_table(self, history):
        """Perform and display quantitative analysis (Sharpe Ratio & Volatility)."""
        if history.empty:
            self.app.notify("No historical data available for quantitative analysis!", severity="warning")
            return

        table = self.query_one("#quant_table", DataTable)
        table.clear()
        if not table.columns:
            table.add_column("Metric")
            table.add_column("Value")

        # ✅ Calculate Quantitative Metrics
        history["Daily Return"] = history["Close"].pct_change()
        avg_daily_return = history["Daily Return"].mean()
        std_daily_return = history["Daily Return"].std()
        sharpe_ratio = (avg_daily_return / std_daily_return) * (252 ** 0.5)  # Annualized Sharpe Ratio
        history["Volatility"] = history["Daily Return"].rolling(window=21).std() * (252 ** 0.5)

        # ✅ Add Data to Table
        table.add_row("Annualized Sharpe Ratio", f"{sharpe_ratio:.2f}")
        table.add_row("Annualized Volatility", f"{std_daily_return * (252 ** 0.5):.2%}")
        table.add_row("Rolling 21-Day Volatility", f"{history['Volatility'].iloc[-1]:.2%}")

    async def _populate_news_table(self, ticker):
        """Fetch and display recent news for the given ticker and perform sentiment analysis."""
        table = self.query_one("#news_table", DataTable)
        table.clear()

        # Ensure columns are set
        if not table.columns:
            table.add_column("Title", width=40)
            table.add_column("Published")
            table.add_column("Description", width=60)
            table.add_column("Sentiment")

        # Fetch news articles
        try:
            news = GNews()
            news_results = news.get_news(ticker)

            if not news_results:
                self.app.notify(f"⚠ No news found for {ticker}.", severity="warning")
                return

            # Process and populate the table
            for article in news_results[:5]:  # Fetch top 5 articles
                title = article.get("title", "No Title")
                published = article.get("published date", "Unknown Date")
                description = article.get("description", "No Description")
                sentiment = await self.analyze_sentiment(title + " " + description)  # Perform sentiment analysis

                table.add_row(title, published, description, sentiment)

            self.app.notify(f"✅ News for {ticker} loaded successfully!")
        except Exception as e:
            self.app.notify(f"❌ Error fetching news: {e}", severity="error")

    async def analyze_sentiment(self, text):
        """
        Perform basic sentiment analysis.
        - 'Positive' if keywords like 'good' or 'positive' are present.
        - 'Negative' if keywords like 'bad' or 'negative' are present.
        - 'Neutral' otherwise.
        """
        text = text.lower()
        if any(word in text for word in ["good", "positive", "great", "excellent", "upbeat"]):
            return "Positive"
        elif any(word in text for word in ["bad", "negative", "poor", "down", "concern"]):
            return "Negative"
        else:
            return "Neutral"
