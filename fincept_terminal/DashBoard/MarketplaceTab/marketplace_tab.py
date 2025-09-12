import dearpygui.dearpygui as dpg
import sys


class FinancialMarketplace:
    """Financial Data Marketplace using Tab-based Navigation"""

    def __init__(self):
        self.SCREEN_WIDTH = 1400
        self.SCREEN_HEIGHT = 900
        self.SIDEBAR_WIDTH = 280
        self.CARD_WIDTH = 350
        self.CARD_HEIGHT = 280

        self.selected_product = None
        self.initialize_data()
        self.setup_ui()

    def initialize_data(self):
        """Initialize marketplace data"""
        self.data_products = {
            "real_time_market": {
                "name": "Real-Time Market Data",
                "provider": "Bloomberg Terminal",
                "category": "Market Data",
                "tier": "Enterprise",
                "price": 2400,
                "subscribers": 285043,
                "rating": 4.9,
                "status": "Active",
                "description": "Comprehensive real-time market data with ultra-low latency feeds covering global exchanges, options, futures, and fixed income instruments. Get Level II data, options chains, and corporate actions in real-time.",
                "features": ["Level II Market Data", "Options Chain", "Futures Data", "FX Rates", "Corporate Actions",
                             "Real-time Analytics"]
            },
            "esg_analytics": {
                "name": "ESG Analytics Platform",
                "provider": "Sustainalytics",
                "category": "ESG Data",
                "tier": "Professional",
                "price": 800,
                "subscribers": 23456,
                "rating": 4.6,
                "status": "Trial Available",
                "description": "Comprehensive ESG scores and sustainability analytics for investment decisions with real-time monitoring, controversy screening, and detailed sustainability reporting.",
                "features": ["ESG Risk Ratings", "Carbon Footprint", "Governance Metrics", "Sustainability Reports",
                             "Controversy Assessment"]
            },
            "crypto_data": {
                "name": "Cryptocurrency Data Feed",
                "provider": "CoinGecko Pro",
                "category": "Digital Assets",
                "tier": "Standard",
                "price": 400,
                "subscribers": 67890,
                "rating": 4.3,
                "status": "Active",
                "description": "Comprehensive cryptocurrency and DeFi market data covering major exchanges with real-time prices, trading volumes, market capitalization, and advanced DeFi analytics.",
                "features": ["Token Prices", "Trading Volume", "Market Cap", "DeFi TVL", "Staking Rewards", "NFT Data"]
            },
            "news_sentiment": {
                "name": "News & Sentiment Analytics",
                "provider": "Refinitiv NewsScope",
                "category": "News Data",
                "tier": "Professional",
                "price": 1500,
                "subscribers": 34567,
                "rating": 4.8,
                "status": "Trial Available",
                "description": "Real-time news analytics with sentiment scoring and market-moving event detection powered by advanced NLP. Process millions of articles daily with precise entity recognition.",
                "features": ["News Feed", "Sentiment Analysis", "Event Detection", "Entity Recognition",
                             "Market Impact Scoring"]
            },
            "options_analytics": {
                "name": "Options Analytics Suite",
                "provider": "OPRA Data Feed",
                "category": "Derivatives",
                "tier": "Enterprise",
                "price": 1800,
                "subscribers": 12345,
                "rating": 4.7,
                "status": "Active",
                "description": "Professional options market data and analytics with real-time Greeks, volatility surfaces, and unusual activity detection. Complete options flow analysis.",
                "features": ["Options Chain", "Greeks Calculation", "Volatility Surface", "Unusual Activity",
                             "Flow Analysis"]
            },
            "forex_data": {
                "name": "FX Market Data Pro",
                "provider": "FXAll Premium",
                "category": "Foreign Exchange",
                "tier": "Standard",
                "price": 600,
                "subscribers": 41230,
                "rating": 4.4,
                "status": "Active",
                "description": "Professional foreign exchange market data with institutional-grade execution prices, forward curves, and options volatility. Access major, minor, and exotic currency pairs.",
                "features": ["Spot Rates", "Forward Curves", "Options Volatility", "Cross Rates", "Central Bank Data"]
            }
        }

    def setup_ui(self):
        """Initialize UI with tab-based navigation"""
        try:
            dpg.create_context()
            self.create_bloomberg_theme()

            dpg.create_viewport(
                title="Fincept Data Marketplace",
                width=self.SCREEN_WIDTH,
                height=self.SCREEN_HEIGHT,
                resizable=True
            )

            dpg.setup_dearpygui()

            with dpg.window(
                    label="Financial Marketplace",
                    tag="main_window",
                    width=self.SCREEN_WIDTH,
                    height=self.SCREEN_HEIGHT,
                    no_title_bar=True,
                    no_resize=True,
                    no_move=True,
                    pos=[0, 0]
            ):
                self.create_complete_layout()

            dpg.bind_theme("bloomberg_theme")
            dpg.set_primary_window("main_window", True)
            dpg.show_viewport()

            print("Starting DearPyGUI...")
            dpg.start_dearpygui()

        except Exception as e:
            print(f"Error in setup_ui: {e}")
            sys.exit(1)
        finally:
            try:
                dpg.destroy_context()
            except:
                pass

    def create_bloomberg_theme(self):
        """Create Bloomberg Terminal theme"""
        try:
            with dpg.theme(tag="bloomberg_theme"):
                with dpg.theme_component(dpg.mvAll):
                    # Bloomberg colors
                    BLOOMBERG_BLACK = [0, 0, 0, 255]
                    BLOOMBERG_DARK = [25, 25, 25, 255]
                    BLOOMBERG_MEDIUM = [45, 45, 45, 255]
                    BLOOMBERG_ORANGE = [255, 140, 0, 255]
                    BLOOMBERG_WHITE = [255, 255, 255, 255]
                    BLOOMBERG_GRAY = [180, 180, 180, 255]

                    # Colors
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, BLOOMBERG_BLACK)
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, BLOOMBERG_BLACK)
                    dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_WHITE)
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, BLOOMBERG_GRAY)
                    dpg.add_theme_color(dpg.mvThemeCol_Button, BLOOMBERG_MEDIUM)
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [255, 140, 0, 120])
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [255, 140, 0, 180])
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, BLOOMBERG_DARK)
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, [255, 140, 0, 60])
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, [255, 140, 0, 100])
                    dpg.add_theme_color(dpg.mvThemeCol_Header, [255, 140, 0, 150])
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, [255, 140, 0, 180])
                    dpg.add_theme_color(dpg.mvThemeCol_Border, [80, 80, 80, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_Separator, [100, 100, 100, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_CheckMark, BLOOMBERG_ORANGE)
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, BLOOMBERG_MEDIUM)
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, [255, 140, 0, 120])
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, [255, 140, 0, 180])

                    # Styling
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 0)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 2)
                    dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 10, 10)
                    dpg.add_theme_style(dpg.mvStyleVar_FramePadding, 8, 6)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 8, 6)

            print("Bloomberg theme created successfully")
        except Exception as e:
            print(f"Error creating theme: {e}")

    def create_complete_layout(self):
        """Create complete layout with tab system"""
        # Header - single line
        with dpg.child_window(width=-1, height=60, border=True, no_scrollbar=True):
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=[255, 140, 0, 255])
                dpg.add_text(" Data Marketplace", color=[255, 255, 255, 255])
                dpg.add_text(" | Search:", color=[180, 180, 180, 255])
                dpg.add_input_text(hint="products...", width=250, height=25)
                dpg.add_button(label="Search", width=60, height=25)
                dpg.add_text(" | 6 Products | 557K+ Users", color=[180, 180, 180, 255])
                dpg.add_button(label="Account", width=70, height=25)

        # Tab system for navigation
        with dpg.tab_bar(tag="main_tabs"):
            # Marketplace Tab
            with dpg.tab(label="Marketplace", tag="marketplace_tab"):
                with dpg.group(horizontal=True):
                    # Sidebar
                    with dpg.child_window(width=self.SIDEBAR_WIDTH, height=self.SCREEN_HEIGHT - 120, border=True):
                        dpg.add_text("FILTERS", color=[255, 140, 0, 255])
                        dpg.add_separator()

                        dpg.add_text("Categories")
                        categories = ["All Categories", "Market Data", "ESG Data", "Digital Assets", "News Data",
                                      "Derivatives"]
                        for i, category in enumerate(categories):
                            dpg.add_checkbox(label=category, default_value=(i == 0))

                        dpg.add_text("Price Range")
                        prices = ["$0-500", "$500-1K", "$1K-2K", "$2K+"]
                        for price in prices:
                            dpg.add_checkbox(label=price)

                        dpg.add_text("Tier")
                        tiers = ["Standard", "Professional", "Enterprise"]
                        for tier in tiers:
                            dpg.add_checkbox(label=tier)

                        dpg.add_button(label="Apply Filters", width=240, height=35)
                        dpg.add_button(label="Clear All", width=240, height=30)

                    # Product grid
                    with dpg.child_window(width=-1, height=self.SCREEN_HEIGHT - 120, border=True):
                        self.create_marketplace_content()

            # Product Details Tab (initially hidden)
            with dpg.tab(label="Product Details", tag="details_tab", show=False):
                self.create_product_details_content()

    def create_marketplace_content(self):
        """Create marketplace grid content"""
        dpg.add_text("Data Products (6 items)", color=[255, 255, 255, 255])
        dpg.add_separator()

        products_list = list(self.data_products.items())

        # First row
        with dpg.group(horizontal=True):
            for i in range(3):
                if i < len(products_list):
                    product_id, product_data = products_list[i]
                    self.create_product_card(product_id, product_data)

        # Second row
        with dpg.group(horizontal=True):
            for i in range(3, 6):
                if i < len(products_list):
                    product_id, product_data = products_list[i]
                    self.create_product_card(product_id, product_data)

    def create_product_card(self, product_id: str, product_data: dict):
        """Create product card"""
        with dpg.child_window(width=self.CARD_WIDTH, height=self.CARD_HEIGHT, border=True):
            # Header with icon
            with dpg.group(horizontal=True):
                icons = {"market": "📊", "esg": "🌱", "crypto": "₿", "news": "📰", "options": "📈"}
                icon = "💱"
                for key, ico in icons.items():
                    if key in product_data["name"].lower():
                        icon = ico
                        break

                dpg.add_text(icon, color=[255, 140, 0, 255])

                with dpg.group():
                    name = product_data["name"]
                    if len(name) > 22:
                        name = name[:19] + "..."
                    dpg.add_text(name, color=[255, 255, 255, 255])
                    dpg.add_text(f"by {product_data['provider']}", color=[180, 180, 180, 255])

            # Category and tier
            with dpg.group(horizontal=True):
                dpg.add_button(label=product_data["category"], width=120, height=22, enabled=False)
                dpg.add_button(label=product_data["tier"], width=90, height=22, enabled=False)

            # Description
            description = product_data["description"]
            if len(description) > 100:
                description = description[:97] + "..."
            dpg.add_text(description, color=[180, 180, 180, 255], wrap=320)

            # Rating and subscribers
            with dpg.group(horizontal=True):
                stars = "★" * int(product_data["rating"])
                dpg.add_text(f"{stars} {product_data['rating']}", color=[255, 215, 0, 255])

                subscribers = product_data['subscribers']
                sub_text = f"{subscribers // 1000}K users" if subscribers >= 1000 else f"{subscribers} users"
                dpg.add_text(sub_text, color=[0, 255, 100, 255])

            # Price and actions
            with dpg.group(horizontal=True):
                with dpg.group():
                    dpg.add_text("From", color=[180, 180, 180, 255])
                    dpg.add_text(f"${product_data['price']:,}/mo", color=[255, 140, 0, 255])

                with dpg.group():
                    dpg.add_button(label="View Details", width=100, height=30,
                                   user_data=product_id, callback=self.show_product_details)

                    if product_data["status"] == "Trial Available":
                        dpg.add_button(label="Start Trial", width=100, height=25)
                    else:
                        dpg.add_button(label="Subscribe", width=100, height=25)

    def create_product_details_content(self):
        """Create product details content (placeholder initially)"""
        dpg.add_text("Select a product from the marketplace to view details", color=[180, 180, 180, 255])

    def show_product_details(self, sender, app_data, user_data):
        """Show product details in details tab"""
        try:
            self.selected_product = user_data
            print(f"Showing details for: {user_data}")

            # Clear and rebuild details tab content
            dpg.delete_item("details_tab", children_only=True)

            if self.selected_product and self.selected_product in self.data_products:
                product = self.data_products[self.selected_product]

                # Back button
                dpg.add_button(label="← Back to Marketplace", width=200, height=40,
                               callback=self.show_marketplace, parent="details_tab")

                # Product title
                dpg.add_text(product["name"], color=[255, 255, 255, 255], parent="details_tab")
                dpg.add_text(f"by {product['provider']} | {product['category']} | {product['tier']} Tier",
                             color=[180, 180, 180, 255], parent="details_tab")
                dpg.add_separator(parent="details_tab")

                # Two-column layout
                with dpg.group(horizontal=True, parent="details_tab"):
                    # Left column
                    with dpg.child_window(width=650, height=500, border=True):
                        dpg.add_text("Product Overview", color=[255, 140, 0, 255])
                        dpg.add_text(product["description"], color=[255, 255, 255, 255], wrap=620)

                        dpg.add_text("Key Features:", color=[255, 140, 0, 255])
                        for feature in product["features"]:
                            dpg.add_text(f"• {feature}", color=[180, 180, 180, 255])

                        dpg.add_text("Specifications:", color=[255, 140, 0, 255])
                        dpg.add_text(f"Rating: {product['rating']}/5.0", color=[180, 180, 180, 255])
                        dpg.add_text(f"Subscribers: {product['subscribers']:,}", color=[180, 180, 180, 255])
                        dpg.add_text(f"Status: {product['status']}", color=[0, 255, 100, 255])

                    # Right column
                    with dpg.child_window(width=400, height=500, border=True):
                        dpg.add_text("Pricing & Subscription", color=[255, 140, 0, 255])

                        # Monthly plan
                        with dpg.child_window(width=360, height=100, border=True):
                            dpg.add_text("Monthly Plan", color=[255, 255, 255, 255])
                            dpg.add_text(f"${product['price']:,} per month", color=[255, 140, 0, 255])
                            dpg.add_text("• Monthly billing", color=[180, 180, 180, 255])
                            dpg.add_text("• Full API access", color=[180, 180, 180, 255])

                        # Annual plan
                        annual_price = int(product['price'] * 10)
                        with dpg.child_window(width=360, height=120, border=True):
                            with dpg.group(horizontal=True):
                                dpg.add_text("Annual Plan", color=[0, 255, 100, 255])
                                dpg.add_text("(Save 17%)", color=[0, 255, 100, 255])
                            dpg.add_text(f"${annual_price:,} per year", color=[255, 140, 0, 255])
                            dpg.add_text("• Priority support", color=[180, 180, 180, 255])
                            dpg.add_text("• Account manager", color=[180, 180, 180, 255])

                        # Action buttons
                        if product["status"] == "Trial Available":
                            dpg.add_button(label="Start Free Trial", width=360, height=40)

                        dpg.add_button(label="Subscribe Monthly", width=360, height=35)
                        dpg.add_button(label="Subscribe Annually", width=360, height=35)

            # Show details tab and hide marketplace tab
            dpg.configure_item("details_tab", show=True)
            dpg.set_value("main_tabs", "details_tab")

        except Exception as e:
            print(f"Error in show_product_details: {e}")

    def show_marketplace(self, sender=None, app_data=None, user_data=None):
        """Switch back to marketplace tab"""
        try:
            print("Switching to marketplace")
            dpg.set_value("main_tabs", "marketplace_tab")
        except Exception as e:
            print(f"Error in show_marketplace: {e}")


if __name__ == "__main__":
    try:
        print("Initializing Financial Marketplace...")
        marketplace = FinancialMarketplace()
    except Exception as e:
        print(f"Fatal error: {e}")
        sys.exit(1)