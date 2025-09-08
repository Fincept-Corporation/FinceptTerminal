# -*- coding: utf-8 -*-
"""
Consumer Behaviour Tab module for Fincept Terminal
Updated with new centralized logging system and performance improvements
"""

from fincept_terminal.utils.base_tab import BaseTab
import yfinance as yf
import pandas as pd
import json
import os
import requests
import threading
import time
from empyrical import annual_volatility, sharpe_ratio, max_drawdown, calmar_ratio
from scholarly import scholarly
from PyPDF2 import PdfReader
from io import BytesIO
import google.generativeai as genai
import dearpygui.dearpygui as dpg
from functools import lru_cache
import concurrent.futures

# Import new logging system
from fincept_terminal.utils.Logging.logger import (
    logger, debug, info, warning, error, critical,
    operation, monitor_performance
)


def get_settings_path():
    """Returns settings file path"""
    return "settings.json"


SETTINGS_FILE = get_settings_path()


class ConsumerBehaviorTab(BaseTab):
    """Consumer Behavior Analysis using AI"""

    def __init__(self, app):
        super().__init__(app)
        self.analysis_running = False
        self.max_papers = 3
        self.request_timeout = 10
        self.max_content_length = 2000

        info("Consumer Behavior Tab initialized")

    def get_label(self):
        return "Consumer Behavior"

    @monitor_performance
    def create_content(self):
        """Create consumer behavior analysis interface"""
        with dpg.child_window(
                tag="consumer_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            self.add_section_header("Consumer Behavior Analysis")
            dpg.add_text("AI-Powered Consumer Insights for Cities", color=[200, 200, 255])
            dpg.add_spacer(height=20)

            # Input section
            dpg.add_text("City Analysis", color=[100, 255, 100])
            dpg.add_input_text(
                tag="city_input",
                hint="Enter city to analyze (e.g., Mumbai, Delhi)",
                width=300
            )
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_button(
                    label="Analyze City",
                    callback=self.analyze_city_callback,
                    width=150,
                    height=35
                )
                dpg.add_text("Ready to analyze", tag="analysis_status", color=[200, 200, 200])

            dpg.add_spacer(height=20)

            # Results section
            dpg.add_text("Analysis Report", color=[100, 255, 100])
            dpg.add_separator()

            with dpg.child_window(
                    tag="summary_display",
                    width=-1,
                    height=400,
                    border=True
            ):
                dpg.add_text("Summary Report will appear here...", color=[200, 200, 200])

        debug("Consumer behavior content created")

    def analyze_city_callback(self):
        """Start city analysis"""
        city = dpg.get_value("city_input").strip()
        if not city:
            warning("City analysis attempted without city name", context={'city': city})
            self.show_message("Please enter a city name", "error")
            return

        if self.analysis_running:
            warning("City analysis attempted while already running", context={'city': city})
            self.show_message("Analysis already running...", "warning")
            return

        info("Starting city analysis", context={'city': city})
        threading.Thread(target=self.process_analysis, args=(city,), daemon=True).start()

    @monitor_performance
    def process_analysis(self, city):
        """Process consumer behavior analysis with improved error handling"""
        try:
            with operation("city_analysis", city=city):
                self.analysis_running = True
                dpg.set_value("analysis_status", "Starting analysis...")

                # Fetch research papers
                dpg.set_value("analysis_status", f"Fetching papers for {city}...")
                papers = self.fetch_research_papers(city)

                if not papers:
                    error("No research papers found", context={'city': city})
                    self.show_message("No research papers found", "error")
                    return

                # Consolidate content with concurrent processing
                dpg.set_value("analysis_status", "Processing content...")
                content = self.consolidate_content(papers[:self.max_papers])

                if not content.strip():
                    warning("No accessible content found", context={'city': city, 'papers_count': len(papers)})
                    self.show_message("No accessible content found", "error")
                    return

                # Generate report
                dpg.set_value("analysis_status", "Generating AI report...")
                report = self.generate_summary_report(content)

                if report:
                    self.display_report(report)
                    dpg.set_value("analysis_status", "Analysis complete!")
                    info("Analysis completed successfully", context={'city': city})
                    self.show_message("Analysis completed successfully!", "success")
                else:
                    error("Error generating report", context={'city': city})
                    self.show_message("Error generating report", "error")

        except Exception as e:
            error("Analysis failed with exception", context={'city': city}, exc_info=True)
            self.show_message(f"Analysis error: {e}", "error")
            dpg.set_value("analysis_status", "Analysis failed")
        finally:
            self.analysis_running = False

    @monitor_performance
    def fetch_research_papers(self, city, max_papers=5):
        """Fetch research papers for city with timeout handling"""
        try:
            with operation("fetch_papers", city=city, max_papers=max_papers):
                query = f"consumer behavior in {city}"
                search_results = scholarly.search_pubs(query)
                papers = []

                for result in search_results:
                    title = result.get("title", "Unknown")
                    url = result.get("eprint_url", result.get("pub_url", ""))
                    if title and url:
                        papers.append({"title": title, "url": url})
                    if len(papers) >= max_papers:
                        break

                info("Papers fetched successfully", context={'city': city, 'papers_count': len(papers)})
                return papers
        except Exception as e:
            error("Error fetching papers", context={'city': city}, exc_info=True)
            return []

    @monitor_performance
    def consolidate_content(self, papers):
        """Consolidate paper content with concurrent processing"""
        try:
            with operation("consolidate_content", papers_count=len(papers)):
                combined_content = ""

                # Use ThreadPoolExecutor for concurrent paper fetching
                with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
                    future_to_paper = {
                        executor.submit(self.fetch_paper_content, paper["url"]): paper
                        for paper in papers
                    }

                    for future in concurrent.futures.as_completed(future_to_paper, timeout=30):
                        paper = future_to_paper[future]
                        try:
                            content = future.result(timeout=self.request_timeout)
                            if content and "Error" not in content:
                                combined_content += f"Title: {paper['title']}\n{content[:1000]}\n\n"
                        except Exception as e:
                            warning("Failed to fetch paper content",
                                    context={'paper_title': paper['title'], 'error': str(e)})

                debug("Content consolidation completed",
                      context={'total_length': len(combined_content)})
                return combined_content
        except Exception as e:
            error("Error consolidating content", exc_info=True)
            return ""

    def fetch_paper_content(self, url):
        """Fetch content from paper URL with improved error handling"""
        try:
            with operation("fetch_paper_content", url=url[:50]):
                response = requests.get(url, timeout=self.request_timeout)
                response.raise_for_status()

                if "application/pdf" in response.headers.get("Content-Type", ""):
                    pdf_file = BytesIO(response.content)
                    reader = PdfReader(pdf_file)
                    text = ""
                    for page in reader.pages[:3]:  # First 3 pages only
                        text += page.extract_text()
                    return text[:self.max_content_length]
                return response.text[:self.max_content_length]
        except requests.RequestException as e:
            warning("HTTP request failed", context={'url': url[:50], 'error': str(e)})
            return f"Request Error: {e}"
        except Exception as e:
            warning("Paper content fetch failed", context={'url': url[:50], 'error': str(e)})
            return f"Error: {e}"

    @monitor_performance
    def generate_summary_report(self, content):
        """Generate AI summary using Gemini with better error handling"""
        try:
            with operation("generate_report", content_length=len(content)):
                settings = self.load_settings()
                api_key = settings.get("data_sources", {}).get("genai_model", {}).get("apikey")

                if not api_key:
                    warning("API key not found in settings")
                    return "API key not found in settings"

                genai.configure(api_key=api_key)
                model = genai.GenerativeModel("gemini-1.5-flash")

                prompt = (
                    "Create a brief consumer behavior analysis report based on this research content. "
                    "Focus on key insights, trends, and actionable findings:\n\n"
                    f"{content}"
                )

                response = model.generate_content(prompt)
                info("AI report generated successfully")
                return response.text
        except Exception as e:
            error("Error generating AI report", exc_info=True)
            return f"Error generating report: {e}"

    def display_report(self, report):
        """Display the generated report with improved formatting"""
        try:
            with operation("display_report"):
                # Clear existing content
                children = dpg.get_item_children("summary_display", slot=1)
                if children:
                    for child in children:
                        dpg.delete_item(child)

                # Add report content with better formatting
                lines = report.split('\n')
                for line in lines:
                    if line.strip():
                        if any(keyword in line for keyword in ['#', 'Summary', 'Report', 'Analysis', 'Key Insights']):
                            dpg.add_text(line, color=[255, 200, 100], parent="summary_display", wrap=-1)
                        else:
                            dpg.add_text(line, color=[255, 255, 255], parent="summary_display", wrap=-1)
                    else:
                        dpg.add_spacer(height=5, parent="summary_display")

                debug("Report displayed successfully")
        except Exception as e:
            error("Error displaying report", exc_info=True)

    @lru_cache(maxsize=1)
    def load_settings(self):
        """Load settings from file with caching"""
        try:
            if os.path.exists(SETTINGS_FILE):
                with open(SETTINGS_FILE, "r", encoding='utf-8') as f:
                    settings = json.load(f)
                    debug("Settings loaded successfully")
                    return settings
            debug("Settings file not found, using defaults")
            return {}
        except Exception as e:
            error("Error loading settings", exc_info=True)
            return {}

    def show_message(self, message, message_type="info"):
        """Show message to user"""
        if hasattr(self.app, 'show_message'):
            self.app.show_message(message, message_type)
        else:
            info(f"{message_type.upper()}: {message}", context={'message': message})

    def cleanup(self):
        """Cleanup resources"""
        info("Cleaning up Consumer Behavior Tab")
        self.analysis_running = False


class ComparisonAnalysisTab(BaseTab):
    """Streamlined Comparison Analysis with improved performance"""

    def __init__(self, app):
        super().__init__(app)
        self.current_view = "portfolio"
        self.analysis_running = False
        self.cache = {}
        info("Comparison Analysis Tab initialized")

    def get_label(self):
        return "Comparison"

    @monitor_performance
    def create_content(self):
        """Create comparison analysis interface"""
        with dpg.child_window(
                tag="comparison_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            self.add_section_header("Comparison Analysis")

            # Navigation
            self.create_navigation()
            dpg.add_spacer(height=15)

            # Content areas
            self.create_portfolio_content()
            self.create_index_content()
            self.create_stock_content()

        debug("Comparison analysis content created")

    def create_navigation(self):
        """Create navigation tabs"""
        with dpg.group(horizontal=True):
            dpg.add_button(
                label="Portfolio",
                callback=lambda: self.switch_view("portfolio"),
                tag="portfolio_nav_btn",
                width=120
            )
            dpg.add_button(
                label="Index",
                callback=lambda: self.switch_view("index"),
                tag="index_nav_btn",
                width=120
            )
            dpg.add_button(
                label="Stocks",
                callback=lambda: self.switch_view("stock"),
                tag="stock_nav_btn",
                width=120
            )

        dpg.add_separator()
        self.update_nav_buttons()

    def switch_view(self, view):
        """Switch between analysis types"""
        debug("Switching view", context={'from': self.current_view, 'to': view})
        self.current_view = view

        # Hide all content
        for v in ["portfolio", "index", "stock"]:
            if dpg.does_item_exist(f"comparison_{v}_content"):
                dpg.hide_item(f"comparison_{v}_content")

        # Show selected content
        if dpg.does_item_exist(f"comparison_{view}_content"):
            dpg.show_item(f"comparison_{view}_content")

        self.update_nav_buttons()

    def update_nav_buttons(self):
        """Update navigation button states"""
        buttons = {"portfolio": "portfolio_nav_btn", "index": "index_nav_btn", "stock": "stock_nav_btn"}

        for view, btn_tag in buttons.items():
            if dpg.does_item_exist(btn_tag):
                if view == self.current_view:
                    dpg.bind_item_theme(btn_tag, self.get_active_theme())
                else:
                    dpg.bind_item_theme(btn_tag, 0)

    def get_active_theme(self):
        """Get active button theme"""
        if not dpg.does_item_exist("active_nav_theme"):
            with dpg.theme(tag="active_nav_theme"):
                with dpg.theme_component(dpg.mvButton):
                    dpg.add_theme_color(dpg.mvThemeCol_Button, [70, 130, 180, 255])
        return "active_nav_theme"

    def create_portfolio_content(self):
        """Create portfolio comparison content"""
        with dpg.group(tag="comparison_portfolio_content"):
            dpg.add_text("Portfolio Comparison", color=[100, 255, 100])

            dpg.add_input_text(
                tag="portfolio_input",
                hint="Enter portfolio names (comma-separated)",
                width=400
            )
            dpg.add_button(
                label="Compare Portfolios",
                callback=self.compare_portfolios_callback,
                width=150
            )
            dpg.add_spacer(height=15)

            # Results table
            with dpg.table(
                    tag="portfolio_results_table",
                    resizable=True,
                    borders_innerH=True,
                    borders_outerH=True,
                    borders_innerV=True,
                    borders_outerV=True,
                    height=300
            ):
                dpg.add_table_column(label="Portfolio")
                dpg.add_table_column(label="Investment")
                dpg.add_table_column(label="Current Value")
                dpg.add_table_column(label="Return %")
                dpg.add_table_column(label="Volatility")
                dpg.add_table_column(label="Sharpe Ratio")

    def create_index_content(self):
        """Create index comparison content"""
        with dpg.group(tag="comparison_index_content", show=False):
            dpg.add_text("Index Comparison", color=[100, 255, 100])

            dpg.add_input_text(
                tag="index_input",
                hint="Enter index symbols (e.g., ^NSEI, ^BSESN)",
                width=400
            )
            dpg.add_button(
                label="Compare Indices",
                callback=self.compare_indices_callback,
                width=150
            )
            dpg.add_spacer(height=15)

            # Results table
            with dpg.table(
                    tag="index_results_table",
                    resizable=True,
                    borders_innerH=True,
                    borders_outerH=True,
                    borders_innerV=True,
                    borders_outerV=True,
                    height=300
            ):
                dpg.add_table_column(label="Index")
                dpg.add_table_column(label="1Y Return %")
                dpg.add_table_column(label="Volatility %")
                dpg.add_table_column(label="Max Drawdown %")

    def create_stock_content(self):
        """Create stock comparison content"""
        with dpg.group(tag="comparison_stock_content", show=False):
            dpg.add_text("Stock Comparison", color=[100, 255, 100])

            dpg.add_input_text(
                tag="stock_input",
                hint="Enter stock symbols (e.g., RELIANCE.NS, TCS.NS)",
                width=400
            )
            dpg.add_button(
                label="Compare Stocks",
                callback=self.compare_stocks_callback,
                width=150
            )
            dpg.add_spacer(height=15)

            # Results table
            with dpg.table(
                    tag="stock_results_table",
                    resizable=True,
                    borders_innerH=True,
                    borders_outerH=True,
                    borders_innerV=True,
                    borders_outerV=True,
                    height=300
            ):
                dpg.add_table_column(label="Stock")
                dpg.add_table_column(label="Price")
                dpg.add_table_column(label="P/E Ratio")
                dpg.add_table_column(label="Market Cap")
                dpg.add_table_column(label="1Y Return %")
                dpg.add_table_column(label="Beta")

    # Callback methods with improved error handling
    def compare_portfolios_callback(self):
        """Compare portfolios"""
        portfolios = dpg.get_value("portfolio_input").strip()
        if not portfolios:
            warning("Portfolio comparison attempted without portfolio names")
            self.show_message("Enter portfolio names", "error")
            return

        info("Starting portfolio comparison", context={'portfolios': portfolios})
        threading.Thread(target=self.analyze_portfolios, args=(portfolios,), daemon=True).start()

    def compare_indices_callback(self):
        """Compare indices"""
        indices = dpg.get_value("index_input").strip()
        if not indices:
            warning("Index comparison attempted without index symbols")
            self.show_message("Enter index symbols", "error")
            return

        info("Starting index comparison", context={'indices': indices})
        threading.Thread(target=self.analyze_indices, args=(indices,), daemon=True).start()

    def compare_stocks_callback(self):
        """Compare stocks"""
        stocks = dpg.get_value("stock_input").strip()
        if not stocks:
            warning("Stock comparison attempted without stock symbols")
            self.show_message("Enter stock symbols", "error")
            return

        info("Starting stock comparison", context={'stocks': stocks})
        threading.Thread(target=self.analyze_stocks, args=(stocks,), daemon=True).start()

    @monitor_performance
    def analyze_portfolios(self, portfolio_names):
        """Analyze portfolio performance with improved error handling"""
        try:
            with operation("analyze_portfolios", portfolio_names=portfolio_names):
                self.clear_table("portfolio_results_table")
                names = [name.strip() for name in portfolio_names.split(",")]
                portfolios = self.load_portfolios()

                for name in names:
                    try:
                        if name.lower() not in portfolios:
                            warning("Portfolio not found", context={'portfolio': name})
                            self.add_portfolio_error_row(name, "Not Found")
                            continue

                        portfolio = portfolios[name.lower()]
                        investment, current_value, returns = self.calculate_portfolio_metrics(portfolio)

                        with dpg.table_row(parent="portfolio_results_table"):
                            dpg.add_text(name)
                            dpg.add_text(f"₹{investment:.2f}")
                            dpg.add_text(f"₹{current_value:.2f}")
                            dpg.add_text(f"{((current_value - investment) / investment * 100):.2f}%")
                            dpg.add_text(f"{returns.std() * 252 ** 0.5:.2%}" if not returns.empty else "N/A")
                            dpg.add_text(f"{sharpe_ratio(returns):.2f}" if not returns.empty else "N/A")

                    except Exception as e:
                        error("Error analyzing portfolio", context={'portfolio': name}, exc_info=True)
                        self.add_portfolio_error_row(name, "Error")

                info("Portfolio comparison completed")
                self.show_message("Portfolio comparison completed", "success")
        except Exception as e:
            error("Portfolio analysis failed", exc_info=True)
            self.show_message(f"Error: {e}", "error")

    def add_portfolio_error_row(self, name, status):
        """Add error row to portfolio table"""
        with dpg.table_row(parent="portfolio_results_table"):
            dpg.add_text(name)
            dpg.add_text(status)
            dpg.add_text(status)
            dpg.add_text(status)
            dpg.add_text(status)
            dpg.add_text(status)

    @monitor_performance
    def analyze_indices(self, index_symbols):
        """Analyze index performance with concurrent processing"""
        try:
            with operation("analyze_indices", index_symbols=index_symbols):
                self.clear_table("index_results_table")
                symbols = [symbol.strip().upper() for symbol in index_symbols.split(",")]

                # Use ThreadPoolExecutor for concurrent index analysis
                with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
                    future_to_symbol = {
                        executor.submit(self.get_index_data, symbol): symbol
                        for symbol in symbols
                    }

                    for future in concurrent.futures.as_completed(future_to_symbol, timeout=30):
                        symbol = future_to_symbol[future]
                        try:
                            data = future.result()
                            self.add_index_row(symbol, data)
                        except Exception as e:
                            error("Error processing index", context={'symbol': symbol}, exc_info=True)
                            self.add_index_error_row(symbol)

                info("Index comparison completed")
                self.show_message("Index comparison completed", "success")
        except Exception as e:
            error("Index analysis failed", exc_info=True)
            self.show_message(f"Error: {e}", "error")

    def get_index_data(self, symbol):
        """Get index data with caching"""
        cache_key = f"index_{symbol}_{int(time.time() // 3600)}"  # Cache for 1 hour

        if cache_key in self.cache:
            debug("Using cached index data", context={'symbol': symbol})
            return self.cache[cache_key]

        try:
            ticker = yf.Ticker(symbol)
            data = ticker.history(period="1y")

            if data.empty:
                return None

            returns = data["Close"].pct_change().dropna()
            total_return = ((data["Close"][-1] - data["Close"][0]) / data["Close"][0]) * 100
            volatility = annual_volatility(returns) * 100
            max_dd = max_drawdown(returns) * 100

            result = {
                'total_return': total_return,
                'volatility': volatility,
                'max_drawdown': max_dd
            }

            self.cache[cache_key] = result
            return result

        except Exception as e:
            warning("Failed to get index data", context={'symbol': symbol, 'error': str(e)})
            return None

    def add_index_row(self, symbol, data):
        """Add index data row"""
        if data:
            with dpg.table_row(parent="index_results_table"):
                dpg.add_text(symbol)
                dpg.add_text(f"{data['total_return']:.2f}%")
                dpg.add_text(f"{data['volatility']:.2f}%")
                dpg.add_text(f"{data['max_drawdown']:.2f}%")
        else:
            self.add_index_error_row(symbol)

    def add_index_error_row(self, symbol):
        """Add error row to index table"""
        with dpg.table_row(parent="index_results_table"):
            dpg.add_text(symbol)
            dpg.add_text("No Data")
            dpg.add_text("No Data")
            dpg.add_text("No Data")

    @monitor_performance
    def analyze_stocks(self, stock_symbols):
        """Analyze stock performance with concurrent processing"""
        try:
            with operation("analyze_stocks", stock_symbols=stock_symbols):
                self.clear_table("stock_results_table")
                symbols = [symbol.strip().upper() for symbol in stock_symbols.split(",")]

                # Use ThreadPoolExecutor for concurrent stock analysis
                with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
                    future_to_symbol = {
                        executor.submit(self.get_stock_data, symbol): symbol
                        for symbol in symbols
                    }

                    for future in concurrent.futures.as_completed(future_to_symbol, timeout=30):
                        symbol = future_to_symbol[future]
                        try:
                            data = future.result()
                            self.add_stock_row(symbol, data)
                        except Exception as e:
                            error("Error processing stock", context={'symbol': symbol}, exc_info=True)
                            self.add_stock_error_row(symbol)

                info("Stock comparison completed")
                self.show_message("Stock comparison completed", "success")
        except Exception as e:
            error("Stock analysis failed", exc_info=True)
            self.show_message(f"Error: {e}", "error")

    def get_stock_data(self, symbol):
        """Get stock data with caching"""
        cache_key = f"stock_{symbol}_{int(time.time() // 1800)}"  # Cache for 30 minutes

        if cache_key in self.cache:
            debug("Using cached stock data", context={'symbol': symbol})
            return self.cache[cache_key]

        try:
            ticker = yf.Ticker(symbol)
            info_data = ticker.info
            hist_data = ticker.history(period="1y")

            if hist_data.empty:
                return None

            current_price = info_data.get("currentPrice", hist_data["Close"][-1])
            pe_ratio = info_data.get("trailingPE")
            market_cap = info_data.get("marketCap")
            beta = info_data.get("beta")
            year_return = ((hist_data["Close"][-1] - hist_data["Close"][0]) / hist_data["Close"][0]) * 100

            result = {
                'current_price': current_price,
                'pe_ratio': pe_ratio,
                'market_cap': market_cap,
                'year_return': year_return,
                'beta': beta
            }

            self.cache[cache_key] = result
            return result

        except Exception as e:
            warning("Failed to get stock data", context={'symbol': symbol, 'error': str(e)})
            return None

    def add_stock_row(self, symbol, data):
        """Add stock data row"""
        if data:
            with dpg.table_row(parent="stock_results_table"):
                dpg.add_text(symbol)
                dpg.add_text(f"₹{data['current_price']:.2f}")
                dpg.add_text(f"{data['pe_ratio']:.2f}" if isinstance(data['pe_ratio'], (int, float)) else "N/A")
                dpg.add_text(
                    f"₹{data['market_cap'] / 1e9:.2f}B" if isinstance(data['market_cap'], (int, float)) else "N/A")
                dpg.add_text(f"{data['year_return']:.2f}%")
                dpg.add_text(f"{data['beta']:.2f}" if isinstance(data['beta'], (int, float)) else "N/A")
        else:
            self.add_stock_error_row(symbol)

    def add_stock_error_row(self, symbol):
        """Add error row to stock table"""
        with dpg.table_row(parent="stock_results_table"):
            dpg.add_text(symbol)
            dpg.add_text("Error")
            dpg.add_text("Error")
            dpg.add_text("Error")
            dpg.add_text("Error")
            dpg.add_text("Error")

    @monitor_performance
    def calculate_portfolio_metrics(self, portfolio):
        """Calculate portfolio metrics with improved performance"""
        investment = 0
        current_value = 0
        returns = pd.Series(dtype=float)

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get("quantity", 0)
                avg_price = data.get("avg_price", 0)
                investment += quantity * avg_price

                # Use cache for current prices
                cache_key = f"price_{symbol}_{int(time.time() // 300)}"  # Cache for 5 minutes

                if cache_key in self.cache:
                    current_price = self.cache[cache_key]
                else:
                    try:
                        ticker = yf.Ticker(symbol)
                        current_price = ticker.history(period="1d")["Close"].iloc[-1]
                        self.cache[cache_key] = current_price
                    except Exception as e:
                        warning("Failed to get current price", context={'symbol': symbol, 'error': str(e)})
                        current_price = avg_price

                current_value += quantity * current_price

        return investment, current_value, returns

    @lru_cache(maxsize=1)
    def load_portfolios(self):
        """Load portfolios from settings with caching"""
        try:
            if os.path.exists(SETTINGS_FILE):
                with open(SETTINGS_FILE, "r", encoding='utf-8') as f:
                    settings = json.load(f)
                    portfolios = settings.get("portfolios", {})
                    # Convert keys to lowercase for case-insensitive matching
                    debug("Portfolios loaded successfully", context={'count': len(portfolios)})
                    return {k.lower(): v for k, v in portfolios.items()}
            debug("No portfolios file found")
            return {}
        except Exception as e:
            error("Error loading portfolios", exc_info=True)
            return {}

    def clear_table(self, table_tag):
        """Clear table rows efficiently"""
        try:
            if dpg.does_item_exist(table_tag):
                children = dpg.get_item_children(table_tag, slot=1)
                if children:
                    for child in children:
                        dpg.delete_item(child)
                debug("Table cleared", context={'table': table_tag})
        except Exception as e:
            warning("Error clearing table", context={'table': table_tag, 'error': str(e)})

    def show_message(self, message, message_type="info"):
        """Show message to user with logging"""
        try:
            if hasattr(self.app, 'show_message'):
                self.app.show_message(message, message_type)
            else:
                # Log message with appropriate level
                if message_type == "error":
                    error(f"UI Message: {message}", context={'type': message_type})
                elif message_type == "warning":
                    warning(f"UI Message: {message}", context={'type': message_type})
                else:
                    info(f"UI Message: {message}", context={'type': message_type})
        except Exception as e:
            error("Error showing message", context={'message': message, 'type': message_type}, exc_info=True)

    def cleanup(self):
        """Cleanup resources and clear cache"""
        info("Cleaning up Comparison Analysis Tab")
        self.analysis_running = False
        self.cache.clear()
        debug("Cache cleared")

    def get_cache_stats(self):
        """Get cache statistics for debugging"""
        return {
            'cache_size': len(self.cache),
            'cache_keys': list(self.cache.keys())[:10]  # First 10 keys only
        }