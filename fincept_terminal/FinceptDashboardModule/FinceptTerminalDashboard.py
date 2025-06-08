import asyncio
from textual.app import ComposeResult
from textual.binding import Binding
from textual.screen import Screen
from textual.containers import Container, Vertical, VerticalScroll
from textual.widgets import Static, Link, DataTable, Header, TabbedContent, TabPane, Footer, Markdown, Button
from textual import log, on
import sys, os
import warnings
import logging

# Suppress all FutureWarnings
warnings.simplefilter(action='ignore', category=FutureWarning)

# Optionally, suppress warnings from specific libraries
logging.getLogger("yfinance").setLevel(logging.ERROR)
logging.getLogger("urllib3").setLevel(logging.ERROR)
logging.getLogger("asyncio").setLevel(logging.ERROR)

BINDINGS = [
    Binding(key="q", action="quit", description="Quit the app"),
]

class FinceptTerminalDashboard(Screen):
    """Main Dashboard Screen."""

    # Dynamically find the CSS file
    BASE_DIR = getattr(sys, '_MEIPASS', os.path.abspath(os.path.dirname(__file__)))
    CSS_PATH = os.path.join(BASE_DIR, "FinceptTerminalDashboard.tcss")

    SIDEBAR_TAB_MAPPING = {
        "world_tracker": "world-tracker",                    
        "economic_analysis": "economic-analysis",
        "financial_markets": "financial-markets",
        "market_prediction": "market-prediction",
        "ai_powered_research": "ai-research",
        "finscript": "finscript",
        "portfolio_management": "portfolio-management",      
        "edu_resources": "edu-resources",
        "forum": "forum",
        "settings": "FinceptSettingModule",
        "help_about": "help-about",
        # "terminal_docs": "terminal_docs"      # External link, not a TabPane
    }

    # To work with tab to sidebar mapping.
    TAB_TO_BUTTON_MAPPING = {v: k for k, v in SIDEBAR_TAB_MAPPING.items()}

    def __init__(self):
        super().__init__()
        from fincept_terminal.FinceptNewsModule.FinceptTerminalNewsFooter import NewsManager
        self.news_manager = NewsManager()
        from fincept_terminal.FinceptUtilsModule.DashboardUtils.RotatingTickerUtil import AsyncRotatingStockTicker
        self.ticker = AsyncRotatingStockTicker()

        # Track the currently active button
        self.active_button_id = "world_tracker"  # Default active button

    def compose(self) -> ComposeResult:
        """Compose the professional dashboard layout."""
        yield Header(show_clock=True,name="Fincept Terminal")
        yield self.ticker  # Adding the Stock Ticker Below the Header

        with Container(id="dashboard-grid"):
            # Sidebar
            with VerticalScroll(id="sidebar-scroll", classes="sidebar"):
                # yield Static("Menu not Working")
                yield Static("-------------------")
                yield Button("World Tracker", id="world_tracker", classes="sidebar-button sidebar-button-active")
                yield Static("-------------------")
                yield Button("Economic Analysis", id="economic_analysis", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("Financial Markets", id="financial_markets", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("Market Prediction", id="market_prediction", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("AI-Powered Research", id="ai_powered_research", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("FinScript", id="finscript", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("Portfolio Management", id="portfolio_management", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("Edu. & Resources", id="edu_resources", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("Forum", id="forum", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("Settings", id="settings", classes="sidebar-button")
                yield Static("-------------------")
                yield Button("Help & About", id="help_about", classes="sidebar-button")
                yield Static("-------------------")
                yield Link("Terminal Docs", url="https://docs.fincept.in/", tooltip="View Scenario Analysis",
                           id="terminal_docs")
                yield Static("-------------------")

            # Main Content Inside TabbedContent
            with Container(classes="main-content"):
                with TabbedContent(initial="world-tracker", id="main-tabs"):
                    with TabPane("World Tracker", id="world-tracker"):
                        # **Nested TabbedContent inside "World Tracker"**
                        with TabbedContent(initial="world_market_trackers"):

                            with TabPane("World Market Tracker", id="world_market_trackers"):
                                from fincept_terminal.FinceptDashboardModule.FinceptTerminalWorldMarketTracker import \
                                    MarketTab
                                yield MarketTab()
                                
                            with TabPane("World Sentiment Tracker", id="global_sentiment_tab"):
                                from fincept_terminal.FinceptDashboardModule.FinceptTerminalSentimentTracker import YouTubeTranscriptApp
                                yield YouTubeTranscriptApp()

                            with TabPane("Stock Tracker", id="stock_tracker"):
                                from fincept_terminal.FinceptDashboardModule.FinceptTerminalStockTracker import StockTrackerTab
                                yield StockTrackerTab()

                            with TabPane("Watchlist", id="watchlist"):
                                from fincept_terminal.FinceptDashboardModule.FinceptWatchlist import WatchlistApp
                                yield WatchlistApp()


                    # Other Main Tabs
                    with TabPane("Economic Analysis", id="economic-analysis"):
                        with TabbedContent():
                            with TabPane("DataGovIN"):
                                from fincept_terminal.FinceptEcoAnModule.WorldEconomyData.EcoIndia.DataGovIN_economy import DataGovINtab
                                yield DataGovINtab()

                            with TabPane("EconDB"):
                                from fincept_terminal.FinceptChannelIntegration.DataProvider.EconDB.FinceptTerminalEconDBTab import EconDBTab
                                yield EconDBTab()


                    with TabPane("Financial Markets", id="financial-markets"):
                        with TabbedContent():
                            with TabPane("Stock Search", id="stock_searchs"):
                                from fincept_terminal.FinceptFinMarketModule.FinceptTerminalStockSearchTab import \
                                    StockTrackerTab
                                yield StockTrackerTab()
                            with TabPane("Robo Advisor", id="robo_advisor"):
                                from fincept_terminal.FinceptFinMarketModule.robo_advisory.FinceptTerminalRoboAdvisorTab import \
                                    RoboAdvisorTab
                                yield RoboAdvisorTab()
                            with TabPane("Global Index", id="global_index"):
                                from fincept_terminal.FinceptFinMarketModule.FinceptTerminalGlobalIndexTab import \
                                    GlobalIndicesTab
                                yield GlobalIndicesTab()
                            with TabPane("ETF Market", id="etf_market"):
                                from fincept_terminal.FinceptFinMarketModule.FinceptTerminalETFTab import \
                                    ETFMarketTab
                                yield ETFMarketTab()
                            with TabPane("Currency Market", id="currency_market"):
                                from fincept_terminal.FinceptFinMarketModule.FinceptTerminalCurrencyTab import \
                                    CurrencyForexTab
                                yield CurrencyForexTab()
                            with TabPane("Crypto Market", id="crypto_market"):
                                from fincept_terminal.FinceptFinMarketModule.FinceptTerminalCryptoTab import \
                                    CryptoMarketTab
                                yield CryptoMarketTab()
                            with TabPane("Global Funds", id="global_funds"):
                                from fincept_terminal.FinceptFinMarketModule.FinceptTerminalGlobalFundTab import \
                                    GlobalFundsTab
                                yield GlobalFundsTab()
                            with TabPane("Money Market", id="money_market"):
                                from fincept_terminal.FinceptFinMarketModule.FinceptTerminalMoneyMarketTab import \
                                    MoneyMarketTab
                                yield MoneyMarketTab()
                            with TabPane("Other Analysis", id="consumer_comparison_tab"):
                                with TabbedContent():
                                    with TabPane("Consumer Behaviour", id="consumer_behaviour"):
                                        from fincept_terminal.FinceptFinMarketModule.FinceptTerminalConsumerCompareTab import ConsumerBehaviorTab
                                        yield ConsumerBehaviorTab()
                                    with TabPane("Comparison Analysis", id="comparison_analysis"):
                                        from fincept_terminal.FinceptFinMarketModule.FinceptTerminalConsumerCompareTab import \
                                            ComparisonAnalysisTab
                                        yield ComparisonAnalysisTab()


                    with TabPane("Market Prediction", id="market-prediction"):
                        with TabbedContent():
                            with TabPane("Stock Prediction"):
                                from fincept_terminal.FinceptMarketPredictionModule.FinceptTerminalPredictor import PredictorTab
                                yield PredictorTab()


                    with TabPane("AI-Powered Research", id="ai-research"):
                        with TabbedContent():
                            with TabPane("GenAI Chat", id="genai_chat"):
                                from fincept_terminal.FinceptAIPwrResModule.FinceptTerminalGenAI import GenAITab
                                yield GenAITab()
                            with TabPane("AI Hedge Fund", id="ai_hedge_fund"):
                                from fincept_terminal.FinceptAIPwrResModule.ai_hedge_fund.FinceptTerminalAIHedgeFundTab import AIAgentsTab
                                yield AIAgentsTab()


                    with TabPane("FinScript", id="finscript"):
                        with TabbedContent():
                            with TabPane("Code Editor", id="code_editor"):
                                from fincept_terminal.FinceptLangModule.finscript import FinceptLangScreen
                                yield FinceptLangScreen()


                    with TabPane("Portfolio Management", id="portfolio-management"):
                        from fincept_terminal.FinceptPortfolioModule.FinceptTerminalPortfolioTab import \
                            PortfolioTab
                        yield PortfolioTab()
                    with TabPane("Edu. & Resources", id="edu-resources"):
                        yield Markdown("# Educational & Resources Content")

                    with TabPane("Forum", id="forum"):
                        from fincept_terminal.FinceptForumModule.FinceptTerminalGlobalForumTab import ForumTab
                        yield ForumTab()
                    with TabPane("Settings", id="FinceptSettingModule"):
                        from fincept_terminal.FinceptSettingModule.FinceptTerminalSettings import SettingsScreen
                        yield SettingsScreen()
                    with TabPane("Help & About", id="help-about"):
                        from fincept_terminal.FinceptUtilsModule.FinceptTerminalHelpTab.FinceptTerminalHelpTab import HelpWindow
                        yield HelpWindow()

            # News Section (Always Visible)
            with Vertical(classes="news-section", id="news-section"):
                yield Static("📰 Latest News", id="news-title", classes="news-title")
                yield DataTable(id="news-table")

        yield Footer()


    async def on_mount(self) -> None:
        """On mount, do an initial fetch & display, then start the auto refresh."""
        # 1) Initial fetch
        initial = await self.news_manager.fetch_news()
        if initial:
            self.news_manager.latest_items = initial

        # 2) Display once
        await self.news_manager.display_in_table(self, self.news_manager.latest_items)

        # 3) Start auto-refresh
        asyncio.create_task(self.news_manager.auto_refresh_news(self))
        asyncio.create_task(self.ticker.refresh_stock_data_periodically())
    

    def update_sidebar_button_state(self, new_active_button_id):
        """Update the visual state of sidebar buttons - only one active at a time."""
        # Remove active class from the previously active button
        if self.active_button_id:
            prev_button = self.query_one(f"#{self.active_button_id}", Button)
            prev_button.remove_class("sidebar-button-active")
        
            # Add active class to the new active button
            new_button = self.query_one(f"#{new_active_button_id}", Button)
            new_button.add_class("sidebar-button-active")
        
        # Update the tracked active button
        self.active_button_id = new_active_button_id


    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle sidebar button presses for tab switching."""
        button_id = event.button.id
        
        # Get the corresponding tab ID from the mapping
        if button_id in self.SIDEBAR_TAB_MAPPING:
            tab_id = self.SIDEBAR_TAB_MAPPING[button_id]

            # Update button visual states
            self.update_sidebar_button_state(button_id)

            # switch the tab
            self.action_switch_tab(tab_id)

    @on(TabbedContent.TabActivated, "#main-tabs")
    def on_tab_activated(self, event) -> None:
        """Update sidebar when main tab is changed via TabbedContent."""
        # if event.tab.parent.id == "main-tabs":
        tab_id = event.pane.id
        button_id = self.TAB_TO_BUTTON_MAPPING[tab_id]

        if button_id:
            self.update_sidebar_button_state(button_id)


    def action_switch_tab(self, tab_id: str) -> None:
        """Activates the tab-pane as per the sidebar click."""
        main_content = self.query_one("#main-tabs", TabbedContent)
        if main_content:
            main_content.active = tab_id
        else:
            log("Main TabbedContent not found")


if __name__ == "__main__":
    app = FinceptTerminalDashboard()
    app.run()