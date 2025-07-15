import dearpygui.dearpygui as dpg
import yfinance as yf
import financedatabase as fd
import asyncio
import threading
from concurrent.futures import ThreadPoolExecutor
from fincept_terminal.Utils.base_tab import BaseTab

class StockResearchTab(BaseTab):
    """Optimized Stock Research tab with faster loading and caching."""

    def __init__(self, app):
        super().__init__(app)
        # Lazy initialization for faster startup
        self.equities_db = None
        self.suggestions = []
        self.ticker_symbol = ''
        self.ticker = None
        self.cache = {}  # Simple cache for repeated queries
        self.executor = ThreadPoolExecutor(max_workers=3)  # For parallel processing
        self._db_loading = False

    def _init_db_lazy(self):
        """Initialize database only when needed"""
        if self.equities_db is None and not self._db_loading:
            self._db_loading = True

            # Load database in background thread
            def load_db():
                try:
                    self.equities_db = fd.Equities()
                except Exception:
                    self.equities_db = None
                finally:
                    self._db_loading = False

            threading.Thread(target=load_db, daemon=True).start()

    def get_label(self):
        return "Stock Research"

    def create_content(self):
        self.add_section_header("Stock Research")

        # Search section with improved layout
        with dpg.group(horizontal=True):
            dpg.add_input_text(
                label="Symbol",
                tag="stock_search_input",
                width=200,
                callback=self._on_search_input,
                on_enter=True,
                hint="Enter stock symbol..."
            )
            dpg.add_button(label="Search", callback=self._on_search)
            dpg.add_button(label="Clear", callback=self._on_clear)

        # Suggestions with loading indicator
        dpg.add_listbox(
            items=[],
            tag="stock_suggestions",
            num_items=5,
            width=200,
            callback=self._on_suggestion
        )
        dpg.add_text("", tag="search_status", color=[100, 100, 100])
        dpg.add_separator()

        # Nested tabs with lazy content loading
        dpg.add_tab_bar(tag="research_bar", parent=dpg.last_container(), callback=self._on_tab_change)

        # Overview tab
        with dpg.tab(label="Overview", tag="tab_overview", parent="research_bar"):
            dpg.add_text("Select a stock symbol to view overview", tag="overview_placeholder")
            self._create_overview_table()

        # Financials tab
        with dpg.tab(label="Financials", tag="tab_financials", parent="research_bar"):
            with dpg.group(horizontal=True):
                dpg.add_combo(
                    items=["financials", "balance_sheet", "cashflow"],
                    default_value="financials",
                    tag="fin_combo"
                )
                dpg.add_button(label="Load", callback=self._on_financials)
            dpg.add_separator()
            dpg.add_text("Select a statement type and click Load", tag="fin_placeholder")
            self._create_financials_table()

        # Price Data tab
        with dpg.tab(label="Price Data", tag="tab_price", parent="research_bar"):
            dpg.add_text("Select a stock symbol to view price chart", tag="price_placeholder")
            self._create_price_plot()

    def _create_overview_table(self):
        """Create overview table with initial hidden state"""
        with dpg.table(
                tag="overview_table",
                header_row=True,
                row_background=True,
                borders_innerH=True,
                borders_outerH=True,
                borders_innerV=True,
                borders_outerV=True,
                reorderable=True,
                resizable=True,
                policy=dpg.mvTable_SizingFixedFit,
                height=350,
                width=-1,
                show=False
        ):
            dpg.add_table_column(label="Field", width_fixed=True, init_width_or_weight=150)
            dpg.add_table_column(label="Value")

    def _create_financials_table(self):
        """Create financials table with initial hidden state"""
        with dpg.table(
                tag="fin_table",
                header_row=True,
                row_background=True,
                borders_innerH=True,
                borders_outerH=True,
                borders_innerV=True,
                borders_outerV=True,
                reorderable=True,
                resizable=True,
                policy=dpg.mvTable_SizingFixedFit,
                height=300,
                width=-1,
                show=False
        ):
            dpg.add_table_column(label="Metric", width_fixed=True, init_width_or_weight=200)

    def _create_price_plot(self):
        """Create price plot with initial hidden state"""
        with dpg.plot(
                label="Price Chart",
                height=350,
                width=-1,
                tag="price_plot",
                show=False
        ):
            dpg.add_plot_axis(dpg.mvXAxis, label="Time", tag="price_x", time_unit=dpg.mvTimeUnit_Day)
            dpg.add_plot_axis(dpg.mvYAxis, label="Price", tag="price_y")

    def _on_tab_change(self, sender, app_data):
        """Handle tab changes to load content on demand"""
        if app_data == "tab_price" and self.ticker_symbol and not dpg.get_item_configuration("price_plot")["show"]:
            asyncio.create_task(self._fetch_price())

    def _on_clear(self):
        """Clear all data and reset interface"""
        self.ticker_symbol = ''
        self.ticker = None
        dpg.set_value("stock_search_input", "")
        dpg.configure_item("stock_suggestions", items=[])
        dpg.set_value("search_status", "")

        # Hide and clear all content
        dpg.configure_item("overview_table", show=False)
        dpg.configure_item("overview_placeholder", show=True)
        dpg.configure_item("fin_table", show=False)
        dpg.configure_item("fin_placeholder", show=True)
        dpg.configure_item("price_plot", show=False)
        dpg.configure_item("price_placeholder", show=True)

    def _on_search_input(self, sender, data):
        """Handle search input with debouncing and caching"""
        query = data.strip().upper()
        if not query:
            dpg.configure_item("stock_suggestions", items=[])
            dpg.set_value("search_status", "")
            return

        # Check cache first
        if query in self.cache:
            items = self.cache[query]
            self.suggestions = items
            dpg.configure_item("stock_suggestions", items=items)
            dpg.set_value("search_status", f"Found {len(items)} matches")
            return

        # Initialize database if needed
        self._init_db_lazy()

        if self._db_loading:
            dpg.set_value("search_status", "Loading database...")
            return

        if self.equities_db is None:
            dpg.set_value("search_status", "Database unavailable")
            return

        # Search in background thread
        def search_async():
            try:
                df = self.equities_db.search(index=query)
                items = df['symbol'].tolist()[:10] if not df.empty else []

                # Cache results
                self.cache[query] = items

                # Update UI in main thread
                dpg.run_async_callback(lambda: self._update_suggestions(items, len(items)))

            except Exception:
                dpg.run_async_callback(lambda: self._update_suggestions([], 0))

        self.executor.submit(search_async)
        dpg.set_value("search_status", "Searching...")

    def _update_suggestions(self, items, count):
        """Update suggestions in main thread"""
        self.suggestions = items
        dpg.configure_item("stock_suggestions", items=items)
        if count > 0:
            dpg.set_value("search_status", f"Found {count} matches")
        else:
            dpg.set_value("search_status", "No matches found")

    def _on_suggestion(self, sender, data):
        """Handle suggestion selection"""
        dpg.set_value("stock_search_input", data)
        dpg.set_value("search_status", f"Selected: {data}")

    def _on_search(self):
        """Handle search button click"""
        sym = dpg.get_value("stock_search_input").strip().upper()
        if not sym:
            return

        self.ticker_symbol = sym
        dpg.set_value("search_status", f"Loading data for {sym}...")

        # Start async data loading
        asyncio.create_task(self._refresh_all(sym))

    async def _refresh_all(self, sym):
        """Load all data for symbol with parallel processing"""
        try:
            # Create ticker object
            self.ticker = await asyncio.to_thread(yf.Ticker, sym)

            # Update status
            dpg.run_async_callback(lambda: dpg.set_value("search_status", f"Loading overview for {sym}..."))

            # Load overview first (fastest)
            await self._fetch_overview()

            # Load other data in parallel if their tabs are visible
            current_tab = dpg.get_value("research_bar")

            if current_tab == "tab_financials":
                await self._fetch_financials("financials")
            elif current_tab == "tab_price":
                await self._fetch_price()

            dpg.run_async_callback(lambda: dpg.set_value("search_status", f"Data loaded for {sym}"))

        except Exception as e:
            dpg.run_async_callback(lambda: dpg.set_value("search_status", f"Error loading {sym}: {str(e)[:50]}"))

    async def _fetch_overview(self):
        """Fetch and display overview information"""
        if not self.ticker:
            return

        try:
            # Get basic info with timeout
            info = await asyncio.wait_for(
                asyncio.to_thread(lambda: self.ticker.info),
                timeout=10.0
            )

            # Filter to most important fields for faster display
            important_fields = [
                'symbol', 'longName', 'sector', 'industry', 'marketCap',
                'currentPrice', 'previousClose', 'dayLow', 'dayHigh',
                'volume', 'averageVolume', 'dividendYield', 'trailingPE',
                'forwardPE', 'priceToBook', 'debtToEquity', 'returnOnEquity'
            ]

            filtered_info = {k: v for k, v in info.items() if k in important_fields and v is not None}

            dpg.run_async_callback(self._update_overview, filtered_info)

        except asyncio.TimeoutError:
            dpg.run_async_callback(lambda: self._update_overview({"Error": "Timeout loading data"}))
        except Exception as e:
            dpg.run_async_callback(lambda: self._update_overview({"Error": str(e)}))

    def _update_overview(self, info):
        """Update overview table in main thread"""
        # Clear existing rows
        dpg.delete_item("overview_table", children_only=True)

        # Recreate columns (they get deleted with children)
        dpg.add_table_column(label="Field", parent="overview_table", width_fixed=True, init_width_or_weight=150)
        dpg.add_table_column(label="Value", parent="overview_table")

        # Add rows with formatting
        for key, value in info.items():
            formatted_key = key.replace('_', ' ').title()
            formatted_value = self._format_value(value)

            with dpg.table_row(parent="overview_table"):
                dpg.add_text(formatted_key)
                dpg.add_text(formatted_value)

        # Show table and hide placeholder
        dpg.configure_item("overview_table", show=True)
        dpg.configure_item("overview_placeholder", show=False)

    def _format_value(self, value):
        """Format values for display"""
        if isinstance(value, (int, float)):
            if abs(value) >= 1e9:
                return f"{value / 1e9:.2f}B"
            elif abs(value) >= 1e6:
                return f"{value / 1e6:.2f}M"
            elif abs(value) >= 1e3:
                return f"{value / 1e3:.2f}K"
            else:
                return f"{value:.2f}"
        return str(value)

    def _on_financials(self):
        """Handle financials load button"""
        if not self.ticker_symbol:
            dpg.set_value("search_status", "Select a stock first")
            return

        stmt = dpg.get_value("fin_combo")
        dpg.set_value("search_status", f"Loading {stmt}...")
        asyncio.create_task(self._fetch_financials(stmt))

    async def _fetch_financials(self, stmt):
        """Fetch financial statements with optimization"""
        if not self.ticker:
            return

        try:
            # Use shorter timeout for financial data
            df = await asyncio.wait_for(
                asyncio.to_thread(lambda: getattr(self.ticker, stmt)),
                timeout=15.0
            )

            dpg.run_async_callback(self._update_financials, df, stmt)

        except asyncio.TimeoutError:
            dpg.run_async_callback(lambda: self._update_financials(None, stmt, "Timeout"))
        except Exception as e:
            dpg.run_async_callback(lambda: self._update_financials(None, stmt, str(e)))

    def _update_financials(self, df, stmt, error=None):
        """Update financials table"""
        # Clear existing content
        dpg.delete_item("fin_table", children_only=True)

        if error:
            dpg.add_table_column(label="Error", parent="fin_table")
            with dpg.table_row(parent="fin_table"):
                dpg.add_text(f"Failed to load {stmt}: {error}")
            dpg.configure_item("fin_table", show=True)
            dpg.configure_item("fin_placeholder", show=False)
            return

        if df is None or df.empty:
            dpg.add_table_column(label="No Data", parent="fin_table")
            with dpg.table_row(parent="fin_table"):
                dpg.add_text(f"No {stmt} data available")
            dpg.configure_item("fin_table", show=True)
            dpg.configure_item("fin_placeholder", show=False)
            return

        # Add metric column
        dpg.add_table_column(label="Metric", parent="fin_table", width_fixed=True, init_width_or_weight=200)

        # Add period columns (limit to last 4 periods for performance)
        periods = df.columns[-4:] if len(df.columns) > 4 else df.columns
        for period in periods:
            label = period.strftime('%Y-%m-%d') if hasattr(period, 'strftime') else str(period)
            dpg.add_table_column(label=label, parent="fin_table")

        # Add rows (limit to top 20 metrics for performance)
        metrics = df.index[:20] if len(df.index) > 20 else df.index
        for metric in metrics:
            with dpg.table_row(parent="fin_table"):
                dpg.add_text(str(metric))
                for period in periods:
                    value = df.at[metric, period]
                    formatted_value = self._format_value(value) if value is not None else "N/A"
                    dpg.add_text(formatted_value)

        # Show table and hide placeholder
        dpg.configure_item("fin_table", show=True)
        dpg.configure_item("fin_placeholder", show=False)
        dpg.set_value("search_status", f"{stmt.title()} loaded")

    async def _fetch_price(self):
        """Fetch price data with optimization"""
        if not self.ticker:
            return

        try:
            # Fetch 1 year of data for faster loading
            df = await asyncio.wait_for(
                asyncio.to_thread(lambda: self.ticker.history(period="1y")),
                timeout=10.0
            )

            dpg.run_async_callback(self._update_price, df)

        except asyncio.TimeoutError:
            dpg.run_async_callback(lambda: self._update_price(None, "Timeout"))
        except Exception as e:
            dpg.run_async_callback(lambda: self._update_price(None, str(e)))

    def _update_price(self, df, error=None):
        """Update price chart"""
        # Clear existing plot data
        dpg.delete_item("price_plot", children_only=True)

        if error:
            dpg.add_plot_axis(dpg.mvXAxis, label="Error", parent="price_plot", tag="price_x_error")
            dpg.add_plot_axis(dpg.mvYAxis, label="Price", parent="price_plot", tag="price_y_error")
            dpg.configure_item("price_plot", show=True)
            dpg.configure_item("price_placeholder", show=False)
            dpg.set_value("search_status", f"Price data error: {error}")
            return

        if df is None or df.empty:
            dpg.add_plot_axis(dpg.mvXAxis, label="Time", parent="price_plot", tag="price_x_empty")
            dpg.add_plot_axis(dpg.mvYAxis, label="Price", parent="price_plot", tag="price_y_empty")
            dpg.configure_item("price_plot", show=True)
            dpg.configure_item("price_placeholder", show=False)
            dpg.set_value("search_status", "No price data available")
            return

        # Create axes
        dpg.add_plot_axis(dpg.mvXAxis, label="Time", parent="price_plot", tag="price_x_new",
                          time_unit=dpg.mvTimeUnit_Day)
        dpg.add_plot_axis(dpg.mvYAxis, label="Price", parent="price_plot", tag="price_y_new")

        # Sample data for performance (every 5th point if more than 100 points)
        if len(df) > 100:
            df = df.iloc[::5]

        # Convert timestamps and create candlestick data
        xs = [d.timestamp() for d in df.index]
        opens = df['Open'].tolist()
        closes = df['Close'].tolist()
        lows = df['Low'].tolist()
        highs = df['High'].tolist()

        # Add candlestick series
        try:
            dpg.add_candle_series(
                xs, opens, closes, lows, highs,
                parent="price_y_new",
                tag="price_candle",
                label=self.ticker_symbol
            )
        except Exception:
            # Fallback to line series if candlestick fails
            dpg.add_line_series(
                xs, closes,
                parent="price_y_new",
                tag="price_line",
                label=f"{self.ticker_symbol} Close"
            )

        # Show plot and hide placeholder
        dpg.configure_item("price_plot", show=True)
        dpg.configure_item("price_placeholder", show=False)
        dpg.set_value("search_status", f"Price chart loaded for {self.ticker_symbol}")

    def cleanup(self):
        """Clean up resources"""
        if hasattr(self, 'executor'):
            self.executor.shutdown(wait=False)
        self.ticker = None
        self.equities_db = None
        self.cache.clear()