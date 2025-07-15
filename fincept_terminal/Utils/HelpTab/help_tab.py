# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import datetime


class HelpTab(BaseTab):
    """Bloomberg Terminal style Help and About tab"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app
        self.scroll_position = 0

        # Bloomberg color scheme
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 150, 250]
        self.BLOOMBERG_BLACK = [0, 0, 0]

    def get_label(self):
        return "❓ Help & About"

    def create_content(self):
        """Create Bloomberg-style help terminal layout"""
        try:
            # Top bar with Bloomberg branding
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("HELP TERMINAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_input_text(label="", default_value="Search Help Topics", width=300)
                dpg.add_button(label="SEARCH", width=80, callback=self.search_help)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

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

        except Exception as e:
            print(f"Error creating help content: {e}")
            # Fallback content
            dpg.add_text("HELP TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Error loading help content. Please try again.")

    def create_left_help_panel(self):
        """Create left help navigation panel"""
        with dpg.child_window(width=350, height=650, border=True):
            dpg.add_text("HELP NAVIGATOR", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Help sections table
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           scrollY=True, height=300):
                dpg.add_table_column(label="Section", width_fixed=True, init_width_or_weight=120)
                dpg.add_table_column(label="Status", width_fixed=True, init_width_or_weight=80)
                dpg.add_table_column(label="Action", width_fixed=True, init_width_or_weight=100)

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

                with dpg.table_row():
                    dpg.add_text("Total Help Topics:")
                    dpg.add_text("47", color=self.BLOOMBERG_WHITE)
                with dpg.table_row():
                    dpg.add_text("Video Tutorials:")
                    dpg.add_text("12", color=self.BLOOMBERG_WHITE)
                with dpg.table_row():
                    dpg.add_text("FAQ Articles:")
                    dpg.add_text("25", color=self.BLOOMBERG_WHITE)
                with dpg.table_row():
                    dpg.add_text("API Endpoints:")
                    dpg.add_text("156", color=self.BLOOMBERG_WHITE)

            dpg.add_separator()

            # System status
            dpg.add_text("SYSTEM STATUS", color=self.BLOOMBERG_YELLOW)
            with dpg.group(horizontal=True):
                dpg.add_text("●", color=self.BLOOMBERG_GREEN)
                dpg.add_text("ALL SYSTEMS OPERATIONAL", color=self.BLOOMBERG_GREEN)

    def create_center_help_panel(self):
        """Create center help content panel"""
        with dpg.child_window(width=900, height=650, border=True):
            with dpg.tab_bar():
                # About Tab
                with dpg.tab(label="About"):
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

                                with dpg.table_row():
                                    dpg.add_text("Version:")
                                    dpg.add_text("4.2.1 Professional", color=self.BLOOMBERG_WHITE)
                                with dpg.table_row():
                                    dpg.add_text("Build:")
                                    dpg.add_text("20250115.1", color=self.BLOOMBERG_WHITE)
                                with dpg.table_row():
                                    dpg.add_text("License:")
                                    dpg.add_text("Enterprise", color=self.BLOOMBERG_GREEN)
                                with dpg.table_row():
                                    dpg.add_text("Data Sources:")
                                    dpg.add_text("Real-time", color=self.BLOOMBERG_GREEN)
                                with dpg.table_row():
                                    dpg.add_text("API Status:")
                                    dpg.add_text("Connected", color=self.BLOOMBERG_GREEN)

                        with dpg.group():
                            dpg.add_text("Core Features", color=self.BLOOMBERG_YELLOW)
                            dpg.add_text("• Real-time market data & analytics")
                            dpg.add_text("• Portfolio management & tracking")
                            dpg.add_text("• Advanced charting & technical analysis")
                            dpg.add_text("• Financial news & sentiment analysis")
                            dpg.add_text("• Risk management tools")
                            dpg.add_text("• Algorithmic trading support")
                            dpg.add_text("• Multi-asset class coverage")
                            dpg.add_text("• Professional-grade security")

                    dpg.add_spacer(height=20)

                    # Description
                    about_text = (
                        "Fincept Terminal is a cutting-edge financial analysis platform designed to provide "
                        "real-time market data, portfolio management, and actionable insights to investors, traders, "
                        "and financial professionals. Our platform integrates advanced analytics, AI-driven sentiment analysis, "
                        "and the latest market trends to help you make well-informed investment decisions."
                    )
                    dpg.add_text(about_text, wrap=850, color=self.BLOOMBERG_WHITE)

                # Features Tab
                with dpg.tab(label="Features"):
                    dpg.add_text("TERMINAL FEATURES & CAPABILITIES", color=self.BLOOMBERG_ORANGE)
                    dpg.add_separator()

                    # Features in Bloomberg table style
                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                                   scrollY=True, height=400):
                        dpg.add_table_column(label="Feature Category", width_fixed=True, init_width_or_weight=200)
                        dpg.add_table_column(label="Description", width_fixed=True, init_width_or_weight=400)
                        dpg.add_table_column(label="Status", width_fixed=True, init_width_or_weight=100)
                        dpg.add_table_column(label="Access Level", width_fixed=True, init_width_or_weight=120)

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

                # Support Tab
                with dpg.tab(label="Support"):
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

                                with dpg.table_row():
                                    dpg.add_text("Email Support:")
                                    dpg.add_text("support@fincept.in", color=self.BLOOMBERG_BLUE)
                                with dpg.table_row():
                                    dpg.add_text("Phone Support:")
                                    dpg.add_text("+1 (555) 123-4567", color=self.BLOOMBERG_WHITE)
                                with dpg.table_row():
                                    dpg.add_text("Live Chat:")
                                    dpg.add_text("Available 24/7", color=self.BLOOMBERG_GREEN)
                                with dpg.table_row():
                                    dpg.add_text("Response Time:")
                                    dpg.add_text("< 2 hours", color=self.BLOOMBERG_GREEN)
                                with dpg.table_row():
                                    dpg.add_text("Support Hours:")
                                    dpg.add_text("24/7/365", color=self.BLOOMBERG_WHITE)

                        with dpg.group():
                            dpg.add_text("Support Channels", color=self.BLOOMBERG_YELLOW)
                            dpg.add_spacer(height=10)

                            support_buttons = [
                                ("📧 Email Support", self.contact_email_support),
                                ("💬 Live Chat", self.open_live_chat),
                                ("📞 Phone Support", self.contact_phone_support),
                                ("📖 Documentation", self.open_documentation),
                                ("🎥 Video Tutorials", self.open_tutorials),
                                ("🌐 Community Forum", self.open_community),
                                ("🐛 Report Bug", self.report_bug),
                                ("💡 Feature Request", self.request_feature)
                            ]

                            for label, callback in support_buttons:
                                dpg.add_button(label=label, callback=callback, width=200)
                                dpg.add_spacer(height=5)

                # API Documentation Tab
                with dpg.tab(label="API Docs"):
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

    def create_right_help_panel(self):
        """Create right quick actions panel"""
        with dpg.child_window(width=350, height=650, border=True):
            dpg.add_text("QUICK ACTIONS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Quick action buttons
            quick_actions = [
                ("📞 Contact Support", self.contact_support),
                ("📧 Send Feedback", self.send_feedback),
                ("📖 User Manual", self.open_manual),
                ("🎥 Watch Tutorials", self.open_tutorials),
                ("💬 Join Community", self.open_community),
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

    def create_help_status_bar(self):
        """Create help status bar"""
        with dpg.group(horizontal=True):
            dpg.add_text("HELP STATUS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ONLINE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SUPPORT AVAILABLE:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("24/7", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LAST UPDATED:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("2025-01-15", color=self.BLOOMBERG_WHITE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("HELP VERSION:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("4.2.1", color=self.BLOOMBERG_WHITE)

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
        print(f"Navigating to help section: {target_tab}")

    def search_help(self):
        """Search help topics"""
        print("🔍 Help search functionality")

    # Support callback methods
    def contact_support(self):
        """Contact support"""
        print("📞 Contacting support team...")
        print("Support: support@fincept.in | Phone: +1 (555) 123-4567")

    def contact_email_support(self):
        """Contact email support"""
        print("📧 Opening email support: support@fincept.in")

    def open_live_chat(self):
        """Open live chat"""
        print("💬 Opening live chat support...")

    def contact_phone_support(self):
        """Contact phone support"""
        print("📞 Phone support: +1 (555) 123-4567")

    def send_feedback(self):
        """Send feedback"""
        print("📧 Opening feedback form...")

    def open_manual(self):
        """Open user manual"""
        print("📖 Opening user manual...")

    def open_documentation(self):
        """Open documentation"""
        print("📖 Opening documentation...")

    def open_tutorials(self):
        """Open video tutorials"""
        print("🎥 Opening video tutorials...")

    def open_community(self):
        """Open community forum"""
        print("💬 Opening community forum...")

    def check_updates(self):
        """Check for updates"""
        print("🔄 Checking for updates...")

    def open_settings(self):
        """Open settings"""
        print("⚙️ Opening system settings...")

    def report_bug(self):
        """Report a bug"""
        print("🐛 Opening bug report form...")

    def request_feature(self):
        """Request a feature"""
        print("💡 Opening feature request form...")

    def back_to_dashboard(self):
        """Navigate back to dashboard"""
        try:
            if hasattr(self.main_app, 'tabs') and 'dashboard' in self.main_app.tabs:
                print("🔄 Returning to Dashboard...")
                # Navigate to dashboard tab if available
                dpg.set_value("main_tab_bar", "tab_dashboard")
            else:
                print("Dashboard not available")
        except Exception as e:
            print(f"Error navigating to dashboard: {e}")

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle component resizing"""
        # Bloomberg terminal has fixed layout
        pass

    def cleanup(self):
        """Clean up help tab resources"""
        try:
            print("🧹 Cleaning up help tab...")
            self.scroll_position = 0
            print("✅ Help tab cleanup complete")
        except Exception as e:
            print(f"Error in help cleanup: {e}")