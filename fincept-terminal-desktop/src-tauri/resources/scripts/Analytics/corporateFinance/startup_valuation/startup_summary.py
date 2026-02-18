"""Comprehensive Startup Valuation Summary"""
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.startup_valuation.berkus_method import BerkusMethod, BerkusFactor
from corporateFinance.startup_valuation.scorecard_method import ScorecardMethod
from corporateFinance.startup_valuation.vc_method import VCMethod
from corporateFinance.startup_valuation.first_chicago_method import FirstChicagoMethod, Scenario
from corporateFinance.startup_valuation.risk_factor_summation import RiskFactorSummation, RiskFactor


def _coerce_berkus_scores(raw: dict) -> dict:
    """Convert JSON string-keyed dict to BerkusFactor enum-keyed dict."""
    key_map = {f.value: f for f in BerkusFactor}
    return {key_map[k]: float(v) for k, v in raw.items() if k in key_map}


def _coerce_first_chicago_scenarios(raw: list) -> list:
    """Convert list of dicts to list of Scenario dataclass objects."""
    return [
        Scenario(
            name=s.get('name', 'Scenario'),
            probability=float(s.get('probability', 0.33)),
            exit_year=int(s.get('exit_year', 5)),
            exit_value=float(s.get('exit_value', 0)),
            description=s.get('description', ''),
        )
        for s in raw
    ]


def _coerce_risk_factor_assessments(raw: dict) -> dict:
    """Convert JSON string-keyed dict to RiskFactor enum-keyed dict."""
    key_map = {f.value: f for f in RiskFactor}
    return {key_map[k]: int(v) for k, v in raw.items() if k in key_map}


class StartupValuationSummary:
    """Aggregate all startup valuation methods into comprehensive summary"""

    def __init__(self, startup_name: str):
        self.startup_name = startup_name

    def comprehensive_valuation(self,
                               berkus_scores: Optional[Dict[BerkusFactor, float]] = None,
                               scorecard_inputs: Optional[Dict[str, Any]] = None,
                               vc_inputs: Optional[Dict[str, Any]] = None,
                               first_chicago_scenarios: Optional[List[Scenario]] = None,
                               risk_factor_assessments: Optional[Dict[RiskFactor, int]] = None) -> Dict[str, Any]:
        """
        Generate comprehensive startup valuation using all applicable methods

        Returns summary with all methods, weighted average, and range
        """

        valuations = {}

        # Berkus Method (pre-revenue)
        if berkus_scores:
            berkus = BerkusMethod()
            # Coerce string keys to BerkusFactor enums if needed
            if berkus_scores and not isinstance(next(iter(berkus_scores), None), BerkusFactor):
                berkus_scores = _coerce_berkus_scores(berkus_scores)
            if berkus_scores:  # Re-check after coercion (may have filtered out invalid keys)
                berkus_result = berkus.calculate_valuation(berkus_scores)
                valuations['berkus'] = {
                    'method': 'Berkus Method',
                    'valuation': berkus_result['total_valuation'],
                    'applicability': 'pre_revenue',
                    'details': berkus_result
                }

        # Scorecard Method
        if scorecard_inputs:
            scorecard = ScorecardMethod(region=scorecard_inputs.get('region', 'US'))
            # Map alternative frontend key names to scorecard factor names
            assessments = scorecard_inputs.get('assessments', {})
            key_map = {
                'market_opportunity': 'size_of_opportunity', 'market_size': 'size_of_opportunity',
                'marketing_channels': 'marketing_sales_channels', 'sales_channels': 'marketing_sales_channels',
                'need_for_investment': 'need_for_additional_investment',
                'other': 'other_factors',
            }
            mapped_assessments = {key_map.get(k, k): v for k, v in assessments.items()}
            scorecard_result = scorecard.calculate_valuation(
                stage=scorecard_inputs['stage'],
                factor_assessments=mapped_assessments
            )
            valuations['scorecard'] = {
                'method': 'Scorecard Method',
                'valuation': scorecard_result['final_valuation'],
                'applicability': 'early_stage',
                'details': scorecard_result
            }

        # VC Method
        if vc_inputs:
            vc = VCMethod()
            vc_result = vc.comprehensive_valuation(**vc_inputs)
            valuations['vc_method'] = {
                'method': 'VC Method',
                'valuation': vc_result['pre_money_valuation'],
                'applicability': 'all_stages',
                'details': vc_result
            }

        # First Chicago Method
        if first_chicago_scenarios:
            fc = FirstChicagoMethod()
            # Coerce dicts to Scenario objects before calling calculate_expected_value
            if first_chicago_scenarios and not isinstance(first_chicago_scenarios[0], Scenario):
                first_chicago_scenarios = _coerce_first_chicago_scenarios(first_chicago_scenarios)
            if first_chicago_scenarios:
                fc_result = fc.calculate_expected_value(first_chicago_scenarios)
                valuations['first_chicago'] = {
                    'method': 'First Chicago Method',
                    'valuation': fc_result['expected_present_value'],
                    'applicability': 'all_stages',
                    'details': fc_result
                }

        # Risk Factor Summation
        if risk_factor_assessments:
            rfs = RiskFactorSummation(base_valuation=2_000_000)
            # Coerce string keys to RiskFactor enums before calling calculate_valuation
            if risk_factor_assessments and not isinstance(next(iter(risk_factor_assessments), None), RiskFactor):
                risk_factor_assessments = _coerce_risk_factor_assessments(risk_factor_assessments)
            if risk_factor_assessments:
                rfs_result = rfs.calculate_valuation(risk_factor_assessments)
                valuations['risk_factor'] = {
                    'method': 'Risk Factor Summation',
                    'valuation': rfs_result['final_valuation'],
                    'applicability': 'early_stage',
                    'details': rfs_result
                }

        if not valuations:
            return {'error': 'No valuation methods provided'}

        # Calculate statistics
        valuation_values = [v['valuation'] for v in valuations.values()]

        min_val = min(valuation_values)
        max_val = max(valuation_values)
        mean_val = sum(valuation_values) / len(valuation_values)
        median_val = sorted(valuation_values)[len(valuation_values) // 2]

        # Weighted average (give more weight to methods with higher applicability)
        weights = {
            'berkus': 0.15,
            'scorecard': 0.20,
            'vc_method': 0.30,
            'first_chicago': 0.25,
            'risk_factor': 0.10
        }

        total_weight = sum(weights.get(k, 0.2) for k in valuations.keys())
        weighted_valuation = sum(
            v['valuation'] * weights.get(k, 0.2)
            for k, v in valuations.items()
        ) / total_weight

        return {
            'startup_name': self.startup_name,
            'valuations_by_method': valuations,
            'valuation_summary': {
                'min': min_val,
                'max': max_val,
                'mean': mean_val,
                'median': median_val,
                'weighted_average': weighted_valuation,
                'range': max_val - min_val,
                'coefficient_of_variation': (
                    (max_val - min_val) / mean_val * 100
                ) if mean_val > 0 else 0
            },
            'methods_used': list(valuations.keys()),
            'num_methods': len(valuations),
            'recommendation': {
                'suggested_valuation': weighted_valuation,
                'valuation_range': f"${min_val:,.0f} - ${max_val:,.0f}",
                'confidence': 'high' if len(valuations) >= 3 else 'moderate'
            }
        }

    def quick_pre_revenue_valuation(self,
                                   idea_quality: int,
                                   team_quality: int,
                                   prototype_status: int,
                                   market_size: int) -> Dict[str, Any]:
        """
        Quick pre-revenue valuation using Berkus and Risk Factor

        All inputs: 0-100 scale
        """

        # Convert to Berkus scores (0-1)
        berkus_scores = {
            BerkusFactor.SOUND_IDEA: idea_quality / 100,
            BerkusFactor.QUALITY_MANAGEMENT: team_quality / 100,
            BerkusFactor.PROTOTYPE: prototype_status / 100,
            BerkusFactor.STRATEGIC_RELATIONSHIPS: 0.3,
            BerkusFactor.PRODUCT_ROLLOUT: 0.2
        }

        # Convert to risk factor scores (-2 to +2)
        def scale_to_risk(score: int) -> int:
            if score >= 80:
                return 2
            elif score >= 60:
                return 1
            elif score >= 40:
                return 0
            elif score >= 20:
                return -1
            else:
                return -2

        risk_assessments = {
            RiskFactor.MANAGEMENT: scale_to_risk(team_quality),
            RiskFactor.STAGE_OF_BUSINESS: -1,
            RiskFactor.TECHNOLOGY: scale_to_risk(prototype_status),
            RiskFactor.COMPETITION: 0,
            RiskFactor.SALES_MARKETING: scale_to_risk(market_size),
            RiskFactor.FUNDING_CAPITAL: -1
        }

        return self.comprehensive_valuation(
            berkus_scores=berkus_scores,
            risk_factor_assessments=risk_assessments
        )

    def series_a_valuation(self,
                          revenue: float,
                          revenue_growth: float,
                          exit_year: int = 5,
                          exit_multiple: float = 8.0,
                          investment_amount: float = 10_000_000) -> Dict[str, Any]:
        """Series A stage valuation using VC and First Chicago methods"""

        # VC Method inputs
        vc_inputs = {
            'exit_year_metric': revenue * (1 + revenue_growth) ** exit_year,
            'exit_multiple': exit_multiple,
            'years_to_exit': exit_year,
            'investment_amount': investment_amount,
            'stage': 'series_a'
        }

        # First Chicago scenarios
        exit_revenue = revenue * (1 + revenue_growth) ** exit_year
        scenarios = [
            Scenario('Bear Case', 0.25, exit_year + 1, exit_revenue * 0.6 * 6.0, 'Conservative exit'),
            Scenario('Base Case', 0.50, exit_year, exit_revenue * exit_multiple, 'Expected outcome'),
            Scenario('Bull Case', 0.25, exit_year - 1, exit_revenue * 1.4 * 10.0, 'Strong exit')
        ]

        return self.comprehensive_valuation(
            vc_inputs=vc_inputs,
            first_chicago_scenarios=scenarios
        )

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    def _parse_optional_json(val):
        """Parse a JSON string, returning None for 'null' or empty strings"""
        if val is None or val == 'null' or val == '' or val == 'None':
            return None
        try:
            return json.loads(val)
        except (json.JSONDecodeError, TypeError):
            return None

    try:
        if command == "quick_pre_revenue":
            # Rust sends: "quick_pre_revenue" idea_quality team_quality prototype_status market_size
            if len(sys.argv) < 6:
                raise ValueError("Quality scores required: idea_quality, team_quality, prototype_status, market_size")

            # Parse quality scores - accept int, float, or descriptive strings
            def parse_quality(val):
                try:
                    f = float(val)
                    # If 0-1 scale, convert to 0-100
                    return int(f * 100) if f <= 1.0 else int(f)
                except ValueError:
                    # Map string descriptions to 0-100 scale
                    quality_map = {'low': 25, 'small': 25, 'medium': 50, 'moderate': 50,
                                   'high': 75, 'large': 75, 'very_high': 90, 'excellent': 90}
                    return quality_map.get(val.lower(), 50)

            idea_quality = parse_quality(sys.argv[2])
            team_quality = parse_quality(sys.argv[3])
            prototype_status = parse_quality(sys.argv[4])
            market_size = parse_quality(sys.argv[5])

            startup = StartupValuationSummary("Startup")
            valuation = startup.quick_pre_revenue_valuation(
                idea_quality=idea_quality,
                team_quality=team_quality,
                prototype_status=prototype_status,
                market_size=market_size
            )

            result = {"success": True, "data": valuation}
            print(json.dumps(result))

        elif command == "comprehensive":
            # Rust sends: "comprehensive" startup_name berkus_scores scorecard_inputs vc_inputs first_chicago_scenarios risk_factor_assessments
            # Any of the optional args may be "null"
            if len(sys.argv) < 3:
                raise ValueError("Startup name required")

            startup_name = sys.argv[2]
            berkus_scores = _parse_optional_json(sys.argv[3]) if len(sys.argv) > 3 else None
            scorecard_inputs = _parse_optional_json(sys.argv[4]) if len(sys.argv) > 4 else None
            vc_inputs = _parse_optional_json(sys.argv[5]) if len(sys.argv) > 5 else None
            first_chicago_scenarios = _parse_optional_json(sys.argv[6]) if len(sys.argv) > 6 else None
            risk_factor_assessments = _parse_optional_json(sys.argv[7]) if len(sys.argv) > 7 else None

            startup = StartupValuationSummary(startup_name)

            inputs = {}
            if berkus_scores is not None:
                inputs['berkus_scores'] = berkus_scores
            if scorecard_inputs is not None:
                inputs['scorecard_inputs'] = scorecard_inputs
            if vc_inputs is not None:
                inputs['vc_inputs'] = vc_inputs
            if first_chicago_scenarios is not None:
                inputs['first_chicago_scenarios'] = first_chicago_scenarios
            if risk_factor_assessments is not None:
                inputs['risk_factor_assessments'] = risk_factor_assessments

            valuation = startup.comprehensive_valuation(**inputs)

            result = {"success": True, "data": valuation}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}. Available: quick_pre_revenue, comprehensive"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
