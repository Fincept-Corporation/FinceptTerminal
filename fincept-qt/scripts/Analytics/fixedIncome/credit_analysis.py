"""
Credit Analysis Module
======================

Credit risk measurement (CFA-curriculum methods): default probabilities, loss given default,
expected / unexpected loss, credit VaR, spread-implied PD, rating transitions.

Notes
-----
* Probabilities, rates and spreads are decimals.
* HISTORICAL_DEFAULT_RATES, HISTORICAL_RECOVERY_RATES and the transition matrix are hard-coded
  approximations of long-run agency averages, not live data; every output that uses them says so.
* Ratings accept S&P/Fitch style (BBB+) and Moody's style (Baa1).

PARAMETERS:
  probability_of_default, recovery_rate (0.40), exposure, time_horizon (1), confidence_level (0.99),
  correlation (0.20), credit_spread, risk_free_rate, rating, years
"""

import logging
import math
import os
import sys
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict, List, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num  # noqa: E402

logger = logging.getLogger(__name__)

N = fc.NORMAL  # standard normal (statistics.NormalDist): cdf / inv_cdf

TABLE_SOURCE = 'Hard-coded long-run averages (approximate), not live agency data'


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

_MOODYS = {
    'AAA': 'AAA', 'AA1': 'AA+', 'AA2': 'AA', 'AA3': 'AA-', 'A1': 'A+', 'A2': 'A', 'A3': 'A-',
    'BAA1': 'BBB+', 'BAA2': 'BBB', 'BAA3': 'BBB-', 'BA1': 'BB+', 'BA2': 'BB', 'BA3': 'BB-',
    'B1': 'B+', 'B2': 'B', 'B3': 'B-', 'CAA1': 'CCC', 'CAA2': 'CCC', 'CAA3': 'CCC', 'CAA': 'CCC',
    'CA': 'CC',
}

# Order used for upgrade / downgrade classification
RATING_ORDER = ['AAA', 'AA', 'A', 'BBB', 'BB', 'B', 'CCC', 'D']


def parse_rating(rating: Any) -> CreditRating:
    """'BBB+', 'bbb+', 'Baa1' -> CreditRating."""
    if not isinstance(rating, str) or not rating.strip():
        raise FIError('rating must be a rating string such as "BBB+" or "Baa1"')
    s = rating.strip().upper()
    for r_ in CreditRating:
        if r_.value == s:
            return r_
    if s in _MOODYS:
        return CreditRating(_MOODYS[s])
    raise FIError(f'Unknown rating: {rating} (use S&P/Fitch style AAA..D or Moody\'s style Aaa..C)')


def _pd(x: float, name: str = 'probability_of_default') -> float:
    if not 0 <= x <= 1:
        raise FIError(f'{name} must be between 0 and 1 (got {x:g})')
    return x


def _recovery(x: float, allow_one: bool = True) -> float:
    if x < 0 or x > 1 or (x == 1 and not allow_one):
        raise FIError(f'recovery_rate must be >= 0 and {"<=" if allow_one else "<"} 1 (got {x:g})')
    return x


class DefaultProbabilityModel:
    """Default probability modeling: cumulative PDs, spread-implied PD, Merton, historical tables."""

    def calculate_cumulative_pd(
        self,
        annual_pd: float,
        years: int,
        method: str = "hazard",
    ) -> Dict[str, Any]:
        """
        Cumulative default probability for a constant annual PD.

        'hazard' (constant conditional PD): CPD_t = 1 - (1 - PD)^t;  'simple': min(PD * t, 1).
        """
        _pd(annual_pd, 'annual_pd')
        if years < 1:
            raise FIError('years must be >= 1')
        if method not in ('hazard', 'simple'):
            raise FIError('method must be hazard or simple')
        cumulative_pds = []
        for t in range(1, years + 1):
            cum_pd = 1 - (1 - annual_pd) ** t if method == "hazard" else min(annual_pd * t, 1.0)
            cumulative_pds.append({
                'year': t,
                'cumulative_pd': round(cum_pd, 6),
                'cumulative_pd_pct': round(cum_pd * 100, 4),
                'survival_prob': round(1 - cum_pd, 6)
            })
        marginal_pds = []
        prev = 0.0
        for cpd in cumulative_pds:
            marginal_pds.append({'year': cpd['year'], 'marginal_pd': round(cpd['cumulative_pd'] - prev, 6)})
            prev = cpd['cumulative_pd']
        return {
            'annual_pd': annual_pd,
            'cumulative_pds': cumulative_pds,
            'marginal_pds': marginal_pds,
            'marginal_pd_definition': 'unconditional probability of defaulting in that year',
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
        Spread-implied (risk-neutral) default probability.

        simple ("credit triangle"): PD = s / LGD
        hazard (constant intensity): lambda = s / LGD, 1-year PD = 1 - exp(-lambda)
        one-period discrete: price of a risky 1y zero = 1/(1+r+s) = [(1-PD) + PD*R]/(1+r)
                             => PD = s / (LGD * (1 + r + s))
        """
        _recovery(recovery_rate, allow_one=False)
        if credit_spread < 0:
            raise FIError('credit_spread must be >= 0')
        lgd = 1 - recovery_rate
        pd_simple = credit_spread / lgd
        hazard = credit_spread / lgd
        pd_hazard = 1 - math.exp(-hazard)
        pd_one_period = credit_spread / (lgd * (1 + risk_free_rate + credit_spread))
        return {
            'implied_pd_simple': round(pd_simple, 6),
            'implied_pd_simple_pct': round(pd_simple * 100, 4),
            'hazard_rate': round(hazard, 6),
            'implied_pd_hazard': round(pd_hazard, 6),
            'implied_pd_hazard_pct': round(pd_hazard * 100, 4),
            'implied_pd_one_period': round(pd_one_period, 6),
            'implied_pd_one_period_pct': round(pd_one_period * 100, 4),
            'credit_spread': round(credit_spread, 6),
            'credit_spread_bps': round(credit_spread * 10000, 1),
            'recovery_rate': recovery_rate,
            'risk_free_rate': risk_free_rate,
            'lgd': round(lgd, 4),
            'measure': 'risk-neutral (includes risk and liquidity premia; above historical default rates)',
            'interpretation': f"Market implies ~{round(pd_hazard * 100, 2)}% 1-year default probability "
                              f"(constant hazard {round(hazard * 100, 2)}%/yr)"
        }

    def pd_from_merton_model(
        self,
        asset_value: float,
        asset_volatility: float,
        debt_face_value: float,
        risk_free_rate: float,
        time_horizon: float = 1.0,
        asset_return: Optional[float] = None,
    ) -> Dict[str, Any]:
        """
        Merton structural model. d1 = (ln(V/D) + (mu + sigma^2/2) T) / (sigma sqrt T), d2 = d1 - sigma sqrt T,
        PD = N(-d2). mu = risk_free_rate (risk-neutral PD) unless asset_return is given (real-world PD).
        """
        if asset_value <= 0 or debt_face_value <= 0:
            raise FIError('asset_value and debt_face_value must be > 0')
        if asset_volatility <= 0:
            raise FIError('asset_volatility must be > 0')
        if time_horizon <= 0:
            raise FIError('time_horizon must be > 0')
        mu = risk_free_rate if asset_return is None else asset_return
        st = asset_volatility * math.sqrt(time_horizon)
        d1 = (math.log(asset_value / debt_face_value) + (mu + 0.5 * asset_volatility ** 2) * time_horizon) / st
        d2 = d1 - st
        pd = N.cdf(-d2)
        return {
            'probability_of_default': round(pd, 6),
            'pd_percent': round(pd * 100, 4),
            'distance_to_default': round(d2, 4),
            'd1': round(d1, 4),
            'd2': round(d2, 4),
            'asset_value': asset_value,
            'debt_face_value': debt_face_value,
            'leverage_ratio': round(debt_face_value / asset_value, 4),
            'time_horizon': time_horizon,
            'measure': 'risk-neutral (drift = risk_free_rate)' if asset_return is None
                       else 'real-world (drift = asset_return)',
            'interpretation': f"DD of {round(d2, 2)} standard deviations implies {round(pd * 100, 2)}% "
                              f"default probability over {time_horizon:g}y"
        }

    def get_historical_pd(
        self,
        rating: str,
        years: int = 1,
    ) -> Dict[str, Any]:
        """Historical (table) default probability by rating; cumulative PD compounds the flat annual rate."""
        rating_enum = parse_rating(rating)
        if years < 1:
            raise FIError('years must be >= 1')
        annual_pd = HISTORICAL_DEFAULT_RATES[rating_enum]
        cumulative_pd = 1 - (1 - annual_pd) ** years
        return {
            'rating': rating_enum.value,
            'annual_pd': round(annual_pd, 6),
            'annual_pd_pct': round(annual_pd * 100, 4),
            'cumulative_pd': round(cumulative_pd, 6),
            'cumulative_pd_pct': round(cumulative_pd * 100, 4),
            'years': years,
            'is_investment_grade': rating_enum.value in ('AAA', 'AA+', 'AA', 'AA-', 'A+', 'A', 'A-',
                                                         'BBB+', 'BBB', 'BBB-'),
            'source': TABLE_SOURCE,
            'note': 'cumulative PD assumes the same annual PD every year (no term structure)',
        }


class CreditAnalyzer:
    """Expected loss, unexpected loss, credit VaR and related analytics."""

    def calculate_expected_loss(
        self,
        exposure: float,
        probability_of_default: float,
        recovery_rate: float = 0.40,
    ) -> Dict[str, Any]:
        """EL = EAD x PD x LGD"""
        if exposure <= 0:
            raise FIError('exposure must be > 0')
        _pd(probability_of_default)
        _recovery(recovery_rate)
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
        """UL = EAD x sqrt(PD x sigma_LGD^2 + LGD^2 x PD x (1 - PD))"""
        if exposure <= 0:
            raise FIError('exposure must be > 0')
        _pd(probability_of_default)
        _recovery(recovery_rate)
        if lgd_volatility < 0:
            raise FIError('lgd_volatility must be >= 0')
        lgd = 1 - recovery_rate
        ul = exposure * math.sqrt(probability_of_default * lgd_volatility ** 2 +
                                  lgd ** 2 * probability_of_default * (1 - probability_of_default))
        expected_loss = exposure * probability_of_default * lgd
        return {
            'unexpected_loss': round(ul, 4),
            'expected_loss': round(expected_loss, 4),
            'ul_el_ratio': round(ul / expected_loss, 4) if expected_loss > 0 else None,
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
        Credit VaR with the Vasicek single-factor model:
        PD_stress = N[(N^-1(PD) + sqrt(rho) N^-1(q)) / sqrt(1 - rho)];  VaR = EAD x LGD x (PD_stress - PD).
        """
        if exposure <= 0:
            raise FIError('exposure must be > 0')
        if not 0 < probability_of_default < 1:
            raise FIError('probability_of_default must be strictly between 0 and 1')
        _recovery(recovery_rate)
        if not 0 < confidence_level < 1:
            raise FIError('confidence_level must be strictly between 0 and 1 (e.g. 0.99)')
        if not 0 <= correlation < 1:
            raise FIError('correlation must be >= 0 and < 1')
        lgd = 1 - recovery_rate
        z = N.inv_cdf(confidence_level)
        pd_stress = N.cdf((N.inv_cdf(probability_of_default) + math.sqrt(correlation) * z)
                          / math.sqrt(1 - correlation))
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
            'interpretation': f"At {confidence_level * 100:g}% confidence, losses could exceed EL by "
                              f"{round(credit_var, 2)}"
        }

    def analyze_credit_spread(
        self,
        observed_spread: float,
        rating: str,
        maturity: float,
        recovery_rate: float = 0.40,
    ) -> Dict[str, Any]:
        """
        Split an observed spread into the pure expected-loss spread implied by the historical PD table
        (annual PD x LGD) and the remainder (risk premium + liquidity premium + model error).
        No rich/cheap verdict: the expected-loss spread is not a fair spread.
        """
        _recovery(recovery_rate, allow_one=False)
        if maturity <= 0:
            raise FIError('maturity must be > 0')
        pd_result = DefaultProbabilityModel().get_historical_pd(rating, max(1, int(round(maturity))))
        annual_pd = pd_result['annual_pd']
        lgd = 1 - recovery_rate
        el_spread = annual_pd * lgd
        premium = observed_spread - el_spread
        return {
            'observed_spread': round(observed_spread, 6),
            'observed_spread_bps': round(observed_spread * 10000, 1),
            'expected_loss_spread': round(el_spread, 6),
            'expected_loss_spread_bps': round(el_spread * 10000, 1),
            'spread_above_expected_loss': round(premium, 6),
            'spread_above_expected_loss_bps': round(premium * 10000, 1),
            'expected_loss_share_pct': round(el_spread / observed_spread * 100, 2) if observed_spread > 0 else None,
            'rating': pd_result['rating'],
            'implied_pd_from_spread': round(observed_spread / lgd, 6),
            'historical_pd': round(annual_pd, 6),
            'source': TABLE_SOURCE,
            'interpretation': ('The expected-loss spread is the compensation for average historical defaults only; '
                               'the remainder is risk premium, liquidity premium and model error, which is normal '
                               'and is not by itself evidence that the bond is cheap'),
        }

    def calculate_recovery_rate(
        self,
        seniority: str,
        collateral_value: float = None,
        debt_outstanding: float = None,
    ) -> Dict[str, Any]:
        """
        Recovery estimate: historical average by seniority (table), optionally blended 80/20 with
        collateral coverage (capped at 100%) when collateral_value and debt_outstanding are given.
        """
        try:
            seniority_enum = SeniorityLevel(str(seniority).lower())
        except ValueError:
            raise FIError('seniority must be one of: ' + ', '.join(s.value for s in SeniorityLevel)) from None
        historical_recovery = HISTORICAL_RECOVERY_RATES[seniority_enum]
        result = {
            'seniority': seniority_enum.value,
            'historical_recovery_rate': round(historical_recovery, 4),
            'historical_recovery_pct': round(historical_recovery * 100, 2),
            'lgd': round(1 - historical_recovery, 4),
            'source': TABLE_SOURCE,
        }
        if collateral_value is not None and debt_outstanding is not None:
            if debt_outstanding <= 0 or collateral_value < 0:
                raise FIError('debt_outstanding must be > 0 and collateral_value >= 0')
            coverage = collateral_value / debt_outstanding
            adjusted = min(coverage, 1.0) * 0.8 + historical_recovery * 0.2
            result['collateral_coverage'] = round(coverage, 4)
            result['adjusted_recovery_rate'] = round(adjusted, 4)
            result['adjustment_method'] = '0.8 x min(collateral coverage, 1) + 0.2 x historical (heuristic)'
        return result

    def rating_transition_analysis(
        self,
        current_rating: str,
        transition_matrix: Dict[str, Dict[str, float]] = None,
    ) -> Dict[str, Any]:
        """One-year rating transition probabilities from a (hard-coded, approximate) matrix."""
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
        base_rating = parse_rating(current_rating).value.replace('+', '').replace('-', '')
        if base_rating in ('CC', 'C'):
            base_rating = 'CCC'
        if base_rating not in matrix:
            raise FIError(f'Rating {current_rating} is not in the transition matrix (AAA..CCC)')
        transitions = matrix[base_rating]
        idx = RATING_ORDER.index(base_rating)
        stay_prob = transitions.get(base_rating, 0)
        upgrade_prob = sum(v for k, v in transitions.items() if RATING_ORDER.index(k) < idx)
        downgrade_prob = sum(v for k, v in transitions.items() if idx < RATING_ORDER.index(k) and k != 'D')
        default_prob = transitions.get('D', 0)
        return {
            'current_rating': current_rating,
            'rating_category': base_rating,
            'transition_probabilities': {k: round(v, 4) for k, v in transitions.items()},
            'stay_probability': round(stay_prob, 4),
            'upgrade_probability': round(upgrade_prob, 4),
            'downgrade_probability': round(downgrade_prob, 4),
            'default_probability': round(default_prob, 4),
            'expected_direction': 'upgrade' if upgrade_prob > downgrade_prob else
                                  'downgrade' if downgrade_prob > upgrade_prob else 'stable',
            'source': TABLE_SOURCE,
        }


ANALYSIS_TYPES = ('expected_loss', 'unexpected_loss', 'credit_var', 'pd_from_spread', 'merton_pd',
                  'historical_pd', 'cumulative_pd', 'spread_analysis', 'recovery_rate', 'rating_transition')


def run_credit_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Main entry point for credit analysis (params['analysis_type'] in ANALYSIS_TYPES)."""
    analysis_type = params.get('analysis_type', 'expected_loss')

    try:
        if analysis_type == 'expected_loss':
            return CreditAnalyzer().calculate_expected_loss(
                exposure=num(params, 'exposure', 1000000),
                probability_of_default=num(params, 'probability_of_default', 0.02),
                recovery_rate=num(params, 'recovery_rate', 0.40))

        elif analysis_type == 'unexpected_loss':
            return CreditAnalyzer().calculate_unexpected_loss(
                exposure=num(params, 'exposure', 1000000),
                probability_of_default=num(params, 'probability_of_default', 0.02),
                recovery_rate=num(params, 'recovery_rate', 0.40),
                lgd_volatility=num(params, 'lgd_volatility', 0.25))

        elif analysis_type == 'credit_var':
            return CreditAnalyzer().calculate_credit_var(
                exposure=num(params, 'exposure', 1000000),
                probability_of_default=num(params, 'probability_of_default', 0.02),
                recovery_rate=num(params, 'recovery_rate', 0.40),
                confidence_level=num(params, 'confidence_level', 0.99),
                correlation=num(params, 'correlation', 0.20))

        elif analysis_type == 'pd_from_spread':
            return DefaultProbabilityModel().pd_from_credit_spread(
                credit_spread=num(params, 'credit_spread', 0.02),
                recovery_rate=num(params, 'recovery_rate', 0.40),
                risk_free_rate=num(params, 'risk_free_rate', 0.03))

        elif analysis_type == 'merton_pd':
            return DefaultProbabilityModel().pd_from_merton_model(
                asset_value=num(params, 'asset_value', 100),
                asset_volatility=num(params, 'asset_volatility', 0.30),
                debt_face_value=num(params, 'debt_face_value', 70),
                risk_free_rate=num(params, 'risk_free_rate', 0.03),
                time_horizon=num(params, 'time_horizon', 1.0),
                asset_return=num(params, 'asset_return', None, allow_none=True))

        elif analysis_type == 'historical_pd':
            return DefaultProbabilityModel().get_historical_pd(
                rating=params.get('rating', 'BBB'),
                years=num(params, 'years', 1, integer=True))

        elif analysis_type == 'cumulative_pd':
            return DefaultProbabilityModel().calculate_cumulative_pd(
                annual_pd=num(params, 'annual_pd', 0.02),
                years=num(params, 'years', 5, integer=True, lo=1, hi=100),
                method=fc.text(params, 'method', 'hazard', choices=('hazard', 'simple')))

        elif analysis_type == 'spread_analysis':
            return CreditAnalyzer().analyze_credit_spread(
                observed_spread=num(params, 'observed_spread', 0.015),
                rating=params.get('rating', 'BBB'),
                maturity=num(params, 'maturity', 5),
                recovery_rate=num(params, 'recovery_rate', 0.40))

        elif analysis_type == 'recovery_rate':
            return CreditAnalyzer().calculate_recovery_rate(
                seniority=params.get('seniority', 'senior_unsecured'),
                collateral_value=num(params, 'collateral_value', None, allow_none=True),
                debt_outstanding=num(params, 'debt_outstanding', None, allow_none=True))

        elif analysis_type == 'rating_transition':
            return CreditAnalyzer().rating_transition_analysis(current_rating=params.get('rating', 'BBB'))

        else:
            return {'error': f'Unknown analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}

    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        logger.error(f"Credit analysis error: {e}")
        return {'error': str(e) or type(e).__name__}


if __name__ == "__main__":
    fc.import_fincept_json()
    fc.emit_usage_or_run(sys.argv, run_credit_analysis, ANALYSIS_TYPES, 'credit_analysis')
