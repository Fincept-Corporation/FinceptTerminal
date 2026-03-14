"""
Structured Products Analytics Module
====================================

Mortgage-backed securities (MBS) and asset-backed securities (ABS) analysis
implementing CFA Institute standard methodologies.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Pool characteristics (WAC, WAM, WALA)
  - Prepayment assumptions (CPR, PSA, SMM)
  - Default and loss severity assumptions
  - Interest rate scenarios
  - Tranche structures

OUTPUT:
  - Prepayment metrics (CPR, PSA, SMM)
  - Weighted average life (WAL)
  - Cash flow projections
  - OAS and spread analysis
  - Tranche analytics

PARAMETERS:
  - principal_balance: Outstanding principal - default: 1000000
  - wac: Weighted average coupon - default: 0.06
  - wam: Weighted average maturity (months) - default: 360
  - cpr: Constant prepayment rate - default: 0.06
  - psa: PSA prepayment speed - default: 100
  - default_rate: Annual default rate - default: 0.02
  - loss_severity: Loss given default - default: 0.35
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple
from enum import Enum
import numpy as np
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class SecurityType(Enum):
    """Types of structured products"""
    MBS_PASSTHROUGH = "mbs_passthrough"
    CMO = "cmo"
    ABS = "abs"
    CLO = "clo"
    CDO = "cdo"
    CMBS = "cmbs"


class TrancheType(Enum):
    """CMO tranche types"""
    SEQUENTIAL = "sequential"
    PAC = "pac"  # Planned Amortization Class
    TAC = "tac"  # Targeted Amortization Class
    SUPPORT = "support"
    IO = "interest_only"
    PO = "principal_only"
    Z_TRANCHE = "z_tranche"


@dataclass
class PoolCharacteristics:
    """MBS pool characteristics"""
    principal_balance: float = 1000000
    wac: float = 0.06  # Weighted Average Coupon
    wam: int = 360  # Weighted Average Maturity (months)
    wala: int = 0  # Weighted Average Loan Age (months)
    passthrough_rate: float = 0.055  # Pass-through rate to investors


@dataclass
class CashFlow:
    """Single period cash flow"""
    month: int
    beginning_balance: float
    scheduled_principal: float
    prepayment: float
    interest: float
    total_principal: float
    ending_balance: float
    total_cash_flow: float


class PrepaymentModel:
    """
    Prepayment modeling for MBS analysis.

    Implements CPR, SMM, and PSA prepayment conventions.
    """

    def cpr_to_smm(self, cpr: float) -> float:
        """
        Convert CPR (annual) to SMM (monthly).

        SMM = 1 - (1 - CPR)^(1/12)

        Args:
            cpr: Constant Prepayment Rate (annual)

        Returns:
            Single Monthly Mortality rate
        """
        return 1 - (1 - cpr) ** (1 / 12)

    def smm_to_cpr(self, smm: float) -> float:
        """
        Convert SMM (monthly) to CPR (annual).

        CPR = 1 - (1 - SMM)^12

        Args:
            smm: Single Monthly Mortality

        Returns:
            Constant Prepayment Rate (annual)
        """
        return 1 - (1 - smm) ** 12

    def psa_to_cpr(self, psa_speed: float, month: int) -> float:
        """
        Calculate CPR from PSA speed.

        PSA model: CPR increases 0.2% per month for first 30 months,
        then levels off at 6% (at 100 PSA).

        Args:
            psa_speed: PSA speed as percentage (100 = 100% PSA)
            month: Loan age in months

        Returns:
            CPR for the given month and PSA speed
        """
        if month <= 30:
            base_cpr = 0.002 * month  # 0.2% per month up to 6%
        else:
            base_cpr = 0.06  # 6% after month 30

        return base_cpr * (psa_speed / 100)

    def calculate_prepayment_schedule(
        self,
        wam: int = 360,
        wala: int = 0,
        psa_speed: float = 100,
    ) -> Dict[str, Any]:
        """
        Generate prepayment schedule based on PSA model.

        Args:
            wam: Weighted average maturity (months)
            wala: Weighted average loan age (months)
            psa_speed: PSA speed

        Returns:
            Dictionary with prepayment schedule
        """
        schedule = []

        for month in range(1, wam + 1):
            loan_age = wala + month
            cpr = self.psa_to_cpr(psa_speed, loan_age)
            smm = self.cpr_to_smm(cpr)

            schedule.append({
                'month': month,
                'loan_age': loan_age,
                'cpr': round(cpr, 6),
                'cpr_pct': round(cpr * 100, 4),
                'smm': round(smm, 6),
                'smm_pct': round(smm * 100, 4)
            })

        # Summary statistics
        avg_cpr = np.mean([s['cpr'] for s in schedule])
        max_cpr = max(s['cpr'] for s in schedule)

        return {
            'schedule': schedule[:60],  # First 60 months for brevity
            'psa_speed': psa_speed,
            'average_cpr': round(avg_cpr, 6),
            'max_cpr': round(max_cpr, 6),
            'seasoning_period': min(30 - wala, 30) if wala < 30 else 0
        }


class MBSAnalyzer:
    """
    Mortgage-Backed Securities analysis.

    Provides cash flow projections, WAL, and spread analysis.
    """

    def __init__(self):
        self.prepay_model = PrepaymentModel()

    def project_cash_flows(
        self,
        principal_balance: float = 1000000,
        wac: float = 0.06,
        wam: int = 360,
        passthrough_rate: float = 0.055,
        cpr: float = None,
        psa_speed: float = 100,
        wala: int = 0,
    ) -> Dict[str, Any]:
        """
        Project MBS cash flows.

        Args:
            principal_balance: Initial principal balance
            wac: Weighted average coupon
            wam: Weighted average maturity (months)
            passthrough_rate: Rate passed to investors
            cpr: Constant prepayment rate (if None, use PSA)
            psa_speed: PSA speed (if cpr is None)
            wala: Weighted average loan age

        Returns:
            Dictionary with projected cash flows
        """
        balance = principal_balance
        monthly_rate = wac / 12
        passthrough_monthly = passthrough_rate / 12
        cash_flows = []

        # Calculate scheduled payment (level payment mortgage)
        if monthly_rate > 0:
            scheduled_payment = balance * (monthly_rate * (1 + monthly_rate) ** wam) / \
                               ((1 + monthly_rate) ** wam - 1)
        else:
            scheduled_payment = balance / wam

        total_interest = 0
        total_principal = 0
        total_prepayment = 0

        for month in range(1, wam + 1):
            if balance <= 0:
                break

            # Get prepayment rate
            if cpr is not None:
                smm = self.prepay_model.cpr_to_smm(cpr)
            else:
                current_cpr = self.prepay_model.psa_to_cpr(psa_speed, wala + month)
                smm = self.prepay_model.cpr_to_smm(current_cpr)

            # Interest payment
            interest = balance * passthrough_monthly

            # Scheduled principal
            scheduled_principal = min(scheduled_payment - balance * monthly_rate, balance)

            # Prepayment
            remaining_after_scheduled = balance - scheduled_principal
            prepayment = remaining_after_scheduled * smm

            # Total principal
            total_principal_payment = scheduled_principal + prepayment

            # Ending balance
            ending_balance = balance - total_principal_payment

            # Total cash flow
            total_cf = interest + total_principal_payment

            cash_flows.append({
                'month': month,
                'beginning_balance': round(balance, 2),
                'interest': round(interest, 2),
                'scheduled_principal': round(scheduled_principal, 2),
                'prepayment': round(prepayment, 2),
                'total_principal': round(total_principal_payment, 2),
                'total_cash_flow': round(total_cf, 2),
                'ending_balance': round(max(ending_balance, 0), 2),
                'smm': round(smm, 6)
            })

            total_interest += interest
            total_principal += total_principal_payment
            total_prepayment += prepayment
            balance = max(ending_balance, 0)

        return {
            'cash_flows': cash_flows[:60],  # First 60 months
            'total_cash_flows': len(cash_flows),
            'total_interest': round(total_interest, 2),
            'total_principal': round(total_principal, 2),
            'total_prepayment': round(total_prepayment, 2),
            'final_balance': round(balance, 2),
            'payoff_month': len(cash_flows)
        }

    def calculate_wal(
        self,
        principal_balance: float = 1000000,
        wac: float = 0.06,
        wam: int = 360,
        cpr: float = None,
        psa_speed: float = 100,
        wala: int = 0,
    ) -> Dict[str, Any]:
        """
        Calculate Weighted Average Life (WAL).

        WAL = Σ(t × Principal_t) / Total Principal

        Args:
            principal_balance: Initial principal
            wac: Weighted average coupon
            wam: Weighted average maturity
            cpr: Constant prepayment rate
            psa_speed: PSA speed
            wala: Weighted average loan age

        Returns:
            Dictionary with WAL calculation
        """
        cf_result = self.project_cash_flows(
            principal_balance, wac, wam, wac, cpr, psa_speed, wala
        )

        # Calculate WAL
        weighted_sum = 0
        total_principal = 0

        for cf in cf_result['cash_flows']:
            time_in_years = cf['month'] / 12
            principal = cf['total_principal']
            weighted_sum += time_in_years * principal
            total_principal += principal

        # Add remaining cash flows if truncated
        if len(cf_result['cash_flows']) < cf_result['total_cash_flows']:
            # Need full calculation
            full_cf = self.project_cash_flows(
                principal_balance, wac, wam, wac, cpr, psa_speed, wala
            )
            weighted_sum = 0
            total_principal = 0
            for cf in full_cf['cash_flows']:
                time_in_years = cf['month'] / 12
                principal = cf['total_principal']
                weighted_sum += time_in_years * principal
                total_principal += principal

        wal = weighted_sum / total_principal if total_principal > 0 else 0

        return {
            'wal': round(wal, 4),
            'wal_months': round(wal * 12, 2),
            'stated_maturity': wam / 12,
            'wal_reduction': round((wam / 12) - wal, 4),
            'prepayment_assumption': f'PSA {psa_speed}' if cpr is None else f'CPR {cpr * 100}%',
            'interpretation': f'Average time to receive principal is {round(wal, 2)} years'
        }

    def calculate_wal_sensitivity(
        self,
        principal_balance: float = 1000000,
        wac: float = 0.06,
        wam: int = 360,
        psa_speeds: List[float] = None,
    ) -> Dict[str, Any]:
        """
        Calculate WAL under different prepayment scenarios.

        Args:
            principal_balance: Initial principal
            wac: Weighted average coupon
            wam: Weighted average maturity
            psa_speeds: List of PSA speeds to test

        Returns:
            Dictionary with WAL sensitivity analysis
        """
        if psa_speeds is None:
            psa_speeds = [50, 75, 100, 150, 200, 300, 400]

        results = []

        for psa in psa_speeds:
            wal_result = self.calculate_wal(
                principal_balance, wac, wam, None, psa
            )
            results.append({
                'psa_speed': psa,
                'wal': wal_result['wal'],
                'wal_months': wal_result['wal_months']
            })

        # Calculate sensitivity metrics
        base_wal = next(r['wal'] for r in results if r['psa_speed'] == 100)
        sensitivities = []

        for r in results:
            change = r['wal'] - base_wal
            pct_change = (change / base_wal) * 100 if base_wal > 0 else 0
            sensitivities.append({
                'psa_speed': r['psa_speed'],
                'wal': r['wal'],
                'change_from_100psa': round(change, 4),
                'pct_change': round(pct_change, 2)
            })

        return {
            'wal_sensitivity': sensitivities,
            'base_psa': 100,
            'base_wal': base_wal,
            'min_wal': min(r['wal'] for r in results),
            'max_wal': max(r['wal'] for r in results),
            'contraction_risk': f'WAL shortens to {min(r["wal"] for r in results):.2f} at fast prepay',
            'extension_risk': f'WAL extends to {max(r["wal"] for r in results):.2f} at slow prepay'
        }


class ABSAnalyzer:
    """
    Asset-Backed Securities analysis.

    Handles auto loans, credit cards, and other ABS structures.
    """

    def project_abs_cash_flows(
        self,
        principal_balance: float = 1000000,
        coupon_rate: float = 0.05,
        term_months: int = 60,
        abs_speed: float = 1.0,  # ABS prepayment multiplier
        default_rate: float = 0.02,
        loss_severity: float = 0.50,
    ) -> Dict[str, Any]:
        """
        Project ABS cash flows with defaults.

        Args:
            principal_balance: Initial balance
            coupon_rate: Asset coupon rate
            term_months: Scheduled term
            abs_speed: Prepayment speed multiplier
            default_rate: Annual default rate
            loss_severity: Loss given default

        Returns:
            Dictionary with cash flow projection
        """
        balance = principal_balance
        monthly_rate = coupon_rate / 12
        monthly_default = 1 - (1 - default_rate) ** (1 / 12)

        cash_flows = []
        total_defaults = 0
        total_losses = 0
        total_recoveries = 0

        # Level payment amortization
        if monthly_rate > 0:
            payment = balance * (monthly_rate * (1 + monthly_rate) ** term_months) / \
                     ((1 + monthly_rate) ** term_months - 1)
        else:
            payment = balance / term_months

        for month in range(1, term_months + 1):
            if balance <= 0:
                break

            # Defaults
            defaults = balance * monthly_default
            loss = defaults * loss_severity
            recovery = defaults * (1 - loss_severity)

            total_defaults += defaults
            total_losses += loss
            total_recoveries += recovery

            # Interest
            interest = (balance - defaults) * monthly_rate

            # Principal (adjusted for defaults)
            scheduled_principal = payment - balance * monthly_rate
            principal = min(scheduled_principal * abs_speed, balance - defaults)

            ending_balance = balance - principal - defaults

            cash_flows.append({
                'month': month,
                'beginning_balance': round(balance, 2),
                'interest': round(interest, 2),
                'principal': round(principal, 2),
                'defaults': round(defaults, 2),
                'losses': round(loss, 2),
                'recoveries': round(recovery, 2),
                'ending_balance': round(max(ending_balance, 0), 2)
            })

            balance = max(ending_balance, 0)

        cumulative_default_rate = total_defaults / principal_balance
        cumulative_loss_rate = total_losses / principal_balance

        return {
            'cash_flows': cash_flows,
            'total_defaults': round(total_defaults, 2),
            'total_losses': round(total_losses, 2),
            'total_recoveries': round(total_recoveries, 2),
            'cumulative_default_rate': round(cumulative_default_rate, 6),
            'cumulative_loss_rate': round(cumulative_loss_rate, 6),
            'loss_severity_actual': round(total_losses / total_defaults, 4) if total_defaults > 0 else 0
        }

    def analyze_credit_enhancement(
        self,
        pool_balance: float = 1000000,
        subordination_pct: float = 0.10,
        reserve_account: float = 0,
        excess_spread: float = 0.02,
        expected_loss: float = 0.03,
    ) -> Dict[str, Any]:
        """
        Analyze credit enhancement levels.

        Args:
            pool_balance: Total pool balance
            subordination_pct: Subordination percentage
            reserve_account: Reserve account balance
            excess_spread: Annual excess spread
            expected_loss: Expected cumulative loss

        Returns:
            Dictionary with credit enhancement analysis
        """
        subordination_amount = pool_balance * subordination_pct
        total_enhancement = subordination_amount + reserve_account

        # Coverage ratio
        expected_loss_amount = pool_balance * expected_loss
        coverage_ratio = total_enhancement / expected_loss_amount if expected_loss_amount > 0 else float('inf')

        # Break-even loss rate
        breakeven_loss = (total_enhancement + excess_spread * pool_balance) / pool_balance

        return {
            'subordination_pct': round(subordination_pct * 100, 2),
            'subordination_amount': round(subordination_amount, 2),
            'reserve_account': round(reserve_account, 2),
            'total_credit_enhancement': round(total_enhancement, 2),
            'total_enhancement_pct': round((total_enhancement / pool_balance) * 100, 2),
            'expected_loss_amount': round(expected_loss_amount, 2),
            'coverage_ratio': round(coverage_ratio, 2),
            'breakeven_loss_rate': round(breakeven_loss * 100, 2),
            'excess_spread_annual': round(excess_spread * 100, 2),
            'adequacy': 'Adequate' if coverage_ratio >= 2 else 'May be insufficient'
        }


class CMOAnalyzer:
    """
    Collateralized Mortgage Obligation analysis.

    Handles sequential, PAC, and support tranche structures.
    """

    def analyze_sequential_cmo(
        self,
        collateral_balance: float = 1000000,
        tranche_sizes: List[float] = None,
        coupon_rates: List[float] = None,
        wac: float = 0.06,
        wam: int = 360,
        psa_speed: float = 100,
    ) -> Dict[str, Any]:
        """
        Analyze sequential pay CMO structure.

        Args:
            collateral_balance: Total collateral balance
            tranche_sizes: Size of each tranche (as % of collateral)
            coupon_rates: Coupon rate for each tranche
            wac: Collateral WAC
            wam: Collateral WAM
            psa_speed: Prepayment speed

        Returns:
            Dictionary with tranche analysis
        """
        if tranche_sizes is None:
            tranche_sizes = [0.30, 0.30, 0.25, 0.15]  # A, B, C, Z
        if coupon_rates is None:
            coupon_rates = [0.04, 0.045, 0.05, 0.055]

        # Generate collateral cash flows
        mbs = MBSAnalyzer()
        cf_result = mbs.project_cash_flows(
            collateral_balance, wac, wam, wac, None, psa_speed
        )

        # Initialize tranches
        tranches = []
        for i, (size, coupon) in enumerate(zip(tranche_sizes, coupon_rates)):
            tranches.append({
                'name': chr(65 + i),  # A, B, C, D...
                'initial_balance': collateral_balance * size,
                'balance': collateral_balance * size,
                'coupon': coupon,
                'principal_received': 0,
                'interest_received': 0,
                'first_principal_month': None,
                'last_principal_month': None
            })

        # Allocate cash flows sequentially
        for cf in cf_result['cash_flows']:
            remaining_principal = cf['total_principal']

            for tranche in tranches:
                if tranche['balance'] > 0:
                    # Interest payment
                    interest = tranche['balance'] * (tranche['coupon'] / 12)
                    tranche['interest_received'] += interest

                    # Principal payment (sequential)
                    principal_to_tranche = min(remaining_principal, tranche['balance'])
                    tranche['principal_received'] += principal_to_tranche
                    tranche['balance'] -= principal_to_tranche
                    remaining_principal -= principal_to_tranche

                    if principal_to_tranche > 0:
                        if tranche['first_principal_month'] is None:
                            tranche['first_principal_month'] = cf['month']
                        tranche['last_principal_month'] = cf['month']

                    if remaining_principal <= 0:
                        break

        # Calculate WAL for each tranche
        results = []
        for tranche in tranches:
            wal = None
            if tranche['principal_received'] > 0:
                # Simplified WAL estimation based on payment window
                if tranche['first_principal_month'] and tranche['last_principal_month']:
                    avg_month = (tranche['first_principal_month'] + tranche['last_principal_month']) / 2
                    wal = avg_month / 12

            results.append({
                'tranche': tranche['name'],
                'initial_balance': round(tranche['initial_balance'], 2),
                'coupon': round(tranche['coupon'] * 100, 2),
                'total_principal': round(tranche['principal_received'], 2),
                'total_interest': round(tranche['interest_received'], 2),
                'first_principal_month': tranche['first_principal_month'],
                'last_principal_month': tranche['last_principal_month'],
                'estimated_wal': round(wal, 2) if wal else None,
                'payment_window': f"Month {tranche['first_principal_month']} to {tranche['last_principal_month']}" if tranche['first_principal_month'] else 'N/A'
            })

        return {
            'structure': 'Sequential Pay CMO',
            'collateral_balance': collateral_balance,
            'psa_speed': psa_speed,
            'tranches': results,
            'num_tranches': len(results)
        }


def run_structured_products_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for structured products analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'mbs_cash_flows')

    try:
        if analysis_type == 'mbs_cash_flows':
            analyzer = MBSAnalyzer()
            return analyzer.project_cash_flows(
                principal_balance=params.get('principal_balance', 1000000),
                wac=params.get('wac', 0.06),
                wam=params.get('wam', 360),
                passthrough_rate=params.get('passthrough_rate', 0.055),
                cpr=params.get('cpr'),
                psa_speed=params.get('psa_speed', 100),
                wala=params.get('wala', 0)
            )

        elif analysis_type == 'wal':
            analyzer = MBSAnalyzer()
            return analyzer.calculate_wal(
                principal_balance=params.get('principal_balance', 1000000),
                wac=params.get('wac', 0.06),
                wam=params.get('wam', 360),
                cpr=params.get('cpr'),
                psa_speed=params.get('psa_speed', 100),
                wala=params.get('wala', 0)
            )

        elif analysis_type == 'wal_sensitivity':
            analyzer = MBSAnalyzer()
            return analyzer.calculate_wal_sensitivity(
                principal_balance=params.get('principal_balance', 1000000),
                wac=params.get('wac', 0.06),
                wam=params.get('wam', 360),
                psa_speeds=params.get('psa_speeds')
            )

        elif analysis_type == 'prepayment_schedule':
            model = PrepaymentModel()
            return model.calculate_prepayment_schedule(
                wam=params.get('wam', 360),
                wala=params.get('wala', 0),
                psa_speed=params.get('psa_speed', 100)
            )

        elif analysis_type == 'abs_cash_flows':
            analyzer = ABSAnalyzer()
            return analyzer.project_abs_cash_flows(
                principal_balance=params.get('principal_balance', 1000000),
                coupon_rate=params.get('coupon_rate', 0.05),
                term_months=params.get('term_months', 60),
                abs_speed=params.get('abs_speed', 1.0),
                default_rate=params.get('default_rate', 0.02),
                loss_severity=params.get('loss_severity', 0.50)
            )

        elif analysis_type == 'credit_enhancement':
            analyzer = ABSAnalyzer()
            return analyzer.analyze_credit_enhancement(
                pool_balance=params.get('pool_balance', 1000000),
                subordination_pct=params.get('subordination_pct', 0.10),
                reserve_account=params.get('reserve_account', 0),
                excess_spread=params.get('excess_spread', 0.02),
                expected_loss=params.get('expected_loss', 0.03)
            )

        elif analysis_type == 'sequential_cmo':
            analyzer = CMOAnalyzer()
            return analyzer.analyze_sequential_cmo(
                collateral_balance=params.get('collateral_balance', 1000000),
                tranche_sizes=params.get('tranche_sizes'),
                coupon_rates=params.get('coupon_rates'),
                wac=params.get('wac', 0.06),
                wam=params.get('wam', 360),
                psa_speed=params.get('psa_speed', 100)
            )

        elif analysis_type == 'cpr_smm_convert':
            model = PrepaymentModel()
            if 'cpr' in params:
                smm = model.cpr_to_smm(params['cpr'])
                return {'cpr': params['cpr'], 'smm': round(smm, 6)}
            elif 'smm' in params:
                cpr = model.smm_to_cpr(params['smm'])
                return {'smm': params['smm'], 'cpr': round(cpr, 6)}
            else:
                return {'error': 'Provide either cpr or smm parameter'}

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Structured products analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_structured_products_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Structured Products Demo:")

        mbs = MBSAnalyzer()

        # WAL calculation
        result = mbs.calculate_wal(psa_speed=150)
        print(f"\nWAL at 150 PSA: {result['wal']} years")

        # WAL sensitivity
        result = mbs.calculate_wal_sensitivity()
        print("\nWAL Sensitivity:")
        for r in result['wal_sensitivity']:
            print(f"  PSA {r['psa_speed']}: {r['wal']:.2f} years")
