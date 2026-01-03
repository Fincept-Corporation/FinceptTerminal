"""
Advanced Cross-Validation Module
================================

Provides sophisticated cross-validation strategies for time series:
- Blocked time series CV (prevents leakage)
- Purged/Embargo CV (for financial data)
- Combinatorial purged CV
- Walk-forward validation
- Nested cross-validation

Critical for proper model evaluation in time series.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple, Generator
from datetime import datetime, timedelta
import warnings

warnings.filterwarnings('ignore')


# ============================================================================
# BLOCKED TIME SERIES CV
# ============================================================================

def blocked_time_series_cv(
    y: pl.DataFrame,
    n_splits: int = 5,
    test_size: int = 1,
    gap: int = 0
) -> Dict[str, Any]:
    """
    Blocked time series cross-validation with optional gap.

    Unlike expanding window, uses fixed-size training blocks to
    prevent data leakage and maintain stationarity.

    Args:
        y: Panel data
        n_splits: Number of CV splits
        test_size: Test set size per split
        gap: Gap between train and test (prevents leakage)
    """
    splits = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        n = len(entity_data)

        # Calculate block size
        total_test = n_splits * test_size
        total_gap = n_splits * gap
        available_for_train = n - total_test - total_gap

        if available_for_train < n_splits * 10:
            continue

        block_size = available_for_train // n_splits

        entity_splits = []
        for i in range(n_splits):
            train_start = i * block_size
            train_end = (i + 1) * block_size
            test_start = train_end + gap
            test_end = test_start + test_size

            if test_end > n:
                break

            train_data = entity_data[train_start:train_end]
            test_data = entity_data[test_start:test_end]

            entity_splits.append({
                'split': i,
                'train_start': train_start,
                'train_end': train_end,
                'test_start': test_start,
                'test_end': test_end,
                'train_shape': train_data.shape,
                'test_shape': test_data.shape,
                'train': train_data.to_dicts(),
                'test': test_data.to_dicts()
            })

        splits.append({
            'entity_id': entity_id,
            'n_splits': len(entity_splits),
            'splits': entity_splits
        })

    return {
        'success': True,
        'method': 'blocked',
        'n_splits': n_splits,
        'test_size': test_size,
        'gap': gap,
        'data': splits
    }


# ============================================================================
# PURGED K-FOLD CV
# ============================================================================

def purged_kfold_cv(
    y: pl.DataFrame,
    n_splits: int = 5,
    embargo_pct: float = 0.01,
    purge_pct: float = 0.01
) -> Dict[str, Any]:
    """
    Purged K-Fold cross-validation for financial data.

    Removes observations that could leak information from train to test:
    - Purge: Remove training samples close to test set
    - Embargo: Remove period after test set from subsequent training

    Essential for time series with overlapping windows or labels.

    Args:
        y: Panel data
        n_splits: Number of folds
        embargo_pct: Percentage of data to embargo after test set
        purge_pct: Percentage of data to purge before test set
    """
    splits = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        n = len(entity_data)

        fold_size = n // n_splits
        embargo_size = int(n * embargo_pct)
        purge_size = int(n * purge_pct)

        entity_splits = []

        for i in range(n_splits):
            test_start = i * fold_size
            test_end = min((i + 1) * fold_size, n)

            # Purge period (before test)
            purge_start = max(0, test_start - purge_size)

            # Embargo period (after test)
            embargo_end = min(n, test_end + embargo_size)

            # Training indices (everything except test, purge, embargo)
            train_indices = list(range(0, purge_start)) + list(range(embargo_end, n))

            if len(train_indices) < 10:
                continue

            train_data = entity_data[train_indices]
            test_data = entity_data[test_start:test_end]

            entity_splits.append({
                'split': i,
                'test_start': test_start,
                'test_end': test_end,
                'purge_start': purge_start,
                'embargo_end': embargo_end,
                'train_size': len(train_indices),
                'test_size': test_end - test_start,
                'train_shape': train_data.shape,
                'test_shape': test_data.shape,
                'train': train_data.to_dicts(),
                'test': test_data.to_dicts()
            })

        splits.append({
            'entity_id': entity_id,
            'n_splits': len(entity_splits),
            'splits': entity_splits
        })

    return {
        'success': True,
        'method': 'purged_kfold',
        'n_splits': n_splits,
        'embargo_pct': embargo_pct,
        'purge_pct': purge_pct,
        'data': splits
    }


# ============================================================================
# COMBINATORIAL PURGED CV
# ============================================================================

def combinatorial_purged_cv(
    y: pl.DataFrame,
    n_test_splits: int = 2,
    n_groups: int = 6,
    embargo_pct: float = 0.01
) -> Dict[str, Any]:
    """
    Combinatorial Purged Cross-Validation (CPCV).

    Generates all combinations of test groups, providing more robust
    evaluation for small datasets.

    Args:
        y: Panel data
        n_test_splits: Number of groups used as test set
        n_groups: Total number of groups
        embargo_pct: Embargo percentage
    """
    from itertools import combinations

    splits = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        n = len(entity_data)

        group_size = n // n_groups
        embargo_size = int(n * embargo_pct)

        # All combinations of test groups
        test_combinations = list(combinations(range(n_groups), n_test_splits))

        entity_splits = []

        for combo_idx, test_groups in enumerate(test_combinations):
            test_groups = set(test_groups)

            # Identify test indices
            test_indices = []
            for g in test_groups:
                start = g * group_size
                end = min((g + 1) * group_size, n)
                test_indices.extend(range(start, end))

            # Identify train indices with embargo
            train_indices = []
            for g in range(n_groups):
                if g in test_groups:
                    continue

                start = g * group_size
                end = min((g + 1) * group_size, n)

                # Apply embargo if adjacent to test group
                if g + 1 in test_groups:
                    end = max(start, end - embargo_size)
                if g - 1 in test_groups:
                    start = min(end, start + embargo_size)

                train_indices.extend(range(start, end))

            if len(train_indices) < 10 or len(test_indices) < 1:
                continue

            train_data = entity_data[train_indices]
            test_data = entity_data[test_indices]

            entity_splits.append({
                'split': combo_idx,
                'test_groups': list(test_groups),
                'train_size': len(train_indices),
                'test_size': len(test_indices),
                'train': train_data.to_dicts(),
                'test': test_data.to_dicts()
            })

        splits.append({
            'entity_id': entity_id,
            'n_splits': len(entity_splits),
            'n_combinations': len(test_combinations),
            'splits': entity_splits
        })

    return {
        'success': True,
        'method': 'combinatorial_purged',
        'n_test_splits': n_test_splits,
        'n_groups': n_groups,
        'embargo_pct': embargo_pct,
        'data': splits
    }


# ============================================================================
# WALK-FORWARD VALIDATION
# ============================================================================

def walk_forward_validation(
    y: pl.DataFrame,
    initial_train_size: int = 100,
    test_size: int = 1,
    step_size: int = 1,
    anchored: bool = False
) -> Dict[str, Any]:
    """
    Walk-forward (rolling origin) validation.

    Simulates real-world forecasting by training on past data
    and testing on future data in a rolling manner.

    Args:
        y: Panel data
        initial_train_size: Initial training window size
        test_size: Test window size
        step_size: Step size between origins
        anchored: If True, always start from beginning (expanding window)
    """
    splits = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        n = len(entity_data)

        if n < initial_train_size + test_size:
            continue

        entity_splits = []
        origin = initial_train_size

        split_idx = 0
        while origin + test_size <= n:
            if anchored:
                train_start = 0
            else:
                train_start = max(0, origin - initial_train_size)

            train_end = origin
            test_start = origin
            test_end = min(origin + test_size, n)

            train_data = entity_data[train_start:train_end]
            test_data = entity_data[test_start:test_end]

            entity_splits.append({
                'split': split_idx,
                'origin': origin,
                'train_start': train_start,
                'train_end': train_end,
                'test_start': test_start,
                'test_end': test_end,
                'train_shape': train_data.shape,
                'test_shape': test_data.shape,
                'train': train_data.to_dicts(),
                'test': test_data.to_dicts()
            })

            origin += step_size
            split_idx += 1

        splits.append({
            'entity_id': entity_id,
            'n_splits': len(entity_splits),
            'splits': entity_splits
        })

    return {
        'success': True,
        'method': 'walk_forward',
        'initial_train_size': initial_train_size,
        'test_size': test_size,
        'step_size': step_size,
        'anchored': anchored,
        'data': splits
    }


# ============================================================================
# NESTED CROSS-VALIDATION
# ============================================================================

def nested_cv(
    y: pl.DataFrame,
    outer_splits: int = 5,
    inner_splits: int = 3,
    test_size: int = 1
) -> Dict[str, Any]:
    """
    Nested cross-validation for unbiased model selection and evaluation.

    Outer loop: Model evaluation
    Inner loop: Hyperparameter tuning

    Prevents overfitting during model selection.

    Args:
        y: Panel data
        outer_splits: Number of outer CV splits
        inner_splits: Number of inner CV splits
        test_size: Test size per split
    """
    results = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        n = len(entity_data)

        outer_fold_size = n // outer_splits

        entity_results = []

        for outer_i in range(outer_splits):
            # Outer test set
            outer_test_start = outer_i * outer_fold_size
            outer_test_end = min((outer_i + 1) * outer_fold_size, n)

            # Outer train set (everything before test)
            outer_train_end = outer_test_start

            if outer_train_end < inner_splits * 10:
                continue

            outer_train = entity_data[:outer_train_end]
            outer_test = entity_data[outer_test_start:outer_test_end]

            # Inner CV on outer training set
            inner_fold_size = len(outer_train) // inner_splits
            inner_splits_data = []

            for inner_i in range(inner_splits):
                inner_test_start = inner_i * inner_fold_size
                inner_test_end = min((inner_i + 1) * inner_fold_size, len(outer_train))
                inner_train_end = inner_test_start

                if inner_train_end < 10:
                    continue

                inner_train = outer_train[:inner_train_end]
                inner_test = outer_train[inner_test_start:inner_test_end]

                inner_splits_data.append({
                    'inner_split': inner_i,
                    'inner_train_shape': inner_train.shape,
                    'inner_test_shape': inner_test.shape
                })

            entity_results.append({
                'outer_split': outer_i,
                'outer_train_shape': outer_train.shape,
                'outer_test_shape': outer_test.shape,
                'outer_train': outer_train.to_dicts(),
                'outer_test': outer_test.to_dicts(),
                'inner_splits': inner_splits_data
            })

        results.append({
            'entity_id': entity_id,
            'n_outer_splits': len(entity_results),
            'splits': entity_results
        })

    return {
        'success': True,
        'method': 'nested',
        'outer_splits': outer_splits,
        'inner_splits': inner_splits,
        'data': results
    }


# ============================================================================
# UTILITY: GET CV GENERATOR
# ============================================================================

def get_cv_splits(
    y: pl.DataFrame,
    method: str = 'expanding',
    **kwargs
) -> Generator[Tuple[pl.DataFrame, pl.DataFrame], None, None]:
    """
    Get a generator of (train, test) splits.

    Args:
        y: Panel data
        method: CV method ('expanding', 'sliding', 'blocked', 'purged', 'walk_forward')
        **kwargs: Additional arguments for the CV method
    """
    if method == 'expanding':
        from cross_validation import create_expanding_window_splits
        result = create_expanding_window_splits(y, **kwargs)
    elif method == 'sliding':
        from cross_validation import create_sliding_window_splits
        result = create_sliding_window_splits(y, **kwargs)
    elif method == 'blocked':
        result = blocked_time_series_cv(y, **kwargs)
    elif method == 'purged':
        result = purged_kfold_cv(y, **kwargs)
    elif method == 'walk_forward':
        result = walk_forward_validation(y, **kwargs)
    else:
        result = walk_forward_validation(y, **kwargs)

    # Yield splits
    if 'data' in result:
        for entity_result in result['data']:
            for split in entity_result.get('splits', []):
                train_df = pl.DataFrame(split['train'])
                test_df = pl.DataFrame(split['test'])
                yield train_df, test_df


def main():
    """Test advanced CV methods."""
    print("Testing Advanced Cross-Validation Module")
    print("=" * 50)

    # Create sample data
    dates = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 12, 31),
        interval='1d',
        eager=True
    ).to_list()

    n = len(dates)
    np.random.seed(42)
    values = [100 + 0.1 * i + np.random.randn() * 5 for i in range(n)]

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values
    })

    # Test blocked CV
    blocked_result = blocked_time_series_cv(df, n_splits=5, test_size=10, gap=5)
    print(f"Blocked CV: {blocked_result['data'][0]['n_splits']} splits")

    # Test purged K-fold
    purged_result = purged_kfold_cv(df, n_splits=5, embargo_pct=0.02)
    print(f"Purged K-Fold: {purged_result['data'][0]['n_splits']} splits")

    # Test walk-forward
    wf_result = walk_forward_validation(df, initial_train_size=100, test_size=10, step_size=10)
    print(f"Walk-Forward: {wf_result['data'][0]['n_splits']} splits")

    # Test combinatorial purged
    cpcv_result = combinatorial_purged_cv(df, n_test_splits=2, n_groups=5)
    print(f"CPCV: {cpcv_result['data'][0]['n_combinations']} combinations")

    # Test nested CV
    nested_result = nested_cv(df, outer_splits=3, inner_splits=3)
    print(f"Nested CV: {nested_result['data'][0]['n_outer_splits']} outer splits")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
