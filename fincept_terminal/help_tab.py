import dearpygui.dearpygui as dpg
from base_tab import BaseTab


class HelpTab(BaseTab):
    """Help and About Us tab with scrollable content"""

    def __init__(self, app):
        super().__init__(app)
        self.scroll_position = 0

    def get_label(self):
        return "Help & About"

    def create_content(self):
        """Create help and about content with vertical scrolling"""
        # Create main scrollable container
        with dpg.child_window(
                tag="help_main_container",
                width=-1,
                height=-1,
                horizontal_scrollbar=False,
                border=True
        ):
            # Header
            self.create_header()
            dpg.add_spacer(height=20)

            # Main content sections
            self.create_about_section()
            dpg.add_spacer(height=25)

            self.create_features_section()
            dpg.add_spacer(height=25)

            self.create_assistance_section()
            dpg.add_spacer(height=25)

            self.create_feedback_section()
            dpg.add_spacer(height=25)

            self.create_footer()
            dpg.add_spacer(height=30)

            # Back button
            self.create_back_button()

    def create_header(self):
        """Create the header section"""
        # Title with styling
        dpg.add_text("Help & About Us", color=[255, 215, 0])  # Gold color for header
        dpg.add_separator()
        dpg.add_spacer(height=10)

        # Subtitle
        dpg.add_text(
            "Empowering Your Financial Journey with Precision & Insight",
            color=[200, 200, 255],
            wrap=self.app.usable_width - 40
        )

    def create_about_section(self):
        """Create About Fincept Terminal section"""
        # Section header with emoji
        dpg.add_text("📌 About Fincept Terminal", color=[100, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=8)

        # Content text
        about_text = (
            "Fincept Terminal is a cutting-edge financial analysis platform designed to provide "
            "real-time market data, portfolio management, and actionable insights to investors, traders, "
            "and financial professionals. Our platform integrates advanced analytics, AI-driven sentiment analysis, "
            "and the latest market trends to help you make well-informed decisions."
        )

        dpg.add_text(about_text, wrap=self.app.usable_width - 40, color=[220, 220, 220])

    def create_features_section(self):
        """Create Features & Capabilities section"""
        # Section header
        dpg.add_text("🔹 Features & Capabilities", color=[100, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=8)

        # Market Tracking subsection
        dpg.add_text("📈 Market Tracking & Real-Time Data", color=[255, 200, 100])
        dpg.add_text(
            "• Access real-time stock prices, indices, bonds, and currency exchange rates.",
            wrap=self.app.usable_width - 60,
            indent=20
        )
        dpg.add_text(
            "• Stay ahead with live updates on market movements and trends.",
            wrap=self.app.usable_width - 60,
            indent=20
        )
        dpg.add_spacer(height=10)

        # Portfolio Management subsection
        dpg.add_text("💼 Portfolio Management", color=[255, 200, 100])
        dpg.add_text(
            "• Track, analyze, and optimize your investment portfolio.",
            wrap=self.app.usable_width - 60,
            indent=20
        )
        dpg.add_text(
            "• View sector-wise and industry-wise allocations for informed diversification.",
            wrap=self.app.usable_width - 60,
            indent=20
        )
        dpg.add_spacer(height=10)

        # Advanced Analytics subsection
        dpg.add_text("📊 Advanced Financial Analytics", color=[255, 200, 100])
        dpg.add_text(
            "• Perform in-depth technical & fundamental analysis.",
            wrap=self.app.usable_width - 60,
            indent=20
        )
        dpg.add_text(
            "• Backtest strategies and simulate different investment scenarios.",
            wrap=self.app.usable_width - 60,
            indent=20
        )
        dpg.add_spacer(height=10)

        # News & Sentiment subsection
        dpg.add_text("📰 Financial News & Sentiment Analysis", color=[255, 200, 100])
        dpg.add_text(
            "• Get curated financial news from global sources.",
            wrap=self.app.usable_width - 60,
            indent=20
        )
        dpg.add_text(
            "• Analyze public sentiment around stocks, industries, and market events.",
            wrap=self.app.usable_width - 60,
            indent=20
        )

    def create_assistance_section(self):
        """Create Need Assistance section"""
        # Section header
        dpg.add_text("📢 Need Assistance? We're Here to Help!", color=[100, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=8)

        # Introduction text
        intro_text = (
            "Whether you're a beginner exploring the markets or a seasoned investor optimizing your strategy, "
            "Fincept Terminal provides robust support to ensure you maximize your financial potential."
        )
        dpg.add_text(intro_text, wrap=self.app.usable_width - 40, color=[220, 220, 220])
        dpg.add_spacer(height=10)

        # Support options with interactive buttons
        with dpg.group(horizontal=True):
            dpg.add_text("📖", color=[255, 255, 100])
            dpg.add_button(
                label="Help & Documentation",
                callback=self.open_documentation,
                width=200
            )

        dpg.add_spacer(height=5)

        with dpg.group(horizontal=True):
            dpg.add_text("📧", color=[255, 255, 100])
            dpg.add_button(
                label="Contact Support (support@fincept.in)",
                callback=self.contact_support,
                width=300
            )

        dpg.add_spacer(height=5)

        with dpg.group(horizontal=True):
            dpg.add_text("💬", color=[255, 255, 100])
            dpg.add_button(
                label="Community Forum",
                callback=self.open_community,
                width=200
            )

    def create_feedback_section(self):
        """Create Suggestions & Feedback section"""
        # Section header
        dpg.add_text("💡 Suggestions & Feedback", color=[100, 255, 100])
        dpg.add_separator()
        dpg.add_spacer(height=8)

        # Content text
        feedback_text = (
            "Your feedback is valuable to us! We continuously work on improving Fincept Terminal to enhance "
            "your experience. If you have suggestions or feature requests, please let us know at:"
        )
        dpg.add_text(feedback_text, wrap=self.app.usable_width - 40, color=[220, 220, 220])
        dpg.add_spacer(height=10)

        # Feedback options
        with dpg.group(horizontal=True):
            dpg.add_text("📩", color=[255, 255, 100])
            dpg.add_button(
                label="support@fincept.in",
                callback=self.send_feedback,
                width=200
            )

        dpg.add_spacer(height=5)

        with dpg.group(horizontal=True):
            dpg.add_text("📣", color=[255, 255, 100])
            dpg.add_button(
                label="Community Polls & Surveys",
                callback=self.open_surveys,
                width=250
            )

    def create_footer(self):
        """Create footer section"""
        dpg.add_separator()
        dpg.add_spacer(height=10)

        # Footer message with styling
        footer_text = "Stay Informed. Stay Ahead. Invest Smarter with Fincept Terminal. 🚀"
        dpg.add_text(
            footer_text,
            color=[255, 215, 0],  # Gold color
            wrap=self.app.usable_width - 40
        )

    def create_back_button(self):
        """Create back button at the bottom"""
        with dpg.group(horizontal=True):
            dpg.add_spacer(width=10)
            dpg.add_button(
                label="← Back to Dashboard",
                callback=self.back_to_dashboard,
                width=200,
                height=35
            )

    # Callback methods
    def open_documentation(self):
        """Open documentation"""
        print("📖 Opening help documentation...")
        # In a real implementation, this might open a web browser or another tab
        # self.app.goto_documentation_tab() or similar

    def contact_support(self):
        """Contact support"""
        print("📧 Opening support contact...")
        print("Support Email: support@fincept.in")
        # In a real implementation, this might open email client or support form

    def open_community(self):
        """Open community forum"""
        print("💬 Opening community forum...")
        # In a real implementation, this might open web browser to forum

    def send_feedback(self):
        """Send feedback"""
        print("📩 Opening feedback form...")
        print("Feedback Email: support@fincept.in")
        # In a real implementation, this might open feedback form or email client

    def open_surveys(self):
        """Open surveys"""
        print("📣 Opening community polls and surveys...")
        # In a real implementation, this might open web browser to surveys

    def back_to_dashboard(self):
        """Navigate back to dashboard"""
        print("🔄 Returning to Dashboard...")
        try:
            # Navigate to dashboard tab
            dpg.set_value("main_tab_bar", "tab_dashboard")
        except Exception as e:
            print(f"Error navigating to dashboard: {e}")

    def resize_components(self, left_width, center_width, right_width,
                          top_height, bottom_height, cell_height):
        """Handle component resizing"""
        try:
            # Update main container size if it exists
            if dpg.does_item_exist("help_main_container"):
                # The child window will automatically resize with parent
                pass
        except Exception as e:
            print(f"Error resizing help components: {e}")

    def cleanup(self):
        """Clean up help tab resources"""
        try:
            # Clean up any resources if needed
            self.scroll_position = 0
            print("✅ Help tab cleaned up")
        except Exception as e:
            print(f"❌ Help tab cleanup error: {e}")