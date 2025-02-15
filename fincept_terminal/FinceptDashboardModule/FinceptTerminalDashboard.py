import asyncio
from textual.app import ComposeResult
from textual.binding import Binding
from textual.screen import Screen
from textual.containers import Container, Vertical, VerticalScroll
from textual.widgets import Static, Link, DataTable, Header, TabbedContent, TabPane, Footer, Markdown, Button
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

    def __init__(self):
        super().__init__()
        from fincept_terminal.FinceptNewsModule.FinceptTerminalNewsFooter import NewsManager
        self.news_manager = NewsManager()
        from fincept_terminal.FinceptUtilsModule.DashboardUtils.RotatingTickerUtil import AsyncRotatingStockTicker
        self.ticker = AsyncRotatingStockTicker()

    def compose(self) -> ComposeResult:
        """Compose the professional dashboard layout."""
        yield Header(show_clock=True,name="Fincept Terminal")
        yield self.ticker  # Adding the Stock Ticker Below the Header

        with Container(id="dashboard-grid"):
            # Sidebar
            with VerticalScroll(id="sidebar-scroll", classes="sidebar"):
                yield Static("Menu not Working")
                yield Static("-------------------")
                yield Link("Overview", url="#", tooltip="Go to Overview", id="overview")
                yield Static("-------------------")
                yield Link("Balance Sheet", url="#", tooltip="View Balance Sheet", id="balance_sheet")
                yield Static("-------------------")
                yield Link("Cash Flow", url="#", tooltip="View Cash Flow", id="cash_flow")
                yield Static("-------------------")
                yield Link("Aging Report", url="#", tooltip="View Aging Report", id="aging_report")
                yield Static("-------------------")
                yield Link("Forecasting", url="#", tooltip="View Forecasting", id="forecasting")
                yield Static("-------------------")
                yield Link("Terminal Docs", url="https://docs.fincept.in/", tooltip="View Scenario Analysis",
                           id="scenario")
                yield Static("-------------------")

                # 15 Additional Dummy Options
                yield Link("Market Insights", url="#", tooltip="Latest Market Insights", id="market_insights")
                yield Static("-------------------")
                yield Link("Economic Calendar", url="#", tooltip="View Upcoming Economic Events",
                           id="economic_calendar")
                yield Static("-------------------")
                yield Link("Technical Indicators", url="#", tooltip="Explore Technical Indicators",
                           id="technical_indicators")
                yield Static("-------------------")
                yield Link("Stock Screeners", url="#", tooltip="Find the Best Stocks", id="stock_screeners")
                yield Static("-------------------")
                yield Link("AI Market Predictions", url="#", tooltip="AI-driven Market Trends",
                           id="ai_market_predictions")
                yield Static("-------------------")
                yield Link("Risk Analysis", url="#", tooltip="Assess Market Risks", id="risk_analysis")
                yield Static("-------------------")
                yield Link("Sector Performance", url="#", tooltip="Track Sector-Wise Performance",
                           id="sector_performance")
                yield Static("-------------------")
                yield Link("Top Gainers & Losers", url="#", tooltip="Daily Market Movers", id="top_gainers_losers")
                yield Static("-------------------")
                yield Link("Crypto Market", url="#", tooltip="Latest Crypto Trends", id="crypto_market")
                yield Static("-------------------")
                yield Link("Commodities", url="#", tooltip="Gold, Silver, Oil & More", id="commodities")
                yield Static("-------------------")
                yield Link("Forex Rates", url="#", tooltip="Live Currency Exchange Rates", id="forex_rates")
                yield Static("-------------------")
                yield Link("ETF Analysis", url="#", tooltip="Track Exchange-Traded Funds", id="etf_analysis")
                yield Static("-------------------")
                yield Link("Options & Derivatives", url="#", tooltip="View Derivatives Data", id="options_derivatives")
                yield Static("-------------------")
                yield Link("Market Sentiment", url="#", tooltip="Track Investor Sentiment", id="market_sentiment")
                yield Static("-------------------")
                yield Link("Global Indices", url="#", tooltip="World Stock Market Indices", id="global_indices")
                yield Static("-------------------")
                yield Link("News & Reports", url="#", tooltip="Financial News & Reports", id="news_reports")
                yield Static("-------------------")

            # Main Content Inside TabbedContent
            with Container(classes="main-content"):
                with TabbedContent(initial="world-tracker"):
                    with TabPane("World Tracker", id="world-tracker"):
                        # **Nested TabbedContent inside "World Tracker"**
                        with TabbedContent(initial="world_market_trackers"):

                            with TabPane("World Market Tracker", id="world_market_trackers"):
                                from fincept_terminal.FinceptDashboardModule.FinceptTerminalWorldMarketTracker import \
                                    MarketTab
                                yield MarketTab()

                            with TabPane("Watchlist", id="dashboard_watchlist"):
                                from fincept_terminal.FinceptDashboardModule.FinceptWatchlist import \
                                    WatchlistApp
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



if __name__ == "__main__":
    app = FinceptTerminalDashboard()
    app.run()
