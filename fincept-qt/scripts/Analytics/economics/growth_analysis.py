
"""Economic Growth Analysis Module
=============================

Economic growth analysis and forecasting

===== DATA SOURCES REQUIRED =====
INPUT:
  - Macroeconomic time series data from official sources
  - Central bank policy statements and interest rate data
  - International trade and balance of payments statistics
  - Market indicators and sentiment measures
  - Demographic and structural economic data

OUTPUT:
  - Economic trend analysis and forecasts
  - Policy impact assessment and scenario modeling
  - Market cycle identification and timing analysis
  - Cross-country economic comparisons and rankings
  - Investment recommendations based on economic outlook

PARAMETERS:
  - forecast_horizon: Economic forecast horizon (default: 12 months)
  - confidence_level: Confidence level for predictions (default: 0.90)
  - base_currency: Base currency for analysis (default: 'USD')
  - seasonal_adjustment: Seasonal adjustment method (default: true)
  - lookback_period: Historical analysis period (default: 10 years)
"""

from decimal import Decimal
from typing import Dict, List, Tuple, Optional, Any, Union
from datetime import datetime, timedelta
from .core import EconomicsBase, ValidationError, CalculationError, DataError, CalculationUtils


class GrowthAnalyzer(EconomicsBase):
    """Main economic growth analysis coordinator"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)
        self.productivity = ProductivityAnalyzer(precision, base_currency)
        self.convergence = ConvergenceAnalyzer(precision, base_currency)
        self.demographic = DemographicAnalyzer(precision, base_currency)

    def compare_growth_factors(self, country_type: str, economic_data: Dict[str, Any]) -> Dict[str, Any]:
        """Compare factors favoring and limiting growth in developed vs developing economies"""

        if country_type.lower() not in ['developed', 'developing']:
            raise ValidationError("Country type must be 'developed' or 'developing'")

        if country_type.lower() == 'developed':
            return self._analyze_developed_economy_factors(economic_data)
        else:
            return self._analyze_developing_economy_factors(economic_data)

    # The factor lists below used to carry made-up "scores" (fixed 85 / 70 / 80 / 60 points and
    # unbounded rescalings such as GDP per capita / 1000) and a weighted "net growth potential"
    # built from them. The scales are uncalibrated, so the composite was not a meaningful number.
    # Each factor now reports the indicator it rests on and the value supplied (null if absent).
    _NO_COMPOSITE = ('No composite score: the factors are measured on different, uncalibrated '
                     'scales (and some have no data input), so a weighted sum would not be meaningful.')

    def _factor(self, data: Dict[str, Any], indicator: Optional[str], description: str) -> Dict[str, Any]:
        value = data.get(indicator) if indicator else None
        return {
            'indicator': indicator,
            'value': self.to_decimal(value) if value is not None else None,
            'description': description,
        }

    def _factor_result(self, country_type: str, data: Dict[str, Any], favoring: Dict[str, Any],
                       limiting: Dict[str, Any], drivers: List[str], constraints: List[str]) -> Dict[str, Any]:
        used = {f['indicator'] for f in list(favoring.values()) + list(limiting.values()) if f['indicator']}
        return {
            'country_type': country_type,
            'favoring_factors': favoring,
            'limiting_factors': limiting,
            'composite_favoring_score': None,
            'composite_limiting_score': None,
            'net_growth_potential': None,
            'scoring_note': self._NO_COMPOSITE,
            'missing_inputs': sorted(k for k in used if data.get(k) is None),
            'primary_growth_drivers': drivers,
            'main_constraints': constraints
        }

    def _analyze_developed_economy_factors(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Growth factors for developed economies (qualitative, with the supplied indicators)"""
        f = lambda key, text: self._factor(data, key, text)
        favoring = {
            'technological_innovation': f('rd_spending_percent_gdp', 'High R&D spending drives innovation-led growth'),
            'human_capital': f('education_index', 'Skilled workforce enables productivity gains'),
            'infrastructure_quality': f('infrastructure_quality', 'Quality infrastructure supports efficient markets'),
            'capital_deepening': f(None, 'Existing capital stock supports productivity'),
        }
        limiting = {
            'demographic_constraints': f('old_age_dependency_ratio', 'Aging population reduces labor force growth'),
            'diminishing_returns': f('gdp_per_capita', 'High income levels face diminishing marginal returns'),
            'low_population_growth': f('population_growth_rate', 'Low population growth limits labor force expansion'),
            'mature_economy_constraints': f(None, 'Limited catch-up growth opportunities'),
        }
        return self._factor_result('developed', data, favoring, limiting,
                                   ['technological_innovation', 'human_capital'],
                                   ['demographic_constraints', 'diminishing_returns'])

    def _analyze_developing_economy_factors(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Growth factors for developing economies (qualitative, with the supplied indicators)"""
        f = lambda key, text: self._factor(data, key, text)
        favoring = {
            'catch_up_potential': f('gdp_per_capita', 'Low income levels allow rapid catch-up growth'),
            'demographic_dividend': f('population_growth_rate', 'Young population provides growing workforce'),
            'capital_accumulation': f('savings_rate', 'High savings enable capital investment'),
            'technology_transfer': f('fdi_percent_gdp', 'FDI brings advanced technology and knowledge'),
            'education_expansion': f('secondary_education_enrollment', 'Growing human capital base'),
        }
        limiting = {
            'institutional_weaknesses': f('institutional_quality_index',
                                          'Weak institutions hinder efficient resource allocation'),
            'infrastructure_gaps': f(None, 'Inadequate infrastructure limits productivity'),
            'human_capital_deficits': f('secondary_education_enrollment',
                                        'Limited education reduces productivity potential'),
            'external_dependence': f(None, 'Dependence on external financing and technology'),
        }
        return self._factor_result('developing', data, favoring, limiting,
                                   ['catch_up_potential', 'demographic_dividend'],
                                   ['institutional_weaknesses', 'infrastructure_gaps'])

    def analyze_stock_market_growth_relationship(self, market_data: Dict[str, Any],
                                                 economic_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze relationship between stock market appreciation and sustainable growth rate"""

        # Market data
        stock_returns = [self.to_decimal(r) for r in market_data.get('annual_returns', [])]
        dividend_yield = self.to_decimal(market_data.get('dividend_yield', 0))
        pe_ratio = self.to_decimal(market_data.get('pe_ratio', 0))

        # Economic data
        gdp_growth = self.to_decimal(economic_data.get('gdp_growth_rate', 0))
        productivity_growth = self.to_decimal(economic_data.get('productivity_growth', 0))
        employment_growth = self.to_decimal(economic_data.get('employment_growth', 0))

        if not stock_returns:
            raise ValidationError("Stock returns data is required")

        # Calculate average stock returns
        avg_stock_return = sum(stock_returns) / self.to_decimal(len(stock_returns))

        # Calculate sustainable growth rate (simplified)
        sustainable_growth = gdp_growth

        # Decompose stock returns
        earnings_growth_component = gdp_growth  # Simplified: earnings grow with economy
        dividend_component = dividend_yield
        valuation_change_component = avg_stock_return - earnings_growth_component - dividend_component

        # Analyze long-run relationship
        excess_return = avg_stock_return - sustainable_growth

        return {
            'average_stock_return': avg_stock_return,
            'sustainable_growth_rate': sustainable_growth,
            'excess_return': excess_return,
            'return_decomposition': {
                'earnings_growth': earnings_growth_component,
                'dividend_yield': dividend_component,
                'valuation_change': valuation_change_component
            },
            'long_run_relationship': {
                'description': 'In long run, stock returns should converge to sustainable growth + dividend yield',
                'theoretical_return': sustainable_growth + dividend_yield,
                'current_deviation': avg_stock_return - (sustainable_growth + dividend_yield),
                'sustainable': abs(excess_return) < self.to_decimal(2)  # Within 2% considered sustainable
            },
            'implications_for_investors': self._generate_stock_growth_implications(excess_return, pe_ratio)
        }

    def _generate_stock_growth_implications(self, excess_return: Decimal, pe_ratio: Decimal) -> Dict[str, str]:
        """Generate investment implications from stock-growth relationship"""

        implications = {}

        if excess_return > self.to_decimal(3):
            implications['valuation'] = 'Market may be overvalued relative to economic fundamentals'
            implications['future_returns'] = 'Expected returns may be below historical average'
            implications['risk'] = 'Higher risk of market correction'
        elif excess_return < self.to_decimal(-3):
            implications['valuation'] = 'Market may be undervalued relative to economic fundamentals'
            implications['future_returns'] = 'Expected returns may be above historical average'
            implications['risk'] = 'Potential opportunity for higher returns'
        else:
            implications['valuation'] = 'Market appears fairly valued relative to economic growth'
            implications['future_returns'] = 'Expected returns align with sustainable growth'
            implications['risk'] = 'Balanced risk-return profile'

        # PE ratio implications
        if pe_ratio > self.to_decimal(25):
            implications['pe_signal'] = 'High PE suggests expensive market'
        elif pe_ratio < self.to_decimal(12):
            implications['pe_signal'] = 'Low PE suggests attractive valuations'
        else:
            implications['pe_signal'] = 'PE ratio within normal range'

        return implications

    def potential_gdp_importance(self, gdp_data: Dict[str, Any], investor_type: str) -> Dict[str, Any]:
        """Explain importance of potential GDP for equity and fixed income investors"""

        if gdp_data.get('potential_gdp') is None or gdp_data.get('actual_gdp') is None:
            raise ValidationError("potential_gdp and actual_gdp are required")
        potential_gdp = self.to_decimal(gdp_data['potential_gdp'])
        actual_gdp = self.to_decimal(gdp_data['actual_gdp'])
        if potential_gdp <= 0:
            raise ValidationError("potential_gdp must be positive")
        potential_growth = self.to_decimal(gdp_data.get('potential_growth_rate', 0))

        # Calculate output gap
        output_gap = ((actual_gdp - potential_gdp) / potential_gdp) * self.to_decimal(100)

        if investor_type.lower() == 'equity':
            return self._equity_investor_implications(output_gap, potential_growth, gdp_data)
        elif investor_type.lower() == 'fixed_income':
            return self._fixed_income_implications(output_gap, potential_growth, gdp_data)
        else:
            # Return both
            return {
                'equity_implications': self._equity_investor_implications(output_gap, potential_growth, gdp_data),
                'fixed_income_implications': self._fixed_income_implications(output_gap, potential_growth, gdp_data),
                'output_gap': output_gap,
                'potential_growth_rate': potential_growth
            }

    def _equity_investor_implications(self, output_gap: Decimal, potential_growth: Decimal,
                                      gdp_data: Dict[str, Any]) -> Dict[str, Any]:
        """Implications of potential GDP for equity investors"""

        return {
            'earnings_growth_potential': {
                'description': 'Potential GDP growth sets upper bound for long-term earnings growth',
                'implication': f'Long-term earnings growth limited to ~{potential_growth:.1f}% annually',
                'current_position': 'Above potential' if output_gap > 0 else 'Below potential'
            },
            'cyclical_positioning': {
                'output_gap': output_gap,
                'interpretation': self._interpret_output_gap_equity(output_gap),
                'strategy': self._equity_strategy_from_gap(output_gap)
            },
            'sector_implications': {
                'cyclical_sectors': 'Sensitive to output gap fluctuations',
                'defensive_sectors': 'Less sensitive, focus on long-term potential growth',
                'growth_sectors': 'Beneficiaries of productivity improvements driving potential growth'
            },
            'valuation_framework': {
                'sustainable_pe': f'Long-term PE ratios should reflect potential growth of {potential_growth:.1f}%',
                'cyclical_adjustment': 'Adjust for temporary deviations from potential'
            }
        }

    def _fixed_income_implications(self, output_gap: Decimal, potential_growth: Decimal,
                                   gdp_data: Dict[str, Any]) -> Dict[str, Any]:
        """Implications of potential GDP for fixed income investors"""

        inflation_rate = self.to_decimal(gdp_data.get('inflation_rate', 0))

        return {
            'monetary_policy_stance': {
                'output_gap': output_gap,
                'policy_implication': self._monetary_policy_from_gap(output_gap),
                'interest_rate_direction': self._interest_rate_direction(output_gap)
            },
            'inflation_expectations': {
                'gap_pressure': 'Positive gap = inflationary pressure' if output_gap > 0 else 'Negative gap = disinflationary pressure',
                'long_term_anchor': (f'Long-run nominal yields anchor on potential real growth '
                                     f'({potential_growth:.1f}%) plus expected inflation'),
                'current_risk': 'Inflation risk elevated' if output_gap > self.to_decimal(
                    2) else 'Inflation risk contained'
            },
            'yield_curve_implications': {
                'short_end': 'Driven by central bank response to output gap',
                'long_end': 'Anchored by potential growth and inflation expectations',
                'curve_shape': self._yield_curve_shape(output_gap)
            },
            'credit_risk_assessment': {
                'corporate_earnings': 'Tied to actual vs potential GDP performance',
                'default_risk': 'Lower when economy operates near potential',
                'recovery_rates': 'Higher potential growth supports better recovery values'
            }
        }

    def _interpret_output_gap_equity(self, gap: Decimal) -> str:
        """Interpret output gap for equity investors"""
        if gap > self.to_decimal(2):
            return "Economy overheating - potential for policy tightening and earnings pressure"
        elif gap > self.to_decimal(0):
            return "Economy above potential - supporting earnings but watch for inflation"
        elif gap > self.to_decimal(-2):
            return "Economy near potential - balanced growth environment"
        else:
            return "Economy below potential - room for growth but current earnings pressure"

    def _equity_strategy_from_gap(self, gap: Decimal) -> str:
        """Suggest equity strategy based on output gap"""
        if gap > self.to_decimal(2):
            return "Consider defensive positioning, watch for policy tightening"
        elif gap > self.to_decimal(0):
            return "Balanced approach, favor quality cyclicals"
        else:
            return "Growth opportunities available, consider cyclical exposure"

    def _monetary_policy_from_gap(self, gap: Decimal) -> str:
        """Predict monetary policy stance from output gap"""
        if gap > self.to_decimal(1):
            return "Likely tightening bias"
        elif gap > self.to_decimal(-1):
            return "Neutral stance"
        else:
            return "Likely easing bias"

    def _interest_rate_direction(self, gap: Decimal) -> str:
        """Predict interest rate direction"""
        if gap > self.to_decimal(1):
            return "Upward pressure"
        elif gap > self.to_decimal(-1):
            return "Stable"
        else:
            return "Downward pressure"

    def _yield_curve_shape(self, gap: Decimal) -> str:
        """Predict yield curve shape"""
        if gap > self.to_decimal(2):
            return "Flattening risk (short rates rising faster)"
        elif gap < self.to_decimal(-2):
            return "Steepening (short rates falling faster)"
        else:
            return "Stable shape"

    def forecast_potential_gdp(self, historical_data: Dict[str, Any],
                               forecast_assumptions: Dict[str, Any]) -> Dict[str, Any]:
        """Forecast potential GDP using growth accounting relations"""

        # Historical data
        labor_force_growth = [self.to_decimal(x) for x in historical_data.get('labor_force_growth', [])]
        productivity_growth = [self.to_decimal(x) for x in historical_data.get('productivity_growth', [])]
        capital_growth = [self.to_decimal(x) for x in historical_data.get('capital_growth', [])]

        # Forecast assumptions
        forecast_periods = int(forecast_assumptions.get('periods', 5))
        labor_growth_forecast = self.to_decimal(forecast_assumptions.get('labor_growth_rate', 0))
        productivity_growth_forecast = self.to_decimal(forecast_assumptions.get('productivity_growth_rate', 0))
        capital_growth_forecast = self.to_decimal(forecast_assumptions.get('capital_growth_rate', 0))

        # Growth accounting: Y = A * K^α * L^(1-α)
        # Growth rate: g_Y = g_A + α * g_K + (1-α) * g_L
        alpha = self.to_decimal(forecast_assumptions.get('capital_share', 0.3))  # Capital's share of output

        # Calculate historical potential growth
        historical_potential = []
        min_length = min(len(labor_force_growth), len(productivity_growth), len(capital_growth))

        for i in range(min_length):
            potential_growth = (productivity_growth[i] +
                                alpha * capital_growth[i] +
                                (self.to_decimal(1) - alpha) * labor_force_growth[i])
            historical_potential.append(potential_growth)

        # Forecast potential GDP growth
        forecast_potential_growth = (productivity_growth_forecast +
                                     alpha * capital_growth_forecast +
                                     (self.to_decimal(1) - alpha) * labor_growth_forecast)

        # Calculate trend components
        trend_productivity = sum(productivity_growth) / self.to_decimal(
            len(productivity_growth)) if productivity_growth else self.to_decimal(0)
        trend_labor = sum(labor_force_growth) / self.to_decimal(
            len(labor_force_growth)) if labor_force_growth else self.to_decimal(0)
        trend_capital = sum(capital_growth) / self.to_decimal(
            len(capital_growth)) if capital_growth else self.to_decimal(0)

        return {
            'growth_accounting_framework': {
                'formula': 'GDP Growth = Productivity Growth + α×Capital Growth + (1-α)×Labor Growth',
                'capital_share_alpha': alpha,
                'labor_share': self.to_decimal(1) - alpha
            },
            'historical_analysis': {
                'historical_potential_growth': historical_potential,
                'average_historical_potential': sum(historical_potential) / self.to_decimal(
                    len(historical_potential)) if historical_potential else self.to_decimal(0),
                'trend_components': {
                    'productivity': trend_productivity,
                    'labor_force': trend_labor,
                    'capital_stock': trend_capital
                }
            },
            'forecast': {
                'periods': forecast_periods,
                'potential_gdp_growth': forecast_potential_growth,
                'components': {
                    'productivity_contribution': productivity_growth_forecast,
                    'capital_contribution': alpha * capital_growth_forecast,
                    'labor_contribution': (self.to_decimal(1) - alpha) * labor_growth_forecast
                },
                'assumptions': forecast_assumptions
            },
            'sensitivity_analysis': self._sensitivity_analysis_potential_gdp(
                alpha, productivity_growth_forecast, capital_growth_forecast, labor_growth_forecast
            )
        }

    def _sensitivity_analysis_potential_gdp(self, alpha: Decimal, prod_growth: Decimal,
                                            cap_growth: Decimal, lab_growth: Decimal) -> Dict[str, Any]:
        """Sensitivity analysis for potential GDP forecast"""

        base_growth = prod_growth + alpha * cap_growth + (self.to_decimal(1) - alpha) * lab_growth

        # Test scenarios
        scenarios = {
            'productivity_high': prod_growth + self.to_decimal(0.005),  # +0.5pp
            'productivity_low': prod_growth - self.to_decimal(0.005),  # -0.5pp
            'capital_high': cap_growth + self.to_decimal(0.01),  # +1pp
            'capital_low': cap_growth - self.to_decimal(0.01),  # -1pp
            'labor_high': lab_growth + self.to_decimal(0.005),  # +0.5pp
            'labor_low': lab_growth - self.to_decimal(0.005)  # -0.5pp
        }

        sensitivity_results = {}
        for scenario, value in scenarios.items():
            if 'productivity' in scenario:
                new_growth = value + alpha * cap_growth + (self.to_decimal(1) - alpha) * lab_growth
            elif 'capital' in scenario:
                new_growth = prod_growth + alpha * value + (self.to_decimal(1) - alpha) * lab_growth
            else:  # labor scenario
                new_growth = prod_growth + alpha * cap_growth + (self.to_decimal(1) - alpha) * value

            sensitivity_results[scenario] = {
                'growth_rate': new_growth,
                'change_from_base': new_growth - base_growth
            }

        return {
            'base_case': base_growth,
            'scenarios': sensitivity_results,
            'most_sensitive_to': max(sensitivity_results.items(),
                                     key=lambda x: abs(x[1]['change_from_base']))[0]
        }

    def calculate(self, analysis_type: str, **kwargs) -> Dict[str, Any]:
        """Main calculation dispatcher"""

        calculations = {
            'growth_factors': lambda: self.compare_growth_factors(
                kwargs['country_type'], kwargs['economic_data']
            ),
            'stock_growth_relationship': lambda: self.analyze_stock_market_growth_relationship(
                kwargs['market_data'], kwargs['economic_data']
            ),
            'potential_gdp_importance': lambda: self.potential_gdp_importance(
                kwargs['gdp_data'], kwargs.get('investor_type', 'both')
            ),
            'forecast_potential_gdp': lambda: self.forecast_potential_gdp(
                kwargs['historical_data'], kwargs['forecast_assumptions']
            )
        }

        if analysis_type not in calculations:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = calculations[analysis_type]()
        result['metadata'] = self.get_metadata()
        result['analysis_type'] = analysis_type

        return result


class ProductivityAnalyzer(EconomicsBase):
    """Capital deepening vs technological progress analysis"""

    def analyze_capital_deepening_vs_technology(self, productivity_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze effects of capital deepening vs technological progress"""

        # Extract data
        capital_per_worker = self.to_decimal(productivity_data.get('capital_per_worker_growth', 0))
        total_factor_productivity = self.to_decimal(productivity_data.get('tfp_growth', 0))
        labor_productivity = self.to_decimal(productivity_data.get('labor_productivity_growth', 0))

        # Production function: Y = A * K^α * L^(1-α)
        # Labor productivity: Y/L = A * (K/L)^α
        # Growth: g(Y/L) = g(A) + α * g(K/L)

        alpha = self.to_decimal(0.3)  # Typical capital share

        # Decompose labor productivity growth
        capital_deepening_contribution = alpha * capital_per_worker
        technology_contribution = total_factor_productivity

        # Verify decomposition
        implied_productivity_growth = capital_deepening_contribution + technology_contribution
        residual = labor_productivity - implied_productivity_growth

        return {
            'decomposition': {
                'labor_productivity_growth': labor_productivity,
                'capital_deepening_contribution': capital_deepening_contribution,
                'technology_contribution': technology_contribution,
                'residual': residual
            },
            'relative_importance': {
                'capital_deepening_share': (capital_deepening_contribution / labor_productivity * self.to_decimal(
                    100)) if labor_productivity != 0 else self.to_decimal(0),
                'technology_share': (technology_contribution / labor_productivity * self.to_decimal(
                    100)) if labor_productivity != 0 else self.to_decimal(0)
            },
            'economic_implications': {
                'capital_deepening': {
                    'description': 'Increasing capital per worker',
                    'effects': 'Diminishing returns, temporary boost to productivity',
                    'sustainability': 'Limited by diminishing marginal returns',
                    'policy_focus': 'Investment incentives, savings rates'
                },
                'technological_progress': {
                    'description': 'Improvements in total factor productivity',
                    'effects': 'Sustainable productivity gains, no diminishing returns',
                    'sustainability': 'Can sustain long-term growth',
                    'policy_focus': 'R&D investment, education, innovation'
                }
            },
            'growth_sustainability': self._assess_growth_sustainability(
                capital_deepening_contribution, technology_contribution
            )
        }

    def _assess_growth_sustainability(self, capital_contrib: Decimal, tech_contrib: Decimal) -> Dict[str, Any]:
        """Assess sustainability of growth based on contributions"""

        total_contrib = capital_contrib + tech_contrib

        if total_contrib == 0:
            return {'assessment': 'No productivity growth', 'sustainability': 'Poor'}

        tech_share = tech_contrib / total_contrib

        if tech_share > self.to_decimal(0.7):
            sustainability = 'High'
            assessment = 'Technology-driven growth is highly sustainable'
        elif tech_share > self.to_decimal(0.4):
            sustainability = 'Moderate'
            assessment = 'Balanced growth with good sustainability prospects'
        else:
            sustainability = 'Low'
            assessment = 'Capital-dependent growth faces diminishing returns'

        return {
            'assessment': assessment,
            'sustainability': sustainability,
            'technology_share': tech_share * self.to_decimal(100),
            'recommendations': self._generate_sustainability_recommendations(tech_share)
        }

    def _generate_sustainability_recommendations(self, tech_share: Decimal) -> List[str]:
        """Generate recommendations based on technology share"""

        recommendations = []

        if tech_share < self.to_decimal(0.3):
            recommendations.extend([
                'Increase R&D spending to boost technological progress',
                'Invest in education and human capital development',
                'Encourage innovation through patent protection and incentives',
                'Reduce reliance on pure capital accumulation'
            ])
        elif tech_share < self.to_decimal(0.6):
            recommendations.extend([
                'Maintain balanced approach to capital and technology',
                'Continue investing in both physical and human capital',
                'Focus on technology transfer and adoption'
            ])
        else:
            recommendations.extend([
                'Sustain high-technology focus',
                'Ensure adequate capital to complement technology',
                'Maintain competitive advantage in innovation'
            ])

        return recommendations

    def calculate(self, **kwargs) -> Dict[str, Any]:
        """Calculate productivity analysis"""
        return self.analyze_capital_deepening_vs_technology(kwargs['productivity_data'])


class ConvergenceAnalyzer(EconomicsBase):
    """Economic convergence hypotheses analysis"""

    def test_convergence_hypotheses(self, country_data: List[Dict[str, Any]],
                                    convergence_type: str = 'beta',
                                    years: Optional[float] = None,
                                    growth_rate_units: str = 'percent') -> Dict[str, Any]:
        """Test convergence hypotheses (beta and sigma convergence).
        years = length T of the window over which avg_growth_rate was measured (needed for the
        beta-convergence speed and half-life); growth_rate_units = 'percent' (3.5) or 'fraction'."""

        if convergence_type not in ['beta', 'sigma', 'both']:
            raise ValidationError("Convergence type must be 'beta', 'sigma', or 'both'")

        results = {}

        if convergence_type in ['beta', 'both']:
            results['beta_convergence'] = self._test_beta_convergence(country_data, years, growth_rate_units)

        if convergence_type in ['sigma', 'both']:
            results['sigma_convergence'] = self._test_sigma_convergence(country_data)

        results['convergence_theories'] = self._explain_convergence_theories()

        return results

    def _test_beta_convergence(self, country_data: List[Dict[str, Any]], years: Optional[float] = None,
                               growth_rate_units: str = 'percent') -> Dict[str, Any]:
        """Barro beta convergence: OLS of the average annual growth rate g_i over the window on
        ln(initial GDP per capita): g_i = a + b ln(y_i0). Convergence means b < 0; with window
        length T the implied speed is beta = -ln(1 + bT)/T and the half-life ln 2 / beta
        (Barro & Sala-i-Martin; g as a fraction per year)."""

        if growth_rate_units not in ('percent', 'fraction'):
            raise ValidationError("growth_rate_units must be 'percent' or 'fraction'")
        scale = Decimal(100) if growth_rate_units == 'percent' else Decimal(1)

        ln_initial = []
        growth_rates = []
        for country in country_data:
            y0 = self.to_decimal(country['initial_gdp_per_capita'])
            if y0 <= 0:
                raise ValidationError("initial_gdp_per_capita must be positive")
            ln_initial.append(y0.ln())
            growth_rates.append(self.to_decimal(country['avg_growth_rate']) / scale)

        if len(ln_initial) < 3:
            raise ValidationError("At least 3 countries required for convergence analysis")

        n = Decimal(len(ln_initial))
        mx = sum(ln_initial) / n
        my = sum(growth_rates) / n
        sxx = sum((x - mx) ** 2 for x in ln_initial)
        if sxx == 0:
            raise ValidationError("initial_gdp_per_capita must differ across countries")
        slope = sum((x - mx) * (y - my) for x, y in zip(ln_initial, growth_rates)) / sxx
        correlation = self._calculate_correlation(ln_initial, growth_rates)

        speed = half_life = None
        speed_note = None
        if years is None:
            speed_note = 'needs years (length of the growth window) for speed and half-life'
        else:
            T = Decimal(str(years))
            if T <= 0:
                raise ValidationError("years must be positive")
            inner = Decimal(1) + slope * T
            if slope >= 0:
                speed_note = 'no convergence (slope >= 0): speed and half-life undefined'
            elif inner <= 0:
                speed_note = 'slope too steep for the window (1 + bT <= 0): speed undefined'
            else:
                speed = -inner.ln() / T
                half_life = Decimal(2).ln() / speed

        return {
            'regression_slope_b': slope,
            'slope_units': 'change in annual growth (fraction) per unit of ln(initial GDP per capita)',
            'correlation_coefficient': correlation,
            'convergence_exists': slope < 0,
            'convergence_speed': speed,
            'half_life_years': half_life,
            'speed_note': speed_note,
            'interpretation': self._interpret_beta_convergence(correlation),
            'countries_analyzed': len(country_data)
        }

    def _test_sigma_convergence(self, country_data: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Sigma convergence: is the cross-country dispersion of LOG GDP per capita (sample
        standard deviation of ln y) falling? (Dispersion of levels rises under uniform growth.)"""

        # Extract GDP per capita over time
        time_periods = {}

        for country in country_data:
            for year, gdp in country.get('gdp_time_series', {}).items():
                value = self.to_decimal(gdp)
                if value <= 0:
                    raise ValidationError("gdp_time_series values must be positive")
                time_periods.setdefault(year, []).append(value.ln())

        # Calculate standard deviation over time
        dispersions = {}
        for year, gdp_values in time_periods.items():
            if len(gdp_values) > 1:
                mean_gdp = sum(gdp_values) / self.to_decimal(len(gdp_values))
                variance = sum((x - mean_gdp) ** 2 for x in gdp_values) / self.to_decimal(len(gdp_values) - 1)
                dispersions[year] = variance.sqrt()

        # Check if dispersion is decreasing
        years = sorted(dispersions.keys())
        if len(years) < 2:
            raise ValidationError("At least 2 time periods required for sigma convergence")

        initial_dispersion = dispersions[years[0]]
        final_dispersion = dispersions[years[-1]]
        dispersion_change = ((final_dispersion - initial_dispersion) / initial_dispersion
                             if initial_dispersion != 0 else None)

        return {
            'dispersion_measure': 'sample standard deviation of ln(GDP per capita) across countries',
            'initial_dispersion': initial_dispersion,
            'final_dispersion': final_dispersion,
            'dispersion_change_percent': (dispersion_change * self.to_decimal(100)
                                          if dispersion_change is not None else None),
            'sigma_convergence_exists': final_dispersion < initial_dispersion,
            'time_periods_analyzed': len(years),
            'dispersion_trend': 'Decreasing' if final_dispersion < initial_dispersion else 'Increasing'
        }

    def _calculate_correlation(self, x_values: List[Decimal], y_values: List[Decimal]) -> Decimal:
        """Calculate correlation coefficient"""
        n = len(x_values)
        if n != len(y_values) or n < 2:
            return self.to_decimal(0)

        mean_x = sum(x_values) / self.to_decimal(n)
        mean_y = sum(y_values) / self.to_decimal(n)

        numerator = sum((x_values[i] - mean_x) * (y_values[i] - mean_y) for i in range(n))

        sum_sq_x = sum((x - mean_x) ** 2 for x in x_values)
        sum_sq_y = sum((y - mean_y) ** 2 for y in y_values)

        denominator = (sum_sq_x * sum_sq_y).sqrt()

        return numerator / denominator if denominator != 0 else self.to_decimal(0)

    def _interpret_beta_convergence(self, correlation: Decimal) -> str:
        """Interpret beta convergence results"""
        if correlation < self.to_decimal(-0.5):
            return "Strong beta convergence: Poor countries growing significantly faster"
        elif correlation < self.to_decimal(-0.3):
            return "Moderate beta convergence: Some catch-up effect observed"
        elif correlation < self.to_decimal(-0.1):
            return "Weak beta convergence: Limited catch-up effect"
        else:
            return "No beta convergence: No systematic catch-up by poor countries"

    def _explain_convergence_theories(self) -> Dict[str, Any]:
        """Explain convergence theories"""
        return {
            'neoclassical_theory': {
                'prediction': 'Unconditional convergence due to diminishing returns',
                'mechanism': 'Poor countries have higher marginal returns to capital',
                'assumptions': 'Same technology, preferences, institutions',
                'reality': 'Limited empirical support for unconditional convergence'
            },
            'conditional_convergence': {
                'prediction': 'Convergence to country-specific steady states',
                'mechanism': 'Countries converge to own equilibrium based on fundamentals',
                'factors': 'Savings rates, population growth, technology, institutions',
                'evidence': 'Stronger empirical support'
            },
            'endogenous_growth': {
                'prediction': 'Divergence possible due to increasing returns',
                'mechanism': 'Knowledge spillovers, human capital externalities',
                'implications': 'Rich countries may grow faster permanently',
                'policy': 'Government intervention may be needed'
            }
        }

    def calculate(self, convergence_type: str = 'both', **kwargs) -> Dict[str, Any]:
        """Calculate convergence analysis"""
        return self.test_convergence_hypotheses(kwargs['country_data'], convergence_type,
                                                kwargs.get('years'), kwargs.get('growth_rate_units', 'percent'))


class DemographicAnalyzer(EconomicsBase):
    """Demographics, immigration, and labor force participation analysis"""

    def analyze_demographic_impact(self, demographic_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze how demographics affect economic growth"""

        # Extract demographic data
        population_growth = self.to_decimal(demographic_data.get('population_growth_rate', 0))
        working_age_share = self.to_decimal(demographic_data.get('working_age_population_share', 0))
        dependency_ratio = self.to_decimal(demographic_data.get('dependency_ratio', 0))
        life_expectancy = self.to_decimal(demographic_data.get('life_expectancy', 0))
        fertility_rate = self.to_decimal(demographic_data.get('fertility_rate', 0))

        # Immigration data
        immigration_rate = self.to_decimal(demographic_data.get('net_immigration_rate', 0))
        immigrant_age_profile = demographic_data.get('immigrant_avg_age', 30)

        # Labor force data
        labor_force_participation = self.to_decimal(demographic_data.get('labor_force_participation_rate', 0))
        female_participation = self.to_decimal(demographic_data.get('female_labor_participation', 0))

        expected = ['population_growth_rate', 'working_age_population_share', 'dependency_ratio',
                    'life_expectancy', 'fertility_rate', 'net_immigration_rate',
                    'labor_force_participation_rate', 'female_labor_participation']
        missing = [k for k in expected if demographic_data.get(k) is None]
        if len(missing) == len(expected):
            raise ValidationError("demographic_data is empty: supply at least the working-age share, "
                                  "dependency ratio and fertility rate")

        return {
            # Absent inputs are read as 0 by the sections below; they are listed here so a
            # narrative built on a 0 is recognisable.
            'missing_inputs': missing,
            'demographic_dividend_analysis': self._analyze_demographic_dividend(
                working_age_share, dependency_ratio, population_growth
            ),
            'immigration_impact': self._analyze_immigration_impact(
                immigration_rate, immigrant_age_profile, labor_force_participation
            ),
            'labor_force_dynamics': self._analyze_labor_force_participation(
                labor_force_participation, female_participation, working_age_share
            ),
            'long_term_sustainability': self._assess_demographic_sustainability(
                fertility_rate, life_expectancy, dependency_ratio
            ),
            'policy_implications': self._generate_demographic_policy_recommendations(
                fertility_rate, dependency_ratio, immigration_rate, female_participation
            )
        }

    def _analyze_demographic_dividend(self, working_age_share: Decimal,
                                      dependency_ratio: Decimal, pop_growth: Decimal) -> Dict[str, Any]:
        """Analyze demographic dividend potential"""

        # Demographic dividend occurs when working age population grows faster than dependents
        dividend_potential = working_age_share / dependency_ratio if dependency_ratio > 0 else self.to_decimal(0)

        if working_age_share > self.to_decimal(65) and dependency_ratio < self.to_decimal(50):
            dividend_stage = "Peak dividend period"
            growth_impact = "High positive impact on growth"
        elif working_age_share > self.to_decimal(60):
            dividend_stage = "Dividend period"
            growth_impact = "Positive impact on growth"
        elif working_age_share < self.to_decimal(55):
            dividend_stage = "Post-dividend or pre-dividend"
            growth_impact = "Limited or negative growth impact"
        else:
            dividend_stage = "Transition period"
            growth_impact = "Moderate growth impact"

        return {
            'working_age_share': working_age_share,
            'dependency_ratio': dependency_ratio,
            'dividend_potential_score': dividend_potential,
            'dividend_stage': dividend_stage,
            'growth_impact': growth_impact,
            'duration_estimate': self._estimate_dividend_duration(working_age_share, pop_growth),
            'policy_window': "15-30 years to capitalize on demographic dividend"
        }

    def _analyze_immigration_impact(self, immigration_rate: Decimal,
                                    avg_age: float, lfpr: Decimal) -> Dict[str, Any]:
        """Analyze immigration impact on growth"""

        # Young immigrants have higher growth impact
        age_factor = max(self.to_decimal(0), self.to_decimal(50 - avg_age) / self.to_decimal(20))

        # Immigration impact on labor force
        labor_force_boost = immigration_rate * lfpr / self.to_decimal(100)

        # Fiscal impact (simplified)
        if avg_age < 35:
            fiscal_impact = "Positive (young workers, long contribution period)"
        elif avg_age < 50:
            fiscal_impact = "Neutral to positive"
        else:
            fiscal_impact = "Potentially negative (shorter contribution period)"

        return {
            'immigration_rate': immigration_rate,
            'average_immigrant_age': avg_age,
            'age_factor_score': age_factor,
            'labor_force_contribution': labor_force_boost,
            'fiscal_impact_assessment': fiscal_impact,
            'skill_considerations': "High-skilled immigration provides greater growth benefits",
            'integration_factors': "Language, credential recognition affect productivity"
        }

    def _analyze_labor_force_participation(self, overall_lfpr: Decimal,
                                           female_lfpr: Decimal, working_age_share: Decimal) -> Dict[str, Any]:
        """Analyze labor force participation trends"""

        # Potential labor force growth from increased participation
        max_lfpr = self.to_decimal(85)  # Realistic maximum
        participation_gap = max_lfpr - overall_lfpr

        # Female participation potential (often lower than male)
        female_potential = self.to_decimal(80) - female_lfpr  # Assuming male rate ~85%

        return {
            'current_participation_rate': overall_lfpr,
            'female_participation_rate': female_lfpr,
            'participation_gap': participation_gap,
            'participation_gap_basis': 'percentage points below an assumed 85 % maximum participation rate',
            'female_participation_potential': female_potential,
            # Workers added if participation rose to the 85 % ceiling, as percentage points of the
            # TOTAL population (gap x working-age share); and as % of today's labour force.
            'growth_potential_from_participation': participation_gap * working_age_share / self.to_decimal(100),
            'growth_potential_from_participation_unit': 'percentage points of total population',
            'labor_force_increase_pct': (participation_gap / overall_lfpr * self.to_decimal(100)
                                         if overall_lfpr > 0 else None),
            'policy_levers': [
                'Childcare support to increase female participation',
                'Flexible work arrangements',
                'Education and skills training',
                'Retirement age adjustments for aging societies'
            ]
        }

    def _assess_demographic_sustainability(self, fertility_rate: Decimal,
                                           life_expectancy: Decimal, dependency_ratio: Decimal) -> Dict[str, Any]:
        """Assess long-term demographic sustainability"""

        replacement_rate = self.to_decimal(2.1)  # Fertility rate needed for population stability

        if fertility_rate < self.to_decimal(1.5):
            sustainability_level = "Low - Population decline likely"
            policy_urgency = "High"
        elif fertility_rate < replacement_rate:
            sustainability_level = "Moderate - Below replacement rate"
            policy_urgency = "Medium"
        else:
            sustainability_level = "High - Above replacement rate"
            policy_urgency = "Low"

        # Aging challenge
        if dependency_ratio > self.to_decimal(60):
            aging_challenge = "Severe aging burden"
        elif dependency_ratio > self.to_decimal(45):
            aging_challenge = "Moderate aging challenge"
        else:
            aging_challenge = "Manageable dependency ratio"

        return {
            'fertility_rate': fertility_rate,
            'replacement_rate': replacement_rate,
            'fertility_gap': fertility_rate - replacement_rate,
            'life_expectancy': life_expectancy,
            'dependency_ratio': dependency_ratio,
            'sustainability_assessment': sustainability_level,
            'aging_challenge': aging_challenge,
            'policy_urgency': policy_urgency,
            'time_horizon': "Demographic changes take 20-30 years to materialize"
        }

    def _estimate_dividend_duration(self, working_age_share: Decimal, pop_growth: Decimal) -> str:
        """Estimate demographic dividend duration"""
        if working_age_share > self.to_decimal(65):
            return "10-20 years remaining"
        elif working_age_share > self.to_decimal(60):
            return "20-30 years remaining"
        else:
            return "Dividend period ending or not yet started"

    def _generate_demographic_policy_recommendations(self, fertility_rate: Decimal,
                                                     dependency_ratio: Decimal,
                                                     immigration_rate: Decimal,
                                                     female_lfpr: Decimal) -> List[str]:
        """Generate policy recommendations based on demographic profile"""

        recommendations = []

        # Fertility-based recommendations
        if fertility_rate < self.to_decimal(1.8):
            recommendations.extend([
                'Implement family-friendly policies (parental leave, childcare)',
                'Provide financial incentives for families',
                'Improve work-life balance policies'
            ])

        # Aging-based recommendations
        if dependency_ratio > self.to_decimal(50):
            recommendations.extend([
                'Gradually increase retirement age',
                'Reform pension systems for sustainability',
                'Invest in elderly care infrastructure'
            ])

        # Immigration recommendations
        if immigration_rate < self.to_decimal(0.5) and dependency_ratio > self.to_decimal(45):
            recommendations.extend([
                'Develop skilled immigration programs',
                'Improve integration services',
                'Streamline immigration processes'
            ])

        # Female participation recommendations
        if female_lfpr < self.to_decimal(70):
            recommendations.extend([
                'Expand affordable childcare',
                'Promote flexible work arrangements',
                'Address gender wage gaps'
            ])

        return recommendations

    def calculate(self, **kwargs) -> Dict[str, Any]:
        """Calculate demographic analysis"""
        return self.analyze_demographic_impact(kwargs['demographic_data'])