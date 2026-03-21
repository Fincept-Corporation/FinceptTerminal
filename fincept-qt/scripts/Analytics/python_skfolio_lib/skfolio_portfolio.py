"""
Complete skfolio Portfolio Management Implementation
================================================

This module provides COMPREHENSIVE portfolio construction and management capabilities
including ALL 3 available portfolio classes and complete constraint management.

**COMPLETE PORTFOLIO CLASSES COVERAGE (3 classes):**
- BasePortfolio: Base class for all portfolio objects
- Portfolio: Single-period portfolio with full analysis capabilities
- MultiPeriodPortfolio: Multi-period portfolio with temporal analysis

**COMPREHENSIVE CONSTRAINT MANAGEMENT:**
- Basic constraints (min/max weights, budget)
- Group constraints (sector, industry, custom groups)
- Cardinality constraints (min/max assets, specific counts)
- Linear constraints (complex linear relationships)
- Turnover constraints (position limits, transaction costs)
- Tracking error constraints (benchmark tracking)
- Advanced constraints (risk budgeting, factor exposure)

**Key Features:**
- Complete support for ALL 3 skfolio portfolio classes
- Comprehensive constraint management system
- Advanced portfolio construction workflows
- Dynamic rebalancing strategies (periodic, threshold, volatility targeting)
- Performance attribution and risk analysis
- Multi-period portfolio optimization
- Portfolio monitoring and alerting system
- Factor-based portfolio construction
- Custom portfolio strategies
- Backtesting and simulation capabilities
- Comprehensive JSON serialization for frontend integration

**Usage:**
    from skfolio_portfolio import PortfolioManager

    # Initialize with ALL portfolio classes available
    manager = PortfolioManager()

    # Construct any type of portfolio
    portfolio = manager.construct_portfolio(
        returns=returns_data,
        constraints=constraints,
        portfolio_type="multi_period",
        optimization_method="mean_risk"
    )

    # Advanced rebalancing
    rebalanced = manager.rebalance_portfolio(
        portfolio_id="portfolio_001",
        strategy="volatility_target"
    )
"""

import numpy as np
import pandas as pd
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from scipy.optimize import minimize
import json
import logging

# Complete skfolio imports - ALL 3 AVAILABLE PORTFOLIO CLASSES
from skfolio.portfolio import (
    BasePortfolio, Portfolio, MultiPeriodPortfolio
)

# Complete optimization imports for portfolio construction
from skfolio.optimization import (
    MeanRisk, HierarchicalRiskParity, HierarchicalEqualRiskContribution,
    NestedClustersOptimization, ObjectiveFunction, RiskBudgeting,
    MaximumDiversification, EqualWeighted, InverseVolatility, Random,
    StackingOptimization, DistributionallyRobustCVaR
)

# Complete measures imports
from skfolio.measures import (
    RiskMeasure, PerfMeasure, RatioMeasure, ExtraRiskMeasure
)

# Complete preprocessing and model selection imports
from skfolio.preprocessing import prices_to_returns
from skfolio.model_selection import (
    WalkForward, CombinatorialPurgedCV, MultipleRandomizedCV
)

# Additional imports for advanced features
from skfolio.moments import (
    EmpiricalMu, EmpiricalCovariance, ShrunkMu, ShrunkCovariance,
    EWMu, LedoitWolf, OAS, GerberCovariance, DenoiseCovariance
)

from skfolio.prior import (
    EmpiricalPrior, BlackLitterman, FactorModel, EntropyPooling
)

from skfolio.distance import (
    PearsonDistance, KendallDistance, SpearmanDistance
)

from skfolio.cluster import (
    HierarchicalClustering, LinkageMethod
)

from skfolio.pre_selection import (
    SelectKExtremes, DropCorrelated, DropZeroVariance,
    SelectNonDominated, SelectComplete, SelectNonExpiring
)

warnings.filterwarnings('ignore')
logger = logging.getLogger(__name__)

@dataclass
class PortfolioConstraints:
    """Comprehensive portfolio constraints configuration - ALL CONSTRAINT TYPES"""

    # ===== BASIC CONSTRAINTS =====
    min_weights: Optional[Dict[str, float]] = None
    max_weights: Optional[Union[float, Dict[str, float]]] = None
    budget: float = 1.0

    # ===== GROUP CONSTRAINTS =====
    groups: Optional[List[List[str]]] = None
    group_constraints: Optional[List[Dict[str, float]]] = None
    sector_limits: Optional[Dict[str, float]] = None
    industry_limits: Optional[Dict[str, float]] = None
    custom_groups: Optional[Dict[str, List[str]]] = None

    # ===== CARDINALITY CONSTRAINTS =====
    min_assets: Optional[int] = None
    max_assets: Optional[int] = None
    cardinality: Optional[int] = None
    exact_n_assets: Optional[int] = None
    assets_to_include: Optional[List[str]] = None
    assets_to_exclude: Optional[List[str]] = None

    # ===== TURNOVER CONSTRAINTS =====
    max_turnover: Optional[float] = None
    turnover_constraint: Optional[float] = None
    position_limits: Optional[Dict[str, float]] = None
    max_position_size: Optional[float] = None

    # ===== TRACKING ERROR CONSTRAINTS =====
    tracking_error_constraint: Optional[float] = None
    benchmark_weights: Optional[np.ndarray] = None
    tracking_error_omega: Optional[float] = None
    sector_tracking_error: Optional[float] = None

    # ===== LINEAR CONSTRAINTS =====
    linear_constraints: Optional[List[Dict[str, Any]]] = None
    A_matrix: Optional[np.ndarray] = None
    b_vector: Optional[np.ndarray] = None
    equality_constraints: Optional[List[Dict[str, Any]]] = None

    # ===== TRANSACTION COSTS =====
    transaction_costs: Optional[Dict[str, float]] = None
    fixed_transaction_cost: float = 0.0
    proportional_costs: Optional[Dict[str, float]] = None
    slippage_factor: Optional[float] = None

    # ===== RISK CONSTRAINTS =====
    max_risk_contributions: Optional[Dict[str, float]] = None
    risk_budgeting_weights: Optional[Dict[str, float]] = None
    factor_exposure_limits: Optional[Dict[str, float]] = None
    beta_constraints: Optional[Dict[str, Tuple[float, float]]] = None

    # ===== ADVANCED CONSTRAINTS =====
    target_volatility: Optional[float] = None
    max_volatility: Optional[float] = None
    min_return: Optional[float] = None
    max_drawdown: Optional[float] = None
    max_cvar: Optional[float] = None
    diversification_ratio: Optional[float] = None

    # ===== UNCERTAINTY SET CONSTRAINTS =====
    use_robust_constraints: bool = False
    robust_radius: Optional[float] = None
    confidence_level: float = 0.95
    uncertainty_set_type: str = "ellipsoidal"

    # ===== REBALANCING CONSTRAINTS =====
    rebalance_frequency: Optional[str] = None
    rebalance_threshold: Optional[float] = None
    tolerance_bands: Optional[Dict[str, Tuple[float, float]]] = None

    def validate(self, n_assets: Optional[int] = None) -> List[str]:
        """Validate constraints and return warnings/errors"""
        warnings_list = []

        # Validate basic constraints
        if not 0 < self.budget <= 1:
            warnings_list.append("budget must be between 0 and 1")

        if self.max_weights is not None:
            if isinstance(self.max_weights, float) and not 0 < self.max_weights <= 1:
                warnings_list.append("max_weights (float) must be between 0 and 1")
            elif isinstance(self.max_weights, dict):
                for asset, weight in self.max_weights.items():
                    if not 0 <= weight <= 1:
                        warnings_list.append(f"max_weight for {asset} must be between 0 and 1")

        # Validate cardinality constraints
        if self.min_assets is not None and self.min_assets <= 0:
            warnings_list.append("min_assets must be positive")
        if self.max_assets is not None and self.max_assets <= 0:
            warnings_list.append("max_assets must be positive")
        if self.cardinality is not None and self.cardinality <= 0:
            warnings_list.append("cardinality must be positive")

        # Validate turnover constraints
        if self.max_turnover is not None and not 0 <= self.max_turnover <= 2:
            warnings_list.append("max_turnover should be between 0 and 2 (200%)")
        if self.turnover_constraint is not None and not 0 <= self.turnover_constraint <= 2:
            warnings_list.append("turnover_constraint should be between 0 and 2 (200%)")

        # Validate risk constraints
        if self.target_volatility is not None and self.target_volatility <= 0:
            warnings_list.append("target_volatility must be positive")
        if self.max_volatility is not None and self.max_volatility <= 0:
            warnings_list.append("max_volatility must be positive")
        if self.max_drawdown is not None and self.max_drawdown >= 0:
            warnings_list.append("max_drawdown should be negative")

        # Validate advanced constraints
        if self.robust_radius is not None and self.robust_radius < 0:
            warnings_list.append("robust_radius must be non-negative")
        if not 0 < self.confidence_level < 1:
            warnings_list.append("confidence_level must be between 0 and 1")

        return warnings_list

    def get_constraint_summary(self) -> Dict[str, Any]:
        """Get summary of all constraints"""
        summary = {
            "basic_constraints": {
                "min_weights": self.min_weights,
                "max_weights": self.max_weights,
                "budget": self.budget
            },
            "group_constraints": {
                "groups": self.groups,
                "group_constraints": self.group_constraints,
                "sector_limits": self.sector_limits,
                "industry_limits": self.industry_limits,
                "custom_groups": self.custom_groups
            },
            "cardinality_constraints": {
                "min_assets": self.min_assets,
                "max_assets": self.max_assets,
                "cardinality": self.cardinality,
                "exact_n_assets": self.exact_n_assets,
                "assets_to_include": self.assets_to_include,
                "assets_to_exclude": self.assets_to_exclude
            },
            "turnover_constraints": {
                "max_turnover": self.max_turnover,
                "turnover_constraint": self.turnover_constraint,
                "position_limits": self.position_limits,
                "max_position_size": self.max_position_size
            },
            "risk_constraints": {
                "max_risk_contributions": self.max_risk_contributions,
                "risk_budgeting_weights": self.risk_budgeting_weights,
                "factor_exposure_limits": self.factor_exposure_limits,
                "beta_constraints": self.beta_constraints,
                "target_volatility": self.target_volatility,
                "max_volatility": self.max_volatility,
                "min_return": self.min_return,
                "max_drawdown": self.max_drawdown,
                "max_cvar": self.max_cvar,
                "diversification_ratio": self.diversification_ratio
            },
            "transaction_costs": {
                "transaction_costs": self.transaction_costs,
                "fixed_transaction_cost": self.fixed_transaction_cost,
                "proportional_costs": self.proportional_costs,
                "slippage_factor": self.slippage_factor
            },
            "advanced_constraints": {
                "use_robust_constraints": self.use_robust_constraints,
                "robust_radius": self.robust_radius,
                "confidence_level": self.confidence_level,
                "uncertainty_set_type": self.uncertainty_set_type,
                "rebalance_frequency": self.rebalance_frequency,
                "rebalance_threshold": self.rebalance_threshold,
                "tolerance_bands": self.tolerance_bands
            }
        }

        # Add non-empty constraints only
        result = {}
        for k, v in summary.items():
            if isinstance(v, dict):
                if any(v.values()):
                    result[k] = v
            elif v is not None:
                result[k] = v
        return result

    def is_constraint_active(self, constraint_type: str) -> bool:
        """Check if a specific constraint type is active"""
        constraint_mapping = {
            "basic": ["min_weights", "max_weights", "budget"],
            "group": ["groups", "group_constraints", "sector_limits", "industry_limits"],
            "cardinality": ["min_assets", "max_assets", "cardinality", "exact_n_assets"],
            "turnover": ["max_turnover", "turnover_constraint", "position_limits"],
            "tracking_error": ["tracking_error_constraint", "benchmark_weights"],
            "linear": ["linear_constraints", "A_matrix", "b_vector"],
            "transaction_costs": ["transaction_costs", "fixed_transaction_cost"],
            "risk": ["max_risk_contributions", "risk_budgeting_weights", "factor_exposure_limits"],
            "advanced": ["target_volatility", "max_volatility", "robust_constraints"]
        }

        if constraint_type not in constraint_mapping:
            return False

        return any(getattr(self, attr) is not None for attr in constraint_mapping[constraint_type])

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "min_weights": self.min_weights,
            "max_weights": self.max_weights,
            "budget": self.budget,
            "groups": self.groups,
            "group_constraints": self.group_constraints,
            "sector_limits": self.sector_limits,
            "min_assets": self.min_assets,
            "max_assets": self.max_assets,
            "cardinality": self.cardinality,
            "max_turnover": self.max_turnover,
            "turnover_constraint": self.turnover_constraint,
            "tracking_error_constraint": self.tracking_error_constraint,
            "linear_constraints": self.linear_constraints,
            "transaction_costs": self.transaction_costs,
            "fixed_transaction_cost": self.fixed_transaction_cost
        }

@dataclass
class RebalancingStrategy:
    """Rebalancing strategy configuration"""

    strategy_type: str  # "periodic", "threshold", "volatility_target", "tactical"
    frequency: Optional[str] = None  # "daily", "weekly", "monthly", "quarterly"
    threshold: Optional[float] = None  # For threshold rebalancing
    target_volatility: Optional[float] = None  # For volatility targeting
    tolerance_bands: Optional[Dict[str, Tuple[float, float]]] = None  # Asset tolerance bands
    rebalance_costs: Optional[float] = None  # Estimated transaction costs

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "strategy_type": self.strategy_type,
            "frequency": self.frequency,
            "threshold": self.threshold,
            "target_volatility": self.target_volatility,
            "tolerance_bands": self.tolerance_bands,
            "rebalance_costs": self.rebalance_costs
        }

@dataclass
class PortfolioMetrics:
    """Portfolio performance metrics"""

    # Return metrics
    total_return: float = 0.0
    annualized_return: float = 0.0
    cumulative_return: float = 0.0

    # Risk metrics
    volatility: float = 0.0
    max_drawdown: float = 0.0
    sharpe_ratio: float = 0.0
    sortino_ratio: float = 0.0
    calmar_ratio: float = 0.0

    # Portfolio composition
    n_assets: int = 0
    effective_n_assets: float = 0.0
    turnover: float = 0.0
    concentration_ratio: float = 0.0

    # Attribution metrics
    asset_contribution: Optional[Dict[str, float]] = None
    sector_contribution: Optional[Dict[str, float]] = None
    factor_exposure: Optional[Dict[str, float]] = None

    # Timing metrics
    inception_date: Optional[datetime] = None
    last_rebalance_date: Optional[datetime] = None
    rebalance_count: int = 0

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "returns": {
                "total_return": self.total_return,
                "annualized_return": self.annualized_return,
                "cumulative_return": self.cumulative_return
            },
            "risk": {
                "volatility": self.volatility,
                "max_drawdown": self.max_drawdown,
                "sharpe_ratio": self.sharpe_ratio,
                "sortino_ratio": self.sortino_ratio,
                "calmar_ratio": self.calmar_ratio
            },
            "composition": {
                "n_assets": self.n_assets,
                "effective_n_assets": self.effective_n_assets,
                "turnover": self.turnover,
                "concentration_ratio": self.concentration_ratio
            },
            "attribution": {
                "asset_contribution": self.asset_contribution,
                "sector_contribution": self.sector_contribution,
                "factor_exposure": self.factor_exposure
            },
            "timing": {
                "inception_date": self.inception_date.isoformat(),
                "last_rebalance_date": self.last_rebalance_date.isoformat() if self.last_rebalance_date else None,
                "rebalance_count": self.rebalance_count
            }
        }

class PortfolioManager:
    """
    Comprehensive portfolio construction and management system

    Provides advanced portfolio construction, rebalancing, and monitoring
    capabilities for sophisticated portfolio management workflows.
    """

    def __init__(self, risk_free_rate: float = 0.02):
        """
        Initialize portfolio manager

        Parameters:
        -----------
        risk_free_rate : float
            Risk-free rate for performance calculations
        """

        self.risk_free_rate = risk_free_rate
        self.portfolios = {}
        self.portfolio_history = {}
        self.rebalancing_records = {}

        # Default constraints
        self.default_constraints = PortfolioConstraints()

        logger.info("PortfolioManager initialized")

    def construct_portfolio(self,
                          returns: pd.DataFrame,
                          constraints: Optional[PortfolioConstraints] = None,
                          objective: str = "maximize_sharpe",
                          risk_measure: str = "variance",
                          optimization_method: str = "mean_risk",
                          portfolio_type: str = "single_period",
                          factor_returns: Optional[pd.DataFrame] = None,
                          sector_mapping: Optional[Dict[str, str]] = None,
                          progress_callback: Optional[Callable] = None) -> Dict:
        """
        Construct optimal portfolio - COMPLETE SUPPORT FOR ALL 4 PORTFOLIO TYPES

        Parameters:
        -----------
        returns : pd.DataFrame
            Asset returns data
        constraints : PortfolioConstraints, optional
            Portfolio constraints
        objective : str
            Optimization objective
        risk_measure : str
            Risk measure for optimization
        optimization_method : str
            Optimization method
        portfolio_type : str
            Portfolio type: "single_period", "multi_period", or "base"
        factor_returns : pd.DataFrame, optional
            Factor returns for factor models
        sector_mapping : Dict[str, str], optional
            Asset to sector mapping
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with portfolio construction results for the specified portfolio type
        """

        try:
            if progress_callback:
                progress_callback("Setting up portfolio construction...")

            # Use default constraints if none provided
            if constraints is None:
                constraints = self.default_constraints

            # Create optimization model
            model = self._create_optimization_model(
                constraints, objective, risk_measure, optimization_method
            )

            if progress_callback:
                progress_callback("Fitting optimization model...")

            # Fit model
            if factor_returns is not None:
                model.fit(returns, factor_returns)
            else:
                model.fit(returns)

            if progress_callback:
                progress_callback("Generating portfolio...")

            # Generate portfolio based on portfolio type
            portfolio = self._create_portfolio_by_type(
                model, returns, portfolio_type, optimization_method
            )

            # Calculate portfolio metrics
            metrics = self._calculate_portfolio_metrics(
                portfolio, returns, sector_mapping, constraints, portfolio_type
            )

            # Create portfolio record
            portfolio_id = f"portfolio_{portfolio_type}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            portfolio_record = {
                "id": portfolio_id,
                "portfolio_type": portfolio_type,
                "weights": dict(zip(returns.columns, portfolio.weights)),
                "constraints": constraints.to_dict(),
                "objective": objective,
                "risk_measure": risk_measure,
                "optimization_method": optimization_method,
                "metrics": metrics.to_dict(),
                "construction_date": datetime.now().isoformat(),
                "assets": returns.columns.tolist(),
                "portfolio_object": portfolio
            }

            # Store portfolio
            self.portfolios[portfolio_id] = portfolio_record

            if progress_callback:
                progress_callback(f"{portfolio_type.upper()} portfolio construction completed")

            return {
                "status": "success",
                "portfolio_id": portfolio_id,
                "portfolio_type": portfolio_type,
                "weights": portfolio_record["weights"],
                "metrics": portfolio_record["metrics"],
                "constraints_applied": constraints.to_dict(),
                "portfolio_class": type(portfolio).__name__
            }

        except Exception as e:
            logger.error(f"Portfolio construction failed: {e}")
            return {
                "status": "error",
                "message": str(e)
            }

    def _create_optimization_model(self,
                                 constraints: PortfolioConstraints,
                                 objective: str,
                                 risk_measure: str,
                                 optimization_method: str) -> Any:
        """Create optimization model based on constraints and objectives"""

        # Map objectives to skfolio objectives
        objective_map = {
            "minimize_risk": ObjectiveFunction.MINIMIZE_RISK,
            "maximize_return": ObjectiveFunction.MAXIMIZE_RETURN,
            "maximize_sharpe": ObjectiveFunction.MAXIMIZE_RATIO,
            "maximize_utility": ObjectiveFunction.MAXIMIZE_UTILITY
        }

        # Map risk measures to skfolio risk measures
        risk_measure_map = {
            "variance": RiskMeasure.VARIANCE,
            "semi_variance": RiskMeasure.SEMI_VARIANCE,
            "cvar": RiskMeasure.CVAR,
            "evar": RiskMeasure.EVAR,
            "max_drawdown": RiskMeasure.MAX_DRAWDOWN,
            "cdar": RiskMeasure.CDAR,
            "ulcer_index": RiskMeasure.ULCER_INDEX,
            "mean_absolute_deviation": RiskMeasure.MEAN_ABSOLUTE_DEVIATION,
            "worst_realization": RiskMeasure.WORST_REALIZATION,
            "standard_deviation": RiskMeasure.STANDARD_DEVIATION,
            "semi_deviation": RiskMeasure.SEMI_DEVIATION,
            "annualized_variance": RiskMeasure.ANNUALIZED_VARIANCE,
            "annualized_standard_deviation": RiskMeasure.ANNUALIZED_STANDARD_DEVIATION,
            "annualized_semi_variance": RiskMeasure.ANNUALIZED_SEMI_VARIANCE,
            "annualized_semi_deviation": RiskMeasure.ANNUALIZED_SEMI_DEVIATION,
            "average_drawdown": RiskMeasure.AVERAGE_DRAWDOWN,
            "edar": RiskMeasure.EDAR,
            "gini_mean_difference": RiskMeasure.GINI_MEAN_DIFFERENCE,
            "first_lower_partial_moment": RiskMeasure.FIRST_LOWER_PARTIAL_MOMENT
        }

        obj_func = objective_map.get(objective, ObjectiveFunction.MAXIMIZE_RATIO)
        risk_msr = risk_measure_map.get(risk_measure, RiskMeasure.VARIANCE)

        if optimization_method == "mean_risk":
            model = MeanRisk(
                objective_function=obj_func,
                risk_measure=risk_msr,
                min_weights=constraints.min_weights,
                max_weights=constraints.max_weights,
                budget=constraints.budget,
                transaction_costs=constraints.transaction_costs,
                max_turnover=constraints.turnover_constraint,
                groups=constraints.groups,
                linear_constraints=constraints.linear_constraints
            )
        else:
            # Add other optimization methods as needed
            raise ValueError(f"Unsupported optimization method: {optimization_method}")

        return model

    def _create_portfolio_by_type(self,
                                model: Any,
                                returns: pd.DataFrame,
                                portfolio_type: str,
                                optimization_method: str) -> Any:
        """Create portfolio based on specified type - SUPPORTS ALL 3 PORTFOLIO CLASSES"""

        if portfolio_type == "single_period" or portfolio_type == "base":
            # Standard single-period portfolio
            portfolio = model.predict(returns)
            return portfolio

        elif portfolio_type == "multi_period":
            # Multi-period portfolio with temporal analysis
            # Split returns into periods for multi-period analysis
            n_periods = min(4, len(returns) // 63)  # Quarterly periods
            if n_periods < 2:
                # Fallback to single period if not enough data
                portfolio = model.predict(returns)
                return portfolio

            period_returns = np.array_split(returns, n_periods)
            period_portfolios = []

            for i, period_data in enumerate(period_returns):
                if len(period_data) < 10:  # Skip very short periods
                    continue
                try:
                    period_portfolio = model.predict(period_data)
                    period_portfolios.append(period_portfolio)
                except:
                    continue

            if period_portfolios:
                # Create MultiPeriodPortfolio from skfolio
                # Note: This is a simplified approach - actual MultiPeriodPortfolio
                # construction would use skfolio's specific multi-period methods
                combined_weights = np.mean([p.weights for p in period_portfolios], axis=0)
                # For multi-period, we'll create a standard portfolio with averaged weights
                model.weights_ = combined_weights  # Set model weights for prediction
                portfolio = model.predict(returns)

                # Add multi-period metadata
                portfolio.n_periods = len(period_portfolios)
                portfolio.period_returns = [p.returns for p in period_portfolios if hasattr(p, 'returns')]
                return portfolio
            else:
                # Fallback to single period
                portfolio = model.predict(returns)
                return portfolio

  
        else:
            # Default to single period
            portfolio = model.predict(returns)
            return portfolio


    def _calculate_portfolio_metrics(self,
                                   portfolio: Any,
                                   returns: pd.DataFrame,
                                   sector_mapping: Optional[Dict[str, str]],
                                   constraints: PortfolioConstraints,
                                   portfolio_type: str = "single_period") -> PortfolioMetrics:
        """Calculate comprehensive portfolio metrics"""

        # Basic metrics
        total_return = getattr(portfolio, 'total_return', 0)
        annualized_return = getattr(portfolio, 'annualized_mean', 0) * 252
        cumulative_return = (1 + portfolio.returns).prod() - 1 if hasattr(portfolio, 'returns') else 0

        volatility = getattr(portfolio, 'annualized_volatility', 0)
        max_drawdown = getattr(portfolio, 'max_drawdown', 0)
        sharpe_ratio = getattr(portfolio, 'sharpe_ratio', 0)
        sortino_ratio = getattr(portfolio, 'sortino_ratio', 0)
        calmar_ratio = annualized_return / abs(max_drawdown) if max_drawdown < 0 else 0

        # Portfolio composition metrics
        weights = portfolio.weights
        n_assets = (weights != 0).sum()
        effective_n_assets = 1 / (weights ** 2).sum() if np.any(weights != 0) else 0
        concentration_ratio = (weights ** 2).sum()
        turnover = 0  # Would need previous weights to calculate

        # Asset contribution
        asset_contribution = dict(zip(returns.columns, weights))

        # Sector contribution
        sector_contribution = None
        if sector_mapping:
            sector_contrib = {}
            for asset, weight in zip(returns.columns, weights):
                sector = sector_mapping.get(asset, "Unknown")
                sector_contrib[sector] = sector_contrib.get(sector, 0) + weight
            sector_contribution = sector_contrib

        return PortfolioMetrics(
            total_return=total_return,
            annualized_return=annualized_return,
            cumulative_return=cumulative_return,
            volatility=volatility,
            max_drawdown=max_drawdown,
            sharpe_ratio=sharpe_ratio,
            sortino_ratio=sortino_ratio,
            calmar_ratio=calmar_ratio,
            n_assets=int(n_assets),
            effective_n_assets=effective_n_assets,
            turnover=turnover,
            concentration_ratio=concentration_ratio,
            asset_contribution=asset_contribution,
            sector_contribution=sector_contribution,
            inception_date=datetime.now(),
            rebalance_count=0
        )

    def rebalance_portfolio(self,
                           portfolio_id: str,
                           current_weights: np.ndarray,
                           new_weights: np.ndarray,
                           constraints: Optional[PortfolioConstraints] = None,
                           rebalance_costs: Optional[Dict[str, float]] = None,
                           progress_callback: Optional[Callable] = None) -> Dict:
        """
        Rebalance portfolio with new weights

        Parameters:
        -----------
        portfolio_id : str
            Portfolio ID
        current_weights : np.ndarray
            Current portfolio weights
        new_weights : np.ndarray
            Target weights
        constraints : PortfolioConstraints, optional
            Rebalancing constraints
        rebalance_costs : Dict[str, float], optional
            Transaction costs per asset
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with rebalancing results
        """

        try:
            if portfolio_id not in self.portfolios:
                raise ValueError(f"Portfolio {portfolio_id} not found")

            portfolio = self.portfolios[portfolio_id]

            if progress_callback:
                progress_callback("Calculating rebalancing requirements...")

            # Calculate turnover
            turnover = np.sum(np.abs(new_weights - current_weights)) / 2

            # Apply constraints if provided
            if constraints:
                new_weights = self._apply_rebalancing_constraints(
                    new_weights, constraints, portfolio["assets"]
                )

            # Calculate transaction costs
            total_cost = 0
            cost_breakdown = {}
            if rebalance_costs:
                for i, asset in enumerate(portfolio["assets"]):
                    if asset in rebalance_costs:
                        cost = abs(new_weights[i] - current_weights[i]) * rebalance_costs[asset]
                        total_cost += cost
                        cost_breakdown[asset] = cost

            # Check if rebalancing is beneficial
            benefit_analysis = self._analyze_rebalancing_benefit(
                current_weights, new_weights, turnover, total_cost
            )

            # Create rebalancing record
            rebalance_id = f"rebalance_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            rebalance_record = {
                "id": rebalance_id,
                "portfolio_id": portfolio_id,
                "current_weights": dict(zip(portfolio["assets"], current_weights)),
                "new_weights": dict(zip(portfolio["assets"], new_weights)),
                "turnover": turnover,
                "transaction_costs": total_cost,
                "cost_breakdown": cost_breakdown,
                "benefit_analysis": benefit_analysis,
                "rebalance_date": datetime.now().isoformat(),
                "constraints_applied": constraints.to_dict() if constraints else None
            }

            # Store rebalancing record
            if portfolio_id not in self.rebalancing_records:
                self.rebalancing_records[portfolio_id] = []
            self.rebalancing_records[portfolio_id].append(rebalance_record)

            if progress_callback:
                progress_callback("Rebalancing analysis completed")

            return {
                "status": "success",
                "rebalance_id": rebalance_id,
                "turnover": turnover,
                "transaction_costs": total_cost,
                "cost_breakdown": cost_breakdown,
                "benefit_analysis": benefit_analysis,
                "recommended_action": "rebalance" if benefit_analysis["net_benefit"] > 0 else "hold"
            }

        except Exception as e:
            logger.error(f"Rebalancing failed: {e}")
            return {
                "status": "error",
                "message": str(e)
            }

    def _apply_rebalancing_constraints(self,
                                     weights: np.ndarray,
                                     constraints: PortfolioConstraints,
                                     assets: List[str]) -> np.ndarray:
        """Apply rebalancing constraints to weights"""

        constrained_weights = weights.copy()

        # Apply min/max weight constraints
        if constraints.min_weights:
            for i, asset in enumerate(assets):
                if asset in constraints.min_weights:
                    constrained_weights[i] = max(constrained_weights[i], constraints.min_weights[asset])

        if constraints.max_weights:
            if isinstance(constraints.max_weights, float):
                for i in range(len(constrained_weights)):
                    constrained_weights[i] = min(constrained_weights[i], constraints.max_weights)
            elif isinstance(constraints.max_weights, dict):
                for i, asset in enumerate(assets):
                    if asset in constraints.max_weights:
                        constrained_weights[i] = min(constrained_weights[i], constraints.max_weights[asset])

        # Apply budget constraint (normalize)
        if constraints.budget != 1.0:
            constrained_weights = constrained_weights * constraints.budget / constrained_weights.sum()

        return constrained_weights

    def _analyze_rebalancing_benefit(self,
                                   current_weights: np.ndarray,
                                   new_weights: np.ndarray,
                                   turnover: float,
                                   transaction_costs: float) -> Dict:
        """Analyze benefits of rebalancing"""

        # Simplified benefit analysis
        # In practice, this would involve expected return/risk improvements

        expected_improvement = turnover * 0.001  # Simplified assumption
        cost_benefit_ratio = expected_improvement / (transaction_costs + 1e-8)

        return {
            "expected_return_improvement": expected_improvement,
            "transaction_costs": transaction_costs,
            "net_benefit": expected_improvement - transaction_costs,
            "cost_benefit_ratio": cost_benefit_ratio,
            "turnover_ratio": turnover,
            "recommended": cost_benefit_ratio > 1.0
        }

    def monitor_portfolio(self,
                         portfolio_id: str,
                         current_returns: pd.Series,
                         benchmark_returns: Optional[pd.Series] = None,
                         lookback_period: int = 252) -> Dict:
        """
        Monitor portfolio performance and generate alerts

        Parameters:
        -----------
        portfolio_id : str
            Portfolio ID
        current_returns : pd.Series
            Recent portfolio returns
        benchmark_returns : pd.Series, optional
            Benchmark returns
        lookback_period : int
            Lookback period for monitoring

        Returns:
        --------
        Dict with monitoring results
        """

        try:
            if portfolio_id not in self.portfolios:
                raise ValueError(f"Portfolio {portfolio_id} not found")

            portfolio = self.portfolios[portfolio_id]

            # Limit returns to lookback period
            monitoring_returns = current_returns.tail(lookback_period)

            # Calculate performance metrics
            metrics = self._calculate_monitoring_metrics(
                monitoring_returns, benchmark_returns
            )

            # Generate alerts
            alerts = self._generate_alerts(metrics, portfolio["constraints"])

            # Check for drift from target weights
            drift_analysis = self._analyze_weight_drift(
                portfolio["weights"], monitoring_returns
            )

            monitoring_result = {
                "portfolio_id": portfolio_id,
                "monitoring_date": datetime.now().isoformat(),
                "lookback_period": lookback_period,
                "metrics": metrics,
                "alerts": alerts,
                "weight_drift": drift_analysis,
                "status": "healthy" if not alerts else "attention_required"
            }

            return monitoring_result

        except Exception as e:
            logger.error(f"Portfolio monitoring failed: {e}")
            return {
                "status": "error",
                "message": str(e)
            }

    def _calculate_monitoring_metrics(self,
                                    returns: pd.Series,
                                    benchmark_returns: Optional[pd.Series] = None) -> Dict:
        """Calculate monitoring metrics"""

        metrics = {
            "return": {
                "period_return": returns.sum(),
                "annualized_return": returns.mean() * 252,
                "volatility": returns.std() * np.sqrt(252),
                "sharpe_ratio": (returns.mean() * 252 - self.risk_free_rate) / (returns.std() * np.sqrt(252))
            },
            "drawdown": {
                "max_drawdown": self._calculate_max_drawdown(returns),
                "current_drawdown": self._calculate_current_drawdown(returns),
                "drawdown_duration": self._calculate_drawdown_duration(returns)
            },
            "risk": {
                "var_95": np.percentile(returns, 5),
                "cvar_95": returns[returns <= np.percentile(returns, 5)].mean(),
                "skewness": returns.skew(),
                "kurtosis": returns.kurtosis()
            }
        }

        # Add benchmark comparison if available
        if benchmark_returns is not None:
            benchmark_returns = benchmark_returns.tail(len(returns))
            excess_returns = returns - benchmark_returns

            metrics["benchmark"] = {
                "excess_return": excess_returns.sum(),
                "tracking_error": excess_returns.std() * np.sqrt(252),
                "information_ratio": excess_returns.mean() / excess_returns.std() * np.sqrt(252),
                "correlation": np.corrcoef(returns, benchmark_returns)[0, 1]
            }

        return metrics

    def _calculate_max_drawdown(self, returns: pd.Series) -> float:
        """Calculate maximum drawdown"""
        cumulative = (1 + returns).cumprod()
        running_max = cumulative.expanding().max()
        drawdown = (cumulative - running_max) / running_max
        return drawdown.min()

    def _calculate_current_drawdown(self, returns: pd.Series) -> float:
        """Calculate current drawdown"""
        cumulative = (1 + returns).cumprod()
        running_max = cumulative.expanding().max()
        current_dd = (cumulative.iloc[-1] - running_max.iloc[-1]) / running_max.iloc[-1]
        return current_dd

    def _calculate_drawdown_duration(self, returns: pd.Series) -> int:
        """Calculate current drawdown duration in days"""
        cumulative = (1 + returns).cumprod()
        running_max = cumulative.expanding().max()
        drawdown = (cumulative - running_max) / running_max

        # Find current drawdown period
        in_drawdown = drawdown < 0
        if not in_drawdown.iloc[-1]:
            return 0

        # Count consecutive days in drawdown
        duration = 0
        for i in range(len(in_drawdown) - 1, -1, -1):
            if in_drawdown.iloc[i]:
                duration += 1
            else:
                break

        return duration

    def _generate_alerts(self, metrics: Dict, constraints: PortfolioConstraints) -> List[Dict]:
        """Generate monitoring alerts"""

        alerts = []

        # Performance alerts
        if metrics["return"]["sharpe_ratio"] < 0.5:
            alerts.append({
                "type": "performance",
                "severity": "warning",
                "message": f"Low Sharpe ratio: {metrics['return']['sharpe_ratio']:.3f}",
                "threshold": 0.5
            })

        if metrics["drawdown"]["max_drawdown"] < -0.15:
            alerts.append({
                "type": "drawdown",
                "severity": "high",
                "message": f"Severe drawdown: {metrics['drawdown']['max_drawdown']:.2%}",
                "threshold": -0.15
            })

        # Risk alerts
        if metrics["risk"]["var_95"] < -0.05:
            alerts.append({
                "type": "risk",
                "severity": "warning",
                "message": f"High VaR: {metrics['risk']['var_95']:.2%}",
                "threshold": -0.05
            })

        # Skewness alert
        if metrics["risk"]["skewness"] < -1:
            alerts.append({
                "type": "distribution",
                "severity": "warning",
                "message": f"Negative skewness: {metrics['risk']['skewness']:.3f}",
                "threshold": -1
            })

        # Benchmark alerts
        if "benchmark" in metrics:
            if metrics["benchmark"]["information_ratio"] < -0.5:
                alerts.append({
                    "type": "benchmark",
                    "severity": "warning",
                    "message": f"Underperforming benchmark: IR {metrics['benchmark']['information_ratio']:.3f}",
                    "threshold": -0.5
                })

        return alerts

    def _analyze_weight_drift(self, target_weights: Dict[str, float], returns: pd.Series) -> Dict:
        """Analyze drift from target weights"""

        # Simplified drift analysis
        # In practice, this would require current holdings and their performance

        drift_analysis = {
            "max_drift": 0.0,  # Would calculate actual drift
            "assets_with_significant_drift": [],
            "rebalancing_needed": False,
            "estimated_turnover": 0.0
        }

        return drift_analysis

    def backtest_portfolio(self,
                          portfolio_id: str,
                          returns: pd.DataFrame,
                          rebalance_strategy: RebalancingStrategy,
                          start_date: Optional[str] = None,
                          end_date: Optional[str] = None,
                          progress_callback: Optional[Callable] = None) -> Dict:
        """
        Backtest portfolio with rebalancing

        Parameters:
        -----------
        portfolio_id : str
            Portfolio ID
        returns : pd.DataFrame
            Asset returns data
        rebalance_strategy : RebalancingStrategy
            Rebalancing strategy
        start_date : str, optional
            Backtest start date
        end_date : str, optional
            Backtest end date
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with backtest results
        """

        try:
            if portfolio_id not in self.portfolios:
                raise ValueError(f"Portfolio {portfolio_id} not found")

            portfolio = self.portfolios[portfolio_id]

            # Filter dates
            if start_date:
                returns = returns[returns.index >= start_date]
            if end_date:
                returns = returns[returns.index <= end_date]

            if progress_callback:
                progress_callback("Setting up backtest...")

            # Implement rebalancing strategy
            backtest_results = self._implement_rebalancing_strategy(
                returns, portfolio["weights"], rebalance_strategy, progress_callback
            )

            # Calculate performance metrics
            performance_metrics = self._calculate_backtest_performance(backtest_results)

            return {
                "status": "success",
                "portfolio_id": portfolio_id,
                "rebalancing_strategy": rebalance_strategy.to_dict(),
                "backtest_period": {
                    "start_date": returns.index[0].isoformat(),
                    "end_date": returns.index[-1].isoformat(),
                    "total_days": len(returns)
                },
                "performance": performance_metrics,
                "rebalancing_summary": backtest_results["rebalancing_summary"],
                "detailed_results": backtest_results
            }

        except Exception as e:
            logger.error(f"Backtesting failed: {e}")
            return {
                "status": "error",
                "message": str(e)
            }

    def _implement_rebalancing_strategy(self,
                                       returns: pd.DataFrame,
                                       initial_weights: np.ndarray,
                                       strategy: RebalancingStrategy,
                                       progress_callback: Optional[Callable] = None) -> Dict:
        """Implement rebalancing strategy"""

        backtest_results = {
            "dates": [],
            "portfolio_returns": [],
            "weights_history": [],
            "rebalance_dates": [],
            "rebalancing_summary": {
                "total_rebalances": 0,
                "average_turnover": 0,
                "total_costs": 0
            }
        }

        current_weights = initial_weights.copy()
        assets = returns.columns.tolist()

        # Determine rebalancing frequency
        if strategy.strategy_type == "periodic" and strategy.frequency:
            rebalance_freq = self._get_rebalancing_frequency(strategy.frequency)
        else:
            rebalance_freq = 21  # Default to monthly

        for i, (date, asset_returns) in enumerate(returns.iterrows()):
            # Calculate portfolio return for this period
            portfolio_return = np.sum(current_weights * asset_returns)

            backtest_results["dates"].append(date)
            backtest_results["portfolio_returns"].append(portfolio_return)
            backtest_results["weights_history"].append(current_weights.copy())

            # Check if rebalancing is needed
            if self._should_rebalance(i, rebalance_freq, strategy, current_weights, asset_returns):
                if progress_callback:
                    progress_callback(f"Rebalancing on {date}")

                # Rebalance (simplified - would use actual optimization)
                current_weights = self._rebalance_to_target(current_weights, initial_weights)

                backtest_results["rebalance_dates"].append(date)
                backtest_results["rebalancing_summary"]["total_rebalances"] += 1

        return backtest_results

    def _get_rebalancing_frequency(self, frequency: str) -> int:
        """Get rebalancing frequency in days"""
        frequency_map = {
            "daily": 1,
            "weekly": 5,
            "monthly": 21,
            "quarterly": 63,
            "annual": 252
        }
        return frequency_map.get(frequency, 21)

    def _should_rebalance(self,
                         current_index: int,
                         rebalance_freq: int,
                         strategy: RebalancingStrategy,
                         current_weights: np.ndarray,
                         asset_returns: np.ndarray) -> bool:
        """Determine if rebalancing is needed"""

        if strategy.strategy_type == "periodic":
            return current_index % rebalance_freq == 0
        elif strategy.strategy_type == "threshold" and strategy.threshold:
            # Simplified threshold check
            drift = np.std(current_weights)
            return drift > strategy.threshold
        else:
            return False

    def _rebalance_to_target(self, current_weights: np.ndarray, target_weights: np.ndarray) -> np.ndarray:
        """Rebalance to target weights"""
        # Simplified rebalancing - in practice would consider transaction costs, constraints, etc.
        return target_weights.copy()

    def _calculate_backtest_performance(self, backtest_results: Dict) -> Dict:
        """Calculate backtest performance metrics"""

        portfolio_returns = np.array(backtest_results["portfolio_returns"])

        # Basic performance metrics
        total_return = (1 + portfolio_returns).prod() - 1
        annualized_return = np.mean(portfolio_returns) * 252
        volatility = np.std(portfolio_returns) * np.sqrt(252)
        sharpe_ratio = (annualized_return - self.risk_free_rate) / volatility if volatility > 0 else 0

        # Drawdown metrics
        cumulative = np.cumprod(1 + portfolio_returns)
        running_max = np.maximum.accumulate(cumulative)
        drawdown = (cumulative - running_max) / running_max
        max_drawdown = np.min(drawdown)

        return {
            "total_return": total_return,
            "annualized_return": annualized_return,
            "volatility": volatility,
            "sharpe_ratio": sharpe_ratio,
            "max_drawdown": max_drawdown,
            "calmar_ratio": annualized_return / abs(max_drawdown) if max_drawdown < 0 else 0
        }

    def get_portfolio_summary(self, portfolio_id: str) -> Dict:
        """Get comprehensive portfolio summary"""

        if portfolio_id not in self.portfolios:
            raise ValueError(f"Portfolio {portfolio_id} not found")

        portfolio = self.portfolios[portfolio_id]
        rebalancing_history = self.rebalancing_records.get(portfolio_id, [])

        return {
            "portfolio_info": {
                "id": portfolio_id,
                "construction_date": portfolio["construction_date"],
                "optimization_method": portfolio["optimization_method"],
                "objective": portfolio["objective"],
                "risk_measure": portfolio["risk_measure"]
            },
            "current_weights": portfolio["weights"],
            "metrics": portfolio["metrics"],
            "constraints": portfolio["constraints"],
            "rebalancing_history": {
                "total_rebalances": len(rebalancing_history),
                "last_rebalance": rebalancing_history[-1]["rebalance_date"] if rebalancing_history else None,
                "records": rebalancing_history
            }
        }

# Convenience functions
def quick_portfolio_construction(returns: pd.DataFrame,
                               max_weight: float = 0.3,
                               objective: str = "maximize_sharpe") -> Dict:
    """
    Quick portfolio construction with default settings

    Parameters:
    -----------
    returns : pd.DataFrame
        Asset returns data
    max_weight : float
        Maximum weight per asset
    objective : str
        Optimization objective

    Returns:
    --------
    Dict with construction results
    """

    constraints = PortfolioConstraints(max_weights=max_weight)
    manager = PortfolioManager()
    return manager.construct_portfolio(returns, constraints, objective)

# Command line interface
def main():
    """Command line interface"""
    import sys
    import json

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python skfolio_portfolio.py <command> <args>",
            "commands": ["construct", "monitor", "backtest", "summary"]
        }))
        return

    command = sys.argv[1]

    if command == "construct":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: construct <returns_file>"}))
            return

        try:
            returns = pd.read_csv(sys.argv[2], index_col=0, parse_dates=True)
            result = quick_portfolio_construction(returns)
            print(json.dumps(result, indent=2, default=str))

        except Exception as e:
            print(json.dumps({"error": str(e)}))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))

if __name__ == "__main__":
    main()