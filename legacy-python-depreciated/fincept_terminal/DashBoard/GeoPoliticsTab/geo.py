"""
Geo module for Fincept Terminal
Updated with real-world geopolitical data and India focus
"""

# geopolitical_analysis.py - Comprehensive Bloomberg Terminal Geopolitical Analysis Dashboard
# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
import random
import datetime
import math
import json
from fincept_terminal.utils.base_tab import BaseTab

from fincept_terminal.utils.Logging.logger import logger

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
        """Initialize comprehensive geopolitical analysis data with real-world information"""

        # Updated countries database with real-world 2024-2025 data
        self.countries = {
            "India": {
                "region": "Asia Pacific", "gdp_trillion": 3.9, "military_budget_billion": 83.6,
                "political_stability": 6.8, "economic_freedom": 6.5, "current_risk": "Medium",
                "market_influence": 7.8, "currency": "INR", "population_million": 1428,
                "debt_to_gdp": 84.0, "democracy_index": 7.18, "corruption_index": 40,
                "trade_freedom": 6.9, "innovation_index": 6.8, "cyber_security": 6.5,
                "key_allies": ["United States", "Israel", "France", "Japan", "UAE", "Australia"],
                "tensions": ["China", "Pakistan", "Canada (Diplomatic)"],
                "recent_events": ["G20 Presidency Success", "Digital India Push", "Make in India",
                                  "Border Infrastructure", "Quad Alliance", "INR Internationalization"],
                "military_bases": 12, "nuclear_warheads": 172, "aircraft_carriers": 2
            },
            "United States": {
                "region": "North America", "gdp_trillion": 27.4, "military_budget_billion": 842,
                "political_stability": 7.0, "economic_freedom": 7.6, "current_risk": "Low",
                "market_influence": 9.5, "currency": "USD", "population_million": 335,
                "debt_to_gdp": 123.3, "democracy_index": 7.85, "corruption_index": 67,
                "trade_freedom": 8.0, "innovation_index": 9.1, "cyber_security": 8.4,
                "key_allies": ["UK", "Japan", "Germany", "France", "Australia", "Canada", "India"],
                "tensions": ["China", "Russia", "Iran", "North Korea"],
                "recent_events": ["Fed Rate Decisions", "Ukraine Support", "Chip Act Implementation",
                                  "AI Regulation", "Indo-Pacific Strategy"],
                "military_bases": 750, "nuclear_warheads": 5244, "aircraft_carriers": 11
            },
            "China": {
                "region": "Asia Pacific", "gdp_trillion": 18.2, "military_budget_billion": 296,
                "political_stability": 6.5, "economic_freedom": 5.3, "current_risk": "Medium",
                "market_influence": 8.9, "currency": "CNY", "population_million": 1412,
                "debt_to_gdp": 83.2, "democracy_index": 2.12, "corruption_index": 42,
                "trade_freedom": 6.5, "innovation_index": 7.6, "cyber_security": 7.8,
                "key_allies": ["Russia", "Pakistan", "Iran", "North Korea", "Belarus"],
                "tensions": ["United States", "India", "Taiwan", "Japan", "Philippines", "Australia"],
                "recent_events": ["Property Crisis", "Youth Unemployment", "Belt & Road", "LAC Tensions",
                                  "South China Sea Claims", "Taiwan Pressure"],
                "military_bases": 5, "nuclear_warheads": 410, "aircraft_carriers": 3
            },
            "Russia": {
                "region": "Europe/Asia", "gdp_trillion": 2.0, "military_budget_billion": 109,
                "political_stability": 4.0, "economic_freedom": 3.8, "current_risk": "High",
                "market_influence": 6.2, "currency": "RUB", "population_million": 144,
                "debt_to_gdp": 19.3, "democracy_index": 2.94, "corruption_index": 28,
                "trade_freedom": 4.2, "innovation_index": 4.9, "cyber_security": 6.7,
                "key_allies": ["China", "Iran", "North Korea", "Belarus", "Syria"],
                "tensions": ["United States", "NATO", "Ukraine", "EU"],
                "recent_events": ["Ukraine Conflict Ongoing", "BRICS Expansion", "Energy Leverage",
                                  "Wagner Group Issues", "Nuclear Rhetoric"],
                "military_bases": 21, "nuclear_warheads": 5889, "aircraft_carriers": 1
            },
            "Pakistan": {
                "region": "South Asia", "gdp_trillion": 0.374, "military_budget_billion": 10.3,
                "political_stability": 3.8, "economic_freedom": 4.9, "current_risk": "High",
                "market_influence": 3.2, "currency": "PKR", "population_million": 240,
                "debt_to_gdp": 77.8, "democracy_index": 4.13, "corruption_index": 27,
                "trade_freedom": 5.1, "innovation_index": 3.7, "cyber_security": 4.2,
                "key_allies": ["China", "Saudi Arabia", "Turkey", "UAE"],
                "tensions": ["India", "Afghanistan", "TTP Terrorism"],
                "recent_events": ["Economic Crisis", "IMF Bailout", "Political Instability",
                                  "Terror Groups", "CPEC Debt", "India Border Tensions"],
                "military_bases": 3, "nuclear_warheads": 170, "aircraft_carriers": 0
            },
            "European Union": {
                "region": "Europe", "gdp_trillion": 17.2, "military_budget_billion": 288,
                "political_stability": 7.3, "economic_freedom": 7.1, "current_risk": "Medium",
                "market_influence": 8.0, "currency": "EUR", "population_million": 448,
                "debt_to_gdp": 88.1, "democracy_index": 8.08, "corruption_index": 64,
                "trade_freedom": 7.6, "innovation_index": 7.8, "cyber_security": 7.4,
                "key_allies": ["United States", "NATO", "UK", "Japan", "India"],
                "tensions": ["Russia", "Energy Security", "Migration"],
                "recent_events": ["Ukraine Support", "Green Deal", "Digital Markets Act",
                                  "Defense Integration", "Energy Transition"],
                "military_bases": 42, "nuclear_warheads": 515, "aircraft_carriers": 4
            }
        }

        # Updated global events with real-world current situations
        self.global_events = [
            {
                "event": "Russia-Ukraine War (Ongoing)", "impact_level": "Critical", "probability": 100,
                "market_sectors": ["Energy", "Agriculture", "Defense", "Commodities"],
                "timeframe": "Ongoing - Year 3", "market_impact": -3.8, "volatility_impact": 3.2,
                "affected_currencies": ["RUB", "UAH", "EUR", "USD"],
                "commodity_impact": ["Natural Gas", "Wheat", "Fertilizers", "Palladium"],
                "escalation_probability": 20, "resolution_timeline": "12-24 months"
            },
            {
                "event": "India-China LAC Border Tensions", "impact_level": "High", "probability": 85,
                "market_sectors": ["Defense", "Technology", "Infrastructure", "Trade"],
                "timeframe": "Ongoing", "market_impact": -2.1, "volatility_impact": 2.8,
                "affected_currencies": ["INR", "CNY", "USD"],
                "commodity_impact": ["Rare Earths", "Electronics"],
                "escalation_probability": 35, "resolution_timeline": "18+ months"
            },
            {
                "event": "US-China Strategic Competition", "impact_level": "Critical", "probability": 98,
                "market_sectors": ["Technology", "Semiconductors", "Manufacturing", "Defense"],
                "timeframe": "Ongoing", "market_impact": -3.5, "volatility_impact": 3.1,
                "affected_currencies": ["USD", "CNY", "JPY", "KRW"],
                "commodity_impact": ["Semiconductors", "Rare Earths", "Lithium"],
                "escalation_probability": 40, "resolution_timeline": "5+ years"
            },
            {
                "event": "Middle East Conflicts (Gaza, Yemen, Syria)", "impact_level": "High", "probability": 90,
                "market_sectors": ["Energy", "Shipping", "Insurance", "Defense"],
                "timeframe": "Ongoing", "market_impact": -2.9, "volatility_impact": 2.6,
                "affected_currencies": ["USD", "EUR", "ILS"],
                "commodity_impact": ["Oil", "Gold", "LNG"],
                "escalation_probability": 45, "resolution_timeline": "6-18 months"
            },
            {
                "event": "India's Economic Rise & Geopolitical Positioning", "impact_level": "High", "probability": 95,
                "market_sectors": ["Technology", "Manufacturing", "Services", "Infrastructure"],
                "timeframe": "Accelerating", "market_impact": +4.2, "volatility_impact": 1.8,
                "affected_currencies": ["INR", "USD"],
                "commodity_impact": ["Manufacturing Inputs", "Energy"],
                "escalation_probability": 5, "resolution_timeline": "Positive Long-term"
            },
            {
                "event": "Global AI Arms Race", "impact_level": "Critical", "probability": 92,
                "market_sectors": ["Technology", "Defense", "Semiconductors", "Cyber"],
                "timeframe": "Accelerating", "market_impact": +2.8, "volatility_impact": 2.4,
                "affected_currencies": ["USD", "CNY", "EUR"],
                "commodity_impact": ["Computing Power", "Energy"],
                "escalation_probability": 60, "resolution_timeline": "Ongoing"
            }
        ]

        # Real economic indicators (as of early 2025)
        self.economic_indicators = {
            "Global Trade Volume": {"current": 29.4, "change": -1.8, "trend": "DOWN", "impact": "High"},
            "Oil Price (Brent)": {"current": 82.30, "change": 2.1, "trend": "UP", "impact": "Critical"},
            "Gold Price": {"current": 2045.60, "change": 18.30, "trend": "UP", "impact": "High"},
            "VIX (Fear Index)": {"current": 16.8, "change": -1.2, "trend": "DOWN", "impact": "Medium"},
            "Dollar Index (DXY)": {"current": 104.8, "change": 0.3, "trend": "UP", "impact": "Critical"},
            "Baltic Dry Index": {"current": 1567, "change": -89, "trend": "DOWN", "impact": "Medium"},
            "India NSE Nifty": {"current": 24850, "change": 235, "trend": "UP", "impact": "High"},
            "INR/USD Rate": {"current": 83.12, "change": -0.08, "trend": "STABLE", "impact": "High"}
        }

        # Real sanctions data (updated)
        self.sanctions_data = {
            "Active Sanctions": 3124, "Countries Affected": 52, "Economic Impact ($B)": 1456,
            "Trade Routes Disrupted": 27, "Financial Institutions": 428, "Individuals": 14892,
            "Major Targets": ["Russia", "Iran", "North Korea", "Myanmar", "Belarus", "Syria"],
            "Recent Changes": [
                "Russia: Expanded oil price cap to $60, new banking restrictions",
                "China: Semiconductor equipment export controls tightened",
                "Iran: Additional Revolutionary Guard designations",
                "North Korea: Crypto-related sanctions for missile programs",
                "Belarus: Potash and fertilizer export restrictions"
            ],
            "Effectiveness Rating": {
                "Russia": {"rating": 7.4, "compliance": "Medium", "evasion": "High"},
                "Iran": {"rating": 6.5, "compliance": "Low", "evasion": "Very High"},
                "North Korea": {"rating": 7.8, "compliance": "Very Low", "evasion": "Critical"}
            }
        }

        # Updated market correlation matrix
        self.correlation_matrix = {
            "Energy Stocks": {
                "Russia Conflict": 0.89, "Middle East Tensions": 0.76, "Sanctions": 0.71,
                "Oil Prices": 0.93, "Gas Prices": 0.85, "Geopolitical Risk": 0.80
            },
            "Technology Stocks": {
                "US-China Relations": 0.84, "Taiwan Crisis": 0.92, "Semiconductor Sanctions": 0.90,
                "Supply Chain Disruption": 0.78, "Cyber Threats": 0.62, "Trade Wars": 0.73
            },
            "Defense Stocks": {
                "Military Conflicts": 0.94, "NATO Spending": 0.81, "Arms Sales": 0.86,
                "Regional Tensions": 0.69, "Threat Levels": 0.75, "Alliance Changes": 0.58
            },
            "Indian Markets": {
                "China Relations": -0.68, "Infrastructure Spending": 0.82, "FDI Flows": 0.76,
                "Digital India": 0.71, "Manufacturing Growth": 0.79, "Border Security": -0.54
            },
            "Emerging Markets": {
                "Regional Stability": 0.80, "Capital Flows": 0.72, "Commodity Dependence": 0.61,
                "Currency Crisis": -0.83, "Political Risk": -0.76, "External Debt": -0.69
            }
        }

        # Updated risk assessment model
        self.risk_factors = {
            "Military Conflicts": {"weight": 0.25, "current": 8.1, "trend": "UP", "volatility": 2.3},
            "Economic Sanctions": {"weight": 0.20, "current": 7.4, "trend": "UP", "volatility": 1.9},
            "Political Instability": {"weight": 0.18, "current": 7.2, "trend": "UP", "volatility": 1.6},
            "Trade Disputes": {"weight": 0.15, "current": 6.8, "trend": "FLAT", "volatility": 1.4},
            "Cyber Threats": {"weight": 0.12, "current": 7.8, "trend": "UP", "volatility": 2.5},
            "Climate Events": {"weight": 0.10, "current": 6.4, "trend": "UP", "volatility": 1.8}
        }

        # India-specific intelligence feeds
        self.intelligence_feeds = [
            {"time": "16:15", "priority": "HIGH", "source": "HUMINT", "classification": "SECRET",
             "event": "India-China border talks scheduled; infrastructure buildout accelerating"},
            {"time": "16:02", "priority": "CRITICAL", "source": "ECONINT", "classification": "TOP SECRET",
             "event": "Significant FDI inflows to India's semiconductor sector detected"},
            {"time": "15:48", "priority": "MEDIUM", "source": "OSINT", "classification": "CONFIDENTIAL",
             "event": "India-Middle East-Europe Corridor gaining momentum among traders"},
            {"time": "15:35", "priority": "HIGH", "source": "SIGINT", "classification": "SECRET",
             "event": "Pakistan-China military coordination intensifying near LAC"},
            {"time": "15:21", "priority": "CRITICAL", "source": "CYBINT", "classification": "TOP SECRET",
             "event": "State-sponsored cyber activity targeting Indian digital infrastructure"}
        ]

        # Updated trade route analysis (India-focused)
        self.trade_routes = {
            "Strait of Hormuz": {"status": "HIGH RISK", "daily_oil_mbpd": 21.0, "threat_level": 8.2,
                                 "india_exposure": "Critical - 40% oil imports"},
            "Strait of Malacca": {"status": "MEDIUM RISK", "daily_trade_billions": 3.8, "threat_level": 6.4,
                                  "india_exposure": "High - Major trade route"},
            "Suez Canal": {"status": "MEDIUM RISK", "daily_trade_billions": 10.2, "threat_level": 6.8,
                           "india_exposure": "High - Europe trade"},
            "India-Middle East Corridor": {"status": "DEVELOPING", "daily_trade_billions": 1.2, "threat_level": 4.5,
                                           "india_exposure": "Strategic - Alternative to China"},
            "South China Sea": {"status": "HIGH RISK", "daily_trade_billions": 5.8, "threat_level": 8.1,
                                "india_exposure": "Medium - ASEAN trade"},
            "Indian Ocean Region": {"status": "MEDIUM RISK", "daily_trade_billions": 4.5, "threat_level": 5.9,
                                    "india_exposure": "Critical - Maritime security"}
        }

    def create_content(self):
        """Create the comprehensive geopolitical analysis dashboard"""
        try:
            # Enhanced header with comprehensive metrics
            self.create_comprehensive_header()

            # Enhanced function keys
            self.create_advanced_function_keys()

            # First section - Intelligence dashboard (was last)
            self.create_intelligence_dashboard()

            dpg.add_separator()

            # Second section - Advanced analytics (was bottom)
            with dpg.group(horizontal=True):
                self.create_predictive_analytics_panel()
                self.create_sanctions_monitoring_panel()
                self.create_trade_route_analysis_panel()

            dpg.add_separator()

            # Fourth section - India-specific dashboard
            self.create_india_focused_section()

            dpg.add_separator()

            # Final section - Main analysis (was in middle, now at end)
            with dpg.group(horizontal=True):
                # Left column - Risk assessment and country analysis
                self.create_comprehensive_risk_panel()

                # Center column - Event analysis and intelligence
                self.create_comprehensive_event_panel()

                # Right column - Market correlations and predictions
                self.create_comprehensive_market_panel()

            dpg.add_separator()

            # Third section - Critical metrics and alerts (was top)
            with dpg.group(horizontal=True):
                self.create_global_threat_matrix()
                self.create_real_time_alerts_panel()
                self.create_market_impact_summary()

        except Exception as e:
            logger.error(f"Error creating comprehensive geopolitical content: {e}", module="Geo", context={'e': e})
            dpg.add_text("GEOPOLITICAL ANALYSIS SYSTEM", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Initializing comprehensive intelligence systems...")

    def create_india_focused_section(self):
        """Create India-specific geopolitical analysis section"""
        with dpg.child_window(height=200, border=True):
            dpg.add_text("INDIA GEOPOLITICAL DASHBOARD - REAL-TIME ANALYSIS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            with dpg.group(horizontal=True):
                # India metrics column 1
                with dpg.child_window(width=350, height=150, border=True):
                    dpg.add_text("INDIA: KEY METRICS", color=self.BLOOMBERG_YELLOW)

                    india_data = self.countries["India"]

                    with dpg.group(horizontal=True):
                        dpg.add_text("GDP:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${india_data['gdp_trillion']:.1f}T", color=self.BLOOMBERG_GREEN)
                        dpg.add_text("Population:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"{india_data['population_million']}M", color=self.BLOOMBERG_BLUE)

                    with dpg.group(horizontal=True):
                        dpg.add_text("Military Budget:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"${india_data['military_budget_billion']}B", color=self.BLOOMBERG_RED)
                        dpg.add_text("Risk Level:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(india_data['current_risk'], color=self.RISK_MEDIUM)

                    with dpg.group(horizontal=True):
                        dpg.add_text("Market Influence:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"{india_data['market_influence']}/10", color=self.BLOOMBERG_GREEN)
                        dpg.add_text("Stability:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"{india_data['political_stability']}/10", color=self.BLOOMBERG_GREEN)

                # India strategic position column 2
                with dpg.child_window(width=400, height=150, border=True):
                    dpg.add_text("STRATEGIC DEVELOPMENTS", color=self.BLOOMBERG_YELLOW)

                    dpg.add_text("✓ G20 Presidency - Global Leadership", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("✓ Quad Alliance - Indo-Pacific Security", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("✓ Digital India - Tech Hub Growth", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("⚠ LAC Tensions - China Border Issues", color=self.BLOOMBERG_ORANGE)
                    dpg.add_text("⚠ Pakistan Relations - Ongoing Tensions", color=self.BLOOMBERG_ORANGE)
                    dpg.add_text("✓ Manufacturing Hub - China Alternative", color=self.BLOOMBERG_GREEN)

                # India threat assessment column 3
                with dpg.child_window(width=350, height=150, border=True):
                    dpg.add_text("THREAT ASSESSMENT", color=self.BLOOMBERG_YELLOW)

                    india_threats = [
                        ("China Border", 7.8, "HIGH"),
                        ("Pakistan Terror", 6.9, "MEDIUM"),
                        ("Cyber Threats", 6.5, "MEDIUM"),
                        ("Energy Security", 5.8, "MEDIUM"),
                        ("Maritime Security", 5.2, "MEDIUM")
                    ]

                    for threat, level, category in india_threats:
                        threat_color = self.get_risk_color(level)
                        with dpg.group(horizontal=True):
                            dpg.add_text(f"{threat}:", color=self.BLOOMBERG_WHITE)
                            dpg.add_text(f"{level:.1f}", color=threat_color)
                            dpg.add_text(f"[{category}]", color=threat_color)

                # India opportunities column 4
                with dpg.child_window(width=350, height=150, border=True):
                    dpg.add_text("STRATEGIC OPPORTUNITIES", color=self.BLOOMBERG_YELLOW)

                    dpg.add_text("• Global Manufacturing Hub", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("• Semiconductor Production", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("• Renewable Energy Leader", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("• Defense Exports Growth", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("• IMEC Trade Corridor", color=self.BLOOMBERG_GREEN)
                    dpg.add_text("• Digital Economy Expansion", color=self.BLOOMBERG_GREEN)

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
            dpg.add_text("CONFLICTS: 15", color=self.BLOOMBERG_RED)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("SANCTIONS: 3124", color=self.BLOOMBERG_YELLOW)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("VOLATILITY: HIGH", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("INTEL: ACTIVE", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

        # Secondary metrics bar with India focus
        with dpg.group(horizontal=True):
            dpg.add_text("INDIA NIFTY:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("24,850", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | INR/USD:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("83.12", color=self.BLOOMBERG_BLUE)
            dpg.add_text(" | OIL:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("$82.30", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | GOLD:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("$2045", color=self.BLOOMBERG_YELLOW)
            dpg.add_text(" | VIX:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("16.8", color=self.BLOOMBERG_GREEN)
            dpg.add_text(" | ALERTS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("52 ACTIVE", color=self.BLOOMBERG_RED)

        dpg.add_separator()

    def create_advanced_function_keys(self):
        """Enhanced function keys with India focus"""
        with dpg.group(horizontal=True):
            function_keys = [
                "F1:INDIA", "F2:CHINA", "F3:PAKISTAN", "F4:USA",
                "F5:RUSSIA", "F6:LAC", "F7:QUAD", "F8:BRICS",
                "F9:TRADE", "F10:DEFENSE", "F11:ENERGY", "F12:CYBER"
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
                ("MILITARY", 8.1, "15 Active Conflicts", "HIGH"),
                ("ECONOMIC", 7.4, "Sanctions Expanding", "HIGH"),
                ("CYBER", 7.8, "State Cyber Warfare", "HIGH"),
                ("NUCLEAR", 7.2, "Arms Race Concerns", "HIGH"),
                ("CLIMATE", 6.4, "Resource Wars Risk", "MEDIUM"),
                ("TERROR", 5.8, "Regional Activity", "MEDIUM")
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
        """Real-time intelligence alerts with India focus"""
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
        """Market impact summary panel with India markets"""
        with dpg.child_window(width=350, height=200, border=True):
            dpg.add_text("MARKET IMPACT SUMMARY", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Impact metrics including India
            impact_data = [
                ("India Nifty 50", +0.95, "FDI Inflows Strong"),
                ("S&P 500", -1.8, "Geopolitical Risk"),
                ("Energy Sector", +3.8, "Middle East Supply"),
                ("Tech Stocks", -2.9, "US-China Tensions"),
                ("Defense", +3.2, "Global Conflicts"),
                ("Gold", +2.1, "Safe Haven"),
                ("Oil (Brent)", +2.6, "Supply Concerns"),
                ("INR", -0.1, "Stable Inflows")
            ]

            for asset, impact, reason in impact_data:
                impact_color = self.BLOOMBERG_GREEN if impact > 0 else self.BLOOMBERG_RED
                impact_sign = "+" if impact > 0 else ""

                with dpg.group(horizontal=True):
                    dpg.add_text(f"{asset}:", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(f"{impact_sign}{impact:.1f}%", color=impact_color)

                dpg.add_text(f"  {reason}", color=self.BLOOMBERG_GRAY, wrap=300)

    def create_comprehensive_risk_panel(self):
        """Comprehensive risk assessment panel with India focus"""
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
                                dpg.add_text(f"${data['military_budget_billion']:.0f}B")
                                dpg.add_text(f"${data['gdp_trillion']:.1f}T")

                # Regional Analysis
                with dpg.tab(label="Regional"):
                    dpg.add_text("REGIONAL HOTSPOT ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    regional_hotspots = [
                        ("Eastern Europe", "CRITICAL", "Ukraine war year 3", 9.0),
                        ("Indo-Pacific", "HIGH", "China assertiveness", 8.3),
                        ("Middle East", "HIGH", "Multiple conflicts", 8.0),
                        ("South Asia", "HIGH", "India-Pakistan-China", 7.6),
                        ("South China Sea", "HIGH", "Maritime disputes", 7.8),
                        ("Korean Peninsula", "MEDIUM", "Nuclear standoff", 6.5),
                        ("Sahel Region", "MEDIUM", "Terrorism & coups", 6.2),
                        ("Arctic", "LOW", "Resource competition", 4.1)
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
                            ("USA", 842, 5244, 11, 750),
                            ("China", 296, 410, 3, 5),
                            ("Russia", 109, 5889, 1, 21),
                            ("India", 83.6, 172, 2, 12),
                            ("UK", 68, 225, 2, 16),
                            ("France", 59, 290, 1, 12)
                        ]

                        for country, budget, nuclear, carriers, bases in military_data:
                            with dpg.table_row():
                                dpg.add_text(country[:8])
                                dpg.add_text(f"${budget:.0f}")
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
                                impact_color_val = self.BLOOMBERG_RED if event['market_impact'] < 0 else self.BLOOMBERG_GREEN
                                dpg.add_text(f"Market Impact: {event['market_impact']:+.1f}%",
                                             color=impact_color_val)

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

                # Intelligence Analysis
                with dpg.tab(label="Intelligence"):
                    dpg.add_text("INTELLIGENCE ANALYSIS", color=self.BLOOMBERG_ORANGE)

                    # Intelligence source breakdown
                    intel_sources = [
                        ("SIGINT", "Signals Intelligence", 24, "Electronic intercepts"),
                        ("HUMINT", "Human Intelligence", 18, "Agent networks"),
                        ("OSINT", "Open Source", 35, "Public analysis"),
                        ("GEOINT", "Geospatial Intelligence", 12, "Satellite imagery"),
                        ("CYBINT", "Cyber Intelligence", 11, "Digital threats")
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
                            "name": "Status Quo", "probability": 42, "description": "Current tensions persist",
                            "market_impact": -1.8, "timeline": "6-12 months",
                            "key_factors": ["Diplomatic gridlock", "Economic pressure", "Limited conflicts"]
                        },
                        {
                            "name": "Escalation", "probability": 35, "description": "Major conflict expansion",
                            "market_impact": -9.2, "timeline": "3-6 months",
                            "key_factors": ["Military escalation", "Alliance activation", "Economic warfare"]
                        },
                        {
                            "name": "De-escalation", "probability": 18, "description": "Peace breakthroughs",
                            "market_impact": +4.1, "timeline": "12+ months",
                            "key_factors": ["Diplomatic progress", "Economic cooperation", "Power shifts"]
                        },
                        {
                            "name": "Black Swan", "probability": 5, "description": "Catastrophic event",
                            "market_impact": -18.7, "timeline": "Immediate",
                            "key_factors": ["Nuclear incident", "Major cyber attack", "Pandemic"]
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
                        ("Technology", 8.4, "Chip war impact", ["Semiconductors", "Hardware", "Software"]),
                        ("Energy", 8.9, "Geopolitical supply", ["Oil", "Gas", "Renewables"]),
                        ("Defense", 7.8, "Rising budgets", ["Aerospace", "Weapons", "Cyber"]),
                        ("Financials", 7.1, "Sanctions regime", ["Banking", "Insurance", "Payments"]),
                        ("Materials", 8.0, "Trade disruption", ["Metals", "Mining", "Chemicals"]),
                        ("Healthcare", 5.2, "Supply chains", ["Pharma", "Devices", "Services"]),
                        ("Consumer", 5.9, "Economic impact", ["Discretionary", "Staples", "Retail"]),
                        ("Utilities", 6.4, "Infrastructure", ["Power", "Water", "Telecom"])
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
                        ("INR", +0.8, "Strong FDI inflows", "Manufacturing hub"),
                        ("USD", +1.4, "Safe haven demand", "Fed policy support"),
                        ("EUR", -2.3, "Energy crisis", "Recession risks"),
                        ("JPY", +0.6, "Risk-off flows", "BOJ policy shift"),
                        ("GBP", -1.1, "Economic weakness", "Post-Brexit issues"),
                        ("CNY", -2.9, "Capital outflows", "Property crisis"),
                        ("RUB", -15.3, "Sanctions impact", "War economy"),
                        ("CHF", +1.9, "Safe haven", "Neutral status"),
                        ("CAD", +0.4, "Commodity strength", "Energy exports"),
                        ("BRL", -2.2, "Political risk", "EM volatility")
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

                # Base risk trend
                base_risk = []
                for i in range(90):
                    seasonal = 0.5 * math.sin(i * 0.1)
                    trend = 7.2 + 0.015 * i
                    noise = random.uniform(-0.4, 0.4)
                    base_risk.append(max(0, min(10, trend + seasonal + noise)))

                dpg.add_line_series(x_data, base_risk, label="Base Risk Forecast", parent=y_axis)

                # Conflict escalation scenario
                conflict_risk = []
                for i in range(90):
                    base = base_risk[i]
                    if 20 <= i <= 35:
                        event_impact = 2.5 * (1 - abs(i - 27.5) / 7.5)
                        base += event_impact
                    conflict_risk.append(max(0, min(10, base)))

                dpg.add_line_series(x_data, conflict_risk, label="Conflict Escalation", parent=y_axis)

                # Economic crisis scenario
                economic_risk = []
                for i in range(90):
                    base = base_risk[i]
                    if 45 <= i <= 75:
                        crisis_impact = 1.8 * math.sin((i - 45) * math.pi / 30)
                        base += crisis_impact
                    economic_risk.append(max(0, min(10, base)))

                dpg.add_line_series(x_data, economic_risk, label="Economic Crisis", parent=y_axis)

    def create_sanctions_monitoring_panel(self):
        """Comprehensive sanctions monitoring"""
        with dpg.child_window(width=450, height=250, border=True):
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
                dpg.add_text(f"• {change}", color=self.BLOOMBERG_WHITE, wrap=480)

    def create_trade_route_analysis_panel(self):
        """Trade route security analysis with India focus"""
        with dpg.child_window(width=450, height=250, border=True):
            dpg.add_text("CRITICAL TRADE ROUTE ANALYSIS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Trade route status table
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True):
                dpg.add_table_column(label="Route")
                dpg.add_table_column(label="Status")
                dpg.add_table_column(label="Daily Value")
                dpg.add_table_column(label="India Impact")

                for route, data in self.trade_routes.items():
                    status_color = (self.RISK_HIGH if data["status"] == "HIGH RISK" else
                                    self.RISK_MEDIUM if data["status"] == "MEDIUM RISK" else
                                    self.RISK_LOW if data["status"] == "LOW RISK" else
                                    self.BLOOMBERG_GREEN)

                    with dpg.table_row():
                        dpg.add_text(route[:15])
                        dpg.add_text(data["status"][:8], color=status_color)

                        # Format daily value
                        if "daily_oil_mbpd" in data:
                            dpg.add_text(f"{data['daily_oil_mbpd']:.1f}mbpd")
                        else:
                            dpg.add_text(f"${data['daily_trade_billions']:.1f}B")

                        dpg.add_text(data.get("india_exposure", "N/A")[:12])

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

                    for indicator, data in list(self.economic_indicators.items())[:8]:
                        change_color = (self.BLOOMBERG_GREEN if data["change"] > 0 else
                                        self.BLOOMBERG_RED if data["change"] < 0 else self.BLOOMBERG_GRAY)

                        impact_color = (self.RISK_CRITICAL if data["impact"] == "Critical" else
                                        self.RISK_HIGH if data["impact"] == "High" else self.RISK_MEDIUM)

                        with dpg.table_row():
                            dpg.add_text(indicator[:15])
                            dpg.add_text(f"{data['current']:.1f}")

                            change_sign = "+" if data["change"] > 0 else ""
                            dpg.add_text(f"{change_sign}{data['change']:.1f}", color=change_color)
                            dpg.add_text(data["impact"][:4], color=impact_color)

            # Nuclear threat assessment
            with dpg.child_window(width=350, height=150, border=True):
                dpg.add_text("NUCLEAR THREAT ASSESSMENT", color=self.BLOOMBERG_ORANGE)

                nuclear_powers = [
                    ("Russia", 5889, "Deployed: ~1600"),
                    ("USA", 5244, "Deployed: ~1670"),
                    ("China", 410, "Deployed: ~250"),
                    ("France", 290, "Deployed: 280"),
                    ("UK", 225, "Deployed: 120"),
                    ("Pakistan", 170, "Deployed: ~110"),
                    ("India", 172, "Deployed: ~100"),
                    ("Israel", 90, "Undeclared"),
                    ("N. Korea", 30, "Estimated")
                ]

                for country, total, status in nuclear_powers[:7]:
                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{country[:8]}:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"{total}", color=self.BLOOMBERG_RED)
                    dpg.add_text(f"  {status}", color=self.BLOOMBERG_GRAY)

            # Cyber threat landscape
            with dpg.child_window(width=400, height=150, border=True):
                dpg.add_text("CYBER THREAT LANDSCAPE", color=self.BLOOMBERG_ORANGE)

                cyber_threats = [
                    ("State APTs", "CRITICAL", "48 active groups"),
                    ("Financial Sector", "HIGH", "Banking targeted"),
                    ("Infrastructure", "HIGH", "Power/water grids"),
                    ("India Digital", "HIGH", "UPI/Aadhaar threats"),
                    ("Supply Chain", "MEDIUM", "Software attacks"),
                    ("Ransomware", "MEDIUM", "Criminal gangs")
                ]

                for threat_type, level, description in cyber_threats:
                    level_color = (self.RISK_CRITICAL if level == "CRITICAL" else
                                   self.RISK_HIGH if level == "HIGH" else
                                   self.RISK_MEDIUM if level == "MEDIUM" else self.RISK_LOW)

                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{threat_type}:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"[{level}]", color=level_color)
                    dpg.add_text(f"  {description}", color=self.BLOOMBERG_GRAY, wrap=350)

            # India-specific strategic position
            with dpg.child_window(width=350, height=150, border=True):
                dpg.add_text("INDIA STRATEGIC POSITION", color=self.BLOOMBERG_ORANGE)

                india_strategic = [
                    ("Quad Alliance", "ACTIVE", "Indo-Pacific security"),
                    ("BRICS Expansion", "MEMBER", "Economic bloc"),
                    ("IMEC Corridor", "DEVELOPING", "Trade alternative"),
                    ("LAC Border", "TENSE", "China standoff"),
                    ("Maritime Power", "GROWING", "Indian Ocean"),
                    ("Defense Exports", "RISING", "Global reach")
                ]

                for position, status, description in india_strategic:
                    status_color = (self.BLOOMBERG_GREEN if status in ["ACTIVE", "MEMBER", "GROWING", "RISING"] else
                                   self.BLOOMBERG_YELLOW if status == "DEVELOPING" else
                                   self.BLOOMBERG_ORANGE)

                    with dpg.group(horizontal=True):
                        dpg.add_text(f"{position}:", color=self.BLOOMBERG_WHITE)
                        dpg.add_text(f"[{status}]", color=status_color)
                    dpg.add_text(f"  {description}", color=self.BLOOMBERG_GRAY, wrap=300)

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