"""
AI Quant Lab - Advanced Models Module
LSTM, GRU and Transformer sequence models (PyTorch) trained on a ticker's daily bars.

train_model builds 16 scale-free features on the ticker's daily bars
(qlib_feature_engineering.build_supervised_dataset), standardises them with
training-window statistics, feeds windows of `seq_length` bars to the network and
predicts the forward `horizon`-day return. The split is chronological (~80/20, the
last `horizon` training rows purged); the hold-out is scored against the naive
"training-window mean return" predictor. There is no synthetic-data path.

CLI (one JSON payload on stdout):
  train_model   {"model_type": "LSTM"|"GRU"|"Transformer", "ticker": "AAPL", "start_date"?, "end_date"?,
                 "horizon"?: 5, "seq_length"?: 20,
                 "model_config": {"hidden_size", "num_layers", "dropout", "epochs", "batch_size"?,
                                  "learning_rate"?}}
  list_available | list_models | info <model_id> | create {"model_type", "model_id"?, "config"?}
  train / predict: refused -- weights are not kept between runs; train_model trains and scores
                   in one process.
"""

import os

# Cap native thread pools before numpy / torch load (an LSTM run used to spread over
# every core of a 48-core host and crawl under contention).
_MAX_THREADS = min(4, os.cpu_count() or 1)
for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "NUMEXPR_NUM_THREADS",
           "VECLIB_MAXIMUM_THREADS"):
    os.environ.setdefault(_v, str(_MAX_THREADS))

import importlib.util
import json
import pickle
import sys
import warnings
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, Optional

warnings.filterwarnings('ignore')

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (_HERE, os.path.join(_HERE, '..')):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import fincept_json  # noqa: E402

TORCH_AVAILABLE = False
try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    torch.set_num_threads(_MAX_THREADS)
    TORCH_AVAILABLE = True
except ImportError:
    pass


def _has(module: str) -> bool:
    try:
        return importlib.util.find_spec(module) is not None
    except (ImportError, ValueError):
        return False


# Reported only (nothing here uses qlib); probing avoids a multi-second import.
QLIB_AVAILABLE = _has('qlib')

MODEL_TYPES = ('lstm', 'gru', 'transformer')
_MAX_SAVED_MODELS = 50


if TORCH_AVAILABLE:
    class SimpleLSTM(nn.Module):
        def __init__(self, input_size=16, hidden_size=64, num_layers=2, output_size=1, dropout=0.2):
            super().__init__()
            self.hidden_size = hidden_size
            self.num_layers = num_layers
            self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                                batch_first=True, dropout=dropout if num_layers > 1 else 0)
            self.fc = nn.Linear(hidden_size, output_size)

        def forward(self, x):
            out, _ = self.lstm(x)
            return self.fc(out[:, -1, :])

    class SimpleGRU(nn.Module):
        def __init__(self, input_size=16, hidden_size=64, num_layers=2, output_size=1, dropout=0.2):
            super().__init__()
            self.hidden_size = hidden_size
            self.num_layers = num_layers
            self.gru = nn.GRU(input_size, hidden_size, num_layers,
                              batch_first=True, dropout=dropout if num_layers > 1 else 0)
            self.fc = nn.Linear(hidden_size, output_size)

        def forward(self, x):
            out, _ = self.gru(x)
            return self.fc(out[:, -1, :])

    class SimpleTransformer(nn.Module):
        def __init__(self, input_size=16, d_model=64, nhead=4, num_layers=2, output_size=1, dropout=0.2,
                     max_len=512):
            super().__init__()
            self.d_model = d_model
            self.input_proj = nn.Linear(input_size, d_model)
            # Learned positional embedding: without one, self-attention cannot tell bar 1 from bar 20.
            self.pos = nn.Parameter(torch.zeros(1, max_len, d_model))
            encoder_layer = nn.TransformerEncoderLayer(d_model, nhead, dim_feedforward=d_model * 4,
                                                       dropout=dropout, batch_first=True)
            self.transformer = nn.TransformerEncoder(encoder_layer, num_layers)
            self.fc = nn.Linear(d_model, output_size)

        def forward(self, x):
            x = self.input_proj(x) + self.pos[:, :x.size(1), :]
            x = self.transformer(x)
            return self.fc(x[:, -1, :])


def _regression_metrics(y_true: np.ndarray, y_pred: np.ndarray) -> Dict[str, Optional[float]]:
    err = y_pred - y_true
    mse = float(np.mean(err ** 2))
    sst = float(np.sum((y_true - y_true.mean()) ** 2))
    nz = y_true != 0
    return {
        'mse': mse,
        'rmse': float(np.sqrt(mse)),
        'r2_score': float(1.0 - np.sum(err ** 2) / sst) if sst > 0 else None,
        'directional_accuracy': float(np.mean(np.sign(y_pred[nz]) == np.sign(y_true[nz]))) if nz.any() else None,
        'ic': (float(np.corrcoef(y_pred, y_true)[0, 1])
               if np.std(y_pred) > 0 and np.std(y_true) > 0 else None),
    }


class AdvancedModelsManager:
    """Sequence-model training on market data; model metadata in
    <data root>/script-data/advanced_models (fincept_paths)."""

    def __init__(self):
        self.models: Dict[str, Dict] = {}
        self.training_history: Dict[str, list] = {}
        # Resolved here, not at import; the ~/.fincept/advanced_models of releases is copied
        # in once, never moved.
        import fincept_paths
        self.CACHE_DIR = fincept_paths.user_data_dir('advanced_models', legacy='advanced_models')
        self._load_state()

    def _load_state(self):
        cache_file = self.CACHE_DIR / 'models_state.pkl'
        if cache_file.exists():
            try:
                with open(cache_file, 'rb') as f:
                    data = pickle.load(f)
                self.models = data.get('models', {})
                self.training_history = data.get('history', {})
            except Exception as e:
                print(f"Warning: Could not load cached models: {e}", file=sys.stderr)

    def _save_state(self):
        """Metadata only (weights are not persisted), the most recent _MAX_SAVED_MODELS models."""
        cache_file = self.CACHE_DIR / 'models_state.pkl'
        try:
            keep = sorted(self.models, key=lambda k: self.models[k].get('created_at', ''))[-_MAX_SAVED_MODELS:]
            meta = {k: {kk: vv for kk, vv in self.models[k].items() if kk != 'model'} for k in keep}
            history = {k: v for k, v in self.training_history.items() if k in meta}
            with open(cache_file, 'wb') as f:
                pickle.dump({'models': meta, 'history': history}, f)
        except Exception as e:
            print(f"Warning: Could not save models: {e}", file=sys.stderr)

    @staticmethod
    def get_available_models() -> Dict[str, Any]:
        models = [
            {'id': 'lstm', 'name': 'LSTM', 'category': 'Time-Series',
             'description': 'Long Short-Term Memory network for sequential data', 'available': TORCH_AVAILABLE},
            {'id': 'gru', 'name': 'GRU', 'category': 'Time-Series',
             'description': 'Gated Recurrent Unit - lighter alternative to LSTM', 'available': TORCH_AVAILABLE},
            {'id': 'transformer', 'name': 'Transformer', 'category': 'Attention',
             'description': 'Self-attention encoder with learned positions', 'available': TORCH_AVAILABLE},
        ]
        return {'success': True, 'models': models, 'count': len([m for m in models if m['available']]),
                'torch_available': TORCH_AVAILABLE, 'qlib_available': QLIB_AVAILABLE}

    @staticmethod
    def _build_network(model_type: str, input_size: int, config: Dict):
        hidden_size = int(config.get('hidden_size', 64))
        num_layers = int(config.get('num_layers', 2))
        dropout = float(config.get('dropout', 0.2))
        if not 1 <= num_layers <= 12:
            raise ValueError('num_layers must be between 1 and 12')
        if not 0.0 <= dropout < 1.0:
            raise ValueError('dropout must be in [0, 1)')
        if hidden_size < 4:
            raise ValueError('hidden_size must be at least 4')
        if model_type == 'lstm':
            return SimpleLSTM(input_size, hidden_size, num_layers, 1, dropout)
        if model_type == 'gru':
            return SimpleGRU(input_size, hidden_size, num_layers, 1, dropout)
        if model_type == 'transformer':
            nhead = int(config.get('nhead', 4))
            if hidden_size % nhead != 0:
                raise ValueError(f'Transformer hidden_size ({hidden_size}) must be a multiple of the '
                                 f'{nhead} attention heads')
            return SimpleTransformer(input_size, hidden_size, nhead, num_layers, 1, dropout)
        raise ValueError(f"Model type '{model_type}' is not implemented. Available: {', '.join(MODEL_TYPES)}")

    def create_model(self, model_type: str, model_id: Optional[str] = None,
                     config: Optional[Dict] = None) -> Dict[str, Any]:
        """Register a model configuration (metadata only -- train_model trains)."""
        if not TORCH_AVAILABLE:
            return {'success': False, 'error': 'PyTorch is not installed'}
        model_type = str(model_type).strip().lower().replace(' ', '_')
        config = dict(config or {})
        try:
            self._build_network(model_type, 16, config)       # validates the configuration
        except ValueError as e:
            return {'success': False, 'error': str(e)}
        model_id = model_id or f"{model_type}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        clean = {k: config[k] for k in ('hidden_size', 'num_layers', 'dropout', 'nhead') if k in config}
        self.models[model_id] = {'type': model_type, 'config': clean, 'created_at': datetime.now().isoformat(),
                                 'trained': False, 'epochs_trained': 0}
        self._save_state()
        return {'success': True, 'model_id': model_id, 'model_type': model_type, 'config': clean,
                'message': f'{model_type.upper()} configuration saved; run train_model to train it'}

    def train_model(self, params: Dict) -> Dict[str, Any]:
        """Create + train + score in one process (weights are not kept between runs)."""
        if not TORCH_AVAILABLE:
            return {'success': False, 'error': 'PyTorch is not installed'}
        from qlib_feature_engineering import build_supervised_dataset

        model_type = str(params.get('model_type', 'lstm')).strip().lower().replace(' ', '_')
        if model_type not in MODEL_TYPES:
            return {'success': False,
                    'error': f"Model type '{model_type}' is not implemented. Available: LSTM, GRU, Transformer"}
        config = dict(params.get('config') or params.get('model_config') or {})
        epochs = int(config.pop('epochs', params.get('epochs', 10)))
        batch_size = int(config.pop('batch_size', params.get('batch_size', 32)))
        learning_rate = float(config.pop('learning_rate', params.get('learning_rate', 0.001)))
        seq_length = int(params.get('seq_length', 20))
        if not 1 <= epochs <= 1000:
            raise ValueError('epochs must be between 1 and 1000')
        if not 8 <= batch_size <= 1024:
            raise ValueError('batch_size must be between 8 and 1024')
        if not 5 <= seq_length <= 120:
            raise ValueError('seq_length must be between 5 and 120 bars')
        ticker = str(params.get('ticker') or '').strip()
        if not ticker:
            raise ValueError('ticker is required: the model is trained on that ticker\'s daily bars '
                             '(e.g. {"ticker": "AAPL"})')

        ds = build_supervised_dataset(ticker, params.get('start_date') or None, params.get('end_date') or None,
                                      params.get('horizon', 5), 'regression')
        info = ds['info']
        frame, split, horizon = ds['frame'], ds['split_index'], info['horizon']
        names = ds['feature_names']
        feats = frame[names].to_numpy(dtype=float)
        target = frame['_target'].to_numpy(dtype=float)
        train_end = split - horizon                       # purged boundary (exclusive)

        # Training-window statistics only.
        mu = feats[:train_end].mean(axis=0)
        sd = feats[:train_end].std(axis=0)
        sd[sd == 0] = 1.0
        z = ((feats - mu) / sd).astype(np.float32)
        y_mu = float(target[:train_end].mean())
        y_sd = float(target[:train_end].std()) or 1.0

        def windows(rows):
            X = np.stack([z[r - seq_length + 1:r + 1] for r in rows]).astype(np.float32)
            return X, target[rows]

        train_rows = list(range(seq_length - 1, train_end))
        test_rows = list(range(max(split, seq_length - 1), len(frame)))
        if len(train_rows) < 100 or len(test_rows) < 20:
            raise ValueError(f'Too few sequences ({len(train_rows)} train / {len(test_rows)} test) for '
                             f'seq_length {seq_length} -- widen the date range')
        X_train, y_train = windows(train_rows)
        X_test, y_test = windows(test_rows)

        torch.manual_seed(42)
        np.random.seed(42)
        model = self._build_network(model_type, len(names), config)
        criterion = nn.MSELoss()
        optimizer = optim.Adam(model.parameters(), lr=learning_rate)
        X_t = torch.from_numpy(X_train)
        y_t = torch.from_numpy(((y_train - y_mu) / y_sd).astype(np.float32)).reshape(-1, 1)

        losses = []
        model.train()
        for _ in range(epochs):
            order = torch.randperm(len(X_t))       # shuffles samples, not time: each is a self-contained window
            epoch_losses = []
            for i in range(0, len(order), batch_size):
                idx = order[i:i + batch_size]
                optimizer.zero_grad()
                loss = criterion(model(X_t[idx]), y_t[idx])
                loss.backward()
                optimizer.step()
                epoch_losses.append(float(loss.item()))
            losses.append(float(np.mean(epoch_losses)))

        model.eval()
        with torch.no_grad():
            pred = model(torch.from_numpy(X_test)).numpy().reshape(-1) * y_sd + y_mu
        holdout = _regression_metrics(y_test, pred)
        base_pred = np.full_like(y_test, y_mu)
        baseline = _regression_metrics(y_test, base_pred)
        nz = y_test != 0
        baseline = {
            'name': 'training-window mean return',
            'mean_return': y_mu,
            'mse': baseline['mse'],
            'rmse': baseline['rmse'],
            'r2_score': baseline['r2_score'],
            'directional_accuracy': (float(np.mean(np.sign(y_test[nz]) == np.sign(y_mu)))
                                     if nz.any() and y_mu != 0 else None),
        }

        model_id = str(params.get('model_id') or f"{model_type}_{datetime.now().strftime('%Y%m%d_%H%M%S')}")
        clean = {'hidden_size': int(config.get('hidden_size', 64)), 'num_layers': int(config.get('num_layers', 2)),
                 'dropout': float(config.get('dropout', 0.2)), 'input_size': len(names)}
        dates = [str(d.date()) for d in frame.index[test_rows]]
        self.models[model_id] = {'type': model_type, 'config': clean, 'created_at': datetime.now().isoformat(),
                                 'trained': True, 'epochs_trained': epochs, 'last_loss': losses[-1],
                                 'ticker': info['ticker'], 'holdout_metrics': holdout}
        self.training_history.setdefault(model_id, []).append(
            {'timestamp': datetime.now().isoformat(), 'epochs': epochs, 'final_loss': losses[-1]})
        self._save_state()

        data = {k: info[k] for k in ('ticker', 'start_date', 'end_date', 'horizon', 'target', 'bars',
                                     'train_period', 'test_period', 'source')}
        data.update({'seq_length': seq_length, 'train_sequences': len(train_rows), 'test_sequences': len(test_rows),
                     'purged_rows': info['purged_rows'], 'features': names})
        mse_ratio = holdout['mse'] / baseline['mse'] if baseline['mse'] else None
        return {
            'success': True,
            'model_id': model_id,
            'model_type': model_type,
            'config': clean,
            'epochs_trained': epochs,
            'total_epochs': epochs,
            'batch_size': batch_size,
            'learning_rate': learning_rate,
            'final_loss': losses[-1],
            'loss_history': losses,
            'loss_units': 'MSE of the standardised target (train window)',
            'holdout_metrics': holdout,
            'baseline': baseline,
            'mse_vs_baseline': mse_ratio,
            'beats_baseline': bool(mse_ratio is not None and mse_ratio < 1.0),
            'holdout': {'dates': dates, 'actual': y_test.tolist(), 'predicted': pred.tolist()},
            'data': data,
            'message': f'{model_type.upper()} trained {epochs} epoch(s) on {info["ticker"]}; '
                       f'hold-out MSE {"below" if mse_ratio is not None and mse_ratio < 1 else "not below"} '
                       f'the mean-return baseline',
        }

    def get_model_info(self, model_id: str) -> Dict[str, Any]:
        if model_id not in self.models:
            return {'success': False, 'error': f'Model {model_id} not found'}
        info = self.models[model_id]
        return {'success': True, 'model_id': model_id, 'model_type': info['type'], 'config': info['config'],
                'created_at': info['created_at'], 'trained': info.get('trained', False),
                'epochs_trained': info.get('epochs_trained', 0), 'last_loss': info.get('last_loss'),
                'ticker': info.get('ticker'), 'holdout_metrics': info.get('holdout_metrics'),
                'weights_saved': False}

    def list_models(self) -> Dict[str, Any]:
        rows = [{'model_id': k, 'type': v['type'], 'created_at': v['created_at'], 'trained': v.get('trained', False),
                 'epochs_trained': v.get('epochs_trained', 0), 'ticker': v.get('ticker')}
                for k, v in self.models.items()]
        return {'success': True, 'models': rows, 'count': len(rows)}


def main():
    if len(sys.argv) < 2:
        fincept_json.emit({'success': False, 'error': 'Usage: python qlib_advanced_models.py <command> [args...]'})
        return
    command = sys.argv[1]
    try:
        manager = AdvancedModelsManager()

        def json_arg(i):
            if len(sys.argv) <= i:
                return {}
            value = json.loads(sys.argv[i])
            if not isinstance(value, dict):
                raise ValueError('parameters must be a JSON object')
            return value

        if command == 'list_available':
            result = manager.get_available_models()
        elif command in ('create', 'create_model'):
            p = json_arg(2)
            result = manager.create_model(p.get('model_type', 'lstm'), p.get('model_id'),
                                          p.get('config') or p.get('model_config') or {})
        elif command == 'train_model':
            result = manager.train_model(json_arg(2))
        elif command in ('train', 'predict'):
            result = {'success': False,
                      'error': f"'{command}' is not available: network weights are not kept between runs. "
                               "Use train_model with a ticker -- it trains and scores the model on the "
                               "hold-out window in one run."}
        elif command == 'info':
            result = manager.get_model_info(sys.argv[2] if len(sys.argv) > 2 else '')
        elif command == 'list_models':
            result = manager.list_models()
        else:
            result = {'success': False, 'error': f'Unknown command: {command}'}
    except (ValueError, json.JSONDecodeError) as e:
        result = {'success': False, 'error': str(e)}
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)
        result = {'success': False, 'error': f'{command} failed: {e}'}

    fincept_json.emit(result)


if __name__ == '__main__':
    main()
