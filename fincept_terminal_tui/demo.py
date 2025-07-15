# Optimized Bloomberg Terminal Code with Performance Improvements
import dearpygui.dearpygui as dpg
import random
import time
import datetime
from threading import Thread, Event
import math
import queue

# Initialize DearPyGUI
dpg.create_context()
dpg.create_viewport(title="Fincept Terminal", width=1600, height=900)

# Performance optimization: Use threading events and queues
update_event = Event()
data_queue = queue.Queue(maxsize=100)

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

# Reduced dataset for better performance
tickers = ["AAPL", "MSFT", "AMZN", "GOOGL", "META", "TSLA", "NVDA", "JPM", "V", "JNJ",
           "BAC", "PG", "MA", "UNH", "HD"]  # Reduced from 30 to 15

stock_data = {}
historical_data = {}

# Reduced news headlines for better performance
news_headlines = [
    "Fed Signals Potential Rate Cuts Amid Economic Slowdown",
    "Tech Stocks Surge on Strong Earnings Reports",
    "Oil Prices Stabilize After OPEC+ Meeting",
    "Treasury Yields Hit New Heights as Inflation Concerns Rise",
    "Economic Data Points to Resilient Consumer Spending",
    "Global Markets React to Central Bank Policy Decisions",
    "Merger Talks Between Major Financial Institutions Confirmed",
    "Cryptocurrency Market Faces Regulatory Scrutiny"
]

# Optimized news ticker variables
ticker_position = 0
ticker_speed = 1  # Reduced speed
ticker_news = [
    "BREAKING: Fed maintains interest rates at 5.25-5.50% range",
    "MARKET ALERT: S&P 500 reaches new all-time high",
    "EARNINGS: AAPL beats Q4 estimates with $1.46 EPS vs $1.39 expected",
    "CRYPTO: Bitcoin surges past $45,000 amid institutional buying",
    "COMMODITIES: Gold hits $2,100/oz on inflation hedge demand"
]

# Market data (unchanged)
indices = {
    "S&P 500": {"value": 5232.35, "change": 0.78},
    "DOW JONES": {"value": 38546.12, "change": 0.42},
    "NASDAQ": {"value": 16412.87, "change": 1.25},
    "FTSE 100": {"value": 7623.98, "change": -0.31},
    "DAX": {"value": 18245.67, "change": 0.29},
    "NIKKEI 225": {"value": 35897.45, "change": 0.85}
}

economic_indicators = {
    "US 10Y Treasury": {"value": 4.35, "change": 0.05},
    "US GDP Growth": {"value": 2.8, "change": 0.1},
    "US Unemployment": {"value": 3.6, "change": -0.1},
    "US Inflation Rate": {"value": 3.2, "change": -0.1},
    "EUR/USD": {"value": 1.084, "change": -0.002},
    "USD/JPY": {"value": 151.45, "change": 0.32}
}

# Initialize stock data (optimized)
for ticker in tickers:
    price = round(random.uniform(50, 2000), 2)
    change_pct = round(random.uniform(-3, 3), 2)
    change_val = round(price * change_pct / 100, 2)
    volume = random.randint(500000, 10000000)
    high = round(price * (1 + random.uniform(0, 0.05)), 2)
    low = round(price * (1 - random.uniform(0, 0.05)), 2)
    open_price = round(price * (1 - random.uniform(-0.02, 0.02)), 2)

    stock_data[ticker] = {
        "price": price,
        "change_pct": change_pct,
        "change_val": change_val,
        "volume": volume,
        "high": high,
        "low": low,
        "open": open_price,
        "pe_ratio": round(random.uniform(10, 40), 2),
        "52w_high": round(price * (1 + random.uniform(0.05, 0.2)), 2),
        "52w_low": round(price * (1 - random.uniform(0.05, 0.3)), 2),
        "market_cap": round(price * random.uniform(1000000, 3000000000), 0),
        "dividend_yield": round(random.uniform(0, 4), 2),
    }

    # Reduced historical data points for better performance
    historical_data[ticker] = []
    base_price = price * 0.85
    for i in range(20):  # Reduced from 30 to 20
        day_price = base_price * (1 + random.uniform(-0.15, 0.25))
        historical_data[ticker].append(round(day_price, 2))

def create_ticker_text():
    """Create a continuous scrolling text from news items"""
    separator = "    ***    "
    full_text = separator.join(ticker_news)
    return full_text + separator

ticker_text = create_ticker_text()
ticker_display_width = 150
ticker_full_length = len(ticker_text)

# Optimized update function with reduced frequency and batch updates
def update_stock_prices():
    """Background thread for updating stock prices with reduced frequency"""
    while not update_event.is_set():
        try:
            # Update fewer stocks per iteration for better performance
            update_batch = random.sample(tickers, min(5, len(tickers)))

            for ticker in update_batch:
                change = random.uniform(-0.3, 0.3)  # Reduced volatility
                old_price = stock_data[ticker]["price"]
                new_price = round(old_price * (1 + change / 100), 2)
                stock_data[ticker]["price"] = new_price
                stock_data[ticker]["change_val"] = round(new_price - stock_data[ticker]["open"], 2)
                stock_data[ticker]["change_pct"] = round(
                    (stock_data[ticker]["change_val"] / stock_data[ticker]["open"]) * 100, 2)

                if new_price > stock_data[ticker]["high"]:
                    stock_data[ticker]["high"] = new_price
                if new_price < stock_data[ticker]["low"]:
                    stock_data[ticker]["low"] = new_price

                # Update historical data less frequently
                if random.random() < 0.3:  # 30% chance to update historical data
                    historical_data[ticker].append(new_price)
                    if len(historical_data[ticker]) > 20:
                        historical_data[ticker].pop(0)

            # Update indices less frequently
            if random.random() < 0.5:  # 50% chance to update indices
                for index in list(indices.keys())[:3]:  # Update only first 3 indices
                    change = random.uniform(-0.1, 0.1)
                    indices[index]["change"] = round(indices[index]["change"] + change, 2)
                    indices[index]["value"] = round(indices[index]["value"] * (1 + indices[index]["change"] / 100), 2)

            # Longer sleep for better performance
            time.sleep(3)  # Increased from 2 to 3 seconds

        except Exception as e:
            print(f"Error in update_stock_prices: {e}")
            time.sleep(1)

# Font registry (optimized)
try:
    with dpg.font_registry():
        default_font = dpg.add_font("C:/Windows/Fonts/arial.ttf", 16)
        bold_font = dpg.add_font("C:/Windows/Fonts/arialbd.ttf", 16)
except:
    print("Warning: Could not load custom fonts, using default")

# Themes (unchanged but optimized)
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

with dpg.theme() as positive_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_GREEN, category=dpg.mvThemeCat_Core)

with dpg.theme() as negative_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_RED, category=dpg.mvThemeCat_Core)

with dpg.theme() as neutral_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_WHITE, category=dpg.mvThemeCat_Core)

with dpg.theme() as header_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_YELLOW, category=dpg.mvThemeCat_Core)

with dpg.theme() as orange_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Core)

with dpg.theme() as chart_theme:
    with dpg.theme_component(dpg.mvLineSeries):
        dpg.add_theme_color(dpg.mvPlotCol_Line, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Plots)
    with dpg.theme_component(dpg.mvPlot):
        dpg.add_theme_color(dpg.mvPlotCol_FrameBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Plots)
        dpg.add_theme_color(dpg.mvPlotCol_PlotBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Plots)

# Main window layout
with dpg.window(label="Fincept Professional", tag="primary_window", no_collapse=True):
    # Top bar
    with dpg.group(horizontal=True):
        dpg.add_text("FINCEPT", color=BLOOMBERG_ORANGE)
        dpg.add_text("PROFESSIONAL", color=BLOOMBERG_WHITE)
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_input_text(label="", default_value="Enter Command", width=300)
        dpg.add_button(label="Search", width=80)
        dpg.add_text(" | ", color=BLOOMBERG_GRAY)
        dpg.add_text(f"{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}", tag="time_display")

    dpg.add_separator()

    # Function keys
    with dpg.group(horizontal=True):
        function_keys = ["F1:HELP", "F2:MARKETS", "F3:NEWS", "F4:PORT", "F5:MOVERS", "F6:ECON"]
        for key in function_keys:
            dpg.add_button(label=key, width=80)

    dpg.add_separator()

    # Main work area
    with dpg.group(horizontal=True):
        # Left panel (Market Monitor)
        with dpg.child_window(width=380, height=580, border=False):
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

            dpg.add_separator()

            # Recent news headlines
            dpg.add_text("LATEST NEWS", color=BLOOMBERG_YELLOW)
            for i, headline in enumerate(news_headlines[:6]):  # Reduced to 6
                dpg.add_text(f"{datetime.datetime.now().strftime('%H:%M')} - {headline}", wrap=380, tag=f"news_{i}")

        # Center panel (Market Data and Charts)
        with dpg.child_window(width=800, height=580, border=False):
            with dpg.tab_bar():
                # Market Data Tab
                with dpg.tab(label="Market Data"):
                    dpg.add_text("TOP STOCKS", color=BLOOMBERG_ORANGE)

                    # Stock ticker table (optimized)
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True, borders_innerV=True,
                                   borders_outerV=True, scrollY=True, height=200):  # Reduced height
                        dpg.add_table_column(label="Ticker")
                        dpg.add_table_column(label="Last")
                        dpg.add_table_column(label="Chg")
                        dpg.add_table_column(label="Chg%")
                        dpg.add_table_column(label="Volume")

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

                                if data['change_pct'] > 0:
                                    dpg.bind_item_theme(chg_tag, positive_theme)
                                    dpg.bind_item_theme(chg_pct_tag, positive_theme)
                                elif data['change_pct'] < 0:
                                    dpg.bind_item_theme(chg_tag, negative_theme)
                                    dpg.bind_item_theme(chg_pct_tag, negative_theme)

                    # Stock details section
                    dpg.add_text("STOCK DETAILS", color=BLOOMBERG_ORANGE)
                    with dpg.group(horizontal=True):
                        dpg.add_input_text(label="Ticker", default_value="AAPL", width=150, tag="detail_ticker")
                        dpg.add_button(label="Load", callback=lambda: None)

                    # Details for AAPL
                    with dpg.group(horizontal=True):
                        with dpg.group():
                            dpg.add_text("Apple Inc (AAPL US Equity)", color=BLOOMBERG_ORANGE)
                            with dpg.table(header_row=False):
                                dpg.add_table_column()
                                dpg.add_table_column()
                                with dpg.table_row():
                                    dpg.add_text("Last Price:")
                                    dpg.add_text(f"{stock_data['AAPL']['price']:.2f}", tag="detail_price")
                                with dpg.table_row():
                                    dpg.add_text("Change:")
                                    dpg.add_text(f"{stock_data['AAPL']['change_val']:+.2f}", tag="detail_change")
                                with dpg.table_row():
                                    dpg.add_text("Volume:")
                                    dpg.add_text(f"{stock_data['AAPL']['volume']:,}", tag="detail_volume")

                    # Simplified chart
                    with dpg.plot(height=150, width=-1, tag="detail_chart"):
                        dpg.add_plot_axis(dpg.mvXAxis, tag="detail_x_axis")
                        y_axis = dpg.add_plot_axis(dpg.mvYAxis, tag="detail_y_axis")
                        dpg.add_line_series(list(range(len(historical_data["AAPL"]))),
                                            historical_data["AAPL"],
                                            parent=y_axis,
                                            tag="detail_line")
                    dpg.bind_item_theme("detail_chart", chart_theme)

                # Simplified News Tab
                with dpg.tab(label="News"):
                    dpg.add_text("FINANCIAL NEWS", color=BLOOMBERG_ORANGE)
                    for i, headline in enumerate(news_headlines):
                        timestamp = (datetime.datetime.now() - datetime.timedelta(
                            minutes=random.randint(5, 500))).strftime("%H:%M")
                        dpg.add_text(f"{timestamp} - {headline}", color=BLOOMBERG_WHITE, wrap=750)
                        if i < len(news_headlines) - 1:
                            dpg.add_separator()

        # Right panel (Command Line)
        with dpg.child_window(width=400, height=580, border=False):
            dpg.add_text("COMMAND LINE", color=BLOOMBERG_ORANGE)
            dpg.add_separator()

            with dpg.child_window(height=200, border=True):
                dpg.add_text("> AAPL US Equity <GO>", color=BLOOMBERG_WHITE)
                dpg.add_text("  Loading AAPL US Equity...", color=BLOOMBERG_GRAY)
                dpg.add_text("> TOP <GO>", color=BLOOMBERG_WHITE)
                dpg.add_text("  Loading TOP news...", color=BLOOMBERG_GRAY)

            dpg.add_input_text(label=">", width=-1, tag="cmd_input")
            dpg.add_text("<HELP> for commands", color=BLOOMBERG_GRAY)

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

# Optimized update functions
def update_news_ticker():
    """Update scrolling news ticker with optimized performance"""
    global ticker_position
    try:
        display_text = ticker_text[ticker_position:ticker_position + ticker_display_width]
        if ticker_position + ticker_display_width >= ticker_full_length:
            remaining = ticker_display_width - (ticker_full_length - ticker_position)
            display_text += ticker_text[:remaining]

        dpg.set_value("scrolling_news", display_text)
        ticker_position += ticker_speed
        if ticker_position >= ticker_full_length:
            ticker_position = 0
    except Exception as e:
        print(f"Error updating ticker: {e}")

def update_status_bar():
    """Update status bar with reduced frequency"""
    try:
        current_time = datetime.datetime.now()
        dpg.set_value("last_update_time", current_time.strftime("%H:%M:%S"))

        # Update market status
        market_open = current_time.hour >= 9 and current_time.hour < 16
        if market_open:
            dpg.set_value("market_status", "MARKET OPEN")
        else:
            dpg.set_value("market_status", "MARKET CLOSED")

        # Update latency occasionally
        if random.random() < 0.1:  # 10% chance
            latency = random.randint(8, 25)
            dpg.set_value("latency_indicator", f"LATENCY: {latency}ms")
    except Exception as e:
        print(f"Error updating status bar: {e}")

# Optimized UI update function
update_counter = 0
def update_ui():
    """Optimized UI update with reduced operations"""
    global update_counter
    update_counter += 1

    try:
        # Update time display
        dpg.set_value("time_display", datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

        # Update ticker and status bar every iteration
        update_news_ticker()
        update_status_bar()

        # Update market data every 3rd iteration (reduced frequency)
        if update_counter % 3 == 0:
            # Update only first few indices for performance
            for i, (index, data) in enumerate(list(indices.items())[:3]):
                if dpg.does_item_exist(f"idx_{index}_val"):
                    dpg.set_value(f"idx_{index}_val", f"{data['value']:.2f}")
                    dpg.set_value(f"idx_{index}_chg", f"{data['change']:+.2f}%")

            # Update only first 5 stocks for performance
            for ticker in tickers[:5]:
                data = stock_data[ticker]
                if dpg.does_item_exist(f"{ticker}_price"):
                    dpg.set_value(f"{ticker}_price", f"{data['price']:.2f}")
                    dpg.set_value(f"{ticker}_chg", f"{data['change_val']:+.2f}")
                    dpg.set_value(f"{ticker}_chg_pct", f"{data['change_pct']:+.2f}%")

            # Update AAPL details
            if dpg.does_item_exist("detail_price"):
                dpg.set_value("detail_price", f"{stock_data['AAPL']['price']:.2f}")
                dpg.set_value("detail_change", f"{stock_data['AAPL']['change_val']:+.2f}")

        # Update charts every 5th iteration
        if update_counter % 5 == 0:
            if dpg.does_item_exist("detail_line"):
                dpg.set_value("detail_line", [list(range(len(historical_data["AAPL"]))), historical_data["AAPL"]])

    except Exception as e:
        print(f"Error in update_ui: {e}")

# Optimized UI update loop
def ui_update_loop():
    """Optimized update loop with proper timing"""
    while not update_event.is_set():
        try:
            if dpg.is_dearpygui_running():
                dpg.split_frame()  # Better performance than invoke
                update_ui()
                time.sleep(1.0)  # Increased to 1 second for better performance
            else:
                break
        except Exception as e:
            print(f"Error in ui_update_loop: {e}")
            time.sleep(1)

# Start optimized background threads
price_thread = Thread(target=update_stock_prices, daemon=True)
price_thread.start()

ui_thread = Thread(target=ui_update_loop, daemon=True)
ui_thread.start()

# Setup and start DearPyGUI with optimization
try:
    dpg.setup_dearpygui()
    dpg.show_viewport()
    dpg.set_primary_window("primary_window", True)
    dpg.start_dearpygui()
except Exception as e:
    print(f"Error starting DearPyGUI: {e}")
finally:
    update_event.set()  # Signal threads to stop
    dpg.destroy_context()