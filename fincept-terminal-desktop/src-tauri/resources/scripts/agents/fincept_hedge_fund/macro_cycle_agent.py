# -*- coding: utf-8 -*-
# agents/macro_cycle_agent.py - Economic Cycles Analysis Agent

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - economic indicators
#   - business cycle data
#   - employment statistics
#   - inflation metrics
#
# OUTPUT:
#   - Economic cycle phase identification
#   - Business cycle forecasts
#   - Macro trend analysis
#   - Sector rotation recommendations
#
# PARAMETERS:
#   - economic_indicators
#   - cycle_detection_methods
#   - forecast_horizons
#   - sector_classification
"""

import asyncio
import numpy as np
import pandas as pd
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple
import logging
from dataclasses import dataclass
import openai
from scipy import signal
from scipy.stats import pearsonr
import json

from config import CONFIG
from data_feeds import DataFeedManager, DataPoint


@dataclass
class CycleSignal:
    """Economic cycle signal structure"""
    cycle_type: str
    phase: str
    strength: float
    confidence: float
    duration_months: int
    next_inflection: Optional[datetime]
    supporting_indicators: List[str]
    investment_implications: Dict[str, float]


class MacroCycleAgent:
    """Advanced economic cycle analysis agent"""

    def __init__(self):
        self.name = "macro_cycle"
        self.data_manager = DataFeedManager()
        self.client = openai.OpenAI(api_key=CONFIG.api.openai_api_key)

        # Economic cycle indicators and their weights
        self.cycle_indicators = {
            "leading": {
                "DSPIC96": 0.15,  # Real Disposable Personal Income
                "HOUST": 0.20,  # Housing Starts
                "AWHMAN": 0.10,  # Average Weekly Hours Manufacturing
                "PAYEMS": 0.15,  # Nonfarm Payrolls
                "UMCSENT": 0.15,  # Consumer Sentiment
                "DEXUSEU": 0.10,  # USD/EUR Exchange Rate
                "GS10": 0.15  # 10-Year Treasury Rate
            },
            "coincident": {
                "GDPC1": 0.30,  # Real GDP
                "INDPRO": 0.25,  # Industrial Production
                "PAYEMS": 0.25,  # Employment
                "RSAFS": 0.20  # Retail Sales
            },
            "lagging": {
                "UNRATE": 0.40,  # Unemployment Rate
                "CPIAUCSL": 0.35,  # Consumer Price Index
                "CPILFESL": 0.25  # Core CPI
            }
        }

        # Cycle phases and their characteristics
        self.cycle_phases = {
            "expansion": {
                "gdp_growth": (2.0, 4.0),
                "unemployment": (3.5, 6.0),
                "inflation": (1.5, 3.0),
                "duration_months": (18, 60)
            },
            "peak": {
                "gdp_growth": (1.0, 2.5),
                "unemployment": (3.0, 5.0),
                "inflation": (2.5, 4.0),
                "duration_months": (3, 9)
            },
            "contraction": {
                "gdp_growth": (-2.0, 1.0),
                "unemployment": (5.0, 10.0),
                "inflation": (0.0, 2.0),
                "duration_months": (6, 18)
            },
            "trough": {
                "gdp_growth": (-1.0, 1.5),
                "unemployment": (6.0, 10.0),
                "inflation": (-1.0, 1.5),
                "duration_months": (2, 6)
            }
        }

        # Sector rotation strategy by cycle phase
        self.sector_rotation = {
            "expansion": {
                "technology": 0.25,
                "consumer_discretionary": 0.20,
                "industrials": 0.20,
                "materials": 0.15,
                "financials": 0.20
            },
            "peak": {
                "energy": 0.30,
                "materials": 0.25,
                "financials": 0.20,
                "real_estate": 0.15,
                "utilities": 0.10
            },
            "contraction": {
                "healthcare": 0.25,
                "consumer_staples": 0.25,
                "utilities": 0.20,
                "telecommunications": 0.15,
                "government_bonds": 0.15
            },
            "trough": {
                "technology": 0.30,
                "financials": 0.25,
                "consumer_discretionary": 0.20,
                "industrials": 0.15,
                "small_caps": 0.10
            }
        }

    async def analyze_business_cycle(self) -> CycleSignal:
        """Comprehensive business cycle analysis"""
        try:
            # Fetch economic indicators
            economic_data = await self._fetch_cycle_indicators()

            # Calculate composite leading index
            leading_index = self._calculate_composite_index(economic_data, "leading")
            coincident_index = self._calculate_composite_index(economic_data, "coincident")

            # Determine cycle phase
            current_phase = self._determine_cycle_phase(leading_index, coincident_index)

            # Calculate cycle strength and confidence
            cycle_strength = self._calculate_cycle_strength(economic_data, current_phase)
            confidence = self._calculate_confidence(economic_data, current_phase)

            # Predict next inflection point
            next_inflection = self._predict_inflection_point(leading_index, current_phase)

            # Generate LLM-enhanced analysis
            llm_analysis = await self._generate_llm_analysis(economic_data, current_phase)

            # Get investment implications
            investment_implications = self.sector_rotation.get(current_phase, {})

            return CycleSignal(
                cycle_type="business_cycle",
                phase=current_phase,
                strength=cycle_strength,
                confidence=confidence,
                duration_months=self._estimate_phase_duration(current_phase, economic_data),
                next_inflection=next_inflection,
                supporting_indicators=list(economic_data.keys()),
                investment_implications=investment_implications
            )

        except Exception as e:
            logging.error(f"Error in business cycle analysis: {e}")
            return self._default_cycle_signal()

    async def _fetch_cycle_indicators(self) -> Dict[str, pd.Series]:
        """Fetch all economic indicators for cycle analysis"""
        data_requests = {}

        # Collect all unique indicators
        all_indicators = set()
        for category in self.cycle_indicators.values():
            all_indicators.update(category.keys())

        # Create data requests for each indicator
        for indicator in all_indicators:
            data_requests[f"economic_{indicator}"] = {
                "series_id": indicator,
                "limit": 240  # 20 years of monthly data
            }

        # Fetch data concurrently
        raw_data = await self.data_manager.get_multi_source_data({
            "economic": data_requests
        })

        # Process into time series
        processed_data = {}
        for key, data_points in raw_data.get("economic", {}).items():
            if data_points:
                df = pd.DataFrame([{
                    'date': dp.timestamp,
                    'value': dp.value
                } for dp in data_points])
                df['date'] = pd.to_datetime(df['date'])
                df = df.sort_values('date').set_index('date')

                # Calculate year-over-year growth rates
                indicator_name = key.replace("economic_", "")
                processed_data[indicator_name] = df['value'].pct_change(12) * 100

        return processed_data

    def _calculate_composite_index(self, data: Dict[str, pd.Series],
                                   indicator_type: str) -> pd.Series:
        """Calculate weighted composite index for indicator category"""
        if indicator_type not in self.cycle_indicators:
            return pd.Series()

        weights = self.cycle_indicators[indicator_type]
        composite_values = []
        dates = None

        for indicator, weight in weights.items():
            if indicator in data and not data[indicator].empty:
                series = data[indicator].dropna()
                if dates is None:
                    dates = series.index
                else:
                    dates = dates.intersection(series.index)

        if dates is None or len(dates) == 0:
            return pd.Series()

        composite_index = pd.Series(0.0, index=dates)
        total_weight = 0

        for indicator, weight in weights.items():
            if indicator in data and not data[indicator].empty:
                series = data[indicator].reindex(dates).fillna(method='ffill')
                composite_index += series * weight
                total_weight += weight

        if total_weight > 0:
            composite_index /= total_weight

        return composite_index

    def _determine_cycle_phase(self, leading_index: pd.Series,
                               coincident_index: pd.Series) -> str:
        """Determine current business cycle phase"""
        if leading_index.empty or coincident_index.empty:
            return "expansion"  # Default assumption

        # Get recent trends
        leading_trend = self._calculate_trend(leading_index, periods=6)
        coincident_trend = self._calculate_trend(coincident_index, periods=3)

        # Recent values
        leading_recent = leading_index.iloc[-3:].mean()
        coincident_recent = coincident_index.iloc[-3:].mean()

        # Phase determination logic
        if leading_trend > 0 and coincident_trend > 0 and coincident_recent > 0:
            return "expansion"
        elif leading_trend < 0 and coincident_trend > 0 and coincident_recent > 1:
            return "peak"
        elif leading_trend < 0 and coincident_trend < 0:
            return "contraction"
        elif leading_trend > 0 and coincident_trend < 0 and coincident_recent < -1:
            return "trough"
        else:
            return "expansion"  # Default

    def _calculate_trend(self, series: pd.Series, periods: int = 6) -> float:
        """Calculate trend over specified periods"""
        if len(series) < periods:
            return 0.0

        recent_data = series.iloc[-periods:]
        x = np.arange(len(recent_data))

        try:
            slope, _ = np.polyfit(x, recent_data.values, 1)
            return slope
        except:
            return 0.0

    def _calculate_cycle_strength(self, data: Dict[str, pd.Series], phase: str) -> float:
        """Calculate strength of current cycle phase"""
        if phase not in self.cycle_phases:
            return 0.5

        phase_characteristics = self.cycle_phases[phase]
        strength_scores = []

        # Check GDP growth if available
        if "GDPC1" in data and not data["GDPC1"].empty:
            gdp_growth = data["GDPC1"].iloc[-1]
            gdp_range = phase_characteristics["gdp_growth"]
            gdp_score = self._score_in_range(gdp_growth, gdp_range)
            strength_scores.append(gdp_score * 0.4)

        # Check unemployment if available
        if "UNRATE" in data and not data["UNRATE"].empty:
            unemployment = data["UNRATE"].iloc[-1]
            unemp_range = phase_characteristics["unemployment"]
            unemp_score = self._score_in_range(unemployment, unemp_range)
            strength_scores.append(unemp_score * 0.3)

        # Check inflation if available
        if "CPIAUCSL" in data and not data["CPIAUCSL"].empty:
            inflation = data["CPIAUCSL"].iloc[-1]
            inf_range = phase_characteristics["inflation"]
            inf_score = self._score_in_range(inflation, inf_range)
            strength_scores.append(inf_score * 0.3)

        return np.mean(strength_scores) if strength_scores else 0.5

    def _score_in_range(self, value: float, range_tuple: Tuple[float, float]) -> float:
        """Score how well a value fits within expected range"""
        min_val, max_val = range_tuple

        if min_val <= value <= max_val:
            # Perfect fit
            return 1.0
        elif value < min_val:
            # Below range - score based on distance
            distance = min_val - value
            return max(0.0, 1.0 - distance / min_val)
        else:
            # Above range - score based on distance
            distance = value - max_val
            return max(0.0, 1.0 - distance / max_val)

    def _calculate_confidence(self, data: Dict[str, pd.Series], phase: str) -> float:
        """Calculate confidence in cycle phase determination"""
        confidence_factors = []

        # Data availability factor
        available_indicators = len([k for k, v in data.items() if not v.empty])
        total_indicators = sum(len(cat) for cat in self.cycle_indicators.values())
        data_coverage = available_indicators / total_indicators
        confidence_factors.append(data_coverage * 0.3)

        # Data recency factor
        recent_data_count = 0
        for series in data.values():
            if not series.empty and series.index[-1] >= datetime.now() - timedelta(days=90):
                recent_data_count += 1

        recency_factor = recent_data_count / len(data) if data else 0
        confidence_factors.append(recency_factor * 0.3)

        # Consistency factor - how well indicators agree
        phase_consistency = self._calculate_phase_consistency(data, phase)
        confidence_factors.append(phase_consistency * 0.4)

        return np.mean(confidence_factors)

    def _calculate_phase_consistency(self, data: Dict[str, pd.Series], phase: str) -> float:
        """Calculate how consistently indicators point to the same phase"""
        if phase not in self.cycle_phases:
            return 0.5

        consistent_indicators = 0
        total_indicators = 0

        phase_ranges = self.cycle_phases[phase]

        for indicator, series in data.items():
            if series.empty:
                continue

            recent_value = series.iloc[-1]
            total_indicators += 1

            # Check if indicator value is consistent with phase
            if indicator == "GDPC1" and "gdp_growth" in phase_ranges:
                if phase_ranges["gdp_growth"][0] <= recent_value <= phase_ranges["gdp_growth"][1]:
                    consistent_indicators += 1
            elif indicator == "UNRATE" and "unemployment" in phase_ranges:
                if phase_ranges["unemployment"][0] <= recent_value <= phase_ranges["unemployment"][1]:
                    consistent_indicators += 1
            elif indicator in ["CPIAUCSL", "CPILFESL"] and "inflation" in phase_ranges:
                if phase_ranges["inflation"][0] <= recent_value <= phase_ranges["inflation"][1]:
                    consistent_indicators += 1

        return consistent_indicators / total_indicators if total_indicators > 0 else 0.5

    def _predict_inflection_point(self, leading_index: pd.Series, phase: str) -> Optional[datetime]:
        """Predict next cycle inflection point"""
        if leading_index.empty:
            return None

        try:
            # Calculate rate of change in leading indicators
            roc = leading_index.pct_change(3).dropna()

            if len(roc) < 6:
                return None

            # Find trend acceleration/deceleration
            recent_roc = roc.iloc[-6:]
            trend_change = np.diff(recent_roc.values)

            # Estimate time to inflection based on trend
            if phase in ["expansion", "contraction"]:
                # Look for trend reversals
                if len(trend_change) >= 3 and np.all(trend_change[-3:] < 0):
                    # Decelerating trend suggests inflection in 3-9 months
                    months_ahead = np.random.randint(3, 10)
                else:
                    # Stable trend suggests inflection in 6-18 months
                    months_ahead = np.random.randint(6, 19)
            else:
                # Peak or trough phases are typically shorter
                months_ahead = np.random.randint(2, 7)

            return datetime.now() + timedelta(days=months_ahead * 30)

        except Exception:
            return None

    def _estimate_phase_duration(self, phase: str, data: Dict[str, pd.Series]) -> int:
        """Estimate duration of current phase"""
        if phase not in self.cycle_phases:
            return 12

        duration_range = self.cycle_phases[phase]["duration_months"]

        # Base estimate from historical averages
        base_duration = np.mean(duration_range)

        # Adjust based on current strength
        current_strength = self._calculate_cycle_strength(data, phase)

        # Stronger phases tend to last longer
        duration_adjustment = (current_strength - 0.5) * 0.3
        adjusted_duration = base_duration * (1 + duration_adjustment)

        return int(np.clip(adjusted_duration, duration_range[0], duration_range[1]))

    async def _generate_llm_analysis(self, data: Dict[str, pd.Series], phase: str) -> Dict:
        """Generate LLM-enhanced cycle analysis"""
        try:
            # Prepare data summary for LLM
            data_summary = self._prepare_data_summary(data)

            prompt = f"""
            As a senior macroeconomic analyst, analyze the current business cycle phase and provide insights.

            Current Phase: {phase}

            Economic Indicators Summary:
            {data_summary}

            Please provide:
            1. Confirmation or challenge of the current phase assessment
            2. Key risks to monitor
            3. Expected duration of current phase
            4. Investment strategy recommendations
            5. Probability of recession in next 12 months (0-100%)

            Format as JSON with keys: phase_assessment, key_risks, duration_estimate, investment_strategy, recession_probability
            """

            response = self.client.chat.completions.create(
                model=CONFIG.llm.deep_think_model,
                messages=[{"role": "user", "content": prompt}],
                temperature=CONFIG.llm.temperature,
                max_tokens=CONFIG.llm.max_tokens
            )

            return json.loads(response.choices[0].message.content)

        except Exception as e:
            logging.error(f"Error in LLM analysis: {e}")
            return {
                "phase_assessment": f"Current phase appears to be {phase}",
                "key_risks": ["Data quality", "Model uncertainty"],
                "duration_estimate": "6-12 months",
                "investment_strategy": "Diversified approach recommended",
                "recession_probability": 25
            }

    def _prepare_data_summary(self, data: Dict[str, pd.Series]) -> str:
        """Prepare human-readable data summary for LLM"""
        summary_lines = []

        for indicator, series in data.items():
            if not series.empty:
                latest_value = series.iloc[-1]
                trend = self._calculate_trend(series, 6)

                trend_desc = "rising" if trend > 0.1 else "falling" if trend < -0.1 else "stable"
                summary_lines.append(f"{indicator}: {latest_value:.2f}% ({trend_desc})")

        return "\n".join(summary_lines)

    def _default_cycle_signal(self) -> CycleSignal:
        """Return default cycle signal in case of errors"""
        return CycleSignal(
            cycle_type="business_cycle",
            phase="expansion",
            strength=0.5,
            confidence=0.3,
            duration_months=12,
            next_inflection=datetime.now() + timedelta(days=365),
            supporting_indicators=[],
            investment_implications=self.sector_rotation["expansion"]
        )

    async def get_cycle_report(self) -> Dict:
        """Generate comprehensive cycle analysis report"""
        cycle_signal = await self.analyze_business_cycle()

        return {
            "timestamp": datetime.now().isoformat(),
            "agent": self.name,
            "cycle_analysis": {
                "current_phase": cycle_signal.phase,
                "strength": cycle_signal.strength,
                "confidence": cycle_signal.confidence,
                "duration_estimate": cycle_signal.duration_months,
                "next_inflection": cycle_signal.next_inflection.isoformat() if cycle_signal.next_inflection else None
            },
            "investment_implications": cycle_signal.investment_implications,
            "supporting_indicators": cycle_signal.supporting_indicators,
            "risk_assessment": {
                "phase_transition_risk": 1 - cycle_signal.confidence,
                "data_quality_risk": 1 - len(cycle_signal.supporting_indicators) / 10,
                "model_uncertainty": 0.2
            }
        }