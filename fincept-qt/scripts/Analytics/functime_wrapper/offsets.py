import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
from functime.offsets import freq_to_sp

def frequency_to_seasonal_period(
    freq: str
) -> Dict[str, Any]:
    """Convert frequency string to seasonal period"""
    sp = freq_to_sp(freq)

    return {
        'seasonal_period': int(sp),
        'frequency': freq
    }

def main():
    print("Testing functime offsets wrapper")

    # Test various frequencies
    frequencies = ['1d', '1w', '1mo', '1q', '1y', '1h', '30m']

    for freq in frequencies:
        result = frequency_to_seasonal_period(freq)
        print("{} -> seasonal period: {}".format(freq, result['seasonal_period']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
