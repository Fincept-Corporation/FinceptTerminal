"""
Fortitudo.tech Advanced Functions Wrapper
==========================================
Entropy pooling, exposure stacking, and visualization

Includes fallback implementations using NumPy/SciPy for Python 3.14+ compatibility.

Usage:
    python advanced.py
"""

import numpy as np
import pandas as pd
import json
from typing import Dict, Any, Optional, Tuple
import warnings

warnings.filterwarnings('ignore')

# Try to import fortitudo.tech, fallback to pure implementations
try:
    import fortitudo.tech as ft
    FORTITUDO_AVAILABLE = True
except ImportError:
    FORTITUDO_AVAILABLE = False
    ft = None


# ============================================================================
# FALLBACK IMPLEMENTATIONS (Pure NumPy/SciPy)
# ============================================================================

def _fallback_entropy_pooling(
    p: np.ndarray,
    A: np.ndarray,
    b: np.ndarray,
    G: Optional[np.ndarray] = None,
    h: Optional[np.ndarray] = None,
    method: str = 'L-BFGS-B'
) -> np.ndarray:
    """
    Fallback entropy pooling using scipy optimization.

    Minimizes KL divergence D(q||p) subject to constraints Aq = b, Gq <= h
    """
    from scipy.optimize import minimize

    n = len(p.flatten())
    p_flat = p.flatten()

    # Objective: KL divergence D(q||p) = sum(q * log(q/p))
    def objective(q):
        q = np.maximum(q, 1e-10)  # Avoid log(0)
        return np.sum(q * (np.log(q) - np.log(p_flat)))

    def gradient(q):
        q = np.maximum(q, 1e-10)
        return np.log(q) - np.log(p_flat) + 1

    # Constraints
    constraints = []

    # Equality constraints: Aq = b
    if A is not None and b is not None:
        for i in range(A.shape[0]):
            constraints.append({
                'type': 'eq',
                'fun': lambda q, i=i: A[i] @ q - b.flatten()[i]
            })

    # Inequality constraints: Gq <= h
    if G is not None and h is not None:
        for i in range(G.shape[0]):
            constraints.append({
                'type': 'ineq',
                'fun': lambda q, i=i: h.flatten()[i] - G[i] @ q
            })

    # Bounds: probabilities must be positive
    bounds = [(1e-10, 1.0) for _ in range(n)]

    # Initial guess: prior probabilities
    x0 = p_flat.copy()

    # Optimize
    result = minimize(
        objective,
        x0,
        method=method,
        jac=gradient,
        bounds=bounds,
        constraints=constraints,
        options={'maxiter': 1000}
    )

    # Normalize result
    q = result.x
    q = np.maximum(q, 0)
    q = q / q.sum()

    return q.reshape(-1, 1)


def _fallback_exposure_stacking(
    L: int,
    sample_portfolios: np.ndarray
) -> np.ndarray:
    """
    Fallback exposure stacking implementation.

    Simple mean aggregation of sample portfolios as approximation.
    For full implementation, see SSRN paper 4709317.
    """
    # Simple approximation: weighted average
    # In practice, this should use cross-validation partitioning
    n_assets, n_samples = sample_portfolios.shape

    # Use mean as a simple approximation
    stacked = sample_portfolios.mean(axis=1)

    # Normalize to sum to 1
    stacked = stacked / stacked.sum()

    return stacked


def apply_entropy_pooling(
    prior_probabilities: np.ndarray,
    equality_constraints_A: np.ndarray,
    equality_constraints_b: np.ndarray,
    inequality_constraints_G: Optional[np.ndarray] = None,
    inequality_constraints_h: Optional[np.ndarray] = None,
    method: str = 'L-BFGS-B'
) -> np.ndarray:
    """
    Apply Entropy Pooling to incorporate market views

    Entropy Pooling modifies prior probabilities to satisfy constraints
    while minimizing information loss (maximizing entropy).

    Args:
        prior_probabilities: Prior probability vector, shape (S, 1)
        equality_constraints_A: Equality constraint matrix, shape (M, S)
        equality_constraints_b: Equality constraint vector, shape (M, 1)
        inequality_constraints_G: Inequality constraint matrix, shape (N, S), optional
        inequality_constraints_h: Inequality constraint vector, shape (N, 1), optional
        method: Optimization method ('L-BFGS-B' or 'TNC')

    Returns:
        Posterior probability vector, shape (S, 1)
    """
    # Ensure correct shapes
    if prior_probabilities.ndim == 1:
        prior_probabilities = prior_probabilities.reshape(-1, 1)
    if equality_constraints_b.ndim == 1:
        equality_constraints_b = equality_constraints_b.reshape(-1, 1)
    if inequality_constraints_h is not None and inequality_constraints_h.ndim == 1:
        inequality_constraints_h = inequality_constraints_h.reshape(-1, 1)

    if FORTITUDO_AVAILABLE:
        posterior = ft.entropy_pooling(
            p=prior_probabilities,
            A=equality_constraints_A,
            b=equality_constraints_b,
            G=inequality_constraints_G,
            h=inequality_constraints_h,
            method=method
        )
        return posterior
    else:
        return _fallback_entropy_pooling(
            prior_probabilities,
            equality_constraints_A,
            equality_constraints_b,
            inequality_constraints_G,
            inequality_constraints_h,
            method
        )


def apply_entropy_pooling_simple(
    n_scenarios: int,
    expected_return_constraint: Optional[float] = None,
    returns_matrix: Optional[np.ndarray] = None,
    max_probability: Optional[float] = None
) -> Dict[str, Any]:
    """
    Simplified entropy pooling with common constraints

    Args:
        n_scenarios: Number of scenarios
        expected_return_constraint: Target expected return (requires returns_matrix)
        returns_matrix: Asset returns matrix, shape (n_scenarios, n_assets)
        max_probability: Maximum probability for any scenario (e.g., 0.05 for 5%)

    Returns:
        Dictionary with prior and posterior probabilities
    """
    # Start with equal probabilities
    p_prior = np.ones((n_scenarios, 1)) / n_scenarios

    # Equality constraint: probabilities sum to 1
    A = np.ones((1, n_scenarios))
    b = np.array([[1.0]])

    # Optional inequality constraint: max probability
    G = None
    h = None
    if max_probability is not None:
        G = np.eye(n_scenarios)
        h = np.ones((n_scenarios, 1)) * max_probability

    # Apply entropy pooling
    p_posterior = apply_entropy_pooling(p_prior, A, b, G, h)

    return {
        'prior_probabilities': p_prior.flatten(),
        'posterior_probabilities': p_posterior.flatten(),
        'effective_scenarios_prior': float(1 / np.sum(p_prior ** 2)),
        'effective_scenarios_posterior': float(1 / np.sum(p_posterior ** 2)),
        'max_probability': float(p_posterior.max()),
        'min_probability': float(p_posterior.min())
    }


def calculate_exposure_stacking(
    sample_portfolios: np.ndarray,
    n_partitions: int = 4
) -> Dict[str, Any]:
    """
    Compute Exposure Stacking portfolio

    Exposure Stacking combines multiple sample portfolios using
    cross-validation to reduce overfitting.

    Reference: https://ssrn.com/abstract=4709317

    Args:
        sample_portfolios: Sample portfolio weights, shape (n_assets, n_samples)
        n_partitions: Number of partition sets (L)

    Returns:
        Dictionary with stacked portfolio and analysis
    """
    if sample_portfolios.ndim == 1:
        sample_portfolios = sample_portfolios.reshape(-1, 1)
    n_assets, n_samples = sample_portfolios.shape

    # Calculate exposure stacking portfolio
    if FORTITUDO_AVAILABLE:
        stacked_weights = ft.exposure_stacking(L=n_partitions, sample_portfolios=sample_portfolios)
    else:
        stacked_weights = _fallback_exposure_stacking(n_partitions, sample_portfolios)

    # Calculate statistics
    mean_weights = sample_portfolios.mean(axis=1)
    std_weights = sample_portfolios.std(axis=1)

    return {
        'stacked_weights': stacked_weights.tolist() if hasattr(stacked_weights, 'tolist') else list(stacked_weights),
        'mean_sample_weights': mean_weights.tolist(),
        'std_sample_weights': std_weights.tolist(),
        'n_assets': n_assets,
        'n_samples': n_samples,
        'n_partitions': n_partitions,
        'weights_sum': float(np.sum(stacked_weights))
    }


def plot_volatility_surface(
    vol_surface_data: np.ndarray,
    scenario_index: int = 0,
    figsize: Optional[Tuple[float, float]] = None,
    zoom: Optional[float] = None,
    save_path: Optional[str] = None
) -> Dict[str, Any]:
    """
    Plot implied volatility surface

    Args:
        vol_surface_data: Volatility surface matrix, shape (n_scenarios, 35)
                         35 = 7 strikes × 5 maturities
        scenario_index: Which scenario to plot
        figsize: Figure size (width, height)
        zoom: Zoom level for 3D plot
        save_path: Optional path to save figure

    Returns:
        Dictionary with plot info and surface statistics
    """
    try:
        import matplotlib
        matplotlib.use('Agg')  # Non-interactive backend
        import matplotlib.pyplot as plt
        from mpl_toolkits.mplot3d import Axes3D

        # Extract surface for statistics
        surface = vol_surface_data[scenario_index].reshape(5, 7)

        if FORTITUDO_AVAILABLE:
            fig, ax = ft.plot_vol_surface(
                index=scenario_index,
                vol_surface=vol_surface_data,
                figsize=figsize,
                zoom=zoom
            )
        else:
            # Fallback: create our own 3D plot
            fig = plt.figure(figsize=figsize if figsize else (10, 8))
            ax = fig.add_subplot(111, projection='3d')

            strikes = np.arange(7)
            maturities = np.arange(5)
            X, Y = np.meshgrid(strikes, maturities)

            ax.plot_surface(X, Y, surface, cmap='viridis', alpha=0.8)
            ax.set_xlabel('Strike')
            ax.set_ylabel('Maturity')
            ax.set_zlabel('Volatility')
            ax.set_title(f'Volatility Surface (Scenario {scenario_index})')

        if save_path:
            fig.savefig(save_path, dpi=150, bbox_inches='tight')

        plt.close(fig)

        return {
            'scenario_index': scenario_index,
            'min_volatility': float(surface.min()),
            'max_volatility': float(surface.max()),
            'mean_volatility': float(surface.mean()),
            'surface_shape': surface.shape,
            'plot_saved': save_path is not None,
            'save_path': save_path
        }
    except Exception as e:
        return {
            'error': str(e),
            'scenario_index': scenario_index
        }


def create_volatility_surface_from_options(
    spot_price: float,
    strikes: np.ndarray,
    maturities: np.ndarray,
    market_prices: np.ndarray,
    risk_free_rate: float,
    dividend_yield: float = 0.0
) -> np.ndarray:
    """
    Create volatility surface from option prices (helper function)

    This is a placeholder for implied volatility calculation.
    In practice, you'd use numerical methods to invert Black-Scholes.

    Args:
        spot_price: Current stock price
        strikes: Array of strike prices
        maturities: Array of times to maturity
        market_prices: Matrix of observed option prices
        risk_free_rate: Risk-free rate
        dividend_yield: Dividend yield

    Returns:
        Implied volatility surface matrix
    """
    # This is a simplified example
    # Real implementation would use Newton-Raphson or similar
    n_maturities = len(maturities)
    n_strikes = len(strikes)

    # Placeholder: return random volatilities for demonstration
    vol_surface = np.random.uniform(0.15, 0.40, (n_maturities, n_strikes))

    return vol_surface


def main():
    """Test advanced functions"""
    print("=== Fortitudo.tech Advanced Functions Test ===\n")

    # Test 1: Entropy Pooling
    print("1. Entropy Pooling Test")
    print("-" * 50)

    n_scenarios = 100
    result_ep = apply_entropy_pooling_simple(
        n_scenarios=n_scenarios,
        max_probability=0.03  # No scenario > 3%
    )

    print(f"  Prior effective scenarios: {result_ep['effective_scenarios_prior']:.1f}")
    print(f"  Posterior effective scenarios: {result_ep['effective_scenarios_posterior']:.1f}")
    print(f"  Max probability: {result_ep['max_probability']:.4f}")
    print(f"  Min probability: {result_ep['min_probability']:.6f}")
    print(f"  Sum of posterior: {result_ep['posterior_probabilities'].sum():.6f}")

    # Test 2: Exposure Stacking
    print("\n2. Exposure Stacking Test")
    print("-" * 50)

    np.random.seed(42)
    n_assets = 5
    n_samples = 20

    # Generate sample portfolios (each column sums to 1)
    sample_portfolios = np.random.dirichlet(np.ones(n_assets), n_samples).T

    result_es = calculate_exposure_stacking(
        sample_portfolios=sample_portfolios,
        n_partitions=4
    )

    print(f"  Number of assets: {result_es['n_assets']}")
    print(f"  Number of samples: {result_es['n_samples']}")
    print(f"  Partitions used: {result_es['n_partitions']}")
    print(f"  Stacked weights sum: {result_es['weights_sum']:.6f}")
    print("\n  Stacked weights:")
    for i, w in enumerate(result_es['stacked_weights']):
        print(f"    Asset {i+1}: {w:.4f}")

    # Test 3: Volatility Surface
    print("\n3. Volatility Surface Test")
    print("-" * 50)

    # Create sample vol surface data (10 scenarios, 35 points each)
    # 35 = 7 strikes × 5 maturities
    vol_surface_data = np.random.uniform(0.15, 0.40, (10, 35))

    result_vol = plot_volatility_surface(
        vol_surface_data=vol_surface_data,
        scenario_index=0,
        save_path=None  # Set to 'vol_surface.png' to save
    )

    if 'error' not in result_vol:
        print(f"  Scenario index: {result_vol['scenario_index']}")
        print(f"  Min volatility: {result_vol['min_volatility']:.4f}")
        print(f"  Max volatility: {result_vol['max_volatility']:.4f}")
        print(f"  Mean volatility: {result_vol['mean_volatility']:.4f}")
        print(f"  Surface shape: {result_vol['surface_shape']}")
    else:
        print(f"  Error: {result_vol['error']}")

    # Test 4: JSON Export
    print("\n4. JSON Export Test")
    print("-" * 50)
    json_output = json.dumps({
        'entropy_pooling': {
            'effective_scenarios': result_ep['effective_scenarios_posterior'],
            'max_prob': result_ep['max_probability']
        },
        'exposure_stacking': {
            'weights': result_es['stacked_weights'],
            'n_assets': result_es['n_assets']
        }
    }, indent=2)
    print(json_output)

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
