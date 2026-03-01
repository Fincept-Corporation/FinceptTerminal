// Library selection
export const LIBRARIES = [
  { value: 'pyportfolioopt', label: 'PyPortfolioOpt', description: 'Classical optimization' },
  { value: 'skfolio', label: 'skfolio', description: 'Modern ML-integrated optimization (2025)' },
];

// PyPortfolioOpt methods
export const PYPORTFOLIOOPT_METHODS = [
  { value: 'efficient_frontier', label: 'Efficient Frontier', description: 'Mean-variance optimization' },
  { value: 'hrp', label: 'HRP (Hierarchical Risk Parity)', description: 'Machine learning based' },
  { value: 'cla', label: 'CLA (Critical Line Algorithm)', description: 'Analytical solution' },
  { value: 'black_litterman', label: 'Black-Litterman', description: 'Views-based approach' },
  { value: 'efficient_semivariance', label: 'Efficient Semivariance', description: 'Downside risk focus' },
  { value: 'efficient_cvar', label: 'Efficient CVaR', description: 'Conditional Value at Risk' },
  { value: 'efficient_cdar', label: 'Efficient CDaR', description: 'Conditional Drawdown at Risk' },
];

// skfolio methods
export const SKFOLIO_METHODS = [
  { value: 'mean_risk', label: 'Mean-Risk', description: 'Mean-variance with advanced estimators' },
  { value: 'risk_parity', label: 'Risk Parity', description: 'Equal risk contribution' },
  { value: 'hrp', label: 'Hierarchical Risk Parity', description: 'Clustering-based allocation' },
  { value: 'max_div', label: 'Maximum Diversification', description: 'Maximize portfolio diversification' },
  { value: 'equal_weight', label: 'Equal Weight', description: '1/N allocation' },
  { value: 'inverse_vol', label: 'Inverse Volatility', description: 'Weight by inverse volatility' },
];

// PyPortfolioOpt objectives
export const PYPORTFOLIOOPT_OBJECTIVES = [
  { value: 'max_sharpe', label: 'Maximize Sharpe Ratio' },
  { value: 'min_volatility', label: 'Minimize Volatility' },
  { value: 'max_quadratic_utility', label: 'Maximize Quadratic Utility' },
  { value: 'efficient_risk', label: 'Target Risk Level' },
  { value: 'efficient_return', label: 'Target Return Level' },
];

// skfolio objectives
export const SKFOLIO_OBJECTIVES = [
  { value: 'minimize_risk', label: 'Minimize Risk' },
  { value: 'maximize_return', label: 'Maximize Return' },
  { value: 'maximize_ratio', label: 'Maximize Sharpe Ratio' },
  { value: 'maximize_utility', label: 'Maximize Utility' },
];

// skfolio risk measures
export const SKFOLIO_RISK_MEASURES = [
  { value: 'variance', label: 'Variance' },
  { value: 'semi_variance', label: 'Semi-Variance' },
  { value: 'cvar', label: 'CVaR (Conditional Value at Risk)' },
  { value: 'evar', label: 'EVaR (Entropic Value at Risk)' },
  { value: 'max_drawdown', label: 'Maximum Drawdown' },
  { value: 'cdar', label: 'CDaR (Conditional Drawdown at Risk)' },
  { value: 'ulcer_index', label: 'Ulcer Index' },
];

export const EXPECTED_RETURNS_METHODS = [
  { value: 'mean_historical_return', label: 'Mean Historical Return' },
  { value: 'ema_historical_return', label: 'EMA Historical Return' },
  { value: 'capm_return', label: 'CAPM Return' },
];

export const RISK_MODELS = [
  { value: 'sample_cov', label: 'Sample Covariance' },
  { value: 'semicovariance', label: 'Semicovariance' },
  { value: 'exp_cov', label: 'Exponential Covariance' },
  { value: 'shrunk_covariance', label: 'Shrunk Covariance' },
  { value: 'ledoit_wolf', label: 'Ledoit-Wolf' },
];

export interface OptimizationConfig {
  optimization_method: string;
  objective: string;
  expected_returns_method: string;
  risk_model_method: string;
  risk_free_rate: number;
  weight_bounds_min: number;
  weight_bounds_max: number;
  gamma: number;
  total_portfolio_value: number;
}

export interface OptimizationResults {
  weights?: Record<string, number>;
  performance_metrics?: {
    expected_annual_return: number;
    annual_volatility: number;
    sharpe_ratio: number;
    sortino_ratio?: number;
    calmar_ratio?: number;
    max_drawdown?: number;
  };
  discrete_allocation?: {
    allocation: Record<string, number>;
    leftover_cash: number;
    total_value: number;
  };
  efficient_frontier?: {
    returns: number[];
    volatility: number[];
    sharpe_ratios: number[];
  };
  backtest_results?: {
    annual_return: number;
    annual_volatility: number;
    sharpe_ratio: number;
    max_drawdown: number;
    calmar_ratio: number;
  };
}
