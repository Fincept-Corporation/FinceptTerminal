
"""Economic Trade Geopolitics Module
=============================

Trade and geopolitical risk analysis

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

import math
from decimal import Decimal
from typing import Dict, List, Any, Optional, Tuple
try:
    from .core import EconomicsBase, ValidationError
except ImportError:
    # Launched as a plain script (PythonRunner does this when a parent directory has no
    # __init__.py, as with Analytics/) so there is no parent package for the relative
    # import. The script's own directory is sys.path[0], so import core.py directly.
    import os
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from core import EconomicsBase, ValidationError


class TradeAnalyzer(EconomicsBase):
    """International trade analysis and policy assessment"""

    def analyze_trade_benefits_costs(self, trade_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze benefits and costs of international trade"""
        return {
            'trade_benefits': {
                'efficiency_gains': {
                    'comparative_advantage': 'Countries specialize in relative strengths',
                    'resource_allocation': 'More efficient global resource use',
                    'scale_economies': 'Larger markets enable economies of scale',
                    'quantitative_benefit': self._calculate_trade_gains(trade_data)
                },
                'consumer_benefits': {
                    'variety': 'Greater product variety and choice',
                    'lower_prices': 'Increased competition reduces prices',
                    'quality_improvement': 'Competition drives quality improvements',
                    'consumer_surplus_gain': self._estimate_consumer_surplus_gain(trade_data)
                },
                'growth_benefits': {
                    'technology_transfer': 'Access to foreign technology and knowledge',
                    'productivity_spillovers': 'Learning from foreign competition',
                    'investment_flows': 'Foreign direct investment attraction',
                    'innovation_incentives': 'Competition spurs innovation'
                }
            },
            'trade_costs': {
                'adjustment_costs': {
                    'job_displacement': 'Workers in import-competing industries lose jobs',
                    'regional_impacts': 'Concentrated effects in specific regions',
                    'skill_premiums': 'Wage gaps between skilled/unskilled workers',
                    'adjustment_period': 'Time and cost of worker reallocation'
                },
                'distributional_effects': {
                    'income_inequality': 'May worsen within-country inequality',
                    'factor_returns': 'Changes in wages, profits, land rents',
                    'sectoral_shifts': 'Decline of import-competing sectors',
                    'compensation_needs': 'Required support for affected workers'
                },
                'vulnerability_risks': {
                    'import_dependence': 'Reliance on foreign suppliers',
                    'economic_security': 'Potential supply chain disruptions',
                    'policy_autonomy': 'Constraints on domestic policy flexibility'
                }
            },
            'net_welfare_assessment': self._assess_net_welfare_impact(trade_data),
            # Numbers for the headline view (the sentences above stay for context).
            'headline': {
                'efficiency_gain_pct_gdp': self._calculate_trade_gains(trade_data),
                'consumer_surplus_gain_pct': self._estimate_consumer_surplus_gain(trade_data),
                'trade_volume_pct_gdp': self._num(trade_data, 'trade_volume_gdp', 0, lo=0, hi=200),
            },
            'model': self._benefits_model(trade_data),
        }

    def analyze_trade_restrictions(self, restriction_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze different types of trade restrictions and their impacts"""
        return {
            'tariffs': {
                'mechanism': 'Tax on imports',
                'economic_effects': self._analyze_tariff_effects(restriction_data),
                'revenue_generation': 'Provides government revenue',
                'protection_level': 'Proportional to tariff rate',
                'welfare_impact': 'Net welfare loss (deadweight loss)'
            },
            'quotas': {
                'mechanism': 'Quantity limit on imports',
                'economic_effects': self._analyze_quota_effects(),
                'revenue_generation': 'No government revenue (quota rents to importers)',
                'protection_level': 'Fixed quantity protection',
                'welfare_impact': 'Similar to tariffs but different rent distribution'
            },
            'export_subsidies': {
                'mechanism': 'Government payments to exporters',
                'economic_effects': self._analyze_subsidy_effects(restriction_data),
                'revenue_generation': 'Costs government revenue',
                'protection_level': 'Supports domestic producers',
                'welfare_impact': 'Welfare loss in subsidizing country'
            },
            'non_tariff_barriers': {
                'types': ['Technical standards', 'Sanitary measures', 'Administrative procedures'],
                'effects': 'Hidden protection, often more restrictive than tariffs',
                'measurement_difficulty': 'Hard to quantify economic impact',
                'welfare_impact': 'Potentially large welfare costs'
            },
            'restriction_comparison': self._compare_trade_restrictions(),
            'optimal_policy_recommendation': self._recommend_trade_policy(restriction_data),
            'headline': self._restriction_headline(restriction_data),
            'model': self._restriction_model(restriction_data),
        }

    def analyze_trading_blocs(self, bloc_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze trading blocs, common markets, and economic unions"""
        integration_types = {
            'free_trade_area': {
                'definition': 'Eliminate tariffs among members, keep individual external tariffs',
                'examples': ['NAFTA/USMCA', 'ASEAN FTA'],
                'advantages': ['Trade creation', 'Market access', 'Political cooperation'],
                'disadvantages': ['Trade diversion', 'Rules of origin complexity'],
                'economic_impact': self._assess_fta_impact(bloc_data)
            },
            'customs_union': {
                'definition': 'Free trade area plus common external tariff',
                'examples': ['EU Customs Union', 'Mercosur'],
                'advantages': ['Eliminates trade deflection', 'Stronger negotiating power'],
                'disadvantages': ['Loss of tariff autonomy', 'Complex revenue sharing'],
                'economic_impact': self._assess_customs_union_impact(bloc_data)
            },
            'common_market': {
                'definition': 'Customs union plus free movement of factors',
                'examples': ['EU Single Market', 'ECOWAS'],
                'advantages': ['Factor mobility benefits', 'Efficiency gains', 'Scale economies'],
                'disadvantages': ['Adjustment pressures', 'Migration concerns', 'Policy coordination needs'],
                'economic_impact': self._assess_common_market_impact(bloc_data)
            },
            'economic_union': {
                'definition': 'Common market plus unified economic policies',
                'examples': ['European Union', 'Proposed ASEAN Economic Community'],
                'advantages': ['Maximum integration benefits', 'Policy coherence', 'Stability'],
                'disadvantages': ['Sovereignty loss', 'Complex governance', 'Asymmetric effects'],
                'economic_impact': self._assess_economic_union_impact(bloc_data)
            }
        }

        # The terminal sends its INTEGRATION TYPE choice; answer for that level only.
        type_keys = {
            'fta': 'free_trade_area', 'free_trade_area': 'free_trade_area',
            'customs_union': 'customs_union', 'common_market': 'common_market',
            'economic_union': 'economic_union',
        }
        chosen = type_keys.get(str(bloc_data.get('integration_type', '')).lower())
        levels = {chosen: integration_types[chosen]} if chosen else integration_types

        creation = self.to_decimal(bloc_data.get('trade_creation', 0))
        diversion = self.to_decimal(bloc_data.get('trade_diversion', 0))
        return {
            'integration_type': chosen or 'all',
            'integration_levels': levels,
            'motivations_for_integration': self._analyze_integration_motivations(),
            'success_factors': self._identify_integration_success_factors(),
            'trade_creation_vs_diversion': self._analyze_trade_creation_diversion(bloc_data),
            'headline': {
                'trade_creation_busd': creation,
                'trade_diversion_busd': diversion,
                'net_trade_effect_busd': creation - diversion,
            }
        }

    def assess_trade_barrier_removal(self, liberalization_data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess impact of removing trade barriers"""
        return {
            'capital_investment_effects': {
                'foreign_direct_investment': {
                    'expected_change': 'Significant increase',
                    'mechanisms': ['Market access', 'Lower costs', 'Efficiency seeking'],
                    'sectoral_impact': 'Manufacturing and services benefit most',
                    'quantitative_estimate': self._estimate_fdi_increase(liberalization_data)
                },
                'domestic_investment': {
                    'expected_change': 'Mixed effects',
                    'mechanisms': ['Competitive pressure', 'Technology access', 'Scale opportunities'],
                    'adjustment_period': '3-7 years for full effects',
                    'productivity_gains': self._estimate_productivity_gains(liberalization_data)
                }
            },
            'employment_wage_effects': {
                'aggregate_employment': {
                    'short_term': 'May decline due to adjustment',
                    'long_term': 'Likely increase from higher productivity',
                    'skill_composition': 'Shift toward higher-skilled jobs',
                    'quantitative_estimate': self._estimate_employment_effects(liberalization_data)
                },
                'wage_effects': {
                    'average_wages': 'Generally increase over time',
                    'wage_distribution': 'May increase inequality initially',
                    'sectoral_variation': 'Export sectors gain, import-competing sectors lose',
                    'skill_premium_changes': self._analyze_skill_premium_effects(liberalization_data)
                }
            },
            'growth_effects': {
                'gdp_impact': {
                    'magnitude': self._estimate_gdp_impact(liberalization_data),
                    'channels': ['Productivity', 'Investment', 'Competition', 'Innovation'],
                    'time_horizon': 'Full effects realized over 10-15 years',
                    'persistence': 'Permanent level effects, temporary growth effects'
                },
                'sectoral_growth': self._analyze_sectoral_growth_effects(liberalization_data),
                'regional_effects': self._assess_regional_impact_variation(liberalization_data)
            },
            'policy_recommendations': self._recommend_liberalization_policies(liberalization_data),
            'headline': self._barrier_removal_headline(liberalization_data)
        }

    # ── Input parsing ────────────────────────────────────────────────────────
    @staticmethod
    def _num(data: Dict[str, Any], key: str, default: Optional[float],
             lo: Optional[float] = None, hi: Optional[float] = None,
             lo_open: bool = False, hi_open: bool = False) -> Optional[float]:
        """data[key] as a finite float (None/absent -> default), range-checked.
        Raises ValidationError with a plain message on bad input."""
        raw = data.get(key)
        if raw is None or (isinstance(raw, str) and not raw.strip()):
            return default
        if isinstance(raw, bool):
            raise ValidationError(f"{key} must be a number, got {raw!r}")
        try:
            value = float(raw)
        except (TypeError, ValueError):
            raise ValidationError(f"{key} must be a number, got {raw!r}")
        if not math.isfinite(value):
            raise ValidationError(f"{key} must be a finite number")
        if lo is not None and (value <= lo if lo_open else value < lo):
            raise ValidationError(f"{key} must be {'>' if lo_open else '>='} {lo:g}, got {value:g}")
        if hi is not None and (value >= hi if hi_open else value > hi):
            raise ValidationError(f"{key} must be {'<' if hi_open else '<='} {hi:g}, got {value:g}")
        return value

    # ── benefits_costs model ─────────────────────────────────────────────────
    # efficiency_gain_pct_gdp = static gains from trade relative to autarky, in % of real
    #   income (≈ % of GDP), by the Arkolakis-Costinot-Rodriguez-Clare (AER 2012) formula
    #   1 - lambda^(1/eps): lambda = domestic expenditure share = 1 - imports/GDP, eps = trade
    #   elasticity (default 5). imports/GDP = 'import_share_gdp' when given, otherwise half of
    #   'trade_volume_gdp' ((exports + imports)/GDP with balanced trade assumed).
    # consumer_surplus_gain_pct = first-order consumer gain of a p % price fall on a share s
    #   of consumption = p*s/100, in % of total consumption. Exact with no demand response,
    #   a lower bound otherwise (the extra triangle needs a demand elasticity).
    DEFAULT_TRADE_ELASTICITY = 5.0

    def _benefits_inputs(self, data: Dict[str, Any]) -> Dict[str, Any]:
        volume = self._num(data, 'trade_volume_gdp', 0.0, lo=0, hi=200)
        import_share = self._num(data, 'import_share_gdp', None, lo=0, hi=100, hi_open=True)
        source = 'input import_share_gdp'
        if import_share is None:
            import_share = volume / 2.0
            source = 'half of trade_volume_gdp (balanced trade assumed)'
            if import_share >= 100:
                raise ValidationError("trade_volume_gdp must be below 200 % of GDP")
        return {
            'trade_volume_gdp_pct': volume,
            'import_share_gdp_pct': import_share,
            'import_share_source': source,
            'trade_elasticity': self._num(data, 'trade_elasticity', self.DEFAULT_TRADE_ELASTICITY,
                                          lo=0, lo_open=True),
            'price_reduction_pct': self._num(data, 'price_reduction_percent', 5.0, lo=0, hi=100),
            'traded_goods_consumption_pct': self._num(data, 'traded_goods_consumption', 30.0, lo=0, hi=100),
        }

    def _calculate_trade_gains(self, data: Dict[str, Any]) -> float:
        """Gains from trade vs autarky, % of real income (ACR formula, see model note above)."""
        inp = self._benefits_inputs(data)
        lam = 1.0 - inp['import_share_gdp_pct'] / 100.0
        return round((1.0 - lam ** (1.0 / inp['trade_elasticity'])) * 100.0, 4)

    def _estimate_consumer_surplus_gain(self, data: Dict[str, Any]) -> float:
        """First-order consumer gain, % of total consumption: price fall % x traded share % / 100."""
        inp = self._benefits_inputs(data)
        return round(inp['price_reduction_pct'] * inp['traded_goods_consumption_pct'] / 100.0, 4)

    def _benefits_model(self, data: Dict[str, Any]) -> Dict[str, Any]:
        inp = self._benefits_inputs(data)
        return {
            'efficiency_gain': ('Gains from trade vs autarky, % of real income (~% of GDP): '
                                '1 - (1 - imports/GDP)^(1/trade elasticity) '
                                '(Arkolakis-Costinot-Rodriguez-Clare 2012)'),
            'consumer_surplus_gain': ('First-order gain, % of total consumption: '
                                      'price reduction % x traded-goods share % / 100; a lower bound '
                                      '(no demand response)'),
            'imports_pct_gdp_used': round(inp['import_share_gdp_pct'], 4),
            'imports_pct_gdp_source': inp['import_share_source'],
            'trade_elasticity': inp['trade_elasticity'],
        }

    # ── restrictions model ───────────────────────────────────────────────────
    # Partial equilibrium, small open economy (world price p_w fixed), one tariffed good or
    # bundle of tariffed imports, constant-elasticity curves:
    #   import demand    M(p) = M0 (p/p_w)^-em   em = 'import_demand_elasticity'   (default 1.5)
    #   domestic supply  S(p) = S0 (p/p_w)^es    es = 'domestic_supply_elasticity' (default 0.8)
    #   export supply    X(p) = X0 (p/p_w)^ex    ex = 'export_supply_elasticity'   (default 1.2)
    # An ad-valorem tariff t raises the domestic price by the full t; imports change by
    # (1+t)^-em - 1, domestic output by (1+t)^es - 1. Deadweight loss = the area under M between
    # p_w and p_w(1+t) minus tariff revenue, as a share of the free-trade import bill:
    #   [(1+t)^(1-em) - 1]/(1-em) - t(1+t)^-em      (-> em t^2 / 2 for small t: Harberger).
    # In % of GDP it is that times imports-of-the-tariffed-goods / GDP ('import_share_gdp'), which
    # the inputs must supply: without it the % of GDP figure is null, not guessed.
    # An export subsidy s raises the domestic price of the exported good by the full s; exports
    # change by (1+s)^ex - 1; fiscal cost = s per unit of subsidised export value (world prices);
    # national deadweight loss, share of pre-subsidy export value:
    #   s(1+s)^ex - [(1+s)^(1+ex) - 1]/(1+ex)       (-> ex s^2 / 2 for small s).
    DEFAULT_IMPORT_DEMAND_ELASTICITY = 1.5
    DEFAULT_DOMESTIC_SUPPLY_ELASTICITY = 0.8
    DEFAULT_EXPORT_SUPPLY_ELASTICITY = 1.2

    def _restriction_inputs(self, data: Dict[str, Any]) -> Dict[str, Any]:
        return {
            't': self._num(data, 'tariff_rate', 0.0, lo=0, hi=1000) / 100.0,
            's': self._num(data, 'subsidy_rate', 0.0, lo=0, hi=1000) / 100.0,
            'em': self._num(data, 'import_demand_elasticity', self.DEFAULT_IMPORT_DEMAND_ELASTICITY,
                            lo=0, lo_open=True, hi=20),
            'es': self._num(data, 'domestic_supply_elasticity', self.DEFAULT_DOMESTIC_SUPPLY_ELASTICITY,
                            lo=0, hi=20),
            'ex': self._num(data, 'export_supply_elasticity', self.DEFAULT_EXPORT_SUPPLY_ELASTICITY,
                            lo=0, hi=20),
            # Imports of the tariffed goods as % of GDP: needed only for DWL in % of GDP.
            'import_share': self._num(data, 'import_share_gdp', None, lo=0, hi=100),
        }

    @staticmethod
    def _tariff_dwl_share(t: float, em: float) -> float:
        """Exact tariff deadweight loss / free-trade import bill for CE import demand."""
        if t == 0:
            return 0.0
        g = 1.0 + t
        if abs(em - 1.0) < 1e-12:
            area = math.log(g)
        else:
            area = (g ** (1.0 - em) - 1.0) / (1.0 - em)
        return area - t * g ** (-em)

    @staticmethod
    def _subsidy_dwl_share(s: float, ex: float) -> float:
        """Exact export-subsidy national welfare loss / pre-subsidy export value (CE export supply)."""
        if s == 0:
            return 0.0
        g = 1.0 + s
        return s * g ** ex - (g ** (1.0 + ex) - 1.0) / (1.0 + ex)

    def _restriction_numbers(self, data: Dict[str, Any]) -> Dict[str, Any]:
        i = self._restriction_inputs(data)
        t, s, em, es, ex = i['t'], i['s'], i['em'], i['es'], i['ex']
        dwl_imports = self._tariff_dwl_share(t, em) * 100.0          # % of free-trade import bill
        dwl_gdp = (dwl_imports * i['import_share'] / 100.0) if i['import_share'] is not None else None
        r = lambda v: None if v is None else round(v, 4)
        return {
            'tariff_rate_pct': r(t * 100.0),
            'tariff_price_rise_pct': r(t * 100.0),
            'tariff_import_fall_pct': r((1.0 - (1.0 + t) ** (-em)) * 100.0),
            'tariff_domestic_output_pct': r(((1.0 + t) ** es - 1.0) * 100.0),
            'tariff_revenue_pct_imports': r(t * (1.0 + t) ** (-em) * 100.0),
            'tariff_deadweight_loss_pct_imports': r(dwl_imports),
            'tariff_deadweight_loss_pct_gdp': r(dwl_gdp),
            'import_share_gdp_pct': r(i['import_share']),
            'subsidy_rate_pct': r(s * 100.0),
            'subsidy_export_rise_pct': r(((1.0 + s) ** ex - 1.0) * 100.0),
            'subsidy_domestic_price_rise_pct': r(s * 100.0),
            'subsidy_fiscal_cost_pct_exports': r(s * 100.0),
            'subsidy_deadweight_loss_pct_exports': r(self._subsidy_dwl_share(s, ex) * 100.0),
            '_elasticities': (em, es, ex),
        }

    def _analyze_tariff_effects(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze economic effects of tariffs (same numbers as the headline)"""
        n = self._restriction_numbers(data)
        em, es, _ = n['_elasticities']
        if n['tariff_deadweight_loss_pct_gdp'] is not None:
            dwl = (f"Deadweight loss {n['tariff_deadweight_loss_pct_gdp']:.3f}% of GDP "
                   f"({n['tariff_deadweight_loss_pct_imports']:.2f}% of the pre-tariff import bill)")
        else:
            dwl = (f"Deadweight loss {n['tariff_deadweight_loss_pct_imports']:.2f}% of the pre-tariff "
                   f"import bill (needs imports of the tariffed goods as % of GDP to express in % of GDP)")
        return {
            'price_increase': (f"Domestic price rises by {n['tariff_price_rise_pct']:.2f}% "
                               f"(small open economy: full pass-through)"),
            'import_reduction': (f"Imports fall by {n['tariff_import_fall_pct']:.2f}% "
                                 f"(import demand elasticity {em:g}, constant-elasticity curve)"),
            'domestic_production': (f"Domestic production increases by {n['tariff_domestic_output_pct']:.2f}% "
                                    f"(supply elasticity {es:g})"),
            'tariff_revenue': f"Tariff revenue {n['tariff_revenue_pct_imports']:.2f}% of the pre-tariff import bill",
            'welfare_loss': dwl,
        }

    def _analyze_quota_effects(self) -> Dict[str, Any]:
        """Qualitative effects of import quotas (a volume alone, without demand and
        world-price data, cannot be turned into a tariff equivalent or quota rent)."""
        return {
            'price_effect': 'Domestic price rises to clear market at quota level',
            'quantity_certainty': 'Import volume fixed regardless of demand changes',
            'rent_distribution': 'Quota rents accrue to license holders',
            'supply_response': 'Domestic producers expand to fill demand gap'
        }

    def _analyze_subsidy_effects(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze economic effects of export subsidies (same numbers as the headline)"""
        n = self._restriction_numbers(data)
        _, _, ex = n['_elasticities']
        return {
            'export_increase': (f"Exports rise by {n['subsidy_export_rise_pct']:.2f}% "
                                f"(export supply elasticity {ex:g})"),
            'domestic_price_rise': (f"Domestic price of the exported good rises by "
                                    f"{n['subsidy_domestic_price_rise_pct']:.2f}% (small open economy)"),
            'fiscal_cost': f"Government cost {n['subsidy_fiscal_cost_pct_exports']:.2f}% of subsidised export value",
            'welfare_loss': (f"National deadweight loss {n['subsidy_deadweight_loss_pct_exports']:.3f}% "
                             f"of the pre-subsidy export value"),
            'foreign_welfare': 'Foreign consumers benefit from lower prices'
        }

    def _restriction_headline(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """The tariff and export-subsidy effects as numbers, all in percent; the model and the
        unit of each field are described above _restriction_inputs and in the 'model' block."""
        n = self._restriction_numbers(data)
        n.pop('_elasticities')
        return n

    def _restriction_model(self, data: Dict[str, Any]) -> Dict[str, Any]:
        i = self._restriction_inputs(data)
        model = {
            'description': ('Partial equilibrium, small open economy (world price fixed, full '
                            'pass-through), constant-elasticity import demand, domestic supply and '
                            'export supply'),
            'import_demand_elasticity': i['em'],
            'domestic_supply_elasticity': i['es'],
            'export_supply_elasticity': i['ex'],
            'units': ('price/import/output/export changes: % change; deadweight loss: % of the '
                      'pre-tariff import bill and % of GDP; fiscal cost: % of subsidised export '
                      'value; subsidy deadweight loss: % of pre-subsidy export value'),
        }
        if i['import_share'] is None:
            model['missing_input'] = ('import_share_gdp (imports of the tariffed goods as % of GDP) '
                                      'is needed for the deadweight loss in % of GDP; without it '
                                      'tariff_deadweight_loss_pct_gdp is null')
        return model

    def _barrier_removal_headline(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Scale the textbook bands by the size of the tariff cut and the economy:
        a full removal is taken as a 1-3 % permanent GDP level gain and a 2-5 %
        productivity gain; FDI bands follow the liberalization scope."""
        cut = self.to_decimal(data.get('tariff_reduction', 100)) / self.to_decimal(100)
        gdp = self.to_decimal(data.get('gdp_size', 0))
        fdi_bands = {'limited': (20, 40), 'moderate': (50, 100), 'comprehensive': (100, 200)}
        fdi_low, fdi_high = fdi_bands.get(data.get('liberalization_scope', 'moderate'), (50, 100))
        gdp_low_pct = self.to_decimal(1) * cut
        gdp_high_pct = self.to_decimal(3) * cut
        return {
            'tariff_reduction_pct': cut * self.to_decimal(100),
            'gdp_gain_low_pct': gdp_low_pct,
            'gdp_gain_high_pct': gdp_high_pct,
            'gdp_gain_low_busd': gdp * gdp_low_pct / self.to_decimal(100),
            'gdp_gain_high_busd': gdp * gdp_high_pct / self.to_decimal(100),
            'productivity_gain_low_pct': self.to_decimal(2) * cut,
            'productivity_gain_high_pct': self.to_decimal(5) * cut,
            'fdi_increase_low_pct': fdi_low,
            'fdi_increase_high_pct': fdi_high,
        }

    def _compare_trade_restrictions(self) -> Dict[str, str]:
        """Compare different trade restriction types"""
        return {
            'transparency': 'Tariffs > Quotas > Non-tariff barriers',
            'revenue_generation': 'Tariffs > Export subsidies (cost) > Quotas (no revenue)',
            'welfare_impact': 'All create deadweight losses, magnitude varies',
            'administrative_burden': 'Non-tariff barriers > Quotas > Tariffs',
            'flexibility': 'Tariffs > Export subsidies > Quotas'
        }

    def _recommend_trade_policy(self, data: Dict[str, Any]) -> str:
        """Recommend optimal trade policy"""
        development_level = data.get('development_level', 'middle')
        industry_maturity = data.get('industry_maturity', 'mature')

        if development_level == 'developing' and industry_maturity == 'infant':
            return 'Temporary protection may be justified for infant industries'
        elif development_level == 'developed':
            return 'Free trade generally optimal for developed economies'
        else:
            return 'Gradual liberalization with adjustment assistance'

    def _assess_fta_impact(self, data: Dict[str, Any]) -> str:
        """Assess free trade agreement impact"""
        trade_creation = self.to_decimal(data.get('trade_creation', 0))
        trade_diversion = self.to_decimal(data.get('trade_diversion', 0))

        if trade_creation > trade_diversion:
            return 'Net welfare gain from trade creation effects'
        else:
            return 'Potential welfare loss from trade diversion'

    def _assess_customs_union_impact(self, data: Dict[str, Any]) -> str:
        """Assess customs union impact"""
        return 'Generally more beneficial than FTA due to common external tariff'

    def _assess_common_market_impact(self, data: Dict[str, Any]) -> str:
        """Assess common market impact"""
        return 'Significant benefits from factor mobility, but requires strong institutions'

    def _assess_economic_union_impact(self, data: Dict[str, Any]) -> str:
        """Assess economic union impact"""
        return 'Maximum benefits but requires political integration and sovereignty transfer'

    def _analyze_integration_motivations(self) -> List[str]:
        """Analyze motivations for regional integration"""
        return [
            'Economic: Market access, scale economies, efficiency gains',
            'Political: Peace, cooperation, international influence',
            'Strategic: Counterbalance to other blocs, bargaining power',
            'Development: Technology transfer, investment attraction'
        ]

    def _identify_integration_success_factors(self) -> List[str]:
        """Identify factors for successful regional integration"""
        return [
            'Geographic proximity and cultural similarity',
            'Similar development levels and economic structures',
            'Political commitment and institutional capacity',
            'Complementary rather than competing economies',
            'Mechanism for handling adjustment costs'
        ]

    def _analyze_trade_creation_diversion(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze trade creation vs trade diversion effects"""
        return {
            'trade_creation': {
                'definition': 'New trade due to elimination of barriers among members',
                'welfare_effect': 'Positive - increases efficiency',
                'mechanism': 'Efficient producers replace inefficient domestic production'
            },
            'trade_diversion': {
                'definition': 'Trade shifts from efficient non-members to less efficient members',
                'welfare_effect': 'Negative - reduces efficiency',
                'mechanism': 'Preferential access distorts comparative advantage'
            },
            'net_effect': 'Depends on relative magnitude of creation vs diversion'
        }

    def _estimate_fdi_increase(self, data: Dict[str, Any]) -> str:
        """Estimate FDI increase from liberalization"""
        liberalization_scope = data.get('liberalization_scope', 'moderate')

        increases = {
            'limited': '20-40% increase over 5 years',
            'moderate': '50-100% increase over 5 years',
            'comprehensive': '100-200% increase over 5 years'
        }

        return increases.get(liberalization_scope, '50-100% increase over 5 years')

    def _estimate_productivity_gains(self, data: Dict[str, Any]) -> str:
        """Estimate productivity gains from liberalization"""
        h = self._barrier_removal_headline(data)
        return (f"{h['productivity_gain_low_pct']:.1f}-{h['productivity_gain_high_pct']:.1f}% "
                f"productivity gain over 5-10 years")

    def _estimate_employment_effects(self, data: Dict[str, Any]) -> str:
        """Estimate employment effects of liberalization"""
        return 'Short-term adjustment costs, long-term employment gains'

    def _analyze_skill_premium_effects(self, data: Dict[str, Any]) -> str:
        """Analyze effects on skill premiums"""
        return 'Skill premium may increase initially, then stabilize with education/training'

    def _estimate_gdp_impact(self, data: Dict[str, Any]) -> str:
        """Estimate GDP impact of trade liberalization"""
        h = self._barrier_removal_headline(data)
        text = (f"{h['gdp_gain_low_pct']:.2f}-{h['gdp_gain_high_pct']:.2f}% permanent GDP level increase, "
                f"spread over 10-15 years")
        if h['gdp_gain_high_busd'] > 0:
            text += f" (about {h['gdp_gain_low_busd']:.1f}-{h['gdp_gain_high_busd']:.1f} B USD)"
        return text

    def _analyze_sectoral_growth_effects(self, data: Dict[str, Any]) -> Dict[str, str]:
        """Analyze sectoral growth effects"""
        return {
            'export_sectors': 'Strong growth, increased investment',
            'import_competing_sectors': 'Decline, but may become more efficient',
            'service_sectors': 'Generally benefit from lower input costs',
            'technology_sectors': 'Benefit from knowledge spillovers'
        }

    def _assess_regional_impact_variation(self, data: Dict[str, Any]) -> str:
        """Assess regional variation in impacts"""
        return 'Urban areas and regions with comparative advantage benefit most'

    def _recommend_liberalization_policies(self, data: Dict[str, Any]) -> List[str]:
        """Recommend supporting policies for liberalization"""
        return [
            'Trade adjustment assistance for displaced workers',
            'Education and training programs for skill upgrading',
            'Infrastructure investment to support new trade patterns',
            'Competition policy to ensure domestic market efficiency',
            'Social safety net to manage transition costs'
        ]

    def _assess_net_welfare_impact(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess net welfare impact of trade"""
        return {
            'aggregate_welfare': 'Generally positive but distribution matters',
            'time_dimension': 'Short-term costs, long-term benefits',
            'policy_implications': 'Need complementary policies for inclusive growth',
            'measurement_challenges': 'Difficult to quantify all benefits and costs'
        }

    def calculate(self, analysis_type: str = 'benefits_costs', **kwargs) -> Dict[str, Any]:
        """Main trade analysis dispatcher"""
        analyses = {
            'benefits_costs': lambda: self.analyze_trade_benefits_costs(kwargs.get('trade_data', {})),
            'restrictions': lambda: self.analyze_trade_restrictions(kwargs.get('restriction_data', {})),
            'trading_blocs': lambda: self.analyze_trading_blocs(kwargs.get('bloc_data', {})),
            'barrier_removal': lambda: self.assess_trade_barrier_removal(kwargs.get('liberalization_data', {}))
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        return result


class GeopoliticalRiskAnalyzer(EconomicsBase):
    """Geopolitical risk assessment and investment implications"""

    def analyze_geopolitics_framework(self) -> Dict[str, Any]:
        """Analyze geopolitics from cooperation vs competition perspective"""
        return {
            'cooperation_perspective': {
                'drivers': ['Economic interdependence', 'Shared challenges', 'Institutional frameworks'],
                'mechanisms': ['Trade agreements', 'International organizations', 'Diplomatic engagement'],
                'benefits': ['Peace dividend', 'Economic gains', 'Global public goods'],
                'examples': ['EU integration', 'WTO system', 'Climate cooperation']
            },
            'competition_perspective': {
                'drivers': ['National interests', 'Power struggles', 'Resource competition'],
                'mechanisms': ['Military buildup', 'Economic sanctions', 'Technology competition'],
                'risks': ['Conflict escalation', 'Economic fragmentation', 'Arms races'],
                'examples': ['US-China rivalry', 'Russia-West tensions', 'Cyber warfare']
            },
            'cooperation_competition_spectrum': {
                'coopetition': 'Simultaneous cooperation and competition',
                'issue_specificity': 'Cooperation on some issues, competition on others',
                'temporal_variation': 'Shifting between cooperation and competition over time',
                'stakeholder_differences': 'Different actors may prefer different approaches'
            },
            'current_global_trends': self._assess_current_geopolitical_trends()
        }

    def analyze_geopolitics_globalization(self) -> Dict[str, Any]:
        """Analyze relationship between geopolitics and globalization"""
        return {
            'globalization_drivers': {
                'economic': 'Trade, investment, financial integration',
                'technological': 'Communication, transportation, digital connectivity',
                'political': 'International governance, regulatory convergence',
                'cultural': 'Information flow, cultural exchange, migration'
            },
            'geopolitical_constraints': {
                'sovereignty_concerns': 'National autonomy vs global integration',
                'security_considerations': 'Economic interdependence vs strategic autonomy',
                'distributional_effects': 'Winners and losers from globalization',
                'cultural_resistance': 'Preserving national identity and values'
            },
            'interaction_dynamics': {
                'reinforcing_effects': 'Economic integration can reduce conflict incentives',
                'tension_creation': 'Globalization can threaten traditional power structures',
                'policy_responses': 'Governments balance integration with national interests',
                'cyclical_patterns': 'Periods of integration followed by fragmentation'
            },
            'current_deglobalization_trends': self._analyze_deglobalization_trends()
        }

    def analyze_international_organizations(self) -> Dict[str, Any]:
        """Analyze functions and objectives of key international organizations"""
        return {
            'world_bank': {
                'primary_objective': 'Reduce poverty and promote shared prosperity',
                'functions': [
                    'Development financing and technical assistance',
                    'Policy advice and capacity building',
                    'Knowledge sharing and research',
                    'Crisis response and post-conflict reconstruction'
                ],
                'lending_instruments': ['IBRD loans', 'IDA grants', 'Private sector lending'],
                'governance': '189 member countries, voting power based on capital contributions',
                'effectiveness_assessment': 'Mixed results, criticism for conditionality and governance'
            },
            'international_monetary_fund': {
                'primary_objective': 'Ensure stability of international monetary system',
                'functions': [
                    'Surveillance of global economy and exchange rates',
                    'Financial assistance to countries in balance of payments difficulties',
                    'Technical assistance and capacity development',
                    'Standard setting and policy coordination'
                ],
                'lending_facilities': ['Stand-by arrangements', 'Extended fund facility', 'Emergency assistance'],
                'governance': '190 member countries, quota-based voting system',
                'effectiveness_assessment': 'Critical role in crisis response, debates over conditionality'
            },
            'world_trade_organization': {
                'primary_objective': 'Promote free and fair trade globally',
                'functions': [
                    'Trade rule making and negotiation',
                    'Dispute settlement between members',
                    'Trade policy monitoring and transparency',
                    'Technical assistance and capacity building'
                ],
                'key_principles': ['Non-discrimination', 'Market access', 'Fair competition', 'Development'],
                'governance': '164 members, consensus-based decision making',
                'effectiveness_assessment': 'Success in trade liberalization, challenges with dispute resolution'
            },
            'organizational_interactions': self._analyze_organizational_coordination(),
            'reform_needs': self._assess_reform_requirements()
        }

    def assess_geopolitical_risk(self, risk_data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess geopolitical risk levels and components"""
        return {
            'risk_categories': {
                'interstate_conflict': {
                    'probability': self._assess_conflict_probability(risk_data),
                    'impact': 'High - disrupts trade, increases defense spending',
                    'indicators': ['Military buildups', 'Territorial disputes', 'Alliance shifts'],
                    'current_hotspots': ['Taiwan Strait', 'Ukraine', 'Middle East', 'South China Sea']
                },
                'domestic_instability': {
                    'probability': self._assess_instability_probability(risk_data),
                    'impact': 'Medium to High - affects governance and economic policy',
                    'indicators': ['Political polarization', 'Social unrest', 'Economic inequality'],
                    'monitoring_metrics': ['Polity IV scores', 'Fragile States Index', 'Social cohesion indices']
                },
                'economic_warfare': {
                    'probability': 'Medium - already occurring',
                    'impact': 'High - trade disruption, technology decoupling',
                    'manifestations': ['Trade wars', 'Technology sanctions', 'Financial restrictions'],
                    'current_examples': ['US-China tech competition', 'Russia sanctions', 'Supply chain nationalism']
                },
                'cyber_threats': {
                    'probability': 'High - ongoing',
                    'impact': 'Medium to High - infrastructure and financial system risks',
                    'evolution': 'Rapidly increasing sophistication and frequency',
                    'mitigation_challenges': 'Attribution difficulties, cross-border nature'
                }
            },
            'risk_assessment_methodology': self._describe_risk_methodology(),
            'early_warning_indicators': self._identify_early_warning_signs(),
            'risk_mitigation_strategies': self._recommend_risk_mitigation()
        }

    def analyze_geopolitical_tools(self) -> Dict[str, Any]:
        """Analyze tools of geopolitics and their economic impact"""
        return {
            'economic_tools': {
                'trade_policy': {
                    'instruments': ['Tariffs', 'Quotas', 'Trade agreements', 'Export controls'],
                    'effectiveness': 'High for economic coercion, limited for security goals',
                    'economic_impact': 'Efficiency losses, distributional effects',
                    'examples': ['China trade war', 'Iran sanctions', 'Brexit negotiations']
                },
                'financial_sanctions': {
                    'instruments': ['Asset freezes', 'Banking restrictions', 'Capital market access'],
                    'effectiveness': 'High when multilateral, moderate when unilateral',
                    'economic_impact': 'Disrupts financial flows, increases transaction costs',
                    'examples': ['Russia SWIFT exclusion', 'Iran banking sanctions', 'North Korea restrictions']
                },
                'investment_controls': {
                    'instruments': ['FDI screening', 'Technology transfer restrictions',
                                    'Sovereign wealth fund limits'],
                    'effectiveness': 'Moderate for strategic sectors',
                    'economic_impact': 'Reduces capital flows, technology diffusion',
                    'examples': ['CFIUS reviews', 'EU FDI screening', 'Technology export controls']
                }
            },
            'diplomatic_tools': {
                'multilateral_engagement': 'International organizations and forums',
                'bilateral_relations': 'Direct government-to-government engagement',
                'public_diplomacy': 'Cultural and informational influence',
                'summit_diplomacy': 'High-level leader engagement'
            },
            'military_tools': {
                'defense_spending': 'Military buildup and alliance strengthening',
                'military_presence': 'Forward deployment and bases',
                'arms_sales': 'Defense cooperation and influence building',
                'security_assistance': 'Training and capacity building'
            },
            'tool_effectiveness_comparison': self._compare_geopolitical_tools()
        }

    def assess_investment_implications(self, geopolitical_data: Dict[str, Any]) -> Dict[str, Any]:
        """Assess investment implications of geopolitical risk"""
        return {
            'asset_class_impacts': {
                'equities': {
                    'safe_haven_flows': 'Flight to quality during crises',
                    'sector_differentiation': 'Defense up, trade-dependent sectors down',
                    'regional_variation': 'Emerging markets more vulnerable',
                    'volatility_impact': 'Increased uncertainty and volatility'
                },
                'fixed_income': {
                    'government_bonds': 'Safe haven demand for developed market bonds',
                    'corporate_bonds': 'Credit spreads widen, especially for affected regions',
                    'emerging_market_debt': 'Capital flight and spread widening',
                    'inflation_expectations': 'Supply disruptions may increase inflation'
                },
                'currencies': {
                    'reserve_currencies': 'Dollar, euro, yen benefit from safe haven flows',
                    'commodity_currencies': 'Impact depends on commodity exposure',
                    'emerging_market_currencies': 'Generally weaken during geopolitical stress',
                    'crypto_currencies': 'Mixed reactions, some safe haven demand'
                },
                'commodities': {
                    'energy': 'Supply disruption premium for oil and gas',
                    'precious_metals': 'Traditional safe haven demand for gold',
                    'agriculture': 'Supply chain disruptions affect food prices',
                    'industrial_metals': 'Demand reduction from economic slowdown'
                }
            },
            'sector_analysis': self._analyze_sector_impacts(geopolitical_data),
            'geographic_considerations': self._assess_geographic_impacts(geopolitical_data),
            'investment_strategies': self._recommend_investment_strategies(geopolitical_data),
            'risk_monitoring_framework': self._develop_monitoring_framework()
        }

    def _assess_current_geopolitical_trends(self) -> List[str]:
        """Assess current global geopolitical trends"""
        return [
            'Rise of China and shifting global power balance',
            'Renewed great power competition between US, China, and Russia',
            'Fragmentation of global governance and institutions',
            'Technology competition and digital sovereignty concerns',
            'Climate change as a security issue and cooperation challenge',
            'Democratic backsliding and authoritarian resilience'
        ]

    def _analyze_deglobalization_trends(self) -> Dict[str, str]:
        """Analyze current deglobalization trends"""
        return {
            'trade_slowdown': 'Growth in global trade relative to GDP has slowed',
            'supply_chain_reshoring': 'Companies reducing dependence on distant suppliers',
            'technology_decoupling': 'Separate technology ecosystems emerging',
            'financial_fragmentation': 'Reduced cross-border capital flows',
            'immigration_restrictions': 'Tighter controls on human mobility',
            'policy_implications': 'Governments balancing efficiency with resilience'
        }

    def _analyze_organizational_coordination(self) -> Dict[str, str]:
        """Analyze coordination between international organizations"""
        return {
            'world_bank_imf': 'Close coordination on development and financial stability',
            'wto_relationship': 'Limited formal links but complementary mandates',
            'regional_organizations': 'Growing importance of regional institutions',
            'coordination_challenges': 'Overlapping mandates and competing priorities'
        }

    def _assess_reform_requirements(self) -> List[str]:
        """Assess reform needs for international organizations"""
        return [
            'IMF: Quota reform to reflect changing global economy',
            'World Bank: Climate focus and private sector engagement',
            'WTO: Dispute settlement reform and digital trade rules',
            'UN Security Council: Representation reform for emerging powers',
            'All: Enhanced coordination and reduced overlap'
        ]

    def _assess_conflict_probability(self, data: Dict[str, Any]) -> str:
        """Assess probability of interstate conflict"""
        tension_level = data.get('tension_index', 0.5)

        if tension_level > 0.8:
            return 'High risk of conflict escalation'
        elif tension_level > 0.6:
            return 'Moderate risk, close monitoring needed'
        else:
            return 'Low to moderate risk'

    def _assess_instability_probability(self, data: Dict[str, Any]) -> str:
        """Assess probability of domestic instability"""
        governance_score = data.get('governance_index', 0.5)

        if governance_score < 0.3:
            return 'High instability risk'
        elif governance_score < 0.6:
            return 'Moderate instability risk'
        else:
            return 'Low instability risk'

    def _describe_risk_methodology(self) -> Dict[str, str]:
        """Describe geopolitical risk assessment methodology"""
        return {
            'quantitative_indicators': 'Economic data, governance indices, conflict databases',
            'qualitative_assessment': 'Expert analysis, scenario planning, historical analogies',
            'early_warning_systems': 'Real-time monitoring of key indicators',
            'scenario_analysis': 'Multiple future scenarios with probability weights',
            'stress_testing': 'Impact assessment under extreme scenarios'
        }

    def _identify_early_warning_signs(self) -> List[str]:
        """Identify early warning indicators of geopolitical stress"""
        return [
            'Diplomatic relations: Embassy closures, ambassador recalls',
            'Military indicators: Troop movements, exercise frequency, defense spending',
            'Economic signals: Trade restrictions, investment controls, sanctions threats',
            'Political rhetoric: Leadership statements, media coverage, public opinion',
            'Market indicators: Risk premiums, capital flows, currency movements'
        ]

    def _recommend_risk_mitigation(self) -> List[str]:
        """Recommend geopolitical risk mitigation strategies"""
        return [
            'Diversification: Geographic and supply chain diversification',
            'Scenario planning: Regular stress testing and contingency planning',
            'Political risk insurance: Coverage for expropriation and conflict',
            'Local partnerships: Joint ventures and local content requirements',
            'Flexible operations: Ability to quickly adjust to changing conditions'
        ]

    def _compare_geopolitical_tools(self) -> Dict[str, str]:
        """Compare effectiveness of different geopolitical tools"""
        return {
            'economic_tools': 'High immediate impact but may create long-term costs',
            'diplomatic_tools': 'Low immediate impact but sustainable and relationship-preserving',
            'military_tools': 'High coercive power but risks escalation and high costs',
            'optimal_strategy': 'Combination of tools tailored to specific objectives and constraints'
        }

    def _analyze_sector_impacts(self, data: Dict[str, Any]) -> Dict[str, str]:
        """Analyze sectoral impacts of geopolitical risk"""
        return {
            'defense_aerospace': 'Generally benefits from increased defense spending',
            'energy': 'Mixed impact depending on supply chain exposure',
            'technology': 'Vulnerable to export controls and technology restrictions',
            'financials': 'Exposed to sanctions and capital flow restrictions',
            'materials': 'Supply chain disruptions and commodity price volatility',
            'consumer_discretionary': 'Reduced confidence affects spending patterns'
        }

    def _assess_geographic_impacts(self, data: Dict[str, Any]) -> Dict[str, str]:
        """Assess geographic variation in geopolitical impacts"""
        return {
            'developed_markets': 'Relative safety but not immune to global shocks',
            'emerging_markets': 'Higher vulnerability to capital flow reversals',
            'frontier_markets': 'Extreme sensitivity to risk sentiment changes',
            'regional_variation': 'Proximity to conflict zones increases impact',
            'economic_integration': 'Highly integrated economies more affected'
        }

    def _recommend_investment_strategies(self, data: Dict[str, Any]) -> List[str]:
        """Recommend investment strategies for geopolitical risk environment"""
        return [
            'Tactical allocation: Adjust portfolio weights based on risk assessment',
            'Safe haven assets: Maintain allocation to defensive assets',
            'Sector rotation: Favor sectors that benefit from geopolitical trends',
            'Currency hedging: Protect against adverse currency movements',
            'Volatility management: Use derivatives to manage downside risk',
            'ESG integration: Consider governance and sustainability factors'
        ]

    def _develop_monitoring_framework(self) -> Dict[str, List[str]]:
        """Develop framework for monitoring geopolitical risks"""
        return {
            'daily_monitoring': ['News flow', 'Market reactions', 'Policy statements'],
            'weekly_assessment': ['Economic data', 'Diplomatic developments', 'Military activities'],
            'monthly_review': ['Risk indicator updates', 'Scenario probability updates', 'Portfolio adjustments'],
            'quarterly_analysis': ['Comprehensive risk assessment', 'Strategy review', 'Stress testing'],
            'annual_planning': ['Long-term scenario development', 'Strategic asset allocation',
                                'Risk budget allocation']
        }

    def calculate(self, analysis_type: str = 'risk_assessment', **kwargs) -> Dict[str, Any]:
        """Main geopolitical analysis dispatcher"""
        analyses = {
            'framework': self.analyze_geopolitics_framework,
            'globalization': self.analyze_geopolitics_globalization,
            'organizations': self.analyze_international_organizations,
            'risk_assessment': lambda: self.assess_geopolitical_risk(kwargs.get('risk_data', {})),
            'tools': self.analyze_geopolitical_tools,
            'investment_implications': lambda: self.assess_investment_implications(kwargs.get('geopolitical_data', {}))
        }

        if analysis_type not in analyses:
            raise ValidationError(f"Unknown analysis type: {analysis_type}")

        result = analyses[analysis_type]()
        result['metadata'] = self.get_metadata()
        return result


class TradingBlocAnalyzer(EconomicsBase):
    """Specialized analysis of trading blocs and economic integration"""

    def analyze_bloc_performance(self, bloc_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze performance and effectiveness of trading blocs"""
        return {
            'trade_creation_measurement': {
                'intra_bloc_trade_growth': self._calculate_intra_bloc_growth(bloc_data),
                'trade_intensity_index': self._calculate_trade_intensity(bloc_data),
                'revealed_comparative_advantage': 'Analysis of changing trade patterns',
                'welfare_impact_estimate': self._estimate_welfare_impact(bloc_data)
            },
            'integration_depth_assessment': {
                'tariff_elimination': bloc_data.get('tariff_elimination_percent', 'N/A'),
                'non_tariff_barriers': bloc_data.get('ntb_reduction_score', 'N/A'),
                'services_liberalization': bloc_data.get('services_openness_index', 'N/A'),
                'factor_mobility': bloc_data.get('factor_mobility_score', 'N/A'),
                'policy_coordination': bloc_data.get('policy_coordination_index', 'N/A')
            },
            'economic_convergence': {
                'income_convergence': 'Analysis of per capita income gaps',
                'inflation_convergence': 'Monetary policy coordination effects',
                'business_cycle_synchronization': 'Economic cycle alignment assessment',
                'structural_convergence': 'Industry structure and productivity alignment'
            },
            'challenges_and_obstacles': self._identify_integration_challenges(bloc_data),
            'success_factors': self._assess_integration_success_factors(bloc_data)
        }

    def _calculate_intra_bloc_growth(self, data: Dict[str, Any]) -> str:
        """Calculate intra-bloc trade growth"""
        if 'baseline_intra_trade' not in data or 'current_intra_trade' not in data:
            return 'N/A (needs baseline_intra_trade and current_intra_trade)'
        baseline_trade = self.to_decimal(data['baseline_intra_trade'])
        current_trade = self.to_decimal(data['current_intra_trade'])
        if baseline_trade <= 0:
            return 'N/A (baseline_intra_trade must be positive)'

        growth_rate = ((current_trade - baseline_trade) / baseline_trade) * self.to_decimal(100)
        return f"Intra-bloc trade grew by {growth_rate:.1f}% since formation"

    def _calculate_trade_intensity(self, data: Dict[str, Any]) -> str:
        """Calculate trade intensity index"""
        return "Trade intensity index measures whether bloc members trade more with each other than expected"

    def _estimate_welfare_impact(self, data: Dict[str, Any]) -> str:
        """Estimate welfare impact of trading bloc"""
        trade_creation = data.get('trade_creation_estimate', 'positive')
        trade_diversion = data.get('trade_diversion_estimate', 'moderate')

        if trade_creation == 'positive' and trade_diversion == 'low':
            return 'Net positive welfare impact'
        elif trade_creation == 'positive' and trade_diversion == 'moderate':
            return 'Likely positive welfare impact'
        else:
            return 'Mixed welfare impact, requires detailed analysis'

    def _identify_integration_challenges(self, data: Dict[str, Any]) -> List[str]:
        """Identify challenges to deeper integration"""
        return [
            'Asymmetric development levels among members',
            'Different regulatory frameworks and standards',
            'Political sovereignty concerns',
            'Unequal distribution of integration benefits',
            'External pressure from non-member countries',
            'Coordination costs and administrative burden'
        ]

    def _assess_integration_success_factors(self, data: Dict[str, Any]) -> Dict[str, str]:
        """Assess factors contributing to integration success"""
        return {
            'political_commitment': 'Strong leadership commitment to integration goals',
            'institutional_framework': 'Effective governance and dispute resolution mechanisms',
            'economic_complementarity': 'Complementary rather than competing economic structures',
            'adjustment_mechanisms': 'Policies to help losers from integration',
            'external_support': 'Technical and financial assistance for integration process'
        }

    def calculate(self, **kwargs) -> Dict[str, Any]:
        """Calculate trading bloc analysis"""
        result = self.analyze_bloc_performance(kwargs.get('bloc_data', {}))
        result['metadata'] = self.get_metadata()
        return result


if __name__ == "__main__":
    # CLI: python trade_geopolitics.py <benefits_costs|restrictions|trading_blocs|barrier_removal> '<json>'
    # One JSON object on stdout (fincept_json: Decimal -> float, NaN/Inf -> null, one line).
    import os
    import sys
    import json
    try:
        import fincept_json
    except ImportError:  # run by hand without PYTHONPATH=scripts
        sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        import fincept_json

    analysis_type = sys.argv[1] if len(sys.argv) > 1 else "benefits_costs"
    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    except ValueError as exc:
        fincept_json.emit({"error": f"Invalid JSON parameters: {exc}"})
        sys.exit(1)
    if not isinstance(params, dict):
        fincept_json.emit({"error": "Parameters must be a JSON object"})
        sys.exit(1)

    analyzer = TradeAnalyzer()

    # Map analysis_type to correct method with correct data key
    try:
        if analysis_type == "benefits_costs":
            result = analyzer.analyze_trade_benefits_costs(params)
        elif analysis_type == "restrictions":
            result = analyzer.analyze_trade_restrictions(params)
        elif analysis_type == "trading_blocs":
            result = analyzer.analyze_trading_blocs(params)
        elif analysis_type == "barrier_removal":
            result = analyzer.assess_trade_barrier_removal(params)
        else:
            result = {"error": f"Unknown analysis type: {analysis_type}"}
    except Exception as exc:  # bad numeric input etc. -> JSON error, not a traceback
        result = {"error": str(exc) or type(exc).__name__}

    fincept_json.emit(result)