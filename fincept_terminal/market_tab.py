import dearpygui.dearpygui as dpg
import threading
import time
import yfinance as yf
from concurrent.futures import ThreadPoolExecutor, as_completed
from base_tab import BaseTab

# Assets data
ASSETS = {
    "Stock Indices": {
        "S&P 500": "^GSPC",
        "Nasdaq 100": "^NDX",
        "Dow Jones": "^DJI",
        "Russell 2000": "^RUT",
        "VIX (Volatility Index)": "^VIX",
        "FTSE 100 (UK)": "^FTSE",
        "DAX 40 (Germany)": "^GDAXI",
        "CAC 40 (France)": "^FCHI",
        "Nikkei 225 (Japan)": "^N225",
        "Hang Seng (Hong Kong)": "^HSI",
        "Shanghai Composite (China)": "000001.SS",
        "Sensex 30 (India)": "^BSESN",
        "Nifty 50 (India)": "^NSEI",
        "ASX 200 (Australia)": "^AXJO",
        "Euro Stoxx 50 (EU)": "^STOXX50E",
        "MSCI Emerging Markets": "EEM",
        "Brazil Bovespa": "^BVSP",
        "Mexico IPC": "^MXX",
        "South Korea KOSPI": "^KS11",
        "Taiwan TAIEX": "^TWII",
        "Singapore STI": "^STI",
    },
    "Forex": {
        "EUR/USD": "EURUSD=X",
        "GBP/USD": "GBPUSD=X",
        "USD/JPY": "USDJPY=X",
        "USD/CHF": "USDCHF=X",
        "USD/CAD": "USDCAD=X",
        "AUD/USD": "AUDUSD=X",
        "NZD/USD": "NZDUSD=X",
        "EUR/GBP": "EURGBP=X",
        "EUR/JPY": "EURJPY=X",
        "GBP/JPY": "GBPJPY=X",
        "CHF/JPY": "CHFJPY=X",
        "AUD/JPY": "AUDJPY=X",
        "NZD/JPY": "NZDJPY=X",
        "EUR/CHF": "EURCHF=X",
        "USD/CNY": "USDCNY=X",
        "USD/INR": "USDINR=X",
        "USD/SGD": "USDSGD=X",
        "USD/KRW": "USDKRW=X",
        "USD/HKD": "USDHKD=X",
        "USD/IDR": "USDIDR=X",
        "USD/THB": "USDTHB=X",
        "USD/MXN": "USDMXN=X",
        "USD/BRL": "USDBRL=X",
        "USD/ZAR": "USDZAR=X",
        "USD/RUB": "USDRUB=X",
        "EUR/AUD": "EURAUD=X",
        "EUR/CAD": "EURCAD=X",
        "GBP/AUD": "GBPAUD=X",
        "GBP/CAD": "GBPCAD=X",
        "AUD/CAD": "AUDCAD=X",
        "AUD/NZD": "AUDNZD=X",
        "CAD/JPY": "CADJPY=X",
        "SGD/JPY": "SGDJPY=X",
        "HKD/JPY": "HKDJPY=X",
    },
    "Commodities": {
        "Gold": "GC=F",
        "Silver": "SI=F",
        "Crude Oil (WTI)": "CL=F",
        "Brent Crude Oil": "BZ=F",
        "Natural Gas": "NG=F",
        "Copper": "HG=F",
        "Platinum": "PL=F",
        "Palladium": "PA=F",
        "Wheat": "ZW=F",
        "Corn": "ZC=F",
        "Soybeans": "ZS=F",
        "Coffee": "KC=F",
        "Cocoa": "CC=F",
        "Sugar": "SB=F",
        "Cotton": "CT=F",
        "Aluminum": "ALI=F",
        "Heating Oil": "HO=F",
        "Gasoline (RBOB)": "RB=F",
        "Live Cattle": "LE=F",
        "Lean Hogs": "HE=F",
        "Feeder Cattle": "GF=F",
        "Oats": "ZO=F",
        "Rice": "ZR=F",
        "Orange Juice": "OJ=F",
    },
    "Bonds": {
        "US 10Y Treasury": "^TNX",
        "US 30Y Treasury": "^TYX",
        "India 10Y Bond": "NIFTYGS10YR.NS",
        "Franklin Emerging Market Debt Opps": "FEMDX",
        "Motilal Oswal Nasdaq 100 Fd of Fd Reg Gr": "0P0001F0CL.BO",
        "FSITC GlobalAIRobotics and SmartAuto USD": "0P000184IC",
        "US Corporate Bonds (LQD)": "LQD",
        "US High Yield Bonds (HYG)": "HYG",
        "US Fed Funds Rate": "^IRX",
    },
    "ETFs": {
        "S&P 500 ETF (SPY)": "SPY",
        "Nasdaq 100 ETF (QQQ)": "QQQ",
        "Dow Jones ETF (DIA)": "DIA",
        "Emerging Markets ETF (EEM)": "EEM",
        "Gold ETF (GLD)": "GLD",
        "Technology ETF (XLK)": "XLK",
        "Energy ETF (XLE)": "XLE",
        "Financials ETF (XLF)": "XLF",
        "Healthcare ETF (XLV)": "XLV",
        "Real Estate ETF (VNQ)": "VNQ",
        "AI & Robotics ETF (BOTZ)": "BOTZ",
        "Clean Energy ETF (ICLN)": "ICLN",
        "EV & Battery ETF (LIT)": "LIT",
        "Cybersecurity ETF (HACK)": "HACK",
    },
    "Cryptocurrencies": {
        "Bitcoin": "BTC-USD",
        "Ethereum": "ETH-USD",
        "Binance Coin": "BNB-USD",
        "Solana": "SOL-USD",
        "XRP": "XRP-USD",
        "Dogecoin": "DOGE-USD",
        "Polygon (MATIC)": "MATIC-USD",
        "Chainlink": "LINK-USD",
        "Cardano": "ADA-USD",
        "Polkadot": "DOT-USD",
        "Avalanche": "AVAX-USD",
        "Litecoin": "LTC-USD",
        "Cosmos": "ATOM-USD",
        "Shiba Inu": "SHIB-USD",
        "Uniswap": "UNI-USD",
        "Algorand": "ALGO-USD",
        "Stellar": "XLM-USD",
        "Filecoin": "FIL-USD",
        "Aave": "AAVE-USD",
        "VeChain": "VET-USD",
        "Elrond": "EGLD-USD",
        "Decentraland": "MANA-USD",
        "Sandbox": "SAND-USD",
        "Axie Infinity": "AXS-USD",
        "ThorChain": "RUNE-USD",
        "Internet Computer": "ICP-USD",
        "Stacks": "STX-USD",
        "Arbitrum": "ARB-USD",
        "Optimism": "OP-USD",
        "Injective Protocol": "INJ-USD",
        "Rocket Pool": "RPL-USD",
    },
}


class MarketTab(BaseTab):
    """Market tab displaying world markets in a 3x2 grid layout with fast loading"""

    REFRESH_INTERVAL = 600  # Update every 10 minutes (600 seconds)
    MAX_WORKERS = 15  # Concurrent threads for faster data fetching

    def __init__(self, app):
        super().__init__(app)
        self.market_data = {}
        self.last_prices = {}
        self.is_updating = False
        self.update_thread = None
        self.stop_updates = False

    def get_label(self):
        return "📈 Markets"

    def create_content(self):
        """Create market dashboard content with safe 3x2 grid layout"""
        try:
            self.add_section_header("World Markets Dashboard")

            # Control panel
            self.create_control_panel()
            dpg.add_spacer(height=15)

            # Markets grid (3x2 layout)
            self.create_markets_grid()

            # Start background updates
            self.start_background_updates()

        except Exception as e:
            print(f"❌ Error creating market tab content: {e}")
            dpg.add_text("❌ Error loading markets tab", color=[255, 100, 100])
            dpg.add_text(f"Details: {str(e)}")

    def create_control_panel(self):
        """Create market control panel with 10-minute default"""
        try:
            with dpg.group(horizontal=True):
                dpg.add_button(label="🔄 Refresh Now", callback=self.manual_refresh)
                dpg.add_button(label="⏸️ Pause Updates", callback=self.toggle_updates, tag="update_toggle")
                dpg.add_combo(["10 minutes", "5 minutes", "15 minutes", "30 minutes"],
                              default_value="10 minutes", tag="refresh_interval",
                              callback=self.update_refresh_interval)
                dpg.add_checkbox(label="Auto-refresh", default_value=True, tag="auto_refresh")
                dpg.add_text("Last updated: Never", tag="last_update_time")
        except Exception as e:
            print(f"❌ Error creating control panel: {e}")

    def create_markets_grid(self):
        """Create markets grid using safe child windows and text layout"""
        try:
            categories = list(ASSETS.keys())

            # Calculate dimensions for 3x2 grid
            grid_width = self.app.usable_width
            grid_height = self.app.usable_height - 100

            cell_width = int(grid_width / 3) - 15
            cell_height = int(grid_height / 2) - 15

            # First row (3 columns)
            with dpg.group(horizontal=True):
                for i in range(3):
                    if i < len(categories):
                        self.create_market_section_safe(categories[i], cell_width, cell_height)

            dpg.add_spacer(height=10)

            # Second row (3 columns)
            with dpg.group(horizontal=True):
                for i in range(3, 6):
                    if i < len(categories):
                        self.create_market_section_safe(categories[i], cell_width, cell_height)

        except Exception as e:
            print(f"❌ Error creating markets grid: {e}")

    def create_market_section_safe(self, category, width, height):
        """Create market section with properly aligned columns like Textual"""
        try:
            tag_suffix = category.replace(' ', '-').replace('&', 'and').lower()

            with self.create_child_window(
                    tag=f"market_section_{tag_suffix}",
                    width=width,
                    height=height
            ):
                # Section header
                dpg.add_text(f"📊 {category}", color=[100, 200, 255])
                dpg.add_separator()
                dpg.add_spacer(height=5)

                # Create header row with fixed column positions
                with dpg.group(horizontal=True):
                    dpg.add_text("Asset".ljust(18), color=[150, 150, 255])
                    dpg.add_text("Symbol".ljust(8), color=[150, 150, 255])
                    dpg.add_text("Price".rjust(12), color=[150, 150, 255])
                    dpg.add_text("1D Chg".rjust(10), color=[150, 150, 255])
                    dpg.add_text("1D%".rjust(8), color=[150, 150, 255])
                    dpg.add_text("7D%".rjust(8), color=[150, 150, 255])
                    dpg.add_text("30D%".rjust(8), color=[150, 150, 255])

                dpg.add_separator()

                # Scrollable data area with monospace layout
                with dpg.child_window(height=height - 80, horizontal_scrollbar=True, border=False,
                                      tag=f"data_scroll_{tag_suffix}"):
                    # Data container
                    dpg.add_group(tag=f"data_container_{tag_suffix}")

                    # Loading indicator
                    dpg.add_text("🔄 Loading market data...", tag=f"loading_text_{tag_suffix}", color=[255, 255, 100])

        except Exception as e:
            print(f"❌ Error creating market section {category}: {e}")

    def fetch_single_asset(self, category, asset_name, ticker):
        """Optimized single asset fetching with timeout"""
        try:
            # Use shorter period and timeout for faster loading
            hist = yf.Ticker(ticker).history(period="2mo", timeout=8)

            if hist.empty:
                return category, asset_name, ticker, None, None, None, None, None

            latest_price = hist["Close"].iloc[-1]
            prev_1d = hist["Close"].iloc[-2] if len(hist) > 1 else latest_price
            prev_7d = hist["Close"].iloc[-7] if len(hist) > 7 else latest_price
            prev_30d = hist["Close"].iloc[-30] if len(hist) > 30 else latest_price

            change_1d = latest_price - prev_1d
            change_percent_1d = (change_1d / prev_1d * 100) if prev_1d else 0
            change_percent_7d = ((latest_price - prev_7d) / prev_7d * 100) if prev_7d else 0
            change_percent_30d = ((latest_price - prev_30d) / prev_30d * 100) if prev_30d else 0

            return category, asset_name, ticker, latest_price, change_1d, change_percent_1d, change_percent_7d, change_percent_30d

        except Exception as e:
            print(f"⚠️ Error fetching {ticker}: {e}")
            return category, asset_name, ticker, None, None, None, None, None

    def fetch_asset_data_parallel(self):
        """Fetch all market data using parallel processing for speed"""
        try:
            all_tasks = []

            # Create list of all assets to fetch
            for category, assets in ASSETS.items():
                for asset_name, ticker in assets.items():
                    all_tasks.append((category, asset_name, ticker))

            # Use ThreadPoolExecutor for parallel fetching
            results = []
            with ThreadPoolExecutor(max_workers=self.MAX_WORKERS) as executor:
                future_to_asset = {
                    executor.submit(self.fetch_single_asset, category, asset_name, ticker): (category, asset_name,
                                                                                             ticker)
                    for category, asset_name, ticker in all_tasks
                }

                for future in as_completed(future_to_asset):
                    try:
                        result = future.result(timeout=12)  # 12 second timeout per asset
                        results.append(result)
                    except Exception as e:
                        category, asset_name, ticker = future_to_asset[future]
                        print(f"⚠️ Timeout/Error for {ticker}: {e}")
                        results.append((category, asset_name, ticker, None, None, None, None, None))

            return results

        except Exception as e:
            print(f"❌ Error in parallel fetch: {e}")
            return []

    def update_market_data(self):
        """Fast parallel market data update"""
        if self.is_updating:
            return

        self.is_updating = True
        start_time = time.time()

        try:
            print("🚀 Fast-fetching market data...")

            # Fetch data in parallel
            rows = self.fetch_asset_data_parallel()

            # Group data by category
            category_data = {}
            for row in rows:
                category = row[0]
                if category not in category_data:
                    category_data[category] = []
                category_data[category].append(row)

            # Store data
            self.market_data = category_data

            # Update UI
            self.update_ui_data_safe()

            elapsed = time.time() - start_time
            print(f"✅ Market data updated in {elapsed:.2f} seconds")

        except Exception as e:
            print(f"❌ Error updating market data: {e}")
        finally:
            self.is_updating = False

    def update_ui_data_safe(self):
        """Safe UI update without problematic table operations"""
        try:
            # Update each category
            for category, data_rows in self.market_data.items():
                tag_suffix = category.replace(' ', '-').replace('&', 'and').lower()
                data_container_tag = f"data_container_{tag_suffix}"
                loading_text_tag = f"loading_text_{tag_suffix}"

                # Remove loading indicator
                if dpg.does_item_exist(loading_text_tag):
                    dpg.delete_item(loading_text_tag)

                # Clear existing data
                if dpg.does_item_exist(data_container_tag):
                    children = dpg.get_item_children(data_container_tag, slot=1)
                    for child in children:
                        try:
                            dpg.delete_item(child)
                        except:
                            pass

                    # Add new data rows (limit to 10 per category for performance)
                    for row_data in data_rows[:10]:
                        self.add_data_row_safe(data_container_tag, row_data)

            # Update timestamp
            current_time = time.strftime("%H:%M:%S")
            if dpg.does_item_exist("last_update_time"):
                dpg.set_value("last_update_time", f"Last updated: {current_time}")

        except Exception as e:
            print(f"❌ Error updating UI: {e}")

    def add_data_row_safe(self, container_tag, row_data):
        """Add data row with perfect column alignment like Textual"""
        try:
            category, asset_name, ticker, price, change_1d, change_percent_1d, change_percent_7d, change_percent_30d = row_data

            # Format values with consistent precision and alignment
            price_str = f"{price:,.2f}" if price is not None else "-"
            change_str = f"{change_1d:+,.2f}" if change_1d is not None else "-"
            change_percent_1d_str = f"{change_percent_1d:+.2f}%" if change_percent_1d is not None else "-"
            change_percent_7d_str = f"{change_percent_7d:+.2f}%" if change_percent_7d is not None else "-"
            change_percent_30d_str = f"{change_percent_30d:+.2f}%" if change_percent_30d is not None else "-"

            # Truncate and pad asset name to exactly 18 characters
            asset_display = (asset_name[:15] + "...") if len(asset_name) > 15 else asset_name
            asset_display = asset_display.ljust(18)

            # Pad ticker to exactly 8 characters
            ticker_display = ticker.ljust(8)

            # Right-align numeric columns for proper alignment
            price_display = price_str.rjust(12)
            change_display = change_str.rjust(10)
            percent_1d_display = change_percent_1d_str.rjust(8)
            percent_7d_display = change_percent_7d_str.rjust(8)
            percent_30d_display = change_percent_30d_str.rjust(8)

            # Determine colors
            change_color = [100, 255, 100] if change_1d and change_1d > 0 else [255, 100, 100]
            percent_1d_color = [100, 255, 100] if change_percent_1d and change_percent_1d > 0 else [255, 100, 100]
            percent_7d_color = [100, 255, 100] if change_percent_7d and change_percent_7d > 0 else [255, 100, 100]
            percent_30d_color = [100, 255, 100] if change_percent_30d and change_percent_30d > 0 else [255, 100, 100]

            # Create data row with monospace-like alignment
            with dpg.group(horizontal=True, parent=container_tag):
                dpg.add_text(asset_display, color=[255, 255, 255])
                dpg.add_text(ticker_display, color=[200, 200, 200])
                dpg.add_text(price_display, color=[255, 255, 255])
                dpg.add_text(change_display, color=change_color)
                dpg.add_text(percent_1d_display, color=percent_1d_color)
                dpg.add_text(percent_7d_display, color=percent_7d_color)
                dpg.add_text(percent_30d_display, color=percent_30d_color)

        except Exception as e:
            print(f"❌ Error adding data row: {e}")

    def start_background_updates(self):
        """Start optimized background updates every 10 minutes"""
        try:
            if self.update_thread is None or not self.update_thread.is_alive():
                self.stop_updates = False
                self.update_thread = threading.Thread(target=self.background_update_loop, daemon=True)
                self.update_thread.start()
                print("🚀 Background market updates started (10-minute intervals)")
        except Exception as e:
            print(f"❌ Error starting background updates: {e}")

    def background_update_loop(self):
        """Optimized background loop with 10-minute intervals"""
        try:
            # Initial update
            self.update_market_data()

            while not self.stop_updates:
                time.sleep(self.REFRESH_INTERVAL)

                if not self.stop_updates and dpg.does_item_exist("auto_refresh"):
                    if dpg.get_value("auto_refresh"):
                        self.update_market_data()
        except Exception as e:
            print(f"❌ Error in background update loop: {e}")

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle resize events safely"""
        try:
            grid_width = self.app.usable_width
            grid_height = self.app.usable_height - 100

            cell_width = int(grid_width / 3) - 15
            cell_height = int(grid_height / 2) - 15

            # Update market sections
            categories = list(ASSETS.keys())
            for i, category in enumerate(categories):
                tag_suffix = category.replace(' ', '-').replace('&', 'and').lower()
                window_tag = f"market_section_{tag_suffix}"
                scroll_tag = f"data_scroll_{tag_suffix}"

                if dpg.does_item_exist(window_tag):
                    dpg.configure_item(window_tag, width=cell_width, height=cell_height)

                if dpg.does_item_exist(scroll_tag):
                    dpg.configure_item(scroll_tag, height=cell_height - 80)

        except Exception as e:
            print(f"❌ Market tab resize error: {e}")

    # Callback methods
    def manual_refresh(self):
        """Manual refresh with safe error handling"""
        try:
            if not self.is_updating:
                # Show loading indicators
                for category in ASSETS.keys():
                    tag_suffix = category.replace(' ', '-').replace('&', 'and').lower()
                    data_container_tag = f"data_container_{tag_suffix}"
                    loading_text_tag = f"loading_text_{tag_suffix}"

                    if dpg.does_item_exist(data_container_tag):
                        # Clear existing data
                        children = dpg.get_item_children(data_container_tag, slot=1)
                        for child in children:
                            try:
                                dpg.delete_item(child)
                            except:
                                pass

                        # Add loading text
                        if not dpg.does_item_exist(loading_text_tag):
                            dpg.add_text("🔄 Refreshing...", parent=data_container_tag,
                                         tag=loading_text_tag, color=[255, 255, 100])

                threading.Thread(target=self.update_market_data, daemon=True).start()
        except Exception as e:
            print(f"❌ Error in manual refresh: {e}")

    def toggle_updates(self):
        """Toggle automatic updates"""
        try:
            auto_refresh = dpg.get_value("auto_refresh")
            dpg.set_value("auto_refresh", not auto_refresh)

            button_text = "▶️ Resume Updates" if auto_refresh else "⏸️ Pause Updates"
            dpg.set_item_label("update_toggle", button_text)
        except Exception as e:
            print(f"❌ Error toggling updates: {e}")

    def update_refresh_interval(self, sender, app_data):
        """Update refresh interval based on combo selection"""
        try:
            interval_map = {
                "5 minutes": 300,
                "10 minutes": 600,
                "15 minutes": 900,
                "30 minutes": 1800
            }

            self.REFRESH_INTERVAL = interval_map.get(app_data, 600)
            print(f"🔄 Refresh interval updated to {self.REFRESH_INTERVAL // 60} minutes")
        except Exception as e:
            print(f"❌ Error updating refresh interval: {e}")

    def cleanup(self):
        """Clean up market tab resources"""
        try:
            print("🧹 Cleaning up market tab...")
            self.stop_updates = True

            if self.update_thread and self.update_thread.is_alive():
                self.update_thread.join(timeout=3)

            self.market_data = {}
            self.last_prices = {}
            print("✅ Market tab cleanup complete")
        except Exception as e:
            print(f"❌ Error in cleanup: {e}")