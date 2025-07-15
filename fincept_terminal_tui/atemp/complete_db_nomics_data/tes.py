import dearpygui.dearpygui as dpg
import random
import time
import datetime
import threading
import math
from collections import deque
import json

# Initialize DearPyGUI
dpg.create_context()


dpg.create_viewport(title="Bloomberg Terminal Professional", width=1920, height=1080, decorated=False)

# Bloomberg Color Palette
COLORS = {
    'ORANGE': [255, 136, 0],
    'AMBER': [255, 170, 0],
    'BLACK': [0, 0, 0],
    'DARK_BG': [15, 15, 15],
    'PANEL_BG': [25, 25, 25],
    'WHITE': [255, 255, 255],
    'RED': [220, 20, 20],
    'GREEN': [0, 180, 0],
    'YELLOW': [255, 215, 0],
    'BLUE': [0, 120, 215],
    'GRAY': [128, 128, 128],
    'LIGHT_GRAY': [180, 180, 180],
    'DARK_GRAY': [60, 60, 60],
    'PURPLE': [128, 0, 255],
    'CYAN': [0, 215, 215]
}


# Global data storage
class MarketData:
    def __init__(self):
        self.tickers = self._generate_tickers()
        self.indices = self._generate_indices()
        self.futures = self._generate_futures()
        self.forex = self._generate_forex()
        self.commodities = self._generate_commodities()
        self.bonds = self._generate_bonds()
        self.crypto = self._generate_crypto()
        self.news = self._generate_news()
        self.heatmap_data = {}
        self.order_book = self._generate_order_book()
        self.time_sales = deque(maxlen=100)
        self.alerts = deque(maxlen=50)
        self._initialize_historical_data()

    def _generate_tickers(self):
        symbols = ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'META', 'TSLA', 'NVDA', 'JPM', 'BAC', 'GS',
                   'JNJ', 'PG', 'V', 'MA', 'UNH', 'HD', 'DIS', 'NFLX', 'ADBE', 'CRM',
                   'PYPL', 'INTC', 'CSCO', 'PEP', 'KO', 'ABT', 'NKE', 'MRK', 'WMT', 'CVX',
                   'XOM', 'PFE', 'TMO', 'ABBV', 'ORCL', 'ACN', 'COST', 'DHR', 'NEE', 'LLY']

        data = {}
        for symbol in symbols:
            price = round(random.uniform(50, 500), 2)
            change_pct = round(random.uniform(-3, 3), 2)
            data[symbol] = {
                'price': price,
                'change': round(price * change_pct / 100, 2),
                'change_pct': change_pct,
                'volume': random.randint(1000000, 50000000),
                'bid': round(price - 0.01, 2),
                'ask': round(price + 0.01, 2),
                'high': round(price * 1.02, 2),
                'low': round(price * 0.98, 2),
                'open': round(price * (1 - change_pct / 100), 2),
                'prev_close': round(price * (1 - change_pct / 100), 2),
                'market_cap': round(price * random.uniform(1e9, 1e12), 0),
                'pe': round(random.uniform(10, 40), 2),
                'eps': round(random.uniform(1, 20), 2),
                'div_yield': round(random.uniform(0, 4), 2),
                '52w_high': round(price * 1.3, 2),
                '52w_low': round(price * 0.7, 2),
                'avg_volume': random.randint(5000000, 30000000),
                'beta': round(random.uniform(0.5, 2), 2)
            }
        return data

    def _generate_indices(self):
        return {
            'SPX': {'name': 'S&P 500', 'value': 5234.18, 'change': 23.45, 'change_pct': 0.45},
            'INDU': {'name': 'DOW JONES', 'value': 39872.99, 'change': -134.21, 'change_pct': -0.34},
            'CCMP': {'name': 'NASDAQ', 'value': 16341.41, 'change': 87.23, 'change_pct': 0.54},
            'RTY': {'name': 'RUSSELL 2000', 'value': 2098.34, 'change': 12.67, 'change_pct': 0.61},
            'VIX': {'name': 'CBOE VIX', 'value': 13.47, 'change': -0.82, 'change_pct': -5.74},
            'FTSE': {'name': 'FTSE 100', 'value': 7682.50, 'change': 45.12, 'change_pct': 0.59},
            'DAX': {'name': 'DAX', 'value': 18567.23, 'change': -78.45, 'change_pct': -0.42},
            'N225': {'name': 'NIKKEI 225', 'value': 39098.68, 'change': 298.34, 'change_pct': 0.77},
            'HSI': {'name': 'HANG SENG', 'value': 17398.52, 'change': -234.78, 'change_pct': -1.33},
            'SHCOMP': {'name': 'SHANGHAI', 'value': 3087.53, 'change': 23.91, 'change_pct': 0.78}
        }

    def _generate_futures(self):
        return {
            'ES': {'name': 'E-MINI S&P', 'price': 5235.75, 'change': 24.50, 'change_pct': 0.47},
            'NQ': {'name': 'E-MINI NASDAQ', 'price': 18234.25, 'change': 112.75, 'change_pct': 0.62},
            'YM': {'name': 'E-MINI DOW', 'price': 39875.00, 'change': -125.00, 'change_pct': -0.31},
            'RTY': {'name': 'E-MINI RUSSELL', 'price': 2098.40, 'change': 13.20, 'change_pct': 0.63},
            'CL': {'name': 'WTI CRUDE', 'price': 78.43, 'change': -1.27, 'change_pct': -1.59},
            'GC': {'name': 'GOLD', 'price': 2342.60, 'change': 18.40, 'change_pct': 0.79},
            'SI': {'name': 'SILVER', 'price': 28.745, 'change': 0.385, 'change_pct': 1.36},
            'NG': {'name': 'NATURAL GAS', 'price': 2.487, 'change': -0.063, 'change_pct': -2.47},
            'ZB': {'name': '30Y T-BOND', 'price': 119.281, 'change': 0.156, 'change_pct': 0.13},
            'ZN': {'name': '10Y T-NOTE', 'price': 109.453, 'change': 0.078, 'change_pct': 0.07}
        }

    def _generate_forex(self):
        return {
            'EURUSD': {'bid': 1.0834, 'ask': 1.0836, 'change': -0.0023, 'change_pct': -0.21},
            'GBPUSD': {'bid': 1.2743, 'ask': 1.2745, 'change': 0.0018, 'change_pct': 0.14},
            'USDJPY': {'bid': 151.43, 'ask': 151.45, 'change': 0.32, 'change_pct': 0.21},
            'USDCHF': {'bid': 0.8634, 'ask': 0.8636, 'change': -0.0012, 'change_pct': -0.14},
            'AUDUSD': {'bid': 0.6523, 'ask': 0.6525, 'change': 0.0008, 'change_pct': 0.12},
            'USDCAD': {'bid': 1.3567, 'ask': 1.3569, 'change': -0.0024, 'change_pct': -0.18},
            'NZDUSD': {'bid': 0.5912, 'ask': 0.5914, 'change': 0.0003, 'change_pct': 0.05},
            'EURGBP': {'bid': 0.8502, 'ask': 0.8504, 'change': -0.0015, 'change_pct': -0.18},
            'EURJPY': {'bid': 164.12, 'ask': 164.15, 'change': 0.28, 'change_pct': 0.17},
            'GBPJPY': {'bid': 193.05, 'ask': 193.08, 'change': 0.64, 'change_pct': 0.33}
        }

    def _generate_commodities(self):
        return {
            'GOLD': {'price': 2342.60, 'change': 18.40, 'change_pct': 0.79, 'unit': 'USD/oz'},
            'SILVER': {'price': 28.745, 'change': 0.385, 'change_pct': 1.36, 'unit': 'USD/oz'},
            'PLATINUM': {'price': 972.40, 'change': -5.60, 'change_pct': -0.57, 'unit': 'USD/oz'},
            'COPPER': {'price': 4.5235, 'change': 0.0342, 'change_pct': 0.76, 'unit': 'USD/lb'},
            'WTI': {'price': 78.43, 'change': -1.27, 'change_pct': -1.59, 'unit': 'USD/bbl'},
            'BRENT': {'price': 82.87, 'change': -1.13, 'change_pct': -1.34, 'unit': 'USD/bbl'},
            'NATGAS': {'price': 2.487, 'change': -0.063, 'change_pct': -2.47, 'unit': 'USD/MMBtu'},
            'WHEAT': {'price': 587.25, 'change': 4.50, 'change_pct': 0.77, 'unit': 'USc/bu'},
            'CORN': {'price': 449.75, 'change': -2.25, 'change_pct': -0.50, 'unit': 'USc/bu'},
            'SOYBEANS': {'price': 1187.50, 'change': 8.75, 'change_pct': 0.74, 'unit': 'USc/bu'}
        }

    def _generate_bonds(self):
        return {
            'US2Y': {'yield': 4.87, 'change': 0.03, 'price': 98.234},
            'US5Y': {'yield': 4.42, 'change': 0.02, 'price': 95.678},
            'US10Y': {'yield': 4.35, 'change': 0.01, 'price': 91.234},
            'US30Y': {'yield': 4.53, 'change': -0.02, 'price': 84.567},
            'DE10Y': {'yield': 2.45, 'change': 0.04, 'price': 97.123},
            'UK10Y': {'yield': 4.12, 'change': -0.03, 'price': 92.456},
            'JP10Y': {'yield': 0.73, 'change': 0.01, 'price': 99.234},
            'IT10Y': {'yield': 3.98, 'change': 0.05, 'price': 93.789},
            'ES10Y': {'yield': 3.42, 'change': 0.02, 'price': 95.123},
            'FR10Y': {'yield': 2.98, 'change': 0.03, 'price': 96.456}
        }

    def _generate_crypto(self):
        return {
            'BTC': {'price': 45234.56, 'change': 1234.56, 'change_pct': 2.81, 'volume': 28934567890},
            'ETH': {'price': 2456.78, 'change': -45.23, 'change_pct': -1.81, 'volume': 12345678900},
            'BNB': {'price': 312.45, 'change': 8.34, 'change_pct': 2.74, 'volume': 1234567890},
            'XRP': {'price': 0.6234, 'change': 0.0234, 'change_pct': 3.90, 'volume': 2345678901},
            'SOL': {'price': 98.76, 'change': -2.34, 'change_pct': -2.31, 'volume': 987654321},
            'ADA': {'price': 0.5678, 'change': 0.0123, 'change_pct': 2.21, 'volume': 567890123},
            'DOGE': {'price': 0.0876, 'change': -0.0023, 'change_pct': -2.56, 'volume': 3456789012},
            'AVAX': {'price': 34.56, 'change': 1.23, 'change_pct': 3.69, 'volume': 456789012},
            'DOT': {'price': 7.89, 'change': 0.12, 'change_pct': 1.54, 'volume': 234567890},
            'MATIC': {'price': 0.8765, 'change': -0.0234, 'change_pct': -2.60, 'volume': 345678901}
        }

    def _generate_news(self):
        return [
            {'time': '14:32', 'headline': 'BREAKING: Fed Minutes Show Officials Divided on Rate Cut Timing',
             'category': 'FED'},
            {'time': '14:28', 'headline': 'Apple Announces $110 Billion Share Buyback Program', 'category': 'TECH'},
            {'time': '14:15', 'headline': 'Oil Prices Decline on Surprise Inventory Build', 'category': 'COMMODITIES'},
            {'time': '14:03', 'headline': 'ECB Signals Possible June Rate Cut Amid Inflation Progress',
             'category': 'CENTRAL BANKS'},
            {'time': '13:55', 'headline': 'Tesla Recalls 125,000 Vehicles Over Seat Belt Issue', 'category': 'AUTO'},
            {'time': '13:42', 'headline': 'JPMorgan Beats Q1 Earnings Estimates, Raises Dividend', 'category': 'BANKS'},
            {'time': '13:30', 'headline': 'US Retail Sales Rise 0.7% in March, Beating Estimates',
             'category': 'ECONOMY'},
            {'time': '13:18', 'headline': 'Microsoft Azure Revenue Growth Accelerates to 31%', 'category': 'TECH'},
            {'time': '13:05', 'headline': 'China GDP Grows 5.3% in Q1, Exceeding Forecasts', 'category': 'ASIA'},
            {'time': '12:52', 'headline': 'Gold Hits New Record High Amid Geopolitical Tensions',
             'category': 'COMMODITIES'}
        ]

    def _generate_order_book(self):
        base_price = 175.50
        orders = {'bids': [], 'asks': []}

        for i in range(10):
            bid_price = round(base_price - (i * 0.01), 2)
            ask_price = round(base_price + (i * 0.01), 2)

            orders['bids'].append({
                'price': bid_price,
                'size': random.randint(100, 10000),
                'orders': random.randint(1, 50)
            })

            orders['asks'].append({
                'price': ask_price,
                'size': random.randint(100, 10000),
                'orders': random.randint(1, 50)
            })

        return orders

    def _initialize_historical_data(self):
        self.historical_data = {}
        for ticker in self.tickers:
            self.historical_data[ticker] = []
            base = self.tickers[ticker]['price']
            for i in range(100):
                price = base * (1 + random.uniform(-0.02, 0.02))
                self.historical_data[ticker].append(round(price, 2))


# Initialize market data
market_data = MarketData()

# Create fonts
with dpg.font_registry():
    default_font = dpg.add_font("C:/Windows/Fonts/consola.ttf", 13)
    bold_font = dpg.add_font("C:/Windows/Fonts/consolab.ttf", 13)
    large_font = dpg.add_font("C:/Windows/Fonts/consolab.ttf", 16)
    small_font = dpg.add_font("C:/Windows/Fonts/consola.ttf", 11)

# Create themes
with dpg.theme() as global_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_WindowBg, COLORS['BLACK'])
        dpg.add_theme_color(dpg.mvThemeCol_ChildBg, COLORS['DARK_BG'])
        dpg.add_theme_color(dpg.mvThemeCol_Text, COLORS['WHITE'])
        dpg.add_theme_color(dpg.mvThemeCol_Button, COLORS['PANEL_BG'])
        dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, COLORS['ORANGE'])
        dpg.add_theme_color(dpg.mvThemeCol_Header, COLORS['PANEL_BG'])
        dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, COLORS['ORANGE'])
        dpg.add_theme_color(dpg.mvThemeCol_FrameBg, COLORS['PANEL_BG'])
        dpg.add_theme_color(dpg.mvThemeCol_Border, COLORS['DARK_GRAY'])
        dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 0, 0)
        dpg.add_theme_style(dpg.mvStyleVar_FramePadding, 2, 2)
        dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 2, 2)

# Color themes for different states
positive_theme = dpg.theme()
with positive_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, COLORS['GREEN'])

negative_theme = dpg.theme()
with negative_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, COLORS['RED'])

orange_theme = dpg.theme()
with orange_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, COLORS['ORANGE'])

yellow_theme = dpg.theme()
with yellow_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, COLORS['YELLOW'])

dpg.bind_theme(global_theme)


# Helper functions
def apply_value_theme(tag, value):
    if value > 0:
        dpg.bind_item_theme(tag, positive_theme)
    elif value < 0:
        dpg.bind_item_theme(tag, negative_theme)


def format_number(num, decimals=2):
    if abs(num) >= 1e9:
        return f"{num / 1e9:.{decimals}f}B"
    elif abs(num) >= 1e6:
        return f"{num / 1e6:.{decimals}f}M"
    elif abs(num) >= 1e3:
        return f"{num / 1e3:.{decimals}f}K"
    return f"{num:.{decimals}f}"


# Create main window
with dpg.window(tag="primary_window", no_title_bar=True, no_move=True, no_resize=True, no_collapse=True):
    # Top bar
    with dpg.group(horizontal=True):
        dpg.add_text("BLOOMBERG", color=COLORS['ORANGE'])
        dpg.add_text("PROFESSIONAL SERVICE", color=COLORS['WHITE'])
        dpg.add_text(" | ", color=COLORS['GRAY'])

        # Function keys
        for i in range(1, 13):
            key_func = {
                1: "HELP", 2: "NEWS", 3: "MSGS", 4: "MKTS", 5: "MONITOR", 6: "GRAPH",
                7: "COMPANY", 8: "PEOPLE", 9: "ANALYZE", 10: "DATA", 11: "SETTINGS", 12: "LAST"
            }
            dpg.add_button(label=f"F{i}:{key_func[i]}", width=80, height=20)
            if i < 12:
                dpg.add_text(" ", color=COLORS['BLACK'])

        dpg.add_text(" " * 10)  # Spacer
        dpg.add_text(datetime.datetime.now().strftime("%d-%b-%Y %H:%M:%S"), tag="time_display", color=COLORS['AMBER'])
        dpg.add_text(" ", color=COLORS['BLACK'])
        dpg.add_text("User: TRADER1", color=COLORS['GRAY'])

    dpg.add_separator()

    # Main content area
    with dpg.group(horizontal=True):
        # Left panel - Market Overview
        with dpg.child_window(width=480, border=False):
            # Market indices
            dpg.add_text("GLOBAL MARKETS", color=COLORS['ORANGE'])
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           borders_innerV=True, borders_outerV=True, resizable=True):
                dpg.add_table_column(label="INDEX", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="LAST", width_fixed=True, init_width_or_weight=100)
                dpg.add_table_column(label="CHG", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="%CHG", width_fixed=True, init_width_or_weight=80)

                for idx_code, idx_data in market_data.indices.items():
                    with dpg.table_row():
                        dpg.add_text(idx_data['name'])
                        dpg.add_text(f"{idx_data['value']:,.2f}")
                        chg_tag = f"idx_{idx_code}_chg"
                        dpg.add_text(f"{idx_data['change']:+,.2f}", tag=chg_tag)
                        apply_value_theme(chg_tag, idx_data['change'])
                        pct_tag = f"idx_{idx_code}_pct"
                        dpg.add_text(f"{idx_data['change_pct']:+.2f}%", tag=pct_tag)
                        apply_value_theme(pct_tag, idx_data['change_pct'])

            dpg.add_separator()

            # Top movers
            dpg.add_text("TOP MOVERS", color=COLORS['ORANGE'])
            with dpg.child_window(height=150, border=True):
                dpg.add_text("GAINERS", color=COLORS['GREEN'])
                gainers = sorted([(k, v) for k, v in market_data.tickers.items()],
                                 key=lambda x: x[1]['change_pct'], reverse=True)[:5]
                for ticker, data in gainers:
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{ticker:<6}")
                        dpg.add_text(f"{data['price']:>8.2f}")
                        gain_tag = f"gain_{ticker}"
                        dpg.add_text(f"{data['change_pct']:>+6.2f}%", tag=gain_tag)
                        dpg.bind_item_theme(gain_tag, positive_theme)

                dpg.add_text("\nLOSERS", color=COLORS['RED'])
                losers = sorted([(k, v) for k, v in market_data.tickers.items()],
                                key=lambda x: x[1]['change_pct'])[:5]
                for ticker, data in losers:
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{ticker:<6}")
                        dpg.add_text(f"{data['price']:>8.2f}")
                        loss_tag = f"loss_{ticker}"
                        dpg.add_text(f"{data['change_pct']:>+6.2f}%", tag=loss_tag)
                        dpg.bind_item_theme(loss_tag, negative_theme)

            dpg.add_separator()

            # Economic calendar
            dpg.add_text("ECONOMIC CALENDAR", color=COLORS['ORANGE'])
            with dpg.child_window(height=120, border=True):
                events = [
                    ("08:30", "US Initial Jobless Claims", "HIGH", "215K", "220K"),
                    ("10:00", "US Existing Home Sales", "MED", "4.38M", "4.35M"),
                    ("10:30", "EIA Natural Gas Storage", "LOW", "-45B", "-50B"),
                    ("14:00", "Fed's Waller Speaks", "HIGH", "-", "-"),
                    ("14:30", "3-Month Bill Auction", "LOW", "5.23%", "5.20%")
                ]

                for time, event, impact, actual, forecast in events:
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{time} ")
                        impact_colors = {"HIGH": COLORS['RED'], "MED": COLORS['YELLOW'], "LOW": COLORS['GRAY']}
                        impact_tag = f"impact_{time}_{event[:10]}"
                        dpg.add_text(f"[{impact}]", tag=impact_tag, color=impact_colors[impact])
                        dpg.add_text(f" {event}")
                        if actual != "-":
                            dpg.add_text(f" A:{actual} F:{forecast}")

            dpg.add_separator()

            # News ticker
            dpg.add_text("LATEST NEWS", color=COLORS['ORANGE'])
            with dpg.child_window(height=200, border=True):
                for news in market_data.news[:8]:
                    with dpg.group():
                        with dpg.group(horizontal=True):
                            dpg.add_text(f"{news['time']}", color=COLORS['GRAY'])
                            dpg.add_text(f" [{news['category']}]", color=COLORS['YELLOW'])
                        dpg.add_text(news['headline'], wrap=450)
                        dpg.add_separator()

        # Center panel - Main workspace
        with dpg.child_window(width=960, border=False):
            with dpg.tab_bar():
                # Monitor tab
                with dpg.tab(label="MONITOR"):
                    # Watchlist
                    dpg.add_text("WATCHLIST", color=COLORS['ORANGE'])
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   borders_innerV=True, borders_outerV=True, scrollY=True, height=300):
                        columns = ["TICKER", "LAST", "CHG", "%CHG", "BID", "ASK", "VOL", "HIGH", "LOW", "OPEN"]
                        for col in columns:
                            dpg.add_table_column(label=col)

                        for ticker, data in list(market_data.tickers.items())[:20]:
                            with dpg.table_row():
                                dpg.add_text(ticker)
                                dpg.add_text(f"{data['price']:.2f}")

                                chg_tag = f"{ticker}_chg"
                                dpg.add_text(f"{data['change']:+.2f}", tag=chg_tag)
                                apply_value_theme(chg_tag, data['change'])

                                pct_tag = f"{ticker}_pct"
                                dpg.add_text(f"{data['change_pct']:+.2f}%", tag=pct_tag)
                                apply_value_theme(pct_tag, data['change_pct'])

                                dpg.add_text(f"{data['bid']:.2f}")
                                dpg.add_text(f"{data['ask']:.2f}")
                                dpg.add_text(format_number(data['volume']))
                                dpg.add_text(f"{data['high']:.2f}")
                                dpg.add_text(f"{data['low']:.2f}")
                                dpg.add_text(f"{data['open']:.2f}")

                    # Market heat map placeholder
                    dpg.add_text("\nMARKET HEAT MAP", color=COLORS['ORANGE'])
                    with dpg.drawlist(width=920, height=250):
                        sectors = ['TECH', 'FINANCE', 'HEALTH', 'ENERGY', 'CONSUMER', 'INDUSTRIAL',
                                   'MATERIALS', 'UTILITIES', 'REAL ESTATE', 'TELECOM']

                        x, y = 10, 10
                        for i, sector in enumerate(sectors):
                            change = random.uniform(-3, 3)
                            if change > 0:
                                color = [0, min(255, int(100 + change * 50)), 0]
                            else:
                                color = [min(255, int(100 - change * 50)), 0, 0]

                            dpg.draw_rectangle((x, y), (x + 175, y + 110), color=color, fill=color)
                            dpg.draw_text((x + 10, y + 10), sector, color=COLORS['WHITE'], size=14)
                            dpg.draw_text((x + 10, y + 40), f"{change:+.2f}%",
                                          color=COLORS['WHITE'], size=20)

                            x += 185
                            if (i + 1) % 5 == 0:
                                x = 10
                                y += 120

                # Charts tab
                with dpg.tab(label="CHARTS"):
                    with dpg.group(horizontal=True):
                        dpg.add_combo(list(market_data.tickers.keys())[:10],
                                      default_value="AAPL", width=100, tag="chart_ticker")
                        dpg.add_combo(["1D", "5D", "1M", "3M", "6M", "1Y", "5Y"],
                                      default_value="1D", width=80)
                        dpg.add_combo(["Line", "Candle", "Bar", "Mountain"],
                                      default_value="Line", width=100)
                        dpg.add_button(label="Studies", width=80)
                        dpg.add_button(label="Compare", width=80)
                        dpg.add_button(label="Events", width=80)

                    # Main chart
                    with dpg.plot(height=400, width=-1, anti_aliased=True):
                        dpg.add_plot_legend()
                        x_axis = dpg.add_plot_axis(dpg.mvXAxis, label="Time")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Price")

                        # Price line
                        dpg.add_line_series(list(range(100)),
                                            market_data.historical_data['AAPL'],
                                            label="AAPL", parent=y_axis, tag="main_chart_line")

                        # Volume bars (simplified)
                        y_axis_vol = dpg.add_plot_axis(dpg.mvYAxis, label="Volume",
                                                       no_gridlines=True)
                        volumes = [random.randint(10000000, 50000000) for _ in range(100)]
                        dpg.add_bar_series(list(range(100)), volumes,
                                           label="Volume", parent=y_axis_vol, weight=0.3)

                    # Technical indicators
                    dpg.add_text("TECHNICAL ANALYSIS", color=COLORS['ORANGE'])
                    with dpg.group(horizontal=True):
                        with dpg.child_window(width=300, height=150, border=True):
                            dpg.add_text("INDICATORS", color=COLORS['YELLOW'])
                            indicators = [
                                ("RSI(14)", "68.45", "Overbought", COLORS['YELLOW']),
                                ("MACD", "+2.34", "Bullish", COLORS['GREEN']),
                                ("SMA(50)", "173.25", "Above", COLORS['GREEN']),
                                ("SMA(200)", "165.80", "Above", COLORS['GREEN']),
                                ("Stochastic", "82.15", "Overbought", COLORS['YELLOW'])
                            ]
                            for ind, val, signal, color in indicators:
                                with dpg.group(horizontal=True):
                                    dpg.add_text(f"{ind:<12}")
                                    dpg.add_text(f"{val:>8}")
                                    sig_tag = f"sig_{ind}"
                                    dpg.add_text(f" {signal}", tag=sig_tag, color=color)

                        with dpg.child_window(width=300, height=150, border=True):
                            dpg.add_text("PIVOT POINTS", color=COLORS['YELLOW'])
                            pivots = [
                                ("R3", "178.45", COLORS['RED']),
                                ("R2", "176.82", COLORS['RED']),
                                ("R1", "175.64", COLORS['RED']),
                                ("Pivot", "174.23", COLORS['YELLOW']),
                                ("S1", "172.85", COLORS['GREEN']),
                                ("S2", "171.42", COLORS['GREEN']),
                                ("S3", "169.98", COLORS['GREEN'])
                            ]
                            for level, price, color in pivots[:5]:
                                with dpg.group(horizontal=True):
                                    dpg.add_text(f"{level:<8}")
                                    piv_tag = f"piv_{level}"
                                    dpg.add_text(f"{price:>8}", tag=piv_tag, color=color)

                        with dpg.child_window(width=300, height=150, border=True):
                            dpg.add_text("PATTERNS", color=COLORS['YELLOW'])
                            patterns = [
                                "Ascending Triangle (Bullish)",
                                "Support at 172.50",
                                "Resistance at 176.80",
                                "Volume Increasing",
                                "MACD Divergence"
                            ]
                            for pattern in patterns:
                                dpg.add_text(f"• {pattern}")

                # Market Depth tab
                with dpg.tab(label="DEPTH"):
                    dpg.add_text("ORDER BOOK - AAPL", color=COLORS['ORANGE'])

                    with dpg.group(horizontal=True):
                        # Bid side
                        with dpg.child_window(width=450, height=400, border=True):
                            dpg.add_text("BIDS", color=COLORS['GREEN'])
                            with dpg.table(header_row=True, borders_innerH=True):
                                dpg.add_table_column(label="ORDERS")
                                dpg.add_table_column(label="SIZE")
                                dpg.add_table_column(label="BID")
                                dpg.add_table_column(label="TOTAL")

                                total = 0
                                for order in market_data.order_book['bids']:
                                    total += order['size']
                                    with dpg.table_row():
                                        dpg.add_text(str(order['orders']))
                                        dpg.add_text(format_number(order['size'], 0))
                                        bid_tag = f"bid_{order['price']}"
                                        dpg.add_text(f"{order['price']:.2f}", tag=bid_tag)
                                        dpg.bind_item_theme(bid_tag, positive_theme)
                                        dpg.add_text(format_number(total, 0))

                        # Ask side
                        with dpg.child_window(width=450, height=400, border=True):
                            dpg.add_text("ASKS", color=COLORS['RED'])
                            with dpg.table(header_row=True, borders_innerH=True):
                                dpg.add_table_column(label="ASK")
                                dpg.add_table_column(label="SIZE")
                                dpg.add_table_column(label="ORDERS")
                                dpg.add_table_column(label="TOTAL")

                                total = 0
                                for order in market_data.order_book['asks']:
                                    total += order['size']
                                    with dpg.table_row():
                                        ask_tag = f"ask_{order['price']}"
                                        dpg.add_text(f"{order['price']:.2f}", tag=ask_tag)
                                        dpg.bind_item_theme(ask_tag, negative_theme)
                                        dpg.add_text(format_number(order['size'], 0))
                                        dpg.add_text(str(order['orders']))
                                        dpg.add_text(format_number(total, 0))

                    # Time and sales
                    dpg.add_text("\nTIME & SALES", color=COLORS['ORANGE'])
                    with dpg.child_window(height=200, border=True):
                        with dpg.table(header_row=True, borders_innerH=True):
                            dpg.add_table_column(label="TIME")
                            dpg.add_table_column(label="PRICE")
                            dpg.add_table_column(label="SIZE")
                            dpg.add_table_column(label="COND")

                            # Generate dummy time and sales
                            current_time = datetime.datetime.now()
                            for i in range(20):
                                trade_time = (current_time - datetime.timedelta(seconds=i * 2)).strftime("%H:%M:%S")
                                price = 175.50 + random.uniform(-0.10, 0.10)
                                size = random.randint(100, 5000)
                                cond = random.choice(["", "ISO", "ODD", "SWEEP"])

                                with dpg.table_row():
                                    dpg.add_text(trade_time)
                                    price_tag = f"ts_price_{i}"
                                    dpg.add_text(f"{price:.2f}", tag=price_tag)
                                    if random.random() > 0.5:
                                        dpg.bind_item_theme(price_tag, positive_theme)
                                    else:
                                        dpg.bind_item_theme(price_tag, negative_theme)
                                    dpg.add_text(str(size))
                                    dpg.add_text(cond)

                # Analytics tab
                with dpg.tab(label="ANALYTICS"):
                    dpg.add_text("PORTFOLIO ANALYTICS", color=COLORS['ORANGE'])

                    with dpg.group(horizontal=True):
                        # Risk metrics
                        with dpg.child_window(width=300, height=200, border=True):
                            dpg.add_text("RISK METRICS", color=COLORS['YELLOW'])
                            metrics = [
                                ("Portfolio Beta", "1.23"),
                                ("Sharpe Ratio", "1.85"),
                                ("Max Drawdown", "-12.3%"),
                                ("VaR (95%)", "$125,430"),
                                ("Std Deviation", "18.5%"),
                                ("Tracking Error", "4.2%")
                            ]
                            for metric, value in metrics:
                                with dpg.group(horizontal=True):
                                    dpg.add_text(f"{metric:<16}")
                                    dpg.add_text(f"{value:>10}")

                        # Performance
                        with dpg.child_window(width=300, height=200, border=True):
                            dpg.add_text("PERFORMANCE", color=COLORS['YELLOW'])
                            perf = [
                                ("1 Day", "+1.23%", True),
                                ("1 Week", "-0.45%", False),
                                ("1 Month", "+3.67%", True),
                                ("3 Month", "+8.92%", True),
                                ("YTD", "+12.45%", True),
                                ("1 Year", "+18.76%", True)
                            ]
                            for period, ret, positive in perf:
                                with dpg.group(horizontal=True):
                                    dpg.add_text(f"{period:<10}")
                                    ret_tag = f"perf_{period}"
                                    dpg.add_text(f"{ret:>10}", tag=ret_tag)
                                    if positive:
                                        dpg.bind_item_theme(ret_tag, positive_theme)
                                    else:
                                        dpg.bind_item_theme(ret_tag, negative_theme)

                        # Sector allocation
                        with dpg.child_window(width=300, height=200, border=True):
                            dpg.add_text("SECTOR ALLOCATION", color=COLORS['YELLOW'])
                            sectors = [
                                ("Technology", "35.2%"),
                                ("Finance", "22.1%"),
                                ("Healthcare", "15.8%"),
                                ("Energy", "10.3%"),
                                ("Consumer", "8.7%"),
                                ("Other", "7.9%")
                            ]
                            for sector, weight in sectors:
                                with dpg.group(horizontal=True):
                                    dpg.add_text(f"{sector:<14}")
                                    dpg.add_text(f"{weight:>8}")

        # Right panel - Additional data
        with dpg.child_window(width=460, border=False):
            # Command line
            dpg.add_text("COMMAND LINE", color=COLORS['ORANGE'])
            with dpg.child_window(height=100, border=True):
                dpg.add_text("> AAPL US Equity HP", color=COLORS['GREEN'])
                dpg.add_text("  Loading historical prices...", color=COLORS['GRAY'])
                dpg.add_text("> N MSFT US Equity", color=COLORS['GREEN'])
                dpg.add_text("  Loading news for MSFT...", color=COLORS['GRAY'])
                dpg.add_input_text(label=">", default_value="", width=-1, tag="cmd_input")

            dpg.add_separator()

            # Multi-asset monitor
            dpg.add_text("MULTI-ASSET MONITOR", color=COLORS['ORANGE'])
            with dpg.tab_bar():
                # FX tab
                with dpg.tab(label="FX"):
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   borders_innerV=True, scrollY=True, height=200):
                        dpg.add_table_column(label="PAIR")
                        dpg.add_table_column(label="BID")
                        dpg.add_table_column(label="ASK")
                        dpg.add_table_column(label="CHG")
                        dpg.add_table_column(label="%")

                        for pair, data in market_data.forex.items():
                            with dpg.table_row():
                                dpg.add_text(pair)
                                dpg.add_text(f"{data['bid']:.4f}")
                                dpg.add_text(f"{data['ask']:.4f}")
                                fx_chg_tag = f"fx_{pair}_chg"
                                dpg.add_text(f"{data['change']:+.4f}", tag=fx_chg_tag)
                                apply_value_theme(fx_chg_tag, data['change'])
                                fx_pct_tag = f"fx_{pair}_pct"
                                dpg.add_text(f"{data['change_pct']:+.2f}%", tag=fx_pct_tag)
                                apply_value_theme(fx_pct_tag, data['change_pct'])

                # Commodities tab
                with dpg.tab(label="CMDTY"):
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   borders_innerV=True, scrollY=True, height=200):
                        dpg.add_table_column(label="COMMODITY")
                        dpg.add_table_column(label="LAST")
                        dpg.add_table_column(label="CHG")
                        dpg.add_table_column(label="%")

                        for comm, data in market_data.commodities.items():
                            with dpg.table_row():
                                dpg.add_text(comm)
                                dpg.add_text(f"{data['price']:.2f}")
                                comm_chg_tag = f"comm_{comm}_chg"
                                dpg.add_text(f"{data['change']:+.2f}", tag=comm_chg_tag)
                                apply_value_theme(comm_chg_tag, data['change'])
                                comm_pct_tag = f"comm_{comm}_pct"
                                dpg.add_text(f"{data['change_pct']:+.2f}%", tag=comm_pct_tag)
                                apply_value_theme(comm_pct_tag, data['change_pct'])

                # Bonds tab
                with dpg.tab(label="BONDS"):
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   borders_innerV=True, scrollY=True, height=200):
                        dpg.add_table_column(label="BOND")
                        dpg.add_table_column(label="YIELD")
                        dpg.add_table_column(label="CHG")
                        dpg.add_table_column(label="PRICE")

                        for bond, data in market_data.bonds.items():
                            with dpg.table_row():
                                dpg.add_text(bond)
                                dpg.add_text(f"{data['yield']:.2f}%")
                                bond_chg_tag = f"bond_{bond}_chg"
                                dpg.add_text(f"{data['change']:+.2f}", tag=bond_chg_tag)
                                apply_value_theme(bond_chg_tag, data['change'])
                                dpg.add_text(f"{data['price']:.3f}")

                # Crypto tab
                with dpg.tab(label="CRYPTO"):
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   borders_innerV=True, scrollY=True, height=200):
                        dpg.add_table_column(label="COIN")
                        dpg.add_table_column(label="PRICE")
                        dpg.add_table_column(label="CHG")
                        dpg.add_table_column(label="%")

                        for coin, data in market_data.crypto.items():
                            with dpg.table_row():
                                dpg.add_text(coin)
                                dpg.add_text(f"${data['price']:,.2f}")
                                crypto_chg_tag = f"crypto_{coin}_chg"
                                dpg.add_text(f"{data['change']:+.2f}", tag=crypto_chg_tag)
                                apply_value_theme(crypto_chg_tag, data['change'])
                                crypto_pct_tag = f"crypto_{coin}_pct"
                                dpg.add_text(f"{data['change_pct']:+.2f}%", tag=crypto_pct_tag)
                                apply_value_theme(crypto_pct_tag, data['change_pct'])

            dpg.add_separator()

            # Alerts
            dpg.add_text("ALERTS", color=COLORS['ORANGE'])
            with dpg.child_window(height=150, border=True):
                alerts = [
                    ("14:35:22", "AAPL", "Price Alert: Crossed above 175.50", COLORS['GREEN']),
                    ("14:32:15", "VIX", "Volatility Alert: VIX below 14", COLORS['YELLOW']),
                    ("14:28:44", "TSLA", "Volume Alert: Unusual activity detected", COLORS['ORANGE']),
                    ("14:25:12", "EUR/USD", "FX Alert: Broke support at 1.0850", COLORS['RED']),
                    ("14:22:05", "SPX", "Index Alert: New intraday high", COLORS['GREEN'])
                ]

                for time, symbol, message, color in alerts:
                    with dpg.group():
                        with dpg.group(horizontal=True):
                            dpg.add_text(f"{time}", color=COLORS['GRAY'])
                            alert_sym_tag = f"alert_{time}_{symbol}"
                            dpg.add_text(f" [{symbol}]", tag=alert_sym_tag, color=color)
                        dpg.add_text(message, wrap=440)

    # Bottom status bar
    dpg.add_separator()
    with dpg.group(horizontal=True):
        dpg.add_text("CONNECTED", color=COLORS['GREEN'])
        dpg.add_text(" | ", color=COLORS['GRAY'])
        dpg.add_text("MARKET: OPEN", tag="market_status", color=COLORS['GREEN'])
        dpg.add_text(" | ", color=COLORS['GRAY'])
        dpg.add_text("DATA: REAL-TIME", color=COLORS['GREEN'])
        dpg.add_text(" | ", color=COLORS['GRAY'])
        dpg.add_text("LATENCY: 2ms", color=COLORS['GREEN'])
        dpg.add_text(" " * 50)
        dpg.add_text("© 2024 Bloomberg L.P. All rights reserved", color=COLORS['GRAY'])


# Update functions
def update_market_data():
    """Update all market data with random movements"""
    while dpg.is_dearpygui_running():
        try:
            # Update stocks
            for ticker, data in market_data.tickers.items():
                change = random.uniform(-0.5, 0.5)
                data['price'] = round(data['price'] * (1 + change / 100), 2)
                data['change'] = round(data['price'] - data['open'], 2)
                data['change_pct'] = round((data['change'] / data['open']) * 100, 2)
                data['bid'] = round(data['price'] - 0.01, 2)
                data['ask'] = round(data['price'] + 0.01, 2)
                data['volume'] += random.randint(1000, 50000)

                if data['price'] > data['high']:
                    data['high'] = data['price']
                if data['price'] < data['low']:
                    data['low'] = data['price']

                # Update historical data
                market_data.historical_data[ticker].append(data['price'])
                if len(market_data.historical_data[ticker]) > 100:
                    market_data.historical_data[ticker].pop(0)

            # Update indices
            for idx_code, idx_data in market_data.indices.items():
                change = random.uniform(-0.1, 0.1)
                idx_data['value'] = round(idx_data['value'] * (1 + change / 100), 2)
                idx_data['change'] = round(idx_data['change'] + change, 2)
                idx_data['change_pct'] = round(idx_data['change_pct'] + change / 10, 2)

            # Update forex
            for pair, data in market_data.forex.items():
                change = random.uniform(-0.0005, 0.0005)
                data['bid'] = round(data['bid'] + change, 4)
                data['ask'] = round(data['ask'] + change, 4)
                data['change'] = round(data['change'] + change / 10, 4)
                data['change_pct'] = round(data['change_pct'] + random.uniform(-0.05, 0.05), 2)

            # Update commodities
            for comm, data in market_data.commodities.items():
                change = random.uniform(-0.5, 0.5)
                data['price'] = round(data['price'] * (1 + change / 100), 2)
                data['change'] = round(data['change'] + change / 10, 2)
                data['change_pct'] = round(data['change_pct'] + change / 10, 2)

            # Update bonds
            for bond, data in market_data.bonds.items():
                change = random.uniform(-0.02, 0.02)
                data['yield'] = round(data['yield'] + change, 2)
                data['change'] = round(change * 100, 2)
                data['price'] = round(data['price'] - change * 10, 3)

            # Update crypto
            for coin, data in market_data.crypto.items():
                change = random.uniform(-2, 2)
                data['price'] = round(data['price'] * (1 + change / 100), 2)
                data['change'] = round(data['price'] * change / 100, 2)
                data['change_pct'] = change

            time.sleep(2)

        except Exception as e:
            print(f"Update error: {e}")
            time.sleep(2)


def update_ui():
    """Update UI elements with new data"""
    try:
        # Update time
        dpg.set_value("time_display", datetime.datetime.now().strftime("%d-%b-%Y %H:%M:%S"))

        # Update market status
        now = datetime.datetime.now()
        if now.weekday() < 5 and 9 <= now.hour < 16:
            dpg.set_value("market_status", "MARKET: OPEN")
            dpg.bind_item_theme("market_status", positive_theme)
        else:
            dpg.set_value("market_status", "MARKET: CLOSED")
            dpg.bind_item_theme("market_status", negative_theme)

        # Update indices
        for idx_code, idx_data in market_data.indices.items():
            if dpg.does_item_exist(f"idx_{idx_code}_chg"):
                dpg.set_value(f"idx_{idx_code}_chg", f"{idx_data['change']:+,.2f}")
                apply_value_theme(f"idx_{idx_code}_chg", idx_data['change'])
            if dpg.does_item_exist(f"idx_{idx_code}_pct"):
                dpg.set_value(f"idx_{idx_code}_pct", f"{idx_data['change_pct']:+.2f}%")
                apply_value_theme(f"idx_{idx_code}_pct", idx_data['change_pct'])

        # Update stocks
        for ticker, data in market_data.tickers.items():
            if dpg.does_item_exist(f"{ticker}_chg"):
                dpg.set_value(f"{ticker}_chg", f"{data['change']:+.2f}")
                apply_value_theme(f"{ticker}_chg", data['change'])
            if dpg.does_item_exist(f"{ticker}_pct"):
                dpg.set_value(f"{ticker}_pct", f"{data['change_pct']:+.2f}%")
                apply_value_theme(f"{ticker}_pct", data['change_pct'])

        # Update chart if AAPL is selected
        if dpg.does_item_exist("main_chart_line") and dpg.get_value("chart_ticker") == "AAPL":
            dpg.set_value("main_chart_line", [list(range(len(market_data.historical_data['AAPL']))),
                                              market_data.historical_data['AAPL']])

    except Exception as e:
        print(f"UI update error: {e}")


# Start update threads
update_thread = threading.Thread(target=update_market_data, daemon=True)
update_thread.start()

# Setup and run
dpg.setup_dearpygui()
dpg.show_viewport()
dpg.set_primary_window("primary_window", True)
dpg.maximize_viewport()

# Main loop
while dpg.is_dearpygui_running():
    update_ui()
    dpg.render_dearpygui_frame()

dpg.destroy_context()