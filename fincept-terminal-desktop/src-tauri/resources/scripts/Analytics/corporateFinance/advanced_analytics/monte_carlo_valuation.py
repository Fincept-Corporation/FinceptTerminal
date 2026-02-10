"""Monte Carlo Simulation for M&A Valuation"""
from typing import Dict, Any, List, Optional, Callable
import numpy as np

class MonteCarloValuation:
    """Monte Carlo simulation for deal valuation under uncertainty"""

    def __init__(self, num_simulations: int = 10000, random_seed: Optional[int] = None):
        self.num_simulations = num_simulations
        if random_seed:
            np.random.seed(random_seed)

    def simulate_synergies(self, base_revenue_synergy: float,
                          base_cost_synergy: float,
                          revenue_std_dev_pct: float,
                          cost_std_dev_pct: float,
                          correlation: float = 0.3) -> Dict[str, Any]:
        """Simulate synergy outcomes with correlated uncertainties"""

        revenue_std = base_revenue_synergy * revenue_std_dev_pct
        cost_std = base_cost_synergy * cost_std_dev_pct

        mean = [base_revenue_synergy, base_cost_synergy]
        cov = [
            [revenue_std**2, correlation * revenue_std * cost_std],
            [correlation * revenue_std * cost_std, cost_std**2]
        ]

        simulated_synergies = np.random.multivariate_normal(mean, cov, self.num_simulations)

        revenue_synergies = simulated_synergies[:, 0]
        cost_synergies = simulated_synergies[:, 1]
        total_synergies = revenue_synergies + cost_synergies

        revenue_synergies = np.maximum(revenue_synergies, 0)
        cost_synergies = np.maximum(cost_synergies, 0)
        total_synergies = revenue_synergies + cost_synergies

        return {
            'revenue_synergies': {
                'mean': float(np.mean(revenue_synergies)),
                'median': float(np.median(revenue_synergies)),
                'std': float(np.std(revenue_synergies)),
                'percentile_5': float(np.percentile(revenue_synergies, 5)),
                'percentile_25': float(np.percentile(revenue_synergies, 25)),
                'percentile_75': float(np.percentile(revenue_synergies, 75)),
                'percentile_95': float(np.percentile(revenue_synergies, 95))
            },
            'cost_synergies': {
                'mean': float(np.mean(cost_synergies)),
                'median': float(np.median(cost_synergies)),
                'std': float(np.std(cost_synergies)),
                'percentile_5': float(np.percentile(cost_synergies, 5)),
                'percentile_25': float(np.percentile(cost_synergies, 25)),
                'percentile_75': float(np.percentile(cost_synergies, 75)),
                'percentile_95': float(np.percentile(cost_synergies, 95))
            },
            'total_synergies': {
                'mean': float(np.mean(total_synergies)),
                'median': float(np.median(total_synergies)),
                'std': float(np.std(total_synergies)),
                'percentile_5': float(np.percentile(total_synergies, 5)),
                'percentile_25': float(np.percentile(total_synergies, 25)),
                'percentile_75': float(np.percentile(total_synergies, 75)),
                'percentile_95': float(np.percentile(total_synergies, 95))
            },
            'probability_positive': float(np.sum(total_synergies > 0) / self.num_simulations * 100),
            'correlation': correlation,
            'num_simulations': self.num_simulations
        }

    def simulate_dcf_valuation(self, base_fcf: List[float],
                               fcf_growth_mean: float,
                               fcf_growth_std: float,
                               terminal_growth_mean: float,
                               terminal_growth_std: float,
                               wacc_mean: float,
                               wacc_std: float) -> Dict[str, Any]:
        """Simulate DCF valuation with uncertain inputs"""

        valuations = []

        for _ in range(self.num_simulations):
            growth_rates = np.random.normal(fcf_growth_mean, fcf_growth_std, len(base_fcf))
            fcf_projection = [base_fcf[0] * np.prod(1 + growth_rates[:i+1]) for i in range(len(base_fcf))]

            terminal_growth = np.random.normal(terminal_growth_mean, terminal_growth_std)
            terminal_growth = np.clip(terminal_growth, 0, 0.06)

            wacc = np.random.normal(wacc_mean, wacc_std)
            wacc = np.clip(wacc, 0.05, 0.20)

            terminal_fcf = fcf_projection[-1] * (1 + terminal_growth)
            terminal_value = terminal_fcf / (wacc - terminal_growth)

            pv_fcf = sum(fcf / ((1 + wacc) ** (i + 1)) for i, fcf in enumerate(fcf_projection))
            pv_terminal = terminal_value / ((1 + wacc) ** len(base_fcf))

            valuation = pv_fcf + pv_terminal
            valuations.append(valuation)

        valuations = np.array(valuations)

        return {
            'valuation_statistics': {
                'mean': float(np.mean(valuations)),
                'median': float(np.median(valuations)),
                'std': float(np.std(valuations)),
                'min': float(np.min(valuations)),
                'max': float(np.max(valuations)),
                'percentile_5': float(np.percentile(valuations, 5)),
                'percentile_10': float(np.percentile(valuations, 10)),
                'percentile_25': float(np.percentile(valuations, 25)),
                'percentile_75': float(np.percentile(valuations, 75)),
                'percentile_90': float(np.percentile(valuations, 90)),
                'percentile_95': float(np.percentile(valuations, 95))
            },
            'input_assumptions': {
                'fcf_growth_mean': fcf_growth_mean * 100,
                'fcf_growth_std': fcf_growth_std * 100,
                'terminal_growth_mean': terminal_growth_mean * 100,
                'terminal_growth_std': terminal_growth_std * 100,
                'wacc_mean': wacc_mean * 100,
                'wacc_std': wacc_std * 100
            },
            'num_simulations': self.num_simulations
        }

    def simulate_deal_returns(self, purchase_price: float,
                             synergy_mean: float,
                             synergy_std: float,
                             integration_cost_mean: float,
                             integration_cost_std: float,
                             years_to_realize: int = 3,
                             discount_rate: float = 0.10) -> Dict[str, Any]:
        """Simulate deal returns considering synergies and integration costs"""

        npvs = []
        irrs = []

        for _ in range(self.num_simulations):
            annual_synergy = np.random.normal(synergy_mean, synergy_std)
            annual_synergy = max(0, annual_synergy)

            integration_cost = np.random.normal(integration_cost_mean, integration_cost_std)
            integration_cost = max(0, integration_cost)

            cash_flows = [-purchase_price - integration_cost]

            for year in range(1, years_to_realize + 6):
                if year <= years_to_realize:
                    realized_synergy = annual_synergy * (year / years_to_realize)
                else:
                    realized_synergy = annual_synergy

                cash_flows.append(realized_synergy)

            npv = sum(cf / ((1 + discount_rate) ** i) for i, cf in enumerate(cash_flows))
            npvs.append(npv)

            try:
                irr = np.irr(cash_flows)
                if not np.isnan(irr) and -1 < irr < 2:
                    irrs.append(irr)
            except:
                pass

        npvs = np.array(npvs)
        irrs = np.array(irrs) if irrs else np.array([0])

        probability_positive_npv = float(np.sum(npvs > 0) / self.num_simulations * 100)

        return {
            'npv_statistics': {
                'mean': float(np.mean(npvs)),
                'median': float(np.median(npvs)),
                'std': float(np.std(npvs)),
                'percentile_5': float(np.percentile(npvs, 5)),
                'percentile_25': float(np.percentile(npvs, 25)),
                'percentile_75': float(np.percentile(npvs, 75)),
                'percentile_95': float(np.percentile(npvs, 95))
            },
            'irr_statistics': {
                'mean': float(np.mean(irrs)) * 100 if len(irrs) > 0 else None,
                'median': float(np.median(irrs)) * 100 if len(irrs) > 0 else None,
                'std': float(np.std(irrs)) * 100 if len(irrs) > 0 else None
            },
            'probability_positive_npv': probability_positive_npv,
            'probability_negative_npv': 100 - probability_positive_npv,
            'value_at_risk_5pct': float(np.percentile(npvs, 5)),
            'expected_shortfall_5pct': float(np.mean(npvs[npvs <= np.percentile(npvs, 5)])),
            'purchase_price': purchase_price,
            'num_simulations': self.num_simulations
        }

    def simulate_accretion_dilution(self, acquirer_eps: float,
                                   target_eps: float,
                                   purchase_price_mean: float,
                                   purchase_price_std: float,
                                   synergy_mean: float,
                                   synergy_std: float,
                                   acquirer_shares: float,
                                   target_shares: float,
                                   payment_stock_pct: float = 0.5) -> Dict[str, Any]:
        """Simulate EPS accretion/dilution"""

        accretion_pcts = []

        for _ in range(self.num_simulations):
            purchase_price = np.random.normal(purchase_price_mean, purchase_price_std)
            purchase_price = max(purchase_price_mean * 0.5, purchase_price)

            synergy = np.random.normal(synergy_mean, synergy_std)
            synergy = max(0, synergy)

            stock_consideration = purchase_price * payment_stock_pct
            acquirer_price = purchase_price / target_shares

            new_shares = (stock_consideration / acquirer_price) * (target_shares / acquirer_shares)

            combined_earnings = (acquirer_eps * acquirer_shares) + (target_eps * target_shares) + synergy
            pro_forma_shares = acquirer_shares + new_shares

            pro_forma_eps = combined_earnings / pro_forma_shares

            accretion_pct = ((pro_forma_eps - acquirer_eps) / acquirer_eps) * 100
            accretion_pcts.append(accretion_pct)

        accretion_pcts = np.array(accretion_pcts)

        probability_accretive = float(np.sum(accretion_pcts > 0) / self.num_simulations * 100)

        return {
            'accretion_statistics': {
                'mean': float(np.mean(accretion_pcts)),
                'median': float(np.median(accretion_pcts)),
                'std': float(np.std(accretion_pcts)),
                'percentile_5': float(np.percentile(accretion_pcts, 5)),
                'percentile_25': float(np.percentile(accretion_pcts, 25)),
                'percentile_75': float(np.percentile(accretion_pcts, 75)),
                'percentile_95': float(np.percentile(accretion_pcts, 95))
            },
            'probability_accretive': probability_accretive,
            'probability_dilutive': 100 - probability_accretive,
            'expected_accretion_pct': float(np.mean(accretion_pcts)),
            'downside_scenario_5pct': float(np.percentile(accretion_pcts, 5)),
            'upside_scenario_95pct': float(np.percentile(accretion_pcts, 95)),
            'num_simulations': self.num_simulations
        }

    def value_at_risk_analysis(self, deal_value: float,
                              value_distribution: np.ndarray,
                              confidence_level: float = 0.95) -> Dict[str, Any]:
        """Calculate Value at Risk metrics"""

        var_level = 1 - confidence_level
        var_value = float(np.percentile(value_distribution, var_level * 100))

        tail_values = value_distribution[value_distribution <= var_value]
        expected_shortfall = float(np.mean(tail_values)) if len(tail_values) > 0 else var_value

        return {
            'confidence_level': confidence_level * 100,
            'value_at_risk': var_value,
            'expected_shortfall': expected_shortfall,
            'potential_loss': deal_value - var_value,
            'potential_loss_pct': ((deal_value - var_value) / deal_value * 100) if deal_value > 0 else 0
        }

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import sys
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "monte_carlo":
            if len(sys.argv) < 3:
                raise ValueError("Monte Carlo parameters required")

            # Accept either JSON dict or positional args
            try:
                params = json.loads(sys.argv[2])
                base_valuation = float(params.get('base_valuation', params.get('base_value', 1000000000)))
                revenue_growth_mean = float(params.get('revenue_growth_mean', params.get('growth_mean', 0.05)))
                revenue_growth_std = float(params.get('revenue_growth_std', params.get('growth_std', params.get('volatility', 0.15))))
                margin_mean = float(params.get('margin_mean', 0.15))
                margin_std = float(params.get('margin_std', 0.05))
                discount_rate = float(params.get('discount_rate', 0.10))
                simulations = int(params.get('num_simulations', params.get('simulations', 1000)))
            except (json.JSONDecodeError, TypeError):
                if len(sys.argv) < 9:
                    raise ValueError("All parameters required: base_valuation, revenue_growth_mean, revenue_growth_std, margin_mean, margin_std, discount_rate, simulations")
                base_valuation = float(sys.argv[2])
                revenue_growth_mean = float(sys.argv[3])
                revenue_growth_std = float(sys.argv[4])
                margin_mean = float(sys.argv[5])
                margin_std = float(sys.argv[6])
                discount_rate = float(sys.argv[7])
                simulations = int(sys.argv[8])

            mc = MonteCarloValuation(num_simulations=simulations)

            analysis = mc.simulate_synergies(
                base_revenue_synergy=base_valuation * revenue_growth_mean,
                base_cost_synergy=base_valuation * margin_mean,
                revenue_std_dev_pct=revenue_growth_std,
                cost_std_dev_pct=margin_std
            )

            result = {"success": True, "data": analysis}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
