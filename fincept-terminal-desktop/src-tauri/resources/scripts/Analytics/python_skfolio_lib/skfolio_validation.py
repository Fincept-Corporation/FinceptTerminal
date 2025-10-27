"""
skfolio Model Validation & Testing
================================

This module provides comprehensive model validation and testing capabilities
for portfolio optimization models. It includes various cross-validation strategies,
model selection methods, performance evaluation, and statistical significance testing.

Key Features:
- Multiple cross-validation strategies (Walk Forward, Combinatorial Purged, etc.)
- Model selection and comparison
- Hyperparameter tuning with grid/random search
- Performance evaluation metrics
- Statistical significance testing
- Out-of-sample validation
- Model robustness analysis
- Overfitting detection

Usage:
    from skfolio_validation import ModelValidator

    validator = ModelValidator()
    cv_results = validator.walk_forward_validation(returns, models)
    best_model = validator.select_best_model(models, cv_results)
"""

import numpy as np
import pandas as pd
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from scipy import stats
from sklearn.model_selection import GridSearchCV, RandomizedSearchCV, KFold
from sklearn.metrics import mean_squared_error, mean_absolute_error
import json
import logging

# skfolio imports
from skfolio.model_selection import (
    CombinatorialPurgedCV, WalkForward, cross_val_predict
)
from skfolio.portfolio import Portfolio
from skfolio import RiskMeasure, RatioMeasure

warnings.filterwarnings('ignore')
logger = logging.getLogger(__name__)

@dataclass
class CrossValidationConfig:
    """Cross-validation configuration"""

    cv_method: str = "walk_forward"  # "walk_forward", "combinatorial_purged", "kfold", "time_series"
    n_folds: int = 5
    train_size: Optional[int] = None  # For walk-forward
    test_size: Optional[int] = None   # For walk-forward
    n_test_folds: int = 2  # For combinatorial purged
    purge_length: int = 10  # For combinatorial purged
    embargo_length: int = 5  # For combinatorial purged
    gap: int = 1  # Gap between train and test

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "cv_method": self.cv_method,
            "n_folds": self.n_folds,
            "train_size": self.train_size,
            "test_size": self.test_size,
            "n_test_folds": self.n_test_folds,
            "purge_length": self.purge_length,
            "embargo_length": self.embargo_length,
            "gap": self.gap
        }

@dataclass
class ValidationResults:
    """Cross-validation results"""

    model_name: str
    cv_method: str
    mean_score: float
    std_score: float
    scores: List[float]
    fold_results: List[Dict]
    training_time: float
    prediction_time: float

    # Performance metrics
    mean_sharpe: float
    std_sharpe: float
    mean_return: float
    std_return: float
    mean_volatility: float
    std_volatility: float
    mean_max_drawdown: float
    std_max_drawdown: float

    # Statistical significance
    p_value: Optional[float] = None
    confidence_interval: Optional[Tuple[float, float]] = None
    significance_test: Optional[str] = None

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "model_name": self.model_name,
            "cv_method": self.cv_method,
            "scores": {
                "mean": self.mean_score,
                "std": self.std_score,
                "values": self.scores
            },
            "performance": {
                "sharpe": {"mean": self.mean_sharpe, "std": self.std_sharpe},
                "return": {"mean": self.mean_return, "std": self.std_return},
                "volatility": {"mean": self.mean_volatility, "std": self.std_volatility},
                "max_drawdown": {"mean": self.mean_max_drawdown, "std": self.std_max_drawdown}
            },
            "timing": {
                "training_time": self.training_time,
                "prediction_time": self.prediction_time
            },
            "significance": {
                "p_value": self.p_value,
                "confidence_interval": self.confidence_interval,
                "test": self.significance_test
            }
        }

@dataclass
class ModelComparison:
    """Model comparison results"""

    model_results: Dict[str, ValidationResults]
    best_model: str
    ranking: List[Tuple[str, float]]
    statistical_tests: Dict[str, Dict[str, Any]]
    practical_significance: Dict[str, Dict[str, Any]]

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "model_results": {name: results.to_dict() for name, results in self.model_results.items()},
            "best_model": self.best_model,
            "ranking": self.ranking,
            "statistical_tests": self.statistical_tests,
            "practical_significance": self.practical_significance
        }

class ModelValidator:
    """
    Comprehensive model validation and testing system

    Provides advanced cross-validation strategies, model selection,
    and performance evaluation for portfolio optimization models.
    """

    def __init__(self, significance_level: float = 0.05):
        """
        Initialize model validator

        Parameters:
        -----------
        significance_level : float
            Significance level for statistical tests
        """

        self.significance_level = significance_level
        self.validation_history = {}

        # Performance metrics to track
        self.performance_metrics = [
            "sharpe_ratio", "annual_return", "volatility", "max_drawdown",
            "calmar_ratio", "sortino_ratio", "var_95", "cvar_95"
        ]

        logger.info("ModelValidator initialized")

    def walk_forward_validation(self,
                               returns: pd.DataFrame,
                               models: Dict[str, Any],
                               train_size: int = 252,
                               test_size: int = 63,
                               step_size: Optional[int] = None,
                               factor_returns: Optional[pd.DataFrame] = None,
                               progress_callback: Optional[Callable] = None) -> Dict[str, ValidationResults]:
        """
        Perform walk-forward validation

        Parameters:
        -----------
        returns : pd.DataFrame
            Asset returns data
        models : Dict[str, Any]
            Models to validate
        train_size : int
            Training window size
        test_size : int
            Test window size
        step_size : int, optional
            Step size between windows
        factor_returns : pd.DataFrame, optional
            Factor returns for factor models
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with validation results for each model
        """

        try:
            if progress_callback:
                progress_callback("Setting up walk-forward validation...")

            # Create walk-forward cross-validator
            cv = WalkForward(train_size=train_size, test_size=test_size)

            validation_results = {}
            total_models = len(models)

            for i, (model_name, model) in enumerate(models.items()):
                if progress_callback:
                    progress_callback(f"Validating model {i+1}/{total_models}: {model_name}")

                # Perform cross-validation
                start_time = datetime.now()

                if factor_returns is not None:
                    cv_scores = cross_val_predict(
                        model, returns, factor_returns, cv=cv
                    )
                else:
                    cv_scores = cross_val_predict(
                        model, returns, cv=cv
                    )

                training_time = (datetime.now() - start_time).total_seconds()

                # Extract performance metrics
                performance_metrics = self._extract_walk_forward_metrics(cv_scores)

                # Calculate validation results
                mean_score = np.mean([self._get_portfolio_score(score) for score in cv_scores])
                std_score = np.std([self._get_portfolio_score(score) for score in cv_scores])

                results = ValidationResults(
                    model_name=model_name,
                    cv_method="walk_forward",
                    mean_score=mean_score,
                    std_score=std_score,
                    scores=[self._get_portfolio_score(score) for score in cv_scores],
                    fold_results=[self._portfolio_to_dict(score) for score in cv_scores],
                    training_time=training_time,
                    prediction_time=0,  # Would need to measure separately
                    **performance_metrics
                )

                validation_results[model_name] = results

            return validation_results

        except Exception as e:
            logger.error(f"Walk-forward validation failed: {e}")
            raise

    def combinatorial_purged_validation(self,
                                        returns: pd.DataFrame,
                                        models: Dict[str, Any],
                                        n_folds: int = 10,
                                        n_test_folds: int = 2,
                                        purge_length: int = 10,
                                        embargo_length: int = 5,
                                        factor_returns: Optional[pd.DataFrame] = None,
                                        progress_callback: Optional[Callable] = None) -> Dict[str, ValidationResults]:
        """
        Perform combinatorial purged cross-validation

        Parameters:
        -----------
        returns : pd.DataFrame
            Asset returns data
        models : Dict[str, Any]
            Models to validate
        n_folds : int
            Number of folds
        n_test_folds : int
            Number of test folds
        purge_length : int
            Purge length
        embargo_length : int
            Embargo length
        factor_returns : pd.DataFrame, optional
            Factor returns
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with validation results for each model
        """

        try:
            if progress_callback:
                progress_callback("Setting up combinatorial purged validation...")

            # Create combinatorial purged cross-validator
            cv = CombinatorialPurgedCV(
                n_folds=n_folds,
                n_test_folds=n_test_folds,
                purge_length=purge_length,
                embargo_length=embargo_length
            )

            validation_results = {}
            total_models = len(models)

            for i, (model_name, model) in enumerate(models.items()):
                if progress_callback:
                    progress_callback(f"Validating model {i+1}/{total_models}: {model_name}")

                # Perform cross-validation
                start_time = datetime.now()

                if factor_returns is not None:
                    cv_scores = cross_val_predict(
                        model, returns, factor_returns, cv=cv
                    )
                else:
                    cv_scores = cross_val_predict(
                        model, returns, cv=cv
                    )

                training_time = (datetime.now() - start_time).total_seconds()

                # Extract performance metrics
                performance_metrics = self._extract_cv_metrics(cv_scores)

                # Calculate validation results
                mean_score = np.mean([self._get_portfolio_score(score) for score in cv_scores])
                std_score = np.std([self._get_portfolio_score(score) for score in cv_scores])

                results = ValidationResults(
                    model_name=model_name,
                    cv_method="combinatorial_purged",
                    mean_score=mean_score,
                    std_score=std_score,
                    scores=[self._get_portfolio_score(score) for score in cv_scores],
                    fold_results=[self._portfolio_to_dict(score) for score in cv_scores],
                    training_time=training_time,
                    prediction_time=0,
                    **performance_metrics
                )

                validation_results[model_name] = results

            return validation_results

        except Exception as e:
            logger.error(f"Combinatorial purged validation failed: {e}")
            raise

    def kfold_validation(self,
                       returns: pd.DataFrame,
                       models: Dict[str, Any],
                       n_folds: int = 5,
                       shuffle: bool = False,
                       factor_returns: Optional[pd.DataFrame] = None,
                       progress_callback: Optional[Callable] = None) -> Dict[str, ValidationResults]:
        """
        Perform K-fold cross-validation

        Parameters:
        -----------
        returns : pd.DataFrame
            Asset returns data
        models : Dict[str, Any]
            Models to validate
        n_folds : int
            Number of folds
        shuffle : bool
            Whether to shuffle data
        factor_returns : pd.DataFrame, optional
            Factor returns
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with validation results for each model
        """

        try:
            if progress_callback:
                progress_callback("Setting up K-fold validation...")

            # Create K-fold cross-validator
            cv = KFold(n_splits=n_folds, shuffle=shuffle)

            validation_results = {}
            total_models = len(models)

            for i, (model_name, model) in enumerate(models.items()):
                if progress_callback:
                    progress_callback(f"Validating model {i+1}/{total_models}: {model_name}")

                # Perform cross-validation
                start_time = datetime.now()

                if factor_returns is not None:
                    cv_scores = cross_val_predict(
                        model, returns, factor_returns, cv=cv
                    )
                else:
                    cv_scores = cross_val_predict(
                        model, returns, cv=cv
                    )

                training_time = (datetime.now() - start_time).total_seconds()

                # Extract performance metrics
                performance_metrics = self._extract_cv_metrics(cv_scores)

                # Calculate validation results
                mean_score = np.mean([self._get_portfolio_score(score) for score in cv_scores])
                std_score = np.std([self._get_portfolio_score(score) for score in cv_scores])

                results = ValidationResults(
                    model_name=model_name,
                    cv_method="kfold",
                    mean_score=mean_score,
                    std_score=std_score,
                    scores=[self._get_portfolio_score(score) for score in cv_scores],
                    fold_results=[self._portfolio_to_dict(score) for score in cv_scores],
                    training_time=training_time,
                    prediction_time=0,
                    **performance_metrics
                )

                validation_results[model_name] = results

            return validation_results

        except Exception as e:
            logger.error(f"K-fold validation failed: {e}")
            raise

    def hyperparameter_tuning(self,
                             base_model: Any,
                             param_grid: Dict[str, List[Any]],
                             returns: pd.DataFrame,
                             cv_method: str = "walk_forward",
                             scoring: str = "sharpe_ratio",
                             search_method: str = "grid",
                             n_iter: int = 50,
                             factor_returns: Optional[pd.DataFrame] = None,
                             progress_callback: Optional[Callable] = None) -> Dict:
        """
        Perform hyperparameter tuning

        Parameters:
        -----------
        base_model : Any
            Base model for tuning
        param_grid : Dict[str, List[Any]]
            Parameter grid
        returns : pd.DataFrame
            Asset returns data
        cv_method : str
            Cross-validation method
        scoring : str
            Scoring metric
        search_method : str
            Search method ("grid" or "random")
        n_iter : int
            Number of iterations for random search
        factor_returns : pd.DataFrame, optional
            Factor returns
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with tuning results
        """

        try:
            if progress_callback:
                progress_callback("Setting up hyperparameter tuning...")

            # Create cross-validator
            if cv_method == "walk_forward":
                cv = WalkForward(train_size=252, test_size=63)
            elif cv_method == "combinatorial_purged":
                cv = CombinatorialPurgedCV(n_folds=10, n_test_folds=2)
            else:
                cv = KFold(n_splits=5, shuffle=False)

            # Create search object
            if search_method == "grid":
                search = GridSearchCV(
                    estimator=base_model,
                    param_grid=param_grid,
                    cv=cv,
                    scoring=self._get_scoring_function(scoring),
                    n_jobs=-1,
                    verbose=1
                )
            else:  # random search
                search = RandomizedSearchCV(
                    estimator=base_model,
                    param_distributions=param_grid,
                    n_iter=n_iter,
                    cv=cv,
                    scoring=self._get_scoring_function(scoring),
                    n_jobs=-1,
                    verbose=1,
                    random_state=42
                )

            if progress_callback:
                progress_callback("Running hyperparameter search...")

            # Fit search
            start_time = datetime.now()
            if factor_returns is not None:
                search.fit(returns, factor_returns)
            else:
                search.fit(returns)
            search_time = (datetime.now() - start_time).total_seconds()

            # Analyze results
            cv_results = search.cv_results_
            best_params = search.best_params_
            best_score = search.best_score_

            # Statistical analysis of results
            scores = cv_results["mean_test_score"]
            score_std = np.std(scores)
            confidence_interval = stats.t.interval(0.95, len(scores)-1,
                                              loc=np.mean(scores),
                                              scale=stats.sem(scores))

            return {
                "best_params": best_params,
                "best_score": best_score,
                "best_model": search.best_estimator_,
                "search_method": search_method,
                "cv_method": cv_method,
                "scoring": scoring,
                "search_time": search_time,
                "n_candidates": len(cv_results["params"]),
                "statistical_analysis": {
                    "all_scores": scores.tolist(),
                    "mean_score": float(np.mean(scores)),
                    "std_score": float(score_std),
                    "confidence_interval": (float(confidence_interval[0]), float(confidence_interval[1])),
                    "score_distribution": {
                        "min": float(np.min(scores)),
                        "max": float(np.max(scores)),
                        "q25": float(np.percentile(scores, 25)),
                        "median": float(np.median(scores)),
                        "q75": float(np.percentile(scores, 75))
                    }
                },
                "cv_results": cv_results
            }

        except Exception as e:
            logger.error(f"Hyperparameter tuning failed: {e}")
            raise

    def compare_models(self,
                      validation_results: Dict[str, ValidationResults],
                      primary_metric: str = "mean_score",
                      statistical_test: str = "t_test") -> ModelComparison:
        """
        Compare multiple models using validation results

        Parameters:
        -----------
        validation_results : Dict[str, ValidationResults]
            Validation results for each model
        primary_metric : str
            Primary metric for comparison
        statistical_test : str
            Statistical test for significance

        Returns:
        --------
        ModelComparison object
        """

        try:
            # Create ranking
            ranking = sorted(
                [(name, getattr(results, primary_metric))
                 for name, results in validation_results.items()],
                key=lambda x: x[1],
                reverse=True
            )

            best_model = ranking[0][0]

            # Perform statistical tests
            statistical_tests = {}
            if len(validation_results) > 1:
                for i, (model1_name, results1) in enumerate(validation_results.items()):
                    for j, (model2_name, results2) in enumerate(validation_results.items()):
                        if i < j:  # Avoid duplicate comparisons
                            test_result = self._perform_statistical_test(
                                results1.scores, results2.scores, statistical_test
                            )
                            statistical_tests[f"{model1_name}_vs_{model2_name}"] = test_result

            # Practical significance analysis
            practical_significance = self._analyze_practical_significance(validation_results)

            return ModelComparison(
                model_results=validation_results,
                best_model=best_model,
                ranking=ranking,
                statistical_tests=statistical_tests,
                practical_significance=practical_significance
            )

        except Exception as e:
            logger.error(f"Model comparison failed: {e}")
            raise

    def _perform_statistical_test(self,
                                scores1: List[float],
                                scores2: List[float],
                                test_type: str) -> Dict:
        """Perform statistical test between two sets of scores"""

        try:
            if test_type == "t_test":
                # Paired t-test
                statistic, p_value = stats.ttest_rel(scores1, scores2)
                test_name = "Paired t-test"
            elif test_type == "wilcoxon":
                # Wilcoxon signed-rank test
                statistic, p_value = stats.wilcoxon(scores1, scores2)
                test_name = "Wilcoxon signed-rank test"
            elif test_type == "mannwhitney":
                # Mann-Whitney U test
                statistic, p_value = stats.mannwhitneyu(scores1, scores2, alternative='two-sided')
                test_name = "Mann-Whitney U test"
            else:
                raise ValueError(f"Unknown test type: {test_type}")

            # Calculate effect size (Cohen's d)
            mean_diff = np.mean(scores1) - np.mean(scores2)
            pooled_std = np.sqrt(((len(scores1) - 1) * np.var(scores1, ddof=1) +
                                (len(scores2) - 1) * np.var(scores2, ddof=1)) /
                               (len(scores1) + len(scores2) - 2))
            cohens_d = mean_diff / pooled_std if pooled_std > 0 else 0

            # Interpret effect size
            if abs(cohens_d) < 0.2:
                effect_size_interpretation = "small"
            elif abs(cohens_d) < 0.5:
                effect_size_interpretation = "medium"
            else:
                effect_size_interpretation = "large"

            return {
                "test": test_name,
                "statistic": float(statistic),
                "p_value": float(p_value),
                "significant": p_value < self.significance_level,
                "effect_size": float(cohens_d),
                "effect_size_interpretation": effect_size_interpretation,
                "mean_difference": float(mean_diff),
                "confidence_level": 1 - self.significance_level
            }

        except Exception as e:
            logger.error(f"Statistical test failed: {e}")
            return {"error": str(e)}

    def _analyze_practical_significance(self, validation_results: Dict[str, ValidationResults]) -> Dict:
        """Analyze practical significance of model differences"""

        practical_significance = {}

        # Extract key metrics
        sharpe_ratios = {name: results.mean_sharpe for name, results in validation_results.items()}
        returns = {name: results.mean_return for name, results in validation_results.items()}
        volatilities = {name: results.mean_volatility for name, results in validation_results.items()}

        # Calculate practical differences
        if len(sharpe_ratios) > 1:
            max_sharpe = max(sharpe_ratios.values())
            min_sharpe = min(sharpe_ratios.values())
            sharpe_difference = max_sharpe - min_sharpe

            practical_significance["sharpe_ratio"] = {
                "range": sharpe_difference,
                "relative_improvement": (sharpe_difference / min_sharpe) if min_sharpe != 0 else float('inf'),
                "practical_significance": "high" if sharpe_difference > 0.5 else "medium" if sharpe_difference > 0.2 else "low"
            }

        if len(returns) > 1:
            max_return = max(returns.values())
            min_return = min(returns.values())
            return_difference = max_return - min_return

            practical_significance["annual_return"] = {
                "range": return_difference,
                "relative_improvement": (return_difference / abs(min_return)) if min_return != 0 else float('inf'),
                "practical_significance": "high" if return_difference > 0.05 else "medium" if return_difference > 0.02 else "low"
            }

        return practical_significance

    def out_of_sample_test(self,
                          model: Any,
                          train_data: pd.DataFrame,
                          test_data: pd.DataFrame,
                          factor_train: Optional[pd.DataFrame] = None,
                          factor_test: Optional[pd.DataFrame] = None,
                          progress_callback: Optional[Callable] = None) -> Dict:
        """
        Perform out-of-sample testing

        Parameters:
        -----------
        model : Any
            Model to test
        train_data : pd.DataFrame
            Training data
        test_data : pd.DataFrame
            Test data
        factor_train : pd.DataFrame, optional
            Training factor data
        factor_test : pd.DataFrame, optional
            Test factor data
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with out-of-sample test results
        """

        try:
            if progress_callback:
                progress_callback("Training model on training data...")

            # Train model
            start_time = datetime.now()
            if factor_train is not None:
                model.fit(train_data, factor_train)
            else:
                model.fit(train_data)
            training_time = (datetime.now() - start_time).total_seconds()

            if progress_callback:
                progress_callback("Generating out-of-sample predictions...")

            # Generate predictions
            start_time = datetime.now()
            if factor_test is not None:
                portfolio = model.predict(test_data, factor_test)
            else:
                portfolio = model.predict(test_data)
            prediction_time = (datetime.now() - start_time).total_seconds()

            # Extract performance metrics
            performance_metrics = self._extract_portfolio_metrics(portfolio)

            # Calculate in-sample vs out-of-sample comparison
            train_portfolio = model.predict(train_data)
            train_metrics = self._extract_portfolio_metrics(train_portfolio)

            # Calculate overfitting indicators
            overfitting_analysis = self._analyze_overfitting(train_metrics, performance_metrics)

            return {
                "status": "success",
                "training_time": training_time,
                "prediction_time": prediction_time,
                "out_of_sample_performance": performance_metrics,
                "in_sample_performance": train_metrics,
                "overfitting_analysis": overfitting_analysis,
                "generalization_gap": {
                    "sharpe_gap": train_metrics["sharpe_ratio"] - performance_metrics["sharpe_ratio"],
                    "return_gap": train_metrics["annual_return"] - performance_metrics["annual_return"],
                    "volatility_gap": performance_metrics["annual_volatility"] - train_metrics["annual_volatility"]
                }
            }

        except Exception as e:
            logger.error(f"Out-of-sample test failed: {e}")
            return {
                "status": "error",
                "message": str(e)
            }

    def _analyze_overfitting(self, train_metrics: Dict, test_metrics: Dict) -> Dict:
        """Analyze overfitting indicators"""

        # Calculate performance degradation
        sharpe_degradation = (train_metrics["sharpe_ratio"] - test_metrics["sharpe_ratio"]) / train_metrics["sharpe_ratio"]
        return_degradation = (train_metrics["annual_return"] - test_metrics["annual_return"]) / abs(train_metrics["annual_return"]) if train_metrics["annual_return"] != 0 else 0
        volatility_increase = (test_metrics["annual_volatility"] - train_metrics["annual_volatility"]) / train_metrics["annual_volatility"]

        # Determine overfitting level
        overfitting_score = (sharpe_degradation + return_degradation + volatility_increase) / 3

        if overfitting_score > 0.3:
            overfitting_level = "severe"
        elif overfitting_score > 0.15:
            overfitting_level = "moderate"
        elif overfitting_score > 0.05:
            overfitting_level = "mild"
        else:
            overfitting_level = "minimal"

        return {
            "overfitting_score": overfitting_score,
            "overfitting_level": overfitting_level,
            "sharpe_degradation": sharpe_degradation,
            "return_degradation": return_degradation,
            "volatility_increase": volatility_increase,
            "recommendations": self._get_overfitting_recommendations(overfitting_level)
        }

    def _get_overfitting_recommendations(self, overfitting_level: str) -> List[str]:
        """Get recommendations based on overfitting level"""

        recommendations = {
            "severe": [
                "Use more regularization (increase L1/L2 coefficients)",
                "Reduce model complexity",
                "Use cross-validation to select parameters",
                "Increase training data size",
                "Apply ensemble methods"
            ],
            "moderate": [
                "Consider adding regularization",
                "Validate with out-of-sample testing",
                "Monitor performance degradation"
            ],
            "mild": [
                "Continue monitoring",
                "Consider slight regularization"
            ],
            "minimal": [
                "Model appears well-generalized"
            ]
        }

        return recommendations.get(overfitting_level, [])

    def _get_portfolio_score(self, portfolio: Portfolio) -> float:
        """Get a single score from portfolio for ranking"""

        # Use Sharpe ratio as default score
        return getattr(portfolio, 'sharpe_ratio', 0)

    def _extract_walk_forward_metrics(self, cv_scores: List[Portfolio]) -> Dict:
        """Extract metrics from walk-forward CV results"""

        sharpe_ratios = [getattr(score, 'sharpe_ratio', 0) for score in cv_scores]
        returns = [getattr(score, 'annualized_mean', 0) * 252 for score in cv_scores]
        volatilities = [getattr(score, 'annualized_volatility', 0) for score in cv_scores]
        max_drawdowns = [getattr(score, 'max_drawdown', 0) for score in cv_scores]

        return {
            "mean_sharpe": np.mean(sharpe_ratios),
            "std_sharpe": np.std(sharpe_ratios),
            "mean_return": np.mean(returns),
            "std_return": np.std(returns),
            "mean_volatility": np.mean(volatilities),
            "std_volatility": np.std(volatilities),
            "mean_max_drawdown": np.mean(max_drawdowns),
            "std_max_drawdown": np.std(max_drawdowns)
        }

    def _extract_cv_metrics(self, cv_scores: List[Portfolio]) -> Dict:
        """Extract metrics from CV results"""

        return self._extract_walk_forward_metrics(cv_scores)

    def _extract_portfolio_metrics(self, portfolio: Portfolio) -> Dict:
        """Extract metrics from single portfolio"""

        return {
            "sharpe_ratio": getattr(portfolio, 'sharpe_ratio', 0),
            "annual_return": getattr(portfolio, 'annualized_mean', 0) * 252,
            "annual_volatility": getattr(portfolio, 'annualized_volatility', 0),
            "max_drawdown": getattr(portfolio, 'max_drawdown', 0),
            "calmar_ratio": getattr(portfolio, 'calmar_ratio', 0),
            "sortino_ratio": getattr(portfolio, 'sortino_ratio', 0)
        }

    def _portfolio_to_dict(self, portfolio: Portfolio) -> Dict:
        """Convert portfolio to dictionary"""

        return {
            "sharpe_ratio": getattr(portfolio, 'sharpe_ratio', 0),
            "annual_return": getattr(portfolio, 'annualized_mean', 0) * 252,
            "annual_volatility": getattr(portfolio, 'annualized_volatility', 0),
            "max_drawdown": getattr(portfolio, 'max_drawdown', 0),
            "weights": getattr(portfolio, 'weights_', np.array([])).tolist()
        }

    def _get_scoring_function(self, scoring: str):
        """Get scoring function for cross-validation"""

        # This would need to be implemented based on skfolio's scoring capabilities
        # For now, return None to use default
        return None

    def generate_validation_report(self, comparison: ModelComparison) -> str:
        """Generate comprehensive validation report"""

        report = []
        report.append("=" * 80)
        report.append("MODEL VALIDATION REPORT")
        report.append("=" * 80)
        report.append(f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("")

        # Model ranking
        report.append("MODEL RANKING")
        report.append("-" * 40)
        for i, (model_name, score) in enumerate(comparison.ranking, 1):
            report.append(f"{i}. {model_name}: {score:.4f}")
        report.append("")

        # Best model details
        report.append(f"BEST MODEL: {comparison.best_model}")
        report.append("-" * 40)
        best_results = comparison.model_results[comparison.best_model]
        report.append(f"Score: {best_results.mean_score:.4f} (±{best_results.std_score:.4f})")
        report.append(f"Sharpe Ratio: {best_results.mean_sharpe:.4f} (±{best_results.std_sharpe:.4f})")
        report.append(f"Annual Return: {best_results.mean_return:.2%}")
        report.append(f"Volatility: {best_results.mean_volatility:.2%}")
        report.append(f"Max Drawdown: {best_results.mean_max_drawdown:.2%}")
        report.append("")

        # Statistical significance
        if comparison.statistical_tests:
            report.append("STATISTICAL SIGNIFICANCE TESTS")
            report.append("-" * 40)
            for comparison_name, test_result in comparison.statistical_tests.items():
                if "error" not in test_result:
                    report.append(f"{comparison_name}:")
                    report.append(f"  {test_result['test']}: p-value = {test_result['p_value']:.4f}")
                    report.append(f"  Significant: {test_result['significant']}")
                    report.append(f"  Effect size: {test_result['effect_size']:.3f} ({test_result['effect_size_interpretation']})")
                    report.append("")

        # Practical significance
        if comparison.practical_significance:
            report.append("PRACTICAL SIGNIFICANCE")
            report.append("-" * 40)
            for metric, analysis in comparison.practical_significance.items():
                report.append(f"{metric.replace('_', ' ').title()}:")
                report.append(f"  Range: {analysis['range']:.4f}")
                report.append(f"  Relative Improvement: {analysis['relative_improvement']:.2%}")
                report.append(f"  Practical Significance: {analysis['practical_significance'].title()}")
                report.append("")

        return "\n".join(report)

# Convenience functions
def quick_model_validation(returns: pd.DataFrame,
                           models: Dict[str, Any],
                           cv_method: str = "walk_forward") -> Dict:
    """
    Quick model validation with default settings

    Parameters:
    -----------
    returns : pd.DataFrame
        Asset returns data
    models : Dict[str, Any]
        Models to validate
    cv_method : str
        Cross-validation method

    Returns:
    --------
    Dict with validation results
    """

    validator = ModelValidator()

    if cv_method == "walk_forward":
        return validator.walk_forward_validation(returns, models)
    elif cv_method == "combinatorial_purged":
        return validator.combinatorial_purged_validation(returns, models)
    else:
        return validator.kfold_validation(returns, models)

# Command line interface
def main():
    """Command line interface"""
    import sys
    import json

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python skfolio_validation.py <command> <args>",
            "commands": ["validate", "compare", "tune", "oos_test"]
        }))
        return

    command = sys.argv[1]

    if command == "validate":
        print(json.dumps({
            "message": "Model validation requires Python integration",
            "usage": "Use ModelValidator class methods for validation"
        }))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))

if __name__ == "__main__":
    main()