# -*- coding: utf-8 -*-
"""
Robo Advisor Tab module for Fincept Terminal
Updated to use centralized logging system
"""

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import requests
from urllib import parse
import pandas as pd
import yfinance as yf
import threading
import time

from fincept_terminal.Utils.Logging.logger import logger, log_operation

class RoboAdvisorTab(BaseTab):
    """AI-Powered Robo-Advisor Interface for Portfolio Creation"""

    def __init__(self, app):
        super().__init__(app)
        self.selected_country = None
        self.selected_sector = None
        self.selected_industry = None
        self.analysis_running = False
        self.stocks_data = pd.DataFrame()

    def get_label(self):
        return "🤖 Robo-Advisor"

    def create_content(self):
        """Create the robo-advisor interface"""
        with dpg.child_window(
                tag="robo_advisor_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            # Header section
            self.add_section_header("🤖 AI-Powered Robo-Advisor")
            dpg.add_text("Create Your Portfolio Based on AI and Data-Driven Insights",
                         color=[200, 200, 255], wrap=self.app.usable_width - 40)
            dpg.add_spacer(height=20)

            # Selection panels
            self.create_selection_panels()
            dpg.add_spacer(height=20)

            # Analysis controls
            self.create_analysis_controls()
            dpg.add_spacer(height=20)

            # Results section
            self.create_results_section()

            # Initialize country list
            self.show_message("Loading country options...", "info")
            threading.Thread(target=self.populate_country_list, daemon=True).start()

    def create_selection_panels(self):
        """Create the selection panels for country, sector, and industry"""
        # Country Selection Panel
        with dpg.collapsing_header(label=" Select a Country", default_open=True):
            dpg.add_text("Choose a country to analyze markets:", color=[255, 200, 100])
            dpg.add_spacer(height=5)

            with dpg.child_window(height=150, border=True):
                dpg.add_listbox([], tag="country_selector", width=-1,
                                callback=self.on_country_selected, num_items=6)

        dpg.add_spacer(height=15)

        # Sector Selection Panel
        with dpg.collapsing_header(label=" Select a Sector", default_open=False):
            dpg.add_text("Choose a sector for focused analysis:", color=[255, 200, 100])
            dpg.add_spacer(height=5)

            with dpg.child_window(height=150, border=True):
                dpg.add_listbox([], tag="sector_selector", width=-1,
                                callback=self.on_sector_selected, num_items=6)

        dpg.add_spacer(height=15)

        # Industry Selection Panel
        with dpg.collapsing_header(label=" Select an Industry", default_open=False):
            dpg.add_text("Choose a specific industry for deep analysis:", color=[255, 200, 100])
            dpg.add_spacer(height=5)

            with dpg.child_window(height=150, border=True):
                dpg.add_listbox([], tag="industry_selector", width=-1,
                                callback=self.on_industry_selected, num_items=6)

    def create_analysis_controls(self):
        """Create analysis control panel"""
        dpg.add_text(" Portfolio Analysis", color=[100, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        with dpg.group(horizontal=True):
            # Analysis button
            dpg.add_button(
                label=" Run AI Analysis",
                callback=self.run_analysis_callback,
                width=180,
                height=40,
                tag="run_analysis_button"
            )

            dpg.add_spacer(width=20)

            # Analysis progress indicator
            dpg.add_text("Ready to analyze", tag="analysis_status", color=[200, 200, 200])

            dpg.add_spacer(width=20)

            # Clear selections button
            dpg.add_button(
                label=" Clear Selections",
                callback=self.clear_selections,
                width=140,
                height=40
            )

        dpg.add_spacer(height=10)

        # Selection summary
        with dpg.child_window(height=80, border=True):
            dpg.add_text(" Current Selections:", color=[255, 255, 100])
            dpg.add_text("Country: Not selected", tag="selected_country_text")
            dpg.add_text("Sector: Not selected", tag="selected_sector_text")
            dpg.add_text("Industry: Not selected", tag="selected_industry_text")

    def create_results_section(self):
        """Create the portfolio results section"""
        dpg.add_text(" Generated Portfolio", color=[100, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=10)

        # Portfolio analysis table
        with dpg.table(
                tag="generated_portfolio_table",
                resizable=True,
                policy=dpg.mvTable_SizingStretchProp,
                borders_innerH=True,
                borders_outerH=True,
                borders_innerV=True,
                borders_outerV=True,
                height=300
        ):
            # Table columns will be added dynamically when results are available
            pass

        dpg.add_spacer(height=15)

        # Portfolio summary
        with dpg.child_window(height=100, border=True, tag="portfolio_summary_container"):
            dpg.add_text(" Portfolio Summary", color=[255, 200, 100])
            dpg.add_text("Run analysis to see portfolio recommendations", color=[200, 200, 200])

    # ============================================================================
    # DATA FETCHING METHODS
    # ============================================================================

    def populate_country_list(self):
        """Populate the country list"""
        try:
            continent_countries = {
                "Asia": [
                    "Afghanistan", "Bangladesh", "Cambodia", "China", "India", "Indonesia", "Japan", "Kazakhstan",
                    "Kyrgyzstan", "Malaysia", "Mongolia", "Myanmar", "Philippines", "Singapore", "South Korea",
                    "Taiwan", "Thailand", "Vietnam"
                ],
                "Europe": [
                    "Austria", "Azerbaijan", "Belgium", "Cyprus", "Czech Republic", "Denmark", "Estonia", "Finland",
                    "France", "Germany", "Greece", "Hungary", "Iceland", "Ireland", "Italy", "Latvia", "Lithuania",
                    "Luxembourg", "Malta", "Monaco", "Netherlands", "Norway", "Poland", "Portugal", "Romania", "Russia",
                    "Slovakia", "Slovenia", "Spain", "Sweden", "Switzerland", "United Kingdom"
                ],
                "Africa": [
                    "Botswana", "Egypt", "Gabon", "Ghana", "Ivory Coast", "Kenya", "Morocco", "Mozambique", "Namibia",
                    "Nigeria", "South Africa", "Zambia"
                ],
                "North America": [
                    "Bahamas", "Barbados", "Belize", "Bermuda", "Canada", "Cayman Islands", "Costa Rica", "Mexico",
                    "Panama", "United States"
                ],
                "South America": [
                    "Argentina", "Brazil", "Chile", "Colombia", "Peru", "Uruguay"
                ],
                "Oceania": [
                    "Australia", "Fiji", "New Zealand", "Papua New Guinea"
                ],
                "Middle East": [
                    "Israel", "Jordan", "Saudi Arabia", "United Arab Emirates", "Qatar"
                ]
            }

            # Flatten the country list
            countries = [country for sublist in continent_countries.values() for country in sublist]
            countries.sort()  # Sort alphabetically

            # Update the listbox
            dpg.configure_item("country_selector", items=countries)
            self.show_message("Country list loaded successfully!", "success")

        except Exception as e:
            self.show_message(f"Error loading countries: {e}", "error")

    def populate_sector_list(self, country):
        """Populate sectors for selected country"""
        try:
            sectors = self.fetch_sectors_by_country(country)
            dpg.configure_item("sector_selector", items=sectors)
            self.show_message(f"Sectors for {country} loaded successfully!", "success")
        except Exception as e:
            self.show_message(f"Error loading sectors: {e}", "error")

    def populate_industry_list(self, country, sector):
        """Populate industries for selected sector"""
        try:
            industries = self.fetch_industries_by_sector(country, sector)
            dpg.configure_item("industry_selector", items=industries)
            self.show_message(f"Industries for {sector} in {country} loaded successfully!", "success")
        except Exception as e:
            self.show_message(f"Error loading industries: {e}", "error")

    @staticmethod
    def fetch_sectors_by_country(country):
        """Fetch sectors for a specific country"""
        url = f"https://finceptbackend.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks?filter_column=country&filter_value={country}"
        try:
            response = requests.get(url, timeout=10)
            response.raise_for_status()
            data = response.json()
            return data.get("sectors", [])
        except requests.exceptions.RequestException as e:
            raise RuntimeError(f"Error fetching sectors for {country}: {e}")

    @staticmethod
    def fetch_industries_by_sector(country, sector):
        """Fetch industries for a specific sector"""
        sector_encoded = parse.quote(sector)
        url = f"https://finceptbackend.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks?filter_column=country&filter_value={country}&sector={sector_encoded}"
        try:
            response = requests.get(url, timeout=10)
            response.raise_for_status()
            data = response.json()
            return data.get("industries", [])
        except requests.exceptions.RequestException as e:
            raise RuntimeError(f"Error fetching industries for {sector} in {country}: {e}")

    def fetch_stocks_by_industry(self, country, sector, industry):
        """Fetch stocks for specific industry"""
        try:
            sector_encoded = parse.quote(sector)
            industry_encoded = parse.quote(industry)
            url = (
                f"https://finceptbackend.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks"
                f"?filter_column=country&filter_value={country}&sector={sector_encoded}&industry={industry_encoded}"
            )

            response = requests.get(url, timeout=15)
            response.raise_for_status()
            stock_data = response.json()

            self.show_message("Stock data found", "info")
            return pd.DataFrame(stock_data)

        except requests.exceptions.RequestException as e:
            self.show_message("No stock data found", "warning")
            return pd.DataFrame()

    # ============================================================================
    # CALLBACK METHODS
    # ============================================================================

    def on_country_selected(self, sender, app_data):
        """Handle country selection"""
        if app_data is not None:
            self.selected_country = app_data
            dpg.set_value("selected_country_text", f"Country: {self.selected_country}")

            # Clear subsequent selections
            self.selected_sector = None
            self.selected_industry = None
            dpg.set_value("selected_sector_text", "Sector: Not selected")
            dpg.set_value("selected_industry_text", "Industry: Not selected")
            dpg.configure_item("sector_selector", items=[])
            dpg.configure_item("industry_selector", items=[])

            self.show_message(f"Selected Country: {self.selected_country}", "info")

            # Load sectors in background
            threading.Thread(target=self.populate_sector_list, args=(self.selected_country,), daemon=True).start()

    def on_sector_selected(self, sender, app_data):
        """Handle sector selection"""
        if app_data is not None and self.selected_country:
            self.selected_sector = app_data
            dpg.set_value("selected_sector_text", f"Sector: {self.selected_sector}")

            # Clear industry selection
            self.selected_industry = None
            dpg.set_value("selected_industry_text", "Industry: Not selected")
            dpg.configure_item("industry_selector", items=[])

            self.show_message(f"Selected Sector: {self.selected_sector}", "info")

            # Load industries in background
            threading.Thread(target=self.populate_industry_list,
                             args=(self.selected_country, self.selected_sector), daemon=True).start()

    def on_industry_selected(self, sender, app_data):
        """Handle industry selection"""
        if app_data is not None:
            self.selected_industry = app_data
            dpg.set_value("selected_industry_text", f"Industry: {self.selected_industry}")
            self.show_message(f"Selected Industry: {self.selected_industry}", "info")

    def clear_selections(self):
        """Clear all selections"""
        self.selected_country = None
        self.selected_sector = None
        self.selected_industry = None

        dpg.set_value("selected_country_text", "Country: Not selected")
        dpg.set_value("selected_sector_text", "Sector: Not selected")
        dpg.set_value("selected_industry_text", "Industry: Not selected")

        dpg.configure_item("country_selector", default_value="")
        dpg.configure_item("sector_selector", items=[])
        dpg.configure_item("industry_selector", items=[])

        # Clear results table
        self.clear_table("generated_portfolio_table")

        self.show_message("All selections cleared", "info")

    def run_analysis_callback(self):
        """Callback for run analysis button"""
        if self.analysis_running:
            self.show_message("Analysis already in progress...", "warning")
            return

        if not all([self.selected_country, self.selected_sector, self.selected_industry]):
            self.show_message("Please select a country, sector, and industry before running analysis.", "error")
            return

        # Start analysis in background thread
        threading.Thread(target=self.run_robo_analysis, daemon=True).start()

    # ============================================================================
    # ANALYSIS METHODS
    # ============================================================================

    def run_robo_analysis(self):
        """Run the complete robo-advisor analysis"""
        try:
            self.analysis_running = True
            dpg.set_value("analysis_status", " Starting analysis...")
            self.show_message("Running Robo-Advisor Analysis...", "info")

            # Step 1: Fetch stocks
            dpg.set_value("analysis_status", " Fetching stock data...")
            stocks = self.fetch_stocks_by_industry(self.selected_country, self.selected_sector, self.selected_industry)

            if stocks.empty:
                self.show_message(
                    f"No stocks found for {self.selected_industry} in {self.selected_sector} ({self.selected_country}).",
                    "warning")
                return

            # Step 2: Run analysis
            dpg.set_value("analysis_status", "🧠 Running AI analysis...")
            portfolio = self.run_technical_fundamental_analysis(stocks)

            if portfolio.empty:
                self.show_message("No suitable stocks found after analysis.", "warning")
                return

            # Step 3: Display results
            dpg.set_value("analysis_status", " Generating portfolio...")
            self.display_portfolio_results(portfolio)

            dpg.set_value("analysis_status", " Analysis complete!")
            self.show_message("Portfolio generated successfully!", "success")

        except Exception as e:
            self.show_message(f"Analysis error: {e}", "error")
            dpg.set_value("analysis_status", " Analysis failed")
        finally:
            self.analysis_running = False

    def run_technical_fundamental_analysis(self, stocks):
        """Perform technical and fundamental analysis"""
        stock_analysis = []
        total_stocks = len(stocks)

        if stocks.empty or "symbol" not in stocks.columns:
            self.show_message("No stocks available for analysis.", "warning")
            return pd.DataFrame()

        for i, symbol in enumerate(stocks["symbol"]):
            try:
                # Update progress
                progress = f"Analyzing {symbol} ({i + 1}/{total_stocks})"
                dpg.set_value("analysis_status", f" {progress}")

                stock = yf.Ticker(symbol)
                info = stock.info
                hist = stock.history(period="1y")

                # Validate historical data
                if hist.empty or "Close" not in hist.columns:
                    continue

                # Calculate technical indicators
                sma_50 = hist["Close"].rolling(window=50).mean().iloc[-1] if len(hist) >= 50 else None
                sma_200 = hist["Close"].rolling(window=200).mean().iloc[-1] if len(hist) >= 200 else None
                current_price = hist["Close"].iloc[-1]

                # Skip if insufficient data
                if sma_50 is None or sma_200 is None:
                    continue

                # Get fundamental data
                market_cap = info.get("marketCap")
                if market_cap is None:
                    continue

                # Calculate scores
                fundamental_score = 10 if market_cap > 1e10 else (7 if market_cap > 1e9 else 5)
                technical_score = 10 if sma_50 > sma_200 else 5
                combined_score = fundamental_score + technical_score

                # Calculate additional metrics
                pe_ratio = info.get("trailingPE", "N/A")
                dividend_yield = info.get("dividendYield", 0) * 100 if info.get("dividendYield") else 0

                stock_analysis.append({
                    "Symbol": symbol,
                    "Company": info.get("longName", symbol),
                    "Current Price": f"${current_price:.2f}",
                    "Market Cap": f"${market_cap / 1e9:.2f}B" if market_cap > 1e9 else f"${market_cap / 1e6:.0f}M",
                    "P/E Ratio": f"{pe_ratio:.2f}" if isinstance(pe_ratio, (int, float)) else "N/A",
                    "Dividend Yield": f"{dividend_yield:.2f}%",
                    "SMA 50": f"${sma_50:.2f}",
                    "SMA 200": f"${sma_200:.2f}",
                    "Technical Score": technical_score,
                    "Fundamental Score": fundamental_score,
                    "Combined Score": combined_score,
                })

                # Small delay to prevent overwhelming the API
                time.sleep(0.1)

            except Exception as e:
                logger.error(f"Error analyzing {symbol}: {e}", module="Robo_Advisor_Tab", context={'symbol': symbol, 'e': e})
                continue

        # Convert to DataFrame and sort by combined score
        df = pd.DataFrame(stock_analysis)

        if df.empty:
            return pd.DataFrame()

        # Return top 10 stocks by combined score
        return df.sort_values("Combined Score", ascending=False).head(10)

    def display_portfolio_results(self, portfolio):
        """Display the generated portfolio in the table"""
        # Clear existing table
        self.clear_table("generated_portfolio_table")

        if portfolio.empty:
            return

        # Add columns
        columns = list(portfolio.columns)
        for col in columns:
            dpg.add_table_column(label=col, parent="generated_portfolio_table", width_fixed=True,
                                 init_width_or_weight=120)

        # Add rows
        for _, row in portfolio.iterrows():
            with dpg.table_row(parent="generated_portfolio_table"):
                for value in row:
                    dpg.add_text(str(value))

        # Update portfolio summary
        self.update_portfolio_summary(portfolio)

    def update_portfolio_summary(self, portfolio):
        """Update the portfolio summary section"""
        if dpg.does_item_exist("portfolio_summary_container"):
            # Clear existing content
            children = dpg.get_item_children("portfolio_summary_container", slot=1)
            if children:
                for child in children:
                    dpg.delete_item(child)

            # Add new summary content
            dpg.add_text(" Portfolio Summary", color=[255, 200, 100], parent="portfolio_summary_container")

            if not portfolio.empty:
                total_stocks = len(portfolio)
                avg_score = portfolio["Combined Score"].mean()
                top_stock = portfolio.iloc[0]["Symbol"]

                dpg.add_text(f" Total Recommendations: {total_stocks}", parent="portfolio_summary_container")
                dpg.add_text(f" Average Score: {avg_score:.1f}/20", parent="portfolio_summary_container")
                dpg.add_text(f" Top Pick: {top_stock}", parent="portfolio_summary_container")
            else:
                dpg.add_text("No recommendations available", parent="portfolio_summary_container",
                             color=[200, 200, 200])

    # ============================================================================
    # UTILITY METHODS
    # ============================================================================

    def clear_table(self, table_tag):
        """Clear all content from a table"""
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
            type_symbols = {
                "success": "",
                "error": "",
                "warning": "",
                "info": ""
            }
            symbol = type_symbols.get(message_type, "")
            logger.info(f"{symbol} {message}", module="Robo_Advisor_Tab", context={'symbol': symbol, 'message': message})

    def resize_components(self, left_width, center_width, right_width,
                          top_height, bottom_height, cell_height):
        """Handle component resizing"""
        try:
            if dpg.does_item_exist("robo_advisor_main_container"):
                # The child window will automatically resize with parent
                pass
        except Exception as e:
            logger.error(f"Error resizing robo-advisor components: {e}", module="Robo_Advisor_Tab", context={'e': e})

    def cleanup(self):
        """Clean up robo-advisor tab resources"""
        try:
            # Stop any running analysis
            self.analysis_running = False

            # Clean up data
            self.stocks_data = pd.DataFrame()
            self.selected_country = None
            self.selected_sector = None
            self.selected_industry = None

            logger.info("Robo-advisor tab cleaned up", module="Robo_Advisor_Tab")
        except Exception as e:
            logger.error(f" Robo-advisor tab cleanup error: {e}", module="Robo_Advisor_Tab", context={'e': e})