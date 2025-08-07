"""
Geo module for Fincept Terminal
Updated to use centralized logging system
"""

# geopolitical_analysis.py - Comprehensive Bloomberg Terminal Geopolitical Analysis Dashboard
# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
import random
import datetime
import math
import json
from fincept_terminal.Utils.base_tab import BaseTab

from fincept_terminal.Utils.Logging.logger import logger

class GeopoliticalAnalysisTab(BaseTab):
    """Comprehensive Geopolitical Analysis Dashboard for Financial Markets"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app

        # Bloomberg Terminal Colors
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 80, 80]
        self.BLOOMBERG_GREEN = [0, 255, 100]
        self.BLOOMBERG_YELLOW = [255, 255, 100]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 180, 255]
        self.BLOOMBERG_PURPLE = [200, 100, 255]
        self.BLOOMBERG_CYAN = [0, 255, 255]
        self.BLOOMBERG_PINK = [255, 100, 200]

        # Risk level colors
        self.RISK_CRITICAL = [255, 0, 0]  # Red
        self.RISK_HIGH = [255, 100, 0]  # Orange-Red
        self.RISK_MEDIUM = [255, 200, 0]  # Yellow-Orange
        self.RISK_LOW = [100, 255, 100]  # Light Green
        self.RISK_STABLE = [0, 200, 0]  # Green

        self.initialize_comprehensive_data()

    def get_label(self):
        return "GEOPOLITICAL ANALYSIS"

    def initialize_comprehensive_data(self):
        """Initialize comprehensive geopolitical analysis data"""

        # Expanded countries database with detailed metrics
        self.countries = {
            "United States": {
                "region": "North America", "gdp_trillion": 26.9, "military_budget_billion": 816,
                "political_stability": 7.2, "economic_freedom": 7.8, "current_risk": "Low",
                "market_influence": 9.5, "currency": "USD", "population_million": 331,
                "debt_to_gdp": 127.1, "democracy_index": 7.85, "corruption_index": 67,
                "trade_freedom": 8.2, "innovation_index": 9.1, "cyber_security": 8.4,
                "key_allies": ["UK", "Japan", "Germany", "France", "Australia", "Canada"],
                "tensions": ["China", "Russia", "Iran", "North Korea"],
                "recent_events": ["Fed Policy Changes", "Tech Regulations", "Infrastructure Spending",
                                  "Climate Initiatives"],
                "military_bases": 800, "nuclear_warheads": 5428, "aircraft_carriers": 11
            },
            "China": {
                "region": "Asia Pacific", "gdp_trillion": 17.7, "military_budget_billion": 293,
                "political_stability": 6.8, "economic_freedom": 5.7, "current_risk": "Medium",
                "market_influence": 8.9, "currency": "CNY", "population_million": 1412,
                "debt_to_gdp": 77.1, "democracy_index": 2.21, "corruption_index": 45,
                "trade_freedom": 6.8, "innovation_index": 7.4, "cyber_security": 7.8,
                "key_allies": ["Russia", "Pakistan", "Iran", "North Korea", "Cambodia"],
                "tensions": ["United States", "India", "Taiwan", "Japan", "Philippines"],
                "recent_events": ["Zero-COVID Policy End", "Real Estate Crisis", "Taiwan Tensions", "Belt and Road"],
                "military_bases": 3, "nuclear_warheads": 350, "aircraft_carriers": 3
            },
            "Russia": {
                "region": "Europe/Asia", "gdp_trillion": 2.1, "military_budget_billion": 154,
                "political_stability": 4.2, "economic_freedom": 4.1, "current_risk": "High",
                "market_influence": 6.5, "currency": "RUB", "population_million": 146,
                "debt_to_gdp": 17.8, "democracy_index": 3.24, "corruption_index": 30,
                "trade_freedom": 4.5, "innovation_index": 5.2, "cyber_security": 6.9,
                "key_allies": ["China", "Iran", "North Korea", "Belarus", "Syria"],
                "tensions": ["United States", "NATO", "Ukraine", "EU"],
                "recent_events": ["Ukraine Conflict", "Energy Sanctions", "BRICS Expansion", "Nuclear Threats"],
                "military_bases": 21, "nuclear_warheads": 5977, "aircraft_carriers": 1
            },
            "European Union": {
                "region": "Europe", "gdp_trillion": 23.0, "military_budget_billion": 270,
                "political_stability": 7.5, "economic_freedom": 7.3, "current_risk": "Medium",
                "market_influence": 8.2, "currency": "EUR", "population_million": 448,
                "debt_to_gdp": 95.6, "democracy_index": 8.12, "corruption_index": 64,
                "trade_freedom": 7.8, "innovation_index": 7.9, "cyber_security": 7.6,
                "key_allies": ["United States", "NATO", "UK", "Japan"],
                "tensions": ["Russia", "Migration Issues", "Energy Security"],
                "recent_events": ["Energy Crisis", "ECB Policy", "Green Deal", "Defense Integration"],
                "military_bases": 45, "nuclear_warheads": 540, "aircraft_carriers": 4
            }
        }

        # Comprehensive global events database
        self.global_events = [
            {
                "event": "US-China Strategic Competition", "impact_level": "Critical", "probability": 95,
                "market_sectors": ["Technology", "Manufacturing", "Semiconductors", "Defense"],
                "timeframe": "Ongoing", "market_impact": -3.2, "volatility_impact": 2.8,
                "affected_currencies": ["USD", "CNY", "JPY", "KRW"], "commodity_impact": ["Rare Earths", "Oil"],
                "escalation_probability": 35, "resolution_timeline": "2+ years"
            },
            {
                "event": "Russia-Ukraine Conflict", "impact_level": "Critical", "probability": 98,
                "market_sectors": ["Energy", "Agriculture", "Defense", "Shipping"],
                "timeframe": "Ongoing", "market_impact": -4.7, "volatility_impact": 3.9,
                "affected_currencies": ["RUB", "UAH", "EUR", "USD"],
                "commodity_impact": ["Natural Gas", "Wheat", "Fertilizers"],
                "escalation_probability": 25, "resolution_timeline": "1-2 years"
            },
            {
                "event": "Middle East Regional Tensions", "impact_level": "High", "probability": 75,
                "market_sectors": ["Energy", "Shipping", "Insurance", "Defense"],
                "timeframe": "3-6 months", "market_impact": -2.1, "volatility_impact": 2.4,
                "affected_currencies": ["USD", "EUR", "GBP"], "commodity_impact": ["Oil", "Gold"],
                "escalation_probability": 45, "resolution_timeline": "6-12 months"
            },
            {
                "event": "Taiwan Strait Crisis", "impact_level": "Critical", "probability": 30,
                "market_sectors": ["Semiconductors", "Technology", "Shipping", "Manufacturing"],
                "timeframe": "6-18 months", "market_impact": -12.3, "volatility_impact": 8.7,
                "affected_currencies": ["TWD", "CNY", "USD", "JPY"],
                "commodity_impact": ["Semiconductors", "Rare Metals"],
                "escalation_probability": 15, "resolution_timeline": "Unknown"
            }
        ]

        # Advanced economic indicators
        self.economic_indicators = {
            "Global Trade Volume": {"current": 28.7, "change": -2.3, "trend": "DOWN", "impact": "High"},
            "Oil Price (WTI)": {"current": 78.45, "change": 1.8, "trend": "UP", "impact": "Critical"},
            "Gold Price": {"current": 1967.80, "change": 12.40, "trend": "UP", "impact": "Medium"},
            "VIX (Fear Index)": {"current": 18.6, "change": 2.1, "trend": "UP", "impact": "High"},
            "Dollar Index (DXY)": {"current": 103.2, "change": -0.8, "trend": "DOWN", "impact": "Critical"},
            "Baltic Dry Index": {"current": 1423, "change": -45, "trend": "DOWN", "impact": "Medium"},
            "Copper Prices": {"current": 8245, "change": -123, "trend": "DOWN", "impact": "Medium"},
            "Bond Yields (10Y)": {"current": 4.35, "change": 0.05, "trend": "UP", "impact": "High"}
        }

        # Comprehensive sanctions database
        self.sanctions_data = {
            "Active Sanctions": 2847, "Countries Affected": 47, "Economic Impact ($B)": 1247,
            "Trade Routes Disrupted": 23, "Financial Institutions": 356, "Individuals": 12847,
            "Major Targets": ["Russia", "Iran", "North Korea", "Myanmar", "Syria", "Venezuela"],
            "Recent Changes": [
                "Russia energy sector - New restrictions on oil price cap",
                "China technology - Semiconductor equipment export controls",
                "Iran banking - Additional financial institution sanctions",
                "North Korea - Cyber warfare unit designations",
                "Myanmar - Military leadership asset freezes"
            ],
            "Effectiveness Rating": {
                "Russia": {"rating": 7.2, "compliance": "Medium", "evasion": "High"},
                "Iran": {"rating": 6.8, "compliance": "Low", "evasion": "High"},
                "North Korea": {"rating": 8.1, "compliance": "Very Low", "evasion": "Critical"}
            }
        }

        # Advanced market correlation matrix
        self.correlation_matrix = {
            "Energy Stocks": {
                "Russia Conflict": 0.87, "Middle East Tensions": 0.74, "Sanctions": 0.69,
                "Oil Prices": 0.91, "Gas Prices": 0.83, "Geopolitical Risk": 0.78
            },
            "Technology Stocks": {
                "US-China Relations": 0.82, "Taiwan Crisis": 0.94, "Semiconductor Sanctions": 0.88,
                "Supply Chain Disruption": 0.76, "Cyber Threats": 0.58, "Trade Wars": 0.71
            },
            "Defense Stocks": {
                "Military Conflicts": 0.92, "NATO Spending": 0.79, "Arms Sales": 0.84,
                "Regional Tensions": 0.67, "Threat Levels": 0.73, "Alliance Changes": 0.56
            },
            "Currency Markets": {
                "Political Stability": -0.76, "Trade Wars": -0.68, "Central Bank Policy": 0.84,
                "Safe Haven Flows": 0.72, "Economic Sanctions": -0.59, "Conflict Zones": -0.63
            },
            "Commodity Markets": {
                "Supply Disruptions": 0.89, "Trade Route Security": 0.71, "Sanctions Impact": 0.66,
                "Weather Events": 0.54, "Strategic Reserves": -0.62, "Alternative Sources": -0.48
            },
            "Emerging Markets": {
                "Regional Stability": 0.78, "Capital Flows": 0.69, "Commodity Dependence": 0.58,
                "Currency Crisis": -0.81, "Political Risk": -0.74, "External Debt": -0.67
            }
        }

        # Advanced risk assessment model
        self.risk_factors = {
            "Military Conflicts": {"weight": 0.25, "current": 8.3, "trend": "UP", "volatility": 2.1},
            "Economic Sanctions": {"weight": 0.20, "current": 7.1, "trend": "UP", "volatility": 1.8},
            "Political Instability": {"weight": 0.18, "current": 6.8, "trend": "FLAT", "volatility": 1.5},
            "Trade Disputes": {"weight": 0.15, "current": 6.2, "trend": "DOWN", "volatility": 1.2},
            "Cyber Threats": {"weight": 0.12, "current": 7.4, "trend": "UP", "volatility": 2.3},
            "Climate Events": {"weight": 0.10, "current": 5.9, "trend": "UP", "volatility": 1.7}
        }

        # Intelligence feeds and alerts
        self.intelligence_feeds = [
            {"time": "15:45", "priority": "CRITICAL", "source": "SIGINT", "classification": "TOP SECRET",
             "event": "Unusual military movements detected near contested border regions"},
            {"time": "15:32", "priority": "HIGH", "source": "HUMINT", "classification": "SECRET",
             "event": "High-level diplomatic communications intercept indicates policy shift"},
            {"time": "15:18", "priority": "MEDIUM", "source": "OSINT", "classification": "CONFIDENTIAL",
             "event": "Social media sentiment analysis shows rising nationalism"},
            {"time": "15:04", "priority": "HIGH", "source": "ECONINT", "classification": "SECRET",
             "event": "Significant capital flows detected from emerging markets"},
            {"time": "14:51", "priority": "CRITICAL", "source": "CYBINT", "classification": "TOP SECRET",
             "event": "State-sponsored cyber activity targeting financial infrastructure"}
        ]

        # Trade route analysis
        self.trade_routes = {
            "Strait of Hormuz": {"status": "HIGH RISK", "daily_oil_mbpd": 21.0, "threat_level": 8.5},
            "Suez Canal": {"status": "MEDIUM RISK", "daily_trade_billions": 9.6, "threat_level": 6.2},
            "Strait of Malacca": {"status": "MEDIUM RISK", "daily_trade_billions": 3.4, "threat_level": 5.8},
            "South China Sea": {"status": "HIGH RISK", "daily_trade_billions": 5.4, "threat_level": 7.9},
            "Bosphorus Strait": {"status": "MEDIUM RISK", "daily_trade_billions": 1.2, "threat_level": 6.0},
            "Panama Canal": {"status": "LOW RISK", "daily_trade_billions": 1.8, "threat_level": 3.1}
        }

    def create_content(self):
        """Create the comprehensive geopolitical analysis dashboard"""
        try:
            # Enhanced header with comprehensive metrics
            self.create_comprehensive_header()

            # Enhanced function keys
            self.create_advanced_function_keys()

            # Bottom section - Advanced analytics
            with dpg.group(horizontal=True):
                self.create_predictive_analytics_panel()
                self.create_sanctions_monitoring_panel()
                self.create_trade_route_analysis_panel()

            dpg.add_separator()

            # Final section - Intelligence dashboard
            self.create_intelligence_dashboard()

            # Main analysis section
            with dpg.group(horizontal=True):
                # Left column - Risk assessment and country analysis
                self.create_comprehensive_risk_panel()

                # Center column - Event analysis and intelligence
                self.create_comprehensive_event_panel()

                # Right column - Market correlations and predictions
                self.create_comprehensive_market_panel()

            dpg.add_separator()

            # Top section - Critical metrics and alerts
            with dpg.group(horizontal=True):
                self.create_global_threat_matrix()
                self.create_real_time_alerts_panel()
                self.create_market_impact_summary()

            dpg.add_separator()

        except Exception as e:
            logger.error(f"Error creating comprehensive geopolitical content: {e}", module="Geo", context={'e': e})
            dpg.add_text("GEOPOLITICAL ANALYSIS SYSTEM", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Initializing comprehensive intelligence systems...")

    def create_comprehensive_header(self):
        """Enhanced header with global metrics"""
        with dpg.group(horizontal=True):
            dpg.add_text("GEOPOLITICAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("INTELLIGENCE", color=self.BLOOMBERG_WHITE)
            dpg.add_text("SYSTEM", color=self.BLOOMBERG_BLUE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

            # Global risk metrics
            global_risk = self.calculate_global_risk()
            risk_color = self.get_risk_color(global_risk)
            dpg.add_text(f"GLOBAL RISK: {global_risk}/10", color=risk_color)

            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("CONFLICTS: 12", color=self.BLOOMBERG_RED)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SANCTIONS: 2847", color=self.BLOOMBERG_YELLOW)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("VOLATILITY: HIGH", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("INTEL: ACTIVE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

        # Secondary metrics bar
        with dpg.group(horizontal=True):
            dpg.add_text("THREAT LEVEL:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("ELEVATED", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | OIL:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("$78.45", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | GOLD:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("$1967.80", color=self.BLOOMBERG_YELLOW)
            dpg.add_text(" | VIX:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("18.6", color=self.BLOOMBERG_RED)
            dpg.add_text(" | DXY:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("103.2", color=self.BLOOMBERG_BLUE)
            dpg.add_text(" | ALERTS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("47 ACTIVE", color=self.BLOOMBERG_RED)

        dpg.add_separator()

    def create_advanced_function_keys(self):
        """Enhanced function keys with more options"""
        with dpg.group(horizontal=True):
            function_keys = [
                "F1:THREAT MAP", "F2:LIVE INTEL", "F3:SANCTIONS", "F4:CORRELATIONS",
                "F5:SCENARIOS", "F6:ALERTS", "F7:TRADE ROUTES", "F8:CYBER THREATS",
                "F9:NUCLEAR", "F10:CLIMATE", "F11:MIGRATION", "F12:FORECAST"
            ]
            for key in function_keys:
                dpg.add_button(label=key, width=100, height=25)

        dpg.add_separator()

    def create_global_threat_matrix(self):
        """Global threat assessment matrix"""
        with dpg.child_window(width=400, height=200, border=True):
            dpg.add_text("GLOBAL THREAT MATRIX", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Threat level indicators with detailed metrics
            threats = [
                ("MILITARY", 8.3, "12 Active Conflicts", "HIGH"),
                ("ECONOMIC", 7.1, "Sanctions Escalating", "HIGH"),
                ("CYBER", 7.4, "State-Sponsored Activity", "HIGH"),
                ("NUCLEAR", 6.8, "Proliferation Concerns", "MEDIUM"),
                ("CLIMATE", 5.9, "Resource Competition", "MEDIUM"),
                ("TERROR", 5.2, "Regional Activity", "MEDIUM")
            ]

            for threat_type, level, description, category in threats:
                threat_color = self.get_risk_color(level)

                with dpg.group(horizontal=True):
                    dpg.add_text(f"{threat_type}:", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"{level:.1f}", color=threat_color)
                    dpg.add_text(f"[{category}]", color=threat_color)

                dpg.add_text(f"  {description}", color=self.BLOOMBERG_GRAY, wrap=350)

                # Visual threat bar
                bar_length = int(level)
                threat_bar = "=" * bar_length + "-" * (10 - bar_length)
                dpg.add_text(f"  [{threat_bar}]", color=threat_color)

    def create_real_time_alerts_panel(self):
        """Real-time intelligence alerts"""
        with dpg.child_window(width=500, height=200, border=True):
            dpg.add_text("REAL-TIME INTELLIGENCE ALERTS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            for alert in self.intelligence_feeds:
                priority_color = (self.RISK_CRITICAL if alert["priority"] == "CRITICAL" else
                                  self.RISK_HIGH if alert["priority"] == "HIGH" else self.RISK_MEDIUM)

                with dpg.group(horizontal=True):
                    dpg.add_text(alert["time"], color=self.BLOOMBERG_GRAY)
                    dpg.add_text(f"[{alert['priority']}]", color=priority_color)
                    dpg.add_text(f"[{alert['source']}]", color=self.BLOOMBERG_BLUE)

                dpg.add_text(alert["event"], color=self.BLOOMBERG_WHITE, wrap=450)
                dpg.add_text(f"Classification: {alert['classification']}", color=self.BLOOMBERG_YELLOW)
                dpg.add_spacer(height=5)

    def create_market_impact_summary(self):
        """Market impact summary panel"""
        with dpg.child_window(width=350, height=200, border=True):
            dpg.add_text("MARKET IMPACT SUMMARY", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Impact metrics
            impact_data = [
                ("S&P 500", -2.3, "Geopolitical Uncertainty"),
                ("Energy Sector", +4.1, "Supply Concerns"),
                ("Tech Stocks", -3.8, "Trade War Impact"),
                ("Defense", +2.7, "Increased Spending"),
                ("Gold", +1.9, "Safe Haven Demand"),
                ("Oil", +5.2, "Supply Disruption"),
                ("Bonds", +0.8, "Flight to Quality"),
                ("USD", +1.1, "Reserve Currency")
            ]

            for asset, impact, reason in impact_data:
                impact_color = self.BLOOMBERG_GREEN if impact > 0 else self.BLOOMBERG_RED
                impact_sign = "+" if impact > 0 else ""

                with dpg.group(horizontal=True):
                    dpg.add_text(f"{asset}:", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"{impact_sign}{impact:.1f}%", color=impact_color)

                dpg.add_text(f"  {reason}", color=self.BLOOMBERG_GRAY, wrap=300)

    def create_comprehensive_risk_panel(self):
        """Comprehensive risk assessment panel"""
        with dpg.child_window(width=450, height=500, border=True):
            with dpg.tab_bar():
                # Country Risk Analysis
                with dpg.tab(label="Country Risk"):
                    dpg.add_text("COUNTRY RISK ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                        dpg.add_table_column(label="Country")
                        dpg.add_table_column(label="Risk")
                        dpg.add_table_column(label="Stability")
                        dpg.add_table_column(label="Military")
                        dpg.add_table_column(label="Economy")

                        for country, data in list(self.countries.items()):
                            risk_color = (self.RISK_LOW if data["current_risk"] == "Low" else
                                          self.RISK_MEDIUM if data["current_risk"] == "Medium" else self.RISK_HIGH)

                            with dpg.table_row():
                                dpg.add_text(country[:10])
                                dpg.add_text(data["current_risk"], color=risk_color)
                                dpg.add_text(f"{data['political_stability']:.1f}")
                                dpg.add_text(f"${data['military_budget_billion']}B")
                                dpg.add_text(f"${data['gdp_trillion']:.1f}T")

                # Regional Analysis
                with dpg.tab(label="Regional"):
                    dpg.add_text("REGIONAL HOTSPOT ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    regional_hotspots = [
                        ("Eastern Europe", "CRITICAL", "Ukraine conflict ongoing", 9.2),
                        ("Middle East", "HIGH", "Multi-front tensions", 8.1),
                        ("Asia Pacific", "HIGH", "Taiwan strait tensions", 7.8),
                        ("South China Sea", "HIGH", "Maritime disputes", 7.5),
                        ("Korean Peninsula", "MEDIUM", "Nuclear tensions", 6.9),
                        ("Sahel Region", "MEDIUM", "Terrorism and coups", 6.4),
                        ("Balkans", "LOW", "Ethnic tensions", 4.2),
                        ("Arctic", "LOW", "Resource competition", 3.8)
                    ]

                    for region, risk_level, description, score in regional_hotspots:
                        risk_color = self.get_risk_color(score)

                        with dpg.group(horizontal=True):
                            dpg.add_text(f"{region}:", color=self.BLOOMBERG_WHITE)
                            dpg.add_text(f"[{risk_level}]", color=risk_color)
                            dpg.add_text(f"{score:.1f}", color=risk_color)

                        dpg.add_text(f"  {description}", color=self.BLOOMBERG_GRAY, wrap=400)

                        # Risk trend visualization
                        bar_length = int(score)
                        risk_bar = "=" * bar_length + " " * (10 - bar_length)
                        dpg.add_text(f"  [{risk_bar}]", color=risk_color)
                        dpg.add_spacer(height=5)

                # Military Analysis
                with dpg.tab(label="Military"):
                    dpg.add_text("MILITARY CAPABILITIES ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    with dpg.table(header_row=True, borders_innerH=True):
                        dpg.add_table_column(label="Country")
                        dpg.add_table_column(label="Budget ($B)")
                        dpg.add_table_column(label="Nuclear")
                        dpg.add_table_column(label="Carriers")
                        dpg.add_table_column(label="Bases")

                        military_data = [
                            ("United States", 816, 5428, 11, 800),
                            ("China", 293, 350, 3, 3),
                            ("Russia", 154, 5977, 1, 21),
                            ("India", 76, 160, 2, 4),
                            ("France", 59, 290, 1, 12),
                            ("UK", 68, 225, 2, 16)
                        ]

                        for country, budget, nuclear, carriers, bases in military_data:
                            with dpg.table_row():
                                dpg.add_text(country[:8])
                                dpg.add_text(f"${budget}")
                                dpg.add_text(f"{nuclear}")
                                dpg.add_text(f"{carriers}")
                                dpg.add_text(f"{bases}")

    def create_comprehensive_event_panel(self):
        """Comprehensive event analysis panel"""
        with dpg.child_window(width=550, height=500, border=True):
            with dpg.tab_bar():
                # Current Events
                with dpg.tab(label="Active Events"):
                    dpg.add_text("ACTIVE GEOPOLITICAL EVENTS", color=self.BLOOMBERG_ORANGE)

                    for event in self.global_events:
                        impact_color = (self.RISK_CRITICAL if event["impact_level"] == "Critical" else
                                        self.RISK_HIGH if event["impact_level"] == "High" else self.RISK_MEDIUM)

                        with dpg.child_window(height=140, border=True):
                            # Event header
                            with dpg.group(horizontal=True):
                                dpg.add_text(event["event"], color=self.BLOOMBERG_WHITE)
                                dpg.add_text(f"[{event['impact_level']}]", color=impact_color)

                            # Metrics
                            with dpg.group(horizontal=True):
                                dpg.add_text(f"Probability: {event['probability']}%", color=self.BLOOMBERG_GRAY)
                                dpg.add_text(f"Market Impact: {event['market_impact']:.1f}%",
                                             color=self.BLOOMBERG_RED if event[
                                                                             'market_impact'] < 0 else self.BLOOMBERG_GREEN)

                            with dpg.group(horizontal=True):
                                dpg.add_text(f"Volatility: +{event['volatility_impact']:.1f}%",
                                             color=self.BLOOMBERG_YELLOW)
                                dpg.add_text(f"Escalation Risk: {event['escalation_probability']}%",
                                             color=self.BLOOMBERG_RED)

                            # Affected sectors and currencies
                            sectors = ", ".join(event["market_sectors"])
                            dpg.add_text(f"Sectors: {sectors}", color=self.BLOOMBERG_BLUE, wrap=500)

                            currencies = ", ".join(event["affected_currencies"])
                            dpg.add_text(f"Currencies: {currencies}", color=self.BLOOMBERG_PURPLE, wrap=500)

                            commodities = ", ".join(event["commodity_impact"])
                            dpg.add_text(f"Commodities: {commodities}", color=self.BLOOMBERG_CYAN, wrap=500)

                # Intelligence Analysis
                with dpg.tab(label="Intelligence"):
                    dpg.add_text("INTELLIGENCE ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    # Intelligence source breakdown
                    intel_sources = [
                        ("SIGINT", "Signals Intelligence", 24, "Electronic communications"),
                        ("HUMINT", "Human Intelligence", 18, "Human sources and agents"),
                        ("OSINT", "Open Source", 35, "Public information analysis"),
                        ("GEOINT", "Geospatial Intelligence", 12, "Satellite and imagery"),
                        ("CYBINT", "Cyber Intelligence", 11, "Digital and cyber sources")
                    ]

                    for source, full_name, percentage, description in intel_sources:
                        with dpg.group(horizontal=True):
                            dpg.add_text(f"{source}:", color=self.BLOOMBERG_WHITE)
                            dpg.add_text(f"{percentage}%", color=self.BLOOMBERG_GREEN)
                            dpg.add_text(full_name, color=self.BLOOMBERG_GRAY)

                        dpg.add_text(f"  {description}", color=self.BLOOMBERG_GRAY, wrap=500)

                        # Visual percentage bar
                        bar_length = int(percentage / 4)
                        intel_bar = "=" * bar_length + " " * (25 - bar_length)
                        dpg.add_text(f"  [{intel_bar}]", color=self.BLOOMBERG_GREEN)
                        dpg.add_spacer(height=5)

                # Scenario Modeling
                with dpg.tab(label="Scenarios"):
                    dpg.add_text("SCENARIO MODELING & FORECASTING", color=self.BLOOMBERG_ORANGE)

                    scenarios = [
                        {
                            "name": "Status Quo", "probability": 45, "description": "Current tensions persist",
                            "market_impact": -1.2, "timeline": "6-12 months",
                            "key_factors": ["Diplomatic stalemate", "Economic pressure", "Military posturing"]
                        },
                        {
                            "name": "Escalation", "probability": 30, "description": "Conflicts intensify significantly",
                            "market_impact": -8.7, "timeline": "3-6 months",
                            "key_factors": ["Military action", "Sanctions expansion", "Alliance activation"]
                        },
                        {
                            "name": "De-escalation", "probability": 20, "description": "Diplomatic breakthroughs",
                            "market_impact": +3.4, "timeline": "12+ months",
                            "key_factors": ["Peace talks", "Economic cooperation", "Leadership changes"]
                        },
                        {
                            "name": "Black Swan", "probability": 5, "description": "Unexpected major event",
                            "market_impact": -15.3, "timeline": "Immediate",
                            "key_factors": ["Nuclear incident", "Terrorist attack", "Cyber warfare"]
                        }
                    ]

                    for scenario in scenarios:
                        prob_color = (self.BLOOMBERG_GREEN if scenario["probability"] > 40 else
                                      self.BLOOMBERG_YELLOW if scenario["probability"] > 15 else self.BLOOMBERG_RED)

                        impact_color = (self.BLOOMBERG_GREEN if scenario["market_impact"] > 0 else self.BLOOMBERG_RED)

                        with dpg.child_window(height=80, border=True):
                            with dpg.group(horizontal=True):
                                dpg.add_text(scenario["name"], color=self.BLOOMBERG_WHITE)
                                dpg.add_text(f"{scenario['probability']}%", color=prob_color)
                                dpg.add_text(f"{scenario['market_impact']:+.1f}%", color=impact_color)

                            dpg.add_text(scenario["description"], color=self.BLOOMBERG_GRAY)

                            factors = ", ".join(scenario["key_factors"])
                            dpg.add_text(f"Key factors: {factors}", color=self.BLOOMBERG_BLUE, wrap=500)

    def create_comprehensive_market_panel(self):
        """Comprehensive market analysis panel"""
        with dpg.child_window(width=450, height=500, border=True):
            with dpg.tab_bar():
                # Market Correlations
                with dpg.tab(label="Correlations"):
                    dpg.add_text("MARKET CORRELATION ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                        dpg.add_table_column(label="Asset Class")
                        dpg.add_table_column(label="Max Corr")
                        dpg.add_table_column(label="Impact")
                        dpg.add_table_column(label="Volatility")

                        for asset, correlations in self.correlation_matrix.items():
                            max_correlation = max(correlations.values())

                            # Calculate volatility impact
                            volatility = random.uniform(1.2, 3.8)

                            corr_color = (self.BLOOMBERG_RED if max_correlation > 0.8 else
                                          self.BLOOMBERG_YELLOW if max_correlation > 0.6 else
                                          self.BLOOMBERG_GREEN)

                            with dpg.table_row():
                                dpg.add_text(asset[:12])
                                dpg.add_text(f"{max_correlation:.2f}", color=corr_color)

                                impact = "High" if max_correlation > 0.8 else "Med" if max_correlation > 0.6 else "Low"
                                dpg.add_text(impact, color=corr_color)
                                dpg.add_text(f"{volatility:.1f}%", color=self.BLOOMBERG_YELLOW)

                # Sector Analysis
                with dpg.tab(label="Sectors"):
                    dpg.add_text("SECTOR VULNERABILITY ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    sector_data = [
                        ("Technology", 8.7, "Supply chain risks", ["Semiconductors", "Hardware", "Software"]),
                        ("Energy", 9.1, "Geopolitical supply", ["Oil", "Gas", "Renewables"]),
                        ("Defense", 7.3, "Military spending", ["Aerospace", "Weapons", "Cyber"]),
                        ("Financials", 6.9, "Sanctions impact", ["Banking", "Insurance", "Payments"]),
                        ("Materials", 8.2, "Trade disruptions", ["Metals", "Mining", "Chemicals"]),
                        ("Healthcare", 4.8, "Supply security", ["Pharma", "Devices", "Services"]),
                        ("Consumer", 5.5, "Economic impact", ["Discretionary", "Staples", "Retail"]),
                        ("Utilities", 6.1, "Infrastructure", ["Power", "Water", "Telecom"])
                    ]

                    for sector, vulnerability, driver, subsectors in sector_data:
                        vuln_color = self.get_risk_color(vulnerability)

                        with dpg.group(horizontal=True):
                            dpg.add_text(f"{sector}:", color=self.BLOOMBERG_WHITE)
                            dpg.add_text(f"{vulnerability:.1f}", color=vuln_color)

                        dpg.add_text(f"  Driver: {driver}", color=self.BLOOMBERG_GRAY, wrap=400)

                        subsector_text = "  Subsectors: " + ", ".join(subsectors)
                        dpg.add_text(subsector_text, color=self.BLOOMBERG_BLUE, wrap=400)

                        # Vulnerability bar
                        bar_length = int(vulnerability)
                        vuln_bar = "=" * bar_length + " " * (10 - bar_length)
                        dpg.add_text(f"  [{vuln_bar}]", color=vuln_color)
                        dpg.add_spacer(height=3)

                # Currency Impact
                with dpg.tab(label="Currencies"):
                    dpg.add_text("CURRENCY IMPACT ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    currency_data = [
                        ("USD", 1.2, "Safe haven flows", "Reserve currency status"),
                        ("EUR", -2.1, "Energy crisis", "Russian gas dependency"),
                        ("JPY", 0.8, "Risk-off demand", "Low interest rates"),
                        ("GBP", -1.4, "Political uncertainty", "Brexit aftermath"),
                        ("CNY", -3.2, "Trade war impact", "Capital controls"),
                        ("RUB", -18.7, "Sanctions regime", "Commodity dependence"),
                        ("CHF", 1.8, "Neutral safe haven", "Banking secrecy"),
                        ("CAD", 0.3, "Commodity exposure", "Energy exports"),
                        ("AUD", -0.9, "China trade links", "Commodity cycles"),
                        ("BRL", -2.8, "Political instability", "Emerging market risks")
                    ]

                    for currency, impact, primary_driver, secondary_factor in currency_data:
                        impact_color = self.BLOOMBERG_GREEN if impact > 0 else self.BLOOMBERG_RED
                        impact_sign = "+" if impact > 0 else ""

                        with dpg.group(horizontal=True):
                            dpg.add_text(f"{currency}:", color=self.BLOOMBERG_WHITE)
                            dpg.add_text(f"{impact_sign}{impact:.1f}%", color=impact_color)

                        dpg.add_text(f"  Primary: {primary_driver}", color=self.BLOOMBERG_GRAY, wrap=400)
                        dpg.add_text(f"  Factor: {secondary_factor}", color=self.BLOOMBERG_BLUE, wrap=400)

    def create_predictive_analytics_panel(self):
        """Advanced predictive analytics with charts"""
        with dpg.child_window(width=600, height=250, border=True):
            dpg.add_text("PREDICTIVE ANALYTICS & FORECASTING", color=self.BLOOMBERG_ORANGE)

            # Risk prediction chart
            with dpg.plot(height=180, width=-1):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Time (Days)")
                y_axis = dpg.add_plot_axis(dpg.mvYAxis, label="Risk Level (0-10)", tag="risk_y_axis")

                # Generate comprehensive prediction data
                x_data = list(range(90))  # 3 months forecast

                # Base risk trend with seasonal variations
                base_risk = []
                for i in range(90):
                    seasonal = 0.5 * math.sin(i * 0.1)  # Seasonal component
                    trend = 6.5 + 0.02 * i  # Slight upward trend
                    noise = random.uniform(-0.4, 0.4)  # Random variation
                    base_risk.append(max(0, min(10, trend + seasonal + noise)))

                dpg.add_line_series(x_data, base_risk, label="Base Risk Forecast", parent=y_axis)

                # Event impact scenarios
                conflict_risk = []
                for i in range(90):
                    base = base_risk[i]
                    if 20 <= i <= 35:  # Conflict escalation period
                        event_impact = 2.5 * (1 - abs(i - 27.5) / 7.5)  # Peak at day 27.5
                        base += event_impact
                    conflict_risk.append(max(0, min(10, base)))

                dpg.add_line_series(x_data, conflict_risk, label="Conflict Escalation Scenario", parent=y_axis)

                # Economic crisis scenario
                economic_risk = []
                for i in range(90):
                    base = base_risk[i]
                    if 45 <= i <= 75:  # Economic crisis period
                        crisis_impact = 1.8 * math.sin((i - 45) * math.pi / 30)
                        base += crisis_impact
                    economic_risk.append(max(0, min(10, base)))

                dpg.add_line_series(x_data, economic_risk, label="Economic Crisis Scenario", parent=y_axis)

    def create_sanctions_monitoring_panel(self):
        """Comprehensive sanctions monitoring"""
        with dpg.child_window(width=500, height=250, border=True):
            dpg.add_text("SANCTIONS MONITORING SYSTEM", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Sanctions effectiveness metrics
            with dpg.group(horizontal=True):
                with dpg.group():
                    dpg.add_text("SANCTIONS OVERVIEW", color=self.BLOOMBERG_YELLOW)
                    dpg.add_text(f"Total Active: {self.sanctions_data['Active Sanctions']}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"Countries: {self.sanctions_data['Countries Affected']}", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"Economic Impact: ${self.sanctions_data['Economic Impact ($B)']}B",
                                 color=self.BLOOMBERG_RED)
                    dpg.add_text(f"Trade Disruption: {self.sanctions_data['Trade Routes Disrupted']} routes",
                                 color=self.BLOOMBERG_YELLOW)

                with dpg.group():
                    dpg.add_text("EFFECTIVENESS RATINGS", color=self.BLOOMBERG_YELLOW)
                    for target, data in self.sanctions_data["Effectiveness Rating"].items():
                        rating_color = self.get_risk_color(data["rating"])
                        dpg.add_text(f"{target}: {data['rating']:.1f}/10", color=rating_color)
                        dpg.add_text(f"  Compliance: {data['compliance']}", color=self.BLOOMBERG_GRAY)
                        dpg.add_text(f"  Evasion: {data['evasion']}", color=self.BLOOMBERG_RED)

            dpg.add_separator()

            # Recent sanctions changes
            dpg.add_text("RECENT SANCTIONS UPDATES", color=self.BLOOMBERG_YELLOW)
            for i, change in enumerate(self.sanctions_data["Recent Changes"][:3]):
                dpg.add_text(f"* {change}", color=self.BLOOMBERG_WHITE, wrap=480)

    def create_trade_route_analysis_panel(self):
        """Trade route security analysis"""
        with dpg.child_window(width=450, height=250, border=True):
            dpg.add_text("CRITICAL TRADE ROUTE ANALYSIS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Trade route status table
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                dpg.add_table_column(label="Route")
                dpg.add_table_column(label="Status")
                dpg.add_table_column(label="Daily Value")
                dpg.add_table_column(label="Threat Level")

                for route, data in self.trade_routes.items():
                    status_color = (self.RISK_HIGH if data["status"] == "HIGH RISK" else
                                    self.RISK_MEDIUM if data["status"] == "MEDIUM RISK" else
                                    self.RISK_LOW)

                    threat_color = self.get_risk_color(data["threat_level"])

                    with dpg.table_row():
                        dpg.add_text(route[:12])
                        dpg.add_text(data["status"][:6], color=status_color)

                        # Format daily value based on type
                        if "daily_oil_mbpd" in data:
                            dpg.add_text(f"{data['daily_oil_mbpd']:.1f} mbpd")
                        else:
                            dpg.add_text(f"${data['daily_trade_billions']:.1f}B")

                        dpg.add_text(f"{data['threat_level']:.1f}", color=threat_color)

            dpg.add_separator()

            # Route security metrics
            dpg.add_text("SECURITY METRICS", color=self.BLOOMBERG_YELLOW)
            security_metrics = [
                ("Naval Presence", "42 vessels", "International patrols"),
                ("Insurance Rates", "+23% avg", "War risk premiums"),
                ("Transit Times", "+15% delay", "Security protocols"),
                ("Alternative Routes", "3 major", "Backup capacity")
            ]

            for metric, value, description in security_metrics:
                with dpg.group(horizontal=True):
                    dpg.add_text(f"{metric}:", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(value, color=self.BLOOMBERG_GREEN)
                dpg.add_text(f"  {description}", color=self.BLOOMBERG_GRAY, wrap=400)

    def create_intelligence_dashboard(self):
        """Final intelligence dashboard section"""
        with dpg.group(horizontal=True):
            # Economic indicators impact
            with dpg.child_window(width=400, height=150, border=True):
                dpg.add_text("ECONOMIC INDICATORS IMPACT", color=self.BLOOMBERG_ORANGE)

                with dpg.table(header_row=True, borders_innerH=True):
                    dpg.add_table_column(label="Indicator")
                    dpg.add_table_column(label="Current")
                    dpg.add_table_column(label="Change")
                    dpg.add_table_column(label="Impact")

                    for indicator, data in list(self.economic_indicators.items())[:6]:
                        change_color = (self.BLOOMBERG_GREEN if data["change"] > 0 else
                                        self.BLOOMBERG_RED if data["change"] < 0 else self.BLOOMBERG_GRAY)

                        impact_color = (self.RISK_CRITICAL if data["impact"] == "Critical" else
                                        self.RISK_HIGH if data["impact"] == "High" else self.RISK_MEDIUM)

                        with dpg.table_row():
                            dpg.add_text(indicator[:12])
                            dpg.add_text(f"{data['current']:.1f}")

                            change_sign = "+" if data["change"] > 0 else ""
                            dpg.add_text(f"{change_sign}{data['change']:.1f}", color=change_color)
                            dpg.add_text(data["impact"][:4], color=impact_color)

            # Nuclear threat assessment
            with dpg.child_window(width=350, height=150, border=True):
                dpg.add_text("NUCLEAR THREAT ASSESSMENT", color=self.BLOOMBERG_ORANGE)

                nuclear_powers = [
                    ("United States", 5428, "Deployed: 1644"),
                    ("Russia", 5977, "Deployed: 1588"),
                    ("China", 350, "Deployed: ~200"),
                    ("France", 290, "Deployed: 280"),
                    ("United Kingdom", 225, "Deployed: 120"),
                    ("India", 160, "Deployed: ~100"),
                    ("Pakistan", 165, "Deployed: ~110"),
                    ("Israel", 90, "Undeclared"),
                    ("North Korea", 20, "Estimated")
                ]

                for country, total, status in nuclear_powers[:6]:
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{country[:8]}:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"{total}", color=self.BLOOMBERG_RED)
                    dpg.add_text(f"  {status}", color=self.BLOOMBERG_GRAY)

            # Cyber threat landscape
            with dpg.child_window(width=400, height=150, border=True):
                dpg.add_text("CYBER THREAT LANDSCAPE", color=self.BLOOMBERG_ORANGE)

                cyber_threats = [
                    ("State-Sponsored APTs", "CRITICAL", "42 active groups"),
                    ("Financial Infrastructure", "HIGH", "Banking sector targeted"),
                    ("Critical Infrastructure", "HIGH", "Power grid vulnerabilities"),
                    ("Supply Chain Attacks", "MEDIUM", "Software compromises"),
                    ("Ransomware Groups", "MEDIUM", "Criminal organizations"),
                    ("Social Engineering", "LOW", "Information warfare")
                ]

                for threat_type, level, description in cyber_threats:
                    level_color = (self.RISK_CRITICAL if level == "CRITICAL" else
                                   self.RISK_HIGH if level == "HIGH" else
                                   self.RISK_MEDIUM if level == "MEDIUM" else self.RISK_LOW)

                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{threat_type}:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"[{level}]", color=level_color)
                    dpg.add_text(f"  {description}", color=self.BLOOMBERG_GRAY, wrap=350)

            # Climate security nexus
            with dpg.child_window(width=350, height=150, border=True):
                dpg.add_text("CLIMATE SECURITY NEXUS", color=self.BLOOMBERG_ORANGE)

                climate_risks = [
                    ("Water Scarcity", "HIGH", "Resource conflicts"),
                    ("Food Security", "MEDIUM", "Agricultural disruption"),
                    ("Mass Migration", "MEDIUM", "Population displacement"),
                    ("Extreme Weather", "HIGH", "Infrastructure damage"),
                    ("Sea Level Rise", "LOW", "Coastal flooding"),
                    ("Arctic Competition", "MEDIUM", "Resource access")
                ]

                for risk_type, level, impact in climate_risks:
                    level_color = (self.RISK_HIGH if level == "HIGH" else
                                   self.RISK_MEDIUM if level == "MEDIUM" else self.RISK_LOW)

                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{risk_type}:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"[{level}]", color=level_color)
                    dpg.add_text(f"  {impact}", color=self.BLOOMBERG_GRAY, wrap=300)

    def calculate_global_risk(self):
        """Calculate comprehensive global risk score"""
        total_risk = 0
        total_weight = 0

        for factor, data in self.risk_factors.items():
            total_risk += data["current"] * data["weight"]
            total_weight += data["weight"]

        return round(total_risk / total_weight, 1)

    def get_risk_color(self, risk_level):
        """Get color based on risk level"""
        if risk_level >= 8.5:
            return self.RISK_CRITICAL
        elif risk_level >= 7.0:
            return self.RISK_HIGH
        elif risk_level >= 5.5:
            return self.RISK_MEDIUM
        elif risk_level >= 4.0:
            return self.RISK_LOW
        else:
            return self.RISK_STABLE

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Resize components for responsive layout"""
        pass

    def cleanup(self):
        """Clean up geopolitical analysis resources"""
        logger.info("Comprehensive Geopolitical Analysis tab cleanup completed", module="Geo")