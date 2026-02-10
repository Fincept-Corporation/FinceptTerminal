"""
Credit Analysis Module
======================

Credit risk measurement and analysis implementing CFA Institute standard
methodologies for fixed income credit evaluation.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Credit ratings and rating transitions
  - Default statistics and recovery rates
  - Credit spreads by rating and maturity
  - Financial ratios and credit metrics
  - CDS spreads and market-implied probabilities

OUTPUT:
  - Default probability calculations
  - Loss given default (LGD) estimates
  - Expected loss calculations
  - Credit spread analysis
  - Rating transition probabilities
  - Credit VaR metrics

PARAMETERS:
  - probability_of_default: Annual PD as decimal
  - recovery_rate: Expected recovery as decimal - default: 0.40
  - exposure: Exposure at default - default: 1000
  - time_horizon: Analysis period in years - default: 1
  - confidence_level: For VaR calculations - default: 0.99
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple
from enum import Enum
import numpy as np
from scipy import stats
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class CreditRating(Enum):
    """Standard credit rating categories"""
    AAA = "AAA"
    AA_PLUS = "AA+"
    AA = "AA"
    AA_MINUS = "AA-"
    A_PLUS = "A+"
    A = "A"
    A_MINUS = "A-"
    BBB_PLUS = "BBB+"
    BBB = "BBB"
    BBB_MINUS = "BBB-"
    BB_PLUS = "BB+"
    BB = "BB"
    BB_MINUS = "BB-"
    B_PLUS = "B+"
    B = "B"
    B_MINUS = "B-"
    CCC = "CCC"
    CC = "CC"
    C = "C"
    D = "D"


class SeniorityLevel(Enum):
    """Bond seniority levels"""
    SENIOR_SECURED = "senior_secured"
    SENIOR_UNSECURED = "senior_unsecured"
    SENIOR_SUBORDINATED = "senior_subordinated"
    SUBORDINATED = "subordinated"
    JUNIOR_SUBORDINATED = "junior_subordinated"


@dataclass
class CreditMetrics:
    """Container for credit analysis results"""
    probability_of_default: float
    loss_given_default: float
    expected_loss: float
    unexpected_loss: float
    credit_var: float


# Historical average default rates by rating (1-year, approximate)
HISTORICAL_DEFAULT_RATES = {
    CreditRating.AAA: 0.0001,
    CreditRating.AA_PLUS: 0.0002,
    CreditRating.AA: 0.0003,
    CreditRating.AA_MINUS: 0.0004,
    CreditRating.A_PLUS: 0.0006,
    CreditRating.A: 0.0008,
    CreditRating.A_MINUS: 0.0010,
    CreditRating.BBB_PLUS: 0.0015,
    CreditRating.BBB: 0.0020,
    CreditRating.BBB_MINUS: 0.0030,
    CreditRating.BB_PLUS: 0.0050,
    CreditRating.BB: 0.0080,
    CreditRating.BB_MINUS: 0.0120,
    CreditRating.B_PLUS: 0.0200,
    CreditRating.B: 0.0350,
    CreditRating.B_MINUS: 0.0500,
    CreditRating.CCC: 0.1500,
    CreditRating.CC: 0.2500,
    CreditRating.C: 0.3500,
    CreditRating.D: 1.0000,
}

# Historical average recovery rates by seniority
HISTORICAL_RECOVERY_RATES = {
    SeniorityLevel.SENIOR_SECURED: 0.53,
    SeniorityLevel.SENIOR_UNSECURED: 0.37,
    SeniorityLevel.SENIOR_SUBORDINATED: 0.31,
    SeniorityLevel.SUBORDINATED: 0.27,
    SeniorityLevel.JUNIOR_SUBORDINATED: 0.17,
}


class DefaultProbabilityModel:
    """
    Default probability modeling and calculation.

    Implements multiple approaches to estimate default probabilities.
    """

    def calculate_cumulative_pd(
        self,
        annual_pd: float,
        years: int,
        method: str = "hazard",
    ) -> Dict[str, Any]:
        """
        Calculate cumulative default probability over multiple years.

        Args:
            annual_pd: Annual probability of default
            years: Number of years
            method: 'hazard' or 'simple'

        Returns:
            Dictionary with cumulative PD
        """
        cumulative_pds = []

        for t in range(1, years + 1):
            if method == "hazard":
                # Using hazard rate (more accurate)
                cum_pd = 1 - (1 - annual_pd) ** t
            else:
                # Simple approximation
                cum_pd = min(annual_pd * t, 1.0)

            cumulative_pds.append({
                'year': t,
                'cumulative_pd': round(cum_pd, 6),
                'cumulative_pd_pct': round(cum_pd * 100, 4),
                'survival_prob': round(1 - cum_pd, 6)
            })

        # Marginal PDs (probability of defaulting in year t given survival to year t-1)
        marginal_pds = []
        for i, cpd in enumerate(cumulative_pds):
            if i == 0:
                marginal = cpd['cumulative_pd']
            else:
                marginal = cpd['cumulative_pd'] - cumulative_pds[i - 1]['cumulative_pd']
            marginal_pds.append({
                'year': cpd['year'],
                'marginal_pd': round(marginal, 6)
            })

        return {
            'annual_pd': annual_pd,
            'cumulative_pds': cumulative_pds,
            'marginal_pds': marginal_pds,
            'method': method,
            'total_years': years
        }

    def pd_from_credit_spread(
        self,
        credit_spread: float,
        recovery_rate: float = 0.40,
        risk_free_rate: float = 0.03,
    ) -> Dict[str, Any]:
        """
        Derive implied probability of default from credit spread.

        PD ≈ Spread / (1 - Recovery Rate)

        Args:
            credit_spread: Credit spread over risk-free rate
            recovery_rate: Expected recovery rate
            risk_free_rate: Risk-free interest rate

        Returns:
            Dictionary with implied PD
        """
        lgd = 1 - recovery_rate

        # Simple approximation
        pd_simple = credit_spread / lgd

        # More accurate using hazard rate model
        # Spread = PD * LGD / (1 + r)
        pd_hazard = credit_spread * (1 + risk_free_rate) / lgd

        return {
            'implied_pd_simple': round(pd_simple, 6),
            'implied_pd_simple_pct': round(pd_simple * 100, 4),
            'implied_pd_hazard': round(pd_hazard, 6),
            'implied_pd_hazard_pct': round(pd_hazard * 100, 4),
            'credit_spread': round(credit_spread, 6),
            'credit_spread_bps': round(credit_spread * 10000, 1),
            'recovery_rate': recovery_rate,
            'lgd': round(lgd, 4),
            'interpretation': f"Market implies ~{round(pd_simple * 100, 2)}% annual default probability"
        }

    def pd_from_merton_model(
        self,
        asset_value: float,
        asset_volatility: float,
        debt_face_value: float,
        risk_free_rate: float,
        time_horizon: float = 1.0,
    ) -> Dict[str, Any]:
        """
        Calculate default probability using Merton structural model.

        Distance to Default (DD) = (ln(V/D) + (r - σ²/2)T) / (σ√T)
        PD = N(-DD)

        Args:
            asset_value: Current firm asset value
            asset_volatility: Asset volatility
            debt_face_value: Face value of debt
            risk_free_rate: Risk-free rate
            time_horizon: Time horizon in years

        Returns:
            Dictionary with Merton model results
        """
        # Distance to default
        d1 = (np.log(asset_value / debt_face_value) +
              (risk_free_rate + 0.5 * asset_volatility ** 2) * time_horizon) / \
             (asset_volatility * np.sqrt(time_horizon))

        d2 = d1 - asset_volatility * np.sqrt(time_horizon)

        # Probability of default
        pd = stats.norm.cdf(-d2)

        # Distance to default (in standard deviations)
        distance_to_default = d2

        # Expected default frequency (EDF) - similar concept
        edf = stats.norm.cdf(-d2)

        return {
            'probability_of_default': round(pd, 6),
            'pd_percent': round(pd * 100, 4),
            'distance_to_default': round(distance_to_default, 4),
            'd1': round(d1, 4),
            'd2': round(d2, 4),
            'asset_value': asset_value,
            'debt_face_value': debt_face_value,
            'leverage_ratio': round(debt_face_value / asset_value, 4),
            'interpretation': f"DD of {round(distance_to_default, 2)}σ implies {round(pd * 100, 2)}% default probability"
        }

    def get_historical_pd(
        self,
        rating: str,
        years: int = 1,
    ) -> Dict[str, Any]:
        """
        Get historical default probability by credit rating.

        Args:
            rating: Credit rating string (e.g., 'BBB', 'BB+')
            years: Time horizon

        Returns:
            Dictionary with historical default rates
        """
        # Parse rating
        try:
            rating_enum = CreditRating(rating)
        except ValueError:
            # Try to match
            for r in CreditRating:
                if r.value.upper() == rating.upper():
                    rating_enum = r
                    break
            else:
                return {'error': f'Unknown rating: {rating}'}

        annual_pd = HISTORICAL_DEFAULT_RATES.get(rating_enum, 0.05)

        # Calculate cumulative PD
        cumulative_pd = 1 - (1 - annual_pd) ** years

        return {
            'rating': rating_enum.value,
            'annual_pd': round(annual_pd, 6),
            'annual_pd_pct': round(annual_pd * 100, 4),
            'cumulative_pd': round(cumulative_pd, 6),
            'cumulative_pd_pct': round(cumulative_pd * 100, 4),
            'years': years,
            'is_investment_grade': rating_enum.value.startswith(('AAA', 'AA', 'A', 'BBB')),
            'source': 'Historical averages (approximate)'
        }


class CreditAnalyzer:
    """
    Comprehensive credit risk analysis.

    Provides expected loss, unexpected loss, and credit VaR calculations.
    """

    def calculate_expected_loss(
        self,
        exposure: float,
        probability_of_default: float,
        recovery_rate: float = 0.40,
    ) -> Dict[str, Any]:
        """
        Calculate expected loss from credit exposure.

        EL = EAD × PD × LGD

        Args:
            exposure: Exposure at default (EAD)
            probability_of_default: PD
            recovery_rate: Expected recovery rate

        Returns:
            Dictionary with expected loss metrics
        """
        lgd = 1 - recovery_rate
        expected_loss = exposure * probability_of_default * lgd

        return {
            'expected_loss': round(expected_loss, 4),
            'expected_loss_pct': round((expected_loss / exposure) * 100, 4),
            'exposure_at_default': exposure,
            'probability_of_default': probability_of_default,
            'loss_given_default': round(lgd, 4),
            'recovery_rate': recovery_rate,
            'formula': 'EL = EAD × PD × LGD'
        }

    def calculate_unexpected_loss(
        self,
        exposure: float,
        probability_of_default: float,
        recovery_rate: float = 0.40,
        lgd_volatility: float = 0.25,
    ) -> Dict[str, Any]:
        """
        Calculate unexpected loss (standard deviation of losses).

        UL = EAD × √(PD × σ²_LGD + LGD² × PD × (1-PD))

        Args:
            exposure: Exposure at default
            probability_of_default: PD
            recovery_rate: Recovery rate
            lgd_volatility: Volatility of LGD

        Returns:
            Dictionary with unexpected loss
        """
        lgd = 1 - recovery_rate

        # Variance components
        lgd_variance = lgd_volatility ** 2
        pd_variance = probability_of_default * (1 - probability_of_default)

        # Unexpected loss formula
        ul_squared = exposure ** 2 * (
            probability_of_default * lgd_variance +
            lgd ** 2 * pd_variance
        )
        unexpected_loss = np.sqrt(ul_squared)

        # Expected loss for comparison
        expected_loss = exposure * probability_of_default * lgd

        return {
            'unexpected_loss': round(unexpected_loss, 4),
            'expected_loss': round(expected_loss, 4),
            'ul_el_ratio': round(unexpected_loss / expected_loss, 4) if expected_loss > 0 else None,
            'exposure': exposure,
            'lgd_volatility': lgd_volatility,
            'interpretation': 'UL represents one standard deviation of potential losses'
        }

    def calculate_credit_var(
        self,
        exposure: float,
        probability_of_default: float,
        recovery_rate: float = 0.40,
        confidence_level: float = 0.99,
        correlation: float = 0.20,
    ) -> Dict[str, Any]:
        """
        Calculate Credit VaR using single-factor model.

        Uses Vasicek single-factor model for portfolio credit risk.

        Args:
            exposure: Exposure at default
            probability_of_default: PD
            recovery_rate: Recovery rate
            confidence_level: VaR confidence level
            correlation: Asset correlation

        Returns:
            Dictionary with Credit VaR
        """
        lgd = 1 - recovery_rate

        # Conditional PD under stress (Vasicek model)
        z = stats.norm.ppf(confidence_level)
        pd_stress = stats.norm.cdf(
            (stats.norm.ppf(probability_of_default) + np.sqrt(correlation) * z) /
            np.sqrt(1 - correlation)
        )

        # Credit VaR
        expected_loss = exposure * probability_of_default * lgd
        worst_case_loss = exposure * pd_stress * lgd
        credit_var = worst_case_loss - expected_loss

        return {
            'credit_var': round(credit_var, 4),
            'credit_var_pct': round((credit_var / exposure) * 100, 4),
            'expected_loss': round(expected_loss, 4),
            'worst_case_loss': round(worst_case_loss, 4),
            'conditional_pd': round(pd_stress, 6),
            'confidence_level': confidence_level,
            'correlation': correlation,
            'interpretation': f"At {confidence_level * 100}% confidence, losses could exceed EL by ${round(credit_var, 2)}"
        }

    def analyze_credit_spread(
        self,
        observed_spread: float,
        rating: str,
        maturity: float,
        recovery_rate: float = 0.40,
    ) -> Dict[str, Any]:
        """
        Analyze if observed credit spread is fair relative to rating.

        Args:
            observed_spread: Observed market spread
            rating: Credit rating
            maturity: Bond maturity
            recovery_rate: Expected recovery rate

        Returns:
            Dictionary with spread analysis
        """
        # Get historical PD for rating
        pd_model = DefaultProbabilityModel()
        pd_result = pd_model.get_historical_pd(rating, int(maturity))

        if 'error' in pd_result:
            return pd_result

        annual_pd = pd_result['annual_pd']
        lgd = 1 - recovery_rate

        # Fair spread based on historical PD
        fair_spread = annual_pd * lgd

        # Spread difference
        spread_diff = observed_spread - fair_spread

        # Rich/cheap analysis
        if spread_diff > 0.001:  # 10bps
            valuation = "CHEAP (spread too wide)"
        elif spread_diff < -0.001:
            valuation = "RICH (spread too tight)"
        else:
            valuation = "FAIR"

        return {
            'observed_spread': round(observed_spread, 6),
            'observed_spread_bps': round(observed_spread * 10000, 1),
            'fair_spread': round(fair_spread, 6),
            'fair_spread_bps': round(fair_spread * 10000, 1),
            'spread_difference': round(spread_diff, 6),
            'spread_difference_bps': round(spread_diff * 10000, 1),
            'valuation': valuation,
            'rating': rating,
            'implied_pd_from_spread': round(observed_spread / lgd, 6),
            'historical_pd': round(annual_pd, 6)
        }

    def calculate_recovery_rate(
        self,
        seniority: str,
        collateral_value: float = None,
        debt_outstanding: float = None,
    ) -> Dict[str, Any]:
        """
        Estimate recovery rate based on seniority and collateral.

        Args:
            seniority: Debt seniority level
            collateral_value: Value of collateral (optional)
            debt_outstanding: Total debt outstanding (optional)

        Returns:
            Dictionary with recovery rate estimate
        """
        # Get historical average by seniority
        try:
            seniority_enum = SeniorityLevel(seniority.lower())
        except ValueError:
            seniority_enum = SeniorityLevel.SENIOR_UNSECURED

        historical_recovery = HISTORICAL_RECOVERY_RATES.get(seniority_enum, 0.40)

        result = {
            'seniority': seniority_enum.value,
            'historical_recovery_rate': round(historical_recovery, 4),
            'historical_recovery_pct': round(historical_recovery * 100, 2),
            'lgd': round(1 - historical_recovery, 4)
        }

        # Adjust for collateral if provided
        if collateral_value is not None and debt_outstanding is not None:
            collateral_coverage = collateral_value / debt_outstanding
            adjusted_recovery = min(collateral_coverage, 1.0) * 0.8 + historical_recovery * 0.2
            result['collateral_coverage'] = round(collateral_coverage, 4)
            result['adjusted_recovery_rate'] = round(adjusted_recovery, 4)

        return result

    def rating_transition_analysis(
        self,
        current_rating: str,
        transition_matrix: Dict[str, Dict[str, float]] = None,
    ) -> Dict[str, Any]:
        """
        Analyze rating transition probabilities.

        Args:
            current_rating: Current credit rating
            transition_matrix: Custom transition matrix (optional)

        Returns:
            Dictionary with transition probabilities
        """
        # Simplified 1-year transition probabilities (approximate)
        default_matrix = {
            'AAA': {'AAA': 0.91, 'AA': 0.08, 'A': 0.01, 'BBB': 0.00, 'BB': 0.00, 'B': 0.00, 'CCC': 0.00, 'D': 0.00},
            'AA': {'AAA': 0.01, 'AA': 0.91, 'A': 0.07, 'BBB': 0.01, 'BB': 0.00, 'B': 0.00, 'CCC': 0.00, 'D': 0.00},
            'A': {'AAA': 0.00, 'AA': 0.02, 'A': 0.91, 'BBB': 0.05, 'BB': 0.01, 'B': 0.00, 'CCC': 0.00, 'D': 0.00},
            'BBB': {'AAA': 0.00, 'AA': 0.00, 'A': 0.04, 'BBB': 0.89, 'BB': 0.05, 'B': 0.01, 'CCC': 0.00, 'D': 0.00},
            'BB': {'AAA': 0.00, 'AA': 0.00, 'A': 0.00, 'BBB': 0.06, 'BB': 0.83, 'B': 0.08, 'CCC': 0.02, 'D': 0.01},
            'B': {'AAA': 0.00, 'AA': 0.00, 'A': 0.00, 'BBB': 0.00, 'BB': 0.06, 'B': 0.82, 'CCC': 0.07, 'D': 0.05},
            'CCC': {'AAA': 0.00, 'AA': 0.00, 'A': 0.00, 'BBB': 0.01, 'BB': 0.02, 'B': 0.11, 'CCC': 0.61, 'D': 0.25},
        }

        matrix = transition_matrix or default_matrix

        # Simplify rating to base category
        base_rating = current_rating.replace('+', '').replace('-', '')
        if base_rating not in matrix:
            return {'error': f'Rating {current_rating} not in transition matrix'}

        transitions = matrix[base_rating]

        # Calculate key metrics
        stay_prob = transitions.get(base_rating, 0)
        upgrade_prob = sum(v for k, v in transitions.items()
                          if list(matrix.keys()).index(k) < list(matrix.keys()).index(base_rating))
        downgrade_prob = sum(v for k, v in transitions.items()
                            if list(matrix.keys()).index(k) > list(matrix.keys()).index(base_rating) and k != 'D')
        default_prob = transitions.get('D', 0)

        return {
            'current_rating': current_rating,
            'transition_probabilities': {k: round(v, 4) for k, v in transitions.items()},
            'stay_probability': round(stay_prob, 4),
            'upgrade_probability': round(upgrade_prob, 4),
            'downgrade_probability': round(downgrade_prob, 4),
            'default_probability': round(default_prob, 4),
            'expected_direction': 'upgrade' if upgrade_prob > downgrade_prob else 'downgrade' if downgrade_prob > upgrade_prob else 'stable'
        }


def run_credit_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for credit analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'expected_loss')

    try:
        if analysis_type == 'expected_loss':
            analyzer = CreditAnalyzer()
            return analyzer.calculate_expected_loss(
                exposure=params.get('exposure', 1000000),
                probability_of_default=params.get('probability_of_default', 0.02),
                recovery_rate=params.get('recovery_rate', 0.40)
            )

        elif analysis_type == 'unexpected_loss':
            analyzer = CreditAnalyzer()
            return analyzer.calculate_unexpected_loss(
                exposure=params.get('exposure', 1000000),
                probability_of_default=params.get('probability_of_default', 0.02),
                recovery_rate=params.get('recovery_rate', 0.40),
                lgd_volatility=params.get('lgd_volatility', 0.25)
            )

        elif analysis_type == 'credit_var':
            analyzer = CreditAnalyzer()
            return analyzer.calculate_credit_var(
                exposure=params.get('exposure', 1000000),
                probability_of_default=params.get('probability_of_default', 0.02),
                recovery_rate=params.get('recovery_rate', 0.40),
                confidence_level=params.get('confidence_level', 0.99),
                correlation=params.get('correlation', 0.20)
            )

        elif analysis_type == 'pd_from_spread':
            model = DefaultProbabilityModel()
            return model.pd_from_credit_spread(
                credit_spread=params.get('credit_spread', 0.02),
                recovery_rate=params.get('recovery_rate', 0.40),
                risk_free_rate=params.get('risk_free_rate', 0.03)
            )

        elif analysis_type == 'merton_pd':
            model = DefaultProbabilityModel()
            return model.pd_from_merton_model(
                asset_value=params.get('asset_value', 100),
                asset_volatility=params.get('asset_volatility', 0.30),
                debt_face_value=params.get('debt_face_value', 70),
                risk_free_rate=params.get('risk_free_rate', 0.03),
                time_horizon=params.get('time_horizon', 1.0)
            )

        elif analysis_type == 'historical_pd':
            model = DefaultProbabilityModel()
            return model.get_historical_pd(
                rating=params.get('rating', 'BBB'),
                years=params.get('years', 1)
            )

        elif analysis_type == 'cumulative_pd':
            model = DefaultProbabilityModel()
            return model.calculate_cumulative_pd(
                annual_pd=params.get('annual_pd', 0.02),
                years=params.get('years', 5),
                method=params.get('method', 'hazard')
            )

        elif analysis_type == 'spread_analysis':
            analyzer = CreditAnalyzer()
            return analyzer.analyze_credit_spread(
                observed_spread=params.get('observed_spread', 0.015),
                rating=params.get('rating', 'BBB'),
                maturity=params.get('maturity', 5),
                recovery_rate=params.get('recovery_rate', 0.40)
            )

        elif analysis_type == 'recovery_rate':
            analyzer = CreditAnalyzer()
            return analyzer.calculate_recovery_rate(
                seniority=params.get('seniority', 'senior_unsecured'),
                collateral_value=params.get('collateral_value'),
                debt_outstanding=params.get('debt_outstanding')
            )

        elif analysis_type == 'rating_transition':
            analyzer = CreditAnalyzer()
            return analyzer.rating_transition_analysis(
                current_rating=params.get('rating', 'BBB')
            )

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Credit analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_credit_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Credit Analysis Demo:")

        analyzer = CreditAnalyzer()
        pd_model = DefaultProbabilityModel()

        # Expected Loss
        result = analyzer.calculate_expected_loss(
            exposure=1000000, probability_of_default=0.02, recovery_rate=0.40
        )
        print(f"\nExpected Loss: ${result['expected_loss']:,.2f}")

        # Credit VaR
        result = analyzer.calculate_credit_var(
            exposure=1000000, probability_of_default=0.02
        )
        print(f"Credit VaR (99%): ${result['credit_var']:,.2f}")

        # Merton Model
        result = pd_model.pd_from_merton_model(
            asset_value=100, asset_volatility=0.30, debt_face_value=70
        )
        print(f"Merton PD: {result['pd_percent']}%")
