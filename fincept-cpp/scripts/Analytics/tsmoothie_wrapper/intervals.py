from typing import Dict, List, Tuple
import numpy as np
from tsmoothie.smoother import LowessSmoother, ConvolutionSmoother
from tsmoothie.utils_func import sigma_interval, confidence_interval, prediction_interval

def get_sigma_intervals(data: List[float], smooth_fraction: float = 0.1, n_sigma: int = 2) -> Dict:
    smoother = LowessSmoother(smooth_fraction=smooth_fraction)
    smoother.smooth(np.array(data))
    low, up = smoother.get_intervals('sigma_interval', n_sigma=n_sigma)

    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'lower_bound': low[0].tolist(),
        'upper_bound': up[0].tolist(),
        'n_sigma': n_sigma
    }

def get_confidence_intervals(data: List[float], smooth_fraction: float = 0.1, confidence: float = 0.95) -> Dict:
    smoother = LowessSmoother(smooth_fraction=smooth_fraction)
    smoother.smooth(np.array(data))
    low, up = smoother.get_intervals('confidence_interval', confidence=confidence)

    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'lower_bound': low[0].tolist(),
        'upper_bound': up[0].tolist(),
        'confidence': confidence
    }

def get_prediction_intervals(data: List[float], smooth_fraction: float = 0.1, confidence: float = 0.95) -> Dict:
    smoother = LowessSmoother(smooth_fraction=smooth_fraction)
    smoother.smooth(np.array(data))
    low, up = smoother.get_intervals('prediction_interval', confidence=confidence)

    return {
        'smoothed': smoother.smooth_data[0].tolist(),
        'lower_bound': low[0].tolist(),
        'upper_bound': up[0].tolist(),
        'confidence': confidence
    }

def detect_outliers_sigma(data: List[float], smooth_fraction: float = 0.1, n_sigma: int = 2) -> Dict:
    smoother = LowessSmoother(smooth_fraction=smooth_fraction)
    smoother.smooth(np.array(data))
    low, up = smoother.get_intervals('sigma_interval', n_sigma=n_sigma)

    data_array = np.array(data)
    outliers = (data_array < low[0]) | (data_array > up[0])

    return {
        'outliers': outliers.tolist(),
        'outlier_indices': np.where(outliers)[0].tolist(),
        'outlier_count': int(outliers.sum()),
        'n_sigma': n_sigma
    }

def main():
    print("Testing tsmoothie Intervals")

    data = [1, 2, 4, 7, 11, 16, 22, 29, 37, 46, 56, 67, 79, 92, 106] * 2

    print("\n1. Testing Sigma Intervals...")
    result = get_sigma_intervals(data, smooth_fraction=0.2, n_sigma=2)
    print(f"Smoothed length: {len(result['smoothed'])}")
    print(f"Lower bound length: {len(result['lower_bound'])}")
    print(f"Upper bound length: {len(result['upper_bound'])}")
    assert len(result['smoothed']) == len(data)
    print("Test 1: PASSED")

    print("\n2. Testing Confidence Intervals...")
    result = get_confidence_intervals(data, smooth_fraction=0.2, confidence=0.95)
    print(f"Confidence: {result['confidence']}")
    assert len(result['smoothed']) == len(data)
    print("Test 2: PASSED")

    print("\n3. Testing Outlier Detection...")
    result = detect_outliers_sigma(data, smooth_fraction=0.2, n_sigma=2)
    print(f"Outliers detected: {result['outlier_count']}")
    print(f"Outlier indices: {result['outlier_indices'][:5]}")
    assert len(result['outliers']) == len(data)
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
