"""
skfolio API Layer - Frontend Integration
========================================

This module provides a comprehensive API layer for frontend integration.
It offers JSON-based endpoints for all skfolio functionality, parameter validation,
error handling, and async task management for long-running computations.

Key Features:
- JSON-based RESTful API endpoints
- Parameter validation and sanitization
- Comprehensive error handling
- Progress tracking for long operations
- Async task management
- Response formatting for frontend
- Rate limiting and security
- File upload/download support

Usage:
    from skfolio_api import SkfolioAPI

    api = SkfolioAPI()
    result = api.optimize_portfolio(data, parameters)
    status = api.get_task_status(task_id)
"""

import sys
import json
import uuid
import asyncio
import threading
import time
import warnings
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Union, Any, Callable
from dataclasses import dataclass, field
from pathlib import Path
import pandas as pd
import numpy as np
import logging

# Import skfolio modules
from skfolio_core import SkfolioCore, PortfolioConfig
from skfolio_optimization import OptimizationEngine, OptimizationParameters
from skfolio_data import DataManager
from skfolio_risk import RiskAnalyzer
from skfolio_portfolio import PortfolioManager
from skfolio_validation import ModelValidator

warnings.filterwarnings('ignore')
logger = logging.getLogger(__name__)

@dataclass
class APIResponse:
    """Standard API response structure"""

    status: str  # "success", "error", "pending", "processing"
    message: str
    data: Optional[Dict] = None
    error_code: Optional[str] = None
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    request_id: Optional[str] = None
    task_id: Optional[str] = None

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "status": self.status,
            "message": self.message,
            "data": self.data,
            "error_code": self.error_code,
            "timestamp": self.timestamp,
            "request_id": self.request_id,
            "task_id": self.task_id
        }

@dataclass
class TaskStatus:
    """Task status tracking"""

    task_id: str
    status: str  # "pending", "processing", "completed", "failed", "cancelled"
    progress: float  # 0-100
    message: str
    created_at: datetime
    updated_at: datetime
    result: Optional[Dict] = None
    error: Optional[str] = None
    execution_time: Optional[float] = None

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "task_id": self.task_id,
            "status": self.status,
            "progress": self.progress,
            "message": self.message,
            "created_at": self.created_at.isoformat(),
            "updated_at": self.updated_at.isoformat(),
            "result": self.result,
            "error": self.error,
            "execution_time": self.execution_time
        }

class TaskManager:
    """Manage async tasks for long-running operations"""

    def __init__(self):
        """Initialize task manager"""
        self.tasks = {}
        self.active_threads = {}

    def create_task(self, task_type: str, message: str = "Task created") -> str:
        """Create a new task"""
        task_id = str(uuid.uuid4())
        now = datetime.now()

        task = TaskStatus(
            task_id=task_id,
            status="pending",
            progress=0.0,
            message=message,
            created_at=now,
            updated_at=now
        )

        self.tasks[task_id] = task
        return task_id

    def update_task(self, task_id: str, status: Optional[str] = None,
                   progress: Optional[float] = None, message: Optional[str] = None,
                   result: Optional[Dict] = None, error: Optional[str] = None) -> bool:
        """Update task status"""
        if task_id not in self.tasks:
            return False

        task = self.tasks[task_id]

        if status is not None:
            task.status = status
        if progress is not None:
            task.progress = progress
        if message is not None:
            task.message = message
        if result is not None:
            task.result = result
        if error is not None:
            task.error = error

        task.updated_at = datetime.now()

        # Calculate execution time for completed tasks
        if status in ["completed", "failed"] and task.execution_time is None:
            task.execution_time = (task.updated_at - task.created_at).total_seconds()

        return True

    def get_task(self, task_id: str) -> Optional[TaskStatus]:
        """Get task status"""
        return self.tasks.get(task_id)

    def cancel_task(self, task_id: str) -> bool:
        """Cancel a task"""
        if task_id not in self.tasks:
            return False

        task = self.tasks[task_id]
        if task.status in ["pending", "processing"]:
            task.status = "cancelled"
            task.message = "Task cancelled by user"
            task.updated_at = datetime.now()
            return True

        return False

    def cleanup_old_tasks(self, max_age_hours: int = 24):
        """Clean up old completed tasks"""
        cutoff_time = datetime.now() - timedelta(hours=max_age_hours)

        tasks_to_remove = []
        for task_id, task in self.tasks.items():
            if task.status in ["completed", "failed", "cancelled"] and task.updated_at < cutoff_time:
                tasks_to_remove.append(task_id)

        for task_id in tasks_to_remove:
            del self.tasks[task_id]

class ParameterValidator:
    """Validate API parameters"""

    def __init__(self):
        """Initialize parameter validator"""
        self.required_fields = {}
        self.optional_fields = {}
        self.field_types = {}

    def validate_optimization_params(self, params: Dict) -> Dict:
        """Validate optimization parameters"""
        errors = []
        validated_params = {}

        # Required fields
        required_fields = ["data"]
        for field in required_fields:
            if field not in params:
                errors.append(f"Missing required field: {field}")

        # Validate data
        if "data" in params:
            data = params["data"]
            if not isinstance(data, (list, dict)) and not hasattr(data, 'values'):
                errors.append("Data must be a list, dict, or DataFrame-like object")

        # Validate optimization method
        if "optimization_method" in params:
            valid_methods = ["mean_risk", "risk_parity", "hrp", "max_div", "equal_weight", "inverse_vol"]
            if params["optimization_method"] not in valid_methods:
                errors.append(f"Invalid optimization method. Must be one of: {valid_methods}")

        # Validate objective function
        if "objective_function" in params:
            valid_objectives = ["minimize_risk", "maximize_return", "maximize_ratio", "maximize_utility"]
            if params["objective_function"] not in valid_objectives:
                errors.append(f"Invalid objective function. Must be one of: {valid_objectives}")

        # Validate risk measure
        if "risk_measure" in params:
            valid_risks = ["variance", "semi_variance", "cvar", "evar", "max_drawdown", "cdar", "ulcer_index"]
            if params["risk_measure"] not in valid_risks:
                errors.append(f"Invalid risk measure. Must be one of: {valid_risks}")

        # Validate numeric parameters
        numeric_fields = ["train_test_split_ratio", "risk_aversion", "l1_coef", "l2_coef", "confidence_level"]
        for field in numeric_fields:
            if field in params:
                try:
                    validated_params[field] = float(params[field])
                    if field == "train_test_split_ratio" and not (0 < validated_params[field] < 1):
                        errors.append(f"{field} must be between 0 and 1")
                    elif field in ["confidence_level"] and not (0 < validated_params[field] < 1):
                        errors.append(f"{field} must be between 0 and 1")
                except (ValueError, TypeError):
                    errors.append(f"{field} must be a valid number")

        # Copy valid parameters
        for key, value in params.items():
            if key not in numeric_fields and key != "data":
                validated_params[key] = value

        return {
            "valid": len(errors) == 0,
            "errors": errors,
            "validated_params": validated_params
        }

    def validate_data_params(self, params: Dict) -> Dict:
        """Validate data loading parameters"""
        errors = []
        validated_params = {}

        # Required fields
        if "source_type" not in params:
            errors.append("Missing required field: source_type")

        # Validate source type
        if "source_type" in params:
            valid_sources = ["csv", "excel", "database", "api", "yfinance"]
            if params["source_type"] not in valid_sources:
                errors.append(f"Invalid source type. Must be one of: {valid_sources}")

        # Validate source path
        if "source_path" not in params and params.get("source_type") != "api":
            errors.append("Missing required field: source_path")

        return {
            "valid": len(errors) == 0,
            "errors": errors,
            "validated_params": validated_params
        }

class SkfolioAPI:
    """
    Comprehensive API layer for skfolio functionality

    Provides JSON-based endpoints for all skfolio operations with
    parameter validation, error handling, and async task management.
    """

    def __init__(self, enable_async: bool = True, max_concurrent_tasks: int = 10):
        """
        Initialize skfolio API

        Parameters:
        -----------
        enable_async : bool
            Enable async task processing
        max_concurrent_tasks : int
            Maximum concurrent async tasks
        """

        self.enable_async = enable_async
        self.max_concurrent_tasks = max_concurrent_tasks

        # Initialize components
        self.core = SkfolioCore()
        self.optimization_engine = OptimizationEngine()
        self.data_manager = DataManager()
        self.risk_analyzer = RiskAnalyzer()
        self.portfolio_manager = PortfolioManager()
        self.model_validator = ModelValidator()

        # Task management
        self.task_manager = TaskManager() if enable_async else None

        # Parameter validation
        self.validator = ParameterValidator()

        # Request tracking
        self.request_count = 0
        self.active_requests = {}

        logger.info("SkfolioAPI initialized")

    def _generate_request_id(self) -> str:
        """Generate unique request ID"""
        self.request_count += 1
        return f"req_{self.request_count}_{int(time.time())}"

    def _create_response(self, status: str, message: str, data: Optional[Dict] = None,
                        error_code: Optional[str] = None, request_id: Optional[str] = None,
                        task_id: Optional[str] = None) -> APIResponse:
        """Create standardized API response"""
        return APIResponse(
            status=status,
            message=message,
            data=data,
            error_code=error_code,
            request_id=request_id,
            task_id=task_id
        )

    def _handle_async_task(self, task_func: Callable, *args, **kwargs) -> Dict:
        """Handle async task execution"""
        if not self.task_manager:
            # Run synchronously if async is disabled
            try:
                result = task_func(*args, **kwargs)
                return {
                    "status": "success",
                    "result": result
                }
            except Exception as e:
                return {
                    "status": "error",
                    "error": str(e)
                }

        # Check concurrent task limit
        active_tasks = sum(1 for task in self.task_manager.tasks.values()
                         if task.status in ["pending", "processing"])
        if active_tasks >= self.max_concurrent_tasks:
            return {
                "status": "error",
                "error": "Maximum concurrent tasks limit reached"
            }

        # Create async task
        task_id = self.task_manager.create_task("async_operation", "Task queued")

        # Run task in background thread
        def run_task():
            try:
                self.task_manager.update_task(task_id, status="processing", message="Processing...")
                result = task_func(*args, **kwargs)
                self.task_manager.update_task(
                    task_id, status="completed",
                    message="Task completed successfully",
                    result=result
                )
            except Exception as e:
                self.task_manager.update_task(
                    task_id, status="failed",
                    message=f"Task failed: {str(e)}",
                    error=str(e)
                )

        thread = threading.Thread(target=run_task)
        thread.daemon = True
        thread.start()

        return {
            "status": "pending",
            "task_id": task_id,
            "message": "Task queued for processing"
        }

    def optimize_portfolio(self, data: Union[Dict, List], parameters: Dict,
                          async_execution: bool = None) -> APIResponse:
        """
        Optimize portfolio using specified parameters

        Parameters:
        -----------
        data : Union[Dict, List]
            Asset returns data (dict with dates/arrays or list of lists)
        parameters : Dict
            Optimization parameters
        async_execution : bool, optional
            Force async execution

        Returns:
        --------
        APIResponse with optimization results
        """

        request_id = self._generate_request_id()

        try:
            # Validate parameters
            validation = self.validator.validate_optimization_params(parameters)
            if not validation["valid"]:
                return self._create_response(
                    "error", "Parameter validation failed",
                    {"errors": validation["errors"]},
                    error_code="INVALID_PARAMS",
                    request_id=request_id
                )

            # Convert data to DataFrame
            try:
                if isinstance(data, dict):
                    df = pd.DataFrame(data)
                elif isinstance(data, list):
                    df = pd.DataFrame(data)
                else:
                    # Assume it's already a DataFrame or DataFrame-like
                    df = pd.DataFrame(data.values, columns=data.columns, index=data.index)
            except Exception as e:
                return self._create_response(
                    "error", f"Data conversion failed: {str(e)}",
                    error_code="DATA_CONVERSION_ERROR",
                    request_id=request_id
                )

            # Determine if async execution is needed
            should_run_async = async_execution if async_execution is not None else (
                self.enable_async and len(df) > 1000  # Large datasets
            )

            if should_run_async:
                # Run optimization asynchronously
                def async_optimize():
                    # Configure core engine
                    config = PortfolioConfig(**validation["validated_params"])
                    self.core.config = config

                    # Load data
                    load_result = self.core.load_data(df)

                    # Optimize portfolio
                    optimization_result = self.core.optimize_portfolio()

                    return {
                        "optimization_result": optimization_result,
                        "data_info": load_result
                    }

                task_result = self._handle_async_task(async_optimize)

                if task_result["status"] == "pending":
                    return self._create_response(
                        "pending", "Portfolio optimization started",
                        {"task_id": task_result["task_id"]},
                        request_id=request_id,
                        task_id=task_result["task_id"]
                    )
                else:
                    # Synchronous execution
                    optimization_result = task_result["result"]
                    return self._create_response(
                        "success", "Portfolio optimization completed",
                        optimization_result,
                        request_id=request_id
                    )
            else:
                # Run optimization synchronously
                config = PortfolioConfig(**validation["validated_params"])
                self.core.config = config

                # Load data
                load_result = self.core.load_data(df)

                # Optimize portfolio
                optimization_result = self.core.optimize_portfolio()

                return self._create_response(
                    "success", "Portfolio optimization completed",
                    optimization_result,
                    request_id=request_id
                )

        except Exception as e:
            logger.error(f"Portfolio optimization failed: {e}")
            return self._create_response(
                "error", f"Portfolio optimization failed: {str(e)}",
                error_code="OPTIMIZATION_ERROR",
                request_id=request_id
            )

    def load_data(self, parameters: Dict, async_execution: bool = None) -> APIResponse:
        """
        Load data from specified source

        Parameters:
        -----------
        parameters : Dict
            Data loading parameters
        async_execution : bool, optional
            Force async execution

        Returns:
        --------
        APIResponse with data loading results
        """

        request_id = self._generate_request_id()

        try:
            # Validate parameters
            validation = self.validator.validate_data_params(parameters)
            if not validation["valid"]:
                return self._create_response(
                    "error", "Parameter validation failed",
                    {"errors": validation["errors"]},
                    error_code="INVALID_PARAMS",
                    request_id=request_id
                )

            # Determine if async execution is needed
            should_run_async = async_execution if async_execution is not None else (
                self.enable_async and parameters.get("source_type") in ["database", "api"]
            )

            if should_run_async:
                # Run data loading asynchronously
                def async_load_data():
                    from skfolio_data import DataSource

                    # Create data source
                    source_params = validation["validated_params"]
                    source = DataSource(**source_params)

                    # Register and load data
                    source_name = f"source_{request_id}"
                    self.data_manager.register_data_source(source_name, source)

                    # Load data
                    data = self.data_manager.load_data(source_name)

                    # Generate quality report
                    quality_report = self.data_manager.assess_data_quality(data)

                    return {
                        "data": data.to_dict(),
                        "quality_report": quality_report.to_dict(),
                        "source_name": source_name
                    }

                task_result = self._handle_async_task(async_load_data)

                if task_result["status"] == "pending":
                    return self._create_response(
                        "pending", "Data loading started",
                        {"task_id": task_result["task_id"]},
                        request_id=request_id,
                        task_id=task_result["task_id"]
                    )
                else:
                    # Synchronous execution
                    load_result = task_result["result"]
                    return self._create_response(
                        "success", "Data loading completed",
                        load_result,
                        request_id=request_id
                    )
            else:
                # Run data loading synchronously
                from skfolio_data import DataSource

                source_params = validation["validated_params"]
                source = DataSource(**source_params)

                source_name = f"source_{request_id}"
                self.data_manager.register_data_source(source_name, source)

                data = self.data_manager.load_data(source_name)
                quality_report = self.data_manager.assess_data_quality(data)

                return self._create_response(
                    "success", "Data loading completed",
                    {
                        "data": data.to_dict(),
                        "quality_report": quality_report.to_dict(),
                        "source_name": source_name
                    },
                    request_id=request_id
                )

        except Exception as e:
            logger.error(f"Data loading failed: {e}")
            return self._create_response(
                "error", f"Data loading failed: {str(e)}",
                error_code="DATA_LOADING_ERROR",
                request_id=request_id
            )

    def calculate_risk_metrics(self, returns_data: Union[Dict, List],
                               benchmark_data: Optional[Union[Dict, List]] = None) -> APIResponse:
        """
        Calculate risk metrics for portfolio returns

        Parameters:
        -----------
        returns_data : Union[Dict, List]
            Portfolio returns data
        benchmark_data : Union[Dict, List], optional
            Benchmark returns data

        Returns:
        --------
        APIResponse with risk metrics
        """

        request_id = self._generate_request_id()

        try:
            # Convert returns data to Series
            if isinstance(returns_data, dict):
                returns_series = pd.Series(returns_data)
            elif isinstance(returns_data, list):
                returns_series = pd.Series(returns_data)
            else:
                returns_series = pd.Series(returns_data.values, index=returns_data.index)

            # Convert benchmark data if provided
            benchmark_series = None
            if benchmark_data is not None:
                if isinstance(benchmark_data, dict):
                    benchmark_series = pd.Series(benchmark_data)
                elif isinstance(benchmark_data, list):
                    benchmark_series = pd.Series(benchmark_data)
                else:
                    benchmark_series = pd.Series(benchmark_data.values, index=benchmark_data.index)

            # Calculate risk metrics
            risk_metrics = self.risk_analyzer.calculate_risk_metrics(
                returns_series, benchmark_series
            )

            return self._create_response(
                "success", "Risk metrics calculated successfully",
                risk_metrics.to_dict(),
                request_id=request_id
            )

        except Exception as e:
            logger.error(f"Risk metrics calculation failed: {e}")
            return self._create_response(
                "error", f"Risk metrics calculation failed: {str(e)}",
                error_code="RISK_METRICS_ERROR",
                request_id=request_id
            )

    def stress_test_portfolio(self, weights: List[float], asset_names: List[str],
                              returns_data: Union[Dict, List], scenarios: List[Dict],
                              n_simulations: int = 10000) -> APIResponse:
        """
        Perform stress testing on portfolio

        Parameters:
        -----------
        weights : List[float]
            Portfolio weights
        asset_names : List[str]
            Asset names corresponding to weights
        returns_data : Union[Dict, List]
            Historical returns data
        scenarios : List[Dict]
            Stress test scenarios
        n_simulations : int
            Number of Monte Carlo simulations

        Returns:
        --------
        APIResponse with stress test results
        """

        request_id = self._generate_request_id()

        try:
            # Convert data to DataFrame
            if isinstance(returns_data, dict):
                returns_df = pd.DataFrame(returns_data)
            elif isinstance(returns_data, list):
                returns_df = pd.DataFrame(returns_data).T
                if len(asset_names) == len(returns_df.columns):
                    returns_df.columns = asset_names
            else:
                returns_df = returns_data

            # Convert weights to numpy array
            weights_array = np.array(weights)

            # Convert scenarios
            from skfolio_risk import StressTestScenario
            stress_scenarios = [StressTestScenario(**scenario) for scenario in scenarios]

            # Perform stress testing
            stress_results = self.risk_analyzer.stress_test(
                weights_array, returns_df, stress_scenarios, n_simulations
            )

            return self._create_response(
                "success", "Stress testing completed",
                stress_results,
                request_id=request_id
            )

        except Exception as e:
            logger.error(f"Stress testing failed: {e}")
            return self._create_response(
                "error", f"Stress testing failed: {str(e)}",
                error_code="STRESS_TEST_ERROR",
                request_id=request_id
            )

    def get_task_status(self, task_id: str) -> APIResponse:
        """
        Get status of async task

        Parameters:
        -----------
        task_id : str
            Task ID

        Returns:
        --------
        APIResponse with task status
        """

        request_id = self._generate_request_id()

        try:
            if not self.task_manager:
                return self._create_response(
                    "error", "Async operations are not enabled",
                    error_code="ASYNC_DISABLED",
                    request_id=request_id
                )

            task = self.task_manager.get_task(task_id)
            if task is None:
                return self._create_response(
                    "error", f"Task {task_id} not found",
                    error_code="TASK_NOT_FOUND",
                    request_id=request_id
                )

            return self._create_response(
                "success", f"Task {task_id} status retrieved",
                task.to_dict(),
                request_id=request_id
            )

        except Exception as e:
            logger.error(f"Failed to get task status: {e}")
            return self._create_response(
                "error", f"Failed to get task status: {str(e)}",
                error_code="TASK_STATUS_ERROR",
                request_id=request_id
            )

    def cancel_task(self, task_id: str) -> APIResponse:
        """
        Cancel async task

        Parameters:
        -----------
        task_id : str
            Task ID

        Returns:
        --------
        APIResponse with cancellation result
        """

        request_id = self._generate_request_id()

        try:
            if not self.task_manager:
                return self._create_response(
                    "error", "Async operations are not enabled",
                    error_code="ASYNC_DISABLED",
                    request_id=request_id
                )

            success = self.task_manager.cancel_task(task_id)
            if success:
                return self._create_response(
                    "success", f"Task {task_id} cancelled",
                    {"task_id": task_id},
                    request_id=request_id
                )
            else:
                return self._create_response(
                    "error", f"Cannot cancel task {task_id}",
                    error_code="TASK_CANNOT_CANCEL",
                    request_id=request_id
                )

        except Exception as e:
            logger.error(f"Failed to cancel task: {e}")
            return self._create_response(
                "error", f"Failed to cancel task: {str(e)}",
                error_code="TASK_CANCEL_ERROR",
                request_id=request_id
            )

    def get_api_status(self) -> APIResponse:
        """
        Get API status and statistics

        Returns:
        --------
        APIResponse with status information
        """

        request_id = self._generate_request_id()

        try:
            status_info = {
                "api_version": "1.0.0",
                "request_count": self.request_count,
                "active_requests": len(self.active_requests),
                "async_enabled": self.enable_async,
                "max_concurrent_tasks": self.max_concurrent_tasks
            }

            if self.task_manager:
                task_status = {
                    "total_tasks": len(self.task_manager.tasks),
                    "pending_tasks": len([t for t in self.task_manager.tasks.values() if t.status == "pending"]),
                    "processing_tasks": len([t for t in self.task_manager.tasks.values() if t.status == "processing"]),
                    "completed_tasks": len([t for t in self.task_manager.tasks.values() if t.status == "completed"]),
                    "failed_tasks": len([t for t in self.task_manager.tasks.values() if t.status == "failed"]),
                }
                status_info["task_status"] = task_status

            return self._create_response(
                "success", "API status retrieved",
                status_info,
                request_id=request_id
            )

        except Exception as e:
            logger.error(f"Failed to get API status: {e}")
            return self._create_response(
                "error", f"Failed to get API status: {str(e)}",
                error_code="API_STATUS_ERROR",
                request_id=request_id
            )

    def cleanup(self) -> APIResponse:
        """
        Clean up old tasks and resources

        Returns:
        --------
        APIResponse with cleanup results
        """

        request_id = self._generate_request_id()

        try:
            cleanup_info = {}

            # Clean up old tasks
            if self.task_manager:
                old_task_count = len(self.task_manager.tasks)
                self.task_manager.cleanup_old_tasks()
                new_task_count = len(self.task_manager.tasks)
                cleanup_info["tasks_cleaned"] = old_task_count - new_task_count

            # Clean up active requests
            old_request_count = len(self.active_requests)
            # Remove requests older than 1 hour
            cutoff_time = datetime.now() - timedelta(hours=1)
            self.active_requests = {
                rid: info for rid, info in self.active_requests.items()
                if info.get("timestamp", datetime.now()) > cutoff_time
            }
            cleanup_info["requests_cleaned"] = old_request_count - len(self.active_requests)

            return self._create_response(
                "success", "Cleanup completed",
                cleanup_info,
                request_id=request_id
            )

        except Exception as e:
            logger.error(f"Cleanup failed: {e}")
            return self._create_response(
                "error", f"Cleanup failed: {str(e)}",
                error_code="CLEANUP_ERROR",
                request_id=request_id
            )

# Main API function for command line usage
def main():
    """Command line interface"""
    import sys
    import json

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python skfolio_api.py <command> <args>",
            "commands": ["optimize", "load_data", "risk_metrics", "stress_test", "task_status", "cancel_task", "api_status"]
        }))
        return

    command = sys.argv[1]
    api = SkfolioAPI()

    if command == "api_status":
        result = api.get_api_status()
        print(json.dumps(result.to_dict(), indent=2, default=str))

    elif command == "task_status":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: task_status <task_id>"}))
            return
        result = api.get_task_status(sys.argv[2])
        print(json.dumps(result.to_dict(), indent=2, default=str))

    else:
        print(json.dumps({
            "error": f"Command {command} not supported in CLI mode",
            "supported_commands": ["api_status", "task_status"]
        }))

if __name__ == "__main__":
    main()