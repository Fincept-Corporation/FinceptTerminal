# -*- coding: utf-8 -*-
"""
Consumer Behaviour Tab module for Fincept Terminal
Updated to use centralized logging system
"""

from fincept_terminal.Utils.base_tab import BaseTab
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
from fincept_terminal.Utils.Logging.logger import logger, log_operation

def get_settings_path():
    """Returns settings file path"""
    return "settings.json"


SETTINGS_FILE = get_settings_path()


class ConsumerBehaviorTab(BaseTab):
    """Consumer Behavior Analysis using AI"""

    def __init__(self, app):
        super().__init__(app)
        self.analysis_running = False

    def get_label(self):
        return "🧠 Consumer Behavior"

    def create_content(self):
        """Create consumer behavior analysis interface"""
        with dpg.child_window(
                tag="consumer_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            self.add_section_header("🧠 Consumer Behavior Analysis")
            dpg.add_text("AI-Powered Consumer Insights for Cities", color=[200, 200, 255])
            dpg.add_spacer(height=20)

            # Input section
            dpg.add_text(" City Analysis", color=[100, 255, 100])
            dpg.add_input_text(
                tag="city_input",
                hint="Enter city to analyze (e.g., Mumbai, Delhi)",
                width=300
            )
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                dpg.add_button(
                    label=" Analyze City",
                    callback=self.analyze_city_callback,
                    width=150,
                    height=35
                )
                dpg.add_text("Ready to analyze", tag="analysis_status", color=[200, 200, 200])

            dpg.add_spacer(height=20)

            # Results section
            dpg.add_text(" Analysis Report", color=[100, 255, 100])
            dpg.add_separator()

            with dpg.child_window(
                    tag="summary_display",
                    width=-1,
                    height=400,
                    border=True
            ):
                dpg.add_text("Summary Report will appear here...", color=[200, 200, 200])

    def analyze_city_callback(self):
        """Start city analysis"""
        city = dpg.get_value("city_input").strip()
        if not city:
            self.show_message("Please enter a city name", "error")
            return

        if self.analysis_running:
            self.show_message("Analysis already running...", "warning")
            return

        threading.Thread(target=self.process_analysis, args=(city,), daemon=True).start()

    def process_analysis(self, city):
        """Process consumer behavior analysis"""
        try:
            self.analysis_running = True
            dpg.set_value("analysis_status", " Starting analysis...")

            # Fetch research papers
            dpg.set_value("analysis_status", f" Fetching papers for {city}...")
            papers = self.fetch_research_papers(city)

            if not papers:
                self.show_message("No research papers found", "error")
                return

            # Consolidate content
            dpg.set_value("analysis_status", " Processing content...")
            content = self.consolidate_content(papers[:3])  # Limit to 3 papers

            if not content.strip():
                self.show_message("No accessible content found", "error")
                return

            # Generate report
            dpg.set_value("analysis_status", "🤖 Generating AI report...")
            report = self.generate_summary_report(content)

            if report:
                self.display_report(report)
                dpg.set_value("analysis_status", " Analysis complete!")
                self.show_message("Analysis completed successfully!", "success")
            else:
                self.show_message("Error generating report", "error")

        except Exception as e:
            self.show_message(f"Analysis error: {e}", "error")
            dpg.set_value("analysis_status", " Analysis failed")
        finally:
            self.analysis_running = False

    def fetch_research_papers(self, city, max_papers=5):
        """Fetch research papers for city"""
        try:
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

            return papers
        except Exception as e:
            logger.error(f"Error fetching papers: {e}", module="Consumer_Behaviour_Tab", context={'e': e})
            return []

    def consolidate_content(self, papers):
        """Consolidate paper content"""
        combined_content = ""
        for paper in papers:
            content = self.fetch_paper_content(paper["url"])
            if content and "Error" not in content:
                combined_content += f"Title: {paper['title']}\n{content[:1000]}\n\n"
        return combined_content

    def fetch_paper_content(self, url):
        """Fetch content from paper URL"""
        try:
            response = requests.get(url, timeout=10)
            if "application/pdf" in response.headers.get("Content-Type", ""):
                pdf_file = BytesIO(response.content)
                reader = PdfReader(pdf_file)
                text = ""
                for page in reader.pages[:3]:  # First 3 pages only
                    text += page.extract_text()
                return text[:2000]  # Limit text length
            return response.text[:2000]
        except Exception as e:
            return f"Error: {e}"

    def generate_summary_report(self, content):
        """Generate AI summary using Gemini"""
        try:
            settings = self.load_settings()
            api_key = settings.get("data_sources", {}).get("genai_model", {}).get("apikey")

            if not api_key:
                return "API key not found in settings"

            genai.configure(api_key=api_key)
            model = genai.GenerativeModel("gemini-1.5-flash")

            prompt = f"Create a brief consumer behavior analysis report based on this research content. Focus on key insights, trends, and actionable findings:\n\n{content}"
            response = model.generate_content(prompt)
            return response.text
        except Exception as e:
            return f"Error generating report: {e}"

    def display_report(self, report):
        """Display the generated report"""
        # Clear existing content
        children = dpg.get_item_children("summary_display", slot=1)
        if children:
            for child in children:
                dpg.delete_item(child)

        # Add report content
        lines = report.split('\n')
        for line in lines:
            if line.strip():
                if line.startswith('#') or 'Summary' in line or 'Report' in line:
                    dpg.add_text(line, color=[255, 200, 100], parent="summary_display", wrap=-1)
                else:
                    dpg.add_text(line, color=[255, 255, 255], parent="summary_display", wrap=-1)
            else:
                dpg.add_spacer(height=5, parent="summary_display")

    def load_settings(self):
        """Load settings from file"""
        try:
            if os.path.exists(SETTINGS_FILE):
                with open(SETTINGS_FILE, "r") as f:
                    return json.load(f)
            return {}
        except Exception:
            return {}

    def show_message(self, message, message_type="info"):
        """Show message to user"""
        if hasattr(self.app, 'show_message'):
            self.app.show_message(message, message_type)
        else:
            logger.info(f"{message_type.upper()}: {message}", module="Consumer_Behaviour_Tab", context={'message': message})

    def cleanup(self):
        """Cleanup resources"""
        self.analysis_running = False


class ComparisonAnalysisTab(BaseTab):
    """Streamlined Comparison Analysis"""

    def __init__(self, app):
        super().__init__(app)
        self.current_view = "portfolio"
        self.analysis_running = False

    def get_label(self):
        return " Comparison"

    def create_content(self):
        """Create comparison analysis interface"""
        with dpg.child_window(
                tag="comparison_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            self.add_section_header(" Comparison Analysis")

            # Navigation
            self.create_navigation()
            dpg.add_spacer(height=15)

            # Content areas
            self.create_portfolio_content()
            self.create_index_content()
            self.create_stock_content()

    def create_navigation(self):
        """Create navigation tabs"""
        with dpg.group(horizontal=True):
            dpg.add_button(
                label=" Portfolio",
                callback=lambda: self.switch_view("portfolio"),
                tag="portfolio_nav_btn",
                width=120
            )
            dpg.add_button(
                label=" Index",
                callback=lambda: self.switch_view("index"),
                tag="index_nav_btn",
                width=120
            )
            dpg.add_button(
                label=" Stocks",
                callback=lambda: self.switch_view("stock"),
                tag="stock_nav_btn",
                width=120
            )

        dpg.add_separator()
        self.update_nav_buttons()

    def switch_view(self, view):
        """Switch between analysis types"""
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
            dpg.add_text(" Portfolio Comparison", color=[100, 255, 100])

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
            dpg.add_text(" Index Comparison", color=[100, 255, 100])

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
            dpg.add_text(" Stock Comparison", color=[100, 255, 100])

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

    # Callback methods
    def compare_portfolios_callback(self):
        """Compare portfolios"""
        portfolios = dpg.get_value("portfolio_input").strip()
        if not portfolios:
            self.show_message("Enter portfolio names", "error")
            return

        threading.Thread(target=self.analyze_portfolios, args=(portfolios,), daemon=True).start()

    def compare_indices_callback(self):
        """Compare indices"""
        indices = dpg.get_value("index_input").strip()
        if not indices:
            self.show_message("Enter index symbols", "error")
            return

        threading.Thread(target=self.analyze_indices, args=(indices,), daemon=True).start()

    def compare_stocks_callback(self):
        """Compare stocks"""
        stocks = dpg.get_value("stock_input").strip()
        if not stocks:
            self.show_message("Enter stock symbols", "error")
            return

        threading.Thread(target=self.analyze_stocks, args=(stocks,), daemon=True).start()

    def analyze_portfolios(self, portfolio_names):
        """Analyze portfolio performance"""
        try:
            self.clear_table("portfolio_results_table")
            names = [name.strip() for name in portfolio_names.split(",")]
            portfolios = self.load_portfolios()

            for name in names:
                if name.lower() not in portfolios:
                    with dpg.table_row(parent="portfolio_results_table"):
                        dpg.add_text(name)
                        dpg.add_text("Not Found")
                        dpg.add_text("Not Found")
                        dpg.add_text("Not Found")
                        dpg.add_text("Not Found")
                        dpg.add_text("Not Found")
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

            self.show_message("Portfolio comparison completed", "success")
        except Exception as e:
            self.show_message(f"Error: {e}", "error")

    def analyze_indices(self, index_symbols):
        """Analyze index performance"""
        try:
            self.clear_table("index_results_table")
            symbols = [symbol.strip().upper() for symbol in index_symbols.split(",")]

            for symbol in symbols:
                try:
                    ticker = yf.Ticker(symbol)
                    data = ticker.history(period="1y")

                    if data.empty:
                        with dpg.table_row(parent="index_results_table"):
                            dpg.add_text(symbol)
                            dpg.add_text("No Data")
                            dpg.add_text("No Data")
                            dpg.add_text("No Data")
                        continue

                    returns = data["Close"].pct_change().dropna()
                    total_return = ((data["Close"][-1] - data["Close"][0]) / data["Close"][0]) * 100
                    volatility = annual_volatility(returns) * 100
                    max_dd = max_drawdown(returns) * 100

                    with dpg.table_row(parent="index_results_table"):
                        dpg.add_text(symbol)
                        dpg.add_text(f"{total_return:.2f}%")
                        dpg.add_text(f"{volatility:.2f}%")
                        dpg.add_text(f"{max_dd:.2f}%")

                except Exception as e:
                    with dpg.table_row(parent="index_results_table"):
                        dpg.add_text(symbol)
                        dpg.add_text("Error")
                        dpg.add_text("Error")
                        dpg.add_text("Error")

            self.show_message("Index comparison completed", "success")
        except Exception as e:
            self.show_message(f"Error: {e}", "error")

    def analyze_stocks(self, stock_symbols):
        """Analyze stock performance"""
        try:
            self.clear_table("stock_results_table")
            symbols = [symbol.strip().upper() for symbol in stock_symbols.split(",")]

            for symbol in symbols:
                try:
                    ticker = yf.Ticker(symbol)
                    info = ticker.info
                    data = ticker.history(period="1y")

                    if data.empty:
                        with dpg.table_row(parent="stock_results_table"):
                            dpg.add_text(symbol)
                            dpg.add_text("No Data")
                            dpg.add_text("No Data")
                            dpg.add_text("No Data")
                            dpg.add_text("No Data")
                            dpg.add_text("No Data")
                        continue

                    current_price = info.get("currentPrice", data["Close"][-1])
                    pe_ratio = info.get("trailingPE", "N/A")
                    market_cap = info.get("marketCap", "N/A")
                    beta = info.get("beta", "N/A")
                    year_return = ((data["Close"][-1] - data["Close"][0]) / data["Close"][0]) * 100

                    with dpg.table_row(parent="stock_results_table"):
                        dpg.add_text(symbol)
                        dpg.add_text(f"₹{current_price:.2f}")
                        dpg.add_text(f"{pe_ratio:.2f}" if isinstance(pe_ratio, (int, float)) else "N/A")
                        dpg.add_text(f"₹{market_cap / 1e9:.2f}B" if isinstance(market_cap, (int, float)) else "N/A")
                        dpg.add_text(f"{year_return:.2f}%")
                        dpg.add_text(f"{beta:.2f}" if isinstance(beta, (int, float)) else "N/A")

                except Exception as e:
                    with dpg.table_row(parent="stock_results_table"):
                        dpg.add_text(symbol)
                        dpg.add_text("Error")
                        dpg.add_text("Error")
                        dpg.add_text("Error")
                        dpg.add_text("Error")
                        dpg.add_text("Error")

            self.show_message("Stock comparison completed", "success")
        except Exception as e:
            self.show_message(f"Error: {e}", "error")

    def calculate_portfolio_metrics(self, portfolio):
        """Calculate basic portfolio metrics"""
        investment = 0
        current_value = 0
        returns = pd.Series(dtype=float)

        for symbol, data in portfolio.items():
            if isinstance(data, dict):
                quantity = data.get("quantity", 0)
                avg_price = data.get("avg_price", 0)
                investment += quantity * avg_price

                try:
                    ticker = yf.Ticker(symbol)
                    current_price = ticker.history(period="1d")["Close"].iloc[-1]
                    current_value += quantity * current_price
                except:
                    current_value += quantity * avg_price

        return investment, current_value, returns

    def load_portfolios(self):
        """Load portfolios from settings"""
        try:
            if os.path.exists(SETTINGS_FILE):
                with open(SETTINGS_FILE, "r") as f:
                    settings = json.load(f)
                    portfolios = settings.get("portfolios", {})
                    # Convert keys to lowercase for case-insensitive matching
                    return {k.lower(): v for k, v in portfolios.items()}
            return {}
        except Exception:
            return {}

    def clear_table(self, table_tag):
        """Clear table rows"""
        if dpg.does_item_exist(table_tag):
            children = dpg.get_item_children(table_tag, slot=1)
            if children:
                for child in children:
                    dpg.delete_item(child)

    def show_message(self, message, message_type="info"):
        """Show message to user"""
        if hasattr(self.app, 'show_message'):
            self.app.show_message(message, message_type)
        else:
            logger.info(f"{message_type.upper()}: {message}", module="Consumer_Behaviour_Tab", context={'message': message})

    def cleanup(self):
        """Cleanup resources"""
        self.analysis_running = False