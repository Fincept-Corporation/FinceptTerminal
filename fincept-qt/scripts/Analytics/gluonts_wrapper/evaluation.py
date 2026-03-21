from typing import Dict, List
import pandas as pd
import numpy as np
from gluonts.dataset.pandas import PandasDataset
from gluonts.evaluation import Evaluator, make_evaluation_predictions
from gluonts.model.seasonal_naive import SeasonalNaivePredictor

def evaluate_forecasts(train_data: List[float], test_data: List[float], prediction_length: int = 10, freq: str = 'D') -> Dict:
    full_data = train_data + test_data
    df = pd.DataFrame({
        'target': np.array(full_data, dtype=np.float32),
        'start': pd.date_range('2020-01-01', periods=len(full_data), freq=freq),
        'item_id': ['item_0'] * len(full_data)
    })
    dataset = PandasDataset.from_long_dataframe(df, target='target', timestamp='start', item_id='item_id')

    predictor = SeasonalNaivePredictor(prediction_length=prediction_length, season_length=7)

    forecast_it, ts_it = make_evaluation_predictions(
        dataset=dataset,
        predictor=predictor,
        num_samples=100
    )

    forecasts = list(forecast_it)
    tss = list(ts_it)

    evaluator = Evaluator()
    agg_metrics, item_metrics = evaluator(tss, forecasts)

    return {
        'aggregate_metrics': {
            'MSE': float(agg_metrics.get('MSE', 0)),
            'RMSE': float(agg_metrics.get('RMSE', 0)),
            'MAE': float(agg_metrics.get('mean_absolute_error', 0)),
            'MAPE': float(agg_metrics.get('MAPE', 0)),
            'sMAPE': float(agg_metrics.get('sMAPE', 0)),
            'MASE': float(agg_metrics.get('MASE', 0)),
            'mean_wQuantileLoss': float(agg_metrics.get('mean_wQuantileLoss', 0))
        },
        'num_forecasts': len(forecasts)
    }

def main():
    print("Testing GluonTS Evaluation")

    train_data = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0] * 5
    test_data = [11.0, 12.0, 13.0, 14.0, 15.0]

    print("\n1. Testing Evaluator...")
    result = evaluate_forecasts(train_data, test_data, prediction_length=5)
    print(f"Num forecasts: {result['num_forecasts']}")
    print(f"RMSE: {result['aggregate_metrics']['RMSE']:.4f}")
    print(f"MAE: {result['aggregate_metrics']['MAE']:.4f}")
    assert result['num_forecasts'] > 0
    print("Test 1: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
