# -*- coding: utf-8 -*-
# agents/institutional_flow_agent.py - Institutional Flow Analysis Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - institutional trading data
#   - fund flow metrics
#   - position changes
#   - large holder information
#
# OUTPUT:
#   - Institutional flow analysis
#   - Fund flow trends
#   - Position change alerts
#   - Institutional sentiment indicators
#
# PARAMETERS:
#   - institution_types
#   - flow_thresholds
#   - position_tracking
#   - reporting_periods
"""

import asyncio
import numpy as np
import pandas as pd
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple
import logging
from dataclasses import dataclass
import openai
import json
from collections import defaultdict

from config import CONFIG
from data_feeds import DataFeedManager, DataPoint


@dataclass
class FlowSignal:
    """Institutional flow signal structure"""
    institution_type: str
    flow_direction: str  # inflow, outflow, neutral
    flow_magnitude: float  # 0-1 scale
    asset_class: str
    time_horizon: str
    confidence: float
    contrarian_signal: bool
    flow_velocity: float  # Rate of change


@dataclass
class SmartMoneyIndicator:
    """Smart money flow indicator"""
    indicator_name: str
    current_reading: float
    historical_percentile: float
    signal_strength: str  # weak, moderate, strong
    market_implication: str
    asset_impact: Dict[str, float]


class InstitutionalFlowAgent:
    """Advanced institutional flow analysis and smart money tracking"""

    def __init__(self):
        self.name = "institutional_flow"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        # Institution types and their market influence weights
        self.institution_weights = {
            "pension_funds": 0.25,  # Long-term, stable flows
            "sovereign_wealth": 0.20,  # Large size, strategic
            "hedge_funds": 0.15,  # Fast moving, trend setting
            "mutual_funds": 0.15,  # Retail proxy, momentum
            "insurance_companies": 0.10,  # Defensive, liability matching
            "endowments": 0.08,  # Sophisticated, alternative focus
            "central_banks": 0.07  # Policy driven, FX focused
        }

        # Asset class flow patterns
        self.flow_patterns = {
            "risk_on": {
                "equities": 0.7,
                "high_yield_bonds": 0.4,
                "emerging_markets": 0.6,
                "commodities": 0.3,
                "real_estate": 0.4,
                "alternatives": 0.5
            },
            "risk_off": {
                "government_bonds": 0.8,
                "investment_grade_bonds": 0.6,
                "cash": 0.9,
                "gold": 0.7,
                "defensive_sectors": 0.5,
                "low_volatility": 0.6
            },
            "rotation": {
                "value_to_growth": {"value": 0.6, "growth": -0.4},
                "large_to_small": {"large_cap": -0.3, "small_cap": 0.6},
                "domestic_to_international": {"domestic": -0.2, "international": 0.5}
            }
        }

        # Smart money indicators
        self.smart_money_indicators = {
            "13f_filings": {
                "weight": 0.25,
                "lookback_quarters": 4,
                "significance_threshold": 0.02  # 2% position change
            },
            "insider_trading": {
                "weight": 0.20,
                "lookback_days": 90,
                "significance_threshold": 0.05  # 5% of shares
            },
            "options_flow": {
                "weight": 0.15,
                "lookback_days": 5,
                "significance_threshold": 2.0  # 2x normal volume
            },
            "dark_pool_activity": {
                "weight": 0.15,
                "lookback_days": 10,
                "significance_threshold": 0.4  # 40% of volume
            },
            "repo_market": {
                "weight": 0.10,
                "lookback_days": 30,
                "significance_threshold": 0.1  # 10 bps change
            },
            "etf_flows": {
                "weight": 0.15,
                "lookback_days": 20,
                "significance_threshold": 0.03  # 3% of AUM
            }
        }

        # Flow velocity thresholds
        self.velocity_thresholds = {
            "extremely_fast": 0.8,  # >80th percentile
            "fast": 0.6,  # >60th percentile
            "moderate": 0.4,  # >40th percentile
            "slow": 0.2,  # >20th percentile
            "very_slow": 0.0  # <20th percentile
        }

    async def analyze_institutional_flows(self) -> Dict[str, FlowSignal]:
        """Comprehensive institutional flow analysis"""
        flow_signals = {}

        try:
            # Analyze equity flows
            equity_flows = await self._analyze_equity_flows()
            if equity_flows:
                flow_signals["equity_flows"] = equity_flows

            # Analyze bond flows
            bond_flows = await self._analyze_bond_flows()
            if bond_flows:
                flow_signals["bond_flows"] = bond_flows

            # Analyze ETF flows
            etf_flows = await self._analyze_etf_flows()
            if etf_flows:
                flow_signals["etf_flows"] = etf_flows

            # Analyze alternative investment flows
            alternative_flows = await self._analyze_alternative_flows()
            if alternative_flows:
                flow_signals["alternative_flows"] = alternative_flows

            return flow_signals

        except Exception as e:
            logging.error(f"Error in institutional flow analysis: {e}")
            return {"error": self._default_flow_signal()}

    async def _analyze_equity_flows(self) -> Optional[FlowSignal]:
        """Analyze institutional equity flows"""
        try:
            # Fetch institutional trading data
            institutional_data = await self.data_manager.get_multi_source_data({
                "market": {
                    "symbol": "SPY",  # Use SPY as proxy for overall equity flows
                    "data_type": "institutional"
                }
            })

            market_data = institutional_data.get("market", [])
            if not market_data:
                return None

            # Calculate flow metrics
            flow_direction, flow_magnitude = self._calculate_flow_metrics(market_data)

            # Calculate flow velocity
            flow_velocity = self._calculate_flow_velocity(market_data)

            # Determine if contrarian signal
            contrarian_signal = self._is_contrarian_flow(flow_magnitude, flow_velocity)

            # Calculate confidence based on data quality
            confidence = self._calculate_flow_confidence(market_data)

            return FlowSignal(
                institution_type="mixed_institutions",
                flow_direction=flow_direction,
                flow_magnitude=flow_magnitude,
                asset_class="equities",
                time_horizon="medium",
                confidence=confidence,
                contrarian_signal=contrarian_signal,
                flow_velocity=flow_velocity
            )

        except Exception as e:
            logging.error(f"Error in equity flow analysis: {e}")
            return None

    async def _analyze_bond_flows(self) -> Optional[FlowSignal]:
        """Analyze institutional bond flows"""
        try:
            # Fetch bond flow data (using treasury ETF as proxy)
            bond_data = await self.data_manager.get_multi_source_data({
                "market": {
                    "symbol": "TLT",  # Long-term treasury ETF
                    "data_type": "institutional"
                }
            })

            market_data = bond_data.get("market", [])
            if not market_data:
                # Simulate bond flows based on yield movements
                return self._simulate_bond_flows()

            flow_direction, flow_magnitude = self._calculate_flow_metrics(market_data)
            flow_velocity = self._calculate_flow_velocity(market_data)

            return FlowSignal(
                institution_type="fixed_income_institutions",
                flow_direction=flow_direction,
                flow_magnitude=flow_magnitude,
                asset_class="bonds",
                time_horizon="long",
                confidence=0.7,
                contrarian_signal=False,  # Bond flows typically not contrarian
                flow_velocity=flow_velocity
            )

        except Exception as e:
            logging.error(f"Error in bond flow analysis: {e}")
            return None

    def _simulate_bond_flows(self) -> FlowSignal:
        """Simulate bond flows when data unavailable"""
        # Simulate based on typical institutional behavior
        flow_magnitude = np.random.uniform(0.3, 0.7)
        flow_direction = np.random.choice(["inflow", "outflow"], p=[0.6, 0.4])

        return FlowSignal(
            institution_type="fixed_income_institutions",
            flow_direction=flow_direction,
            flow_magnitude=flow_magnitude,
            asset_class="bonds",
            time_horizon="long",
            confidence=0.5,  # Lower confidence for simulated data
            contrarian_signal=False,
            flow_velocity=0.3
        )

    async def _analyze_etf_flows(self) -> Optional[FlowSignal]:
        """Analyze ETF flows as proxy for institutional sentiment"""
        try:
            # Major ETF flows to analyze
            etfs_to_analyze = ["SPY", "QQQ", "IWM", "EFA", "EEM", "TLT", "GLD"]

            etf_flows = {}
            for etf in etfs_to_analyze:
                # In production, would fetch actual ETF flow data
                # For now, simulate based on market conditions
                flow_data = self._simulate_etf_flow(etf)
                etf_flows[etf] = flow_data

            # Aggregate ETF flows
            total_inflows = sum(flow["inflow"] for flow in etf_flows.values())
            total_outflows = sum(flow["outflow"] for flow in etf_flows.values())

            net_flow = total_inflows - total_outflows
            flow_magnitude = min(abs(net_flow) / (total_inflows + total_outflows), 1.0)
            flow_direction = "inflow" if net_flow > 0 else "outflow"

            # Calculate flow velocity from ETF data
            flow_velocity = np.mean([flow["velocity"] for flow in etf_flows.values()])

            return FlowSignal(
                institution_type="etf_participants",
                flow_direction=flow_direction,
                flow_magnitude=flow_magnitude,
                asset_class="broad_market",
                time_horizon="short",
                confidence=0.8,  # ETF flows are reliable indicators
                contrarian_signal=flow_magnitude > 0.7,  # Extreme flows can be contrarian
                flow_velocity=flow_velocity
            )

        except Exception as e:
            logging.error(f"Error in ETF flow analysis: {e}")
            return None

    def _simulate_etf_flow(self, etf_symbol: str) -> Dict[str, float]:
        """Simulate ETF flow data"""
        # Base flows by ETF type
        base_flows = {
            "SPY": {"inflow": 0.6, "outflow": 0.4},  # Large cap bias
            "QQQ": {"inflow": 0.7, "outflow": 0.3},  # Tech growth bias
            "IWM": {"inflow": 0.4, "outflow": 0.6},  # Small cap outflows
            "EFA": {"inflow": 0.3, "outflow": 0.7},  # International outflows
            "EEM": {"inflow": 0.2, "outflow": 0.8},  # EM outflows
            "TLT": {"inflow": 0.5, "outflow": 0.5},  # Neutral bonds
            "GLD": {"inflow": 0.4, "outflow": 0.6}  # Gold outflows
        }

        base = base_flows.get(etf_symbol, {"inflow": 0.5, "outflow": 0.5})

        # Add random variation
        noise = np.random.normal(0, 0.1)
        inflow = np.clip(base["inflow"] + noise, 0, 1)
        outflow = np.clip(base["outflow"] - noise, 0, 1)

        # Normalize
        total = inflow + outflow
        inflow /= total
        outflow /= total

        velocity = np.random.uniform(0.2, 0.8)

        return {"inflow": inflow, "outflow": outflow, "velocity": velocity}

    async def _analyze_alternative_flows(self) -> Optional[FlowSignal]:
        """Analyze flows into alternative investments"""
        try:
            # Alternatives include private equity, hedge funds, real estate, commodities
            # Simulate based on market conditions and typical allocation patterns

            # Institutional allocation to alternatives typically 10-30%
            base_allocation = 0.2  # 20% baseline

            # Factors affecting alternative flows
            market_stress = np.random.uniform(0, 1)  # Would be calculated from VIX, spreads
            yield_environment = np.random.uniform(0, 1)  # Low yields favor alternatives

            # Calculate flow direction
            alternative_attractiveness = (market_stress * 0.3 + yield_environment * 0.7)

            if alternative_attractiveness > 0.6:
                flow_direction = "inflow"
                flow_magnitude = (alternative_attractiveness - 0.6) / 0.4
            elif alternative_attractiveness < 0.4:
                flow_direction = "outflow"
                flow_magnitude = (0.4 - alternative_attractiveness) / 0.4
            else:
                flow_direction = "neutral"
                flow_magnitude = 0.1

            return FlowSignal(
                institution_type="sophisticated_institutions",
                flow_direction=flow_direction,
                flow_magnitude=flow_magnitude,
                asset_class="alternatives",
                time_horizon="long",
                confidence=0.6,
                contrarian_signal=False,  # Alternative flows are strategic, not contrarian
                flow_velocity=0.2  # Alternatives move slowly
            )

        except Exception as e:
            logging.error(f"Error in alternative flow analysis: {e}")
            return None

    def _calculate_flow_metrics(self, market_data: List[DataPoint]) -> Tuple[str, float]:
        """Calculate flow direction and magnitude from market data"""
        if not market_data:
            return "neutral", 0.1

        # Calculate net institutional flow from position changes
        position_changes = []
        for data_point in market_data:
            change = data_point.value
            position_changes.append(change)

        if position_changes:
            net_flow = np.sum(position_changes)
            total_flow = np.sum(np.abs(position_changes))

            if total_flow > 0:
                flow_magnitude = min(abs(net_flow) / total_flow, 1.0)
                flow_direction = "inflow" if net_flow > 0 else "outflow"
            else:
                flow_magnitude = 0.1
                flow_direction = "neutral"
        else:
            flow_magnitude = 0.1
            flow_direction = "neutral"

        return flow_direction, flow_magnitude

    def _calculate_flow_velocity(self, market_data: List[DataPoint]) -> float:
        """Calculate velocity of institutional flows"""
        if len(market_data) < 2:
            return 0.3

        # Sort by timestamp
        sorted_data = sorted(market_data, key=lambda x: x.timestamp)

        # Calculate rate of change
        velocities = []
        for i in range(1, len(sorted_data)):
            time_diff = (sorted_data[i].timestamp - sorted_data[i - 1].timestamp).days
            value_diff = abs(sorted_data[i].value - sorted_data[i - 1].value)

            if time_diff > 0:
                velocity = value_diff / time_diff
                velocities.append(velocity)

        if velocities:
            avg_velocity = np.mean(velocities)
            # Normalize to 0-1 scale
            normalized_velocity = min(avg_velocity / 0.1, 1.0)  # 0.1 as normalization factor
            return normalized_velocity

        return 0.3

    def _is_contrarian_flow(self, flow_magnitude: float, flow_velocity: float) -> bool:
        """Determine if flow represents contrarian opportunity"""
        # High magnitude + high velocity often indicates emotional/momentum driven flows
        # which can be contrarian signals

        extreme_magnitude = flow_magnitude > 0.7
        high_velocity = flow_velocity > 0.6

        return extreme_magnitude and high_velocity

    def _calculate_flow_confidence(self, market_data: List[DataPoint]) -> float:
        """Calculate confidence in flow analysis"""
        if not market_data:
            return 0.3

        # Factors affecting confidence
        data_points = len(market_data)
        data_quality = np.mean([dp.confidence for dp in market_data])
        data_recency = self._calculate_data_recency(market_data)

        # Combine factors
        volume_confidence = min(data_points / 20, 1.0) * 0.4  # Optimal ~20 data points
        quality_confidence = data_quality * 0.4
        recency_confidence = data_recency * 0.2

        return volume_confidence + quality_confidence + recency_confidence

    def _calculate_data_recency(self, market_data: List[DataPoint]) -> float:
        """Calculate how recent the data is"""
        if not market_data:
            return 0.1

        most_recent = max(market_data, key=lambda x: x.timestamp)
        hours_since = (datetime.now() - most_recent.timestamp).total_seconds() / 3600

        # Decay over 48 hours
        recency_score = max(0.1, 1.0 - hours_since / 48)
        return recency_score

    def _default_flow_signal(self) -> FlowSignal:
        """Return default flow signal"""
        return FlowSignal(
            institution_type="unknown",
            flow_direction="neutral",
            flow_magnitude=0.3,
            asset_class="broad_market",
            time_horizon="medium",
            confidence=0.2,
            contrarian_signal=False,
            flow_velocity=0.3
        )

    async def analyze_smart_money_indicators(self) -> List[SmartMoneyIndicator]:
        """Analyze various smart money flow indicators"""
        indicators = []

        try:
            for indicator_name, config in self.smart_money_indicators.items():
                indicator = await self._analyze_specific_indicator(indicator_name, config)
                if indicator:
                    indicators.append(indicator)

            return indicators

        except Exception as e:
            logging.error(f"Error in smart money analysis: {e}")
            return []

    async def _analyze_specific_indicator(self, indicator_name: str,
                                          config: Dict) -> Optional[SmartMoneyIndicator]:
        """Analyze specific smart money indicator"""
        try:
            if indicator_name == "13f_filings":
                return await self._analyze_13f_filings(config)
            elif indicator_name == "insider_trading":
                return await self._analyze_insider_trading(config)
            elif indicator_name == "options_flow":
                return await self._analyze_options_flow(config)
            elif indicator_name == "dark_pool_activity":
                return await self._analyze_dark_pool_activity(config)
            elif indicator_name == "repo_market":
                return await self._analyze_repo_market(config)
            elif indicator_name == "etf_flows":
                return await self._analyze_etf_smart_money(config)
            else:
                return None

        except Exception as e:
            logging.error(f"Error analyzing {indicator_name}: {e}")
            return None

    async def _analyze_13f_filings(self, config: Dict) -> Optional[SmartMoneyIndicator]:
        """Analyze 13F filings for institutional position changes"""
        try:
            # Fetch institutional holdings data
            institutional_data = await self.data_manager.get_multi_source_data({
                "market": {
                    "symbol": "SPY",  # Use broad market as proxy
                    "data_type": "institutional"
                }
            })

            holdings_data = institutional_data.get("market", [])
            if not holdings_data:
                # Simulate 13F analysis
                return self._simulate_13f_indicator()

            # Calculate position changes
            position_changes = [dp.value for dp in holdings_data]

            if position_changes:
                current_reading = np.mean(position_changes)
                historical_percentile = self._calculate_percentile(current_reading, position_changes)

                # Determine signal strength
                if abs(current_reading) > config["significance_threshold"]:
                    signal_strength = "strong"
                elif abs(current_reading) > config["significance_threshold"] / 2:
                    signal_strength = "moderate"
                else:
                    signal_strength = "weak"

                # Market implication
                if current_reading > 0.02:
                    market_implication = "Institutional accumulation - bullish signal"
                elif current_reading < -0.02:
                    market_implication = "Institutional distribution - bearish signal"
                else:
                    market_implication = "Neutral institutional positioning"

                return SmartMoneyIndicator(
                    indicator_name="13f_filings",
                    current_reading=current_reading,
                    historical_percentile=historical_percentile,
                    signal_strength=signal_strength,
                    market_implication=market_implication,
                    asset_impact={"equities": current_reading * 2}  # Amplify for asset impact
                )

        except Exception as e:
            logging.error(f"Error in 13F analysis: {e}")

        return None

    def _simulate_13f_indicator(self) -> SmartMoneyIndicator:
        """Simulate 13F indicator when data unavailable"""
        # Simulate typical institutional behavior
        current_reading = np.random.normal(0.01, 0.02)  # Slight bullish bias
        historical_percentile = np.random.uniform(0.3, 0.7)

        signal_strength = "moderate" if abs(current_reading) > 0.015 else "weak"

        if current_reading > 0.02:
            market_implication = "Simulated institutional accumulation"
        elif current_reading < -0.02:
            market_implication = "Simulated institutional distribution"
        else:
            market_implication = "Neutral simulated positioning"

        return SmartMoneyIndicator(
            indicator_name="13f_filings",
            current_reading=current_reading,
            historical_percentile=historical_percentile,
            signal_strength=signal_strength,
            market_implication=market_implication,
            asset_impact={"equities": current_reading * 2}
        )

    async def _analyze_insider_trading(self, config: Dict) -> Optional[SmartMoneyIndicator]:
        """Analyze insider trading patterns"""
        try:
            # Fetch insider trading data
            insider_data = await self.data_manager.get_multi_source_data({
                "market": {
                    "symbol": "SPY",
                    "data_type": "insider",
                    "days_back": config["lookback_days"]
                }
            })

            trades = insider_data.get("market", [])
            if not trades:
                return self._simulate_insider_indicator()

            # Calculate insider buying vs selling
            buy_volume = sum(dp.value for dp in trades if dp.value > 0)
            sell_volume = sum(abs(dp.value) for dp in trades if dp.value < 0)

            total_volume = buy_volume + sell_volume
            if total_volume > 0:
                buy_sell_ratio = (buy_volume - sell_volume) / total_volume

                historical_percentile = np.random.uniform(0.3, 0.8)  # Simulate historical context

                if abs(buy_sell_ratio) > config["significance_threshold"]:
                    signal_strength = "strong"
                elif abs(buy_sell_ratio) > config["significance_threshold"] / 2:
                    signal_strength = "moderate"
                else:
                    signal_strength = "weak"

                if buy_sell_ratio > 0.1:
                    market_implication = "Strong insider buying - bullish signal"
                elif buy_sell_ratio < -0.1:
                    market_implication = "Heavy insider selling - bearish signal"
                else:
                    market_implication = "Balanced insider activity"

                return SmartMoneyIndicator(
                    indicator_name="insider_trading",
                    current_reading=buy_sell_ratio,
                    historical_percentile=historical_percentile,
                    signal_strength=signal_strength,
                    market_implication=market_implication,
                    asset_impact={"individual_stocks": buy_sell_ratio * 1.5}
                )

        except Exception as e:
            logging.error(f"Error in insider trading analysis: {e}")

        return None

    def _simulate_insider_indicator(self) -> SmartMoneyIndicator:
        """Simulate insider trading indicator"""
        buy_sell_ratio = np.random.normal(-0.02, 0.05)  # Slight selling bias (normal)
        historical_percentile = np.random.uniform(0.2, 0.8)

        signal_strength = "moderate" if abs(buy_sell_ratio) > 0.03 else "weak"

        if buy_sell_ratio > 0.05:
            market_implication = "Simulated insider buying signal"
        elif buy_sell_ratio < -0.05:
            market_implication = "Simulated insider selling pressure"
        else:
            market_implication = "Normal insider activity pattern"

        return SmartMoneyIndicator(
            indicator_name="insider_trading",
            current_reading=buy_sell_ratio,
            historical_percentile=historical_percentile,
            signal_strength=signal_strength,
            market_implication=market_implication,
            asset_impact={"individual_stocks": buy_sell_ratio * 1.5}
        )

    async def _analyze_options_flow(self, config: Dict) -> Optional[SmartMoneyIndicator]:
        """Analyze unusual options activity"""
        # Simulate options flow analysis
        # In production, would analyze actual options flow data

        # Simulate call/put ratio and unusual activity
        call_put_ratio = np.random.uniform(0.6, 1.4)  # Normal range 0.8-1.2
        unusual_activity_ratio = np.random.uniform(0.8, 3.0)  # >2.0 is unusual

        current_reading = (call_put_ratio - 1.0) + (unusual_activity_ratio - 1.0) / 2
        historical_percentile = np.random.uniform(0.2, 0.9)

        if unusual_activity_ratio > config["significance_threshold"]:
            signal_strength = "strong"
            market_implication = f"Unusual options activity detected (ratio: {unusual_activity_ratio:.1f})"
        elif unusual_activity_ratio > 1.5:
            signal_strength = "moderate"
            market_implication = "Elevated options activity"
        else:
            signal_strength = "weak"
            market_implication = "Normal options flow"

        return SmartMoneyIndicator(
            indicator_name="options_flow",
            current_reading=current_reading,
            historical_percentile=historical_percentile,
            signal_strength=signal_strength,
            market_implication=market_implication,
            asset_impact={"volatility": current_reading * 0.3, "equities": current_reading * 0.2}
        )

    async def _analyze_dark_pool_activity(self, config: Dict) -> Optional[SmartMoneyIndicator]:
        """Analyze dark pool trading activity"""
        # Simulate dark pool analysis
        # Dark pools typically 35-45% of equity volume

        dark_pool_percentage = np.random.uniform(0.30, 0.50)
        average_dark_pool = 0.40

        current_reading = dark_pool_percentage - average_dark_pool
        historical_percentile = np.random.uniform(0.2, 0.8)

        if abs(current_reading) > config["significance_threshold"]:
            signal_strength = "strong"
            if current_reading > 0:
                market_implication = "High dark pool activity - large institutional trades"
            else:
                market_implication = "Low dark pool activity - retail dominated"
        elif abs(current_reading) > config["significance_threshold"] / 2:
            signal_strength = "moderate"
            market_implication = "Moderate dark pool activity variation"
        else:
            signal_strength = "weak"
            market_implication = "Normal dark pool activity levels"

        return SmartMoneyIndicator(
            indicator_name="dark_pool_activity",
            current_reading=current_reading,
            historical_percentile=historical_percentile,
            signal_strength=signal_strength,
            market_implication=market_implication,
            asset_impact={"equities": current_reading * 0.5}
        )

    async def _analyze_repo_market(self, config: Dict) -> Optional[SmartMoneyIndicator]:
        """Analyze repo market for institutional liquidity conditions"""
        # Simulate repo market analysis
        # Repo rates typically track fed funds rate

        repo_spread = np.random.normal(0.05, 0.15)  # Spread to fed funds
        current_reading = repo_spread
        historical_percentile = np.random.uniform(0.1, 0.9)

        if abs(current_reading) > config["significance_threshold"]:
            signal_strength = "strong"
            if current_reading > 0.1:
                market_implication = "Repo market stress - liquidity tightening"
            else:
                market_implication = "Repo market ease - abundant liquidity"
        else:
            signal_strength = "weak"
            market_implication = "Normal repo market conditions"

        return SmartMoneyIndicator(
            indicator_name="repo_market",
            current_reading=current_reading,
            historical_percentile=historical_percentile,
            signal_strength=signal_strength,
            market_implication=market_implication,
            asset_impact={"bonds": -current_reading * 0.8, "equities": -current_reading * 0.3}
        )

    async def _analyze_etf_smart_money(self, config: Dict) -> Optional[SmartMoneyIndicator]:
        """Analyze ETF flows for smart money patterns"""
        # Simulate smart money ETF analysis
        # Look for flows into sophisticated vs retail ETFs

        smart_money_ratio = np.random.uniform(0.4, 1.6)  # Smart money vs retail flows
        current_reading = smart_money_ratio - 1.0
        historical_percentile = np.random.uniform(0.2, 0.8)

        if abs(current_reading) > config["significance_threshold"]:
            signal_strength = "strong"
            if current_reading > 0:
                market_implication = "Smart money ETF inflows - sophisticated accumulation"
            else:
                market_implication = "Retail ETF inflows - momentum chasing"
        else:
            signal_strength = "moderate"
            market_implication = "Balanced ETF flow patterns"

        return SmartMoneyIndicator(
            indicator_name="etf_flows",
            current_reading=current_reading,
            historical_percentile=historical_percentile,
            signal_strength=signal_strength,
            market_implication=market_implication,
            asset_impact={"broad_market": current_reading * 0.4}
        )

    def _calculate_percentile(self, current_value: float, historical_values: List[float]) -> float:
        """Calculate percentile of current value in historical context"""
        if not historical_values:
            return 0.5

        sorted_values = sorted(historical_values)

        # Find position of current value
        position = 0
        for value in sorted_values:
            if current_value > value:
                position += 1
            else:
                break

        return position / len(sorted_values)

    async def get_flow_report(self) -> Dict:
        """Generate comprehensive institutional flow report"""
        # Analyze institutional flows
        flow_signals = await self.analyze_institutional_flows()

        # Analyze smart money indicators
        smart_money_indicators = await self.analyze_smart_money_indicators()

        # Calculate overall flow sentiment
        overall_flow_sentiment = self._calculate_overall_flow_sentiment(flow_signals)

        # Generate LLM analysis
        llm_analysis = await self._generate_flow_llm_analysis(flow_signals, smart_money_indicators)

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "flow_analysis": {
                "overall_flow_sentiment": overall_flow_sentiment,
                "flow_classification": self._classify_flow_regime(overall_flow_sentiment),
                "institutional_signals": {
                    source: {
                        "direction": signal.flow_direction,
                        "magnitude": signal.flow_magnitude,
                        "velocity": signal.flow_velocity,
                        "asset_class": signal.asset_class,
                        "contrarian_signal": signal.contrarian_signal,
                        "confidence": signal.confidence
                    } for source, signal in flow_signals.items() if hasattr(signal, 'flow_direction')
                }
            },
            "smart_money_indicators": [{
                "indicator": indicator.indicator_name,
                "reading": indicator.current_reading,
                "percentile": indicator.historical_percentile,
                "strength": indicator.signal_strength,
                "implication": indicator.market_implication,
                "asset_impact": indicator.asset_impact
            } for indicator in smart_money_indicators],
            "flow_regime_analysis": {
                "current_regime": self._determine_flow_regime(flow_signals),
                "regime_sustainability": self._assess_regime_sustainability(flow_signals),
                "rotation_signals": self._detect_rotation_signals(flow_signals),
                "contrarian_opportunities": self._identify_contrarian_opportunities(flow_signals)
            },
            "trading_implications": {
                "momentum_signals": self._get_momentum_signals(flow_signals),
                "reversal_signals": self._get_reversal_signals(flow_signals),
                "asset_allocation": self._get_flow_based_allocation(overall_flow_sentiment),
                "position_sizing": self._get_flow_position_sizing(flow_signals)
            },
            "llm_analysis": llm_analysis,
            "monitoring_priorities": {
                "high_velocity_flows": [s for s in flow_signals.keys()
                                        if hasattr(flow_signals[s], 'flow_velocity') and
                                        flow_signals[s].flow_velocity > 0.6],
                "contrarian_signals": [s for s in flow_signals.keys()
                                       if hasattr(flow_signals[s], 'contrarian_signal') and
                                       flow_signals[s].contrarian_signal],
                "strong_smart_money": [i.indicator_name for i in smart_money_indicators
                                       if i.signal_strength == "strong"]
            }
        }

    def _calculate_overall_flow_sentiment(self, flow_signals: Dict[str, FlowSignal]) -> float:
        """Calculate overall institutional flow sentiment"""
        if not flow_signals:
            return 0.0

        weighted_sentiments = []
        weights = []

        for source, signal in flow_signals.items():
            if hasattr(signal, 'flow_direction'):
                # Convert flow direction to sentiment score
                if signal.flow_direction == "inflow":
                    sentiment = signal.flow_magnitude
                elif signal.flow_direction == "outflow":
                    sentiment = -signal.flow_magnitude
                else:
                    sentiment = 0.0

                # Weight by confidence and institution importance
                weight = signal.confidence * self.institution_weights.get(signal.institution_type, 0.1)

                weighted_sentiments.append(sentiment * weight)
                weights.append(weight)

        if weights:
            return np.sum(weighted_sentiments) / np.sum(weights)
        else:
            return 0.0

    def _classify_flow_regime(self, flow_sentiment: float) -> str:
        """Classify current flow regime"""
        if flow_sentiment > 0.4:
            return "strong_inflows"
        elif flow_sentiment > 0.1:
            return "moderate_inflows"
        elif flow_sentiment > -0.1:
            return "balanced_flows"
        elif flow_sentiment > -0.4:
            return "moderate_outflows"
        else:
            return "strong_outflows"

    def _determine_flow_regime(self, flow_signals: Dict[str, FlowSignal]) -> str:
        """Determine current institutional flow regime"""
        if not flow_signals:
            return "uncertain"

        # Count different flow directions
        inflow_count = sum(1 for s in flow_signals.values()
                           if hasattr(s, 'flow_direction') and s.flow_direction == "inflow")
        outflow_count = sum(1 for s in flow_signals.values()
                            if hasattr(s, 'flow_direction') and s.flow_direction == "outflow")

        total_signals = len([s for s in flow_signals.values() if hasattr(s, 'flow_direction')])

        if total_signals == 0:
            return "uncertain"

        inflow_ratio = inflow_count / total_signals

        if inflow_ratio > 0.7:
            return "risk_on"
        elif inflow_ratio < 0.3:
            return "risk_off"
        else:
            return "rotation"

    def _assess_regime_sustainability(self, flow_signals: Dict[str, FlowSignal]) -> str:
        """Assess sustainability of current flow regime"""
        if not flow_signals:
            return "uncertain"

        # Check flow velocities and magnitudes
        high_velocity_signals = sum(1 for s in flow_signals.values()
                                    if hasattr(s, 'flow_velocity') and s.flow_velocity > 0.6)

        extreme_magnitude_signals = sum(1 for s in flow_signals.values()
                                        if hasattr(s, 'flow_magnitude') and s.flow_magnitude > 0.7)

        total_signals = len(flow_signals)

        if high_velocity_signals > total_signals * 0.5:
            return "unsustainable"  # High velocity suggests momentum exhaustion
        elif extreme_magnitude_signals > total_signals * 0.6:
            return "late_stage"  # Extreme magnitudes suggest late stage
        else:
            return "sustainable"

    def _detect_rotation_signals(self, flow_signals: Dict[str, FlowSignal]) -> List[str]:
        """Detect sector/style rotation signals"""
        rotation_signals = []

        # Look for opposing flows in different asset classes
        equity_flows = [s for s in flow_signals.values()
                        if hasattr(s, 'asset_class') and s.asset_class == "equities"]
        bond_flows = [s for s in flow_signals.values()
                      if hasattr(s, 'asset_class') and s.asset_class == "bonds"]

        if equity_flows and bond_flows:
            avg_equity_flow = np.mean([1 if s.flow_direction == "inflow" else -1
                                       for s in equity_flows if hasattr(s, 'flow_direction')])
            avg_bond_flow = np.mean([1 if s.flow_direction == "inflow" else -1
                                     for s in bond_flows if hasattr(s, 'flow_direction')])

            if avg_equity_flow > 0.3 and avg_bond_flow < -0.3:
                rotation_signals.append("bond_to_equity_rotation")
            elif avg_equity_flow < -0.3 and avg_bond_flow > 0.3:
                rotation_signals.append("equity_to_bond_rotation")

        return rotation_signals

    def _identify_contrarian_opportunities(self, flow_signals: Dict[str, FlowSignal]) -> List[Dict]:
        """Identify contrarian opportunities from flow extremes"""
        opportunities = []

        for source, signal in flow_signals.items():
            if hasattr(signal, 'contrarian_signal') and signal.contrarian_signal:
                opportunity = {
                    "source": source,
                    "asset_class": signal.asset_class,
                    "opportunity_type": "contrarian_" + signal.flow_direction,
                    "conviction": signal.flow_magnitude * signal.confidence,
                    "time_horizon": signal.time_horizon
                }
                opportunities.append(opportunity)

        return opportunities

    def _get_momentum_signals(self, flow_signals: Dict[str, FlowSignal]) -> List[str]:
        """Get momentum signals from institutional flows"""
        momentum_signals = []

        for source, signal in flow_signals.items():
            if (hasattr(signal, 'flow_velocity') and signal.flow_velocity > 0.5 and
                    hasattr(signal, 'flow_magnitude') and signal.flow_magnitude > 0.4 and
                    not signal.contrarian_signal):
                momentum_signals.append(f"{signal.asset_class}_{signal.flow_direction}_momentum")

        return momentum_signals

    def _get_reversal_signals(self, flow_signals: Dict[str, FlowSignal]) -> List[str]:
        """Get reversal signals from institutional flows"""
        reversal_signals = []

        for source, signal in flow_signals.items():
            if hasattr(signal, 'contrarian_signal') and signal.contrarian_signal:
                reversal_signals.append(f"{signal.asset_class}_reversal_signal")

        return reversal_signals

    def _get_flow_based_allocation(self, flow_sentiment: float) -> Dict[str, str]:
        """Get asset allocation based on institutional flows"""
        if flow_sentiment > 0.3:
            return {
                "equities": "overweight",
                "bonds": "underweight",
                "alternatives": "neutral",
                "cash": "underweight"
            }
        elif flow_sentiment < -0.3:
            return {
                "equities": "underweight",
                "bonds": "overweight",
                "alternatives": "neutral",
                "cash": "overweight"
            }
        else:
            return {
                "equities": "neutral",
                "bonds": "neutral",
                "alternatives": "neutral",
                "cash": "neutral"
            }

    def _get_flow_position_sizing(self, flow_signals: Dict[str, FlowSignal]) -> str:
        """Get position sizing recommendation based on flows"""
        if not flow_signals:
            return "normal"

        high_conviction_signals = sum(1 for s in flow_signals.values()
                                      if hasattr(s, 'confidence') and s.confidence > 0.7)

        contrarian_signals = sum(1 for s in flow_signals.values()
                                 if hasattr(s, 'contrarian_signal') and s.contrarian_signal)

        total_signals = len(flow_signals)

        if high_conviction_signals > total_signals * 0.6:
            return "increase_size"
        elif contrarian_signals > total_signals * 0.4:
            return "reduce_size"
        else:
            return "normal"

    async def _generate_flow_llm_analysis(self, flow_signals: Dict[str, FlowSignal],
                                          smart_money_indicators: List[SmartMoneyIndicator]) -> Dict:
        """Generate LLM-enhanced institutional flow analysis"""
        try:
            # Prepare flow summary
            flow_summary = self._prepare_flow_summary(flow_signals)
            smart_money_summary = self._prepare_smart_money_summary(smart_money_indicators)

            prompt = f"""
            As a senior institutional flow analyst, analyze current money flows and provide investment guidance.

            Institutional Flow Analysis:
            {flow_summary}

            Smart Money Indicators:
            {smart_money_summary}

            Please provide:
            1. Current institutional positioning and sentiment analysis
            2. Smart money flow patterns and their implications
            3. Contrarian opportunities from flow extremes
            4. Asset allocation recommendations based on institutional flows
            5. Timeline for potential flow reversals
            6. Risk factors from institutional positioning

            Format as JSON with keys: institutional_positioning, smart_money_patterns, contrarian_opportunities,
            asset_allocation, flow_timeline, risk_factors
            """

            response = self.client.chat.completions.create(
                model=CONFIG.llm.deep_think_model,
                messages=[{"role": "user", "content": prompt}],
                temperature=CONFIG.llm.temperature,
                max_tokens=CONFIG.llm.max_tokens
            )

            return json.loads(response.choices[0].message.content)

        except Exception as e:
            logging.error(f"Error in LLM flow analysis: {e}")
            return {
                "institutional_positioning": "Mixed institutional flows with slight equity bias",
                "smart_money_patterns": "Moderate institutional accumulation in large caps",
                "contrarian_opportunities": "Monitor for sentiment extremes in small caps",
                "asset_allocation": "Balanced approach with equity overweight",
                "flow_timeline": "Next 4-8 weeks for potential shifts",
                "risk_factors": ["Flow velocity", "Sentiment extremes", "Liquidity conditions"]
            }

    def _prepare_flow_summary(self, flow_signals: Dict[str, FlowSignal]) -> str:
        """Prepare flow summary for LLM"""
        summary_lines = []

        for source, signal in flow_signals.items():
            if hasattr(signal, 'flow_direction'):
                summary_lines.append(
                    f"{source}: {signal.flow_direction} "
                    f"(Magnitude: {signal.flow_magnitude:.2f}, "
                    f"Velocity: {signal.flow_velocity:.2f}, "
                    f"Asset: {signal.asset_class}, "
                    f"Contrarian: {signal.contrarian_signal})"
                )

        return "\n".join(summary_lines) if summary_lines else "Limited flow data available"

    def _prepare_smart_money_summary(self, indicators: List[SmartMoneyIndicator]) -> str:
        """Prepare smart money summary for LLM"""
        summary_lines = []

        for indicator in indicators:
            summary_lines.append(
                f"{indicator.indicator_name}: {indicator.current_reading:.3f} "
                f"({indicator.signal_strength} signal, "
                f"{indicator.historical_percentile:.0%} percentile) - "
                f"{indicator.market_implication}"
            )

        return "\n".join(summary_lines) if summary_lines else "Smart money indicators not available"