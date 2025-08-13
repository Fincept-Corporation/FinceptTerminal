# -*- coding: utf-8 -*-
# portfolio_business.py

import json
import os
import datetime
import threading
import time
import random
import csv
import io
from collections import defaultdict, deque
from tkinter import filedialog
import tkinter as tk
import yfinance as yf
from concurrent.futures import ThreadPoolExecutor, as_completed
import requests
from functools import lru_cache
from pathlib import Path

# Import the new logger
from fincept_terminal.Utils.Logging.logger import logger, operation, monitor_performance

def get_portfolio_config_path():
    """Get portfolio configuration file path in .fincept directory"""
    config_dir = Path.home() / '.fincept'
    config_dir.mkdir(exist_ok=True)
    return config_dir / 'portfolio_settings.json'

PORTFOLIO_CONFIG_FILE = get_portfolio_config_path()


class PortfolioBusinessLogic:
    """Business logic for Portfolio Management - separated from UI"""

    def __init__(self):
        # Price management - optimized
        self.price_cache = {}
        self.last_price_update = {}
        self.price_fetch_errors = {}
        self.daily_change_cache = {}
        self.previous_close_cache = {}
        self.refresh_thread = None
        self.refresh_running = False
        self.price_update_interval = 3600  # 1 hour in seconds
        self.initial_price_fetch_done = False

        # Portfolio data
        self.portfolios = self.load_portfolios()
        self.current_portfolio = None

        # Country suffix mapping for yfinance - cached
        self.country_suffixes = self._get_country_suffixes()

        # Performance optimizations
        self._portfolio_value_cache = {}
        self._portfolio_investment_cache = {}
        self._cache_timeout = 30  # seconds
        self._last_cache_update = {}

        # CSV import data
        self.csv_data = None
        self.csv_headers = []
        self.column_mapping = {}
        self.csv_preview_data = []
        self.csv_file_path = None

        # Sample market data initialization
        self.initialize_sample_data()

        # Start initial price fetch
        self.fetch_initial_prices()

    def _get_country_suffixes(self):
        """Get country suffix mapping - cached"""
        return {
            "India": ".NS",
            "United States": "",
            "United Kingdom": ".L",
            "Germany": ".DE",
            "Japan": ".T",
            "Australia": ".AX",
            "Canada": ".TO",
            "France": ".PA",
            "Hong Kong": ".HK",
            "South Korea": ".KS"
        }

    def initialize_sample_data(self):
        """Initialize sample portfolio data for demonstration"""
        if not self.portfolios:
            # Create sample portfolios
            self.portfolios = {
                "Tech Growth": {
                    "AAPL": {"quantity": 50, "avg_price": 150.25, "last_added": "2024-01-15"},
                    "MSFT": {"quantity": 30, "avg_price": 280.75, "last_added": "2024-01-10"},
                    "GOOGL": {"quantity": 25, "avg_price": 125.50, "last_added": "2024-01-05"},
                    "NVDA": {"quantity": 20, "avg_price": 450.30, "last_added": "2024-01-20"}
                },
                "Dividend Income": {
                    "JNJ": {"quantity": 100, "avg_price": 160.80, "last_added": "2024-01-12"},
                    "PG": {"quantity": 75, "avg_price": 145.20, "last_added": "2024-01-08"},
                    "KO": {"quantity": 150, "avg_price": 58.90, "last_added": "2024-01-18"}
                }
            }
            self.save_portfolios()

    @monitor_performance
    def fetch_initial_prices(self):
        """Fetch initial prices for all stocks in portfolios"""
        threading.Thread(target=self._fetch_initial_prices_worker, daemon=True).start()

    def _fetch_initial_prices_worker(self):
        """Background worker to fetch initial prices"""
        try:
            with operation("initial_price_fetch"):
                logger.info("Fetching initial stock prices...")

                # Collect all unique symbols from all portfolios
                all_symbols = set()
                for portfolio in self.portfolios.values():
                    for symbol in portfolio.keys():
                        all_symbols.add(symbol)

                if not all_symbols:
                    logger.info("No stocks found in portfolios")
                    self.initial_price_fetch_done = True
                    return

                # Fetch prices in batches for better performance
                self._fetch_prices_batch(list(all_symbols))

                self.initial_price_fetch_done = True
                logger.info(f"Initial price fetch completed for {len(all_symbols)} symbols",
                            context={'symbols_count': len(all_symbols)})

        except Exception as e:
            logger.error(f"Error in initial price fetch: {e}", exc_info=True)
            self.initial_price_fetch_done = True

    @monitor_performance
    def _fetch_prices_batch(self, symbols):
        """Fetch prices for a batch of symbols - optimized"""
        max_workers = min(10, len(symbols))  # Adaptive worker count

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            # Submit fetch tasks
            future_to_symbol = {
                executor.submit(self._fetch_single_price, symbol): symbol
                for symbol in symbols
            }

            # Collect results with timeout
            for future in as_completed(future_to_symbol, timeout=60):
                symbol = future_to_symbol[future]
                try:
                    price = future.result(timeout=30)
                    if price is not None:
                        self.price_cache[symbol] = price
                        self.last_price_update[symbol] = datetime.datetime.now()
                        self.price_fetch_errors.pop(symbol, None)  # Clear any previous errors
                        logger.debug(f"Price updated: {symbol} = ${price:.2f}")
                    else:
                        self.price_fetch_errors[symbol] = "No price data available"
                        logger.warning(f"No price data available for {symbol}")

                except Exception as e:
                    self.price_fetch_errors[symbol] = str(e)
                    logger.error(f"Error fetching price for {symbol}: {e}")

                # Small delay to avoid overwhelming the API
                time.sleep(0.1)

    def _fetch_single_price(self, symbol):
        """Fetch price and daily change data for a single symbol using yfinance - optimized"""
        try:
            # Create ticker object
            ticker = yf.Ticker(symbol)

            # Get current data with timeout
            info = ticker.info

            # Try different price fields in order of preference
            price_fields = [
                'regularMarketPrice',
                'currentPrice',
                'previousClose',
                'regularMarketPreviousClose'
            ]

            current_price = None
            previous_close = None

            # Get current price
            for field in price_fields:
                if field in info and info[field] is not None:
                    current_price = float(info[field])
                    if current_price > 0:  # Valid price
                        break

            # Get previous close
            prev_close_fields = ['regularMarketPreviousClose', 'previousClose']
            for field in prev_close_fields:
                if field in info and info[field] is not None:
                    previous_close = float(info[field])
                    if previous_close > 0:
                        break

            # If we couldn't get from info, try historical data
            if current_price is None or previous_close is None:
                hist = ticker.history(period="2d", interval="1d")
                if not hist.empty and len(hist) >= 2:
                    if current_price is None:
                        current_price = float(hist['Close'].iloc[-1])
                    if previous_close is None:
                        previous_close = float(hist['Close'].iloc[-2])
                elif not hist.empty:
                    if current_price is None:
                        current_price = float(hist['Close'].iloc[-1])
                    if previous_close is None:
                        previous_close = current_price  # Use same price as fallback

            # Calculate daily change
            if current_price is not None and previous_close is not None and previous_close > 0:
                daily_change = current_price - previous_close
                daily_change_pct = (daily_change / previous_close) * 100

                # Store in caches
                self.previous_close_cache[symbol] = previous_close
                self.daily_change_cache[symbol] = {
                    'change': daily_change,
                    'change_pct': daily_change_pct
                }

                return current_price

            return current_price

        except Exception as e:
            logger.error(f"Error fetching price for {symbol}: {e}")
            return None

    @lru_cache(maxsize=128)
    def get_daily_change(self, symbol):
        """Get today's change for a symbol - cached"""
        if symbol in self.daily_change_cache:
            return self.daily_change_cache[symbol]

        # Calculate from current and previous close if available
        current_price = self.get_current_price(symbol)
        previous_close = self.previous_close_cache.get(symbol)

        if current_price and previous_close and previous_close > 0:
            daily_change = current_price - previous_close
            daily_change_pct = (daily_change / previous_close) * 100
            return {
                'change': daily_change,
                'change_pct': daily_change_pct
            }

        # Fallback to zero change
        return {'change': 0.0, 'change_pct': 0.0}

    def calculate_portfolio_daily_change(self, portfolio_name):
        """Calculate total daily change for a portfolio - cached"""
        cache_key = f"daily_change_{portfolio_name}"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_value_cache:
                return self._portfolio_value_cache[cache_key]

        portfolio = self.portfolios.get(portfolio_name, {})
        total_change = 0.0
        total_previous_value = 0.0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                current_price = self.get_current_price(symbol)
                previous_close = self.previous_close_cache.get(symbol, current_price)

                # Calculate change for this holding
                current_value = quantity * current_price
                previous_value = quantity * previous_close
                holding_change = current_value - previous_value

                total_change += holding_change
                total_previous_value += previous_value

        # Calculate percentage change
        change_pct = (total_change / total_previous_value * 100) if total_previous_value > 0 else 0.0

        result = {
            'change': total_change,
            'change_pct': change_pct
        }

        # Cache result
        self._portfolio_value_cache[cache_key] = result
        self._last_cache_update[cache_key] = current_time

        return result

    def calculate_total_daily_change(self):
        """Calculate total daily change across all portfolios - cached"""
        cache_key = "total_daily_change"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_value_cache:
                return self._portfolio_value_cache[cache_key]

        total_change = 0.0
        total_previous_value = 0.0

        for portfolio_name in self.portfolios.keys():
            portfolio_change = self.calculate_portfolio_daily_change(portfolio_name)
            portfolio_current_value = self.calculate_portfolio_value(portfolio_name)
            portfolio_previous_value = portfolio_current_value - portfolio_change['change']

            total_change += portfolio_change['change']
            total_previous_value += portfolio_previous_value

        # Calculate percentage change
        change_pct = (total_change / total_previous_value * 100) if total_previous_value > 0 else 0.0

        result = {
            'change': total_change,
            'change_pct': change_pct
        }

        # Cache result
        self._portfolio_value_cache[cache_key] = result
        self._last_cache_update[cache_key] = current_time

        return result

    @lru_cache(maxsize=256)
    def get_current_price(self, symbol):
        """Get current price from cache or return fallback price - cached"""
        # Return cached price if available
        if symbol in self.price_cache:
            return self.price_cache[symbol]

        # If symbol has an error, use average price as fallback
        if symbol in self.price_fetch_errors:
            # Try to find avg_price from portfolios
            for portfolio in self.portfolios.values():
                if symbol in portfolio and isinstance(portfolio[symbol], dict):
                    avg_price = portfolio[symbol].get('avg_price', 100)
                    logger.warning(f"Using avg_price ${avg_price:.2f} for {symbol} (fetch error)")
                    return avg_price

        # Default fallback
        logger.debug(f"Using default price $100.00 for {symbol}")
        return 100.0

    # Calculation methods - optimized with caching
    def calculate_portfolio_value(self, portfolio_name):
        """Calculate current portfolio value - cached"""
        cache_key = f"value_{portfolio_name}"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_value_cache:
                return self._portfolio_value_cache[cache_key]

        portfolio = self.portfolios.get(portfolio_name, {})
        total_value = 0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                current_price = self.get_current_price(symbol)
                total_value += quantity * current_price

        # Cache result
        self._portfolio_value_cache[cache_key] = total_value
        self._last_cache_update[cache_key] = current_time

        return total_value

    def calculate_portfolio_investment(self, portfolio_name):
        """Calculate total portfolio investment - cached"""
        cache_key = f"investment_{portfolio_name}"
        current_time = time.time()

        # Check cache
        if (cache_key in self._last_cache_update and
                current_time - self._last_cache_update[cache_key] < self._cache_timeout):
            if cache_key in self._portfolio_investment_cache:
                return self._portfolio_investment_cache[cache_key]

        portfolio = self.portfolios.get(portfolio_name, {})
        total_investment = 0

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get('quantity', 0)
                avg_price = data.get('avg_price', 0)
                total_investment += quantity * avg_price

        # Cache result
        self._portfolio_investment_cache[cache_key] = total_investment
        self._last_cache_update[cache_key] = current_time

        return total_investment

    def get_portfolio_summary(self):
        """Get comprehensive portfolio summary"""
        total_portfolios = len(self.portfolios)
        total_investment = sum(self.calculate_portfolio_investment(name) for name in self.portfolios.keys())
        total_value = sum(self.calculate_portfolio_value(name) for name in self.portfolios.keys())
        total_pnl = total_value - total_investment
        total_pnl_pct = (total_pnl / total_investment * 100) if total_investment > 0 else 0

        # Get today's change efficiently
        total_daily_change = self.calculate_total_daily_change()
        today_change = total_daily_change['change']
        today_change_pct = total_daily_change['change_pct']

        return {
            'total_portfolios': total_portfolios,
            'total_investment': total_investment,
            'total_value': total_value,
            'total_pnl': total_pnl,
            'total_pnl_pct': total_pnl_pct,
            'today_change': today_change,
            'today_change_pct': today_change_pct
        }

    def get_portfolio_breakdown(self):
        """Get detailed breakdown of all portfolios"""
        breakdown = []
        total_value = sum(self.calculate_portfolio_value(name) for name in self.portfolios.keys())

        for portfolio_name, stocks in self.portfolios.items():
            # Calculate portfolio metrics efficiently
            portfolio_investment = self.calculate_portfolio_investment(portfolio_name)
            portfolio_value = self.calculate_portfolio_value(portfolio_name)
            portfolio_pnl = portfolio_value - portfolio_investment
            portfolio_pnl_pct = (portfolio_pnl / portfolio_investment * 100) if portfolio_investment > 0 else 0
            allocation_pct = (portfolio_value / total_value * 100) if total_value > 0 else 0

            # Get real daily change for this portfolio
            portfolio_daily_change = self.calculate_portfolio_daily_change(portfolio_name)
            today_change = portfolio_daily_change['change']
            today_change_pct = portfolio_daily_change['change_pct']

            breakdown.append({
                'name': portfolio_name,
                'stocks_count': len(stocks),
                'investment': portfolio_investment,
                'value': portfolio_value,
                'pnl': portfolio_pnl,
                'pnl_pct': portfolio_pnl_pct,
                'today_change': today_change,
                'today_change_pct': today_change_pct,
                'allocation_pct': allocation_pct
            })

        return breakdown

    def get_portfolio_holdings(self, portfolio_name):
        """Get detailed holdings for a specific portfolio"""
        if portfolio_name not in self.portfolios:
            return []

        portfolio = self.portfolios[portfolio_name]
        portfolio_value = self.calculate_portfolio_value(portfolio_name)
        holdings = []

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get("quantity", 0)
                avg_price = data.get("avg_price", 0)
                original_symbol = data.get("original_symbol", symbol)
                current_price = self.get_current_price(symbol)

                market_value = quantity * current_price
                investment = quantity * avg_price
                gain_loss = market_value - investment
                gain_loss_pct = (gain_loss / investment * 100) if investment > 0 else 0
                weight_pct = (market_value / portfolio_value * 100) if portfolio_value > 0 else 0

                holdings.append({
                    'symbol': symbol,
                    'original_symbol': original_symbol,
                    'quantity': quantity,
                    'avg_price': avg_price,
                    'current_price': current_price,
                    'market_value': market_value,
                    'investment': investment,
                    'gain_loss': gain_loss,
                    'gain_loss_pct': gain_loss_pct,
                    'weight_pct': weight_pct
                })

        return holdings

    # Portfolio management methods
    def create_portfolio(self, name, description=""):
        """Create a new portfolio"""
        if not name:
            raise ValueError("Please enter a portfolio name.")

        if name in self.portfolios:
            raise ValueError("Portfolio name already exists.")

        self.portfolios[name] = {}
        self.save_portfolios()
        self._clear_portfolio_cache()
        return True

    def create_portfolio_with_stock(self, name, symbol, quantity, price, description=""):
        """Create portfolio and add first stock"""
        if not name:
            raise ValueError("Please enter a portfolio name.")

        if name in self.portfolios:
            raise ValueError("Portfolio name already exists.")

        # Validate stock data if provided
        if symbol and quantity is not None and price is not None:
            try:
                quantity = float(quantity)
                price = float(price)

                # Create portfolio with first stock
                self.portfolios[name] = {
                    symbol: {
                        "quantity": quantity,
                        "avg_price": price,
                        "last_added": datetime.datetime.now().strftime("%Y-%m-%d")
                    }
                }

                # Fetch real price for the symbol
                threading.Thread(target=lambda: self._fetch_single_price_and_update(symbol),
                                 daemon=True).start()

            except ValueError:
                raise ValueError("Invalid quantity or price values.")
        else:
            # Create empty portfolio
            self.portfolios[name] = {}

        self.save_portfolios()
        self._clear_portfolio_cache()
        return True

    def add_stock_to_portfolio(self, portfolio_name, symbol, quantity, price):
        """Add stock to existing portfolio"""
        if not portfolio_name or portfolio_name not in self.portfolios:
            raise ValueError("Invalid portfolio selected.")

        if not symbol or quantity is None or price is None:
            raise ValueError("Please fill in all fields.")

        try:
            quantity = float(quantity)
            price = float(price)
        except ValueError:
            raise ValueError("Invalid quantity or price values.")

        # Add or update stock
        if symbol in self.portfolios[portfolio_name]:
            # Update existing stock
            existing = self.portfolios[portfolio_name][symbol]
            current_qty = existing["quantity"]
            current_avg = existing["avg_price"]

            new_qty = current_qty + quantity
            new_avg = ((current_avg * current_qty) + (price * quantity)) / new_qty

            self.portfolios[portfolio_name][symbol] = {
                "quantity": new_qty,
                "avg_price": round(new_avg, 2),
                "last_added": datetime.datetime.now().strftime("%Y-%m-%d")
            }
        else:
            # Add new stock
            self.portfolios[portfolio_name][symbol] = {
                "quantity": quantity,
                "avg_price": price,
                "last_added": datetime.datetime.now().strftime("%Y-%m-%d")
            }

        # Update price cache
        self.price_cache[symbol] = price * (1 + random.uniform(-0.05, 0.05))

        self.save_portfolios()
        self._clear_portfolio_cache()

        # Fetch real price for new symbol
        threading.Thread(target=lambda: self._fetch_single_price_and_update(symbol), daemon=True).start()
        return True

    def remove_stock_from_portfolio(self, portfolio_name, symbol):
        """Remove stock from portfolio"""
        if not portfolio_name or portfolio_name not in self.portfolios:
            raise ValueError("Portfolio not found")

        if symbol not in self.portfolios[portfolio_name]:
            raise ValueError("Stock not found in portfolio")

        del self.portfolios[portfolio_name][symbol]
        self.save_portfolios()
        self._clear_portfolio_cache()
        return True

    def delete_portfolio(self, portfolio_name):
        """Delete the specified portfolio"""
        if portfolio_name not in self.portfolios:
            raise ValueError("Portfolio not found")

        # Remove from portfolios
        del self.portfolios[portfolio_name]

        # Save changes
        self.save_portfolios()
        self._clear_portfolio_cache()

        # Clear current portfolio if it was the deleted one
        if self.current_portfolio == portfolio_name:
            self.current_portfolio = None

        return True

    # CSV Import methods
    @monitor_performance
    def select_csv_file(self):
        """Open file dialog to select CSV file - optimized"""
        try:
            # Create a temporary tkinter root window (hidden)
            root = tk.Tk()
            root.withdraw()  # Hide the main window

            # Open file dialog
            file_path = filedialog.askopenfilename(
                title="Select Portfolio CSV File",
                filetypes=[("CSV files", "*.csv"), ("All files", "*.*")]
            )

            root.destroy()  # Clean up

            if file_path:
                self.csv_file_path = file_path
                filename = os.path.basename(file_path)
                return filename
            else:
                return None

        except Exception as e:
            logger.error(f"Error selecting CSV file: {e}", exc_info=True)
            raise Exception("Error selecting file")

    @monitor_performance
    def analyze_csv_file(self):
        """Analyze the selected CSV file and return column info"""
        try:
            if not hasattr(self, 'csv_file_path'):
                raise ValueError("Please select a CSV file first")

            with operation("csv_analysis"):
                # Read CSV file efficiently
                with open(self.csv_file_path, 'r', encoding='utf-8') as file:
                    # Try to detect delimiter
                    sample = file.read(1024)
                    file.seek(0)

                    sniffer = csv.Sniffer()
                    delimiter = sniffer.sniff(sample).delimiter

                    reader = csv.reader(file, delimiter=delimiter)
                    rows = list(reader)

                if not rows:
                    raise ValueError("CSV file is empty")

                self.csv_headers = rows[0]
                self.csv_data = rows[1:] if len(rows) > 1 else []

                return {
                    'headers': self.csv_headers,
                    'row_count': len(self.csv_data),
                    'columns_count': len(self.csv_headers)
                }

        except Exception as e:
            logger.error(f"Error analyzing CSV: {e}", exc_info=True)
            raise Exception(f"Error analyzing CSV: {str(e)}")

    @lru_cache(maxsize=64)
    def auto_detect_column(self, field_type):
        """Auto-detect CSV column based on common naming patterns - cached"""
        field_patterns = {
            "symbol": ["symbol", "instrument", "stock", "ticker", "scrip", "name"],
            "quantity": ["quantity", "qty", "shares", "units", "holding"],
            "avg_price": ["avg", "average", "cost", "price", "purchase", "buy"],
            "current_price": ["ltp", "current", "market", "last", "trading"],
            "investment": ["invested", "investment", "total_cost", "amount"],
            "current_value": ["current_value", "market_value", "value", "cur"],
            "pnl": ["pnl", "p&l", "profit", "loss", "gain", "net"]
        }

        patterns = field_patterns.get(field_type, [])

        for header in self.csv_headers:
            header_lower = header.lower().replace(" ", "_").replace(".", "").replace("-", "_")
            for pattern in patterns:
                if pattern in header_lower:
                    return header

        return ""

    @monitor_performance
    def preview_import(self, column_mapping, country_suffix):
        """Preview the import with current column mapping"""
        try:
            with operation("import_preview"):
                # Validate required mappings
                required_mappings = ["symbol", "quantity", "avg_price"]
                self.column_mapping = {}

                for field in required_mappings:
                    mapped_column = column_mapping.get(field)
                    if not mapped_column:
                        raise ValueError(f"Please map the required field: {field}")
                    self.column_mapping[field] = mapped_column

                # Get optional mappings
                optional_mappings = ["current_price", "investment", "current_value", "pnl"]
                for field in optional_mappings:
                    mapped_column = column_mapping.get(field)
                    if mapped_column:
                        self.column_mapping[field] = mapped_column

                # Process preview data efficiently
                self.csv_preview_data = []

                for row in self.csv_data[:10]:  # Preview first 10 rows
                    if len(row) >= len(self.csv_headers):
                        row_dict = dict(zip(self.csv_headers, row))

                        # Extract mapped data
                        symbol = str(row_dict.get(self.column_mapping["symbol"], "")).strip()
                        if not symbol:
                            continue

                        # Add country suffix for yfinance compatibility
                        if country_suffix and not symbol.endswith(country_suffix):
                            symbol_yf = symbol + country_suffix
                        else:
                            symbol_yf = symbol

                        try:
                            quantity = float(row_dict.get(self.column_mapping["quantity"], 0))
                            avg_price = float(row_dict.get(self.column_mapping["avg_price"], 0))
                        except (ValueError, TypeError):
                            continue

                        preview_item = {
                            "original_symbol": symbol,
                            "yfinance_symbol": symbol_yf,
                            "quantity": quantity,
                            "avg_price": avg_price
                        }

                        self.csv_preview_data.append(preview_item)

                return {
                    'preview_data': self.csv_preview_data,
                    'valid_rows': len(self.csv_preview_data),
                    'total_investment': sum(item["quantity"] * item["avg_price"] for item in self.csv_preview_data)
                }

        except Exception as e:
            logger.error(f"Error creating preview: {e}", exc_info=True)
            raise Exception(f"Error creating preview: {str(e)}")

    @monitor_performance
    def import_csv_portfolio(self, portfolio_name):
        """Import the CSV data as a new portfolio"""
        try:
            with operation("csv_portfolio_import"):
                if not portfolio_name.strip():
                    raise ValueError("Please enter a portfolio name")

                if portfolio_name in self.portfolios:
                    raise ValueError("Portfolio name already exists")

                if not self.csv_preview_data:
                    raise ValueError("No preview data available. Please analyze CSV first")

                # Create new portfolio efficiently
                new_portfolio = {}

                for item in self.csv_preview_data:
                    symbol = item["yfinance_symbol"]
                    new_portfolio[symbol] = {
                        "quantity": item["quantity"],
                        "avg_price": item["avg_price"],
                        "last_added": datetime.datetime.now().strftime("%Y-%m-%d"),
                        "original_symbol": item["original_symbol"]  # Keep original symbol for reference
                    }

                    # Update price cache with avg_price as initial current price
                    self.price_cache[symbol] = item["avg_price"] * (1 + random.uniform(-0.05, 0.05))

                # Add to portfolios
                self.portfolios[portfolio_name] = new_portfolio
                self.save_portfolios()

                # Clear cache for affected calculations
                self._clear_portfolio_cache()

                # Fetch real prices for imported symbols
                imported_symbols = list(new_portfolio.keys())
                threading.Thread(target=lambda: self._fetch_prices_batch(imported_symbols), daemon=True).start()

                # Clear import data
                self.csv_data = None
                self.csv_headers = []
                self.column_mapping = {}
                self.csv_preview_data = []

                return {
                    'portfolio_name': portfolio_name,
                    'stocks_imported': len(new_portfolio),
                    'success': True
                }

        except Exception as e:
            logger.error(f"Error importing portfolio: {e}", exc_info=True)
            raise Exception(f"Error importing portfolio: {str(e)}")

    def _clear_portfolio_cache(self):
        """Clear portfolio calculation cache"""
        self._portfolio_value_cache.clear()
        self._portfolio_investment_cache.clear()
        self._last_cache_update.clear()
        # Clear LRU caches
        self.get_current_price.cache_clear()
        self.get_daily_change.cache_clear()

    def _fetch_single_price_and_update(self, symbol):
        """Fetch price for a single symbol and update cache - optimized"""
        try:
            with operation("fetch_single_price", context={'symbol': symbol}):
                price = self._fetch_single_price(symbol)
                if price is not None:
                    self.price_cache[symbol] = price
                    self.last_price_update[symbol] = datetime.datetime.now()
                    logger.debug(f"Updated price for {symbol}: ${price:.2f}")

                    # Clear cache for affected calculations
                    self._clear_portfolio_cache()
                else:
                    logger.warning(f"Could not fetch price for {symbol}")
        except Exception as e:
            logger.error(f"Error fetching price for {symbol}: {e}")

    # Price refresh methods
    def start_price_refresh_thread(self):
        """Start the auto price refresh thread"""
        if not self.refresh_running:
            self.refresh_running = True
            self.refresh_thread = threading.Thread(target=self._price_refresh_loop, daemon=True)
            self.refresh_thread.start()
            logger.info("Started hourly price refresh thread")

    def _price_refresh_loop(self):
        """Background thread for price refresh - runs every hour - optimized"""
        while self.refresh_running:
            try:
                # Wait for initial fetch to complete
                while not self.initial_price_fetch_done and self.refresh_running:
                    time.sleep(10)

                if not self.refresh_running:
                    break

                # Sleep for 1 hour (3600 seconds) - optimized with smaller checks
                for _ in range(0, self.price_update_interval, 10):
                    if not self.refresh_running:
                        break
                    time.sleep(10)

                if self.refresh_running:
                    logger.info("Hourly price update starting...")
                    self.refresh_all_prices_background()

            except Exception as e:
                logger.error(f"Error in price refresh loop: {e}")
                time.sleep(300)  # Wait 5 minutes before retrying

    @monitor_performance
    def refresh_all_prices_background(self):
        """Refresh all prices in background - optimized"""
        try:
            with operation("refresh_all_prices"):
                # Collect all unique symbols from all portfolios
                all_symbols = set()
                for portfolio in self.portfolios.values():
                    for symbol in portfolio.keys():
                        all_symbols.add(symbol)

                if not all_symbols:
                    return

                logger.info(f"Refreshing prices for {len(all_symbols)} symbols...",
                            context={'symbols_count': len(all_symbols)})

                # Fetch updated prices
                self._fetch_prices_batch(list(all_symbols))

                # Clear cache for affected calculations
                self._clear_portfolio_cache()

                logger.info("Price refresh completed")
                return True

        except Exception as e:
            logger.error(f"Error refreshing prices: {e}")
            return False

    def refresh_all_prices_now(self):
        """Refresh all prices immediately"""
        threading.Thread(target=self.refresh_all_prices_background, daemon=True).start()
        return True

    # Export methods
    @monitor_performance
    def export_portfolio_data(self):
        """Export portfolio data - optimized"""
        try:
            with operation("export_portfolio_data"):
                # Create CSV export data efficiently
                export_data = []
                export_data.append(
                    ["Portfolio", "Symbol", "Original_Symbol", "Quantity", "Avg_Price", "Current_Price", "Investment",
                     "Current_Value", "P&L", "P&L_%"])

                for portfolio_name, stocks in self.portfolios.items():
                    for symbol, data in stocks.items():
                        if isinstance(data, dict):
                            quantity = data.get("quantity", 0)
                            avg_price = data.get("avg_price", 0)
                            original_symbol = data.get("original_symbol", symbol)
                            current_price = self.get_current_price(symbol)

                            investment = quantity * avg_price
                            current_value = quantity * current_price
                            pnl = current_value - investment
                            pnl_pct = (pnl / investment * 100) if investment > 0 else 0

                            export_data.append([
                                portfolio_name, symbol, original_symbol, quantity, avg_price,
                                current_price, investment, current_value, pnl, pnl_pct
                            ])

                # Save to CSV file
                export_filename = f"portfolio_export_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
                with open(export_filename, 'w', newline='', encoding='utf-8') as csvfile:
                    writer = csv.writer(csvfile)
                    writer.writerows(export_data)

                return export_filename

        except Exception as e:
            logger.error(f"Error exporting portfolio data: {e}", exc_info=True)
            raise Exception("Error exporting portfolio data")

    # File operations - optimized
    @monitor_performance
    def load_portfolios(self):
        """Load portfolios from settings file - optimized"""
        if PORTFOLIO_CONFIG_FILE.exists():
            try:
                with operation("load_portfolios"):
                    with open(PORTFOLIO_CONFIG_FILE, "r") as file:
                        settings = json.load(file)
                        portfolios = settings.get("portfolios", {})

                        # Filter out 'watchlist' which is handled by watchlist tab
                        if "watchlist" in portfolios:
                            del portfolios["watchlist"]

                        return portfolios
            except (json.JSONDecodeError, IOError) as e:
                logger.error(f"Error loading portfolios: Corrupted portfolio_settings.json file - {e}")
                return {}
        return {}

    @monitor_performance
    def save_portfolios(self):
        """Save portfolios to settings file - optimized"""
        try:
            with operation("save_portfolios"):
                # Load existing settings
                settings = {}
                if PORTFOLIO_CONFIG_FILE.exists():
                    try:
                        with open(PORTFOLIO_CONFIG_FILE, "r") as file:
                            settings = json.load(file)
                    except json.JSONDecodeError:
                        settings = {}

                # Ensure portfolios section exists
                if "portfolios" not in settings:
                    settings["portfolios"] = {}

                # Update only the portfolio data (preserve watchlist)
                for portfolio_name, portfolio_data in self.portfolios.items():
                    settings["portfolios"][portfolio_name] = portfolio_data

                # Save back to file atomically
                temp_file = str(PORTFOLIO_CONFIG_FILE) + ".tmp"
                with open(temp_file, "w") as file:
                    json.dump(settings, file, indent=4)

                # Atomic rename
                import shutil
                shutil.move(temp_file, str(PORTFOLIO_CONFIG_FILE))

                logger.debug("Portfolios saved successfully")

        except Exception as e:
            logger.error(f"Error saving portfolios: {e}", exc_info=True)
            raise Exception(f"Error saving portfolios: {e}")

    @monitor_performance
    def cleanup(self):
        """Clean up portfolio business logic resources - optimized"""
        try:
            with operation("portfolio_business_cleanup"):
                logger.info("🧹 Cleaning up portfolio business logic...")
                self.refresh_running = False

                if hasattr(self, 'portfolios'):
                    self.save_portfolios()

                # Clear all data structures efficiently
                self.portfolios.clear()
                self.current_portfolio = None
                self.price_cache.clear()
                self.last_price_update.clear()
                self.price_fetch_errors.clear()
                self.daily_change_cache.clear()
                self.previous_close_cache.clear()

                # Clear CSV import data
                self.csv_data = None
                self.csv_headers = []
                self.column_mapping = {}
                self.csv_preview_data = []

                # Clear caches
                self._clear_portfolio_cache()

                # Clear LRU caches
                self.get_current_price.cache_clear()
                self.get_daily_change.cache_clear()
                self.auto_detect_column.cache_clear()

                logger.info("Portfolio business logic cleanup complete")

        except Exception as e:
            logger.error(f"Error in portfolio business cleanup: {e}", exc_info=True)