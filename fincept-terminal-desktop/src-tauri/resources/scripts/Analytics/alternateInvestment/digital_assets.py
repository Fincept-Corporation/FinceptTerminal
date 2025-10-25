"""
Digital Assets Analytics Module
===============================

Comprehensive analysis framework for digital assets including cryptocurrencies, blockchain tokens, DeFi protocols, and NFTs. Provides valuation methodologies, risk assessment, and portfolio integration analysis for digital asset investments.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Market price data for digital assets (historical prices, volume)
  - Market capitalization and circulating supply data
  - Protocol revenue and metrics for DeFi tokens
  - Staking yields and reward rates for PoS tokens
  - Blockchain network metrics and transaction data

OUTPUT:
  - Digital asset valuation metrics and price analysis
  - Volatility and risk assessment reports
  - Correlation analysis with traditional assets
  - Portfolio integration impact analysis
  - DeFi protocol fundamentals evaluation

PARAMETERS:
  - asset_type: Type of digital asset (cryptocurrency, defi_token, nft, stablecoin)
  - blockchain: Blockchain network (bitcoin, ethereum, etc.) - default: bitcoin
  - market_cap: Current market capitalization
  - circulating_supply: Current circulating supply
  - total_supply: Maximum or total supply
  - trading_volume_24h: 24-hour trading volume
  - staking_yield: Annual staking yield for PoS tokens
  - protocol_revenue: Annual protocol revenue for DeFi tokens
"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class DigitalAssetAnalyzer(AlternativeInvestmentBase):
    """
    Digital Asset investment analysis and valuation
    CFA Standards: Risk assessment, correlation analysis, portfolio integration
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.asset_type = getattr(parameters, 'asset_type',
                                  'cryptocurrency')  # cryptocurrency, defi_token, nft, stablecoin
        self.blockchain = getattr(parameters, 'blockchain', 'bitcoin')
        self.market_cap = getattr(parameters, 'market_cap', None)
        self.circulating_supply = getattr(parameters, 'circulating_supply', None)
        self.total_supply = getattr(parameters, 'total_supply', None)
        self.trading_volume_24h = getattr(parameters, 'trading_volume_24h', None)
        self.staking_yield = getattr(parameters, 'staking_yield', None)  # For PoS tokens
        self.protocol_revenue = getattr(parameters, 'protocol_revenue', None)  # For DeFi tokens

    def fundamental_metrics(self) -> Dict[str, Any]:
        """
        Calculate fundamental valuation metrics for digital assets
        CFA Standard: Fundamental analysis adaptation for digital assets
        """
        metrics = {}

        # Basic metrics
        if self.market_cap and self.circulating_supply:
            price_per_token = self.market_cap / self.circulating_supply
            metrics['price_per_token'] = float(price_per_token)
            metrics['market_cap'] = float(self.market_cap)
            metrics['circulating_supply'] = float(self.circulating_supply)

        # Supply metrics
        if self.total_supply and self.circulating_supply:
            inflation_rate = (self.total_supply - self.circulating_supply) / self.circulating_supply
            metrics['potential_inflation'] = float(inflation_rate)

        # Liquidity metrics
        if self.trading_volume_24h and self.market_cap:
            volume_to_mcap = self.trading_volume_24h / self.market_cap
            metrics['volume_to_market_cap'] = float(volume_to_mcap)

            # Liquidity classification
            if volume_to_mcap > Decimal('0.1'):
                liquidity_tier = "high"
            elif volume_to_mcap > Decimal('0.01'):
                liquidity_tier = "medium"
            else:
                liquidity_tier = "low"
            metrics['liquidity_tier'] = liquidity_tier

        # Yield metrics for staking tokens
        if self.staking_yield:
            metrics['staking_yield_annual'] = float(self.staking_yield)

            # Risk-adjusted staking yield
            crypto_vol = self.config.CRYPTO_VOLATILITY_FLOOR
            if self.market_data:
                returns = self.calculate_simple_returns()
                if returns:
                    actual_vol = self.calculate_volatility(returns)
                    crypto_vol = max(actual_vol, crypto_vol)

            risk_adjusted_yield = self.staking_yield / crypto_vol
            metrics['risk_adjusted_staking_yield'] = float(risk_adjusted_yield)

        # Protocol metrics for DeFi tokens
        if self.protocol_revenue and self.market_cap:
            revenue_multiple = self.market_cap / self.protocol_revenue
            metrics['price_to_protocol_revenue'] = float(revenue_multiple)

        return metrics

    def volatility_analysis(self) -> Dict[str, Any]:
        """
        Comprehensive volatility analysis for digital assets
        CFA Standard: Risk measurement adapted for high volatility assets
        """
        returns = self.calculate_simple_returns()

        if not returns:
            return {"error": "Insufficient price data for volatility analysis"}

        analysis = {}

        # Basic volatility metrics
        daily_vol = self.calculate_volatility(returns, annualized=False)
        annualized_vol = daily_vol * Constants.DAYS_IN_YEAR.sqrt()

        analysis['daily_volatility'] = float(daily_vol)
        analysis['annualized_volatility'] = float(annualized_vol)

        # Volatility percentiles
        return_magnitudes = [abs(r) for r in returns]
        sorted_magnitudes = sorted(return_magnitudes)

        if len(sorted_magnitudes) >= 10:
            p90_vol = sorted_magnitudes[int(0.9 * len(sorted_magnitudes))]
            p95_vol = sorted_magnitudes[int(0.95 * len(sorted_magnitudes))]
            p99_vol = sorted_magnitudes[int(0.99 * len(sorted_magnitudes))]

            analysis['90th_percentile_move'] = float(p90_vol)
            analysis['95th_percentile_move'] = float(p95_vol)
            analysis['99th_percentile_move'] = float(p99_vol)

        # Volatility clustering analysis
        vol_clustering = self._detect_volatility_clustering(returns)
        analysis['volatility_clustering'] = vol_clustering

        # Compare to traditional assets
        traditional_equity_vol = Decimal('0.16')  # 16% typical equity volatility
        vol_multiple = annualized_vol / traditional_equity_vol
        analysis['volatility_vs_equity'] = float(vol_multiple)

        return analysis

    def _detect_volatility_clustering(self, returns: List[Decimal]) -> Dict[str, Any]:
        """Detect volatility clustering patterns"""
        if len(returns) < 20:
            return {"insufficient_data": True}

        # Calculate rolling volatility
        window = min(10, len(returns) // 4)
        rolling_vols = []

        for i in range(window, len(returns)):
            window_returns = returns[i - window:i]
            window_vol = self.calculate_volatility(window_returns, annualized=False)
            rolling_vols.append(window_vol)

        if len(rolling_vols) < 2:
            return {"insufficient_data": True}

        # Measure persistence in volatility
        vol_changes = []
        for i in range(1, len(rolling_vols)):
            vol_change = (rolling_vols[i] - rolling_vols[i - 1]) / rolling_vols[i - 1]
            vol_changes.append(vol_change)

        # High volatility tends to be followed by high volatility
        clustering_score = len([v for v in vol_changes if abs(v) < Decimal('0.1')]) / len(vol_changes)

        return {
            "clustering_score": float(clustering_score),
            "interpretation": "High" if clustering_score > 0.6 else "Medium" if clustering_score > 0.4 else "Low"
        }

    def correlation_analysis(self, benchmark_returns: List[Decimal],
                             traditional_assets: Dict[str, List[Decimal]] = None) -> Dict[str, Any]:
        """
        Analyze correlations with traditional assets and crypto market
        CFA Standard: Diversification benefit analysis
        """
        crypto_returns = self.calculate_simple_returns()

        if not crypto_returns or not benchmark_returns:
            return {"error": "Insufficient return data"}

        if len(crypto_returns) != len(benchmark_returns):
            return {"error": "Return series length mismatch"}

        analysis = {}

        # Correlation with crypto market benchmark
        crypto_correlation = self._calculate_correlation(crypto_returns, benchmark_returns)
        analysis['crypto_market_correlation'] = float(crypto_correlation)

        # Beta relative to crypto market
        crypto_beta = self._calculate_beta(crypto_returns, benchmark_returns)
        analysis['crypto_market_beta'] = float(crypto_beta)

        # Correlation with traditional assets
        if traditional_assets:
            traditional_correlations = {}

            for asset_name, asset_returns in traditional_assets.items():
                if len(asset_returns) == len(crypto_returns):
                    correlation = self._calculate_correlation(crypto_returns, asset_returns)
                    traditional_correlations[asset_name] = float(correlation)

            analysis['traditional_asset_correlations'] = traditional_correlations

            # Average correlation with traditional assets
            if traditional_correlations:
                avg_traditional_corr = sum(traditional_correlations.values()) / len(traditional_correlations)
                analysis['average_traditional_correlation'] = avg_traditional_corr

                # Diversification benefit assessment
                if avg_traditional_corr < 0.3:
                    diversification_benefit = "High"
                elif avg_traditional_corr < 0.6:
                    diversification_benefit = "Medium"
                else:
                    diversification_benefit = "Low"

                analysis['diversification_benefit'] = diversification_benefit

        return analysis

    def _calculate_correlation(self, returns1: List[Decimal], returns2: List[Decimal]) -> Decimal:
        """Calculate correlation coefficient between two return series"""
        if len(returns1) != len(returns2) or len(returns1) < 2:
            return Decimal('0')

        mean1 = sum(returns1) / len(returns1)
        mean2 = sum(returns2) / len(returns2)

        numerator = sum((r1 - mean1) * (r2 - mean2) for r1, r2 in zip(returns1, returns2))

        sum_sq1 = sum((r1 - mean1) ** 2 for r1 in returns1)
        sum_sq2 = sum((r2 - mean2) ** 2 for r2 in returns2)

        denominator = (sum_sq1 * sum_sq2).sqrt()

        if denominator == 0:
            return Decimal('0')

        return numerator / denominator

    def _calculate_beta(self, asset_returns: List[Decimal], market_returns: List[Decimal]) -> Decimal:
        """Calculate beta relative to market"""
        if len(asset_returns) != len(market_returns) or len(asset_returns) < 2:
            return Decimal('1')

        asset_mean = sum(asset_returns) / len(asset_returns)
        market_mean = sum(market_returns) / len(market_returns)

        covariance = sum((a - asset_mean) * (m - market_mean)
                         for a, m in zip(asset_returns, market_returns)) / (len(asset_returns) - 1)

        market_variance = sum((m - market_mean) ** 2 for m in market_returns) / (len(market_returns) - 1)

        if market_variance == 0:
            return Decimal('1')

        return Decimal(str(covariance)) / Decimal(str(market_variance))

    def defi_protocol_analysis(self) -> Dict[str, Any]:
        """
        Analyze DeFi protocol fundamentals
        CFA Standard: Business model analysis for DeFi protocols
        """
        if self.asset_type != 'defi_token':
            return {"not_applicable": "Analysis specific to DeFi tokens"}

        analysis = {}

        # Protocol revenue analysis
        if self.protocol_revenue and self.market_cap:
            # Price-to-Revenue ratio
            p_revenue = self.market_cap / self.protocol_revenue
            analysis['price_to_revenue'] = float(p_revenue)

            # Revenue yield
            revenue_yield = self.protocol_revenue / self.market_cap
            analysis['revenue_yield'] = float(revenue_yield)

        # Token utility assessment
        utility_factors = {
            'governance_rights': True,  # Assumed for most DeFi tokens
            'fee_discounts': None,  # Would need specific protocol data
            'staking_rewards': self.staking_yield is not None,
            'protocol_fees_capture': self.protocol_revenue is not None
        }

        analysis['token_utility'] = utility_factors

        # Utility score (simplified)
        utility_score = sum(1 for v in utility_factors.values() if v is True)
        analysis['utility_score'] = utility_score
        analysis['max_utility_score'] = len(utility_factors)

        return analysis

    def risk_assessment(self) -> Dict[str, Any]:
        """
        Comprehensive risk assessment for digital assets
        CFA Standard: Risk identification and measurement
        """
        risk_assessment = {}

        # Technology risk
        risk_assessment['technology_risk'] = {
            'smart_contract_risk': self.asset_type in ['defi_token'],
            'blockchain_risk': self.blockchain,
            'upgrade_risk': True  # Most protocols have upgrade mechanisms
        }

        # Regulatory risk
        risk_assessment['regulatory_risk'] = {
            'classification_uncertainty': True,  # Ongoing regulatory developments
            'geographic_restrictions': 'varies_by_jurisdiction',
            'compliance_requirements': 'evolving'
        }

        # Market risk
        returns = self.calculate_simple_returns()
        if returns:
            vol_analysis = self.volatility_analysis()

            # Risk tier based on volatility
            if 'annualized_volatility' in vol_analysis:
                annual_vol = vol_analysis['annualized_volatility']
                if annual_vol > 1.0:  # >100%
                    risk_tier = "Very High"
                elif annual_vol > 0.5:  # >50%
                    risk_tier = "High"
                elif annual_vol > 0.3:  # >30%
                    risk_tier = "Medium-High"
                else:
                    risk_tier = "Medium"

                risk_assessment['market_risk_tier'] = risk_tier

        # Liquidity risk
        if self.trading_volume_24h and self.market_cap:
            volume_ratio = self.trading_volume_24h / self.market_cap

            if volume_ratio < Decimal('0.001'):  # <0.1%
                liquidity_risk = "High"
            elif volume_ratio < Decimal('0.01'):  # <1%
                liquidity_risk = "Medium"
            else:
                liquidity_risk = "Low"

            risk_assessment['liquidity_risk'] = liquidity_risk

        # Concentration risk
        if self.circulating_supply and self.total_supply:
            supply_concentration = (self.total_supply - self.circulating_supply) / self.total_supply
            risk_assessment['supply_concentration_risk'] = float(supply_concentration)

        return risk_assessment

    def portfolio_integration_analysis(self, portfolio_returns: List[Decimal],
                                       target_allocation: Decimal = Decimal('0.05')) -> Dict[str, Any]:
        """
        Analyze impact of adding digital asset to traditional portfolio
        CFA Standard: Portfolio optimization with alternative assets
        """
        crypto_returns = self.calculate_simple_returns()

        if not crypto_returns or not portfolio_returns:
            return {"error": "Insufficient return data"}

        if len(crypto_returns) != len(portfolio_returns):
            return {"error": "Return series length mismatch"}

        integration_analysis = {}

        # Correlation with existing portfolio
        portfolio_correlation = self._calculate_correlation(crypto_returns, portfolio_returns)
        integration_analysis['portfolio_correlation'] = float(portfolio_correlation)

        # Expected portfolio metrics with crypto allocation
        crypto_weight = target_allocation
        portfolio_weight = Decimal('1') - crypto_weight

        # Expected returns
        crypto_mean = sum(crypto_returns) / len(crypto_returns)
        portfolio_mean = sum(portfolio_returns) / len(portfolio_returns)

        combined_expected_return = (crypto_weight * crypto_mean +
                                    portfolio_weight * portfolio_mean)
        integration_analysis['expected_return_with_crypto'] = float(combined_expected_return)

        # Expected volatility
        crypto_vol = self.calculate_volatility(crypto_returns, annualized=False)
        portfolio_vol = self._calculate_volatility(portfolio_returns)

        # Portfolio variance with crypto
        combined_variance = (
                (crypto_weight ** 2) * (crypto_vol ** 2) +
                (portfolio_weight ** 2) * (portfolio_vol ** 2) +
                2 * crypto_weight * portfolio_weight * portfolio_correlation * crypto_vol * portfolio_vol
        )

        combined_volatility = combined_variance.sqrt()
        integration_analysis['expected_volatility_with_crypto'] = float(combined_volatility)

        # Sharpe ratio comparison
        risk_free_rate = Config.RISK_FREE_RATE / Constants.MONTHS_IN_YEAR

        original_sharpe = (portfolio_mean - risk_free_rate) / portfolio_vol if portfolio_vol > 0 else Decimal('0')
        combined_sharpe = (
                                      combined_expected_return - risk_free_rate) / combined_volatility if combined_volatility > 0 else Decimal(
            '0')

        integration_analysis['original_sharpe_ratio'] = float(original_sharpe)
        integration_analysis['combined_sharpe_ratio'] = float(combined_sharpe)
        integration_analysis['sharpe_improvement'] = float(combined_sharpe - original_sharpe)

        # Diversification benefit
        diversification_ratio = combined_volatility / (crypto_weight * crypto_vol + portfolio_weight * portfolio_vol)
        integration_analysis['diversification_ratio'] = float(diversification_ratio)

        return integration_analysis

    def _calculate_volatility(self, returns: List[Decimal]) -> Decimal:
        """Calculate volatility of returns"""
        if len(returns) < 2:
            return Decimal('0')

        mean_return = sum(returns) / len(returns)
        variance = sum((r - mean_return) ** 2 for r in returns) / (len(returns) - 1)
        return variance.sqrt()

    def calculate_nav(self) -> Decimal:
        """Calculate digital asset NAV"""
        latest_price = self.get_latest_price()
        if latest_price:
            return latest_price

        # Fallback to market cap per token if available
        if self.market_cap and self.circulating_supply:
            return self.market_cap / self.circulating_supply

        return Decimal('0')

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate comprehensive digital asset metrics"""
        metrics = {}

        # Fundamental metrics
        fundamental_metrics = self.fundamental_metrics()
        metrics.update(fundamental_metrics)

        # Volatility analysis
        vol_analysis = self.volatility_analysis()
        if 'error' not in vol_analysis:
            metrics.update(vol_analysis)

        # Risk assessment
        risk_metrics = self.risk_assessment()
        metrics.update(risk_metrics)

        # DeFi-specific analysis
        if self.asset_type == 'defi_token':
            defi_metrics = self.defi_protocol_analysis()
            if 'not_applicable' not in defi_metrics:
                metrics.update(defi_metrics)

        # Basic identifiers
        metrics['asset_type'] = self.asset_type
        metrics['blockchain'] = self.blockchain

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive digital asset valuation summary"""
        return {
            "asset_overview": {
                "asset_type": self.asset_type,
                "blockchain": self.blockchain,
                "market_cap": float(self.market_cap) if self.market_cap else None,
                "circulating_supply": float(self.circulating_supply) if self.circulating_supply else None,
                "trading_volume_24h": float(self.trading_volume_24h) if self.trading_volume_24h else None
            },
            "fundamental_analysis": self.fundamental_metrics(),
            "risk_analysis": self.risk_assessment(),
            "performance_metrics": self.calculate_key_metrics()
        }


class DigitalAssetPortfolio:
    """
    Portfolio-level digital asset analysis
    CFA Standards: Portfolio construction and risk management
    """

    def __init__(self):
        self.digital_assets: List[DigitalAssetAnalyzer] = []

    def add_digital_asset(self, asset: DigitalAssetAnalyzer) -> None:
        """Add digital asset to portfolio"""
        self.digital_assets.append(asset)

    def portfolio_diversification(self) -> Dict[str, Any]:
        """Analyze portfolio diversification across digital asset types"""
        type_allocation = {}
        blockchain_allocation = {}
        total_nav = Decimal('0')

        for asset in self.digital_assets:
            asset_type = asset.asset_type
            blockchain = asset.blockchain
            nav = asset.calculate_nav()

            # Asset type allocation
            if asset_type not in type_allocation:
                type_allocation[asset_type] = {'count': 0, 'total_nav': Decimal('0')}
            type_allocation[asset_type]['count'] += 1
            type_allocation[asset_type]['total_nav'] += nav

            # Blockchain allocation
            if blockchain not in blockchain_allocation:
                blockchain_allocation[blockchain] = {'count': 0, 'total_nav': Decimal('0')}
            blockchain_allocation[blockchain]['count'] += 1
            blockchain_allocation[blockchain]['total_nav'] += nav

            total_nav += nav

        # Convert to percentages
        for allocation_dict in [type_allocation, blockchain_allocation]:
            for key in allocation_dict:
                allocation = allocation_dict[key]
                allocation['weight'] = float(allocation['total_nav'] / total_nav) if total_nav > 0 else 0
                allocation['total_nav'] = float(allocation['total_nav'])

        return {
            "asset_type_allocation": type_allocation,
            "blockchain_allocation": blockchain_allocation,
            "total_portfolio_nav": float(total_nav),
            "number_of_assets": len(self.digital_assets)
        }

    def portfolio_risk_metrics(self) -> Dict[str, Any]:
        """Calculate portfolio-level risk metrics"""
        all_returns = []
        weights = []
        total_nav = sum(asset.calculate_nav() for asset in self.digital_assets)

        # Collect returns and calculate weights
        for asset in self.digital_assets:
            asset_returns = asset.calculate_simple_returns()
            if asset_returns:
                all_returns.append(asset_returns)
                weight = asset.calculate_nav() / total_nav if total_nav > 0 else Decimal('0')
                weights.append(weight)

        if not all_returns:
            return {"error": "No return data available"}

        # Calculate portfolio returns (simplified equal weighting if NAV unavailable)
        if not weights:
            weights = [Decimal('1') / len(all_returns)] * len(all_returns)

        # Portfolio return series
        min_length = min(len(returns) for returns in all_returns)
        portfolio_returns = []

        for i in range(min_length):
            period_return = sum(weight * returns[i] for weight, returns in zip(weights, all_returns))
            portfolio_returns.append(period_return)

        if not portfolio_returns:
            return {"error": "Cannot calculate portfolio returns"}

        # Portfolio risk metrics
        portfolio_vol = self._calculate_portfolio_volatility(portfolio_returns)
        portfolio_var = self._calculate_var(portfolio_returns)

        return {
            "portfolio_volatility": float(portfolio_vol),
            "portfolio_var_95": float(portfolio_var),
            "number_of_periods": len(portfolio_returns),
            "correlation_weighted_risk": "high"  # Digital assets typically highly correlated
        }

    def _calculate_portfolio_volatility(self, returns: List[Decimal]) -> Decimal:
        """Calculate portfolio volatility"""
        if len(returns) < 2:
            return Decimal('0')

        mean_return = sum(returns) / len(returns)
        variance = sum((r - mean_return) ** 2 for r in returns) / (len(returns) - 1)
        daily_vol = variance.sqrt()

        # Annualized volatility
        return daily_vol * Constants.DAYS_IN_YEAR.sqrt()

    def _calculate_var(self, returns: List[Decimal], confidence: Decimal = Decimal('0.05')) -> Decimal:
        """Calculate Value at Risk"""
        if not returns:
            return Decimal('0')

        sorted_returns = sorted(returns)
        var_index = int(len(sorted_returns) * confidence)

        if var_index >= len(sorted_returns):
            var_index = len(sorted_returns) - 1

        return abs(sorted_returns[var_index])


# Export main components
__all__ = ['DigitalAssetAnalyzer', 'DigitalAssetPortfolio']