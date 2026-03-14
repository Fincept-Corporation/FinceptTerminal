from typing import Dict, List
import pandas as pd
import numpy as np
from gluonts.dataset.pandas import PandasDataset
from gluonts.model.seasonal_naive import SeasonalNaivePredictor
from gluonts.model.trivial.mean import MeanPredictor
from gluonts.model.trivial.constant import ConstantValuePredictor

def _prepare_dataset(data: List[float], freq: str = 'D'):
    df = pd.DataFrame({
        'target': np.array(data, dtype=np.float32),
        'start': pd.date_range('2020-01-01', periods=len(data), freq=freq),
        'item_id': ['item_0'] * len(data)
    })
    return PandasDataset.from_long_dataframe(df, target='target', timestamp='start', item_id='item_id')

def _extract_forecast(forecasts, model_name: str, prediction_length: int) -> Dict:
    return {
        'mean': forecasts[0].mean.tolist(),
        'quantiles': {
            '0.1': forecasts[0].quantile(0.1).tolist(),
            '0.5': forecasts[0].quantile(0.5).tolist(),
            '0.9': forecasts[0].quantile(0.9).tolist()
        },
        'prediction_length': prediction_length,
        'model': model_name
    }

def predict_seasonal_naive(data: List[float], prediction_length: int = 10, season_length: int = 7) -> Dict:
    dataset = _prepare_dataset(data)
    predictor = SeasonalNaivePredictor(prediction_length=prediction_length, season_length=season_length)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'SeasonalNaive', prediction_length)

def predict_mean(data: List[float], prediction_length: int = 10) -> Dict:
    dataset = _prepare_dataset(data)
    predictor = MeanPredictor(prediction_length=prediction_length)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'Mean', prediction_length)

def predict_constant(data: List[float], prediction_length: int = 10, constant_value: float = 0.0) -> Dict:
    dataset = _prepare_dataset(data)
    predictor = ConstantValuePredictor(prediction_length=prediction_length, value=constant_value)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'Constant', prediction_length)

def main():
    print("Testing GluonTS Predictors")

    data = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0] * 5

    print("\n1. Testing SeasonalNaive...")
    result = predict_seasonal_naive(data, prediction_length=5, season_length=7)
    print(f"Model: {result['model']}")
    print(f"Mean forecast length: {len(result['mean'])}")
    assert len(result['mean']) == 5
    print("Test 1: PASSED")

    print("\n2. Testing Mean...")
    result = predict_mean(data, prediction_length=5)
    print(f"Model: {result['model']}")
    print(f"Mean forecast: {result['mean'][0]:.2f}")
    assert len(result['mean']) == 5
    print("Test 2: PASSED")

    print("\n3. Testing Constant...")
    result = predict_constant(data, prediction_length=5, constant_value=5.0)
    print(f"Model: {result['model']}")
    print(f"Constant forecast: {result['mean'][0]:.2f}")
    assert len(result['mean']) == 5
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
