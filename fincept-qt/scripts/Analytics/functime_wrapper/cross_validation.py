import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
from functime.cross_validation import (
    expanding_window_split, sliding_window_split, train_test_split
)

def split_train_test(
    y: pl.DataFrame,
    test_size: int = 1
) -> Dict[str, Any]:
    """Split panel data into train and test sets"""
    train, test = train_test_split(y, test_size=test_size)

    return {
        'train': train.to_dicts(),
        'test': test.to_dicts(),
        'train_shape': train.shape,
        'test_shape': test.shape
    }

def create_expanding_window_splits(
    y: pl.DataFrame,
    test_size: int = 1,
    n_splits: int = 3,
    step: int = 1
) -> Dict[str, Any]:
    """Create expanding window cross-validation splits"""
    splits = list(expanding_window_split(
        y=y,
        test_size=test_size,
        n_splits=n_splits,
        step=step
    ))

    return {
        'n_splits': len(splits),
        'splits': [
            {
                'train_shape': train.shape,
                'test_shape': test.shape,
                'train': train.to_dicts(),
                'test': test.to_dicts()
            }
            for train, test in splits
        ]
    }

def create_sliding_window_splits(
    y: pl.DataFrame,
    train_size: int,
    test_size: int = 1,
    n_splits: int = 3,
    step: int = 1
) -> Dict[str, Any]:
    """Create sliding window cross-validation splits"""
    splits = list(sliding_window_split(
        y=y,
        train_size=train_size,
        test_size=test_size,
        n_splits=n_splits,
        step=step
    ))

    return {
        'n_splits': len(splits),
        'splits': [
            {
                'train_shape': train.shape,
                'test_shape': test.shape,
                'train': train.to_dicts(),
                'test': test.to_dicts()
            }
            for train, test in splits
        ]
    }

def main():
    print("Testing functime cross_validation wrapper")

    # Create sample panel data
    df = pl.DataFrame({
        'entity_id': ['A'] * 10 + ['B'] * 10,
        'time': pl.datetime_range(
            start=pl.datetime(2020, 1, 1),
            end=pl.datetime(2020, 1, 10),
            interval='1d',
            eager=True
        ).to_list() * 2,
        'value': list(range(10)) + list(range(10, 20))
    })

    # Test train_test_split
    split_result = split_train_test(df, test_size=2)
    print("Train shape: {}, Test shape: {}".format(
        split_result['train_shape'],
        split_result['test_shape']
    ))

    # Test expanding window
    expand_result = create_expanding_window_splits(df, test_size=1, n_splits=3)
    print("Expanding window splits: {}".format(expand_result['n_splits']))

    # Test sliding window
    slide_result = create_sliding_window_splits(df, train_size=5, test_size=1, n_splits=3)
    print("Sliding window splits: {}".format(slide_result['n_splits']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
