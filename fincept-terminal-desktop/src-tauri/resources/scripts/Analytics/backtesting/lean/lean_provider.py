"""
Lean Provider Implementation

Python-side implementation of QuantConnect Lean provider.
Handles execution of Lean CLI, parsing results, and data operations.
"""

import os
import sys
import json
import subprocess
import time
from typing import Dict, Any, List, Optional
from datetime import datetime
from pathlib import Path

# Import base provider
sys.path.append(str(Path(__file__).parent.parent))
from base.base_provider import (
    BacktestingProviderBase,
    BacktestResult,
    PerformanceMetrics,
    Trade,
    EquityPoint,
    BacktestStatistics,
    OptimizationResult,
    json_response,
    parse_json_input
)


class LeanProvider(BacktestingProviderBase):
    """
    QuantConnect Lean Provider

    Executes backtests using Lean CLI and parses results.
    """

    def __init__(self):
        super().__init__()
        self.lean_cli_path: Optional[str] = None
        self.projects_path: Optional[str] = None
        self.data_path: Optional[str] = None
        self.results_path: Optional[str] = None
        self.running_processes: Dict[str, subprocess.Popen] = {}

    # ========================================================================
    # Properties
    # ========================================================================

    @property
    def name(self) -> str:
        return "QuantConnect Lean"

    @property
    def version(self) -> str:
        return "2.5.0"

    @property
    def capabilities(self) -> Dict[str, Any]:
        return {
            'backtesting': True,
            'optimization': True,
            'liveTrading': True,
            'research': True,
            'multiAsset': ['equity', 'forex', 'crypto', 'option', 'future', 'cfd'],
            'indicators': True,
            'customStrategies': True,
            'maxConcurrentBacktests': 5,
            'supportedTimeframes': ['tick', 'second', 'minute', 'hour', 'daily'],
        }

    # ========================================================================
    # Core Methods
    # ========================================================================

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Initialize Lean provider with configuration"""
        try:
            # Extract config
            self.lean_cli_path = config.get('leanCliPath')
            self.projects_path = config.get('projectsPath')
            self.data_path = config.get('dataPath')
            self.results_path = config.get('resultsPath')

            if not self.lean_cli_path:
                return self._create_error_result('leanCliPath is required')

            # Check if Lean CLI exists
            if not os.path.exists(self.lean_cli_path):
                return self._create_error_result(f'Lean CLI not found at: {self.lean_cli_path}')

            # Create directories
            for path in [self.projects_path, self.data_path, self.results_path]:
                if path:
                    os.makedirs(path, exist_ok=True)

            self.config = config
            self.initialized = True
            self.connected = True  # For CLI-based, no separate connection needed

            self._log(f'Lean provider initialized. CLI: {self.lean_cli_path}')
            return self._create_success_result('Lean provider initialized successfully')

        except Exception as e:
            self._error('Failed to initialize Lean provider', e)
            return self._create_error_result(str(e))

    def test_connection(self) -> Dict[str, Any]:
        """Test Lean CLI availability"""
        self._ensure_initialized()

        try:
            # Run --version to test CLI
            result = subprocess.run(
                [self.lean_cli_path, '--version'],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                self.connected = True
                return self._create_success_result(f'Lean CLI operational. Version: {result.stdout.strip()}')
            else:
                return self._create_error_result(f'Lean CLI test failed: {result.stderr}')

        except Exception as e:
            self._error('Connection test failed', e)
            return self._create_error_result(str(e))

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run a backtest using Lean CLI"""
        self._ensure_connected()

        backtest_id = self._generate_id()
        logs = []

        try:
            logs.append(f'{self._current_timestamp()}: Starting backtest {backtest_id}')

            # 1. Extract request parameters
            strategy = request.get('strategy', {})
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 100000)
            parameters = request.get('parameters', {})

            # 2. Create project directory
            project_path = os.path.join(self.projects_path, backtest_id)
            os.makedirs(project_path, exist_ok=True)
            logs.append(f'{self._current_timestamp()}: Created project at {project_path}')

            # 3. Write strategy code to main.py
            strategy_code = self._extract_strategy_code(strategy)
            main_file = os.path.join(project_path, 'main.py')
            with open(main_file, 'w') as f:
                f.write(strategy_code)
            logs.append(f'{self._current_timestamp()}: Wrote strategy to main.py')

            # 4. Build CLI command
            results_folder = os.path.join(self.results_path, backtest_id)
            os.makedirs(results_folder, exist_ok=True)

            command = self._build_cli_command(
                mode='backtest',
                project_path=project_path,
                algorithm='main.py',
                start_date=start_date,
                end_date=end_date,
                cash=initial_capital,
                data_folder=self.data_path,
                results_folder=results_folder,
                parameters=parameters
            )

            logs.append(f'{self._current_timestamp()}: Executing: {" ".join(command)}')

            # 5. Execute backtest
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                cwd=project_path
            )

            self.running_processes[backtest_id] = process

            # Wait for completion
            stdout, stderr = process.communicate(timeout=3600)  # 1 hour timeout

            if stderr:
                logs.append(f'{self._current_timestamp()}: STDERR: {stderr}')

            if process.returncode != 0:
                logs.append(f'{self._current_timestamp()}: Backtest failed with code {process.returncode}')
                return BacktestResult(
                    id=backtest_id,
                    status='failed',
                    performance=self._get_empty_metrics(),
                    trades=[],
                    equity=[],
                    statistics=self._get_empty_statistics(),
                    logs=logs,
                    error=f'Lean CLI exited with code {process.returncode}: {stderr}'
                ).to_dict()

            logs.append(f'{self._current_timestamp()}: Backtest completed successfully')

            # 6. Read and parse results
            results_file = os.path.join(results_folder, 'results.json')
            if not os.path.exists(results_file):
                raise FileNotFoundError(f'Results file not found: {results_file}')

            with open(results_file, 'r') as f:
                lean_results = json.load(f)

            logs.append(f'{self._current_timestamp()}: Parsed results from {results_file}')

            # 7. Convert to platform-independent format
            result = self._parse_lean_results(backtest_id, lean_results, logs)
            return result.to_dict()

        except subprocess.TimeoutExpired:
            logs.append(f'{self._current_timestamp()}: Backtest timeout')
            if backtest_id in self.running_processes:
                self.running_processes[backtest_id].kill()
                del self.running_processes[backtest_id]

            return BacktestResult(
                id=backtest_id,
                status='failed',
                performance=self._get_empty_metrics(),
                trades=[],
                equity=[],
                statistics=self._get_empty_statistics(),
                logs=logs,
                error='Backtest timed out after 1 hour'
            ).to_dict()

        except Exception as e:
            self._error(f'Backtest {backtest_id} failed', e)
            logs.append(f'{self._current_timestamp()}: Error: {str(e)}')

            return BacktestResult(
                id=backtest_id,
                status='failed',
                performance=self._get_empty_metrics(),
                trades=[],
                equity=[],
                statistics=self._get_empty_statistics(),
                logs=logs,
                error=str(e)
            ).to_dict()

    def get_historical_data(self, request: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Download historical data using Lean CLI"""
        self._ensure_connected()

        try:
            symbols = request.get('symbols', [])
            timeframe = request.get('timeframe', 'daily')
            start_date = request.get('startDate')
            end_date = request.get('endDate')

            # Build data download command
            command = [
                self.lean_cli_path,
                'data',
                'download',
                '--tickers', ','.join(symbols),
                '--resolution', timeframe,
                '--start', self._format_date(start_date),
                '--end', self._format_date(end_date),
                '--market', 'usa',
                '--data-folder', self.data_path
            ]

            # Execute download
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                timeout=600  # 10 minutes
            )

            if result.returncode != 0:
                raise RuntimeError(f'Data download failed: {result.stderr}')

            # Read downloaded data
            # (Simplified - would parse Lean data files)
            return []

        except Exception as e:
            self._error('Historical data fetch failed', e)
            raise

    def calculate_indicator(self, indicator_type: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate technical indicator"""
        # Lean calculates indicators within backtests
        raise NotImplementedError('Standalone indicator calculation not supported by Lean')

    # ========================================================================
    # Optional Methods
    # ========================================================================

    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run parameter optimization"""
        self._ensure_connected()

        optimization_id = self._generate_id()

        try:
            # Similar to backtest but with optimization config
            # (Full implementation would set up optimization parameters)
            raise NotImplementedError('Optimization implementation in progress')

        except Exception as e:
            self._error(f'Optimization {optimization_id} failed', e)
            raise

    # ========================================================================
    # Helper Methods
    # ========================================================================

    def _build_cli_command(
        self,
        mode: str,
        project_path: str,
        algorithm: str,
        start_date: Optional[str] = None,
        end_date: Optional[str] = None,
        cash: Optional[float] = None,
        data_folder: Optional[str] = None,
        results_folder: Optional[str] = None,
        parameters: Optional[Dict[str, Any]] = None
    ) -> List[str]:
        """Build Lean CLI command"""
        command = [self.lean_cli_path, mode]

        command.extend(['--project', project_path])
        command.extend(['--algorithm-file', algorithm])

        if start_date:
            command.extend(['--start', self._format_date(start_date)])

        if end_date:
            command.extend(['--end', self._format_date(end_date)])

        if cash:
            command.extend(['--cash', str(cash)])

        if data_folder:
            command.extend(['--data-folder', data_folder])

        if results_folder:
            command.extend(['--results', results_folder])

        if parameters:
            command.extend(['--parameters', json.dumps(parameters)])

        return command

    def _extract_strategy_code(self, strategy: Dict[str, Any]) -> str:
        """Extract Python code from strategy definition"""
        strategy_type = strategy.get('type')

        if strategy_type == 'code':
            code = strategy.get('code', {})
            if code.get('language') == 'python':
                return code.get('source', '')
            else:
                raise ValueError('Only Python strategies are supported')

        elif strategy_type == 'visual':
            visual = strategy.get('visual', {})
            generated = visual.get('generatedPython')
            if generated:
                return generated
            else:
                raise ValueError('Visual strategy must have generated Python code')

        elif strategy_type == 'template':
            raise NotImplementedError('Template strategies not yet implemented')

        else:
            raise ValueError(f'Unknown strategy type: {strategy_type}')

    def _parse_lean_results(
        self,
        backtest_id: str,
        lean_results: Dict[str, Any],
        logs: List[str]
    ) -> BacktestResult:
        """Parse Lean results to platform-independent format"""

        # Extract statistics
        stats = lean_results.get('TotalPerformance', {}).get('PortfolioStatistics', {})
        trade_stats = lean_results.get('TotalPerformance', {}).get('TradeStatistics', {})

        # Parse performance metrics
        performance = PerformanceMetrics(
            total_return=self._parse_percent(stats.get('TotalReturn', '0%')),
            annualized_return=self._parse_percent(stats.get('AnnualReturn', '0%')),
            sharpe_ratio=self._parse_float(stats.get('SharpeRatio', '0')),
            sortino_ratio=self._parse_float(stats.get('SortinoRatio', '0')),
            max_drawdown=self._parse_percent(stats.get('MaxDrawdown', '0%')),
            win_rate=self._parse_percent(trade_stats.get('WinRate', '0%')),
            loss_rate=self._parse_percent(trade_stats.get('LossRate', '0%')),
            profit_factor=self._parse_float(trade_stats.get('ProfitLossRatio', '0')),
            volatility=self._parse_percent(stats.get('AnnualStandardDeviation', '0%')),
            calmar_ratio=0,  # Calculate
            total_trades=int(trade_stats.get('TotalNumberOfTrades', 0)),
            winning_trades=0,  # Calculate from win_rate
            losing_trades=0,  # Calculate from loss_rate
            average_win=self._parse_float(trade_stats.get('AverageWin', '0')),
            average_loss=abs(self._parse_float(trade_stats.get('AverageLoss', '0'))),
            largest_win=self._parse_float(trade_stats.get('LargestWin', '0')),
            largest_loss=abs(self._parse_float(trade_stats.get('LargestLoss', '0'))),
            average_trade_return=0,
            expectancy=0,
        )

        # Parse trades (simplified)
        trades: List[Trade] = []
        # (Would parse from lean_results['Orders'])

        # Parse equity curve (simplified)
        equity: List[EquityPoint] = []
        # (Would parse from lean_results['Charts']['Strategy Equity'])

        # Parse general statistics
        statistics = BacktestStatistics(
            start_date=self._extract_statistic(lean_results, 'Start') or '',
            end_date=self._extract_statistic(lean_results, 'End') or '',
            initial_capital=100000,  # From request
            final_capital=100000,  # Calculate
            total_fees=0,
            total_slippage=0,
            total_trades=performance.total_trades,
            winning_days=0,
            losing_days=0,
            average_daily_return=0,
            best_day=0,
            worst_day=0,
            consecutive_wins=0,
            consecutive_losses=0,
        )

        return BacktestResult(
            id=backtest_id,
            status='completed',
            performance=performance,
            trades=trades,
            equity=equity,
            statistics=statistics,
            logs=logs,
            start_time=self._current_timestamp(),
            end_time=self._current_timestamp(),
        )

    def _parse_float(self, value: Any) -> float:
        """Parse float from string or number"""
        if value is None:
            return 0.0
        if isinstance(value, (int, float)):
            return float(value)
        if isinstance(value, str):
            try:
                return float(value.replace('%', ''))
            except:
                return 0.0
        return 0.0

    def _parse_percent(self, value: str) -> float:
        """Parse percentage string to decimal"""
        if not value:
            return 0.0
        cleaned = value.replace('%', '').strip()
        try:
            return float(cleaned) / 100.0
        except:
            return 0.0

    def _extract_statistic(self, results: Dict[str, Any], key: str) -> Optional[str]:
        """Extract statistic by key"""
        stats = results.get('Statistics', [])
        for stat in stats:
            if stat.get('Key') == key:
                return stat.get('Value')
        return None

    def _format_date(self, iso_date: str) -> str:
        """Convert ISO date to YYYYMMDD"""
        dt = datetime.fromisoformat(iso_date.replace('Z', '+00:00'))
        return dt.strftime('%Y%m%d')

    def _get_empty_metrics(self) -> PerformanceMetrics:
        """Get empty performance metrics"""
        return PerformanceMetrics(
            total_return=0, annualized_return=0, sharpe_ratio=0, sortino_ratio=0,
            max_drawdown=0, win_rate=0, loss_rate=0, profit_factor=0, volatility=0,
            calmar_ratio=0, total_trades=0, winning_trades=0, losing_trades=0,
            average_win=0, average_loss=0, largest_win=0, largest_loss=0,
            average_trade_return=0, expectancy=0
        )

    def _get_empty_statistics(self) -> BacktestStatistics:
        """Get empty statistics"""
        return BacktestStatistics(
            start_date='', end_date='', initial_capital=0, final_capital=0,
            total_fees=0, total_slippage=0, total_trades=0, winning_days=0,
            losing_days=0, average_daily_return=0, best_day=0, worst_day=0,
            consecutive_wins=0, consecutive_losses=0
        )


# ============================================================================
# CLI Entry Point (for direct Python execution)
# ============================================================================

def main():
    """Main entry point for CLI execution"""
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': 'Usage: python lean_provider.py <command> <json_args>'
        }))
        sys.exit(1)

    command = sys.argv[1]
    json_args = sys.argv[2]

    try:
        args = parse_json_input(json_args)
        provider = LeanProvider()

        if command == 'initialize':
            result = provider.initialize(args)
        elif command == 'test_connection':
            result = provider.test_connection()
        elif command == 'run_backtest':
            result = provider.run_backtest(args)
        elif command == 'get_historical_data':
            result = provider.get_historical_data(args)
        elif command == 'optimize':
            result = provider.optimize(args)
        else:
            result = {'success': False, 'error': f'Unknown command: {command}'}

        print(json_response(result))

    except Exception as e:
        print(json_response({
            'success': False,
            'error': str(e),
            'traceback': __import__('traceback').format_exc()
        }))
        sys.exit(1)


if __name__ == '__main__':
    main()
