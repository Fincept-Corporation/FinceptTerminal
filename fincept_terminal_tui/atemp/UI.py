# bloomberg terminal code UI only nothing is working or getting realtime data this file will be reference for building the UI in our main terminal

import dearpygui.dearpygui as dpg
import random
import time
import datetime
from threading import Thread, Event
import math

# Initialize DearPyGUI
dpg.create_context()
dpg.create_viewport(title="Fincept Terminal", width=1600, height=900)

# Performance optimization: Use threading events for proper shutdown
shutdown_event = Event()

# Define Bloomberg colors
BLOOMBERG_ORANGE = [255, 165, 0]
BLOOMBERG_DARK = [25, 25, 25]
BLOOMBERG_BLACK = [0, 0, 0]
BLOOMBERG_WHITE = [255, 255, 255]
BLOOMBERG_RED = [255, 0, 0]
BLOOMBERG_GREEN = [0, 200, 0]
BLOOMBERG_YELLOW = [255, 255, 0]
BLOOMBERG_BLUE = [100, 150, 250]
BLOOMBERG_GRAY = [120, 120, 120]

# Generate dummy stock data
tickers = ["AAPL", "MSFT", "AMZN", "GOOGL", "META", "TSLA", "NVDA", "JPM", "V", "JNJ",
           "BAC", "PG", "MA", "UNH", "HD", "INTC", "VZ", "DIS", "PYPL", "NFLX",
           "ADBE", "CRM", "CSCO", "ABT", "T", "PEP", "KO", "NKE", "MRK", "PFE"]

stock_data = {}
historical_data = {}

# Generate dummy news headlines
news_headlines = [
    "Fed Signals Potential Rate Cuts Amid Economic Slowdown",
    "Tech Stocks Surge on Strong Earnings Reports",
    "Oil Prices Stabilize After OPEC+ Meeting",
    "Treasury Yields Hit New Heights as Inflation Concerns Rise",
    "Economic Data Points to Resilient Consumer Spending",
    "Global Markets React to Central Bank Policy Decisions",
    "Merger Talks Between Major Financial Institutions Confirmed",
    "Cryptocurrency Market Faces Regulatory Scrutiny",
    "Supply Chain Issues Continue to Impact Manufacturing Sector",
    "Retail Sales Exceed Expectations, Boosting Market Sentiment",
    "Healthcare Stocks Rally on Positive Clinical Trial Results",
    "Housing Market Shows Signs of Cooling as Mortgage Rates Rise",
    "Tech Antitrust Concerns Weigh on Market Leaders",
    "Earnings Season Begins with Mixed Results from Banking Sector",
    "Energy Crisis in Europe Impacts Global Commodity Prices"
]

# News ticker variables
ticker_position = 0
ticker_speed = 2
ticker_news = [
    "BREAKING: Fed maintains interest rates at 5.25-5.50% range",
    "MARKET ALERT: S&P 500 reaches new all-time high",
    "EARNINGS: AAPL beats Q4 estimates with $1.46 EPS vs $1.39 expected",
    "CRYPTO: Bitcoin surges past $45,000 amid institutional buying",
    "COMMODITIES: Gold hits $2,100/oz on inflation hedge demand",
    "FOREX: USD strengthens against major currencies",
    "ENERGY: WTI crude oil rises 2.3% on supply concerns",
    "TECH: AI semiconductor stocks rally on ChatGPT adoption",
    "BANKING: JPM, BAC report strong loan growth in Q4",
    "RETAIL: Holiday sales exceed expectations, up 4.2% YoY"
]

# Generate dummy market data
indices = {
    "S&P 500": {"value": 5232.35, "change": 0.78},
    "DOW JONES": {"value": 38546.12, "change": 0.42},
    "NASDAQ": {"value": 16412.87, "change": 1.25},
    "FTSE 100": {"value": 7623.98, "change": -0.31},
    "DAX": {"value": 18245.67, "change": 0.29},
    "NIKKEI 225": {"value": 35897.45, "change": 0.85},
    "HANG SENG": {"value": 18654.23, "change": -0.92},
}

# Generate dummy economic indicators
economic_indicators = {
    "US 10Y Treasury": {"value": 4.35, "change": 0.05},
    "US GDP Growth": {"value": 2.8, "change": 0.1},
    "US Unemployment": {"value": 3.6, "change": -0.1},
    "US Inflation Rate": {"value": 3.2, "change": -0.1},
    "EUR/USD": {"value": 1.084, "change": -0.002},
    "USD/JPY": {"value": 151.45, "change": 0.32},
    "Gold": {"value": 2312.80, "change": 15.60},
    "WTI Crude": {"value": 78.35, "change": -1.25},
}

# Initialize stock data
for ticker in tickers:
    price = round(random.uniform(50, 2000), 2)
    change_pct = round(random.uniform(-3, 3), 2)
    change_val = round(price * change_pct / 100, 2)
    volume = random.randint(500000, 10000000)
    high = round(price * (1 + random.uniform(0, 0.05)), 2)
    low = round(price * (1 - random.uniform(0, 0.05)), 2)
    open_price = round(price * (1 - random.uniform(-0.02, 0.02)), 2)
    pe_ratio = round(random.uniform(10, 40), 2)

    stock_data[ticker] = {
        "price": price,
        "change_pct": change_pct,
        "change_val": change_val,
        "volume": volume,
        "high": high,
        "low": low,
        "open": open_price,
        "pe_ratio": pe_ratio,
        "52w_high": round(price * (1 + random.uniform(0.05, 0.2)), 2),
        "52w_low": round(price * (1 - random.uniform(0.05, 0.3)), 2),
        "market_cap": round(price * random.uniform(1000000, 3000000000), 0),
        "dividend_yield": round(random.uniform(0, 4), 2),
    }

    # Generate historical data for charts (last 30 data points)
    historical_data[ticker] = []
    base_price = price * 0.85
    for i in range(30):
        day_price = base_price * (1 + random.uniform(-0.15, 0.25))
        historical_data[ticker].append(round(day_price, 2))

def create_ticker_text():
    """Create a continuous scrolling text from news items"""
    separator = "    ***    "
    full_text = separator.join(ticker_news)
    return full_text + separator  # Add separator at end for seamless loop

ticker_text = create_ticker_text()
ticker_display_width = 150  # Number of characters to display
ticker_full_length = len(ticker_text)

# Optimized update function with controlled frequency
update_counter = 0
def update_stock_prices():
    """Background thread for updating stock prices with controlled frequency"""
    global update_counter
    while not shutdown_event.is_set():
        try:
            update_counter += 1

            # Update all stocks but less frequently
            for ticker in tickers:
                change = random.uniform(-0.5, 0.5)
                old_price = stock_data[ticker]["price"]
                new_price = round(old_price * (1 + change / 100), 2)
                stock_data[ticker]["price"] = new_price
                stock_data[ticker]["change_val"] = round(new_price - stock_data[ticker]["open"], 2)
                stock_data[ticker]["change_pct"] = round(
                    (stock_data[ticker]["change_val"] / stock_data[ticker]["open"]) * 100, 2)

                # Update high and low
                if new_price > stock_data[ticker]["high"]:
                    stock_data[ticker]["high"] = new_price
                if new_price < stock_data[ticker]["low"]:
                    stock_data[ticker]["low"] = new_price

                # Update historical data less frequently
                if update_counter % 3 == 0:  # Every 3rd update
                    historical_data[ticker].append(new_price)
                    if len(historical_data[ticker]) > 30:
                        historical_data[ticker].pop(0)

            # Update indices less frequently
            if update_counter % 2 == 0:  # Every 2nd update
                for index in indices:
                    change = random.uniform(-0.15, 0.15)
                    indices[index]["change"] = round(indices[index]["change"] + change, 2)
                    indices[index]["value"] = round(indices[index]["value"] * (1 + indices[index]["change"] / 100), 2)

            # Update economic indicators even less frequently
            if update_counter % 4 == 0:  # Every 4th update
                for indicator in economic_indicators:
                    change = random.uniform(-0.05, 0.05)
                    if "USD" in indicator or "JPY" in indicator:
                        economic_indicators[indicator]["change"] = round(economic_indicators[indicator]["change"] + change, 3)
                        economic_indicators[indicator]["value"] = round(
                            economic_indicators[indicator]["value"] + economic_indicators[indicator]["change"] / 100, 3)
                    else:
                        economic_indicators[indicator]["change"] = round(economic_indicators[indicator]["change"] + change, 2)
                        economic_indicators[indicator]["value"] = round(
                            economic_indicators[indicator]["value"] + economic_indicators[indicator]["change"] / 20, 2)

            time.sleep(2.5)  # Slightly longer sleep for better performance

        except Exception as e:
            print(f"Error in update_stock_prices: {e}")
            time.sleep(1)

# Create a font registry with error handling
try:
    with dpg.font_registry():
        default_font = dpg.add_font("C:/Windows/Fonts/arial.ttf", 16)
        bold_font = dpg.add_font("C:/Windows/Fonts/arialbd.ttf", 16)
        big_font = dpg.add_font("C:/Windows/Fonts/arialbd.ttf", 20)
except:
    print("Warning: Could not load custom fonts, using default")

# Create a theme for Bloomberg-style
with dpg.theme() as global_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_WindowBg, BLOOMBERG_BLACK)
        dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive, BLOOMBERG_DARK)
        dpg.add_theme_color(dpg.mvThemeCol_TitleBg, BLOOMBERG_DARK)
        dpg.add_theme_color(dpg.mvThemeCol_Button, BLOOMBERG_DARK)
        dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Core)
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_WHITE, category=dpg.mvThemeCat_Core)
        dpg.add_theme_color(dpg.mvThemeCol_FrameBg, BLOOMBERG_DARK, category=dpg.mvThemeCat_Core)
        dpg.add_theme_color(dpg.mvThemeCol_PopupBg, BLOOMBERG_DARK, category=dpg.mvThemeCat_Core)
        dpg.add_theme_color(dpg.mvThemeCol_TableHeaderBg, BLOOMBERG_DARK, category=dpg.mvThemeCat_Core)
        dpg.add_theme_color(dpg.mvThemeCol_TableRowBg, [30, 30, 30], category=dpg.mvThemeCat_Core)
        dpg.add_theme_color(dpg.mvThemeCol_TableRowBgAlt, [40, 40, 40], category=dpg.mvThemeCat_Core)

dpg.bind_theme(global_theme)

# Create a theme for positive values (green)
with dpg.theme() as positive_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_GREEN, category=dpg.mvThemeCat_Core)

# Create a theme for negative values (red)
with dpg.theme() as negative_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_RED, category=dpg.mvThemeCat_Core)

# Create a theme for neutral values (white)
with dpg.theme() as neutral_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_WHITE, category=dpg.mvThemeCat_Core)

# Create a theme for headers (yellow)
with dpg.theme() as header_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_YELLOW, category=dpg.mvThemeCat_Core)

# Create orange theme for buttons and headers
with dpg.theme() as orange_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Core)

# Create a theme for charts
with dpg.theme() as chart_theme:
    with dpg.theme_component(dpg.mvLineSeries):
        dpg.add_theme_color(dpg.mvPlotCol_Line, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Plots)
    with dpg.theme_component(dpg.mvScatterSeries):
        dpg.add_theme_color(dpg.mvPlotCol_MarkerFill, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Plots)
        dpg.add_theme_color(dpg.mvPlotCol_MarkerOutline, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Plots)
    with dpg.theme_component(dpg.mvPlot):
        dpg.add_theme_color(dpg.mvPlotCol_FrameBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Plots)
        dpg.add_theme_color(dpg.mvPlotCol_PlotBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Plots)
        dpg.add_theme_color(dpg.mvPlotCol_PlotBorder, BLOOMBERG_DARK, category=dpg.mvThemeCat_Plots)

# Main window layout
with dpg.window(label="Fincept Professional", tag="primary_window", no_collapse=True):
    # Top bar with search and Bloomberg branding
    with dpg.group(horizontal=True):
        dpg.add_text("FINCEPT", color=BLOOMBERG_ORANGE)
        dpg.add_text("PROFESSIONAL", color=BLOOMBERG_WHITE)
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_input_text(label="", default_value="Enter Command", width=300)
        dpg.add_button(label="Search", width=80)
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_text(f"{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}", tag="time_display")

    dpg.add_separator()

    # Function keys (F1-F12) at the top
    with dpg.group(horizontal=True):
        function_keys = ["F1:HELP", "F2:MARKETS", "F3:NEWS", "F4:PORT", "F5:MOVERS", "F6:ECON", "F7:CHARTS",
                         "F8:CORP", "F9:GOVT", "F10:CMDY", "F11:FX", "F12:SETTINGS"]
        for key in function_keys:
            dpg.add_button(label=key, width=80)

    dpg.add_separator()

    # Main work area (using a splitter)
    with dpg.group(horizontal=True):
        # Left panel (Market Monitor)
        with dpg.child_window(width=380, height=650, border=False):
            dpg.add_text("MARKET MONITOR", color=BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Market indices table
            dpg.add_text("GLOBAL INDICES", color=BLOOMBERG_YELLOW)
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True, borders_innerV=True,
                           borders_outerV=True):
                dpg.add_table_column(label="Index")
                dpg.add_table_column(label="Value")
                dpg.add_table_column(label="Change %")

                for index, data in indices.items():
                    with dpg.table_row():
                        dpg.add_text(index)
                        dpg.add_text(f"{data['value']:.2f}", tag=f"idx_{index}_val")

                        chg_tag = f"idx_{index}_chg"
                        dpg.add_text(f"{data['change']:+.2f}%", tag=chg_tag)

                        if data['change'] > 0:
                            dpg.bind_item_theme(chg_tag, positive_theme)
                        elif data['change'] < 0:
                            dpg.bind_item_theme(chg_tag, negative_theme)
                        else:
                            dpg.bind_item_theme(chg_tag, neutral_theme)

            dpg.add_separator()

            # Economic indicators
            dpg.add_text("ECONOMIC INDICATORS", color=BLOOMBERG_YELLOW)
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True, borders_innerV=True,
                           borders_outerV=True):
                dpg.add_table_column(label="Indicator")
                dpg.add_table_column(label="Value")
                dpg.add_table_column(label="Change")

                for indicator, data in economic_indicators.items():
                    with dpg.table_row():
                        dpg.add_text(indicator)
                        dpg.add_text(f"{data['value']:.2f}", tag=f"econ_{indicator}_val")

                        chg_tag = f"econ_{indicator}_chg"
                        dpg.add_text(f"{data['change']:+.2f}", tag=chg_tag)

                        if data['change'] > 0:
                            dpg.bind_item_theme(chg_tag, positive_theme)
                        elif data['change'] < 0:
                            dpg.bind_item_theme(chg_tag, negative_theme)
                        else:
                            dpg.bind_item_theme(chg_tag, neutral_theme)

            dpg.add_separator()

            # Recent news headlines
            dpg.add_text("LATEST NEWS", color=BLOOMBERG_YELLOW)
            for i, headline in enumerate(news_headlines[:8]):
                dpg.add_text(f"{datetime.datetime.now().strftime('%H:%M')} - {headline}", wrap=380, tag=f"news_{i}")

        # Center panel (Market Data and Charts)
        with dpg.child_window(width=800, height=650, border=False):
            with dpg.tab_bar():
                # Market Data Tab
                with dpg.tab(label="Market Data"):
                    dpg.add_text("TOP STOCKS", color=BLOOMBERG_ORANGE)

                    # Stock ticker table
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True, borders_innerV=True,
                                   borders_outerV=True, scrollY=True, height=250):
                        dpg.add_table_column(label="Ticker")
                        dpg.add_table_column(label="Last")
                        dpg.add_table_column(label="Chg")
                        dpg.add_table_column(label="Chg%")
                        dpg.add_table_column(label="Volume")
                        dpg.add_table_column(label="High")
                        dpg.add_table_column(label="Low")
                        dpg.add_table_column(label="Open")

                        for ticker in tickers:
                            data = stock_data[ticker]
                            with dpg.table_row(tag=f"row_{ticker}"):
                                dpg.add_text(ticker)
                                dpg.add_text(f"{data['price']:.2f}", tag=f"{ticker}_price")

                                chg_tag = f"{ticker}_chg"
                                dpg.add_text(f"{data['change_val']:+.2f}", tag=chg_tag)

                                chg_pct_tag = f"{ticker}_chg_pct"
                                dpg.add_text(f"{data['change_pct']:+.2f}%", tag=chg_pct_tag)

                                dpg.add_text(f"{data['volume']:,}", tag=f"{ticker}_volume")
                                dpg.add_text(f"{data['high']:.2f}", tag=f"{ticker}_high")
                                dpg.add_text(f"{data['low']:.2f}", tag=f"{ticker}_low")
                                dpg.add_text(f"{data['open']:.2f}", tag=f"{ticker}_open")

                                if data['change_pct'] > 0:
                                    dpg.bind_item_theme(chg_tag, positive_theme)
                                    dpg.bind_item_theme(chg_pct_tag, positive_theme)
                                elif data['change_pct'] < 0:
                                    dpg.bind_item_theme(chg_tag, negative_theme)
                                    dpg.bind_item_theme(chg_pct_tag, negative_theme)
                                else:
                                    dpg.bind_item_theme(chg_tag, neutral_theme)
                                    dpg.bind_item_theme(chg_pct_tag, neutral_theme)

                    # Stock details section
                    dpg.add_text("STOCK DETAILS", color=BLOOMBERG_ORANGE)
                    with dpg.group(horizontal=True):
                        dpg.add_input_text(label="Ticker", default_value="AAPL", width=150, tag="detail_ticker")
                        dpg.add_button(label="Load", callback=lambda: None)

                    # Details for the selected stock
                    with dpg.group(horizontal=True):
                        # Left column
                        with dpg.group():
                            dpg.add_text("Apple Inc (AAPL US Equity)", tag="detail_name", color=BLOOMBERG_ORANGE)
                            dpg.add_text("Technology - Consumer Electronics", tag="detail_sector")

                            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                                dpg.add_table_column()
                                dpg.add_table_column()

                                with dpg.table_row():
                                    dpg.add_text("Last Price:")
                                    dpg.add_text(f"{stock_data['AAPL']['price']:.2f}", tag="detail_price")
                                with dpg.table_row():
                                    dpg.add_text("Change:")
                                    chg_tag = "detail_change"
                                    dpg.add_text(
                                        f"{stock_data['AAPL']['change_val']:+.2f} ({stock_data['AAPL']['change_pct']:+.2f}%)",
                                        tag=chg_tag)
                                    if stock_data['AAPL']['change_pct'] > 0:
                                        dpg.bind_item_theme(chg_tag, positive_theme)
                                    elif stock_data['AAPL']['change_pct'] < 0:
                                        dpg.bind_item_theme(chg_tag, negative_theme)
                                with dpg.table_row():
                                    dpg.add_text("Open:")
                                    dpg.add_text(f"{stock_data['AAPL']['open']:.2f}", tag="detail_open")
                                with dpg.table_row():
                                    dpg.add_text("High:")
                                    dpg.add_text(f"{stock_data['AAPL']['high']:.2f}", tag="detail_high")
                                with dpg.table_row():
                                    dpg.add_text("Low:")
                                    dpg.add_text(f"{stock_data['AAPL']['low']:.2f}", tag="detail_low")
                                with dpg.table_row():
                                    dpg.add_text("Volume:")
                                    dpg.add_text(f"{stock_data['AAPL']['volume']:,}", tag="detail_volume")

                        # Right column
                        with dpg.group():
                            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                                dpg.add_table_column()
                                dpg.add_table_column()

                                with dpg.table_row():
                                    dpg.add_text("Market Cap:")
                                    dpg.add_text(f"${stock_data['AAPL']['market_cap'] / 1000000000:.2f}B",
                                                 tag="detail_mcap")
                                with dpg.table_row():
                                    dpg.add_text("P/E Ratio:")
                                    dpg.add_text(f"{stock_data['AAPL']['pe_ratio']:.2f}", tag="detail_pe")
                                with dpg.table_row():
                                    dpg.add_text("52W High:")
                                    dpg.add_text(f"{stock_data['AAPL']['52w_high']:.2f}", tag="detail_52wh")
                                with dpg.table_row():
                                    dpg.add_text("52W Low:")
                                    dpg.add_text(f"{stock_data['AAPL']['52w_low']:.2f}", tag="detail_52wl")
                                with dpg.table_row():
                                    dpg.add_text("Dividend Yield:")
                                    dpg.add_text(f"{stock_data['AAPL']['dividend_yield']:.2f}%",
                                                 tag="detail_yield")

                    # Chart for selected stock
                    dpg.add_text("PRICE CHART", color=BLOOMBERG_ORANGE)
                    with dpg.plot(height=200, width=-1, tag="detail_chart"):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Days", tag="detail_x_axis")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Price", tag="detail_y_axis")
                        dpg.add_line_series(list(range(len(historical_data["AAPL"]))),
                                            historical_data["AAPL"],
                                            label="AAPL",
                                            parent=y_axis,
                                            tag="detail_line")
                    dpg.bind_item_theme("detail_chart", chart_theme)

                # Charting Tab
                with dpg.tab(label="Charting"):
                    dpg.add_text("ADVANCED CHARTS", color=BLOOMBERG_ORANGE)
                    with dpg.group(horizontal=True):
                        dpg.add_combo(["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"], default_value="AAPL",
                                      width=150, tag="chart_ticker")
                        dpg.add_combo(["1D", "5D", "1M", "3M", "6M", "1Y", "5Y"], default_value="1M",
                                      width=100, tag="chart_timeframe")
                        dpg.add_combo(["Line", "Candlestick", "OHLC"], default_value="Line", width=120,
                                      tag="chart_type")
                        dpg.add_button(label="Update Chart", callback=lambda: None)

                    # Main chart
                    with dpg.plot(height=400, width=-1, tag="main_chart"):
                        dpg.add_plot_legend()
                        dpg.add_plot_axis(dpg.mvXAxis, label="Time", tag="main_x_axis")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Price", tag="main_y_axis")
                        dpg.add_line_series(list(range(len(historical_data["AAPL"]))),
                                            historical_data["AAPL"],
                                            label="AAPL",
                                            parent=y_axis,
                                            tag="main_line")
                        min_val = min(historical_data["AAPL"]) * 0.95
                        max_val = max(historical_data["AAPL"]) * 1.05
                        dpg.set_axis_limits(y_axis, min_val, max_val)
                    dpg.bind_item_theme("main_chart", chart_theme)

                    # Technical analysis section
                    dpg.add_text("TECHNICAL INDICATORS", color=BLOOMBERG_ORANGE)
                    with dpg.group(horizontal=True):
                        with dpg.group():
                            dpg.add_text("Moving Averages", color=BLOOMBERG_YELLOW)
                            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                                dpg.add_table_column(label="Period")
                                dpg.add_table_column(label="Value")
                                dpg.add_table_column(label="Signal")

                                ma_periods = [20, 50, 100, 200]
                                signals = ["Buy", "Buy", "Neutral", "Sell"]
                                colors = [BLOOMBERG_GREEN, BLOOMBERG_GREEN, BLOOMBERG_WHITE, BLOOMBERG_RED]

                                for i, period in enumerate(ma_periods):
                                    with dpg.table_row():
                                        dpg.add_text(f"MA {period}")
                                        dpg.add_text(
                                            f"{stock_data['AAPL']['price'] * (0.9 + i * 0.05):.2f}")
                                        signal_tag = f"signal_ma_{period}"
                                        dpg.add_text(signals[i], tag=signal_tag)
                                        with dpg.theme() as signal_theme:
                                            with dpg.theme_component(dpg.mvAll):
                                                dpg.add_theme_color(dpg.mvThemeCol_Text, colors[i],
                                                                    category=dpg.mvThemeCat_Core)
                                        dpg.bind_item_theme(signal_tag, signal_theme)
                        with dpg.group():
                            dpg.add_text("Oscillators", color=BLOOMBERG_YELLOW)
                            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                                dpg.add_table_column(label="Indicator")
                                dpg.add_table_column(label="Value")
                                dpg.add_table_column(label="Signal")

                                indicators = ["RSI(14)", "MACD", "Stochastic", "CCI"]
                                values = ["65.42", "2.15", "75.30", "124.5"]
                                signals = ["Neutral", "Buy", "Sell", "Buy"]
                                colors = [BLOOMBERG_WHITE, BLOOMBERG_GREEN, BLOOMBERG_RED, BLOOMBERG_GREEN]

                                for i, indicator in enumerate(indicators):
                                    with dpg.table_row():
                                        dpg.add_text(indicator)
                                        dpg.add_text(values[i])
                                        signal_tag = f"signal_osc_{i}"
                                        dpg.add_text(signals[i], tag=signal_tag)
                                        with dpg.theme() as signal_theme:
                                            with dpg.theme_component(dpg.mvAll):
                                                dpg.add_theme_color(dpg.mvThemeCol_Text, colors[i],
                                                                    category=dpg.mvThemeCat_Core)
                                        dpg.bind_item_theme(signal_tag, signal_theme)
                # News Tab
                with dpg.tab(label="News"):
                    dpg.add_text("FINANCIAL NEWS", color=BLOOMBERG_ORANGE)
                    with dpg.group(horizontal=True):
                        dpg.add_input_text(label="Search", width=300)
                        dpg.add_button(label="Go")
                    with dpg.group(horizontal=True):
                        categories = ["All", "Markets", "Economics", "Companies", "Industries",
                                      "Technology", "Politics"]
                        for category in categories:
                            dpg.add_button(label=category, width=80)
                    dpg.add_separator()
                    for i, headline in enumerate(news_headlines):
                        timestamp = (datetime.datetime.now() - datetime.timedelta(
                            minutes=random.randint(5, 500))).strftime("%Y-%m-%d %H:%M")
                        with dpg.group(tag=f"news_item_{i}"):
                            dpg.add_text(headline, color=BLOOMBERG_ORANGE, tag=f"news_headline_{i}")
                            dpg.add_text(timestamp, tag=f"news_time_{i}", color=BLOOMBERG_GRAY)
                            summary = ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                                       "Nullam ac elit in urna convallis faucibus. Integer vel magna vitae "
                                       "nunc convallis tempus id in lectus. Sed vestibulum erat sit amet leo "
                                       "pulvinar, et condimentum tellus lobortis.")
                            dpg.add_text(summary, wrap=750, tag=f"news_summary_{i}")
                            dpg.add_separator()
                # Portfolio Tab
                with dpg.tab(label="Portfolio"):
                    dpg.add_text("PORTFOLIO OVERVIEW", color=BLOOMBERG_ORANGE)
                    with dpg.group():
                        with dpg.group(horizontal=True):
                            with dpg.child_window(width=375, height=100, border=False):
                                with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                                    dpg.add_table_column()
                                    dpg.add_table_column()
                                    with dpg.table_row():
                                        dpg.add_text("Total Value:")
                                        dpg.add_text("$1,254,367.89", color=BLOOMBERG_WHITE)
                                    with dpg.table_row():
                                        dpg.add_text("Day Change:")
                                        chg_tag = "port_day_change"
                                        dpg.add_text("+$15,243.56 (+1.23%)", tag=chg_tag,
                                                     color=BLOOMBERG_GREEN)
                                    with dpg.table_row():
                                        dpg.add_text("YTD Return:")
                                        ytd_tag = "port_ytd_return"
                                        dpg.add_text("+8.67%", tag=ytd_tag, color=BLOOMBERG_GREEN)
                            with dpg.child_window(width=375, height=100, border=False):
                                with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                                    dpg.add_table_column()
                                    dpg.add_table_column()
                                    with dpg.table_row():
                                        dpg.add_text("Cash Balance:")
                                        dpg.add_text("$245,632.11", color=BLOOMBERG_WHITE)
                                    with dpg.table_row():
                                        dpg.add_text("Unrealized P/L:")
                                        pnl_tag = "port_unrealized_pnl"
                                        dpg.add_text("+$124,367.85", tag=pnl_tag, color=BLOOMBERG_GREEN)
                                    with dpg.table_row():
                                        dpg.add_text("Dividend Yield:")
                                        dpg.add_text("2.34%", color=BLOOMBERG_WHITE)
                    dpg.add_text("HOLDINGS", color=BLOOMBERG_ORANGE)
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True, scrollY=True,
                                   height=250):
                        dpg.add_table_column(label="Ticker")
                        dpg.add_table_column(label="Shares")
                        dpg.add_table_column(label="Avg Price")
                        dpg.add_table_column(label="Current")
                        dpg.add_table_column(label="Market Value")
                        dpg.add_table_column(label="Day Chg")
                        dpg.add_table_column(label="Total Chg")
                        dpg.add_table_column(label="% of Port")
                        holdings = [
                            {"ticker": "AAPL", "shares": 500, "avg_price": 155.75},
                            {"ticker": "MSFT", "shares": 300, "avg_price": 305.25},
                            {"ticker": "AMZN", "shares": 150, "avg_price": 135.50},
                            {"ticker": "GOOGL", "shares": 100, "avg_price": 130.25},
                            {"ticker": "TSLA", "shares": 200, "avg_price": 220.75},
                            {"ticker": "NVDA", "shares": 250, "avg_price": 210.50},
                            {"ticker": "META", "shares": 175, "avg_price": 310.25},
                            {"ticker": "JPM", "shares": 200, "avg_price": 150.25},
                            {"ticker": "V", "shares": 150, "avg_price": 230.75},
                            {"ticker": "JNJ", "shares": 125, "avg_price": 165.50}
                        ]
                        total_port_value = 0
                        for holding in holdings:
                            ticker = holding["ticker"]
                            shares = holding["shares"]
                            avg_price = holding["avg_price"]
                            current_price = stock_data[ticker]["price"]
                            market_value = shares * current_price
                            total_port_value += market_value
                            day_chg_pct = stock_data[ticker]["change_pct"]
                            day_chg_val = market_value * day_chg_pct / 100
                            total_chg_pct = (current_price - avg_price) / avg_price * 100
                            total_chg_val = shares * (current_price - avg_price)
                            with dpg.table_row():
                                dpg.add_text(ticker)
                                dpg.add_text(f"{shares:,}")
                                dpg.add_text(f"${avg_price:.2f}")
                                dpg.add_text(f"${current_price:.2f}")
                                dpg.add_text(f"${market_value:,.2f}")
                                day_chg_tag = f"holding_{ticker}_day_chg"
                                dpg.add_text(f"${day_chg_val:+,.2f} ({day_chg_pct:+.2f}%)", tag=day_chg_tag)
                                total_chg_tag = f"holding_{ticker}_total_chg"
                                dpg.add_text(f"${total_chg_val:+,.2f} ({total_chg_pct:+.2f}%)",
                                             tag=total_chg_tag)
                                dpg.add_text(f"{market_value / 1254367.89 * 100:.2f}%")
                                if day_chg_pct > 0:
                                    dpg.bind_item_theme(day_chg_tag, positive_theme)
                                else:
                                    dpg.bind_item_theme(day_chg_tag, negative_theme)
                                if total_chg_pct > 0:
                                    dpg.bind_item_theme(total_chg_tag, positive_theme)
                                else:
                                    dpg.bind_item_theme(total_chg_tag, negative_theme)
                    dpg.add_text("ASSET ALLOCATION", color=BLOOMBERG_ORANGE)
                    with dpg.group(horizontal=True):
                        with dpg.child_window(width=375, height=200):
                            dpg.add_text("SECTORS", color=BLOOMBERG_YELLOW)
                            sectors = [
                                {"name": "Technology", "percent": 45.5},
                                {"name": "Financial", "percent": 15.8},
                                {"name": "Healthcare", "percent": 12.3},
                                {"name": "Consumer Cyclical", "percent": 10.5},
                                {"name": "Communication", "percent": 8.7},
                                {"name": "Others", "percent": 7.2}
                            ]
                            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                                dpg.add_table_column(label="Sector")
                                dpg.add_table_column(label="Allocation")
                                for sector in sectors:
                                    with dpg.table_row():
                                        dpg.add_text(sector["name"])
                                        dpg.add_text(f"{sector['percent']:.1f}%")
                        with dpg.child_window(width=375, height=200):
                            dpg.add_text("ASSET TYPES", color=BLOOMBERG_YELLOW)
                            asset_types = [
                                {"name": "Stocks", "percent": 72.5},
                                {"name": "Bonds", "percent": 15.2},
                                {"name": "Cash", "percent": 7.8},
                                {"name": "ETFs", "percent": 3.5},
                                {"name": "Options", "percent": 1.0}
                            ]
                            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                                dpg.add_table_column(label="Type")
                                dpg.add_table_column(label="Allocation")
                                for asset in asset_types:
                                    with dpg.table_row():
                                        dpg.add_text(asset["name"])
                                        dpg.add_text(f"{asset['percent']:.1f}%")
        # Right panel (Command Line/Terminal)
        with dpg.child_window(width=400, height=650, border=False):
            dpg.add_text("COMMAND LINE", color=BLOOMBERG_ORANGE)
            dpg.add_separator()
            # Command history window
            with dpg.child_window(height=250, border=True):
                dpg.add_text("> AAPL US Equity <GO>", color=BLOOMBERG_WHITE)
                dpg.add_text("  Loading AAPL US Equity...", color=BLOOMBERG_GRAY)
                dpg.add_text("> AAPL US Equity DES <GO>", color=BLOOMBERG_WHITE)
                dpg.add_text("  Loading company description...", color=BLOOMBERG_GRAY)
                dpg.add_text("> TOP <GO>", color=BLOOMBERG_WHITE)
                dpg.add_text("  Loading TOP news...", color=BLOOMBERG_GRAY)
                dpg.add_text("> AAPL US Equity GP <GO>", color=BLOOMBERG_WHITE)
                dpg.add_text("  Loading price graph...", color=BLOOMBERG_GRAY)
                dpg.add_text("> WEI <GO>", color=BLOOMBERG_WHITE)
                dpg.add_text("  Loading World Equity Indices...", color=BLOOMBERG_GRAY)
            # Command input
            dpg.add_input_text(label=">", width=-1, tag="cmd_input")
            dpg.add_text("<HELP> for available commands. Press <GO> to execute.", color=BLOOMBERG_GRAY)
            dpg.add_separator()
            # Quick commands
            dpg.add_text("COMMON COMMANDS", color=BLOOMBERG_ORANGE)
            with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                dpg.add_table_column()
                dpg.add_table_column()
                with dpg.table_row():
                    dpg.add_text("HELP")
                    dpg.add_text("Show available commands")
                with dpg.table_row():
                    dpg.add_text("DES")
                    dpg.add_text("Company description")
                with dpg.table_row():
                    dpg.add_text("GP")
                    dpg.add_text("Price graph")
                with dpg.table_row():
                    dpg.add_text("TOP")
                    dpg.add_text("Top news headlines")

    # Bottom section - News ticker and status bar
    dpg.add_separator()

    # News ticker section
    with dpg.group():
        dpg.add_text("LIVE NEWS TICKER", color=BLOOMBERG_ORANGE)
        with dpg.child_window(height=60, border=True, tag="news_ticker_window"):
            dpg.add_text("", tag="scrolling_news", wrap=0)

    dpg.add_separator()

    # Status bar
    with dpg.group(horizontal=True, tag="status_bar"):
        dpg.add_text("●", color=BLOOMBERG_GREEN, tag="connection_indicator")
        dpg.add_text("CONNECTED", color=BLOOMBERG_GREEN, tag="connection_status")
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_text("LIVE DATA", color=BLOOMBERG_ORANGE)
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)

        market_open = datetime.datetime.now().hour >= 9 and datetime.datetime.now().hour < 16
        if market_open:
            dpg.add_text("MARKET OPEN", color=BLOOMBERG_GREEN, tag="market_status")
        else:
            dpg.add_text("MARKET CLOSED", color=BLOOMBERG_RED, tag="market_status")
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)

        dpg.add_text("SERVER: NY-01", color=BLOOMBERG_WHITE)
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_text("USER: TRADER001", color=BLOOMBERG_WHITE)
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_text("LAST UPDATE:", color=BLOOMBERG_GRAY)
        dpg.add_text(datetime.datetime.now().strftime("%H:%M:%S"), color=BLOOMBERG_WHITE, tag="last_update_time")
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_text("LATENCY: 12ms", color=BLOOMBERG_GREEN, tag="latency_indicator")

def update_news_ticker():
    """Update scrolling news ticker"""
    global ticker_position
    try:
        display_text = ticker_text[ticker_position:ticker_position + ticker_display_width]
        if ticker_position + ticker_display_width >= ticker_full_length:
            remaining = ticker_display_width - (ticker_full_length - ticker_position)
            display_text += ticker_text[:remaining]

        if dpg.does_item_exist("scrolling_news"):
            dpg.set_value("scrolling_news", display_text)

        ticker_position += ticker_speed
        if ticker_position >= ticker_full_length:
            ticker_position = 0
    except Exception as e:
        print(f"Error updating ticker: {e}")

def update_status_bar():
    """Update status bar elements"""
    try:
        current_time = datetime.datetime.now()

        if dpg.does_item_exist("last_update_time"):
            dpg.set_value("last_update_time", current_time.strftime("%H:%M:%S"))

        # Update market status based on time
        market_open = current_time.hour >= 9 and current_time.hour < 16
        if dpg.does_item_exist("market_status"):
            if market_open:
                dpg.set_value("market_status", "MARKET OPEN")
                dpg.bind_item_theme("market_status", positive_theme)
            else:
                dpg.set_value("market_status", "MARKET CLOSED")
                dpg.bind_item_theme("market_status", negative_theme)

        # Simulate connection status changes (rarely)
        if random.random() < 0.001:  # Very low chance
            if dpg.does_item_exist("connection_status") and dpg.get_value("connection_status") == "CONNECTED":
                dpg.set_value("connection_status", "RECONNECTING...")
                dpg.set_value("connection_indicator", "●")
                dpg.bind_item_theme("connection_status", negative_theme)
                dpg.bind_item_theme("connection_indicator", negative_theme)
            else:
                if dpg.does_item_exist("connection_status"):
                    dpg.set_value("connection_status", "CONNECTED")
                    dpg.set_value("connection_indicator", "●")
                    dpg.bind_item_theme("connection_status", positive_theme)
                    dpg.bind_item_theme("connection_indicator", positive_theme)

        # Update latency with realistic variation
        if random.random() < 0.1:  # 10% chance to update latency
            latency = random.randint(8, 25)
            latency_color = BLOOMBERG_GREEN if latency < 15 else BLOOMBERG_YELLOW if latency < 20 else BLOOMBERG_RED
            if dpg.does_item_exist("latency_indicator"):
                dpg.set_value("latency_indicator", f"LATENCY: {latency}ms")

                with dpg.theme() as latency_theme:
                    with dpg.theme_component(dpg.mvAll):
                        dpg.add_theme_color(dpg.mvThemeCol_Text, latency_color, category=dpg.mvThemeCat_Core)
                dpg.bind_item_theme("latency_indicator", latency_theme)
    except Exception as e:
        print(f"Error updating status bar: {e}")

# Optimized UI update function with controlled frequency
def update_ui():
    """Function to update UI elements based on new data with performance optimization"""
    try:
        # Update time display
        if dpg.does_item_exist("time_display"):
            dpg.set_value("time_display", datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

        # Update news ticker and status bar
        update_news_ticker()
        update_status_bar()

        # Update indices table (every call but with existence check)
        for index, data in indices.items():
            if dpg.does_item_exist(f"idx_{index}_val"):
                dpg.set_value(f"idx_{index}_val", f"{data['value']:.2f}")
                dpg.set_value(f"idx_{index}_chg", f"{data['change']:+.2f}%")
                chg_tag = f"idx_{index}_chg"
                if data['change'] > 0:
                    dpg.bind_item_theme(chg_tag, positive_theme)
                elif data['change'] < 0:
                    dpg.bind_item_theme(chg_tag, negative_theme)
                else:
                    dpg.bind_item_theme(chg_tag, neutral_theme)

        # Update economic indicators
        for indicator, data in economic_indicators.items():
            if dpg.does_item_exist(f"econ_{indicator}_val"):
                dpg.set_value(f"econ_{indicator}_val", f"{data['value']:.2f}")
                dpg.set_value(f"econ_{indicator}_chg", f"{data['change']:+.2f}")
                chg_tag = f"econ_{indicator}_chg"
                if data['change'] > 0:
                    dpg.bind_item_theme(chg_tag, positive_theme)
                elif data['change'] < 0:
                    dpg.bind_item_theme(chg_tag, negative_theme)
                else:
                    dpg.bind_item_theme(chg_tag, neutral_theme)

        # Update stock table
        for ticker in tickers:
            data = stock_data[ticker]
            if dpg.does_item_exist(f"{ticker}_price"):
                dpg.set_value(f"{ticker}_price", f"{data['price']:.2f}")
                dpg.set_value(f"{ticker}_chg", f"{data['change_val']:+.2f}")
                dpg.set_value(f"{ticker}_chg_pct", f"{data['change_pct']:+.2f}%")
                dpg.set_value(f"{ticker}_volume", f"{data['volume']:,}")
                dpg.set_value(f"{ticker}_high", f"{data['high']:.2f}")
                dpg.set_value(f"{ticker}_low", f"{data['low']:.2f}")
                dpg.set_value(f"{ticker}_open", f"{data['open']:.2f}")

            # Update details for AAPL in the details panel
            if ticker == "AAPL":
                if dpg.does_item_exist("detail_price"):
                    dpg.set_value("detail_price", f"{data['price']:.2f}")
                    dpg.set_value("detail_change", f"{data['change_val']:+.2f} ({data['change_pct']:+.2f}%)")
                    dpg.set_value("detail_open", f"{data['open']:.2f}")
                    dpg.set_value("detail_high", f"{data['high']:.2f}")
                    dpg.set_value("detail_low", f"{data['low']:.2f}")
                    dpg.set_value("detail_volume", f"{data['volume']:,}")

        # Update charts for AAPL in detail and charting tabs (less frequently)
        if dpg.does_item_exist("detail_line") and random.random() < 0.3:  # 30% chance
            dpg.set_value("detail_line", [list(range(len(historical_data["AAPL"]))), historical_data["AAPL"]])
        if dpg.does_item_exist("main_line") and random.random() < 0.3:  # 30% chance
            dpg.set_value("main_line", [list(range(len(historical_data["AAPL"]))), historical_data["AAPL"]])

    except Exception as e:
        print(f"Error in update_ui: {e}")

# Optimized UI update loop running in a separate thread
def ui_update_loop():
    """UI update loop with better performance control"""
    while not shutdown_event.is_set():
        try:
            if dpg.is_dearpygui_running():
                # Use split_frame for better performance than invoke
                dpg.split_frame()
                update_ui()
                time.sleep(1.0)  # 1 second update interval for better performance
            else:
                break
        except Exception as e:
            print(f"Error in ui_update_loop: {e}")
            time.sleep(1)

# Start background threads with proper error handling
try:
    price_thread = Thread(target=update_stock_prices, daemon=True)
    price_thread.start()

    ui_thread = Thread(target=ui_update_loop, daemon=True)
    ui_thread.start()

    # Setup and start the DearPyGUI viewport
    dpg.setup_dearpygui()
    dpg.show_viewport()
    dpg.toggle_viewport_fullscreen()  # Restored fullscreen
    dpg.set_primary_window("primary_window", True)
    dpg.start_dearpygui()

except Exception as e:
    print(f"Error starting application: {e}")
finally:
    shutdown_event.set()  # Signal threads to stop
    dpg.destroy_context()