from typing import Dict, List
import pandas as pd
import numpy as np
from gluonts.dataset.pandas import PandasDataset
from gluonts.torch.model.simple_feedforward import SimpleFeedForwardEstimator
from gluonts.torch.model.deepar import DeepAREstimator
from gluonts.torch.model.tft import TemporalFusionTransformerEstimator
from gluonts.torch.model.wavenet import WaveNetEstimator
from gluonts.torch.model.d_linear import DLinearEstimator
from gluonts.torch.model.patch_tst import PatchTSTEstimator
from gluonts.torch.model.tide import TiDEEstimator
from gluonts.torch.model.lag_tst import LagTSTEstimator
from gluonts.torch.model.deep_npts import DeepNPTSEstimator

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

def forecast_feedforward(data: List[float], prediction_length: int = 10, epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data)
    estimator = SimpleFeedForwardEstimator(
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'SimpleFeedForward', prediction_length)

def forecast_deepar(data: List[float], prediction_length: int = 10, freq: str = 'D', epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data, freq)
    estimator = DeepAREstimator(
        freq=freq,
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'DeepAR', prediction_length)

def forecast_tft(data: List[float], prediction_length: int = 10, freq: str = 'D', epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data, freq)
    estimator = TemporalFusionTransformerEstimator(
        freq=freq,
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'TemporalFusionTransformer', prediction_length)

def forecast_wavenet(data: List[float], prediction_length: int = 10, freq: str = 'D', epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data, freq)
    estimator = WaveNetEstimator(
        freq=freq,
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'WaveNet', prediction_length)

def forecast_dlinear(data: List[float], prediction_length: int = 10, epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data)
    estimator = DLinearEstimator(
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'DLinear', prediction_length)

def forecast_patchtst(data: List[float], prediction_length: int = 10, freq: str = 'D', epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data, freq)
    estimator = PatchTSTEstimator(
        freq=freq,
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'PatchTST', prediction_length)

def forecast_tide(data: List[float], prediction_length: int = 10, freq: str = 'D', epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data, freq)
    estimator = TiDEEstimator(
        freq=freq,
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'TiDE', prediction_length)

def forecast_lagtst(data: List[float], prediction_length: int = 10, freq: str = 'D', epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data, freq)
    estimator = LagTSTEstimator(
        freq=freq,
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'LagTST', prediction_length)

def forecast_deepnpts(data: List[float], prediction_length: int = 10, freq: str = 'D', epochs: int = 10) -> Dict:
    dataset = _prepare_dataset(data, freq)
    estimator = DeepNPTSEstimator(
        freq=freq,
        prediction_length=prediction_length,
        trainer_kwargs={'max_epochs': epochs}
    )
    predictor = estimator.train(dataset)
    forecasts = list(predictor.predict(dataset))
    return _extract_forecast(forecasts, 'DeepNPTS', prediction_length)

def main():
    print("Testing GluonTS Forecasters")

    data = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0] * 5

    print("\n1. Testing SimpleFeedForward...")
    result = forecast_feedforward(data, prediction_length=5, epochs=3)
    print(f"Model: {result['model']}")
    print(f"Mean forecast length: {len(result['mean'])}")
    print(f"First 3 predictions: {result['mean'][:3]}")
    assert len(result['mean']) == 5
    print("Test 1: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
