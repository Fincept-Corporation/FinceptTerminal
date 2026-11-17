"""
Structured Products Analytics Module
====================================

Mortgage-backed (MBS) and asset-backed (ABS) securities analysis (CFA-curriculum methods).

Conventions
-----------
* Monthly level-payment amortisation. The scheduled payment is RE-LEVELLED every month on the
  surviving balance and remaining term: payment_t = B_t * r / (1 - (1 + r)^-(WAM - t + 1)), r = WAC/12.
  Scheduled principal = payment - B_t * r; prepayment = SMM_t * (B_t - scheduled principal).
* Investor interest uses the pass-through rate (WAC minus servicing/guarantee fees).
* PSA: CPR_t = 6% x min(age_t, 30) / 30 x PSA/100, age_t = WALA + t (month 1 of a new pool = 0.2% CPR).
  SMM = 1 - (1 - CPR)^(1/12). CPR is capped at 100%.
* WAL = sum(t x principal_t) / sum(principal_t), t in years (months / 12), over the FULL cash-flow
  projection (outputs that list monthly rows may show only the first `max_rows`).
* ABS prepayments use the ABS (absolute prepayment speed) convention: abs_speed is in percent of the
  original pool per month (1.5 = 1.5% ABS), SMM_t = ABS / (1 - ABS x (t - 1)).

PARAMETERS:
  principal_balance (1,000,000), wac (0.06), wam (360 months), wala (0), passthrough_rate,
  psa_speed (100) or cpr, default_rate, loss_severity, tranche_sizes, coupon_rates
"""

import logging
import os
import sys
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict, List, Optional, Tuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num  # noqa: E402

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


def _wal_years(rows: List[Dict[str, float]], key: str = 'total_principal') -> Optional[float]:
    paid = sum(row[key] for row in rows)
    if paid <= 0:
        return None
    return sum(row['month'] * row[key] for row in rows) / paid / 12.0


class PrepaymentModel:
    """Prepayment conventions: CPR, SMM, PSA."""

    def cpr_to_smm(self, cpr: float) -> float:
        """SMM = 1 - (1 - CPR)^(1/12)"""
        if not 0 <= cpr <= 1:
            raise FIError(f'cpr must be between 0 and 1 (got {cpr:g})')
        return 1 - (1 - cpr) ** (1 / 12)

    def smm_to_cpr(self, smm: float) -> float:
        """CPR = 1 - (1 - SMM)^12"""
        if not 0 <= smm <= 1:
            raise FIError(f'smm must be between 0 and 1 (got {smm:g})')
        return 1 - (1 - smm) ** 12

    def psa_to_cpr(self, psa_speed: float, month: int) -> float:
        """CPR at loan age `month` under the PSA benchmark (6% x min(age, 30)/30 x PSA/100), capped at 100%."""
        base_cpr = 0.06 * min(max(month, 0), 30) / 30
        return min(base_cpr * (psa_speed / 100), 1.0)

    def calculate_prepayment_schedule(
        self,
        wam: int = 360,
        wala: int = 0,
        psa_speed: float = 100,
        max_rows: int = 60,
    ) -> Dict[str, Any]:
        """Monthly CPR / SMM under a PSA assumption."""
        _check_pool(wam=wam, wala=wala, psa_speed=psa_speed)
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
        shown = schedule if not max_rows else schedule[:max_rows]
        return {
            'schedule': shown,
            'rows_shown': len(shown),
            'total_rows': len(schedule),
            'truncated': len(shown) < len(schedule),
            'psa_speed': psa_speed,
            'average_cpr': round(sum(s['cpr'] for s in schedule) / len(schedule), 6),
            'max_cpr': round(max(s['cpr'] for s in schedule), 6),
            'seasoning_period': max(30 - wala, 0),
        }


def _check_pool(*, principal_balance=None, wac=None, wam=None, wala=None, psa_speed=None, cpr=None,
                passthrough_rate=None):
    if principal_balance is not None and principal_balance <= 0:
        raise FIError('principal_balance must be > 0')
    if wac is not None and not 0 <= wac < 1:
        raise FIError('wac must be a decimal between 0 and 1 (e.g. 0.06)')
    if passthrough_rate is not None and not 0 <= passthrough_rate < 1:
        raise FIError('passthrough_rate must be a decimal between 0 and 1')
    if wam is not None and not 1 <= wam <= 600:
        raise FIError('wam must be a whole number of months between 1 and 600')
    if wala is not None and wala < 0:
        raise FIError('wala must be >= 0')
    if psa_speed is not None and psa_speed < 0:
        raise FIError('psa_speed must be >= 0')
    if cpr is not None and not 0 <= cpr <= 1:
        raise FIError('cpr must be between 0 and 1')


class MBSAnalyzer:
    """Mortgage pass-through cash flows and weighted average life."""

    def __init__(self):
        self.prepay_model = PrepaymentModel()

    def _project(self, principal_balance: float, wac: float, wam: int, passthrough_rate: float,
                 cpr: Optional[float], psa_speed: float, wala: int) -> List[Dict[str, float]]:
        """Full-precision monthly projection (every month to payoff)."""
        _check_pool(principal_balance=principal_balance, wac=wac, wam=wam, wala=wala, psa_speed=psa_speed,
                    cpr=cpr, passthrough_rate=passthrough_rate)
        r = wac / 12
        pt = passthrough_rate / 12
        bal = principal_balance
        fixed_smm = self.prepay_model.cpr_to_smm(cpr) if cpr is not None else None
        rows = []
        for month in range(1, wam + 1):
            if bal <= principal_balance * 1e-12:
                break
            remaining = wam - month + 1
            payment = bal * r / (1 - (1 + r) ** -remaining) if r > 0 else bal / remaining
            scheduled = min(payment - bal * r, bal)
            if fixed_smm is not None:
                smm = fixed_smm
            else:
                smm = self.prepay_model.cpr_to_smm(self.prepay_model.psa_to_cpr(psa_speed, wala + month))
            prepayment = (bal - scheduled) * smm
            principal = scheduled + prepayment
            interest = bal * pt
            ending = bal - principal
            if ending < principal_balance * 1e-12:
                ending = 0.0
            rows.append({
                'month': month,
                'beginning_balance': bal,
                'scheduled_payment': payment,
                'interest': interest,
                'scheduled_principal': scheduled,
                'prepayment': prepayment,
                'total_principal': principal,
                'total_cash_flow': interest + principal,
                'ending_balance': ending,
                'smm': smm,
            })
            bal = ending
        return rows

    def project_cash_flows(
        self,
        principal_balance: float = 1000000,
        wac: float = 0.06,
        wam: int = 360,
        passthrough_rate: float = 0.055,
        cpr: float = None,
        psa_speed: float = 100,
        wala: int = 0,
        max_rows: int = 60,
    ) -> Dict[str, Any]:
        """Project pass-through cash flows (totals and WAL over the full projection)."""
        rows = self._project(principal_balance, wac, wam, passthrough_rate, cpr, psa_speed, wala)
        shown = rows if not max_rows else rows[:max_rows]
        wal = _wal_years(rows)
        return {
            'cash_flows': [{
                'month': x['month'],
                'beginning_balance': round(x['beginning_balance'], 2),
                'interest': round(x['interest'], 2),
                'scheduled_principal': round(x['scheduled_principal'], 2),
                'prepayment': round(x['prepayment'], 2),
                'total_principal': round(x['total_principal'], 2),
                'total_cash_flow': round(x['total_cash_flow'], 2),
                'ending_balance': round(x['ending_balance'], 2),
                'smm': round(x['smm'], 6),
            } for x in shown],
            'rows_shown': len(shown),
            'truncated': len(shown) < len(rows),
            'total_cash_flows': len(rows),
            'total_interest': round(sum(x['interest'] for x in rows), 2),
            'total_principal': round(sum(x['total_principal'] for x in rows), 2),
            'total_scheduled_principal': round(sum(x['scheduled_principal'] for x in rows), 2),
            'total_prepayment': round(sum(x['prepayment'] for x in rows), 2),
            'final_balance': round(rows[-1]['ending_balance'], 2) if rows else round(principal_balance, 2),
            'payoff_month': len(rows),
            'wal_years': round(wal, 4) if wal is not None else None,
            'prepayment_assumption': f'PSA {psa_speed:g}' if cpr is None else f'CPR {cpr * 100:g}%',
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
        """WAL = sum(t x principal_t) / total principal, t in years."""
        rows = self._project(principal_balance, wac, wam, wac, cpr, psa_speed, wala)
        wal = _wal_years(rows)
        if wal is None:
            raise FIError('no principal is paid under these inputs')
        return {
            'wal': round(wal, 4),
            'wal_months': round(wal * 12, 2),
            'stated_maturity': wam / 12,
            'wal_reduction': round((wam / 12) - wal, 4),
            'payoff_month': len(rows),
            'prepayment_assumption': f'PSA {psa_speed:g}' if cpr is None else f'CPR {cpr * 100:g}%',
            'interpretation': f'Average time to receive principal is {round(wal, 2)} years'
        }

    def calculate_wal_sensitivity(
        self,
        principal_balance: float = 1000000,
        wac: float = 0.06,
        wam: int = 360,
        psa_speeds: List[float] = None,
        wala: int = 0,
    ) -> Dict[str, Any]:
        """WAL under several PSA speeds, compared with 100 PSA (always computed as the base)."""
        if psa_speeds is None:
            psa_speeds = [50, 75, 100, 150, 200, 300, 400]
        if not psa_speeds:
            raise FIError('psa_speeds must be a non-empty list')
        base_wal = self.calculate_wal(principal_balance, wac, wam, None, 100, wala)['wal']
        sensitivities = []
        for psa in psa_speeds:
            wal = self.calculate_wal(principal_balance, wac, wam, None, psa, wala)['wal']
            change = wal - base_wal
            sensitivities.append({
                'psa_speed': psa,
                'wal': wal,
                'wal_months': round(wal * 12, 2),
                'change_from_100psa': round(change, 4),
                'pct_change': round(change / base_wal * 100, 2),
            })
        wals = [s['wal'] for s in sensitivities]
        return {
            'wal_sensitivity': sensitivities,
            'base_psa': 100,
            'base_wal': base_wal,
            'min_wal': min(wals),
            'max_wal': max(wals),
            'contraction_risk': f'WAL shortens to {min(wals):.2f} years at the fastest speed tested',
            'extension_risk': f'WAL extends to {max(wals):.2f} years at the slowest speed tested'
        }


class ABSAnalyzer:
    """Asset-backed securities: amortising pools with defaults, recoveries and ABS-convention prepayments."""

    def project_abs_cash_flows(
        self,
        principal_balance: float = 1000000,
        coupon_rate: float = 0.05,
        term_months: int = 60,
        abs_speed: float = 0.0,
        default_rate: float = 0.02,
        loss_severity: float = 0.50,
    ) -> Dict[str, Any]:
        """
        Monthly projection. Each month: defaults = balance x MDR (MDR = 1 - (1 - annual default rate)^(1/12));
        recoveries = defaults x (1 - loss_severity), received the same month; the performing balance pays a
        re-levelled scheduled payment; prepayments follow the ABS convention (abs_speed in percent).
        Investor cash flow = interest + scheduled principal + prepayment + recoveries.
        """
        if principal_balance <= 0:
            raise FIError('principal_balance must be > 0')
        if not 0 <= coupon_rate < 1:
            raise FIError('coupon_rate must be a decimal between 0 and 1')
        if not 1 <= term_months <= 600:
            raise FIError('term_months must be between 1 and 600')
        if not 0 <= default_rate < 1:
            raise FIError('default_rate must be >= 0 and < 1 (annual, decimal)')
        if not 0 <= loss_severity <= 1:
            raise FIError('loss_severity must be between 0 and 1')
        if not 0 <= abs_speed < 100:
            raise FIError('abs_speed must be >= 0 (percent of the original pool per month, e.g. 1.5)')

        abs_dec = abs_speed / 100.0
        r = coupon_rate / 12
        mdr = 1 - (1 - default_rate) ** (1 / 12)
        bal = principal_balance
        rows = []
        tot = {'defaults': 0.0, 'losses': 0.0, 'recoveries': 0.0, 'scheduled': 0.0, 'prepay': 0.0,
               'interest': 0.0}
        for month in range(1, term_months + 1):
            if bal <= principal_balance * 1e-12:
                break
            defaults = bal * mdr
            loss = defaults * loss_severity
            recovery = defaults - loss
            perf = bal - defaults
            remaining = term_months - month + 1
            payment = perf * r / (1 - (1 + r) ** -remaining) if r > 0 else perf / remaining
            interest = perf * r
            scheduled = min(payment - interest, perf)
            if abs_dec > 0:
                denom = 1 - abs_dec * (month - 1)
                smm = 1.0 if denom <= abs_dec else abs_dec / denom
            else:
                smm = 0.0
            prepay = (perf - scheduled) * smm
            principal = scheduled + prepay
            ending = perf - principal
            if ending < principal_balance * 1e-12:
                ending = 0.0
            rows.append({
                'month': month,
                'beginning_balance': round(bal, 2),
                'interest': round(interest, 2),
                'scheduled_principal': round(scheduled, 2),
                'prepayment': round(prepay, 2),
                'principal': round(principal, 2),
                'defaults': round(defaults, 2),
                'losses': round(loss, 2),
                'recoveries': round(recovery, 2),
                'total_cash_flow': round(interest + principal + recovery, 2),
                'ending_balance': round(ending, 2),
                'smm': round(smm, 6),
            })
            tot['defaults'] += defaults
            tot['losses'] += loss
            tot['recoveries'] += recovery
            tot['scheduled'] += scheduled
            tot['prepay'] += prepay
            tot['interest'] += interest
            bal = ending

        return {
            'cash_flows': rows,
            'total_interest': round(tot['interest'], 2),
            'total_scheduled_principal': round(tot['scheduled'], 2),
            'total_prepayments': round(tot['prepay'], 2),
            'total_defaults': round(tot['defaults'], 2),
            'total_losses': round(tot['losses'], 2),
            'total_recoveries': round(tot['recoveries'], 2),
            'total_investor_cash_flow': round(tot['interest'] + tot['scheduled'] + tot['prepay'] + tot['recoveries'], 2),
            'cumulative_default_rate': round(tot['defaults'] / principal_balance, 6),
            'cumulative_loss_rate': round(tot['losses'] / principal_balance, 6),
            'loss_severity': loss_severity,
            'abs_speed': abs_speed,
            'conventions': {'abs_speed': 'percent of the original pool prepaying per month (ABS convention)',
                            'recoveries': 'received in the month of default and paid to investors'},
        }

    def analyze_credit_enhancement(
        self,
        pool_balance: float = 1000000,
        subordination_pct: float = 0.10,
        reserve_account: float = 0,
        excess_spread: float = 0.02,
        expected_loss: float = 0.03,
    ) -> Dict[str, Any]:
        """Credit enhancement (subordination + reserve) against expected cumulative pool loss."""
        if pool_balance <= 0:
            raise FIError('pool_balance must be > 0')
        if not 0 <= subordination_pct <= 1:
            raise FIError('subordination_pct must be between 0 and 1')
        if reserve_account < 0 or excess_spread < 0 or not 0 <= expected_loss <= 1:
            raise FIError('reserve_account and excess_spread must be >= 0; expected_loss between 0 and 1')
        subordination_amount = pool_balance * subordination_pct
        total_enhancement = subordination_amount + reserve_account
        expected_loss_amount = pool_balance * expected_loss
        coverage_ratio = total_enhancement / expected_loss_amount if expected_loss_amount > 0 else None
        breakeven_loss = (total_enhancement + excess_spread * pool_balance) / pool_balance
        out = {
            'subordination_pct': round(subordination_pct * 100, 2),
            'subordination_amount': round(subordination_amount, 2),
            'reserve_account': round(reserve_account, 2),
            'total_credit_enhancement': round(total_enhancement, 2),
            'total_enhancement_pct': round((total_enhancement / pool_balance) * 100, 2),
            'expected_loss_amount': round(expected_loss_amount, 2),
            'coverage_ratio': round(coverage_ratio, 2) if coverage_ratio is not None else None,
            'breakeven_loss_rate': round(breakeven_loss * 100, 2),
            'breakeven_note': 'enhancement plus one year of excess spread, as % of the pool',
            'excess_spread_annual': round(excess_spread * 100, 2),
            'adequacy': ('No expected loss to cover' if coverage_ratio is None
                         else 'Adequate' if coverage_ratio >= 2 else 'May be insufficient'),
        }
        if coverage_ratio is None:
            out['coverage_ratio_note'] = 'undefined: expected_loss is 0'
        return out


class CMOAnalyzer:
    """Collateralized Mortgage Obligation: sequential-pay structure (optional accrual / Z tranche)."""

    def analyze_sequential_cmo(
        self,
        collateral_balance: float = 1000000,
        tranche_sizes: List[float] = None,
        coupon_rates: List[float] = None,
        wac: float = 0.06,
        wam: int = 360,
        psa_speed: float = 100,
        passthrough_rate: Optional[float] = None,
        z_tranche: bool = False,
        wala: int = 0,
    ) -> Dict[str, Any]:
        """
        Sequential pay: every month each outstanding tranche is paid its coupon interest, then all collateral
        principal retires tranches in order. With z_tranche the last tranche accrues: its coupon is added to its
        balance while earlier tranches are outstanding and that cash pays down the earlier tranches.
        """
        if tranche_sizes is None:
            tranche_sizes = [0.30, 0.30, 0.25, 0.15]
        if coupon_rates is None:
            coupon_rates = [0.04, 0.045, 0.05, 0.055][:len(tranche_sizes)]
        if not tranche_sizes:
            raise FIError('tranche_sizes must be a non-empty list of fractions summing to 1')
        if len(coupon_rates) != len(tranche_sizes):
            raise FIError(f'coupon_rates needs one rate per tranche ({len(tranche_sizes)}), got {len(coupon_rates)}')
        if any(s <= 0 for s in tranche_sizes):
            raise FIError('every tranche size must be > 0')
        if abs(sum(tranche_sizes) - 1.0) > 1e-6:
            raise FIError(f'tranche_sizes must sum to 1 (got {sum(tranche_sizes):g})')
        if any(not 0 <= c < 1 for c in coupon_rates):
            raise FIError('coupon_rates must be decimals between 0 and 1')
        if z_tranche and len(tranche_sizes) < 2:
            raise FIError('a Z tranche needs at least one tranche ahead of it')
        if passthrough_rate is None:
            passthrough_rate = wac

        rows = MBSAnalyzer()._project(collateral_balance, wac, wam, passthrough_rate, None, psa_speed, wala)
        k = len(tranche_sizes)
        tr = [{'name': chr(65 + i) if not (z_tranche and i == k - 1) else 'Z',
               'initial': collateral_balance * tranche_sizes[i], 'bal': collateral_balance * tranche_sizes[i],
               'coupon': coupon_rates[i], 'principal': [], 'interest': 0.0, 'accreted': 0.0,
               'first': None, 'last': None} for i in range(k)]
        residual_total = 0.0
        shortfall_months = 0
        for row in rows:
            month = row['month']
            principal_avail = row['total_principal']
            cash_interest = 0.0
            accrual = 0.0
            for i, t in enumerate(tr):
                if t['bal'] <= 1e-9:
                    continue
                due = t['bal'] * t['coupon'] / 12
                accruing = z_tranche and i == k - 1 and any(x['bal'] > 1e-9 for x in tr[:-1])
                if accruing:
                    t['bal'] += due
                    t['accreted'] += due
                    accrual += due
                else:
                    t['interest'] += due
                    cash_interest += due
            principal_avail += accrual
            residual = row['interest'] - cash_interest - accrual
            residual_total += residual
            if residual < -1e-6:
                shortfall_months += 1
            for t in tr:
                if principal_avail <= 1e-12:
                    break
                if t['bal'] <= 1e-9:
                    continue
                pay = min(principal_avail, t['bal'])
                t['bal'] -= pay
                principal_avail -= pay
                t['principal'].append((month, pay))
                if t['first'] is None:
                    t['first'] = month
                t['last'] = month

        results = []
        for t in tr:
            paid = sum(p for _, p in t['principal'])
            wal = sum(m * p for m, p in t['principal']) / paid / 12 if paid > 0 else None
            results.append({
                'tranche': t['name'],
                'initial_balance': round(t['initial'], 2),
                'coupon': round(t['coupon'] * 100, 4),
                'total_principal': round(paid, 2),
                'total_interest': round(t['interest'], 2),
                'accreted_interest': round(t['accreted'], 2),
                'first_principal_month': t['first'],
                'last_principal_month': t['last'],
                'wal': round(wal, 4) if wal is not None else None,
                'estimated_wal': round(wal, 2) if wal is not None else None,
                'payment_window': f"Month {t['first']} to {t['last']}" if t['first'] else 'N/A',
            })
        out = {
            'structure': 'Sequential Pay CMO' + (' with Z (accrual) tranche' if z_tranche else ''),
            'collateral_balance': collateral_balance,
            'psa_speed': psa_speed,
            'collateral_wal': round(_wal_years(rows), 4),
            'collateral_interest': round(sum(x['interest'] for x in rows), 2),
            'residual_interest': round(residual_total, 2),
            'tranches': results,
            'num_tranches': len(results),
            'conventions': {'wal': 'from each tranche\'s own monthly principal, in years',
                            'collateral_interest_rate': passthrough_rate},
        }
        if shortfall_months:
            out['warnings'] = [f'tranche coupons exceed collateral interest in {shortfall_months} month(s); '
                               'the shortfall is not modelled (residual_interest is negative)']
        return out


ANALYSIS_TYPES = ('mbs_cash_flows', 'wal', 'wal_sensitivity', 'prepayment_schedule', 'abs_cash_flows',
                  'credit_enhancement', 'sequential_cmo', 'cpr_smm_convert')


def _pool(params):
    return dict(
        principal_balance=num(params, 'principal_balance', 1000000),
        wac=num(params, 'wac', 0.06),
        wam=num(params, 'wam', 360, integer=True),
    )


def run_structured_products_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Main entry point for structured products analysis (params['analysis_type'] in ANALYSIS_TYPES)."""
    analysis_type = params.get('analysis_type', 'mbs_cash_flows')

    try:
        if analysis_type == 'mbs_cash_flows':
            return MBSAnalyzer().project_cash_flows(
                **_pool(params),
                passthrough_rate=num(params, 'passthrough_rate', 0.055),
                cpr=num(params, 'cpr', None, allow_none=True),
                psa_speed=num(params, 'psa_speed', 100),
                wala=num(params, 'wala', 0, integer=True),
                max_rows=num(params, 'max_rows', 60, integer=True, lo=0))

        elif analysis_type == 'wal':
            return MBSAnalyzer().calculate_wal(
                **_pool(params),
                cpr=num(params, 'cpr', None, allow_none=True),
                psa_speed=num(params, 'psa_speed', 100),
                wala=num(params, 'wala', 0, integer=True))

        elif analysis_type == 'wal_sensitivity':
            speeds = params.get('psa_speeds')
            return MBSAnalyzer().calculate_wal_sensitivity(
                **_pool(params),
                psa_speeds=fc.num_list(params, 'psa_speeds', min_len=1, lo=0) if speeds is not None else None,
                wala=num(params, 'wala', 0, integer=True))

        elif analysis_type == 'prepayment_schedule':
            return PrepaymentModel().calculate_prepayment_schedule(
                wam=num(params, 'wam', 360, integer=True),
                wala=num(params, 'wala', 0, integer=True),
                psa_speed=num(params, 'psa_speed', 100),
                max_rows=num(params, 'max_rows', 60, integer=True, lo=0))

        elif analysis_type == 'abs_cash_flows':
            return ABSAnalyzer().project_abs_cash_flows(
                principal_balance=num(params, 'principal_balance', 1000000),
                coupon_rate=num(params, 'coupon_rate', 0.05),
                term_months=num(params, 'term_months', 60, integer=True),
                abs_speed=num(params, 'abs_speed', 0.0),
                default_rate=num(params, 'default_rate', 0.02),
                loss_severity=num(params, 'loss_severity', 0.50))

        elif analysis_type == 'credit_enhancement':
            return ABSAnalyzer().analyze_credit_enhancement(
                pool_balance=num(params, 'pool_balance', 1000000),
                subordination_pct=num(params, 'subordination_pct', 0.10),
                reserve_account=num(params, 'reserve_account', 0),
                excess_spread=num(params, 'excess_spread', 0.02),
                expected_loss=num(params, 'expected_loss', 0.03))

        elif analysis_type == 'sequential_cmo':
            sizes = params.get('tranche_sizes')
            coupons = params.get('coupon_rates')
            return CMOAnalyzer().analyze_sequential_cmo(
                collateral_balance=num(params, 'collateral_balance', 1000000, lo=0, lo_open=True),
                tranche_sizes=fc.num_list(params, 'tranche_sizes', min_len=1) if sizes is not None else None,
                coupon_rates=fc.num_list(params, 'coupon_rates', min_len=1) if coupons is not None else None,
                wac=num(params, 'wac', 0.06),
                wam=num(params, 'wam', 360, integer=True),
                psa_speed=num(params, 'psa_speed', 100),
                passthrough_rate=num(params, 'passthrough_rate', None, allow_none=True),
                z_tranche=fc.boolean(params, 'z_tranche', False),
                wala=num(params, 'wala', 0, integer=True))

        elif analysis_type == 'cpr_smm_convert':
            model = PrepaymentModel()
            if params.get('cpr') is not None:
                cpr = num(params, 'cpr')
                return {'cpr': cpr, 'smm': round(model.cpr_to_smm(cpr), 8),
                        'formula': 'SMM = 1 - (1 - CPR)^(1/12)'}
            if params.get('smm') is not None:
                smm = num(params, 'smm')
                return {'smm': smm, 'cpr': round(model.smm_to_cpr(smm), 8),
                        'formula': 'CPR = 1 - (1 - SMM)^12'}
            raise FIError('Provide either cpr or smm (decimal)')

        else:
            return {'error': f'Unknown analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}

    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        logger.error(f"Structured products analysis error: {e}")
        return {'error': str(e) or type(e).__name__}


if __name__ == "__main__":
    fc.import_fincept_json()
    fc.emit_usage_or_run(sys.argv, run_structured_products_analysis, ANALYSIS_TYPES, 'structured_products')
