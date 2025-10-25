"""
Natural Resources Analytics Module
==================================

Comprehensive analysis framework for natural resource investments including commodities, timberland, farmland, raw land, and energy projects. Provides specialized valuation methodologies for biological assets, commodity derivatives, and resource extraction investments.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Commodity spot and futures price data
  - Storage costs and convenience yields for commodities
  - Land acreage and acquisition costs
  - Timber volume and growth rates
  - Energy production data and reserves
  - Operating costs and revenue projections

OUTPUT:
  - Commodity futures basis and roll yield analysis
  - Natural resource valuation using specialized models
  - Total return decomposition for commodities
  - Land investment valuation (timberland, farmland, raw land)
  - Energy project valuation (oil & gas, renewable)

PARAMETERS:
  - commodity_sector: Sector classification (ENERGY, METALS, AGRICULTURE, LIVESTOCK)
  - spot_price: Current spot price of commodity
  - futures_prices: Dictionary of futures contract prices by expiry
  - storage_cost: Annual storage cost percentage - default: Config.COMMODITY_STORAGE_COST_TYPICAL
  - convenience_yield: Convenience yield rate - default: 0
  - contract_size: Contract size multiplier - default: 1
  - land_type: Type of land investment (timberland, farmland, raw_land)
  - acres: Total acreage of land investment
  - energy_type: Energy investment type (oil_gas, renewable)
  - proved_reserves: Reserves or capacity (barrels or MW)
"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config, CommoditySector
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class CommodityAnalyzer(AlternativeInvestmentBase):
    """
    Commodity investment analysis and derivatives
    CFA Standards: Contango/backwardation, roll yield, futures pricing
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.commodity_sector = getattr(parameters, 'commodity_sector', CommoditySector.ENERGY)
        self.spot_price = getattr(parameters, 'spot_price', None)
        self.futures_prices = getattr(parameters, 'futures_prices', {})  # {expiry: price}
        self.storage_cost = getattr(parameters, 'storage_cost', Constants.COMMODITY_STORAGE_COST_TYPICAL)
        self.convenience_yield = getattr(parameters, 'convenience_yield', Decimal('0'))
        self.contract_size = getattr(parameters, 'contract_size', Decimal('1'))

    def calculate_futures_basis(self, futures_price: Decimal, expiry_months: int) -> Dict[str, Decimal]:
        """
        Calculate futures basis and determine market structure
        CFA Standard: Basis = Futures Price - Spot Price
        """
        if not self.spot_price:
            return {"error": "Spot price required"}

        basis = futures_price - self.spot_price
        basis_percentage = basis / self.spot_price

        # Determine market structure
        if basis > 0:
            market_structure = "contango"
        elif basis < 0:
            market_structure = "backwardation"
        else:
            market_structure = "neutral"

        # Annualized basis
        years_to_expiry = Decimal(str(expiry_months)) / Constants.MONTHS_IN_YEAR
        annualized_basis = basis_percentage / years_to_expiry if years_to_expiry > 0 else Decimal('0')

        return {
            "basis": basis,
            "basis_percentage": basis_percentage,
            "annualized_basis": annualized_basis,
            "market_structure": market_structure,
            "years_to_expiry": years_to_expiry
        }

    def theoretical_futures_price(self, time_to_expiry_years: Decimal,
                                  risk_free_rate: Decimal = None) -> Decimal:
        """
        Calculate theoretical futures price
        CFA Standard: F = S * e^((r + storage - convenience) * T)
        """
        if not self.spot_price:
            return Decimal('0')

        if risk_free_rate is None:
            risk_free_rate = Config.RISK_FREE_RATE

        # Net cost of carry
        cost_of_carry = risk_free_rate + self.storage_cost - self.convenience_yield

        # Theoretical futures price
        theoretical_price = self.spot_price * (Decimal('1') + cost_of_carry * time_to_expiry_years)

        return theoretical_price

    def roll_yield_analysis(self, front_month_price: Decimal,
                            next_month_price: Decimal, days_to_roll: int) -> Dict[str, Decimal]:
        """
        Calculate roll yield for commodity futures
        CFA Standard: Roll yield from rolling futures positions
        """
        if days_to_roll <= 0:
            return {"error": "Invalid roll period"}

        # Roll yield calculation
        price_difference = next_month_price - front_month_price
        roll_yield = price_difference / front_month_price

        # Annualized roll yield
        days_in_year = float(Constants.DAYS_IN_YEAR)
        annualized_roll_yield = roll_yield * (Decimal(str(days_in_year)) / Decimal(str(days_to_roll)))

        return {
            "roll_yield": roll_yield,
            "annualized_roll_yield": annualized_roll_yield,
            "price_difference": price_difference,
            "days_to_roll": days_to_roll
        }

    def commodity_total_return(self, spot_returns: List[Decimal],
                               roll_yields: List[Decimal],
                               collateral_returns: List[Decimal]) -> Dict[str, Decimal]:
        """
        Calculate total return components for commodity investment
        CFA Standard: Total Return = Spot Return + Roll Yield + Collateral Return
        """
        if not all([spot_returns, roll_yields, collateral_returns]):
            return {"error": "All return components required"}

        if not (len(spot_returns) == len(roll_yields) == len(collateral_returns)):
            return {"error": "Return series must have equal length"}

        total_returns = []
        spot_component = Decimal('0')
        roll_component = Decimal('0')
        collateral_component = Decimal('0')

        for spot, roll, collateral in zip(spot_returns, roll_yields, collateral_returns):
            total_return = spot + roll + collateral
            total_returns.append(total_return)

            spot_component += spot
            roll_component += roll
            collateral_component += collateral

        periods = len(spot_returns)

        return {
            "total_return": sum(total_returns),
            "average_total_return": sum(total_returns) / periods,
            "spot_contribution": spot_component / periods,
            "roll_contribution": roll_component / periods,
            "collateral_contribution": collateral_component / periods,
            "periods": periods
        }

    def volatility_analysis(self, price_history: List[Decimal]) -> Dict[str, Decimal]:
        """Analyze commodity price volatility"""
        if len(price_history) < 2:
            return {"error": "Insufficient price history"}

        # Calculate returns
        returns = []
        for i in range(1, len(price_history)):
            ret = (price_history[i] - price_history[i - 1]) / price_history[i - 1]
            returns.append(ret)

        # Calculate volatility metrics
        mean_return = sum(returns) / len(returns)
        variance = sum((r - mean_return) ** 2 for r in returns) / (len(returns) - 1)
        volatility = variance.sqrt()

        # Annualized volatility (assuming daily data)
        annualized_vol = volatility * Constants.BUSINESS_DAYS_IN_YEAR.sqrt()

        return {
            "daily_volatility": volatility,
            "annualized_volatility": annualized_vol,
            "mean_daily_return": mean_return,
            "number_of_observations": len(returns)
        }

    def calculate_nav(self) -> Decimal:
        """Calculate commodity position NAV"""
        if self.spot_price:
            return self.spot_price * self.contract_size

        latest_price = self.get_latest_price()
        if latest_price:
            return latest_price * self.contract_size

        return Decimal('0')

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key commodity metrics"""
        metrics = {}

        # Current pricing
        if self.spot_price:
            metrics['spot_price'] = float(self.spot_price)

        # Futures curve analysis
        if self.futures_prices:
            sorted_futures = sorted(self.futures_prices.items())
            metrics['futures_curve'] = {str(exp): float(price) for exp, price in sorted_futures}

            # Front month analysis
            if len(sorted_futures) >= 2:
                front_exp, front_price = sorted_futures[0]
                next_exp, next_price = sorted_futures[1]

                basis_analysis = self.calculate_futures_basis(front_price, 1)  # 1 month
                metrics.update(basis_analysis)

                roll_analysis = self.roll_yield_analysis(front_price, next_price, 30)  # 30 days
                if 'error' not in roll_analysis:
                    metrics.update(roll_analysis)

        # Volatility analysis
        prices = [md.price for md in self.market_data]
        if prices:
            vol_analysis = self.volatility_analysis(prices)
            if 'error' not in vol_analysis:
                metrics.update(vol_analysis)

        metrics['commodity_sector'] = self.commodity_sector.value
        metrics['storage_cost'] = float(self.storage_cost)
        metrics['convenience_yield'] = float(self.convenience_yield)

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive commodity valuation summary"""
        return {
            "commodity_overview": {
                "sector": self.commodity_sector.value,
                "spot_price": float(self.spot_price) if self.spot_price else None,
                "contract_size": float(self.contract_size),
                "storage_cost": float(self.storage_cost),
                "convenience_yield": float(self.convenience_yield)
            },
            "market_analysis": self.calculate_key_metrics(),
            "futures_curve": self.futures_prices
        }


class LandInvestmentAnalyzer(AlternativeInvestmentBase):
    """
    Land investment analysis - Timberland, Farmland, Raw Land
    CFA Standards: Natural resource valuation methods
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.land_type = getattr(parameters, 'land_type', 'timberland')  # timberland, farmland, raw_land
        self.acres = getattr(parameters, 'acres', None)
        self.acquisition_price_per_acre = getattr(parameters, 'acquisition_price_per_acre', None)
        self.annual_revenue_per_acre = getattr(parameters, 'annual_revenue_per_acre', None)
        self.operating_expenses_per_acre = getattr(parameters, 'operating_expenses_per_acre', None)
        self.appreciation_rate = getattr(parameters, 'appreciation_rate', Decimal('0.03'))  # 3% annual

        # Timberland specific
        self.timber_volume = getattr(parameters, 'timber_volume', None)  # board feet or cubic meters
        self.growth_rate = getattr(parameters, 'growth_rate', Decimal('0.04'))  # 4% annual volume growth
        self.harvest_cycle = getattr(parameters, 'harvest_cycle', 25)  # years

        # Farmland specific
        self.crop_yield = getattr(parameters, 'crop_yield', None)  # bushels per acre
        self.commodity_price = getattr(parameters, 'commodity_price', None)  # price per bushel

    def timberland_valuation(self) -> Dict[str, Decimal]:
        """
        Timberland valuation using biological asset model
        CFA Standard: DCF with biological growth consideration
        """
        if self.land_type != 'timberland' or not all([self.timber_volume, self.acres]):
            return {"error": "Timberland parameters required"}

        discount_rate = Config.RISK_FREE_RATE + Decimal('0.04')  # 4% risk premium

        # Land value (separate from timber)
        land_value = Decimal('0')
        if self.acquisition_price_per_acre:
            land_value = self.acquisition_price_per_acre * self.acres

        # Timber value with biological growth
        current_timber_value = self.timber_volume * self.acres

        # Project timber growth and harvest cycles
        total_timber_value = Decimal('0')
        years_to_project = 50  # Long-term projection

        for cycle in range(1, years_to_project // self.harvest_cycle + 1):
            harvest_year = cycle * self.harvest_cycle

            # Timber volume at harvest (with growth)
            harvest_volume = current_timber_value * ((Decimal('1') + self.growth_rate) ** harvest_year)

            # Assumed timber price per unit (simplified)
            timber_price_per_unit = Decimal('100')  # per unit volume
            harvest_revenue = harvest_volume * timber_price_per_unit

            # Present value of harvest
            pv_harvest = harvest_revenue / ((Decimal('1') + discount_rate) ** harvest_year)
            total_timber_value += pv_harvest

        # Annual income from other sources (hunting leases, etc.)
        annual_income = (self.annual_revenue_per_acre or Decimal('0')) * self.acres
        annual_expenses = (self.operating_expenses_per_acre or Decimal('0')) * self.acres
        net_annual_income = annual_income - annual_expenses

        # PV of annual income stream
        if net_annual_income > 0:
            pv_annual_income = net_annual_income / discount_rate  # Perpetuity
        else:
            pv_annual_income = Decimal('0')

        total_value = land_value + total_timber_value + pv_annual_income

        return {
            "total_timberland_value": total_value,
            "land_value": land_value,
            "timber_value": total_timber_value,
            "annual_income_value": pv_annual_income,
            "value_per_acre": total_value / self.acres if self.acres > 0 else Decimal('0')
        }

    def farmland_valuation(self) -> Dict[str, Decimal]:
        """
        Farmland valuation based on productive capacity
        CFA Standard: Income approach for agricultural land
        """
        if self.land_type != 'farmland' or not self.acres:
            return {"error": "Farmland parameters required"}

        # Income approach
        annual_gross_income = Decimal('0')
        if self.crop_yield and self.commodity_price:
            annual_gross_income = self.crop_yield * self.commodity_price * self.acres
        elif self.annual_revenue_per_acre:
            annual_gross_income = self.annual_revenue_per_acre * self.acres

        annual_expenses = (self.operating_expenses_per_acre or Decimal('0')) * self.acres
        net_operating_income = annual_gross_income - annual_expenses

        # Capitalization rate for farmland (typically lower than commercial real estate)
        cap_rate = Decimal('0.05')  # 5% cap rate

        income_value = net_operating_income / cap_rate if cap_rate > 0 else Decimal('0')

        # Market approach (if acquisition price available)
        market_value = Decimal('0')
        if self.acquisition_price_per_acre:
            # Appreciate at assumed rate
            years_held = 1  # Simplified - would need actual holding period
            appreciated_price = self.acquisition_price_per_acre * (
                        (Decimal('1') + self.appreciation_rate) ** years_held)
            market_value = appreciated_price * self.acres

        # Use higher of income or market value
        farmland_value = max(income_value, market_value)

        return {
            "farmland_value": farmland_value,
            "income_value": income_value,
            "market_value": market_value,
            "annual_noi": net_operating_income,
            "value_per_acre": farmland_value / self.acres if self.acres > 0 else Decimal('0'),
            "cap_rate": cap_rate
        }

    def raw_land_valuation(self) -> Dict[str, Decimal]:
        """
        Raw land valuation for development potential
        """
        if self.land_type != 'raw_land' or not self.acres:
            return {"error": "Raw land parameters required"}

        # Simple appreciation model
        current_value_per_acre = self.acquisition_price_per_acre or Decimal('1000')  # Default $1000/acre

        # Project future value based on appreciation
        projection_years = 10
        future_value_per_acre = current_value_per_acre * ((Decimal('1') + self.appreciation_rate) ** projection_years)

        # Discount back to present (accounting for lack of income)
        discount_rate = Config.RISK_FREE_RATE + Decimal('0.06')  # Higher risk premium for raw land
        present_value_per_acre = future_value_per_acre / ((Decimal('1') + discount_rate) ** projection_years)

        total_value = present_value_per_acre * self.acres

        return {
            "raw_land_value": total_value,
            "current_value_per_acre": current_value_per_acre,
            "projected_value_per_acre": future_value_per_acre,
            "present_value_per_acre": present_value_per_acre,
            "total_acres": self.acres
        }

    def calculate_nav(self) -> Decimal:
        """Calculate land investment NAV"""
        if self.land_type == 'timberland':
            valuation = self.timberland_valuation()
            return valuation.get('total_timberland_value', Decimal('0'))
        elif self.land_type == 'farmland':
            valuation = self.farmland_valuation()
            return valuation.get('farmland_value', Decimal('0'))
        elif self.land_type == 'raw_land':
            valuation = self.raw_land_valuation()
            return valuation.get('raw_land_value', Decimal('0'))

        return Decimal('0')

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key land investment metrics"""
        metrics = {
            "land_type": self.land_type,
            "total_acres": float(self.acres) if self.acres else None,
            "appreciation_rate": float(self.appreciation_rate)
        }

        # Type-specific valuations
        if self.land_type == 'timberland':
            valuation = self.timberland_valuation()
        elif self.land_type == 'farmland':
            valuation = self.farmland_valuation()
        elif self.land_type == 'raw_land':
            valuation = self.raw_land_valuation()
        else:
            valuation = {}

        # Convert Decimal values to float
        for key, value in valuation.items():
            if isinstance(value, Decimal):
                metrics[key] = float(value)
            else:
                metrics[key] = value

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive land investment summary"""
        return {
            "land_overview": {
                "land_type": self.land_type,
                "total_acres": float(self.acres) if self.acres else None,
                "acquisition_price_per_acre": float(
                    self.acquisition_price_per_acre) if self.acquisition_price_per_acre else None
            },
            "valuation_analysis": self.calculate_key_metrics()
        }


class EnergyInvestmentAnalyzer(AlternativeInvestmentBase):
    """
    Energy investment analysis - Oil & Gas, Renewables
    CFA Standards: Energy project finance and valuation
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.energy_type = getattr(parameters, 'energy_type', 'oil_gas')  # oil_gas, renewable
        self.proved_reserves = getattr(parameters, 'proved_reserves', None)  # barrels or MW capacity
        self.daily_production = getattr(parameters, 'daily_production', None)
        self.decline_rate = getattr(parameters, 'decline_rate', Decimal('0.15'))  # 15% annual decline
        self.operating_cost_per_unit = getattr(parameters, 'operating_cost_per_unit', None)
        self.commodity_price = getattr(parameters, 'commodity_price', None)  # $/barrel or $/MWh

        # Renewable specific
        self.capacity_factor = getattr(parameters, 'capacity_factor', Decimal('0.35'))  # 35%
        self.power_purchase_agreement = getattr(parameters, 'ppa_price', None)  # $/MWh
        self.asset_life = getattr(parameters, 'asset_life', 25)  # years

    def oil_gas_valuation(self) -> Dict[str, Decimal]:
        """
        Oil & Gas asset valuation using decline curve analysis
        CFA Standard: DCF with production decline
        """
        if self.energy_type != 'oil_gas':
            return {"error": "Oil & Gas parameters required"}

        if not all([self.daily_production, self.commodity_price]):
            return {"error": "Production and price data required"}

        discount_rate = Config.RISK_FREE_RATE + Decimal('0.08')  # 8% risk premium
        projection_years = 20

        total_pv = Decimal('0')
        daily_prod = self.daily_production
        opex_per_unit = self.operating_cost_per_unit or Decimal('0')

        for year in range(1, projection_years + 1):
            # Annual production (accounting for decline)
            annual_production = daily_prod * Constants.DAYS_IN_YEAR

            # Revenue and costs
            annual_revenue = annual_production * self.commodity_price
            annual_opex = annual_production * opex_per_unit

            # Cash flow before taxes
            annual_cash_flow = annual_revenue - annual_opex

            # Present value
            pv = annual_cash_flow / ((Decimal('1') + discount_rate) ** year)
            total_pv += pv

            # Apply decline rate for next year
            daily_prod *= (Decimal('1') - self.decline_rate)

        return {
            "oil_gas_value": total_pv,
            "initial_daily_production": self.daily_production,
            "commodity_price": self.commodity_price,
            "decline_rate": self.decline_rate,
            "projection_years": projection_years
        }

    def renewable_energy_valuation(self) -> Dict[str, Decimal]:
        """
        Renewable energy project valuation
        CFA Standard: Project finance DCF
        """
        if self.energy_type != 'renewable':
            return {"error": "Renewable energy parameters required"}

        if not all([self.proved_reserves, self.capacity_factor]):  # proved_reserves = capacity in MW
            return {"error": "Capacity and capacity factor required"}

        discount_rate = Config.RISK_FREE_RATE + Decimal('0.05')  # 5% risk premium (lower than O&G)

        # Annual energy production
        capacity_mw = self.proved_reserves
        hours_per_year = Decimal('8760')  # 24 * 365
        annual_mwh = capacity_mw * hours_per_year * self.capacity_factor

        # Revenue per year
        ppa_price = self.power_purchase_agreement or Decimal('50')  # $50/MWh default
        annual_revenue = annual_mwh * ppa_price

        # Operating costs (simplified)
        annual_opex = capacity_mw * Decimal('25000')  # $25,000 per MW per year

        # Annual cash flow
        annual_cash_flow = annual_revenue - annual_opex

        # Present value of cash flows
        total_pv = Decimal('0')
        for year in range(1, self.asset_life + 1):
            pv = annual_cash_flow / ((Decimal('1') + discount_rate) ** year)
            total_pv += pv

        return {
            "renewable_value": total_pv,
            "capacity_mw": capacity_mw,
            "annual_mwh": annual_mwh,
            "annual_revenue": annual_revenue,
            "annual_cash_flow": annual_cash_flow,
            "asset_life": self.asset_life
        }

    def calculate_nav(self) -> Decimal:
        """Calculate energy investment NAV"""
        if self.energy_type == 'oil_gas':
            valuation = self.oil_gas_valuation()
            return valuation.get('oil_gas_value', Decimal('0'))
        elif self.energy_type == 'renewable':
            valuation = self.renewable_energy_valuation()
            return valuation.get('renewable_value', Decimal('0'))

        return Decimal('0')

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate key energy investment metrics"""
        metrics = {
            "energy_type": self.energy_type
        }

        if self.energy_type == 'oil_gas':
            valuation = self.oil_gas_valuation()
            metrics.update({k: float(v) if isinstance(v, Decimal) else v for k, v in valuation.items()})
        elif self.energy_type == 'renewable':
            valuation = self.renewable_energy_valuation()
            metrics.update({k: float(v) if isinstance(v, Decimal) else v for k, v in valuation.items()})

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive energy investment summary"""
        return {
            "energy_overview": {
                "energy_type": self.energy_type,
                "proved_reserves": float(self.proved_reserves) if self.proved_reserves else None,
                "daily_production": float(self.daily_production) if self.daily_production else None
            },
            "valuation_analysis": self.calculate_key_metrics()
        }


# Export main components
__all__ = ['CommodityAnalyzer', 'LandInvestmentAnalyzer', 'EnergyInvestmentAnalyzer']