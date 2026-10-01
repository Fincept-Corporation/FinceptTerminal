"""
AI Quant Lab - Qlib Service
Microsoft Qlib integration for quantitative research, model training, backtesting and analysis
Full-featured implementation with all Qlib capabilities
"""

import json
import re
import sys
import os
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Any, Optional, Union
import warnings
warnings.filterwarnings('ignore')

# Recent MLflow releases refuse to open the file-based tracking store that Qlib's
# recorder uses unless this is set ("The filesystem tracking backend ... is in
# maintenance mode"), which made every model.fit() fail with that message.
os.environ.setdefault("MLFLOW_ALLOW_FILE_STORE", "true")

# Qlib imports with availability check
QLIB_AVAILABLE = False
QLIB_ERROR = None

try:
    import qlib
    from qlib.config import REG_CN, REG_US
    from qlib.data import D
    from qlib.data.dataset import DatasetH, TSDatasetH
    from qlib.workflow import R
    from qlib.workflow.expm import MLflowExpManager
    from qlib.utils import init_instance_by_config
    QLIB_AVAILABLE = True
except ImportError as e:
    QLIB_ERROR = str(e)

# Model imports with individual availability
MODELS_AVAILABLE = {}
MODEL_CLASSES = {}

if QLIB_AVAILABLE:
    try:
        from qlib.contrib.model.gbdt import LGBModel
        MODELS_AVAILABLE['lightgbm'] = True
        MODEL_CLASSES['lightgbm'] = LGBModel
    except: MODELS_AVAILABLE['lightgbm'] = False

    try:
        from qlib.contrib.model.xgboost import XGBModel
        MODELS_AVAILABLE['xgboost'] = True
        MODEL_CLASSES['xgboost'] = XGBModel
    except: MODELS_AVAILABLE['xgboost'] = False

    try:
        from qlib.contrib.model.catboost_model import CatBoostModel
        MODELS_AVAILABLE['catboost'] = True
        MODEL_CLASSES['catboost'] = CatBoostModel
    except: MODELS_AVAILABLE['catboost'] = False

    try:
        from qlib.contrib.model.linear import LinearModel
        MODELS_AVAILABLE['linear'] = True
        MODEL_CLASSES['linear'] = LinearModel
    except: MODELS_AVAILABLE['linear'] = False

    try:
        from qlib.contrib.model.pytorch_lstm import LSTM
        MODELS_AVAILABLE['lstm'] = True
        MODEL_CLASSES['lstm'] = LSTM
    except: MODELS_AVAILABLE['lstm'] = False

    try:
        from qlib.contrib.model.pytorch_gru import GRU
        MODELS_AVAILABLE['gru'] = True
        MODEL_CLASSES['gru'] = GRU
    except: MODELS_AVAILABLE['gru'] = False

    try:
        from qlib.contrib.model.pytorch_alstm import ALSTM
        MODELS_AVAILABLE['alstm'] = True
        MODEL_CLASSES['alstm'] = ALSTM
    except: MODELS_AVAILABLE['alstm'] = False

    try:
        from qlib.contrib.model.pytorch_transformer import Transformer
        MODELS_AVAILABLE['transformer'] = True
        MODEL_CLASSES['transformer'] = Transformer
    except: MODELS_AVAILABLE['transformer'] = False

    try:
        from qlib.contrib.model.pytorch_tcn import TCN
        MODELS_AVAILABLE['tcn'] = True
        MODEL_CLASSES['tcn'] = TCN
    except: MODELS_AVAILABLE['tcn'] = False

    try:
        from qlib.contrib.model.pytorch_adarnn import ADARNN
        MODELS_AVAILABLE['adarnn'] = True
        MODEL_CLASSES['adarnn'] = ADARNN
    except: MODELS_AVAILABLE['adarnn'] = False

    try:
        from qlib.contrib.model.pytorch_hist import HIST
        MODELS_AVAILABLE['hist'] = True
        MODEL_CLASSES['hist'] = HIST
    except: MODELS_AVAILABLE['hist'] = False

    try:
        from qlib.contrib.model.pytorch_tabnet import TabnetModel
        MODELS_AVAILABLE['tabnet'] = True
        MODEL_CLASSES['tabnet'] = TabnetModel
    except: MODELS_AVAILABLE['tabnet'] = False

    # SFM_Model is a raw nn.Module - wrap it with fit/predict interface
    try:
        from qlib.contrib.model.pytorch_sfm import SFM_Model
        import torch
        import torch.nn as nn
        import numpy as np
        import pandas as pd
        import copy
        from qlib.data.dataset.handler import DataHandlerLP
        from qlib.utils import get_or_create_path
        from qlib.log import get_module_logger

        class SFMWrapper:
            """Wrapper around SFM_Model to provide qlib-compatible fit/predict interface"""
            def __init__(self, d_feat=6, output_dim=1, freq_dim=10, hidden_size=64,
                         n_epochs=200, lr=0.001, batch_size=2000, early_stop=20,
                         metric='', loss='mse', optimizer='adam', GPU=0, seed=None, **kwargs):
                self.logger = get_module_logger("SFM")
                self.d_feat = d_feat
                self.output_dim = output_dim
                self.freq_dim = freq_dim
                self.hidden_size = hidden_size
                self.n_epochs = n_epochs
                self.lr = lr
                self.batch_size = batch_size
                self.early_stop = early_stop
                self.metric = metric
                self.optimizer = optimizer.lower()
                self.loss = loss
                self.fitted = False
                self.device = torch.device("cuda:%d" % GPU if torch.cuda.is_available() and GPU >= 0 else "cpu")
                if seed is not None:
                    np.random.seed(seed)
                    torch.manual_seed(seed)
                self.sfm_model = SFM_Model(d_feat=d_feat, output_dim=output_dim, freq_dim=freq_dim,
                                           hidden_size=hidden_size, device=self.device).to(self.device)
                if self.optimizer == 'adam':
                    self.train_optimizer = torch.optim.Adam(self.sfm_model.parameters(), lr=self.lr)
                else:
                    self.train_optimizer = torch.optim.SGD(self.sfm_model.parameters(), lr=self.lr)
                self.loss_fn = nn.MSELoss()

            def fit(self, dataset, evals_result=dict(), save_path=None):
                df_train, df_valid = dataset.prepare(["train", "valid"], col_set=["feature", "label"], data_key=DataHandlerLP.DK_L)
                if df_train.empty or df_valid.empty:
                    raise ValueError("Empty data")
                x_train, y_train = df_train["feature"], df_train["label"]
                x_valid, y_valid = df_valid["feature"], df_valid["label"]
                save_path = get_or_create_path(save_path)
                best_score, best_epoch, stop_steps = -np.inf, 0, 0
                self.fitted = True
                best_param = copy.deepcopy(self.sfm_model.state_dict())
                for step in range(self.n_epochs):
                    self.logger.info("Epoch%d:", step)
                    self._train_epoch(x_train, y_train)
                    train_score = self._eval_epoch(x_train, y_train)
                    val_score = self._eval_epoch(x_valid, y_valid)
                    self.logger.info("train %.6f, valid %.6f" % (train_score, val_score))
                    if val_score > best_score:
                        best_score = val_score
                        stop_steps = 0
                        best_epoch = step
                        best_param = copy.deepcopy(self.sfm_model.state_dict())
                    else:
                        stop_steps += 1
                        if stop_steps >= self.early_stop:
                            self.logger.info("early stop")
                            break
                self.logger.info("best score: %.6lf @ %d" % (best_score, best_epoch))
                self.sfm_model.load_state_dict(best_param)
                torch.save(best_param, save_path)

            def predict(self, dataset, segment="test"):
                if not self.fitted:
                    raise ValueError("model is not fitted yet!")
                x_test = dataset.prepare(segment, col_set="feature", data_key=DataHandlerLP.DK_I)
                index = x_test.index
                self.sfm_model.eval()
                x_values = x_test.values
                preds = []
                for begin in range(len(x_values))[::self.batch_size]:
                    end = min(begin + self.batch_size, len(x_values))
                    x_batch = torch.from_numpy(x_values[begin:end]).float().to(self.device)
                    with torch.no_grad():
                        pred = self.sfm_model(x_batch).detach().cpu().numpy()
                    preds.append(pred)
                return pd.Series(np.concatenate(preds), index=index)

            def _train_epoch(self, x_train, y_train):
                self.sfm_model.train()
                x_vals = x_train.values
                y_vals = np.squeeze(y_train.values)
                indices = np.arange(len(x_vals))
                np.random.shuffle(indices)
                for i in range(len(indices))[::self.batch_size]:
                    if len(indices) - i < self.batch_size:
                        break
                    batch = indices[i:i+self.batch_size]
                    feature = torch.from_numpy(x_vals[batch]).float().to(self.device)
                    label = torch.from_numpy(y_vals[batch]).float().to(self.device)
                    pred = self.sfm_model(feature)
                    loss = self.loss_fn(pred, label)
                    self.train_optimizer.zero_grad()
                    loss.backward()
                    torch.nn.utils.clip_grad_value_(self.sfm_model.parameters(), 3.0)
                    self.train_optimizer.step()

            def _eval_epoch(self, x, y):
                self.sfm_model.eval()
                x_vals = x.values
                y_vals = np.squeeze(y.values)
                preds = []
                for begin in range(len(x_vals))[::self.batch_size]:
                    end = min(begin + self.batch_size, len(x_vals))
                    feature = torch.from_numpy(x_vals[begin:end]).float().to(self.device)
                    with torch.no_grad():
                        pred = self.sfm_model(feature).detach().cpu().numpy()
                    preds.append(pred)
                preds = np.concatenate(preds)
                score = -np.mean((preds - y_vals) ** 2)  # negative MSE (higher is better)
                return score

        MODELS_AVAILABLE['sfm'] = True
        MODEL_CLASSES['sfm'] = SFMWrapper
    except Exception:
        MODELS_AVAILABLE['sfm'] = False

    try:
        from qlib.contrib.model.pytorch_gats import GATs
        MODELS_AVAILABLE['gats'] = True
        MODEL_CLASSES['gats'] = GATs
    except: MODELS_AVAILABLE['gats'] = False

    try:
        from qlib.contrib.model.pytorch_add import ADD
        MODELS_AVAILABLE['add'] = True
        MODEL_CLASSES['add'] = ADD
    except: MODELS_AVAILABLE['add'] = False

    try:
        from qlib.contrib.model.double_ensemble import DEnsembleModel
        MODELS_AVAILABLE['densemble'] = True
        MODEL_CLASSES['densemble'] = DEnsembleModel
    except: MODELS_AVAILABLE['densemble'] = False

# Data handler imports
HANDLERS_AVAILABLE = {}
if QLIB_AVAILABLE:
    try:
        from qlib.contrib.data.handler import Alpha158, Alpha360, Alpha158vwap, Alpha360vwap
        HANDLERS_AVAILABLE['Alpha158'] = Alpha158
        HANDLERS_AVAILABLE['Alpha360'] = Alpha360
        HANDLERS_AVAILABLE['Alpha158vwap'] = Alpha158vwap
        HANDLERS_AVAILABLE['Alpha360vwap'] = Alpha360vwap
    except Exception as e:
        pass

# Strategy imports
STRATEGIES_AVAILABLE = {}
if QLIB_AVAILABLE:
    try:
        from qlib.contrib.strategy.signal_strategy import (
            TopkDropoutStrategy, WeightStrategyBase,
            EnhancedIndexingStrategy, BaseSignalStrategy
        )
        STRATEGIES_AVAILABLE['topk_dropout'] = TopkDropoutStrategy
        STRATEGIES_AVAILABLE['weight_base'] = WeightStrategyBase
        STRATEGIES_AVAILABLE['enhanced_indexing'] = EnhancedIndexingStrategy
        STRATEGIES_AVAILABLE['base_signal'] = BaseSignalStrategy
    except: pass

# Backtest imports
if QLIB_AVAILABLE:
    try:
        from qlib.backtest.executor import SimulatorExecutor
        from qlib.backtest.exchange import Exchange
        from qlib.backtest.position import Position
        BACKTEST_AVAILABLE = True
    except:
        BACKTEST_AVAILABLE = False

# Report/Analysis imports
if QLIB_AVAILABLE:
    try:
        from qlib.contrib.report import analysis_position, analysis_model
        ANALYSIS_AVAILABLE = True
    except:
        ANALYSIS_AVAILABLE = False


class QlibService:
    """
    Full-featured Qlib service with comprehensive quantitative research capabilities.

    Features:
    - 15+ Pre-trained models (Tree-based, Neural Networks, Ensemble)
    - Multiple data handlers (Alpha158, Alpha360, VWAP variants)
    - Complete backtesting system with exchange simulation
    - Portfolio strategies (TopK, Enhanced Indexing, Weight-based)
    - Risk analysis and performance reporting
    - Experiment management with MLflow
    """

    def __init__(self, provider_uri: str = "~/.qlib/qlib_data/us_data", region: str = "us"):
        self.trained_models = {}
        self.experiment_manager = None
        self.initialized = False

        if QLIB_AVAILABLE:
            try:
                reg = REG_US if region.lower() == "us" else REG_CN
                # Keep experiment tracking under ~/.fincept instead of creating a
                # ./mlruns folder in whatever directory the process happens to start in.
                mlruns = Path.home() / ".fincept" / "mlruns"
                qlib.init(provider_uri=provider_uri, region=reg,
                          exp_manager={"class": "MLflowExpManager",
                                       "module_path": "qlib.workflow.expm",
                                       "kwargs": {"uri": mlruns.as_uri(),
                                                  "default_exp_name": "Experiment"}})
                self.initialized = True
            except Exception as e:
                # qlib.init may fail if data is not downloaded yet
                self.init_error = str(e)
                self.initialized = False

    # ── Model persistence ───────────────────────────────────────────────────
    # Every CLI call is a fresh process, so ``self.trained_models`` is empty again by
    # the time the user clicks Backtest / Feature Importance with the ID that TRAIN
    # printed ("Model <id> not found"). Trained models are therefore pickled to disk
    # and re-loaded on demand.
    MODELS_DIR = Path.home() / ".fincept" / "qlib_models"
    _MODEL_ID_RE = re.compile(r"^[A-Za-z0-9_.-]{1,80}$")
    _DATA_MISSING_MSG = ("The qlib market dataset is not downloaded. Run: "
                         "python -m qlib.run.get_data qlib_data "
                         "--target_dir ~/.qlib/qlib_data/us_data --region us")

    def _persist_model(self, model_id: str, info: Dict[str, Any]) -> None:
        """Best-effort: a read-only home directory must not fail a successful training run."""
        try:
            self.MODELS_DIR.mkdir(parents=True, exist_ok=True)
            info["model"].to_pickle(str(self.MODELS_DIR / f"{model_id}.pkl"))
            meta = {k: v for k, v in info.items() if k != "model"}
            (self.MODELS_DIR / f"{model_id}.json").write_text(
                json.dumps(meta, default=str, indent=2), encoding="utf-8")
        except Exception as e:
            print(f"Warning: could not persist model {model_id}: {e}", file=sys.stderr)

    def _get_model_info(self, model_id: Optional[str]) -> Optional[Dict[str, Any]]:
        """Return the in-memory entry for model_id, loading it from disk if needed."""
        if not model_id or not isinstance(model_id, str):
            return None
        if model_id in self.trained_models:
            return self.trained_models[model_id]
        if not self._MODEL_ID_RE.match(model_id):      # the id becomes a file name
            return None
        pkl = self.MODELS_DIR / f"{model_id}.pkl"
        meta = self.MODELS_DIR / f"{model_id}.json"
        if not (pkl.exists() and meta.exists()):
            return None
        try:
            from qlib.utils.serial import Serializable
            info = json.loads(meta.read_text(encoding="utf-8"))
            info["model"] = Serializable.load(str(pkl))
            self.trained_models[model_id] = info
            return info
        except Exception as e:
            print(f"Warning: could not load model {model_id}: {e}", file=sys.stderr)
            return None

    def _data_error(self) -> Optional[Dict[str, Any]]:
        """Friendly failure when qlib has no market data, instead of qlib's own
        "can't find a freq from [] that can resample to day!"."""
        if not self.initialized:
            return {"success": False, "error": f"Qlib is not initialised. {self._DATA_MISSING_MSG}"}
        try:
            empty = len(D.calendar(freq="day")) == 0
        except Exception:
            empty = True
        if empty:
            return {"success": False, "error": self._DATA_MISSING_MSG}
        return None

    @staticmethod
    def _instrument_arg(instruments):
        """qlib wants a market-pool name as a string and tickers as a lowercase list."""
        pool_names = {"sp500", "nasdaq100", "all", "csi300", "csi500", "csi100"}
        if isinstance(instruments, str):
            return instruments.lower()
        instruments = [str(i).lower() for i in (instruments or [])]
        if len(instruments) == 1 and instruments[0] in pool_names:
            return instruments[0]
        return instruments

    def _build_test_dataset(self, info: Dict[str, Any], start_date: str, end_date: str):
        """DatasetH over [start_date, end_date] with the model's own handler / universe."""
        dataset_config = {
            "class": "DatasetH",
            "module_path": "qlib.data.dataset",
            "kwargs": {
                "handler": {
                    "class": info.get("handler", "Alpha158"),
                    "module_path": "qlib.contrib.data.handler",
                    "kwargs": {
                        "instruments": self._instrument_arg(info.get("instruments")),
                        "start_time": start_date,
                        "end_time": end_date,
                    },
                },
                "segments": {"test": (start_date, end_date)},
            },
        }
        return init_instance_by_config(dataset_config)

    def list_models(self) -> Dict[str, Any]:
        """List all available pre-trained models with detailed information"""
        models = [
            {
                "id": "lightgbm",
                "name": "LightGBM",
                "description": "Fast gradient boosting model - Industry standard baseline with excellent performance",
                "type": "tree_based",
                "available": MODELS_AVAILABLE.get('lightgbm', False),
                "features": ["Fast training", "High accuracy", "Feature importance", "GPU support"],
                "use_cases": ["General prediction", "Alpha generation", "Factor analysis", "Quick prototyping"],
                "hyperparameters": {
                    "num_leaves": 210,
                    "max_depth": 8,
                    "learning_rate": 0.05,
                    "n_estimators": 500
                }
            },
            {
                "id": "xgboost",
                "name": "XGBoost",
                "description": "Extreme Gradient Boosting - Robust tree-based model",
                "type": "tree_based",
                "available": MODELS_AVAILABLE.get('xgboost', False),
                "features": ["Regularization", "Parallel processing", "Cross-validation"],
                "use_cases": ["Competition winning", "Feature selection", "Ranking"],
                "hyperparameters": {
                    "max_depth": 6,
                    "learning_rate": 0.1,
                    "n_estimators": 100
                }
            },
            {
                "id": "catboost",
                "name": "CatBoost",
                "description": "Gradient boosting optimized for categorical features",
                "type": "tree_based",
                "available": MODELS_AVAILABLE.get('catboost', False),
                "features": ["Categorical handling", "Low overfitting", "Fast prediction"],
                "use_cases": ["Mixed data types", "Sector analysis", "Production systems"],
                "hyperparameters": {
                    "depth": 6,
                    "learning_rate": 0.03,
                    "iterations": 1000
                }
            },
            {
                "id": "linear",
                "name": "Linear Model",
                "description": "Simple linear regression baseline",
                "type": "linear",
                "available": MODELS_AVAILABLE.get('linear', False),
                "features": ["Interpretable", "Fast", "Stable"],
                "use_cases": ["Baseline comparison", "Factor regression", "Risk models"],
                "hyperparameters": {
                    "alpha": 0.001
                }
            },
            {
                "id": "lstm",
                "name": "LSTM",
                "description": "Long Short-Term Memory network for sequential pattern recognition",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('lstm', False),
                "features": ["Sequential patterns", "Memory cells", "Long-range dependencies"],
                "use_cases": ["Trend prediction", "Time-series forecasting", "Pattern recognition"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "num_layers": 2,
                    "dropout": 0.1
                }
            },
            {
                "id": "gru",
                "name": "GRU",
                "description": "Gated Recurrent Unit - Simpler alternative to LSTM",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('gru', False),
                "features": ["Faster training", "Fewer parameters", "Good performance"],
                "use_cases": ["Real-time prediction", "Short sequences", "Efficiency-focused"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "num_layers": 2
                }
            },
            {
                "id": "alstm",
                "name": "ALSTM (Attention LSTM)",
                "description": "LSTM with attention mechanism for improved focus",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('alstm', False),
                "features": ["Attention mechanism", "Better long-range", "Interpretable"],
                "use_cases": ["Complex patterns", "Multi-factor analysis", "Research"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "num_layers": 2,
                    "attention_type": "multihead"
                }
            },
            {
                "id": "transformer",
                "name": "Transformer",
                "description": "Attention-based architecture for complex pattern recognition",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('transformer', False),
                "features": ["Self-attention", "Parallel processing", "State-of-the-art"],
                "use_cases": ["Complex patterns", "Cross-asset analysis", "Multi-factor"],
                "hyperparameters": {
                    "d_model": 64,
                    "nhead": 4,
                    "num_layers": 2
                }
            },
            {
                "id": "tcn",
                "name": "TCN (Temporal Convolutional Network)",
                "description": "Convolutional network designed for time-series data",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('tcn', False),
                "features": ["Parallel processing", "Long memory", "Fast training"],
                "use_cases": ["High-frequency data", "Pattern recognition", "Real-time"],
                "hyperparameters": {
                    "num_channels": [64, 64],
                    "kernel_size": 3,
                    "dropout": 0.2
                }
            },
            {
                "id": "adarnn",
                "name": "AdaRNN (Adaptive RNN)",
                "description": "Adaptive RNN for distribution shift (auto-uses Alpha360 handler)",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('adarnn', False),
                "features": ["Adaptive learning", "Distribution shift handling", "Alpha360 handler"],
                "use_cases": ["Regime changes", "Non-stationary data", "Dynamic markets"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "num_layers": 2
                }
            },
            {
                "id": "hist",
                "name": "HIST (Historical Attention)",
                "description": "Historical attention model - requires CSI300 stock2concept data (CN market)",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('hist', False),
                "features": ["Historical attention", "Concept-oriented", "CSI300 market"],
                "use_cases": ["Chinese market prediction", "Alpha generation", "Research"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "num_layers": 2
                }
            },
            {
                "id": "tabnet",
                "name": "TabNet",
                "description": "Attention-based neural network for tabular data with interpretability",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('tabnet', False),
                "features": ["Interpretable", "Feature selection", "Attention masks"],
                "use_cases": ["Factor analysis", "Feature importance", "Explainable AI"],
                "hyperparameters": {
                    "n_d": 8,
                    "n_a": 8,
                    "n_steps": 3
                }
            },
            {
                "id": "sfm",
                "name": "SFM (State Frequency Memory)",
                "description": "Frequency domain analysis for financial time-series (auto-uses Alpha360 handler)",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('sfm', False),
                "features": ["Frequency analysis", "State memory", "Alpha360 handler"],
                "use_cases": ["Cyclical patterns", "Seasonality", "Multi-frequency"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "freq_dim": 10
                }
            },
            {
                "id": "gats",
                "name": "GATs (Graph Attention Networks)",
                "description": "Graph neural network for stock relationships (auto-uses Alpha360 handler)",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('gats', False),
                "features": ["Graph structure", "Relationship modeling", "Alpha360 handler"],
                "use_cases": ["Stock relationships", "Sector analysis", "Network effects"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "num_layers": 2
                }
            },
            {
                "id": "add",
                "name": "ADD (Adversarial Decomposition)",
                "description": "Adversarial decomposition model separating excess/market returns (auto-uses Alpha360 handler)",
                "type": "neural_network",
                "available": MODELS_AVAILABLE.get('add', False),
                "features": ["Return decomposition", "Adversarial training", "Alpha360 handler"],
                "use_cases": ["Alpha extraction", "Market-neutral signals", "Excess return prediction"],
                "hyperparameters": {
                    "hidden_size": 64,
                    "num_layers": 2
                }
            },
            {
                "id": "densemble",
                "name": "Double Ensemble",
                "description": "Ensemble model combining multiple approaches",
                "type": "ensemble",
                "available": MODELS_AVAILABLE.get('densemble', False),
                "features": ["Model combination", "Robust prediction", "Reduced variance"],
                "use_cases": ["Production systems", "Risk reduction", "Stable returns"],
                "hyperparameters": {
                    "base_models": ["lightgbm", "lstm"]
                }
            }
        ]

        return {
            "success": True,
            "models": models,
            "count": len(models),
            "available_count": sum(1 for m in models if m.get('available', False)),
            "model_types": {
                "tree_based": ["lightgbm", "xgboost", "catboost"],
                "linear": ["linear"],
                "neural_network": ["lstm", "gru", "alstm", "transformer", "tcn", "adarnn", "hist", "tabnet", "sfm", "gats", "add"],
                "ensemble": ["densemble"]
            }
        }

    def get_data_handlers(self) -> Dict[str, Any]:
        """Get available data handlers (factor libraries)"""
        handlers = {
            "Alpha158": {
                "description": "158 technical factors including momentum, volatility, volume indicators",
                "factor_count": 158,
                "category": "technical",
                "available": 'Alpha158' in HANDLERS_AVAILABLE,
                "factors": [
                    "MACD", "RSI", "BOLL", "ATR", "KDJ", "OBV",
                    "ROCP", "KLEN", "STD", "CORR", "CORD", "CNTP"
                ],
                "windows": [5, 10, 20, 30, 60]
            },
            "Alpha360": {
                "description": "360 factors with extended time windows and cross-sectional features",
                "factor_count": 360,
                "category": "technical",
                "available": 'Alpha360' in HANDLERS_AVAILABLE,
                "factors": [
                    "Extended momentum", "Multi-timeframe volatility",
                    "Cross-sectional rank", "Industry neutralized"
                ],
                "windows": [5, 10, 20, 30, 60, 120, 240]
            },
            "Alpha158vwap": {
                "description": "Alpha158 with VWAP (Volume Weighted Average Price) features",
                "factor_count": 158,
                "category": "technical_vwap",
                "available": 'Alpha158vwap' in HANDLERS_AVAILABLE,
                "factors": ["All Alpha158 factors", "VWAP deviation", "VWAP momentum"]
            },
            "Alpha360vwap": {
                "description": "Alpha360 with VWAP features",
                "factor_count": 360,
                "category": "technical_vwap",
                "available": 'Alpha360vwap' in HANDLERS_AVAILABLE,
                "factors": ["All Alpha360 factors", "VWAP extended"]
            }
        }

        return {
            "success": True,
            "handlers": handlers,
            "available_count": len(HANDLERS_AVAILABLE)
        }

    def get_factor_library(self) -> Dict[str, Any]:
        """List built-in Qlib alpha factors with their expressions.

        Pulls the real (name, expression) pairs from the Alpha158/Alpha360
        feature loaders when qlib is installed; falls back to a curated set
        of canonical alpha expressions otherwise so the UI is never empty.
        Returns {"success", "factors":[{name, category, expression}, ...]}.
        """
        factors: List[Dict[str, Any]] = []
        if QLIB_AVAILABLE:
            try:
                from qlib.contrib.data.loader import Alpha158DL, Alpha360DL
                for category, loader in (("Alpha158", Alpha158DL), ("Alpha360", Alpha360DL)):
                    fields, names = loader.get_feature_config()
                    for name, expr in zip(names, fields):
                        factors.append({
                            "name": name,
                            "category": category,
                            "expression": expr,
                        })
            except Exception as e:
                print(f"Falling back to curated factor library: {e}", file=sys.stderr)
                factors = []

        if not factors:
            # Curated fallback — canonical Qlib alpha expressions (Alpha158 style).
            curated = [
                ("KMID",  "($close-$open)/$open"),
                ("KLEN",  "($high-$low)/$open"),
                ("KUP",   "($high-Greater($open,$close))/$open"),
                ("KLOW",  "(Less($open,$close)-$low)/$open"),
                ("OPEN0", "$open/$close"),
                ("HIGH0", "$high/$close"),
                ("LOW0",  "$low/$close"),
                ("ROC5",  "Ref($close,5)/$close"),
                ("MA5",   "Mean($close,5)/$close"),
                ("MA20",  "Mean($close,20)/$close"),
                ("STD5",  "Std($close,5)/$close"),
                ("STD20", "Std($close,20)/$close"),
                ("BETA5", "Slope($close,5)/$close"),
                ("MAX5",  "Max($high,5)/$close"),
                ("MIN5",  "Min($low,5)/$close"),
                ("QTLU5", "Quantile($close,5,0.8)/$close"),
                ("QTLD5", "Quantile($close,5,0.2)/$close"),
                ("RSV5",  "($close-Min($low,5))/(Max($high,5)-Min($low,5)+1e-12)"),
                ("CORR5", "Corr($close,Log($volume+1),5)"),
                ("CORD5", "Corr($close/Ref($close,1),Log($volume/Ref($volume,1)+1),5)"),
                ("CNTP5", "Mean($close>Ref($close,1),5)"),
                ("SUMP5", "Sum(Greater($close-Ref($close,1),0),5)/(Sum(Abs($close-Ref($close,1)),5)+1e-12)"),
                ("VMA5",  "Mean($volume,5)/($volume+1e-12)"),
                ("VSTD5", "Std($volume,5)/($volume+1e-12)"),
            ]
            factors = [{"name": n, "category": "Alpha158", "expression": e} for n, e in curated]

        return {"success": True, "factors": factors, "count": len(factors)}

    def get_strategies(self) -> Dict[str, Any]:
        """Get available trading strategies"""
        strategies = {
            "topk_dropout": {
                "name": "TopK Dropout Strategy",
                "description": "Select top-K stocks based on signal with dropout mechanism",
                "available": 'topk_dropout' in STRATEGIES_AVAILABLE,
                "parameters": {
                    "topk": "Number of stocks to hold",
                    "n_drop": "Number of stocks to drop when rebalancing",
                    "signal_threshold": "Minimum signal strength"
                },
                "use_cases": ["Long-only portfolios", "Stock selection"]
            },
            "enhanced_indexing": {
                "name": "Enhanced Indexing Strategy",
                "description": "Beat the benchmark while tracking it closely",
                "available": 'enhanced_indexing' in STRATEGIES_AVAILABLE,
                "parameters": {
                    "benchmark": "Benchmark index to track",
                    "tracking_error": "Maximum tracking error allowed"
                },
                "use_cases": ["Index enhancement", "Low tracking error"]
            },
            "weight_base": {
                "name": "Weight-Based Strategy",
                "description": "Flexible weight allocation based on signals",
                "available": 'weight_base' in STRATEGIES_AVAILABLE,
                "parameters": {
                    "weight_method": "Method to convert signals to weights"
                },
                "use_cases": ["Custom weighting", "Risk parity"]
            }
        }

        return {
            "success": True,
            "strategies": strategies
        }

    def get_data(self,
                 instruments: List[str],
                 start_date: str,
                 end_date: str,
                 fields: Optional[List[str]] = None,
                 freq: str = "day") -> Dict[str, Any]:
        """
        Fetch market data using Qlib's data API.

        Args:
            instruments: List of stock symbols
            start_date: Start date (YYYY-MM-DD)
            end_date: End date (YYYY-MM-DD)
            fields: List of fields (e.g., ["$close", "$volume"])
            freq: Data frequency ('day', 'min')
        """
        data_err = self._data_error()
        if data_err:
            return data_err

        try:
            if fields is None:
                fields = ["$open", "$high", "$low", "$close", "$volume", "$vwap", "$factor"]

            # Normalize instruments to lowercase (qlib US data uses lowercase dirs)
            instruments = [i.lower() if isinstance(i, str) else i for i in instruments]

            # Clamp dates to available calendar range
            cal = D.calendar(freq=freq)
            cal_warning = None
            if len(cal) > 0:
                cal_start = str(cal[0].date())
                cal_end   = str(cal[-1].date())
                if start_date < cal_start:
                    cal_warning = f"start_date clamped from {start_date} to {cal_start}"
                    start_date = cal_start
                if end_date > cal_end:
                    cal_warning = (cal_warning or "") + f" end_date clamped from {end_date} to {cal_end}"
                    end_date = cal_end

            # Fetch data using Qlib's data API
            data = D.features(instruments, fields, start_date, end_date, freq=freq)

            # Convert to JSON-serializable format
            data_dict = {}
            total_records = 0

            for instrument in instruments:
                try:
                    if instrument in data.index.get_level_values(0):
                        inst_data = data.loc[instrument]
                        data_dict[instrument] = {
                            "dates": [str(d.date()) if hasattr(d, 'date') else str(d) for d in inst_data.index],
                            "data": {col: [float(v) if not pd.isna(v) else None for v in inst_data[col]]
                                    for col in inst_data.columns}
                        }
                        total_records += len(inst_data)
                except Exception as e:
                    data_dict[instrument] = {"error": str(e)}

            result = {
                "success": True,
                "data": data_dict,
                "instruments": instruments,
                "date_range": {"start": start_date, "end": end_date},
                "fields": fields,
                "freq": freq,
                "total_records": total_records
            }
            if cal_warning:
                result["warning"] = cal_warning
            return result
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to fetch data: {str(e)}"
            }

    def create_dataset(self,
                       instruments: Union[str, List[str]],
                       start_time: str,
                       end_time: str,
                       handler_type: str = "Alpha158",
                       segments: Optional[Dict[str, tuple]] = None) -> Dict[str, Any]:
        """
        Create a Qlib dataset with proper train/valid/test splits.

        Args:
            instruments: Stock pool (e.g., 'csi300' or list of symbols)
            start_time: Data start time
            end_time: Data end time
            handler_type: Data handler ('Alpha158', 'Alpha360', etc.)
            segments: Time segments for train/valid/test
        """
        if not self.initialized:
            return {"success": False, "error": "Qlib not initialized"}

        if handler_type not in HANDLERS_AVAILABLE:
            return {"success": False, "error": f"Handler {handler_type} not available"}

        try:
            handler_class = HANDLERS_AVAILABLE[handler_type]

            # Create dataset config
            dataset_config = {
                "class": "DatasetH",
                "module_path": "qlib.data.dataset",
                "kwargs": {
                    "handler": {
                        "class": handler_type,
                        "module_path": "qlib.contrib.data.handler",
                        "kwargs": {
                            "instruments": instruments,
                            "start_time": start_time,
                            "end_time": end_time
                        }
                    },
                    "segments": segments or {
                        "train": (start_time, end_time)
                    }
                }
            }

            # Initialize dataset
            dataset = init_instance_by_config(dataset_config)

            return {
                "success": True,
                "message": "Dataset created successfully",
                "handler": handler_type,
                "instruments": instruments,
                "time_range": {"start": start_time, "end": end_time},
                "segments": segments or {"train": (start_time, end_time)}
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to create dataset: {str(e)}"
            }

    def train_model(self,
                    model_type: str,
                    instruments: Union[str, List[str]],
                    train_start: str,
                    train_end: str,
                    valid_start: str,
                    valid_end: str,
                    handler_type: str = "Alpha158",
                    model_config: Optional[Dict[str, Any]] = None,
                    experiment_name: Optional[str] = None) -> Dict[str, Any]:
        """
        Train a Qlib model with proper data pipeline.

        Args:
            model_type: Model type (lightgbm, lstm, transformer, etc.)
            instruments: Stock pool
            train_start/end: Training period
            valid_start/end: Validation period
            handler_type: Data handler type
            model_config: Model hyperparameters
            experiment_name: Name for experiment tracking
        """
        data_err = self._data_error()
        if data_err:
            return data_err

        model_type_lower = model_type.lower()
        if model_type_lower not in MODEL_CLASSES:
            return {
                "success": False,
                "error": f"Model type '{model_type}' not available. Available: {list(MODEL_CLASSES.keys())}"
            }

        # Normalize instruments: lowercase tickers; a one-element list holding a market pool
        # name (e.g. ["sp500"], what the panel sends for "sp500") becomes the pool string qlib expects.
        instruments = self._instrument_arg(instruments)
        if not instruments:
            return {"success": False, "error": "Enter at least one instrument"}

        try:
            # Models that require Alpha360 handler (they reshape features as d_feat=6, len_seq=60)
            ALPHA360_MODELS = {'adarnn', 'gats', 'add', 'sfm'}
            if model_type_lower in ALPHA360_MODELS:
                handler_type = 'Alpha360'

            # HIST requires CSI300 stock2concept mapping
            stock2concept_path = os.path.expanduser('~/.qlib/qlib_data/stock2concept.npy')
            stock_index_path = os.path.expanduser('~/.qlib/qlib_data/stock_index.npy')

            # Build handler kwargs
            handler_kwargs = {
                "instruments": instruments,
                "start_time": train_start,
                "end_time": valid_end
            }
            # Alpha360 requires fit_start_time and fit_end_time
            if handler_type in ('Alpha360', 'Alpha360vwap'):
                handler_kwargs["fit_start_time"] = train_start
                handler_kwargs["fit_end_time"] = train_end

            # Create dataset
            dataset_config = {
                "class": "DatasetH",
                "module_path": "qlib.data.dataset",
                "kwargs": {
                    "handler": {
                        "class": handler_type,
                        "module_path": "qlib.contrib.data.handler",
                        "kwargs": handler_kwargs
                    },
                    "segments": {
                        "train": (train_start, train_end),
                        "valid": (valid_start, valid_end),
                        "test": (valid_start, valid_end)
                    }
                }
            }

            dataset = init_instance_by_config(dataset_config)

            # Initialize model
            model_class = MODEL_CLASSES[model_type_lower]
            # d_feat must match handler: Alpha158=158, Alpha360 time-series models use d_feat=6
            if model_type_lower in ALPHA360_MODELS:
                d_feat = 6
            elif handler_type in ('Alpha158', 'Alpha158vwap'):
                d_feat = 158
            else:
                d_feat = 360
            default_configs = {
                'lightgbm': {'num_leaves': 210, 'max_depth': 8, 'learning_rate': 0.05},
                'xgboost': {'max_depth': 6, 'learning_rate': 0.1},
                # allow_writing_files=False: CatBoost otherwise drops a catboost_info/ log folder into
                # the process's working directory (the scripts tree).
                'catboost': {'depth': 6, 'learning_rate': 0.03, 'iterations': 1000,
                             'allow_writing_files': False},
                # LinearModel defaults to estimator='ols', which rejects an alpha; the old
                # {'alpha': 0.001} alone made every 'linear' training run fail.
                'linear': {'estimator': 'ridge', 'alpha': 0.001},
                'lstm': {'d_feat': d_feat, 'hidden_size': 64, 'num_layers': 2, 'batch_size': 2048, 'n_epochs': 50},
                'gru': {'d_feat': d_feat, 'hidden_size': 64, 'num_layers': 2, 'batch_size': 2048, 'n_epochs': 50},
                'alstm': {'d_feat': d_feat, 'hidden_size': 64, 'num_layers': 2, 'batch_size': 2048, 'n_epochs': 50},
                'transformer': {'d_feat': d_feat, 'd_model': 64, 'nhead': 4, 'batch_size': 2048, 'n_epochs': 50},
                'tcn': {'num_input': d_feat, 'output_size': 1},
                'adarnn': {'d_feat': 6, 'hidden_size': 64, 'num_layers': 2, 'n_epochs': 50, 'batch_size': 2048, 'len_seq': 60},
                'hist': {'d_feat': d_feat, 'hidden_size': 64, 'num_layers': 2, 'n_epochs': 50, 'base_model': 'GRU',
                         'stock2concept': stock2concept_path, 'stock_index': stock_index_path},
                'tabnet': {'d_feat': d_feat, 'out_dim': 64, 'final_out_dim': 1, 'n_epochs': 50, 'batch_size': 4096, 'pretrain': False},
                'sfm': {'d_feat': 6, 'output_dim': 1, 'freq_dim': 10, 'hidden_size': 64, 'n_epochs': 50, 'batch_size': 2048},
                'gats': {'d_feat': 6, 'hidden_size': 64, 'num_layers': 2, 'n_epochs': 50, 'base_model': 'GRU'},
                'add': {'d_feat': 6, 'hidden_size': 64, 'num_layers': 2, 'n_epochs': 50, 'batch_size': 2048, 'base_model': 'GRU'},
                'densemble': {},
            }

            config = default_configs.get(model_type_lower, {})
            if model_config:
                config.update(model_config)

            model = model_class(**config)

            # Train the model
            model.fit(dataset)

            # Generate predictions on test set
            predictions = model.predict(dataset)

            # Store trained model
            model_id = f"{model_type_lower}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            self.trained_models[model_id] = {
                "model": model,
                "type": model_type,
                "config": config,
                "handler": handler_type,
                "instruments": instruments,
                "train_period": [train_start, train_end],
                "valid_period": [valid_start, valid_end],
            }
            self._persist_model(model_id, self.trained_models[model_id])

            # Calculate basic metrics
            pred_count = len(predictions) if hasattr(predictions, '__len__') else 0

            return {
                "success": True,
                "message": f"{model_type} model trained successfully",
                "model_id": model_id,
                "model_type": model_type,
                "config": config,
                "handler": handler_type,
                "training_period": {"start": train_start, "end": train_end},
                "validation_period": {"start": valid_start, "end": valid_end},
                "predictions_count": pred_count
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Training failed: {str(e)}"
            }

    def predict(self,
                model_id: str,
                instruments: Union[str, List[str]],
                start_date: str,
                end_date: str) -> Dict[str, Any]:
        """Generate predictions using a trained model"""
        model_info = self._get_model_info(model_id)
        if model_info is None:
            return {"success": False, "error": f"Model {model_id} not found"}

        try:
            model = model_info["model"]
            handler_type = model_info["handler"]

            # Create prediction dataset
            dataset_config = {
                "class": "DatasetH",
                "module_path": "qlib.data.dataset",
                "kwargs": {
                    "handler": {
                        "class": handler_type,
                        "module_path": "qlib.contrib.data.handler",
                        "kwargs": {
                            "instruments": instruments,
                            "start_time": start_date,
                            "end_time": end_date
                        }
                    },
                    "segments": {
                        "test": (start_date, end_date)
                    }
                }
            }

            dataset = init_instance_by_config(dataset_config)
            predictions = model.predict(dataset)

            # Convert to serializable format
            pred_dict = predictions.to_dict() if hasattr(predictions, 'to_dict') else {"values": list(predictions)}

            return {
                "success": True,
                "model_id": model_id,
                "predictions": pred_dict,
                "period": {"start": start_date, "end": end_date}
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Prediction failed: {str(e)}"
            }

    def run_backtest(self,
                     model_id: str,
                     strategy_type: str = "topk_dropout",
                     benchmark: Optional[str] = None,
                     topk: int = 50,
                     n_drop: Optional[int] = None,
                     start_date: str = None,
                     end_date: str = None,
                     account: float = 100000000,
                     exchange_config: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Backtest a trained model with Qlib's daily backtester (TopkDropoutStrategy).

        This used to return a hard-coded dict (Sharpe 1.5, 15% annualised, ...) that
        was never computed from the model; the numbers below come from the real
        simulation or the call fails.

        Args:
            model_id: ID of trained model to use
            strategy_type: only "topk_dropout" is supported
            benchmark: benchmark instrument present in the qlib data (None = no benchmark)
            topk: Number of stocks to hold
            n_drop: Number of stocks to drop on rebalance
            start_date/end_date: Backtest period
            account: Initial account value
            exchange_config: Exchange configuration overrides
        """
        model_info = self._get_model_info(model_id)
        if model_info is None:
            return {"success": False, "error": f"Model {model_id} not found"}

        data_err = self._data_error()
        if data_err:
            return data_err

        if strategy_type != "topk_dropout":
            return {"success": False,
                    "error": f"Strategy '{strategy_type}' is not supported; use topk_dropout"}

        try:
            import numpy as np
            import pandas as pd
            from qlib.contrib.evaluate import backtest_daily, risk_analysis
            from qlib.contrib.strategy import TopkDropoutStrategy

            # Clamp to the local calendar: qlib's backtester raises an opaque
            # "index N is out of bounds" when the window reaches the last data day
            # (it looks one step ahead), so stop one trading day short of it.
            cal = D.calendar(freq="day")
            cal_start, cal_end = str(cal[0].date()), str(cal[-1].date())
            last_usable = str(cal[-2].date()) if len(cal) > 1 else cal_end
            start_date = max(start_date, cal_start) if start_date else str(cal[max(0, len(cal) - 253)].date())
            end_date = min(end_date, last_usable) if end_date else last_usable
            if start_date >= end_date:
                return {"success": False,
                        "error": f"Backtest window {start_date} -> {end_date} is empty; "
                                 f"local data covers {cal_start} -> {cal_end}"}
            topk = int(topk)
            n_drop = int(n_drop) if n_drop else max(1, topk // 5)

            # Scores for the whole backtest window from the model's own handler.
            dataset = self._build_test_dataset(model_info, start_date, end_date)
            pred = model_info["model"].predict(dataset)
            if isinstance(pred, pd.DataFrame):
                pred = pred.iloc[:, 0]
            if pred is None or len(pred) == 0:
                return {"success": False, "error": "The model produced no predictions for this period"}

            exchange_kwargs = {
                "limit_threshold": 0.095,
                "deal_price": "close",
                "open_cost": 0.0005,
                "close_cost": 0.0015,
                "min_cost": 5.0,
            }
            if exchange_config:
                exchange_kwargs.update(exchange_config)

            # benchmark=None does NOT mean "none" to qlib -- it falls back to SH000300,
            # which does not exist in US data. A flat zero-return series disables it.
            bench_arg = benchmark
            if not bench_arg:
                bench_arg = pd.Series(
                    0.0, index=pd.DatetimeIndex(D.calendar(start_time=start_date, end_time=end_date, freq="day")))

            strategy = TopkDropoutStrategy(signal=pred, topk=topk, n_drop=n_drop)
            report, _positions = backtest_daily(
                start_time=start_date, end_time=end_date, strategy=strategy, account=account,
                benchmark=bench_arg, exchange_kwargs=exchange_kwargs)

            net = (report["return"] - report["cost"]).astype(float)
            bench = report["bench"].astype(float)
            excess = net - bench
            n_days = int(len(net))
            ann = 252.0
            vol = float(net.std() * np.sqrt(ann)) if n_days > 1 else 0.0
            equity = (1 + net).cumprod()
            drawdown = equity / equity.cummax() - 1
            max_dd = float(drawdown.min()) if n_days else 0.0
            ann_ret = float(equity.iloc[-1] ** (ann / n_days) - 1) if n_days else 0.0
            downside = net[net < 0]
            downside_vol = float(downside.std() * np.sqrt(ann)) if len(downside) > 1 else 0.0
            ex_risk = risk_analysis(excess)["risk"] if n_days > 1 else None
            bench_var = float(bench.var())
            beta = float(net.cov(bench) / bench_var) if bench_var > 0 else None

            metrics = {
                "total_return": float(equity.iloc[-1] - 1),
                "annualized_return": ann_ret,
                "volatility": vol,
                "sharpe_ratio": float(net.mean() / net.std() * np.sqrt(ann)) if net.std() > 0 else 0.0,
                "sortino_ratio": float(net.mean() * ann / downside_vol) if downside_vol > 0 else None,
                "max_drawdown": max_dd,
                "calmar_ratio": float(ann_ret / abs(max_dd)) if max_dd < 0 else None,
                "win_rate": float((net > 0).mean()),
                "excess_annualized_return": float(ex_risk["annualized_return"]) if ex_risk is not None else None,
                "information_ratio": float(ex_risk["information_ratio"]) if ex_risk is not None else None,
                "beta": beta,
                "avg_daily_turnover": float(report["turnover"].mean()) if "turnover" in report else None,
                "total_cost": float(report["cost"].sum()),
                "trading_days": n_days,
            }
            step = max(1, n_days // 250)
            bench_curve = (1 + bench).cumprod()
            curve = [{"date": str(d.date()), "portfolio": float(e), "benchmark": float(b)}
                     for d, e, b in zip(equity.index[::step], equity.values[::step],
                                        bench_curve.values[::step])]
            return {
                "success": True,
                "model_id": model_id,
                "strategy": strategy_type,
                "benchmark": benchmark,
                "topk": topk,
                "n_drop": n_drop,
                "period": {"start": start_date, "end": end_date},
                "metrics": metrics,
                "equity_curve": curve,
                "message": f"Backtest of {model_id} completed over {n_days} trading days"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Backtest failed: {str(e)}"
            }

    def get_factor_analysis(self,
                           model_id: Optional[str] = None,
                           analysis_type: str = "ic",
                           instruments=None,
                           start_date: Optional[str] = None,
                           end_date: Optional[str] = None,
                           top_n: int = 40) -> Dict[str, Any]:
        """
        Information-coefficient analysis.

        * With ``model_id``: daily IC / Rank-IC of that model's scores against the
          label on its validation period.
        * Without: per-factor rank-IC table over the built-in Alpha158 factor library
          for ``instruments`` / ``start_date`` / ``end_date`` (what the Live Signals
          Factor Analysis tab asks for -- it has no model field).

        This used to return fixed numbers (IC 0.045, ICIR 3.75) for any model.
        """
        data_err = self._data_error()
        if data_err:
            return data_err

        try:
            import numpy as np
            import pandas as pd
            from qlib.data.dataset.handler import DataHandlerLP

            if model_id:
                info = self._get_model_info(model_id)
                if info is None:
                    return {"success": False, "error": f"Model {model_id} not found"}
                v_start, v_end = (info.get("valid_period") or [start_date, end_date])
                if not v_start or not v_end:
                    return {"success": False, "error": "Model has no recorded validation period"}
                dataset = self._build_test_dataset(info, v_start, v_end)
                pred = info["model"].predict(dataset)
                if isinstance(pred, pd.DataFrame):
                    pred = pred.iloc[:, 0]
                label = dataset.prepare("test", col_set="label", data_key=DataHandlerLP.DK_L)
                label = label.iloc[:, 0] if isinstance(label, pd.DataFrame) else label
                frame = pd.concat([pred.rename("score"), label.rename("label")], axis=1).dropna()
                if frame.empty:
                    return {"success": False, "error": "No overlapping predictions and labels to evaluate"}
                by_day = frame.groupby(level="datetime")
                ic = by_day.apply(lambda g: g["score"].corr(g["label"])).dropna()
                ric = by_day.apply(lambda g: g["score"].corr(g["label"], method="spearman")).dropna()
                if len(ic) == 0:
                    return {"success": False, "error": "Cross-sections too small to compute an IC"}
                results = {
                    "IC_mean": float(ic.mean()), "IC_std": float(ic.std()),
                    "ICIR": float(ic.mean() / ic.std()) if ic.std() > 0 else 0.0,
                    "Rank_IC_mean": float(ric.mean()), "Rank_IC_std": float(ric.std()),
                    "Rank_ICIR": float(ric.mean() / ric.std()) if ric.std() > 0 else 0.0,
                    "days": int(len(ic)),
                }
                return {"success": True, "model_id": model_id, "analysis_type": "ic",
                        "period": {"start": v_start, "end": v_end}, "results": results}

            # -- Factor-library IC table ----------------------------------------
            universe = self._instrument_arg(instruments)
            if not universe:
                return {"success": False, "error": "Enter at least one instrument"}
            if isinstance(universe, list) and len(universe) < 5:
                return {"success": False,
                        "error": "Cross-sectional factor IC needs at least 5 instruments "
                                 "(or a market pool such as sp500)"}
            cal = D.calendar(freq="day")
            end_date = end_date or str(cal[-1].date())
            start_date = start_date or str(cal[max(0, len(cal) - 253)].date())

            from qlib.contrib.data.handler import Alpha158
            handler = Alpha158(instruments=universe, start_time=start_date, end_time=end_date,
                               fit_start_time=start_date, fit_end_time=end_date)
            feats = handler.fetch(col_set="feature", data_key=DataHandlerLP.DK_R)
            label = handler.fetch(col_set="label", data_key=DataHandlerLP.DK_R).iloc[:, 0]
            feats = feats.replace([np.inf, -np.inf], np.nan)
            ic_by_day = feats.groupby(level="datetime").apply(
                lambda g: g.corrwith(label.loc[g.index], method="spearman"))
            ic_by_day = ic_by_day.dropna(how="all")
            if ic_by_day.empty:
                return {"success": False, "error": "No dates with enough instruments to compute factor IC"}
            mean = ic_by_day.mean()
            std = ic_by_day.std()
            icir = (mean / std.replace(0, np.nan))
            order = mean.abs().sort_values(ascending=False).index[:int(top_n)]
            factors = [{
                "name": str(name),
                "ic": float(mean[name]),
                # ICIR, also kept under the legacy "sharpe" key the display reads
                "sharpe": float(icir[name]) if np.isfinite(icir[name]) else None,
                "icir": float(icir[name]) if np.isfinite(icir[name]) else None,
                "ic_std": float(std[name]) if np.isfinite(std[name]) else None,
                "positive_rate": float((ic_by_day[name].dropna() > 0).mean()),
            } for name in order]
            return {
                "success": True,
                "analysis_type": "factor_ic",
                "date_range": {"start": start_date, "end": end_date},
                "n_days": int(len(ic_by_day)),
                "n_factors": int(feats.shape[1]),
                "instruments": universe if isinstance(universe, list) else [universe],
                "factors": factors,
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Analysis failed: {str(e)}"
            }

    def get_feature_importance(self, model_id: str) -> Dict[str, Any]:
        """Get feature importance for tree-based models"""
        model_info = self._get_model_info(model_id)
        if model_info is None:
            return {"success": False, "error": f"Model {model_id} not found"}

        try:
            model = model_info["model"]

            # Get feature importance if available
            if hasattr(model, 'get_feature_importance'):
                importance = model.get_feature_importance()
                # qlib returns a pandas Series; json.dumps cannot serialise it (the
                # old code raised "Object of type Series is not JSON serializable").
                if hasattr(importance, "to_dict"):
                    importance = importance.to_dict()
                importance = {str(k): float(v) for k, v in importance.items()}
                # LightGBM labels features "Column_<n>" and XGBoost "f<n>"; map them back to
                # the handler's factor names (KMID, ROC5, ...) so the table is readable.
                if importance and all(re.fullmatch(r"(?:Column_|f)\d+", k) for k in importance):
                    try:
                        from qlib.contrib.data.loader import Alpha158DL, Alpha360DL
                        loader = Alpha360DL if str(model_info.get("handler", "")).startswith("Alpha360")                             else Alpha158DL
                        names = loader.get_feature_config()[1]
                        def _col(k):
                            n = int(re.sub(r"\D", "", k))
                            return names[n] if n < len(names) else k
                        importance = {_col(k): v for k, v in importance.items()}
                    except Exception:
                        pass
                return {
                    "success": True,
                    "model_id": model_id,
                    "feature_importance": importance
                }
            else:
                return {
                    "success": False,
                    "error": "Feature importance is only available for tree-based models "
                             f"(this model is '{model_info.get('type', 'unknown')}')"
                }
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to get feature importance: {str(e)}"
            }

    def save_model(self, model_id: str, path: str) -> Dict[str, Any]:
        """Save a trained model to disk"""
        model_info = self._get_model_info(model_id)
        if model_info is None:
            return {"success": False, "error": f"Model {model_id} not found"}

        try:
            model = model_info["model"]

            # Use Qlib's built-in serialization
            model.to_pickle(path)

            return {
                "success": True,
                "model_id": model_id,
                "path": path,
                "message": "Model saved successfully"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to save model: {str(e)}"
            }

    def load_model(self,
                   model_type: str,
                   path: str,
                   model_id: Optional[str] = None) -> Dict[str, Any]:
        """Load a saved model from disk"""
        model_type_lower = model_type.lower()
        if model_type_lower not in MODEL_CLASSES:
            return {"success": False, "error": f"Unknown model type: {model_type}"}

        try:
            model_class = MODEL_CLASSES[model_type_lower]
            model = model_class.load(path)

            # Generate model ID if not provided
            if not model_id:
                model_id = f"{model_type_lower}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

            self.trained_models[model_id] = {
                "model": model,
                "type": model_type,
                "loaded_from": path
            }

            return {
                "success": True,
                "model_id": model_id,
                "path": path,
                "message": "Model loaded successfully"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to load model: {str(e)}"
            }

    def get_calendar(self,
                     start_date: str,
                     end_date: str,
                     freq: str = "day") -> Dict[str, Any]:
        """Get trading calendar"""
        data_err = self._data_error()
        if data_err:
            return data_err

        try:
            calendar = D.calendar(start_time=start_date, end_time=end_date, freq=freq)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in calendar]

            return {
                "success": True,
                "dates": dates,
                "count": len(dates),
                "freq": freq
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to get calendar: {str(e)}"
            }

    def get_instruments(self,
                        market: str = "all",
                        start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get instruments list for a market index"""
        if not self.initialized:
            return {"success": False, "error": "Qlib not initialized"}

        try:
            instruments = D.instruments(market=market)

            # If instruments is a filter, get the actual list
            if hasattr(instruments, 'to_list'):
                instrument_list = instruments.to_list()
            elif isinstance(instruments, (list, tuple)):
                instrument_list = list(instruments)
            else:
                instrument_list = []

            if not instrument_list:
                return {
                    "success": False,
                    "error": (f"No instruments found for market '{market}'. "
                              "The qlib market dataset is not downloaded. Run: "
                              "python -m qlib.run.get_data qlib_data "
                              "--target_dir ~/.qlib/qlib_data/us_data --region us"),
                }

            return {
                "success": True,
                "market": market,
                "instruments": instrument_list,
                "count": len(instrument_list)
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Failed to get instruments: {str(e)}"
            }

    def calculate_ic(self,
                     predictions: Dict[str, Any],
                     actuals: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate Information Coefficient between predictions and actual returns"""
        try:
            import numpy as np
            from scipy import stats

            pred_values = list(predictions.values())
            actual_values = list(actuals.values())

            # Pearson IC
            ic, _ = stats.pearsonr(pred_values, actual_values)

            # Rank IC (Spearman)
            rank_ic, _ = stats.spearmanr(pred_values, actual_values)

            return {
                "success": True,
                "IC": float(ic),
                "Rank_IC": float(rank_ic)
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"IC calculation failed: {str(e)}"
            }


# Import pandas for data handling
try:
    import pandas as pd
except ImportError:
    pd = None


def _json_safe(obj):
    """Replace NaN / +-Infinity with None before json.dumps.

    Python emits them as bare ``NaN`` / ``Infinity`` tokens, which are not JSON: the
    terminal's parser rejects the WHOLE payload ("malformed JSON") over one empty cell.
    """
    if isinstance(obj, float):
        return obj if obj == obj and obj not in (float('inf'), float('-inf')) else None
    if isinstance(obj, dict):
        return {k: _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_json_safe(v) for v in obj]
    return obj


def main():
    """CLI interface for Qlib service"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    service = QlibService()

    try:
        if command == "check_status":
            result = {
                "success": True,
                "qlib_available": QLIB_AVAILABLE,
                "version": qlib.__version__ if QLIB_AVAILABLE else None,
                "models_available": MODELS_AVAILABLE,
                "handlers_available": list(HANDLERS_AVAILABLE.keys()),
                "strategies_available": list(STRATEGIES_AVAILABLE.keys())
            }

        elif command == "list_models":
            result = service.list_models()

        elif command == "get_data_handlers":
            result = service.get_data_handlers()

        elif command == "get_factor_library":
            result = service.get_factor_library()

        elif command == "get_strategies":
            result = service.get_strategies()

        elif command == "get_data":
            params = json.loads(sys.argv[2])
            result = service.get_data(
                params.get("instruments", []),
                params.get("start_date"),
                params.get("end_date"),
                params.get("fields"),
                params.get("freq", "day")
            )

        elif command == "create_dataset":
            params = json.loads(sys.argv[2])
            result = service.create_dataset(
                params.get("instruments"),
                params.get("start_time"),
                params.get("end_time"),
                params.get("handler_type", "Alpha158"),
                params.get("segments")
            )

        elif command == "train_model":
            params = json.loads(sys.argv[2])
            # dataset_config may be a JSON string from the frontend
            dataset_config = params.get("dataset_config")
            if isinstance(dataset_config, str):
                dataset_config = json.loads(dataset_config)
            elif not dataset_config:
                dataset_config = {}
            # model_config may also be a JSON string
            model_config = params.get("model_config")
            if isinstance(model_config, str):
                model_config = json.loads(model_config)
            # Extract instruments and dates from dataset_config or params. The panel
            # sends start_date / end_date (the whole window); train_start / valid_end
            # are the explicit spellings. Reading only the latter made every panel run
            # die in strptime(None, ...).
            instruments = dataset_config.get("instruments") or params.get("instruments") or []
            start_time = (dataset_config.get("start_time") or params.get("train_start")
                          or params.get("start_date"))
            end_time = (dataset_config.get("end_time") or params.get("valid_end")
                        or params.get("end_date"))
            if not start_time or not end_time:
                # No window given: use the most recent ~3 years of the local calendar.
                data_err = service._data_error()
                if data_err:
                    print(json.dumps(data_err))
                    return
                cal = D.calendar(freq="day")
                end_time = end_time or str(cal[-1].date())
                start_time = start_time or str(cal[max(0, len(cal) - 757)].date())
            # Compute 80/20 train/valid split if no explicit split provided
            train_start = params.get("train_start") or start_time
            valid_end = params.get("valid_end") or end_time
            if params.get("train_end") and params.get("valid_start"):
                train_end = params["train_end"]
                valid_start = params["valid_start"]
            else:
                # Auto-split: 80% train, 20% validation
                from datetime import datetime as dt, timedelta
                d_start = dt.strptime(train_start, "%Y-%m-%d")
                d_end = dt.strptime(valid_end, "%Y-%m-%d")
                split_point = d_start + (d_end - d_start) * 0.8
                train_end = split_point.strftime("%Y-%m-%d")
                # qlib segment bounds are inclusive: starting validation on the split day
                # itself would put that day in both train and valid.
                valid_start = (split_point + timedelta(days=1)).strftime("%Y-%m-%d")
            result = service.train_model(
                params.get("model_type"),
                instruments,
                train_start,
                train_end,
                valid_start,
                valid_end,
                params.get("handler_type", "Alpha158"),
                model_config,
                params.get("experiment_name")
            )

        elif command == "predict":
            params = json.loads(sys.argv[2])
            result = service.predict(
                params.get("model_id"),
                params.get("instruments"),
                params.get("start_date"),
                params.get("end_date")
            )

        elif command == "run_backtest":
            params = json.loads(sys.argv[2])
            result = service.run_backtest(
                params.get("model_id"),
                params.get("strategy_type", "topk_dropout"),
                params.get("benchmark") or None,
                params.get("topk", 50),
                params.get("n_drop"),
                params.get("start_date"),
                params.get("end_date"),
                params.get("account", 100000000),
                params.get("exchange_config")
            )

        elif command == "get_factor_analysis":
            params = json.loads(sys.argv[2])
            result = service.get_factor_analysis(
                params.get("model_id"),
                params.get("analysis_type", "ic"),
                params.get("instruments"),
                params.get("start_date"),
                params.get("end_date")
            )

        elif command == "get_feature_importance":
            # The C++ side sends {"model_id": "..."}; the original CLI took the bare id.
            # Treating the whole JSON text as the id made the lookup always fail.
            raw_arg = sys.argv[2].strip()
            model_id = raw_arg
            if raw_arg.startswith("{"):
                model_id = json.loads(raw_arg).get("model_id")
            result = service.get_feature_importance(model_id)

        elif command == "save_model":
            params = json.loads(sys.argv[2])
            result = service.save_model(
                params.get("model_id"),
                params.get("path")
            )

        elif command == "load_model":
            params = json.loads(sys.argv[2])
            result = service.load_model(
                params.get("model_type"),
                params.get("path"),
                params.get("model_id")
            )

        elif command == "get_calendar":
            params = json.loads(sys.argv[2])
            result = service.get_calendar(
                params.get("start_date"),
                params.get("end_date"),
                params.get("freq", "day")
            )

        elif command == "get_instruments":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = service.get_instruments(
                params.get("market", "all"),
                params.get("start_date"),
                params.get("end_date")
            )

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        print(json.dumps(_json_safe(result)))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
