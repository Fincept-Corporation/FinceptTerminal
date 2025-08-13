# -*- coding: utf-8 -*-
# watchlist_tab.py

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import logger, monitor_performance, operation
import datetime
import threading
import time
import random
import duckdb
from pathlib import Path
from typing import Dict, Any, Optional, Tuple
from functools import lru_cache
from decimal import Decimal, ROUND_HALF_UP


# Try to import yfinance for real data, fallback to simulated data
try:
    import yfinance as yf

    HAS_YFINANCE = True
    logger.info("yfinance module loaded successfully")
except ImportError:
    HAS_YFINANCE = False
    logger.warning("yfinance not available, using simulated data")

# Constants
UPDATE_INTERVAL = 4.0  # seconds
PRICE_CHANGE_LIMIT = 0.015  # ±1.5% max simulated change
MAX_RETRIES = 3
REQUEST_DELAY = 0.1  # delay between API requests
DB_FILE = "watchlist.db"


class WatchlistTab(BaseTab):
    """Bloomberg Terminal style Watchlist tab - Enhanced with DuckDB storage"""

    def __init__(self, main_app=None):
        logger.info("Initializing WatchlistTab")

        with operation("WatchlistTab initialization"):
            super().__init__(main_app)
            self.main_app = main_app

            # Bloomberg color scheme - cached for performance
            self._init_colors()

            # Core data structures
            self.watchlist: Dict[str, Dict[str, Any]] = {}
            self.auto_update = True
            self.refresh_running = False
            self.selected_ticker: Optional[str] = None

            # Performance optimizations
            self._last_update_time = 0.0
            self._update_lock = threading.RLock()
            self._price_cache: Dict[str, Tuple[float, float]] = {}  # ticker -> (price, timestamp)
            self._display_update_pending = False

            # Unique identifier for this instance
            self.instance_id = str(id(self))

            # Database connection
            self.db_path = self.get_config_directory() / "watchlist" / DB_FILE
            self.db_connection = None

            # Initialize database and data
            self._initialize_database()
            self._initialize_data()

        logger.info("WatchlistTab initialization completed", context={"instance_id": self.instance_id})

    def get_config_directory(self) -> Path:
        """Get platform-specific config directory - uses .fincept folder"""
        # Use .fincept folder at home directory for all platforms
        config_dir = Path.home() / '.fincept' / 'watchlist'
        return config_dir

    def _init_colors(self):
        """Initialize Bloomberg color scheme - cached for performance"""
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]

    def _initialize_database(self):
        """Initialize DuckDB database and create tables"""
        try:
            with operation("Database initialization"):
                # Create watchlist directory if it doesn't exist
                self.db_path.parent.mkdir(parents=True, exist_ok=True)

                # Initialize database connection
                self.db_connection = duckdb.connect(str(self.db_path))

                # Create watchlist table
                self.db_connection.execute("""
                    CREATE TABLE IF NOT EXISTS watchlist (
                        ticker VARCHAR PRIMARY KEY,
                        quantity DOUBLE NOT NULL,
                        avg_price DOUBLE NOT NULL,
                        alert_price DOUBLE,
                        current_price DOUBLE DEFAULT 0,
                        change_1d DOUBLE DEFAULT 0,
                        change_pct_1d DOUBLE DEFAULT 0,
                        change_pct_7d DOUBLE DEFAULT 0,
                        change_pct_30d DOUBLE DEFAULT 0,
                        last_updated VARCHAR DEFAULT '',
                        total_value DOUBLE DEFAULT 0,
                        unrealized_pnl DOUBLE DEFAULT 0,
                        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                    )
                """)

                # Create settings table for general settings
                self.db_connection.execute("""
                    CREATE TABLE IF NOT EXISTS settings (
                        key VARCHAR PRIMARY KEY,
                        value VARCHAR,
                        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                    )
                """)

                # Create price history table for caching
                self.db_connection.execute("""
                    CREATE TABLE IF NOT EXISTS price_history (
                        ticker VARCHAR,
                        price DOUBLE,
                        timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        PRIMARY KEY (ticker, timestamp)
                    )
                """)

                logger.info("Database initialized successfully", context={"db_path": str(self.db_path)})

        except Exception as e:
            logger.error("Failed to initialize database", context={"error": str(e), "db_path": str(self.db_path)},
                         exc_info=True)
            raise

    def _initialize_data(self):
        """Initialize watchlist data from database"""
        with operation("Data initialization"):
            self.load_watchlist_from_database()

    def get_label(self) -> str:
        return "Watchlist"

    # Add these helper methods to the class first (if not already added)
    def _round_currency(self, value: float) -> float:
        """Round currency values to 2 decimal places consistently"""
        if value is None:
            return 0.0
        return float(Decimal(str(value)).quantize(Decimal('0.01'), rounding=ROUND_HALF_UP))

    def _round_percentage(self, value: float) -> float:
        """Round percentage values to 4 decimal places for precision"""
        if value is None:
            return 0.0
        return float(Decimal(str(value)).quantize(Decimal('0.0001'), rounding=ROUND_HALF_UP))

    def _safe_float(self, value, default=0.0) -> float:
        """Safely convert value to float with default fallback"""
        if value is None:
            return default
        try:
            return float(value)
        except (ValueError, TypeError):
            return default

    # Replace the watchlist dictionary creation with this corrected version:
    def load_watchlist_from_database(self):
        """Load watchlist from DuckDB database with proper precision and validation"""
        try:
            with operation("Load watchlist from database"):
                if not self.db_connection:
                    logger.error("Database connection not available")
                    return

                # Load watchlist data
                result = self.db_connection.execute("SELECT * FROM watchlist").fetchall()
                columns = [desc[0] for desc in self.db_connection.description]

                self.watchlist.clear()

                if result:
                    for row in result:
                        row_dict = dict(zip(columns, row))
                        ticker = row_dict['ticker']

                        # Extract and validate core values
                        quantity = self._safe_float(row_dict.get('quantity'), 0.0)
                        avg_price = self._safe_float(row_dict.get('avg_price'), 0.0)
                        alert_price = self._safe_float(row_dict.get('alert_price')) if row_dict.get(
                            'alert_price') is not None else None

                        # Handle current price with fallback to avg_price
                        current_price = self._safe_float(row_dict.get('current_price'))
                        if current_price == 0.0 and avg_price > 0.0:
                            current_price = avg_price

                        # Handle change values with proper defaults
                        change_1d = self._safe_float(row_dict.get('change_1d'), 0.0)
                        change_pct_1d = self._safe_float(row_dict.get('change_pct_1d'), 0.0)
                        change_pct_7d = self._safe_float(row_dict.get('change_pct_7d'), 0.0)
                        change_pct_30d = self._safe_float(row_dict.get('change_pct_30d'), 0.0)

                        # Handle string values
                        last_updated = row_dict.get('last_updated') or ""
                        if isinstance(last_updated, datetime.datetime):
                            last_updated = last_updated.strftime("%H:%M:%S")

                        # Calculate derived values for consistency (don't trust stored values)
                        total_value = self._round_currency(current_price * quantity) if quantity > 0 else 0.0
                        unrealized_pnl = self._round_currency((current_price - avg_price) * quantity) if abs(
                            avg_price) > 1e-10 and quantity > 0 else 0.0

                        # Validate data integrity
                        if quantity <= 0:
                            logger.warning(f"Invalid quantity for {ticker}: {quantity}, skipping")
                            continue

                        if avg_price < 0:
                            logger.warning(f"Invalid avg_price for {ticker}: {avg_price}, skipping")
                            continue

                        # Create watchlist entry with proper precision
                        self.watchlist[ticker] = {
                            "quantity": quantity,
                            "avg_price": self._round_currency(avg_price),
                            "alert_price": self._round_currency(alert_price) if alert_price is not None else None,
                            "current_price": self._round_currency(current_price),
                            "change_1d": self._round_currency(change_1d),
                            "change_pct_1d": self._round_percentage(change_pct_1d),
                            "change_pct_7d": self._round_percentage(change_pct_7d),
                            "change_pct_30d": self._round_percentage(change_pct_30d),
                            "last_updated": str(last_updated),
                            "total_value": total_value,
                            "unrealized_pnl": unrealized_pnl
                        }

                    logger.info("Watchlist loaded from database", context={"positions": len(self.watchlist)})
                else:
                    logger.info("No watchlist data found in database, initializing with sample data")
                    self.initialize_sample_watchlist()

        except Exception as e:
            logger.error("Error loading watchlist from database", context={"error": str(e)}, exc_info=True)
            # Fallback to sample data
            self.initialize_sample_watchlist()

    @monitor_performance
    def save_watchlist_to_database(self):
        """Save watchlist data to DuckDB database"""
        try:
            if not self.db_connection:
                logger.error("Database connection not available")
                return

            with operation("Save watchlist to database"):
                current_timestamp = datetime.datetime.now()

                # Clear existing data and insert new data
                self.db_connection.execute("DELETE FROM watchlist")

                if self.watchlist:
                    # Prepare data for batch insert
                    data_rows = []
                    for ticker, data in self.watchlist.items():
                        data_rows.append((
                            ticker,
                            data['quantity'],
                            data['avg_price'],
                            data.get('alert_price'),
                            data['current_price'],
                            data['change_1d'],
                            data['change_pct_1d'],
                            data['change_pct_7d'],
                            data['change_pct_30d'],
                            data['last_updated'],
                            data['total_value'],
                            data['unrealized_pnl'],
                            current_timestamp,
                            current_timestamp
                        ))

                    # Batch insert
                    self.db_connection.executemany("""
                        INSERT INTO watchlist (
                            ticker, quantity, avg_price, alert_price, current_price,
                            change_1d, change_pct_1d, change_pct_7d, change_pct_30d,
                            last_updated, total_value, unrealized_pnl, created_at, updated_at
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """, data_rows)

                    logger.debug("Watchlist saved to database", context={"positions": len(self.watchlist)})

        except Exception as e:
            logger.error("Error saving watchlist to database", context={"error": str(e)}, exc_info=True)

    def get_setting(self, key: str, default: Any = None) -> Any:
        """Get setting value from database"""
        try:
            if not self.db_connection:
                return default

            result = self.db_connection.execute("SELECT value FROM settings WHERE key = ?", [key]).fetchone()
            return result[0] if result else default
        except Exception as e:
            logger.error("Error getting setting", context={"key": key, "error": str(e)})
            return default

    def set_setting(self, key: str, value: str):
        """Set setting value in database"""
        try:
            if not self.db_connection:
                return

            self.db_connection.execute("""
                INSERT OR REPLACE INTO settings (key, value, updated_at) 
                VALUES (?, ?, CURRENT_TIMESTAMP)
            """, [key, value])
        except Exception as e:
            logger.error("Error setting value", context={"key": key, "error": str(e)})

    def _create_status_bar(self):
        """Create status bar"""
        try:
            dpg.add_separator()
            with dpg.group(horizontal=True):
                dpg.add_text("WATCHLIST STATUS:", color=self.BLOOMBERG_GRAY)
                dpg.add_text("ACTIVE", color=self.BLOOMBERG_GREEN)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("POSITIONS:", color=self.BLOOMBERG_GRAY)
                dpg.add_text(f"{len(self.watchlist)}", color=self.BLOOMBERG_YELLOW,
                             tag=self.get_tag("status_positions"))
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("AUTO-UPDATE:", color=self.BLOOMBERG_GRAY)
                status_text = "ON" if self.auto_update else "OFF"
                status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
                dpg.add_text(status_text, color=status_color, tag=self.get_tag("auto_status"))
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("SELECTED:", color=self.BLOOMBERG_GRAY)
                dpg.add_text("None", color=self.BLOOMBERG_GRAY, tag=self.get_tag("selected_ticker_text"))
        except Exception as e:
            logger.error("Error creating status bar", context={"error": str(e)}, exc_info=True)

    def reset_watchlist(self, sender, app_data):
        """Reset watchlist to default"""
        try:
            with operation("Reset watchlist"):
                self.watchlist.clear()

                # Clear database tables
                if self.db_connection:
                    self.db_connection.execute("DELETE FROM watchlist")
                    self.db_connection.execute("DELETE FROM settings")
                    self.db_connection.execute("DELETE FROM price_history")

                self.initialize_sample_watchlist()
                self.update_display()

                # Clear cache
                self.calculate_portfolio_metrics.cache_clear()

                logger.info("Watchlist reset to default")
        except Exception as e:
            logger.error("Error resetting watchlist", context={"error": str(e)}, exc_info=True)

    def portfolio_report(self, sender, app_data):
        """Generate portfolio report"""
        try:
            with operation("Generate portfolio report"):
                total_value, total_pnl, day_pnl = self.calculate_portfolio_metrics()

                report = [
                    "\n=== PORTFOLIO REPORT ===",
                    f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
                    f"Total Positions: {len(self.watchlist)}",
                    f"Total Value: ${total_value:,.2f}",
                    f"Unrealized P&L: ${total_pnl:+,.2f}",
                    f"Day P&L: ${day_pnl:+,.2f}",
                    "========================\n"
                ]

                report_text = "\n".join(report)
                print(report_text)

                logger.info("Portfolio report generated",
                            context={"total_value": total_value, "positions": len(self.watchlist)})

        except Exception as e:
            logger.error("Error generating portfolio report", context={"error": str(e)}, exc_info=True)

    def redirect_to_equity_research(self, ticker: str):
        """Redirect to Equity Research tab with selected ticker - MINIMAL IMPLEMENTATION"""
        try:
            logger.info("Redirecting to Equity Research tab", context={"ticker": ticker})

            # Check if main_app exists and has the required method
            if self.main_app and hasattr(self.main_app, 'switch_to_tab_with_ticker'):
                success = self.main_app.switch_to_tab_with_ticker("Equity Research", ticker)
                if success:
                    logger.info(f"Successfully redirected to Equity Research with {ticker}")
                else:
                    logger.warning(f"Failed to redirect to Equity Research with {ticker}")
            else:
                logger.warning("Cannot redirect: main_app not properly configured")

        except Exception as e:
            logger.error("Error redirecting to Equity Research tab",
                         context={"ticker": ticker, "error": str(e)}, exc_info=True)

    # ============================================================================
    # CORE WATCHLIST METHODS
    # ============================================================================

    @monitor_performance
    def add_ticker_to_watchlist(self, ticker: str, quantity: float, avg_price: float, alert_price: Optional[float]):
        """Add a ticker to the watchlist with optimized data structure"""
        try:
            with self._update_lock:
                current_price = avg_price if avg_price > 0 else 100.0
                current_time = datetime.datetime.now().strftime("%H:%M:%S")

                # Pre-calculate values to avoid repeated calculations
                total_value = current_price * quantity
                unrealized_pnl = (current_price - avg_price) * quantity if avg_price > 0 else 0

                self.watchlist[ticker] = {
                    "quantity": quantity,
                    "avg_price": avg_price,
                    "alert_price": alert_price,
                    "current_price": current_price,
                    "change_1d": random.uniform(-5, 5),
                    "change_pct_1d": random.uniform(-3, 3),
                    "change_pct_7d": random.uniform(-8, 8),
                    "change_pct_30d": random.uniform(-20, 20),
                    "last_updated": current_time,
                    "total_value": round(total_value, 2),
                    "unrealized_pnl": round(unrealized_pnl, 2)
                }

                # Clear cache and update
                self.calculate_portfolio_metrics.cache_clear()
                self.save_watchlist_to_database()
                self.update_display()

                # Fetch real price if yfinance is available
                if HAS_YFINANCE:
                    threading.Thread(target=self.fetch_single_price, args=(ticker,), daemon=True).start()

        except Exception as e:
            logger.error("Error adding ticker to watchlist",
                         context={"ticker": ticker, "error": str(e)}, exc_info=True)

    def remove_ticker_from_watchlist(self, ticker: str):
        """Remove a ticker from the watchlist"""
        try:
            with self._update_lock:
                if ticker in self.watchlist:
                    del self.watchlist[ticker]

                # Remove from database
                if self.db_connection:
                    self.db_connection.execute("DELETE FROM watchlist WHERE ticker = ?", [ticker])

                # Clear cache and update display
                self.calculate_portfolio_metrics.cache_clear()
                self.update_display()

                # Clear selection
                self.selected_ticker = None
                selected_tag = self.get_tag("selected_ticker_text")
                if dpg.does_item_exist(selected_tag):
                    dpg.set_value(selected_tag, "None")

                logger.info("Ticker removed from watchlist", context={"ticker": ticker})

        except Exception as e:
            logger.error("Error removing ticker from watchlist",
                         context={"ticker": ticker, "error": str(e)}, exc_info=True)

    def fetch_single_price(self, ticker: str):
        """Fetch price for a single ticker with caching and retry logic"""
        if not HAS_YFINANCE:
            return

        # Check cache first
        current_time = time.time()
        if ticker in self._price_cache:
            cached_price, cached_time = self._price_cache[ticker]
            if current_time - cached_time < 60:  # Use cached price for 1 minute
                return

        retries = 0
        while retries < MAX_RETRIES:
            try:
                with operation(f"Fetch price for {ticker}"):
                    stock = yf.Ticker(ticker)
                    data = stock.history(period="30d", interval="1d")

                    if data.empty:
                        raise ValueError("No data available")

                    last_price = float(data["Close"].iloc[-1])

                    # Cache the price
                    self._price_cache[ticker] = (last_price, current_time)

                    # Store price in database
                    if self.db_connection:
                        self.db_connection.execute("""
                            INSERT INTO price_history (ticker, price, timestamp) 
                            VALUES (?, ?, CURRENT_TIMESTAMP)
                        """, [ticker, last_price])

                    # Calculate changes
                    if len(data) >= 2:
                        prev_price = float(data["Close"].iloc[-2])
                        change = last_price - prev_price
                        change_pct = (change / prev_price * 100) if abs(prev_price) > 1e-10 else 0.0
                        change_pct = self._round_percentage(change_pct)
                    else:
                        change = 0.0
                        change_pct = 0.0

                    # Update watchlist data atomically
                    with self._update_lock:
                        if ticker in self.watchlist:
                            data_entry = self.watchlist[ticker]
                            avg_price = data_entry["avg_price"]
                            quantity = data_entry["quantity"]

                            data_entry.update({
                                "current_price": round(last_price, 2),
                                "change_1d": round(change, 2),
                                "change_pct_1d": round(change_pct, 2),
                                "last_updated": datetime.datetime.now().strftime("%H:%M:%S"),
                                "total_value": round(last_price * quantity, 2),
                                "unrealized_pnl": self._round_currency((last_price - avg_price) * quantity) if abs(avg_price) > 1e-10 else 0.0
                            })

                            # Schedule display update if not already pending
                            if not self._display_update_pending:
                                self._display_update_pending = True
                                # Use threading timer for delayed batch update
                                threading.Timer(0.5, self._batch_update_display).start()

                    logger.debug("Price fetched successfully",
                                 context={"ticker": ticker, "price": last_price})
                    return

            except Exception as e:
                retries += 1
                logger.warning(f"Error fetching price for {ticker} (attempt {retries})",
                               context={"error": str(e)})
                if retries < MAX_RETRIES:
                    time.sleep(REQUEST_DELAY * retries)  # Exponential backoff

        logger.error(f"Failed to fetch price for {ticker} after {MAX_RETRIES} attempts")

    def _batch_update_display(self):
        """Batch update display to reduce UI refresh frequency"""
        try:
            self._display_update_pending = False
            self.calculate_portfolio_metrics.cache_clear()
            self.update_display()
        except Exception as e:
            logger.error("Error in batch display update", context={"error": str(e)})

    @monitor_performance
    def refresh_all_prices_sync(self):
        """Refresh all prices synchronously with rate limiting"""
        if not self.watchlist:
            return

        try:
            with operation("Refresh all prices"):
                tickers = list(self.watchlist.keys())
                logger.info("Starting price refresh", context={"ticker_count": len(tickers)})

                for i, ticker in enumerate(tickers):
                    self.fetch_single_price(ticker)

                    # Rate limiting - avoid overwhelming the API
                    if i < len(tickers) - 1:  # Don't sleep after the last request
                        time.sleep(REQUEST_DELAY)

                self.save_watchlist_to_database()
                logger.info("Price refresh completed")

        except Exception as e:
            logger.error("Error refreshing all prices", context={"error": str(e)}, exc_info=True)

    @monitor_performance
    def update_watchlist_data(self):
        """Update watchlist data with simulated changes - fixed precision"""
        try:
            with self._update_lock:
                current_time = datetime.datetime.now().strftime("%H:%M:%S")

                ticker_count = len(self.watchlist)
                if ticker_count == 0:
                    return

                # Generate random changes with proper bounds
                change_factors = [1 + random.uniform(-PRICE_CHANGE_LIMIT, PRICE_CHANGE_LIMIT)
                                  for _ in range(ticker_count)]

                for i, ticker in enumerate(self.watchlist):
                    data = self.watchlist[ticker]

                    # Apply price movement with precision
                    old_price = data['current_price']
                    new_price = self._round_currency(old_price * change_factors[i])
                    new_change_1d = self._round_currency(new_price - old_price)

                    # Calculate percentage change with protection
                    new_change_pct_1d = 0.0
                    if abs(old_price) > 1e-10:
                        new_change_pct_1d = self._round_percentage((new_change_1d / old_price) * 100)

                    # Recalculate derived values with precision
                    quantity = data['quantity']
                    avg_price = data['avg_price']
                    new_total_value = self._round_currency(new_price * quantity)
                    new_unrealized_pnl = 0.0
                    if abs(avg_price) > 1e-10:
                        new_unrealized_pnl = self._round_currency((new_price - avg_price) * quantity)

                    # Update weekly/monthly changes more realistically (don't accumulate)
                    # Base changes on the current day's performance
                    base_change = new_change_pct_1d
                    weekly_factor = random.uniform(3.0, 7.0)  # Weekly is typically 3-7x daily
                    monthly_factor = random.uniform(8.0, 15.0)  # Monthly is typically 8-15x daily

                    new_change_pct_7d = self._round_percentage(base_change * weekly_factor + random.uniform(-2, 2))
                    new_change_pct_30d = self._round_percentage(base_change * monthly_factor + random.uniform(-5, 5))

                    # Update all values atomically
                    self.watchlist[ticker].update({
                        'current_price': new_price,
                        'change_1d': new_change_1d,
                        'change_pct_1d': new_change_pct_1d,
                        'change_pct_7d': new_change_pct_7d,
                        'change_pct_30d': new_change_pct_30d,
                        'last_updated': current_time,
                        'total_value': new_total_value,
                        'unrealized_pnl': new_unrealized_pnl
                    })

                self._update_time_displays(current_time)
                logger.debug("Watchlist data updated with precision", context={"ticker_count": ticker_count})

        except Exception as e:
            logger.error("Error updating watchlist data", context={"error": str(e)}, exc_info=True)

    def _update_time_displays(self, current_time: str):
        """Update time displays efficiently"""
        time_tags = [
            ("last_update_time", current_time),
            ("watchlist_time", datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
        ]

        for tag_name, time_value in time_tags:
            tag = self.get_tag(tag_name)
            if dpg.does_item_exist(tag):
                dpg.set_value(tag, time_value)

    @monitor_performance
    def update_display(self):
        """Update all display elements with optimizations"""
        try:
            with operation("Update display"):
                # Update table
                self.populate_watchlist_table()

                # Update portfolio metrics
                total_value, total_pnl, day_pnl = self.calculate_portfolio_metrics()

                # Update metrics display with pre-calculated colors
                metrics_updates = [
                    ("total_value_text", f"TOTAL VALUE: ${total_value:,.2f}", self.BLOOMBERG_WHITE),
                    ("total_pnl_text", f"UNREALIZED P&L: ${total_pnl:+,.2f}",
                     self.BLOOMBERG_GREEN if total_pnl >= 0 else self.BLOOMBERG_RED),
                    ("day_pnl_text", f"DAY P&L: ${day_pnl:+,.2f}",
                     self.BLOOMBERG_GREEN if day_pnl >= 0 else self.BLOOMBERG_RED),
                    ("positions_count", f"POSITIONS: {len(self.watchlist)}", self.BLOOMBERG_YELLOW),
                    ("status_positions", f"{len(self.watchlist)}", self.BLOOMBERG_YELLOW)
                ]

                for tag_name, text, color in metrics_updates:
                    tag = self.get_tag(tag_name)
                    if dpg.does_item_exist(tag):
                        dpg.set_value(tag, text)
                        if color:
                            dpg.configure_item(tag, color=color)

                # Update top holdings
                self.update_top_holdings(total_value)

        except Exception as e:
            logger.error("Error updating display", context={"error": str(e)}, exc_info=True)

    def start_auto_update(self):
        """Start auto-update thread with improved error handling"""

        def update_loop():
            logger.info("Auto-update loop started")

            while self.auto_update:
                try:
                    time.sleep(UPDATE_INTERVAL)

                    if not self.auto_update:
                        break

                    # Check if enough time has passed since last update
                    current_time = time.time()
                    if current_time - self._last_update_time < UPDATE_INTERVAL - 0.5:
                        continue

                    self._last_update_time = current_time

                    if HAS_YFINANCE:
                        self.refresh_all_prices_sync()
                    else:
                        self.update_watchlist_data()
                        self.update_display()

                except Exception as e:
                    logger.error("Error in auto-update loop", context={"error": str(e)}, exc_info=True)
                    # Continue loop despite errors
                    time.sleep(UPDATE_INTERVAL)

            logger.info("Auto-update loop stopped")

        if self.auto_update and not self.refresh_running:
            self.refresh_running = True
            update_thread = threading.Thread(target=update_loop, daemon=True,
                                             name=f"WatchlistUpdate-{self.instance_id}")
            update_thread.start()
            logger.info("Auto-update thread started", context={"thread_name": update_thread.name})

    @monitor_performance
    def cleanup(self):
        """Clean up resources with comprehensive error handling"""
        try:
            logger.info("Starting watchlist tab cleanup")

            # Stop auto-update
            self.auto_update = False
            self.refresh_running = False

            # Save current state before cleanup
            self.save_watchlist_to_database()

            # Close database connection
            if self.db_connection:
                self.db_connection.close()
                self.db_connection = None

            # Clear caches
            self.get_tag.cache_clear()
            self.calculate_portfolio_metrics.cache_clear()
            self._price_cache.clear()

            # Clean up UI elements with unique tags
            self.cleanup_existing_items()

            # Clear data structures
            with self._update_lock:
                self.watchlist.clear()
                self.selected_ticker = None

            logger.info("Watchlist tab cleanup completed successfully")

        except Exception as e:
            logger.error("Error during cleanup", context={"error": str(e)}, exc_info=True)

    def initialize_sample_watchlist(self):
        """Initialize with optimized sample watchlist data"""
        sample_stocks = {
            "AAPL": {"quantity": 100, "avg_price": 150.25},
            "MSFT": {"quantity": 50, "avg_price": 305.80},
            "GOOGL": {"quantity": 25, "avg_price": 2650.40},
            "TSLA": {"quantity": 75, "avg_price": 220.15},
            "AMZN": {"quantity": 30, "avg_price": 3280.75},
            "NVDA": {"quantity": 40, "avg_price": 450.60},
            "META": {"quantity": 60, "avg_price": 315.90},
            "NFLX": {"quantity": 20, "avg_price": 485.30}
        }

        # Pre-generate random values to avoid multiple random calls
        random_changes = [random.uniform(-0.15, 0.15) for _ in range(len(sample_stocks))]
        random_1d = [random.uniform(-0.03, 0.03) for _ in range(len(sample_stocks))]
        random_7d = [random.uniform(-8, 8) for _ in range(len(sample_stocks))]
        random_30d = [random.uniform(-20, 20) for _ in range(len(sample_stocks))]

        current_time = datetime.datetime.now().strftime("%H:%M:%S")

        for i, (ticker, data) in enumerate(sample_stocks.items()):
            avg_price = data["avg_price"]
            current_price = avg_price * (1 + random_changes[i])
            change_1d = current_price * random_1d[i]
            change_pct_1d = (change_1d / current_price) * 100
            quantity = data["quantity"]

            self.watchlist[ticker] = {
                "quantity": quantity,
                "avg_price": avg_price,
                "alert_price": None,
                "current_price": round(current_price, 2),
                "change_1d": round(change_1d, 2),
                "change_pct_1d": round(change_pct_1d, 2),
                "change_pct_7d": round(random_7d[i], 2),
                "change_pct_30d": round(random_30d[i], 2),
                "last_updated": current_time,
                "total_value": round(current_price * quantity, 2),
                "unrealized_pnl": round((current_price - avg_price) * quantity, 2)
            }

        # Save to database
        self.save_watchlist_to_database()

    @lru_cache(maxsize=128)
    def get_tag(self, base_name: str) -> str:
        """Generate unique tag for this instance - cached for performance"""
        return f"{base_name}_{self.instance_id}"

    def create_content(self):
        """Create Bloomberg-style watchlist terminal layout"""
        logger.info("Creating watchlist UI content")

        try:
            with operation("UI Content Creation"):
                # Delete any existing items with potential duplicate tags
                self.cleanup_existing_items()

                # Create UI components in logical order
                self._create_header()
                self._create_control_panel()
                self._create_main_area()
                self._create_status_bar()

                # Start auto-update after UI is ready
                if self.auto_update:
                    self.start_auto_update()

            logger.info("Watchlist UI content created successfully")

        except Exception as e:
            logger.error("Error creating watchlist content", context={"error": str(e)}, exc_info=True)
            # Fallback minimal content
            dpg.add_text("WATCHLIST TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(f"Error loading watchlist: {e}")

    def _create_header(self):
        """Create top header bar"""
        with dpg.group(horizontal=True):
            dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("WATCHLIST TERMINAL", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_input_text(default_value="", hint="Add Symbol", width=150,
                               tag=self.get_tag("add_symbol_input"), uppercase=True)
            dpg.add_input_text(default_value="", hint="Qty", width=80,
                               tag=self.get_tag("add_qty_input"), decimal=True)
            dpg.add_input_text(default_value="", hint="Avg Price", width=100,
                               tag=self.get_tag("add_price_input"), decimal=True)
            dpg.add_input_text(default_value="", hint="Alert Price", width=100,
                               tag=self.get_tag("add_alert_input"), decimal=True)
            dpg.add_button(label="ADD", width=60, callback=self.add_to_watchlist_callback)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                         tag=self.get_tag("watchlist_time"))
        dpg.add_separator()

    def _create_control_panel(self):
        """Create control panel"""
        with dpg.group(horizontal=True):
            dpg.add_button(label="REFRESH", width=80, callback=self.refresh_all_callback)
            dpg.add_button(label="REMOVE", width=80, callback=self.remove_ticker_callback)
            dpg.add_button(label="AUTO ON", tag=self.get_tag("auto_toggle_btn"), width=80,
                           callback=self.toggle_auto_update)
            dpg.add_button(label="CHART", width=80, callback=self.show_chart_callback)
            dpg.add_button(label="INFO", width=80, callback=self.show_info_callback)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LAST UPDATE:", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%H:%M:%S'),
                         tag=self.get_tag("last_update_time"), color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LIVE", color=self.BLOOMBERG_GREEN)
        dpg.add_separator()

    def _create_main_area(self):
        """Create main area with watchlist and summary panels"""
        with dpg.group(horizontal=True):
            self.create_watchlist_panel()
            self.create_summary_panel()

    def cleanup_existing_items(self):
        """Clean up any existing items that might cause tag conflicts"""
        cleanup_tags = [
            "add_symbol_input", "add_qty_input", "add_price_input", "add_alert_input",
            "watchlist_time", "auto_toggle_btn", "last_update_time", "watchlist_table",
            "total_value_text", "total_pnl_text", "day_pnl_text", "positions_count",
            "top_holdings_group", "status_positions", "auto_status", "selected_ticker_text"
        ]

        for tag in cleanup_tags:
            unique_tag = self.get_tag(tag)
            if dpg.does_item_exist(unique_tag):
                try:
                    dpg.delete_item(unique_tag)
                except Exception as e:
                    logger.debug("Failed to delete item", context={"tag": unique_tag, "error": str(e)})

    def create_watchlist_panel(self):
        """Create watchlist table panel"""
        with dpg.child_window(width=1000, height=550, border=True):
            dpg.add_text("PORTFOLIO WATCHLIST", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Watchlist table
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           borders_innerV=True, borders_outerV=True,
                           scrollY=True, height=480, tag=self.get_tag("watchlist_table")):
                # Define columns with fixed widths for performance
                columns = [
                    ("Symbol", 80), ("Qty", 70), ("Avg Price", 90), ("Current", 90),
                    ("Chg", 70), ("Chg%", 70), ("7D%", 70), ("30D%", 70),
                    ("Total Value", 110), ("P&L", 90), ("Alert", 80)
                ]

                for label, width in columns:
                    dpg.add_table_column(label=label, width_fixed=True, init_width_or_weight=width)

                # Populate table
                self.populate_watchlist_table()

    def view_selected_details_callback(self, sender, app_data):
        """Callback for view selected details button"""
        if not self.selected_ticker:
            logger.warning("View details failed: no ticker selected")
            return

        self.redirect_to_equity_research(self.selected_ticker)

    def create_summary_panel(self):
        """Create portfolio summary panel"""
        with dpg.child_window(width=450, height=550, border=True):
            dpg.add_text("PORTFOLIO SUMMARY", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Portfolio metrics
            total_value, total_pnl, day_pnl = self.calculate_portfolio_metrics()

            dpg.add_text(f"TOTAL VALUE: ${total_value:,.2f}", color=self.BLOOMBERG_WHITE,
                         tag=self.get_tag("total_value_text"))

            pnl_color = self.BLOOMBERG_GREEN if total_pnl >= 0 else self.BLOOMBERG_RED
            dpg.add_text(f"UNREALIZED P&L: ${total_pnl:+,.2f}", color=pnl_color,
                         tag=self.get_tag("total_pnl_text"))

            day_color = self.BLOOMBERG_GREEN if day_pnl >= 0 else self.BLOOMBERG_RED
            dpg.add_text(f"DAY P&L: ${day_pnl:+,.2f}", color=day_color,
                         tag=self.get_tag("day_pnl_text"))

            dpg.add_text(f"POSITIONS: {len(self.watchlist)}", color=self.BLOOMBERG_YELLOW,
                         tag=self.get_tag("positions_count"))

            dpg.add_separator()

            # Top holdings
            dpg.add_text("TOP HOLDINGS", color=self.BLOOMBERG_YELLOW)

            with dpg.group(tag=self.get_tag("top_holdings_group")):
                self.update_top_holdings(total_value)

            dpg.add_separator()

            # Market alerts
            dpg.add_text("MARKET ALERTS", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("System: Ready", color=self.BLOOMBERG_GREEN)
            if HAS_YFINANCE:
                dpg.add_text("Data: Live Feed", color=self.BLOOMBERG_GREEN)
            else:
                dpg.add_text("Data: Simulated", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Status: Active", color=self.BLOOMBERG_WHITE)

            dpg.add_separator()

            # Quick actions
            dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_YELLOW)
            dpg.add_button(
                label="VIEW SELECTED DETAILS",
                width=-1,
                callback=self.view_selected_details_callback
            )
            dpg.add_spacer(height=5)
            dpg.add_button(label="EXPORT PORTFOLIO", width=-1, callback=self.export_portfolio)
            dpg.add_spacer(height=5)
            dpg.add_button(label="RESET WATCHLIST", width=-1, callback=self.reset_watchlist)
            dpg.add_spacer(height=5)
            dpg.add_button(label="PORTFOLIO REPORT", width=-1, callback=self.portfolio_report)

    def update_top_holdings(self, total_value: float):
        """Update top holdings display with performance optimization - FIXED"""
        try:
            holdings_tag = self.get_tag("top_holdings_group")
            if not dpg.does_item_exist(holdings_tag):
                logger.warning(f"Holdings group does not exist: {holdings_tag}")
                return

            # Clear existing holdings efficiently with error handling
            try:
                children = dpg.get_item_children(holdings_tag, slot=1)
                if children:
                    for child in children:
                        try:
                            if dpg.does_item_exist(child):
                                dpg.delete_item(child)
                        except Exception as delete_error:
                            logger.debug(f"Failed to delete child item {child}: {delete_error}")
            except Exception as clear_error:
                logger.debug(f"Error clearing holdings: {clear_error}")

            # Add new holdings - limit to top 6 for performance
            if total_value > 0 and self.watchlist:
                try:
                    sorted_holdings = sorted(
                        self.watchlist.items(),
                        key=lambda x: x[1]["total_value"],
                        reverse=True
                    )[:6]

                    for ticker, data in sorted_holdings:
                        try:
                            # Verify parent still exists before adding
                            if not dpg.does_item_exist(holdings_tag):
                                logger.warning(f"Holdings group disappeared: {holdings_tag}")
                                break

                            percentage = (data['total_value'] / total_value) * 100
                            dpg.add_text(
                                f"{ticker}: ${data['total_value']:,.0f} ({percentage:.1f}%)",
                                color=self.BLOOMBERG_WHITE,
                                parent=holdings_tag
                            )
                        except Exception as add_error:
                            logger.debug(f"Failed to add holding text for {ticker}: {add_error}")
                            continue

                except Exception as sort_error:
                    logger.error(f"Error sorting holdings: {sort_error}")

        except Exception as e:
            logger.error(f"Error in update_top_holdings: {e}", exc_info=True)

    def create_status_bar(self):
        """Create status bar"""
        with dpg.group(horizontal=True):
            dpg.add_text("WATCHLIST STATUS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ACTIVE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("POSITIONS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text(f"{len(self.watchlist)}", color=self.BLOOMBERG_YELLOW,
                         tag=self.get_tag("status_positions"))
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("AUTO-UPDATE:", color=self.BLOOMBERG_GRAY)
            status_text = "ON" if self.auto_update else "OFF"
            status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
            dpg.add_text(status_text, color=status_color, tag=self.get_tag("auto_status"))
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SELECTED:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("None", color=self.BLOOMBERG_GRAY, tag=self.get_tag("selected_ticker_text"))

    @monitor_performance
    def populate_watchlist_table(self):
        """Populate the watchlist table with current data - optimized with clickable symbols"""
        table_tag = None

        try:
            table_tag = self.get_tag("watchlist_table")
            if not dpg.does_item_exist(table_tag):
                logger.warning("Watchlist table does not exist", context={"table_tag": table_tag})
                return

            # Verify table is still valid before proceeding
            try:
                dpg.get_item_info(table_tag)
            except Exception as info_error:
                logger.warning(f"Table tag invalid: {table_tag}, error: {info_error}")
                return

            # Clear existing table data efficiently
            try:
                children = dpg.get_item_children(table_tag, slot=1)
                if children:
                    for child in children:
                        try:
                            if dpg.does_item_exist(child):
                                dpg.delete_item(child)
                        except Exception as delete_error:
                            logger.debug("Failed to delete table child",
                                         context={"child": child, "error": str(delete_error)})
            except Exception as clear_error:
                logger.debug(f"Error clearing table: {clear_error}")

            # Sort tickers once for better performance
            sorted_tickers = sorted(self.watchlist.keys())

            # Add rows with pre-calculated colors
            for ticker in sorted_tickers:
                try:
                    # Verify table still exists before each row
                    if not dpg.does_item_exist(table_tag):
                        logger.warning(f"Table disappeared during population: {table_tag}")
                        break

                    data = self.watchlist[ticker]

                    with dpg.table_row(parent=table_tag):
                        try:
                            # Symbol - make it selectable and clickable for Equity Research redirection
                            symbol_selectable = dpg.add_selectable(
                                label=ticker,
                                span_columns=False,
                                callback=self.on_ticker_selected,
                                user_data=ticker  # Pass ticker as user_data
                            )

                            # Add tooltip to indicate it's clickable and will redirect
                            try:
                                with dpg.tooltip(symbol_selectable):
                                    dpg.add_text(f"Click {ticker} to view detailed analysis")
                                    dpg.add_text("→ Opens in Equity Research tab", color=self.BLOOMBERG_YELLOW)
                            except Exception as tooltip_error:
                                logger.debug(f"Failed to add tooltip for {ticker}: {tooltip_error}")

                        except Exception as symbol_error:
                            logger.debug(f"Failed to add symbol selectable for {ticker}: {symbol_error}")
                            # Fallback to simple text
                            try:
                                dpg.add_text(ticker, color=self.BLOOMBERG_WHITE)
                            except Exception as fallback_error:
                                logger.debug(f"Failed to add fallback text for {ticker}: {fallback_error}")

                        # Quantity
                        try:
                            dpg.add_text(f"{data['quantity']:,}", color=self.BLOOMBERG_WHITE)
                        except Exception as qty_error:
                            logger.debug(f"Failed to add quantity for {ticker}: {qty_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # Average price
                        try:
                            dpg.add_text(f"${data['avg_price']:.2f}", color=self.BLOOMBERG_GRAY)
                        except Exception as avg_error:
                            logger.debug(f"Failed to add avg_price for {ticker}: {avg_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # Current price
                        try:
                            dpg.add_text(f"${data['current_price']:.2f}", color=self.BLOOMBERG_WHITE)
                        except Exception as price_error:
                            logger.debug(f"Failed to add current_price for {ticker}: {price_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # 1D Change
                        try:
                            change_color = self.BLOOMBERG_GREEN if data['change_1d'] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"${data['change_1d']:+.2f}", color=change_color)
                        except Exception as change_error:
                            logger.debug(f"Failed to add change_1d for {ticker}: {change_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # 1D Change %
                        try:
                            pct_color = self.BLOOMBERG_GREEN if data['change_pct_1d'] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_pct_1d']:+.2f}%", color=pct_color)
                        except Exception as pct_error:
                            logger.debug(f"Failed to add change_pct_1d for {ticker}: {pct_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # 7D Change %
                        try:
                            pct_7d_color = self.BLOOMBERG_GREEN if data['change_pct_7d'] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_pct_7d']:+.2f}%", color=pct_7d_color)
                        except Exception as pct_7d_error:
                            logger.debug(f"Failed to add change_pct_7d for {ticker}: {pct_7d_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # 30D Change %
                        try:
                            pct_30d_color = self.BLOOMBERG_GREEN if data['change_pct_30d'] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"{data['change_pct_30d']:+.2f}%", color=pct_30d_color)
                        except Exception as pct_30d_error:
                            logger.debug(f"Failed to add change_pct_30d for {ticker}: {pct_30d_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # Total value
                        try:
                            dpg.add_text(f"${data['total_value']:,.2f}", color=self.BLOOMBERG_WHITE)
                        except Exception as total_error:
                            logger.debug(f"Failed to add total_value for {ticker}: {total_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # P&L (Profit & Loss)
                        try:
                            pnl_color = self.BLOOMBERG_GREEN if data['unrealized_pnl'] >= 0 else self.BLOOMBERG_RED
                            dpg.add_text(f"${data['unrealized_pnl']:+,.2f}", color=pnl_color)
                        except Exception as pnl_error:
                            logger.debug(f"Failed to add unrealized_pnl for {ticker}: {pnl_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                        # Alert price
                        try:
                            alert_text = f"${data['alert_price']:.2f}" if data['alert_price'] else ""
                            dpg.add_text(alert_text, color=self.BLOOMBERG_YELLOW)
                        except Exception as alert_error:
                            logger.debug(f"Failed to add alert_price for {ticker}: {alert_error}")
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)

                except Exception as row_error:
                    logger.debug(f"Error adding row for {ticker}: {row_error}")
                    # Try to add an error row if possible
                    try:
                        if dpg.does_item_exist(table_tag):
                            with dpg.table_row(parent=table_tag):
                                dpg.add_text(f"ERROR: {ticker}", color=self.BLOOMBERG_RED)
                                for _ in range(10):  # Fill remaining columns
                                    dpg.add_text("--", color=self.BLOOMBERG_GRAY)
                    except Exception as error_row_error:
                        logger.debug(f"Failed to create error row for {ticker}: {error_row_error}")
                    continue

            logger.debug("Watchlist table populated successfully",
                         context={"ticker_count": len(sorted_tickers)})

        except Exception as e:
            logger.error("Error populating watchlist table", context={"error": str(e)}, exc_info=True)

            # Create error row if table exists and is valid
            if table_tag and dpg.does_item_exist(table_tag):
                try:
                    with dpg.table_row(parent=table_tag):
                        dpg.add_text("CRITICAL ERROR", color=self.BLOOMBERG_RED)
                        for _ in range(10):  # Fill remaining columns
                            dpg.add_text("--", color=self.BLOOMBERG_GRAY)
                except Exception as fallback_error:
                    logger.error("Failed to create critical error row", context={"error": str(fallback_error)})

    def on_ticker_selected(self, sender, app_data, user_data):
        """Handle ticker selection with proper data passing"""
        ticker = user_data  # Get ticker from user_data
        if ticker:
            logger.debug(f"Ticker selected via callback: {ticker}")
            self.select_ticker(ticker)
        else:
            logger.warning("No ticker data received in callback")

    @lru_cache(maxsize=1)
    def calculate_portfolio_metrics(self) -> Tuple[float, float, float]:
        """Calculate portfolio summary metrics with high precision"""
        try:
            if not self.watchlist:
                return 0.0, 0.0, 0.0

            # Use Decimal for high precision calculations
            total_value_decimal = Decimal('0')
            total_pnl_decimal = Decimal('0')
            day_pnl_decimal = Decimal('0')

            for ticker, data in self.watchlist.items():
                # Recalculate values to ensure consistency
                current_price = Decimal(str(data['current_price']))
                avg_price = Decimal(str(data['avg_price']))
                quantity = Decimal(str(data['quantity']))
                change_1d = Decimal(str(data['change_1d']))

                # Calculate with precision
                position_value = current_price * quantity
                position_pnl = (current_price - avg_price) * quantity
                position_day_pnl = change_1d * quantity

                total_value_decimal += position_value
                total_pnl_decimal += position_pnl
                day_pnl_decimal += position_day_pnl

            # Convert back to float with proper rounding
            total_value = self._round_currency(float(total_value_decimal))
            total_pnl = self._round_currency(float(total_pnl_decimal))
            day_pnl = self._round_currency(float(day_pnl_decimal))

            return total_value, total_pnl, day_pnl

        except Exception as e:
            logger.error("Error calculating portfolio metrics", context={"error": str(e)})
            return 0.0, 0.0, 0.0

    # ============================================================================
    # CALLBACK METHODS
    # ============================================================================

    def select_ticker(self, ticker: str):
        """Select a ticker from the table and redirect to Equity Research tab"""
        self.selected_ticker = ticker
        selected_tag = self.get_tag("selected_ticker_text")
        if dpg.does_item_exist(selected_tag):
            dpg.set_value(selected_tag, ticker)

        logger.debug("Ticker selected", context={"ticker": ticker})

        # Auto-redirect to Equity Research tab for detailed analysis
        self.redirect_to_equity_research(ticker)

    def add_to_watchlist_callback(self, sender, app_data):
        """Callback for add to watchlist button"""
        try:
            with operation("Add ticker to watchlist"):
                # Get input values
                ticker = dpg.get_value(self.get_tag("add_symbol_input")).strip().upper()
                quantity_str = dpg.get_value(self.get_tag("add_qty_input")).strip()
                avg_price_str = dpg.get_value(self.get_tag("add_price_input")).strip()
                alert_str = dpg.get_value(self.get_tag("add_alert_input")).strip()

                if not ticker:
                    logger.warning("Add ticker failed: empty ticker symbol")
                    return

                if ticker in self.watchlist:
                    logger.warning("Add ticker failed: ticker already exists", context={"ticker": ticker})
                    return

                # Parse numeric values with defaults
                try:
                    quantity = float(quantity_str) if quantity_str else 1.0
                    avg_price = float(avg_price_str) if avg_price_str else 100.0
                    alert_price = float(alert_str) if alert_str else None
                except ValueError:
                    logger.warning("Add ticker failed: invalid numeric values")
                    return

                # Add to watchlist
                self.add_ticker_to_watchlist(ticker, quantity, avg_price, alert_price)

                # Clear input fields
                for tag in ["add_symbol_input", "add_qty_input", "add_price_input", "add_alert_input"]:
                    dpg.set_value(self.get_tag(tag), "")

                logger.info("Ticker added to watchlist", context={"ticker": ticker, "quantity": quantity})

        except Exception as e:
            logger.error("Error in add_to_watchlist_callback", context={"error": str(e)}, exc_info=True)

    def remove_ticker_callback(self, sender, app_data):
        """Callback for remove ticker button"""
        if not self.selected_ticker:
            logger.warning("Remove ticker failed: no ticker selected")
            return

        with operation("Remove ticker from watchlist"):
            self.remove_ticker_from_watchlist(self.selected_ticker)

    def refresh_all_callback(self, sender, app_data):
        """Callback for refresh all button"""
        logger.info("Manual refresh requested")

        with operation("Manual refresh all"):
            if HAS_YFINANCE:
                threading.Thread(target=self.refresh_all_prices_sync, daemon=True).start()
            else:
                self.update_watchlist_data()
                self.update_display()

    def toggle_auto_update(self, sender, app_data):
        """Toggle auto-update on/off"""
        self.auto_update = not self.auto_update

        # Update UI elements
        auto_btn_tag = self.get_tag("auto_toggle_btn")
        if dpg.does_item_exist(auto_btn_tag):
            dpg.set_item_label(auto_btn_tag, "AUTO ON" if self.auto_update else "AUTO OFF")

        auto_status_tag = self.get_tag("auto_status")
        if dpg.does_item_exist(auto_status_tag):
            status_text = "ON" if self.auto_update else "OFF"
            status_color = self.BLOOMBERG_GREEN if self.auto_update else self.BLOOMBERG_RED
            dpg.set_value(auto_status_tag, status_text)
            dpg.configure_item(auto_status_tag, color=status_color)

        # Start/stop auto-update
        if self.auto_update and not self.refresh_running:
            self.start_auto_update()
        elif not self.auto_update:
            self.refresh_running = False

        logger.info("Auto-update toggled", context={"enabled": self.auto_update})

    def show_chart_callback(self, sender, app_data):
        """Callback for show chart button"""
        if not self.selected_ticker:
            logger.warning("Chart request failed: no ticker selected")
            return

        logger.info("Chart requested", context={"ticker": self.selected_ticker})
        # Chart functionality would be implemented here

    def show_info_callback(self, sender, app_data):
        """Callback for show info button"""
        if not self.selected_ticker:
            logger.warning("Info request failed: no ticker selected")
            return

        logger.info("Info requested", context={"ticker": self.selected_ticker})
        # Info functionality would be implemented here

    @monitor_performance
    def export_portfolio(self, sender, app_data):
        """Export portfolio to DuckDB file or CSV"""
        try:
            with operation("Export portfolio"):
                if not self.db_connection:
                    logger.error("Database connection not available for export")
                    return

                # Export to CSV file
                timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
                csv_filename = f"portfolio_export_{timestamp}.csv"

                # Create export query
                export_query = """
                    COPY (
                        SELECT 
                            ticker,
                            quantity,
                            avg_price,
                            current_price,
                            (current_price - avg_price) * quantity as unrealized_pnl,
                            current_price * quantity as total_value,
                            change_pct_1d,
                            change_pct_7d,
                            change_pct_30d,
                            last_updated,
                            created_at
                        FROM watchlist
                        ORDER BY total_value DESC
                    ) TO ? WITH (HEADER, DELIMITER ',')
                """

                self.db_connection.execute(export_query, [csv_filename])

                # Also create a summary file
                total_value, total_pnl, day_pnl = self.calculate_portfolio_metrics()

                summary_filename = f"portfolio_summary_{timestamp}.txt"
                with open(summary_filename, 'w') as f:
                    f.write("=== PORTFOLIO EXPORT SUMMARY ===\n")
                    f.write(f"Export Date: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    f.write(f"Total Positions: {len(self.watchlist)}\n")
                    f.write(f"Total Value: ${total_value:,.2f}\n")
                    f.write(f"Unrealized P&L: ${total_pnl:+,.2f}\n")
                    f.write(f"Day P&L: ${day_pnl:+,.2f}\n")
                    f.write("================================\n")
                    f.write(f"Detailed data exported to: {csv_filename}\n")

                logger.info("Portfolio exported successfully",
                            context={"csv_file": csv_filename, "summary_file": summary_filename})

        except Exception as e:
            logger.error("Error exporting portfolio", context={"error": str(e)}, exc_info=True)

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle resize events - optimized to do minimal work"""
        # Fixed layout for stability - no dynamic resizing
        pass