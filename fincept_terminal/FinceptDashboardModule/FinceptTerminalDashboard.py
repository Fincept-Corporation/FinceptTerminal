import asyncio
from textual.app import ComposeResult
from textual.screen import Screen
from textual.containers import Container, Vertical, VerticalScroll
from textual.widgets import Static, Link, DataTable, Header, TabbedContent, TabPane, Footer, Markdown


class FinceptTerminalDashboard(Screen):
    """Main Dashboard Screen."""
    CSS_PATH = "FinceptTerminalDashboard.tcss"

    def __init__(self):
        super().__init__()
        from fincept_terminal.FinceptTerminalNewsModule.FinceptTerminalNewsFooter import NewsManager
        self.news_manager = NewsManager()
        from fincept_terminal.FinceptTerminalUtils.DashboardUtils.RotatingTickerUtil import AsyncRotatingStockTicker
        self.ticker = AsyncRotatingStockTicker()

    def compose(self) -> ComposeResult:
        """Compose the professional dashboard layout."""
        yield Header(show_clock=True,name="Fincept Terminal")
        yield self.ticker  # Adding the Stock Ticker Below the Header

        with Container(id="dashboard-grid"):
            # Sidebar
            with VerticalScroll(id="sidebar-scroll", classes="sidebar"):
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
                        with TabbedContent(initial="world_market_tracker"):
                            with TabPane("Stock Search", id="stock_search"):
                                from fincept_terminal.FinceptTerminalStockScreen.FinceptTerminalStockSearchTab import StockTrackerTab
                                yield StockTrackerTab()

                            with TabPane("Global Funds", id="global_funds"):
                                yield Static("Loading exchanges for funds...", id="funds-title")
                                yield DataTable(id="fund-exchange-table")
                                yield DataTable(id="funds-table")

                            with TabPane("World Sentiment Tracker", id="global_sentiment_tab"):
                                yield Static("Sentiment Data of India From Different Parts", id="sentiment_text")

                                # Grid container inside TabPane
                                with Container(id="sentiment_grid"):
                                    yield Static("Region 1: Positive", id="region_1")
                                    yield Static("Region 2: Neutral", id="region_2")
                                    yield Static("Region 3: Negative", id="region_3")
                                    yield Static("Region 4: Mixed", id="region_4")
                                    yield Static("Region 5: Mixed", id="region_5")
                                    yield Static("Region 6: Mixed", id="region_6")

                            with TabPane("World Market Tracker", id="world_market_tracker"):
                                from fincept_terminal.FinceptDashboardModule.FinceptTerminalWorldMarketTracker import \
                                    MarketTab
                                yield MarketTab()

                    # Other Main Tabs
                    with TabPane("Economic Analysis", id="economic-analysis"):
                        yield Markdown("# Economic Analysis Content")

                    with TabPane("Financial Markets", id="financial-markets"):
                        yield Markdown("# Financial Markets Content")
                    with TabPane("AI-Powered Research", id="ai-research"):
                        from fincept_terminal.FinceptTerminalGenAI.FinceptTerminalGenAI import GenAIScreen
                        yield GenAIScreen()
                    with TabPane("FinScript", id="finscript"):
                        yield Markdown("# FinScript Content")
                    with TabPane("Portfolio Management", id="portfolio-management"):
                        from fincept_terminal.FinceptTerminalPortfolioModule.FinceptTerminalPortfolioTab import \
                            PortfolioTab
                        yield PortfolioTab()
                    with TabPane("Edu. & Resources", id="edu-resources"):
                        yield Markdown("# Educational & Resources Content")
                    with TabPane("Settings", id="FinceptTerminalSettings"):
                        from fincept_terminal.FinceptTerminalSettings.FinceptTerminalSettings import SettingsScreen
                        yield SettingsScreen()
                    with TabPane("Help & About", id="help-about"):
                        from fincept_terminal.FinceptTerminalUtils.FinceptTerminalHelpTab.FinceptTerminalHelpTab import HelpWindow
                        yield HelpWindow()
                    with TabPane("Exit", id="exit"):
                        yield Markdown("# Closing Application...")

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
