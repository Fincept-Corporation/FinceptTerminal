#!/usr/bin/env python3
"""
GluonTS Worker Handler
Covers all 13 operations: 9 deep-learning forecasters, 3 baseline predictors, 1 evaluation.
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from forecasters import (
    forecast_feedforward, forecast_deepar, forecast_tft, forecast_wavenet,
    forecast_dlinear, forecast_patchtst, forecast_tide, forecast_lagtst, forecast_deepnpts
)
from predictors import predict_seasonal_naive, predict_mean, predict_constant
from evaluation import evaluate_forecasts


def main(args):
    """
    Entry point: [operation, json_data]
    Returns: JSON string
    """
    try:
        if len(args) < 2:
            return json.dumps({"error": "Expected: [operation, json_data]"})

        operation = args[0]
        data = json.loads(args[1])

        dispatch = {
            # Deep learning forecasters
            "forecast_feedforward": _forecast_feedforward,
            "forecast_deepar": _forecast_deepar,
            "forecast_tft": _forecast_tft,
            "forecast_wavenet": _forecast_wavenet,
            "forecast_dlinear": _forecast_dlinear,
            "forecast_patchtst": _forecast_patchtst,
            "forecast_tide": _forecast_tide,
            "forecast_lagtst": _forecast_lagtst,
            "forecast_deepnpts": _forecast_deepnpts,
            # Baseline predictors
            "predict_seasonal_naive": _predict_seasonal_naive,
            "predict_mean": _predict_mean,
            "predict_constant": _predict_constant,
            # Evaluation
            "evaluate": _evaluate,
        }

        handler = dispatch.get(operation)
        if handler is None:
            return json.dumps({"error": f"Unknown operation: {operation}"})

        return handler(data)

    except Exception as e:
        import traceback
        return json.dumps({
            "error": f"GluonTS error: {str(e)}",
            "traceback": traceback.format_exc()
        })


# ---------------------------------------------------------------------------
# Deep Learning Forecasters
# ---------------------------------------------------------------------------

def _forecast_feedforward(data):
    result = forecast_feedforward(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_deepar(data):
    result = forecast_deepar(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D"),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_tft(data):
    result = forecast_tft(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D"),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_wavenet(data):
    result = forecast_wavenet(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D"),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_dlinear(data):
    result = forecast_dlinear(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_patchtst(data):
    result = forecast_patchtst(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D"),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_tide(data):
    result = forecast_tide(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D"),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_lagtst(data):
    result = forecast_lagtst(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D"),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


def _forecast_deepnpts(data):
    result = forecast_deepnpts(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D"),
        epochs=data.get("epochs", 10)
    )
    return json.dumps(result)


# ---------------------------------------------------------------------------
# Baseline Predictors
# ---------------------------------------------------------------------------

def _predict_seasonal_naive(data):
    result = predict_seasonal_naive(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        season_length=data.get("season_length", 7)
    )
    return json.dumps(result)


def _predict_mean(data):
    result = predict_mean(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10)
    )
    return json.dumps(result)


def _predict_constant(data):
    result = predict_constant(
        data=data["data"],
        prediction_length=data.get("prediction_length", 10),
        constant_value=data.get("constant_value", 0.0)
    )
    return json.dumps(result)


# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def _evaluate(data):
    result = evaluate_forecasts(
        train_data=data["train_data"],
        test_data=data["test_data"],
        prediction_length=data.get("prediction_length", 10),
        freq=data.get("freq", "D")
    )
    return json.dumps(result)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(main(sys.argv[1:]))
    else:
        print("Usage: worker_handler.py <operation> <json_data>")
