from typing import Dict, List, Optional
import numpy as np
from tsmoothie.smoother import (
    LowessSmoother, ConvolutionSmoother, SpectralSmoother,
    PolynomialSmoother, SplineSmoother, GaussianSmoother,
    ExponentialSmoother, KalmanSmoother, BinnerSmoother, DecomposeSmoother
)

def smooth_lowess(data: List[float], smooth_fraction: float = 0.1, iterations: int = 1) -> Dict:
    smoother = LowessSmoother(smooth_fraction=smooth_fraction, iterations=iterations)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'smooth_fraction': smooth_fraction,
        'iterations': iterations
    }

def smooth_convolution(data: List[float], window_len: int = 5, window_type: str = 'ones') -> Dict:
    smoother = ConvolutionSmoother(window_len=window_len, window_type=window_type)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'window_len': window_len,
        'window_type': window_type
    }

def smooth_spectral(data: List[float], smooth_fraction: float = 0.3) -> Dict:
    smoother = SpectralSmoother(smooth_fraction=smooth_fraction)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'smooth_fraction': smooth_fraction
    }

def smooth_polynomial(data: List[float], degree: int = 3) -> Dict:
    smoother = PolynomialSmoother(degree=degree)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'degree': degree
    }

def smooth_spline(data: List[float], smooth_fraction: float = 0.3, degree: int = 3) -> Dict:
    smoother = SplineSmoother(smooth_fraction=smooth_fraction, degree=degree)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'smooth_fraction': smooth_fraction,
        'degree': degree
    }

def smooth_gaussian(data: List[float], sigma: float = 1.0) -> Dict:
    smoother = GaussianSmoother(sigma=sigma)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'sigma': sigma
    }

def smooth_exponential(data: List[float], window_len: int = 5, alpha: float = 0.3) -> Dict:
    smoother = ExponentialSmoother(window_len=window_len, alpha=alpha)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'window_len': window_len,
        'alpha': alpha
    }

def smooth_kalman(data: List[float]) -> Dict:
    smoother = KalmanSmoother(component='level_trend', component_noise={'level': 0.1, 'trend': 0.1})
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist()
    }

def smooth_binner(data: List[float], n_knots: int = 10) -> Dict:
    smoother = BinnerSmoother(n_knots=n_knots)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'n_knots': n_knots
    }

def smooth_decompose(data: List[float], period: int = 12, model: str = 'additive') -> Dict:
    smoother = DecomposeSmoother(smooth_type='trend', periods=period, model=model)
    smoother.smooth(np.array(data))
    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'period': period,
        'model': model
    }

def main():
    print("Testing tsmoothie Smoothers")

    data = [1, 2, 4, 7, 11, 16, 22, 29, 37, 46, 56, 67, 79, 92, 106] * 2

    print("\n1. Testing Lowess...")
    result = smooth_lowess(data, smooth_fraction=0.2)
    print(f"Original length: {len(data)}, Smoothed length: {len(result['smoothed'])}")
    assert len(result['smoothed']) == len(data)
    print("Test 1: PASSED")

    print("\n2. Testing Convolution...")
    result = smooth_convolution(data, window_len=5, window_type='hanning')
    print(f"Smoothed length: {len(result['smoothed'])}")
    assert len(result['smoothed']) > 0
    print("Test 2: PASSED")

    print("\n3. Testing Exponential...")
    result = smooth_exponential(data, window_len=5, alpha=0.3)
    print(f"Smoothed length: {len(result['smoothed'])}")
    assert len(result['smoothed']) > 0
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
