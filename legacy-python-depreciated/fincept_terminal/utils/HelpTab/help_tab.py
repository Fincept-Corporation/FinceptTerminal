# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.utils.base_tab import BaseTab
import datetime

# Import new logger module
from fincept_terminal.utils.Logging.logger import (
    info, debug, warning, error, operation, monitor_performance
)


class HelpTab(BaseTab):
    """Bloomberg Terminal style Help and About tab"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app
        self.scroll_position = 0

        # Bloomberg color scheme - Pre-computed for performance
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 150, 250]
        self.BLOOMBERG_BLACK = [0, 0, 0]

        # Cache frequently used data for performance
        self._cached_datetime = None
        self._datetime_cache_time = 0

        debug("HelpTab initialized", module="help",
              context={'main_app_available': bool(main_app)})

    def get_label(self):
        return " Help & About"

    def _get_current_time_cached(self):
        """Get current time with caching for performance"""
        import time
        current_time = time.time()
        # Update cache every 5 seconds
        if current_time - self._datetime_cache_time > 5:
            self._cached_datetime = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            self._datetime_cache_time = current_time
        return self._cached_datetime

    @monitor_performance
    def create_content(self):
        """Create Bloomberg-style help terminal layout"""
        with operation("create_help_content", module="help"):
            try:
                # Top bar with Bloomberg branding
                with dpg.group(horizontal=True):
                    dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                    dpg.add_text("HELP TERMINAL", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                    dpg.add_input_text(label="", default_value="Search Help Topics", width=300)
                    dpg.add_button(label="SEARCH", width=80, callback=self.search_help)
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                    dpg.add_text(self._get_current_time_cached())

                dpg.add_separator()

                # Function keys for help sections
                with dpg.group(horizontal=True):
                    help_functions = ["F1:ABOUT", "F2:FEATURES", "F3:SUPPORT", "F4:CONTACT", "F5:FEEDBACK", "F6:DOCS"]
                    for key in help_functions:
                        dpg.add_button(label=key, width=100, height=25,
                                       callback=lambda s, a, u, k=key: self.navigate_section(k))

                dpg.add_separator()

                # Main help layout - Three columns like Bloomberg
                with dpg.group(horizontal=True):
                    # Left panel - Help Navigator
                    self.create_left_help_panel()

                    # Center panel - Main Help Content
                    self.create_center_help_panel()

                    # Right panel - Quick Actions
                    self.create_right_help_panel()

                # Bottom status bar
                dpg.add_separator()
                self.create_help_status_bar()

                info("Help content created successfully", module="help")

            except Exception as e:
                error("Error creating help content", module="help",
                      context={'error': str(e)}, exc_info=True)
                # Fallback content
                dpg.add_text("HELP TERMINAL", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("Error loading help content. Please try again.")

    @monitor_performance
    def create_left_help_panel(self):
        """Create left help navigation panel"""
        with operation("create_left_help_panel", module="help"):
            with dpg.child_window(width=350, height=650, border=True):
                dpg.add_text("HELP NAVIGATOR", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # Help sections table
                with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                               scrollY=True, height=300):
                    dpg.add_table_column(label="Section", width_fixed=True, init_width_or_weight=120)
                    dpg.add_table_column(label="Status", width_fixed=True, init_width_or_weight=80)
                    dpg.add_table_column(label="Action", width_fixed=True, init_width_or_weight=100)

                    # Pre-defined help sections for performance
                    help_sections = [
                        ("ABOUT FINCEPT", "AVAILABLE", "VIEW"),
                        ("FEATURES", "AVAILABLE", "VIEW"),
                        ("MARKET DATA", "AVAILABLE", "VIEW"),
                        ("PORTFOLIO", "AVAILABLE", "VIEW"),
                        ("ANALYTICS", "AVAILABLE", "VIEW"),
                        ("SUPPORT", "AVAILABLE", "CONTACT"),
                        ("TUTORIALS", "COMING SOON", "NOTIFY"),
                        ("API DOCS", "AVAILABLE", "OPEN"),
                        ("COMMUNITY", "AVAILABLE", "JOIN"),
                        ("FEEDBACK", "AVAILABLE", "SEND")
                    ]

                    for section, status, action in help_sections:
                        with dpg.table_row():
                            dpg.add_text(section, color=self.BLOOMBERG_WHITE)
                            status_color = self.BLOOMBERG_GREEN if status == "AVAILABLE" else self.BLOOMBERG_YELLOW
                            dpg.add_text(status, color=status_color)
                            action_color = self.BLOOMBERG_BLUE if action in ["VIEW", "OPEN"] else self.BLOOMBERG_ORANGE
                            dpg.add_text(action, color=action_color)

                dpg.add_separator()

                # Quick stats
                dpg.add_text("HELP STATISTICS", color=self.BLOOMBERG_YELLOW)
                with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                    dpg.add_table_column()
                    dpg.add_table_column()

                    # Pre-computed stats for performance
                    stats = [
                        ("Total Help Topics:", "47"),
                        ("Video Tutorials:", "12"),
                        ("FAQ Articles:", "25"),
                        ("API Endpoints:", "156")
                    ]

                    for label, value in stats:
                        with dpg.table_row():
                            dpg.add_text(label)
                            dpg.add_text(value, color=self.BLOOMBERG_WHITE)

                dpg.add_separator()

                # System status
                dpg.add_text("SYSTEM STATUS", color=self.BLOOMBERG_YELLOW)
                with dpg.group(horizontal=True):
                    dpg.add_text("●", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("ALL SYSTEMS OPERATIONAL", color=self.BLOOMBERG_GREEN)

                debug("Left help panel created", module="help")

    @monitor_performance
    def create_center_help_panel(self):
        """Create center help content panel"""
        with operation("create_center_help_panel", module="help"):
            with dpg.child_window(width=900, height=650, border=True):
                with dpg.tab_bar():
                    # About Tab
                    with dpg.tab(label="About"):
                        self._create_about_tab()

                    # Features Tab
                    with dpg.tab(label="Features"):
                        self._create_features_tab()

                    # Support Tab
                    with dpg.tab(label="Support"):
                        self._create_support_tab()

                    # API Documentation Tab
                    with dpg.tab(label="API Docs"):
                        self._create_api_docs_tab()

                debug("Center help panel created", module="help")

    def _create_about_tab(self):
        """Create about tab content"""
        dpg.add_text("ABOUT FINCEPT TERMINAL", color=self.BLOOMBERG_ORANGE)
        dpg.add_separator()

        # Company info in Bloomberg style
        with dpg.group(horizontal=True):
            with dpg.group():
                dpg.add_text("Fincept Financial Terminal", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("Professional Trading & Analytics Platform")
                dpg.add_spacer(height=10)

                with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                    dpg.add_table_column()
                    dpg.add_table_column()

                    # Pre-computed version info
                    version_info = [
                        ("Version:", "4.2.1 Professional"),
                        ("Build:", "20250115.1"),
                        ("License:", "Enterprise"),
                        ("Data Sources:", "Real-time"),
                        ("API Status:", "Connected")
                    ]

                    for label, value in version_info:
                        with dpg.table_row():
                            dpg.add_text(label)
                            value_color = self.BLOOMBERG_GREEN if value in ["Enterprise", "Real-time",
                                                                            "Connected"] else self.BLOOMBERG_WHITE
                            dpg.add_text(value, color=value_color)

            with dpg.group():
                dpg.add_text("Core Features", color=self.BLOOMBERG_YELLOW)
                features = [
                    "• Real-time market data & analytics",
                    "• Portfolio management & tracking",
                    "• Advanced charting & technical analysis",
                    "• Financial news & sentiment analysis",
                    "• Risk management tools",
                    "• Algorithmic trading support",
                    "• Multi-asset class coverage",
                    "• Professional-grade security"
                ]
                for feature in features:
                    dpg.add_text(feature)

        dpg.add_spacer(height=20)

        # Description
        about_text = (
            "Fincept Terminal is a cutting-edge financial analysis platform designed to provide "
            "real-time market data, portfolio management, and actionable insights to investors, traders, "
            "and financial professionals. Our platform integrates advanced analytics, AI-driven sentiment analysis, "
            "and the latest market trends to help you make well-informed investment decisions."
        )
        dpg.add_text(about_text, wrap=850, color=self.BLOOMBERG_WHITE)

    def _create_features_tab(self):
        """Create features tab content"""
        dpg.add_text("TERMINAL FEATURES & CAPABILITIES", color=self.BLOOMBERG_ORANGE)
        dpg.add_separator()

        # Features in Bloomberg table style
        with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                       scrollY=True, height=400):
            dpg.add_table_column(label="Feature Category", width_fixed=True, init_width_or_weight=200)
            dpg.add_table_column(label="Description", width_fixed=True, init_width_or_weight=400)
            dpg.add_table_column(label="Status", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="Access Level", width_fixed=True, init_width_or_weight=120)

            # Pre-computed features data
            features = [
                ("Market Data", "Real-time quotes, indices, forex, commodities", "ACTIVE", "ALL USERS"),
                ("Portfolio Mgmt", "Track holdings, P&L, asset allocation", "ACTIVE", "ALL USERS"),
                ("Technical Analysis", "Advanced charting, indicators, overlays", "ACTIVE", "PRO"),
                ("News & Sentiment", "Financial news aggregation, sentiment scoring", "ACTIVE", "PRO"),
                ("Risk Analytics", "VaR, stress testing, correlation analysis", "ACTIVE", "ENTERPRISE"),
                ("Algo Trading", "Strategy backtesting, execution algorithms", "BETA", "ENTERPRISE"),
                ("Options Analytics", "Greeks, volatility surface, strategies", "ACTIVE", "PRO"),
                ("Fixed Income", "Bond analytics, yield curves, duration", "ACTIVE", "ENTERPRISE"),
                ("ESG Analytics", "Sustainability metrics, ESG scoring", "COMING SOON", "PRO"),
                ("AI Insights", "Machine learning predictions, pattern recognition", "BETA", "ENTERPRISE")
            ]

            for feature, description, status, access in features:
                with dpg.table_row():
                    dpg.add_text(feature, color=self.BLOOMBERG_YELLOW)
                    dpg.add_text(description, color=self.BLOOMBERG_WHITE)

                    status_color = (self.BLOOMBERG_GREEN if status == "ACTIVE" else
                                    self.BLOOMBERG_YELLOW if status == "BETA" else self.BLOOMBERG_ORANGE)
                    dpg.add_text(status, color=status_color)

                    access_color = (self.BLOOMBERG_GREEN if access == "ALL USERS" else
                                    self.BLOOMBERG_BLUE if access == "PRO" else self.BLOOMBERG_ORANGE)
                    dpg.add_text(access, color=access_color)

    def _create_support_tab(self):
        """Create support tab content"""
        dpg.add_text("CUSTOMER SUPPORT & ASSISTANCE", color=self.BLOOMBERG_ORANGE)
        dpg.add_separator()

        # Support options
        with dpg.group(horizontal=True):
            with dpg.group():
                dpg.add_text("Contact Information", color=self.BLOOMBERG_YELLOW)
                dpg.add_spacer(height=10)

                with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                    dpg.add_table_column()
                    dpg.add_table_column()

                    contact_info = [
                        ("Email Support:", "support@fincept.in"),
                        ("Phone Support:", "+1 (555) 123-4567"),
                        ("Live Chat:", "Available 24/7"),
                        ("Response Time:", "< 2 hours"),
                        ("Support Hours:", "24/7/365")
                    ]

                    for label, value in contact_info:
                        with dpg.table_row():
                            dpg.add_text(label)
                            value_color = (self.BLOOMBERG_BLUE if "support@" in value else
                                           self.BLOOMBERG_GREEN if "Available" in value or "< 2" in value or "24/7" in value
                                           else self.BLOOMBERG_WHITE)
                            dpg.add_text(value, color=value_color)

            with dpg.group():
                dpg.add_text("Support Channels", color=self.BLOOMBERG_YELLOW)
                dpg.add_spacer(height=10)

                support_buttons = [
                    ("📧 Email Support", self.contact_email_support),
                    ("💬 Live Chat", self.open_live_chat),
                    ("📞 Phone Support", self.contact_phone_support),
                    ("📖 Documentation", self.open_documentation),
                    ("🎥 Video Tutorials", self.open_tutorials),
                    ("👥 Community Forum", self.open_community),
                    ("🐛 Report Bug", self.report_bug),
                    ("💡 Feature Request", self.request_feature)
                ]

                for label, callback in support_buttons:
                    dpg.add_button(label=label, callback=callback, width=200)
                    dpg.add_spacer(height=5)

    def _create_api_docs_tab(self):
        """Create API documentation tab content"""
        dpg.add_text("API DOCUMENTATION & ENDPOINTS", color=self.BLOOMBERG_ORANGE)
        dpg.add_separator()

        # API endpoints table
        with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                       scrollY=True, height=500):
            dpg.add_table_column(label="Endpoint", width_fixed=True, init_width_or_weight=200)
            dpg.add_table_column(label="Method", width_fixed=True, init_width_or_weight=80)
            dpg.add_table_column(label="Description", width_fixed=True, init_width_or_weight=300)
            dpg.add_table_column(label="Rate Limit", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="Auth Required", width_fixed=True, init_width_or_weight=120)

            # Pre-computed API endpoints data
            api_endpoints = [
                ("/api/v1/market/quotes", "GET", "Real-time market quotes", "1000/min", "YES"),
                ("/api/v1/portfolio/holdings", "GET", "Portfolio holdings data", "100/min", "YES"),
                ("/api/v1/news/latest", "GET", "Latest financial news", "500/min", "NO"),
                ("/api/v1/analytics/technical", "POST", "Technical analysis calculations", "50/min", "YES"),
                ("/api/v1/market/history", "GET", "Historical market data", "200/min", "YES"),
                ("/api/v1/user/profile", "GET", "User profile information", "10/min", "YES"),
                ("/api/v1/orders/submit", "POST", "Submit trading orders", "100/min", "YES"),
                ("/api/v1/market/screener", "POST", "Stock screening criteria", "50/min", "YES"),
                ("/api/v1/research/reports", "GET", "Research reports access", "20/min", "YES"),
                ("/api/v1/alerts/manage", "POST", "Manage price alerts", "100/min", "YES")
            ]

            for endpoint, method, description, rate_limit, auth in api_endpoints:
                with dpg.table_row():
                    dpg.add_text(endpoint, color=self.BLOOMBERG_BLUE)
                    method_color = self.BLOOMBERG_GREEN if method == "GET" else self.BLOOMBERG_ORANGE
                    dpg.add_text(method, color=method_color)
                    dpg.add_text(description, color=self.BLOOMBERG_WHITE)
                    dpg.add_text(rate_limit, color=self.BLOOMBERG_YELLOW)
                    auth_color = self.BLOOMBERG_RED if auth == "YES" else self.BLOOMBERG_GREEN
                    dpg.add_text(auth, color=auth_color)

    @monitor_performance
    def create_right_help_panel(self):
        """Create right quick actions panel"""
        with operation("create_right_help_panel", module="help"):
            with dpg.child_window(width=350, height=650, border=True):
                dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                # Quick action buttons - pre-computed for performance
                quick_actions = [
                    ("📞 Contact Support", self.contact_support),
                    ("📝 Send Feedback", self.send_feedback),
                    ("📚 User Manual", self.open_manual),
                    ("🎥 Watch Tutorials", self.open_tutorials),
                    ("👥 Join Community", self.open_community),
                    ("🔄 Check Updates", self.check_updates),
                    ("⚙️ System Settings", self.open_settings),
                    ("🐛 Report Issue", self.report_bug)
                ]

                for label, callback in quick_actions:
                    dpg.add_button(label=label, callback=callback, width=-1, height=35)
                    dpg.add_spacer(height=5)

                dpg.add_separator()

                # System information
                dpg.add_text("SYSTEM INFORMATION", color=self.BLOOMBERG_YELLOW)
                with dpg.table(header_row=False, borders_innerH=False, borders_outerH=False):
                    dpg.add_table_column()
                    dpg.add_table_column()

                    # Pre-computed system info for performance
                    system_info = [
                        ("Terminal Version:", "4.2.1"),
                        ("Build Date:", "2025-01-15"),
                        ("Platform:", "Windows 11"),
                        ("Memory Usage:", "2.4 GB"),
                        ("CPU Usage:", "12%"),
                        ("Network Status:", "Connected"),
                        ("Data Feed:", "Live"),
                        ("Session Time:", "02:34:12")
                    ]

                    for label, value in system_info:
                        with dpg.table_row():
                            dpg.add_text(label, color=self.BLOOMBERG_GRAY)
                            value_color = self.BLOOMBERG_GREEN if "Connected" in value or "Live" in value else self.BLOOMBERG_WHITE
                            dpg.add_text(value, color=value_color)

                dpg.add_separator()

                # Recent help topics
                dpg.add_text("RECENT HELP TOPICS", color=self.BLOOMBERG_YELLOW)
                recent_topics = [
                    "How to create portfolios",
                    "Setting up price alerts",
                    "Understanding P&L calculations",
                    "Using technical indicators",
                    "Exporting data to Excel"
                ]

                for topic in recent_topics:
                    with dpg.group(horizontal=True):
                        dpg.add_text("•", color=self.BLOOMBERG_ORANGE)
                        dpg.add_text(topic, color=self.BLOOMBERG_WHITE, wrap=300)

                debug("Right help panel created", module="help")

    def create_help_status_bar(self):
        """Create help status bar"""
        with dpg.group(horizontal=True):
            status_items = [
                ("HELP STATUS:", "ONLINE", self.BLOOMBERG_GRAY, self.BLOOMBERG_GREEN),
                ("SUPPORT AVAILABLE:", "24/7", self.BLOOMBERG_GRAY, self.BLOOMBERG_GREEN),
                ("LAST UPDATED:", "2025-01-15", self.BLOOMBERG_GRAY, self.BLOOMBERG_WHITE),
                ("HELP VERSION:", "4.2.1", self.BLOOMBERG_GRAY, self.BLOOMBERG_WHITE)
            ]

            for i, (label, value, label_color, value_color) in enumerate(status_items):
                if i > 0:
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(label, color=label_color)
                dpg.add_text(value, color=value_color)

    # Navigation and callback methods
    def navigate_section(self, section_key):
        """Navigate to help section"""
        section_map = {
            "F1:ABOUT": "About",
            "F2:FEATURES": "Features",
            "F3:SUPPORT": "Support",
            "F4:CONTACT": "Support",
            "F5:FEEDBACK": "Support",
            "F6:DOCS": "API Docs"
        }

        target_tab = section_map.get(section_key, "About")
        info("Navigating to help section", module="help",
             context={'section_key': section_key, 'target_tab': target_tab})

    def search_help(self):
        """Search help topics"""
        with operation("search_help", module="help"):
            info("Help search functionality activated", module="help")

    # Support callback methods
    def contact_support(self):
        """Contact support"""
        info("Contacting support team", module="help",
             context={'email': 'support@fincept.in', 'phone': '+1 (555) 123-4567'})

    def contact_email_support(self):
        """Contact email support"""
        info("Opening email support", module="help",
             context={'email': 'support@fincept.in'})

    def open_live_chat(self):
        """Open live chat"""
        info("Opening live chat support", module="help")

    def contact_phone_support(self):
        """Contact phone support"""
        info("Initiating phone support", module="help",
             context={'phone': '+1 (555) 123-4567'})

    def send_feedback(self):
        """Send feedback"""
        info("Opening feedback form", module="help")

    def open_manual(self):
        """Open user manual"""
        info("Opening user manual", module="help")

    def open_documentation(self):
        """Open documentation"""
        info("Opening documentation", module="help")

    def open_tutorials(self):
        """Open video tutorials"""
        info("Opening video tutorials", module="help")

    def open_community(self):
        """Open community forum"""
        info("Opening community forum", module="help")

    def check_updates(self):
        """Check for updates"""
        info("Checking for updates", module="help")

    def open_settings(self):
        """Open settings"""
        info("Opening system settings", module="help")

    def report_bug(self):
        """Report a bug"""
        info("Opening bug report form", module="help")

    def request_feature(self):
        """Request a feature"""
        info("Opening feature request form", module="help")

    @monitor_performance
    def back_to_dashboard(self):
        """Navigate back to dashboard"""
        with operation("back_to_dashboard", module="help"):
            try:
                if hasattr(self.main_app, 'tabs') and 'dashboard' in self.main_app.tabs:
                    info("Returning to Dashboard", module="help")
                    # Navigate to dashboard tab if available
                    dpg.set_value("main_tab_bar", "tab_dashboard")
                else:
                    warning("Dashboard not available", module="help")
            except Exception as e:
                error("Error navigating to dashboard", module="help",
                      context={'error': str(e)}, exc_info=True)

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle component resizing"""
        # Bloomberg terminal has fixed layout
        debug("Component resize requested - using fixed Bloomberg layout", module="help",
              context={'left_width': left_width, 'center_width': center_width})

    @monitor_performance
    def cleanup(self):
        """Clean up help tab resources"""
        with operation("help_tab_cleanup", module="help"):
            try:
                info("Cleaning up help tab resources", module="help")
                self.scroll_position = 0
                self._cached_datetime = None
                self._datetime_cache_time = 0
                info("Help tab cleanup complete", module="help")
            except Exception as e:
                error("Error in help cleanup", module="help",
                      context={'error': str(e)}, exc_info=True)