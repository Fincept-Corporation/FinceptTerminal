"""
AI Quant Lab - Qlib Service
Microsoft Qlib integration for quantitative research, model training, backtesting and analysis
Full-featured implementation with all Qlib capabilities
"""

import importlib
import importlib.util
import json
import re
import sys
import os
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple, Union
import warnings
warnings.filterwarnings('ignore')

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import fincept_json

# Qlib's own model training (LightGBM / XGBoost / CatBoost / torch) would otherwise
# start one thread per core -- 48 on a server -- for every run.
_MAX_THREADS = min(4, os.cpu_count() or 1)
for _v in ("OMP_NUM_THREADS", "MKL_NUM_THREADS", "OPENBLAS_NUM_THREADS", "NUMEXPR_NUM_THREADS",
           "VECLIB_MAXIMUM_THREADS"):
    os.environ.setdefault(_v, str(_MAX_THREADS))

# Recent MLflow releases refuse to open the file-based tracking store that Qlib's
# recorder uses unless this is set ("The filesystem tracking backend ... is in
# maintenance mode"), which made every model.fit() fail with that message.
os.environ.setdefault("MLFLOW_ALLOW_FILE_STORE", "true")

# Sibling modules (qlib_datasets) import however the script is started.
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(1, _HERE)
import qlib_datasets as qds


# ── Where trained models and experiment runs are kept ────────────────────────
# <data root>/script-data/<name> (fincept_paths), resolved on first use rather than at
# import; the ~/.fincept/<name> folder of releases is copied in once, never moved.

def _mlruns_dir() -> Path:
    """MLflow tracking folder (qlib's recorder), shared with qlib_rolling_retraining."""
    import fincept_paths
    return fincept_paths.user_data_dir("mlruns", legacy="mlruns")


class _ScriptDataDir:
    """A class attribute that resolves to fincept_paths.user_data_dir(name) on first access."""

    def __init__(self, name: str):
        self.name = name
        self.path: Optional[Path] = None

    def __get__(self, obj, owner=None) -> Path:
        if self.path is None:
            import fincept_paths
            self.path = fincept_paths.user_data_dir(self.name, legacy=self.name)
        return self.path


# ── Lazy qlib ────────────────────────────────────────────────────────────────
# This module used to import qlib, every model class (torch), qlib.workflow (mlflow) and
# qlib.contrib.report (plotly -> jsonschema, which compiles a URI grammar for 5 s) at
# start-up: ~30 s before ANY command ran, check_status and the panels' dataset probe
# included. qlib's core is now imported by the commands that read market data, and a
# model class only when that model is trained or loaded.
QLIB_AVAILABLE: Optional[bool] = None   # None: not tried yet (see _ensure_qlib)
QLIB_ERROR: Optional[str] = None
qlib = D = REG_US = REG_CN = init_instance_by_config = None


def _ensure_qlib() -> bool:
    global QLIB_AVAILABLE, QLIB_ERROR, qlib, D, REG_US, REG_CN, init_instance_by_config
    if QLIB_AVAILABLE is None:
        try:
            import qlib as _qlib
            from qlib.config import REG_CN as _cn, REG_US as _us
            from qlib.data import D as _d
            from qlib.utils import init_instance_by_config as _iibc
            qlib, REG_CN, REG_US, D, init_instance_by_config = _qlib, _cn, _us, _d, _iibc
            QLIB_AVAILABLE = True
        except ImportError as e:
            QLIB_AVAILABLE, QLIB_ERROR = False, str(e)
    return bool(QLIB_AVAILABLE)


def _find_spec(name: str) -> bool:
    try:
        return importlib.util.find_spec(name) is not None
    except (ImportError, ValueError):
        return False


def _qlib_module_present(module: str) -> bool:
    """Whether qlib ships `module`, judged from its files: find_spec() on a submodule runs
    the parent package, and qlib/contrib/model/__init__.py imports every model and torch."""
    try:
        spec = importlib.util.find_spec("qlib")
    except (ImportError, ValueError):
        return False
    if spec is None or not spec.submodule_search_locations:
        return False
    base = Path(list(spec.submodule_search_locations)[0]).joinpath(*module.split(".")[1:])
    return base.with_suffix(".py").exists() or (base / "__init__.py").exists()


def _qlib_version() -> Optional[str]:
    try:
        from importlib.metadata import version
        return version("pyqlib")
    except Exception:
        return getattr(qlib, "__version__", None) if _ensure_qlib() else None


# model id -> (module, class, packages it needs besides qlib)
_MODEL_SPECS = {
    'lightgbm': ('qlib.contrib.model.gbdt', 'LGBModel', ('lightgbm',)),
    'xgboost': ('qlib.contrib.model.xgboost', 'XGBModel', ('xgboost',)),
    'catboost': ('qlib.contrib.model.catboost_model', 'CatBoostModel', ('catboost',)),
    'linear': ('qlib.contrib.model.linear', 'LinearModel', ('sklearn', 'scipy')),
    'lstm': ('qlib.contrib.model.pytorch_lstm', 'LSTM', ('torch',)),
    'gru': ('qlib.contrib.model.pytorch_gru', 'GRU', ('torch',)),
    'alstm': ('qlib.contrib.model.pytorch_alstm', 'ALSTM', ('torch',)),
    'transformer': ('qlib.contrib.model.pytorch_transformer', 'Transformer', ('torch',)),
    'tcn': ('qlib.contrib.model.pytorch_tcn', 'TCN', ('torch',)),
    'adarnn': ('qlib.contrib.model.pytorch_adarnn', 'ADARNN', ('torch',)),
    'hist': ('qlib.contrib.model.pytorch_hist', 'HIST', ('torch',)),
    'tabnet': ('qlib.contrib.model.pytorch_tabnet', 'TabnetModel', ('torch',)),
    'sfm': ('qlib.contrib.model.pytorch_sfm', 'SFM_Model', ('torch',)),
    'gats': ('qlib.contrib.model.pytorch_gats', 'GATs', ('torch',)),
    'add': ('qlib.contrib.model.pytorch_add', 'ADD', ('torch',)),
    'densemble': ('qlib.contrib.model.double_ensemble', 'DEnsembleModel', ('lightgbm',)),
}
MODEL_CLASSES: Dict[str, Any] = {}   # filled on demand by _model_class()


def _models_available() -> Dict[str, bool]:
    """Installed or not, for every model, without importing any of them (see above). An
    import that still fails is reported by the run that needs the model."""
    return {k: _qlib_module_present(mod) and all(_find_spec(p) for p in deps)
            for k, (mod, _cls, deps) in _MODEL_SPECS.items()}


def _model_class(model_type: Optional[str]):
    name = (model_type or "").lower()
    if name in MODEL_CLASSES:
        return MODEL_CLASSES[name]
    if name not in _MODEL_SPECS or not _ensure_qlib():
        return None
    try:
        if name == "sfm":
            cls = _sfm_wrapper_class()
        else:
            mod, cls_name, _deps = _MODEL_SPECS[name]
            cls = getattr(importlib.import_module(mod), cls_name)
    except Exception as e:
        print(f"Model '{name}' is not importable: {e}", file=sys.stderr)
        return None
    MODEL_CLASSES[name] = cls
    return cls


def _sfm_wrapper_class():
    """SFM_Model is a raw nn.Module: wrap it with a qlib-compatible fit/predict interface.
    Built on first use (it needs torch) and published as the module-level name SFMWrapper,
    which is where pickles of trained SFM models look for it."""
    import copy
    import numpy as np
    import pandas as pd
    import torch
    import torch.nn as nn
    from qlib.contrib.model.pytorch_sfm import SFM_Model
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
    SFMWrapper.__module__ = __name__
    SFMWrapper.__qualname__ = "SFMWrapper"
    globals()["SFMWrapper"] = SFMWrapper
    return SFMWrapper


HANDLER_NAMES = ("Alpha158", "Alpha360", "Alpha158vwap", "Alpha360vwap")
STRATEGY_NAMES = ("topk_dropout", "weight_base", "enhanced_indexing", "base_signal")


def _handlers_available() -> List[str]:
    return list(HANDLER_NAMES) if _qlib_module_present("qlib.contrib.data.handler") else []


def _strategies_available() -> List[str]:
    return list(STRATEGY_NAMES) if _qlib_module_present("qlib.contrib.strategy.signal_strategy") else []


# The dataset of a model trained before datasets were recorded: the public US one, the
# only dataset there was.
_LEGACY_DATASET_ID = "qlib_us"

# Model types whose qlib class has get_feature_importance() (FeatureInt / LightGBMFInt).
_TREE_MODELS = {"lightgbm", "xgboost", "catboost", "densemble"}

# Cross-sectional statistics (factor IC, a model's daily IC) rank instruments against each
# other on each day; below this many names a day's correlation is noise.
MIN_CROSS_SECTION = 5


def _pct(v):
    return v * 100.0 if isinstance(v, (int, float)) and not isinstance(v, bool) else None


def _daily_ic(pred, label, min_names: int = 2) -> Optional[Dict[str, Any]]:
    """Mean daily IC / rank IC of scores against labels (both indexed (datetime, instrument)),
    over the days with at least `min_names` instruments; None when no day qualifies."""
    import pandas as pd
    frame = pd.concat([pred.rename("score"), label.rename("label")], axis=1).dropna()
    if frame.empty:
        return None
    by_day = frame.groupby(level="datetime")
    sizes = by_day.size()
    keep = sizes[sizes >= min_names].index
    frame = frame[frame.index.get_level_values("datetime").isin(keep)]
    if frame.empty:
        return None
    by_day = frame.groupby(level="datetime")
    ic = by_day.apply(lambda g: g["score"].corr(g["label"])).dropna()
    ric = by_day.apply(lambda g: g["score"].corr(g["label"], method="spearman")).dropna()
    if len(ic) == 0:
        return None

    def ratio(s):
        sd = float(s.std()) if len(s) > 1 else 0.0
        return float(s.mean() / sd) if sd > 0 else None

    return {"ic": float(ic.mean()), "rank_ic": float(ric.mean()) if len(ric) else None,
            "icir": ratio(ic), "rank_icir": ratio(ric) if len(ric) else None, "days": int(len(ic))}


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

    def __init__(self, dataset: Optional[Dict[str, Any]] = None, init: bool = True):
        self.trained_models = {}
        self.experiment_manager = None
        self.initialized = False
        self.init_error = None
        # The local qlib dataset this command reads (qlib_datasets.resolve()); None when
        # there is none, which every data command reports as such.
        self.dataset = dataset
        # Notes about the window or the dataset (clamped dates, a model applied to a dataset
        # it was not trained on): main() adds them to the result's "warning".
        self.notes: List[str] = []
        if dataset is None or not init:
            return
        if not _ensure_qlib():
            self.init_error = f"qlib is not installed: {QLIB_ERROR}"
            return
        try:
            reg = REG_US if str(dataset.get("region", "us")).lower() == "us" else REG_CN
            # Keep experiment tracking in the edition's data root (copied once from the
            # ~/.fincept/mlruns of releases) instead of creating a ./mlruns folder in
            # whatever directory the process happens to start in.
            mlruns = _mlruns_dir()
            # kernels: qlib loads data with that many joblib worker PROCESSES;
            # its default is cpu_count() - 2 (46 on a 48-core server).
            qlib.init(provider_uri=dataset["provider_uri"], region=reg, kernels=_MAX_THREADS,
                      exp_manager={"class": "MLflowExpManager",
                                   "module_path": "qlib.workflow.expm",
                                   "kwargs": {"uri": mlruns.as_uri(),
                                              "default_exp_name": "Experiment"}})
            self.initialized = True
        except Exception as e:
            self.init_error = str(e)
            self.initialized = False

    def _window(self, start: Optional[str], end: Optional[str]):
        """Clamp [start, end] to the dataset calendar: (start, end, None), or (None, None,
        error result) when the window lies entirely outside it -- the error names the range
        the dataset covers."""
        s, e, note, err = qds.check_window(self.dataset, start, end)
        if err:
            return None, None, {"success": False, "error": err, "dataset": qds.brief(self.dataset)}
        if note:
            self.notes.append(note)
        return s, e, None

    # ── Model persistence ───────────────────────────────────────────────────
    # Every CLI call is a fresh process, so ``self.trained_models`` is empty again by
    # the time the user clicks Backtest / Feature Importance with the ID that TRAIN
    # printed ("Model <id> not found"). Trained models are therefore pickled to disk
    # and re-loaded on demand. The JSON next to the pickle records the dataset the model
    # was trained on (dataset_id), so it is not applied to another one unnoticed.
    MODELS_DIR = _ScriptDataDir("qlib_models")
    _MODEL_ID_RE = re.compile(r"^[A-Za-z0-9_.-]{1,80}$")

    @classmethod
    def model_meta(cls, model_id: Optional[str]) -> Optional[Dict[str, Any]]:
        """The saved JSON of a model (no qlib needed), or None."""
        if not model_id or not isinstance(model_id, str) or not cls._MODEL_ID_RE.match(model_id):
            return None
        try:
            return json.loads((cls.MODELS_DIR / f"{model_id}.json").read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None

    @classmethod
    def _write_meta(cls, model_id: str, meta: Dict[str, Any]) -> None:
        """Atomic replace of <id>.json (a half-written file would hide the model from every list)."""
        target = cls.MODELS_DIR / f"{model_id}.json"
        tmp = target.with_name(target.name + ".tmp")
        tmp.write_text(json.dumps(_json_safe(meta), default=str, indent=2), encoding="utf-8")
        os.replace(tmp, target)

    def _persist_model(self, model_id: str, info: Dict[str, Any]) -> None:
        """Best-effort: a read-only home directory must not fail a successful training run."""
        try:
            self.MODELS_DIR.mkdir(parents=True, exist_ok=True)
            info["model"].to_pickle(str(self.MODELS_DIR / f"{model_id}.pkl"))
            meta = {k: v for k, v in info.items() if k != "model"}
            self._write_meta(model_id, meta)
        except Exception as e:
            print(f"Warning: could not persist model {model_id}: {e}", file=sys.stderr)

    @classmethod
    def _update_model_meta(cls, model_id: str, updates: Dict[str, Any]) -> None:
        """Merge `updates` into a saved model's JSON (best-effort; unknown / unsaved ids are skipped)."""
        meta = cls.model_meta(model_id)
        if meta is None:
            return
        try:
            meta.update(updates)
            cls._write_meta(model_id, meta)
        except Exception as e:
            print(f"Warning: could not update the record of model {model_id}: {e}", file=sys.stderr)

    @staticmethod
    def _trained_at(model_id: str, meta: Dict[str, Any], pkl: Path) -> Optional[str]:
        """When a model was trained: its record (models trained from now on), else the
        <type>_YYYYmmdd_HHMMSS stamp in its id, else the pickle's modification time."""
        stamp = meta.get("trained_at")
        if isinstance(stamp, str) and stamp:
            return stamp
        m = re.search(r"_(\d{8}_\d{6})$", model_id)
        if m:
            try:
                return datetime.strptime(m.group(1), "%Y%m%d_%H%M%S").isoformat(timespec="seconds")
            except ValueError:
                pass
        try:
            return datetime.fromtimestamp(pkl.stat().st_mtime).isoformat(timespec="seconds")
        except OSError:
            return None

    @staticmethod
    def _metrics_summary(validation: Optional[Dict[str, Any]], backtest: Optional[Dict[str, Any]]) -> str:
        """One line for a model picker's tooltip: validation IC and the last backtest."""
        def num(v, fmt):
            return format(v, fmt) if isinstance(v, (int, float)) and not isinstance(v, bool) else "n/a"
        parts = []
        if validation:
            parts.append(f"validation IC {num(validation.get('ic'), '.3f')}, rank IC "
                         f"{num(validation.get('rank_ic'), '.3f')} over {validation.get('days', 0)} days")
        if backtest:
            period = backtest.get("period") or {}
            parts.append(f"backtest {period.get('start', '?')} to {period.get('end', '?')}: annualised "
                         f"{num(_pct(backtest.get('annualized_return')), '.1f')}%, Sharpe "
                         f"{num(backtest.get('sharpe_ratio'), '.2f')}, max drawdown "
                         f"{num(_pct(backtest.get('max_drawdown')), '.1f')}%")
        return "; ".join(parts) if parts else "No metrics recorded yet (run a backtest to add some)."

    @classmethod
    def list_trained_models(cls) -> Dict[str, Any]:
        """Every model TRAIN saved under MODELS_DIR (a <id>.pkl with its <id>.json record), newest
        first, with what a picker needs: id, type, dataset, when it was trained and a metrics
        summary (validation IC recorded at training, the last backtest's figures). Reads files
        only: no qlib import, no dataset."""
        datasets = {i["id"]: i for i in qds.list_datasets()}
        rows = []
        if cls.MODELS_DIR.is_dir():
            for meta_path in cls.MODELS_DIR.glob("*.json"):
                model_id = meta_path.stem
                pkl = cls.MODELS_DIR / f"{model_id}.pkl"
                if not cls._MODEL_ID_RE.match(model_id) or not pkl.is_file():
                    continue
                meta = cls.model_meta(model_id)
                if not isinstance(meta, dict):
                    continue
                ds_id = meta.get("dataset_id") or _LEGACY_DATASET_ID
                validation = meta.get("metrics") if isinstance(meta.get("metrics"), dict) else None
                backtest = meta.get("last_backtest") if isinstance(meta.get("last_backtest"), dict) else None
                model_type = str(meta.get("type") or model_id.split("_")[0])
                instruments = meta.get("instruments")
                rows.append({
                    "id": model_id,
                    "model_type": model_type,
                    "handler": meta.get("handler"),
                    "dataset_id": ds_id,
                    "dataset_name": meta.get("dataset_name") or (datasets.get(ds_id) or {}).get("name"),
                    "dataset_available": ds_id in datasets,
                    "instruments": instruments if isinstance(instruments, list) else ([instruments] if instruments else []),
                    "train_period": meta.get("train_period"),
                    "valid_period": meta.get("valid_period"),
                    "trained_at": cls._trained_at(model_id, meta, pkl),
                    "supports_feature_importance": model_type.lower() in _TREE_MODELS,
                    "metrics": {"validation": validation, "last_backtest": backtest},
                    "metrics_summary": cls._metrics_summary(validation, backtest),
                })
        rows.sort(key=lambda r: r["id"])
        rows.sort(key=lambda r: r["trained_at"] or "", reverse=True)
        return {"success": True, "models": rows, "count": len(rows), "models_dir": str(cls.MODELS_DIR)}

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
            if not _ensure_qlib():
                return None
            from qlib.utils.serial import Serializable
            info = json.loads(meta.read_text(encoding="utf-8"))
            # The pickle names its class; SFMWrapper only exists once it has been built.
            _model_class(info.get("type"))
            info["model"] = Serializable.load(str(pkl))
            self.trained_models[model_id] = info
            return info
        except Exception as e:
            print(f"Warning: could not load model {model_id}: {e}", file=sys.stderr)
            return None

    def _data_error(self) -> Optional[Dict[str, Any]]:
        """Friendly failure when qlib has no market data, instead of qlib's own
        "can't find a freq from [] that can resample to day!"."""
        if self.dataset is None:
            return {"success": False, "error": qds.no_dataset_message()}
        if not self.initialized:
            return {"success": False,
                    "error": f"Qlib could not open the {self.dataset['id']} dataset ({self.dataset['path']}): "
                             f"{self.init_error}"}
        try:
            empty = len(D.calendar(freq="day")) == 0
        except Exception:
            empty = True
        if empty:
            return {"success": False, "error": f"The {self.dataset['id']} dataset has an empty calendar. "
                                               + qds.no_dataset_message()}
        return None

    def _pool_names(self) -> set:
        pools = {"sp500", "nasdaq100", "all", "csi300", "csi500", "csi100"}
        if self.dataset:
            pools |= {str(m).lower() for m in self.dataset.get("markets", [])}
        return pools

    def _instrument_arg(self, instruments):
        """qlib wants a market-pool name as a string and tickers as a lowercase list. A
        one-element list naming a pool of the dataset (["sp500"], ["us_large"]) is the pool."""
        if isinstance(instruments, str):
            return instruments.lower()
        instruments = [str(i).lower() for i in (instruments or [])]
        if len(instruments) == 1 and instruments[0] in self._pool_names():
            return instruments[0]
        return instruments

    def _pool_members(self, pool: str, start: str, end: str) -> List[str]:
        """The instruments of a market pool that have bars in [start, end]."""
        return D.list_instruments(D.instruments(pool), start_time=start, end_time=end, as_list=True)

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
        avail = _models_available()
        models = [
            {
                "id": "lightgbm",
                "name": "LightGBM",
                "description": "Fast gradient boosting model - Industry standard baseline with excellent performance",
                "type": "tree_based",
                "available": avail.get('lightgbm', False),
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
                "available": avail.get('xgboost', False),
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
                "available": avail.get('catboost', False),
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
                "available": avail.get('linear', False),
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
                "available": avail.get('lstm', False),
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
                "available": avail.get('gru', False),
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
                "available": avail.get('alstm', False),
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
                "available": avail.get('transformer', False),
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
                "available": avail.get('tcn', False),
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
                "available": avail.get('adarnn', False),
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
                "available": avail.get('hist', False),
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
                "available": avail.get('tabnet', False),
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
                "available": avail.get('sfm', False),
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
                "available": avail.get('gats', False),
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
                "available": avail.get('add', False),
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
                "available": avail.get('densemble', False),
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
        handlers_avail = _handlers_available()
        handlers = {
            "Alpha158": {
                "description": "158 technical factors including momentum, volatility, volume indicators",
                "factor_count": 158,
                "category": "technical",
                "available": 'Alpha158' in handlers_avail,
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
                "available": 'Alpha360' in handlers_avail,
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
                "available": 'Alpha158vwap' in handlers_avail,
                "factors": ["All Alpha158 factors", "VWAP deviation", "VWAP momentum"]
            },
            "Alpha360vwap": {
                "description": "Alpha360 with VWAP features",
                "factor_count": 360,
                "category": "technical_vwap",
                "available": 'Alpha360vwap' in handlers_avail,
                "factors": ["All Alpha360 factors", "VWAP extended"]
            }
        }

        return {
            "success": True,
            "handlers": handlers,
            "available_count": len(handlers_avail)
        }

    def get_factor_library(self) -> Dict[str, Any]:
        """List built-in Qlib alpha factors with their expressions.

        Pulls the real (name, expression) pairs from the Alpha158/Alpha360
        feature loaders when qlib is installed; falls back to a curated set
        of canonical alpha expressions otherwise so the UI is never empty.
        Returns {"success", "factors":[{name, category, expression}, ...]}.
        """
        factors: List[Dict[str, Any]] = []
        if _ensure_qlib():
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
        strategies_avail = _strategies_available()
        strategies = {
            "topk_dropout": {
                "name": "TopK Dropout Strategy",
                "description": "Select top-K stocks based on signal with dropout mechanism",
                "available": 'topk_dropout' in strategies_avail,
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
                "available": 'enhanced_indexing' in strategies_avail,
                "parameters": {
                    "benchmark": "Benchmark index to track",
                    "tracking_error": "Maximum tracking error allowed"
                },
                "use_cases": ["Index enhancement", "Low tracking error"]
            },
            "weight_base": {
                "name": "Weight-Based Strategy",
                "description": "Flexible weight allocation based on signals",
                "available": 'weight_base' in strategies_avail,
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
            import pandas as pd   # not at module level: the dataset probe must start fast
            if fields is None:
                fields = ["$open", "$high", "$low", "$close", "$volume", "$vwap", "$factor"]

            # Clamp dates to the dataset calendar (noted in the result's warning). A window
            # entirely outside it used to come back as success with no rows.
            cal_warning = None
            start_date, end_date, window_err = self._window(start_date, end_date)
            if window_err:
                return window_err

            # Normalize instruments to lowercase (qlib US data uses lowercase dirs); a market
            # pool of the dataset ("starter", "sp500") stands for its members.
            pool = self._instrument_arg(instruments)
            if isinstance(pool, str):
                instruments = [str(i).lower() for i in self._pool_members(pool, start_date, end_date)]
            else:
                instruments = [i.lower() if isinstance(i, str) else i for i in instruments]

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

            if total_records == 0:
                return {"success": False,
                        "error": f"No data for {', '.join(map(str, instruments))} between {start_date} and "
                                 f"{end_date} in the {self.dataset['id']} dataset (a symbol it does not hold, or "
                                 f"no bars in the window; it has {self.dataset['instruments']} instruments)."}

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
        data_err = self._data_error()
        if data_err:
            return data_err

        if handler_type not in _handlers_available():
            return {"success": False, "error": f"Handler {handler_type} not available"}

        try:
            start_time, end_time, window_err = self._window(start_time, end_time)
            if window_err:
                return window_err
            instruments = self._instrument_arg(instruments)
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

        model_type_lower = (model_type or "").lower()
        if _model_class(model_type_lower) is None:
            avail = [k for k, ok in _models_available().items() if ok]
            return {
                "success": False,
                "error": f"Model type '{model_type}' not available. Available: {avail}"
            }

        # The whole run must lie inside the dataset: the training start / validation end
        # are clamped to its calendar, a window outside it is refused naming the range.
        ds = self.dataset
        if (train_start or "") > ds["calendar_end"] or (valid_end or "9999") < ds["calendar_start"]                 or (train_end or "") < ds["calendar_start"] or (valid_start or "") > ds["calendar_end"]:
            return {"success": False, "error": qds.window_message(ds, train_start, valid_end),
                    "dataset": qds.brief(ds)}
        train_start, valid_end, window_err = self._window(train_start, valid_end)
        if window_err:
            return window_err
        if not (train_start <= train_end < valid_start <= valid_end):
            return {"success": False,
                    "error": f"Inconsistent split: train {train_start} -> {train_end}, valid {valid_start} -> "
                             f"{valid_end}. Training must end before validation starts."}

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

            # HIST requires CSI300 stock2concept mapping (next to the public datasets; qlib's
            # ~/.qlib/qlib_data copy is read in place when only that one exists)
            stock2concept_path = str(qds.qlib_data_file('stock2concept.npy'))
            stock_index_path = str(qds.qlib_data_file('stock_index.npy'))

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
            model_class = _model_class(model_type_lower)
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
            # Explicit thread caps: CatBoost ignores OMP_NUM_THREADS and defaults to every core.
            thread_keys = {'lightgbm': 'num_threads', 'xgboost': 'nthread', 'catboost': 'thread_count'}
            if model_type_lower in thread_keys:
                config.setdefault(thread_keys[model_type_lower], _MAX_THREADS)

            model = model_class(**config)

            # Train the model
            model.fit(dataset)

            # Generate predictions on test set
            predictions = model.predict(dataset)

            # The validation IC, recorded with the model so a model picker can say how good it
            # is (list_trained_models). Best-effort: it never fails a training run.
            validation = None
            try:
                import pandas as pd
                from qlib.data.dataset.handler import DataHandlerLP
                pred_s = predictions.iloc[:, 0] if isinstance(predictions, pd.DataFrame) else predictions
                label = dataset.prepare("test", col_set="label", data_key=DataHandlerLP.DK_L)
                label = label.iloc[:, 0] if isinstance(label, pd.DataFrame) else label
                validation = _daily_ic(pred_s, label)
                if validation is not None:
                    validation["period"] = [valid_start, valid_end]
            except Exception as e:
                print(f"Warning: could not score the validation period: {e}", file=sys.stderr)

            # Store trained model
            trained_at = datetime.now()
            model_id = f"{model_type_lower}_{trained_at.strftime('%Y%m%d_%H%M%S')}"
            self.trained_models[model_id] = {
                "model": model,
                "type": model_type,
                "config": config,
                "handler": handler_type,
                "instruments": instruments,
                "train_period": [train_start, train_end],
                "valid_period": [valid_start, valid_end],
                # The dataset it learnt from: predict / backtest refuse a dataset that lacks
                # its instruments and warn on any other one.
                "dataset_id": self.dataset["id"],
                "dataset_name": self.dataset["name"],
                "dataset_calendar": [self.dataset["calendar_start"], self.dataset["calendar_end"]],
                "trained_at": trained_at.isoformat(timespec="seconds"),
                "metrics": validation,
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
                "predictions_count": pred_count,
                "trained_at": trained_at.isoformat(timespec="seconds"),
                "validation_metrics": validation,
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
        data_err = self._data_error()
        if data_err:
            return data_err

        try:
            import pandas as pd
            # Default universe: the one the model was trained on; default window: the
            # last ~3 months of the local calendar.
            info = dict(model_info)
            if instruments:
                info["instruments"] = instruments
            cal = D.calendar(freq="day")
            if len(cal) == 0:
                return {"success": False, "error": qds.no_dataset_message()}
            if start_date and end_date and start_date > end_date:
                return {"success": False, "error": f"start_date {start_date} is after end_date {end_date}"}
            end_date = end_date or str(cal[-1].date())
            start_date = start_date or str(cal[max(0, len(cal) - 63)].date())
            start_date, end_date, window_err = self._window(start_date, end_date)
            if window_err:
                return window_err

            dataset = self._build_test_dataset(info, start_date, end_date)
            predictions = info["model"].predict(dataset)
            if isinstance(predictions, pd.DataFrame):
                predictions = predictions.iloc[:, 0]
            if predictions is None or len(predictions) == 0:
                return {"success": False,
                        "error": "The model produced no predictions for these instruments and dates "
                                 "(outside the local data, or no bars in the window)"}

            # MultiIndex (datetime, instrument) -> rows; to_dict() gave tuple keys.
            frame = predictions.rename("score").reset_index()
            date_col, inst_col = frame.columns[0], frame.columns[1]
            frame = frame.dropna(subset=["score"]).sort_values([date_col, inst_col])
            if frame.empty:
                return {"success": False, "error": "The model produced only missing scores for this window"}
            rows = [{"date": str(pd.Timestamp(d).date()), "instrument": str(i), "score": float(v)}
                    for d, i, v in zip(frame[date_col], frame[inst_col], frame["score"])]
            last_day = frame[date_col].max()
            latest = frame[frame[date_col] == last_day].sort_values("score", ascending=False)
            ranking = [{"rank": k + 1, "instrument": str(i), "score": float(v)}
                       for k, (i, v) in enumerate(zip(latest[inst_col], latest["score"]))]

            return {
                "success": True,
                "model_id": model_id,
                "model_type": model_info.get("type"),
                "handler": model_info.get("handler"),
                "period": {"start": start_date, "end": end_date},
                "latest_date": str(pd.Timestamp(last_day).date()),
                "latest_ranking": ranking,
                "predictions": rows[-2000:],
                "prediction_count": len(rows),
                "note": ("Scores are the model's prediction of each instrument's next-period return label; "
                         "they rank instruments against each other on the same date (higher = more attractive)."),
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
                     topk: Optional[int] = None,
                     n_drop: Optional[int] = None,
                     start_date: str = None,
                     end_date: str = None,
                     account: float = 100000000,
                     exchange_config: Optional[Dict] = None,
                     instruments=None) -> Dict[str, Any]:
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
            if (start_date and start_date > cal_end) or (end_date and end_date < cal_start):
                return {"success": False, "error": qds.window_message(self.dataset, start_date, end_date),
                        "dataset": qds.brief(self.dataset)}
            last_usable = str(cal[-2].date()) if len(cal) > 1 else cal_end
            start_date = max(start_date, cal_start) if start_date else str(cal[max(0, len(cal) - 253)].date())
            end_date = min(end_date, last_usable) if end_date else last_usable
            if start_date >= end_date:
                return {"success": False,
                        "error": f"Backtest window {start_date} -> {end_date} is empty; the "
                                 f"{self.dataset['id']} dataset covers {cal_start} -> {cal_end}"}
            # Universe: the instruments the panel sends (they used to be ignored, so the
            # backtest always ran on the model's training universe), else the model's own.
            info = dict(model_info)
            if instruments:
                info["instruments"] = instruments
            universe = self._instrument_arg(info.get("instruments"))
            # A market pool trades its members: the exchange needs them as a list (see below).
            codes = universe if isinstance(universe, list) else self._pool_members(universe, start_date, end_date)
            if not codes:
                return {"success": False,
                        "error": f"No instruments of '{universe}' have bars between {start_date} and {end_date} "
                                 f"in the {self.dataset['id']} dataset."}
            warning = None
            if topk is None:
                topk = max(1, len(codes) // 2) if isinstance(universe, list) else min(50, max(1, len(codes) // 2))
            elif int(topk) >= len(codes):
                warning = (f"Top-K ({topk}) is not smaller than the universe ({len(codes)} names): the "
                           f"strategy would hold everything; Top-K reduced to {max(1, len(codes) - 1)}.")
                topk = max(1, len(codes) - 1)
            topk = int(topk)
            n_drop = int(n_drop) if n_drop else max(1, topk // 5)

            # Scores for the whole backtest window from the model's own handler.
            dataset = self._build_test_dataset(info, start_date, end_date)
            pred = model_info["model"].predict(dataset)
            if isinstance(pred, pd.DataFrame):
                pred = pred.iloc[:, 0]
            if pred is None or len(pred) == 0:
                return {"success": False, "error": "The model produced no predictions for this period"}

            # US data has no daily price limit: the CN rule (limit_threshold 0.095) does not
            # belong here. The exchange is built over the backtest universe only -- with the
            # default codes="all" (every instrument in the dataset) no order was ever filled
            # and the backtest reported a flat, zero-return run as a success.
            exchange_kwargs = {
                "deal_price": "close",
                "open_cost": 0.0005,
                "close_cost": 0.0015,
                "min_cost": 5.0,
            }
            exchange_kwargs["codes"] = codes
            if exchange_config:
                exchange_kwargs.update(exchange_config)

            # benchmark=None does NOT mean "none" to qlib -- it falls back to SH000300,
            # which does not exist in US data. A flat zero-return series disables it.
            # A Yahoo-built dataset carries its benchmark ETF (SPY): use it when none is given.
            if not benchmark and self.dataset.get("benchmark") and                     self.dataset["benchmark"].lower() in qds.instrument_names(Path(self.dataset["path"])):
                benchmark = self.dataset["benchmark"]
            bench_arg = benchmark
            if not bench_arg:
                bench_arg = pd.Series(
                    0.0, index=pd.DatetimeIndex(D.calendar(start_time=start_date, end_time=end_date, freq="day")))

            strategy = TopkDropoutStrategy(signal=pred, topk=topk, n_drop=n_drop)
            report, _positions = backtest_daily(
                start_time=start_date, end_time=end_date, strategy=strategy, account=account,
                benchmark=bench_arg, exchange_kwargs=exchange_kwargs)

            if "turnover" in report and float(report["turnover"].abs().sum()) == 0.0:
                return {"success": False,
                        "error": "The strategy placed no trades in this window (no prices for the universe "
                                 "or no usable scores), so there is no backtest to report."}

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
            # The every-step-th sample misses the final day whenever (n_days - 1) % step != 0;
            # the curve must end on the value TOTAL RETURN is computed from.
            if n_days and curve and curve[-1]["date"] != str(equity.index[-1].date()):
                curve.append({"date": str(equity.index[-1].date()), "portfolio": float(equity.values[-1]),
                              "benchmark": float(bench_curve.values[-1])})
            result = {
                "success": True,
                "model_id": model_id,
                "strategy": strategy_type,
                "benchmark": benchmark,
                "universe": universe if isinstance(universe, list) else [universe],
                "topk": topk,
                "n_drop": n_drop,
                "account": account,
                "period": {"start": start_date, "end": end_date},
                "metrics": metrics,
                "equity_curve": curve,
                "message": f"Backtest of {model_id} completed over {n_days} trading days"
            }
            if warning:
                result["warning"] = warning
            # The model's record keeps its latest backtest: list_trained_models shows it.
            self._update_model_meta(model_id, {"last_backtest": {
                "at": datetime.now().isoformat(timespec="seconds"),
                "dataset_id": self.dataset["id"],
                "period": {"start": start_date, "end": end_date},
                "topk": topk,
                "n_drop": n_drop,
                "benchmark": benchmark,
                **{k: metrics.get(k) for k in ("total_return", "annualized_return", "volatility", "sharpe_ratio",
                                              "max_drawdown", "information_ratio", "excess_annualized_return",
                                              "trading_days")},
            }})
            return result
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
                v_start, v_end, window_err = self._window(v_start, v_end)
                if window_err:
                    window_err["error"] = f"The validation period of {model_id} is outside the dataset. "                                           + window_err["error"]
                    return window_err
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
            cal = D.calendar(freq="day")
            if start_date and end_date and start_date > end_date:
                return {"success": False, "error": f"start_date {start_date} is after end_date {end_date}"}
            end_date = end_date or str(cal[-1].date())
            start_date = start_date or str(cal[max(0, len(cal) - 253)].date())
            start_date, end_date, window_err = self._window(start_date, end_date)
            if window_err:
                return window_err

            # A factor's IC ranks the instruments against each other on every day: it needs
            # MIN_CROSS_SECTION names with prices in the window. Fewer -> say so, with the
            # count, instead of a table of +-1 correlations or qlib's own failure.
            if isinstance(universe, list):
                known = {str(s).lower() for s in self._pool_members("all", start_date, end_date)}
                have = [s for s in universe if s in known]
                missing = [s for s in universe if s not in known]
            else:
                have = [str(s).lower() for s in self._pool_members(universe, start_date, end_date)]
                missing = []
            if len(have) < MIN_CROSS_SECTION:
                markets = ", ".join(self.dataset.get("markets", [])) or "none"
                if isinstance(universe, str):
                    given = f"the market pool '{universe}' has {len(have)} with prices in that window"
                elif missing:
                    given = (f"{len(universe)} were given but only {len(have)} have prices in that window in the "
                             f"{self.dataset['id']} dataset (not found: {', '.join(missing[:8])}"
                             f"{'...' if len(missing) > 8 else ''})")
                else:
                    given = f"{len(have)} were given ({', '.join(have)})"
                return {"success": False,
                        "error": f"Factor analysis needs at least {MIN_CROSS_SECTION} instruments with prices "
                                 f"between {start_date} and {end_date}: it ranks them against each other on every "
                                 f"day. {given[:1].upper() + given[1:]}. Add instruments, or enter a market pool "
                                 f"of the dataset ({markets}).",
                        "error_kind": "too_few_instruments",
                        "min_instruments": MIN_CROSS_SECTION,
                        "instruments_with_data": len(have)}

            from qlib.contrib.data.handler import Alpha158
            handler = Alpha158(instruments=universe, start_time=start_date, end_time=end_date,
                               fit_start_time=start_date, fit_end_time=end_date)
            feats = handler.fetch(col_set="feature", data_key=DataHandlerLP.DK_R)
            label = handler.fetch(col_set="label", data_key=DataHandlerLP.DK_R).iloc[:, 0]
            feats = feats.replace([np.inf, -np.inf], np.nan)
            # Only the days with a full enough cross-section (late listings, gaps).
            names_per_day = label.groupby(level="datetime").count()
            full_days = names_per_day[names_per_day >= MIN_CROSS_SECTION].index
            feats = feats[feats.index.get_level_values("datetime").isin(full_days)]
            if feats.empty:
                return {"success": False,
                        "error": f"No day between {start_date} and {end_date} has at least {MIN_CROSS_SECTION} "
                                 f"instruments with a next-day return, so no factor IC can be computed. Widen the "
                                 f"window or add instruments.",
                        "error_kind": "too_few_instruments", "min_instruments": MIN_CROSS_SECTION}
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
                "n_instruments": len(have),
                "min_instruments": MIN_CROSS_SECTION,
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
        model_type_lower = (model_type or "").lower()
        model_class = _model_class(model_type_lower)
        if model_class is None:
            return {"success": False, "error": f"Unknown model type: {model_type}"}

        try:
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
            if start_date and end_date and start_date > end_date:
                return {"success": False, "error": f"start_date {start_date} is after end_date {end_date}"}
            start_date, end_date, window_err = self._window(start_date, end_date)
            if window_err:
                return window_err
            calendar = D.calendar(start_time=start_date, end_time=end_date, freq=freq)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in calendar]
            if not dates:
                return {"success": False, "error": qds.window_message(self.dataset, start_date, end_date)}

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
        data_err = self._data_error()
        if data_err:
            return data_err

        try:
            market = (market or "all").lower()
            markets = self.dataset.get("markets", [])
            if market not in markets:
                return {"success": False, "error": f"The {self.dataset['id']} dataset has no market '{market}' "
                                                   f"(it has: {', '.join(markets)})."}
            # D.instruments() only returns a {market, filter_pipe} config; the old code read
            # it as a list and so always answered "No instruments found".
            instrument_list = sorted(D.list_instruments(D.instruments(market=market), start_time=start_date,
                                                        end_time=end_date, as_list=True))
            if not instrument_list:
                return {
                    "success": False,
                    "error": f"No instruments found for market '{market}' in the {self.dataset['id']} dataset.",
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


def download_data(region: str = "us", target_dir: Optional[str] = None) -> Dict[str, Any]:
    """Download the public qlib daily dataset (Yahoo-sourced, ~450 MB zip for US) into
    <FINCEPT_DATA_DIR>/qlib_data/<region>_data -- where qlib_datasets reads it from.

    Uses qlib's own downloader (qlib.tests.data.GetData, the code behind
    `python -m qlib.cli.data qlib_data`); its HTTP requests carry a 60 s timeout. An
    existing dataset is left untouched (never deleted or re-downloaded): without a
    target_dir that includes one in qlib's own ~/.qlib/qlib_data/<region>_data (earlier
    releases downloaded there), which is then reported as already present.
    """
    import contextlib
    region = str(region or "us").lower()
    if region not in ("us", "cn"):
        return {"success": False, "error": f"region must be 'us' or 'cn', got '{region}'"}
    if not _ensure_qlib():
        return {"success": False, "error": f"qlib is not installed: {QLIB_ERROR}"}
    from qlib.tests.data import GetData
    from qlib.utils import exists_qlib_data
    if target_dir:
        target = Path(target_dir).expanduser()
    else:
        target = qds.public_dataset_dir(region)
        if not exists_qlib_data(str(target)):
            # Nothing complete to reuse: download into the data root (never into ~/.qlib).
            target = qds.public_download_dir(region)

    def calendar_range():
        cal_file = target / "calendars" / "day.txt"
        try:
            days = [ln.strip() for ln in cal_file.read_text(encoding="utf-8").splitlines() if ln.strip()]
            return (days[0], days[-1], len(days)) if days else (None, None, 0)
        except OSError:
            return (None, None, 0)

    already = bool(exists_qlib_data(str(target)))
    if not already:
        target.mkdir(parents=True, exist_ok=True)
        # qlib logs and tqdm bars must not reach stdout (one JSON payload only).
        with contextlib.redirect_stdout(sys.stderr):
            GetData(delete_zip_file=True).qlib_data(target_dir=str(target), region=region,
                                                    delete_old=False, exists_skip=True)
        if not exists_qlib_data(str(target)):
            return {"success": False, "error": f"Download finished but no qlib dataset was found in {target}"}
    first, last, n_days = calendar_range()
    return {
        "success": True,
        "region": region,
        "target_dir": str(target),
        "already_present": already,
        "calendar_start": first,
        "calendar_end": last,
        "trading_days": n_days,
        "message": (f"qlib {region.upper()} dataset {'was already present' if already else 'downloaded'} "
                    f"in {target}" + (f" ({first} to {last})" if first else "")),
    }


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


# LightGBM's training log ("Training until validation scores don't improve..."),
# CatBoost / XGBoost native output and qlib's own prints go to stdout, ahead of the
# JSON payload. While a command runs, file descriptor 1 is pointed at stderr; the
# final payload is written to the real stdout, so stdout carries exactly one JSON line.
_REAL_STDOUT_FD = None


def _quiet_stdout() -> None:
    global _REAL_STDOUT_FD
    if _REAL_STDOUT_FD is not None:
        return
    try:
        sys.stdout.flush()
        _REAL_STDOUT_FD = os.dup(1)
        os.dup2(2, 1)
    except OSError:
        _REAL_STDOUT_FD = None


def _emit_final(obj) -> None:
    global _REAL_STDOUT_FD
    if _REAL_STDOUT_FD is not None:
        try:
            sys.stdout.flush()
        except Exception:
            pass
        os.dup2(_REAL_STDOUT_FD, 1)
        os.close(_REAL_STDOUT_FD)
        _REAL_STDOUT_FD = None
    fincept_json.emit(obj)
    sys.stdout.flush()


# Commands that read no market data: no dataset is chosen and qlib is not initialised.
_NO_DATA_COMMANDS = {"list_models", "get_data_handlers", "get_factor_library", "get_strategies",
                     "list_trained_models"}
# Commands that use a saved model: by default they run on the dataset the model was trained on.
_MODEL_BOUND = {"predict", "run_backtest", "get_factor_analysis", "get_feature_importance", "save_model"}
_STD_POOLS = {"sp500", "nasdaq100", "all", "csi300", "csi500", "csi100"}


def _parse_params(argv: List[str]) -> Dict[str, Any]:
    if len(argv) <= 2 or not argv[2].strip():
        return {}
    raw = argv[2].strip()
    if not raw.startswith("{"):
        return {"model_id": raw}        # get_feature_importance used to take the bare id
    params = json.loads(raw)
    if not isinstance(params, dict):
        raise ValueError("parameters must be a JSON object")
    return params


def _pool_of(universe) -> Optional[str]:
    if isinstance(universe, str):
        return universe.lower()
    if isinstance(universe, list) and len(universe) == 1:
        return str(universe[0]).lower()
    return None


def _covers(target: Dict[str, Any], universe, source: Optional[Dict[str, Any]]) -> Tuple[bool, str]:
    """Can a model whose universe is `universe` be applied to dataset `target`? A ticker list:
    every ticker must be in it. A market pool: it must exist there with the same members as
    in the model's own dataset (`source`, when that is still on this machine)."""
    pools = {str(m).lower() for m in target.get("markets", [])}
    pool = _pool_of(universe)
    if pool is not None and (isinstance(universe, str) or pool in pools | _STD_POOLS):
        if pool not in pools:
            return False, f"its universe '{pool}' is not a market of that dataset (it has: {', '.join(sorted(pools))})"
        if source is not None:
            a = set(qds.instrument_names(Path(source["path"]), pool))
            b = set(qds.instrument_names(Path(target["path"]), pool))
            if a != b:
                return False, (f"the market '{pool}' has {len(a)} instruments in {source['id']} but {len(b)} in "
                               f"{target['id']} ({len(a & b)} in common)")
        return True, ""
    names = set(qds.instrument_names(Path(target["path"])))
    missing = [str(t).lower() for t in (universe or []) if str(t).lower() not in names]
    if missing:
        return False, (f"{len(missing)} of its {len(universe)} instruments are not in it "
                       f"({', '.join(missing[:8])}{'...' if len(missing) > 8 else ''})")
    return True, ""


def _choose_dataset(command: str, params: Dict[str, Any]):
    """(dataset info, warning, error) for a command. The `dataset` parameter picks one; without
    it, a command on a saved model uses the model's own dataset and every other command the
    freshest dataset. A model is never applied to a dataset that lacks its instruments, and
    applying it to another dataset that has them is flagged in the result's warning."""
    requested = params.get("dataset") or None
    model_id = params.get("model_id") if command in _MODEL_BOUND else None
    meta = QlibService.model_meta(model_id) if model_id else None
    if meta is None:
        info, err = qds.resolve(requested)
        return info, None, err
    model_ds = meta.get("dataset_id") or _LEGACY_DATASET_ID
    source, _ = qds.resolve(model_ds)
    info, err = (source, None) if (not requested and source) else qds.resolve(requested)
    if err or info is None:
        return None, None, err
    if info["id"] == model_ds or command in ("get_feature_importance", "save_model"):
        return info, None, None
    universe = params.get("instruments") or meta.get("instruments")
    ok, detail = _covers(info, universe, source)
    where = "" if source else f" ({model_ds} is no longer on this machine)"
    if not ok:
        return None, None, (f"Model {model_id} was trained on the {model_ds} dataset{where} and cannot be applied "
                            f"to {info['id']} ({info['name']}): {detail}. Select {model_ds} in the dataset bar, or "
                            f"retrain the model on {info['id']}.")
    return info, (f"Model {model_id} was trained on the {model_ds} dataset{where}; it is applied here to "
                  f"{info['id']}, which holds all of its instruments but whose prices come from a different "
                  f"build. Retrain it on {info['id']} for signals consistent with this dataset."), None


def _check_status() -> Dict[str, Any]:
    """Library status without importing qlib's models (that import alone took ~30 s)."""
    installed = _find_spec("qlib")
    infos = qds.list_datasets()
    default = qds._freshest(infos)
    return {
        "success": True,
        "qlib_available": installed,
        "version": _qlib_version() if installed else None,
        "models_available": _models_available() if installed else {},
        "handlers_available": _handlers_available(),
        "strategies_available": _strategies_available(),
        "data_available": default is not None,
        "default_dataset": default["id"] if default else None,
        "datasets": [qds.brief(i) for i in infos],
    }


def main():
    """CLI interface for Qlib service"""
    _quiet_stdout()
    if len(sys.argv) < 2:
        _emit_final({"success": False, "error": "No command specified"})
        sys.exit(1)

    command = sys.argv[1]
    try:
        params = _parse_params(sys.argv)
    except ValueError as e:
        _emit_final({"success": False, "error": f"Invalid JSON parameters: {e}"})
        sys.exit(1)

    # Dataset registry / Yahoo builder: plain files and yfinance, no qlib.
    if command in qds.COMMANDS:
        _emit_final(_json_safe(qds.run_command(command, params)))
        return
    if command == "download_data":
        # Needs no initialised qlib (there is no data yet): handled before QlibService().
        try:
            _ensure_qlib()
            _emit_final(_json_safe(download_data(params.get("region", "us"), params.get("target_dir"))))
        except Exception as e:
            _emit_final({"success": False, "error": f"Dataset download failed: {e}"})
            sys.exit(1)
        return
    if command == "check_status":
        _emit_final(_json_safe(_check_status()))
        return

    if command in _NO_DATA_COMMANDS:
        service = QlibService(init=False)
    else:
        info, ds_warning, ds_error = _choose_dataset(command, params)
        if ds_error:
            _emit_final({"success": False, "error": ds_error})
            return
        service = QlibService(dataset=info)
        if ds_warning:
            service.notes.append(ds_warning)

    try:
        if command == "list_models":
            result = service.list_models()

        elif command == "list_trained_models":
            # The models TRAIN saved (ids for Backtest / Feature Importance / Model Signals).
            result = QlibService.list_trained_models()

        elif command == "get_data_handlers":
            result = service.get_data_handlers()

        elif command == "get_factor_library":
            result = service.get_factor_library()

        elif command == "get_strategies":
            result = service.get_strategies()

        elif command == "get_data":
            result = service.get_data(
                params.get("instruments", []),
                params.get("start_date"),
                params.get("end_date"),
                params.get("fields"),
                params.get("freq", "day")
            )

        elif command == "create_dataset":
            result = service.create_dataset(
                params.get("instruments"),
                params.get("start_time"),
                params.get("end_time"),
                params.get("handler_type", "Alpha158"),
                params.get("segments")
            )

        elif command == "train_model":
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
            instruments = dataset_config.get("instruments") or params.get("instruments") or []
            split_keys = ("train_start", "train_end", "valid_start", "valid_end")
            if all(params.get(k) for k in split_keys):
                # The panel sends the four split dates (defaulted from the dataset's calendar).
                train_start, train_end, valid_start, valid_end = (str(params[k])[:10] for k in split_keys)
            else:
                # The window as start_date / end_date (or the explicit spellings), split 80/20.
                # Reading only train_start / valid_end used to make every panel run die in
                # strptime(None, ...).
                start_time = (dataset_config.get("start_time") or params.get("train_start")
                              or params.get("start_date"))
                end_time = (dataset_config.get("end_time") or params.get("valid_end")
                            or params.get("end_date"))
                defaults = (service.dataset or {}).get("defaults", {})
                if not start_time or not end_time:
                    data_err = service._data_error()
                    if data_err:
                        _emit_final(data_err)
                        return
                if not start_time and not end_time and defaults.get("train") and defaults.get("valid"):
                    # No window at all: the dataset's default split (the year before the
                    # last for validation, up to five years before that for training).
                    (train_start, train_end), (valid_start, valid_end) = defaults["train"], defaults["valid"]
                else:
                    end_time = end_time or defaults.get("valid", [None, None])[1]
                    start_time = start_time or defaults.get("train", [None])[0]
                    train_start, valid_end = str(start_time)[:10], str(end_time)[:10]
                    if params.get("train_end") and params.get("valid_start"):
                        train_end = params["train_end"]
                        valid_start = params["valid_start"]
                    else:
                        from datetime import timedelta
                        d_start = datetime.strptime(train_start, "%Y-%m-%d")
                        d_end = datetime.strptime(valid_end, "%Y-%m-%d")
                        split_point = d_start + (d_end - d_start) * 0.8
                        train_end = split_point.strftime("%Y-%m-%d")
                        # qlib segment bounds are inclusive: starting validation on the split
                        # day itself would put that day in both train and valid.
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
            result = service.predict(
                params.get("model_id"),
                params.get("instruments"),
                params.get("start_date"),
                params.get("end_date")
            )

        elif command == "run_backtest":
            result = service.run_backtest(
                params.get("model_id"),
                params.get("strategy_type", "topk_dropout"),
                params.get("benchmark") or None,
                params.get("topk"),
                params.get("n_drop"),
                params.get("start_date"),
                params.get("end_date"),
                params.get("account", 100000000),
                params.get("exchange_config"),
                params.get("instruments")
            )

        elif command == "get_factor_analysis":
            result = service.get_factor_analysis(
                params.get("model_id"),
                params.get("analysis_type", "ic"),
                params.get("instruments"),
                params.get("start_date"),
                params.get("end_date")
            )

        elif command == "get_feature_importance":
            result = service.get_feature_importance(params.get("model_id"))

        elif command == "save_model":
            result = service.save_model(
                params.get("model_id"),
                params.get("path")
            )

        elif command == "load_model":
            result = service.load_model(
                params.get("model_type"),
                params.get("path"),
                params.get("model_id")
            )

        elif command == "get_calendar":
            result = service.get_calendar(
                params.get("start_date"),
                params.get("end_date"),
                params.get("freq", "day")
            )

        elif command == "get_instruments":
            result = service.get_instruments(
                params.get("market", "all"),
                params.get("start_date"),
                params.get("end_date")
            )

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        # Every data command says which dataset it read, and what was adjusted on the way
        # (dates clamped to the calendar, a model applied to another dataset).
        if isinstance(result, dict) and service.dataset is not None:
            result.setdefault("dataset", qds.brief(service.dataset))
            if service.notes:
                prior = result.get("warning")
                result["warning"] = " ".join(([prior] if prior else []) + service.notes)
        _emit_final(_json_safe(result))

    except Exception as e:
        _emit_final({"success": False, "error": str(e)})
        sys.exit(1)


if __name__ == "__main__":
    main()
