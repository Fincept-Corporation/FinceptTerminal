"""
AI Quant Lab - Meta Learning Module
Model selection, hyperparameter tuning and ensembles on real market data
(scikit-learn, LightGBM, XGBoost, CatBoost).

Every training command takes a market-data spec and builds its dataset from the
ticker's daily bars (qlib_feature_engineering.build_supervised_dataset): 16
scale-free features, target = forward `horizon`-day return (classification: up /
down), chronological ~80/20 split with the last `horizon` train rows purged.
There is no synthetic-data path: no ticker / no data -> {"success": false, ...}.

CLI (one JSON payload on stdout):
  list_models
  run_selection        {"model_ids": ["lightgbm","xgboost"], "task_type": "regression"|"classification",
                        "ticker": "AAPL", "start_date"?, "end_date"?, "horizon"?: 5}
  tune_hyperparameters {"model_id": "lightgbm", "param_grid": {"n_estimators": [50,100]},
                        "search_method": "grid"|"random", "task_type", "ticker", "start_date"?,
                        "end_date"?, "horizon"?, "cv"?: 5}
  create_ensemble      {"model_keys": ["lightgbm", "xgboost_20261116_142913"], "method": "voting"|"stacking"}
                       (a bare model id resolves to its latest trained key; every member must have
                        been trained on the same dataset)
  get_results
"""

import os

# Cap native thread pools before numpy / sklearn / boosters load: a tuning run used
# to start one joblib worker per core (48 on a server), each with a full thread pool.
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
from typing import Any, Dict, List, Optional

warnings.filterwarnings('ignore')

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (_HERE, os.path.join(_HERE, '..')):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import fincept_json  # noqa: E402

# Availability is probed without importing: lightgbm / xgboost / catboost / qlib
# each cost seconds to import and list_models needs none of them.
def _has(module: str) -> bool:
    try:
        return importlib.util.find_spec(module) is not None
    except (ImportError, ValueError):
        return False


SKLEARN_AVAILABLE = _has('sklearn')
LIGHTGBM_AVAILABLE = _has('lightgbm')
XGBOOST_AVAILABLE = _has('xgboost')
CATBOOST_AVAILABLE = _has('catboost')
QLIB_AVAILABLE = _has('qlib')

STATE_VERSION = 2
TASK_TYPES = ('regression', 'classification')
MODEL_IDS = ('random_forest', 'lightgbm', 'xgboost', 'catboost')


# ── Metrics ───────────────────────────────────────────────────────────────────

def _metrics(task_type: str, y_true: np.ndarray, y_pred: np.ndarray, proba: Optional[np.ndarray] = None) -> Dict:
    from sklearn.metrics import accuracy_score, f1_score, mean_squared_error, r2_score, roc_auc_score

    if task_type == 'classification':
        out = {
            'accuracy': float(accuracy_score(y_true, y_pred)),
            'f1_score': float(f1_score(y_true, y_pred, average='weighted', zero_division=0)),
            'auc_roc': None,
        }
        if proba is not None and len(np.unique(y_true)) == 2:
            out['auc_roc'] = float(roc_auc_score(y_true, proba))
        return out
    mse = float(mean_squared_error(y_true, y_pred))
    nz = y_true != 0
    return {
        'mse': mse,
        'rmse': float(np.sqrt(mse)),
        'r2_score': float(r2_score(y_true, y_pred)),
        # Share of hold-out rows whose sign the prediction got right (0 returns skipped).
        'directional_accuracy': float(np.mean(np.sign(y_pred[nz]) == np.sign(y_true[nz]))) if nz.any() else None,
        'ic': (float(np.corrcoef(y_pred, y_true)[0, 1])
               if np.std(y_pred) > 0 and np.std(y_true) > 0 else None),
    }


def _baseline(task_type: str, y_train: np.ndarray, y_test: np.ndarray) -> Dict:
    """What a model has to beat: the train majority class / the train mean, on the hold-out."""
    if task_type == 'classification':
        values, counts = np.unique(y_train, return_counts=True)
        majority = int(values[np.argmax(counts)])
        return {
            'name': 'majority class of the training window',
            'majority_class': majority,
            'accuracy': float(np.mean(y_test == majority)),
            'test_up_share': float(np.mean(y_test == 1)),
        }
    mean = float(np.mean(y_train))
    pred = np.full_like(y_test, mean, dtype=float)
    base = _metrics('regression', y_test, pred)
    nz = y_test != 0
    return {
        'name': 'training-window mean return',
        'mean_return': mean,
        'mse': base['mse'],
        'rmse': base['rmse'],
        'r2_score': base['r2_score'],
        # Always calling the sign of the training mean.
        'directional_accuracy': float(np.mean(np.sign(y_test[nz]) == np.sign(mean))) if nz.any() and mean != 0 else None,
    }


def _score(task_type: str, metrics: Dict) -> Optional[float]:
    return metrics.get('accuracy') if task_type == 'classification' else metrics.get('r2_score')


def _predict(model, task_type: str, X: np.ndarray):
    pred = np.asarray(model.predict(X)).reshape(-1)
    proba = None
    if task_type == 'classification':
        pred = pred.astype(int)
        if hasattr(model, 'predict_proba'):
            try:
                p = model.predict_proba(X)
                proba = p[:, 1] if p.ndim == 2 and p.shape[1] == 2 else None
            except Exception:
                proba = None
    return pred, proba


# ── Datasets ──────────────────────────────────────────────────────────────────

def _data_spec(params: Dict, task_type: str) -> Dict:
    ticker = str(params.get('ticker') or '').strip().upper()
    if not ticker:
        raise ValueError('ticker is required: models are trained on that ticker\'s daily bars '
                         '(e.g. {"ticker": "AAPL"})')
    return {
        'ticker': ticker,
        'start_date': (str(params['start_date']).strip() or None) if params.get('start_date') else None,
        'end_date': (str(params['end_date']).strip() or None) if params.get('end_date') else None,
        'horizon': params.get('horizon', 5),
        'task_type': task_type,
    }


def _dataset_id(info: Dict) -> str:
    return '|'.join(str(info[k]) for k in ('ticker', 'start_date', 'end_date', 'horizon', 'task_type'))


def _public_data(info: Dict) -> Dict:
    return {k: info[k] for k in ('ticker', 'start_date', 'end_date', 'horizon', 'task_type', 'target',
                                 'bars', 'train_rows', 'test_rows', 'purged_rows', 'train_period',
                                 'test_period', 'source') if k in info}


def _load_dataset(spec: Dict) -> Dict:
    from qlib_feature_engineering import build_supervised_dataset
    return build_supervised_dataset(spec['ticker'], spec['start_date'], spec['end_date'],
                                    spec['horizon'], spec['task_type'])


class _Blend:
    """Time-series stacking: base models fit on the first 75% of the training
    window predict the last 25%; a linear final estimator learns from those
    out-of-sample predictions; the base models are then refit on the whole
    training window. (sklearn's Stacking* needs K-fold partitions, which train
    on future rows.) A plain object so it pickles under any __main__."""

    def __init__(self, members, final, task_type):
        self.members = members
        self.final = final
        self.task_type = task_type

    def _base_outputs(self, X):
        cols = []
        for m in self.members:
            pred, proba = _predict(m, self.task_type, X)
            cols.append(proba if proba is not None else pred)
        return np.column_stack(cols)

    def predict(self, X):
        return self.final.predict(self._base_outputs(X))

    def predict_proba(self, X):
        return self.final.predict_proba(self._base_outputs(X))


# ── Manager ───────────────────────────────────────────────────────────────────

class MetaLearningManager:
    """Model comparison, hyperparameter tuning and ensembles; state in
    <data root>/script-data/meta_learning (fincept_paths)."""

    def __init__(self):
        self.trained_models: Dict[str, Any] = {}
        self.evaluation_results: Dict[str, Dict] = {}
        self.datasets: Dict[str, Dict] = {}      # dataset id -> arrays + info (members of an ensemble share one)
        self.best_model_id: Optional[str] = None
        self.load_warning: Optional[str] = None
        # Resolved here, not at import; the ~/.fincept/meta_learning of releases is copied in
        # once, never moved.
        import fincept_paths
        self.CACHE_DIR = fincept_paths.user_data_dir('meta_learning', legacy='meta_learning')
        self._load_state()

    # -- persistence --
    def _load_state(self):
        cache_file = self.CACHE_DIR / 'meta_state.pkl'
        if not cache_file.exists():
            return
        try:
            with open(cache_file, 'rb') as f:
                data = pickle.load(f)
        except Exception as e:
            self.load_warning = (f'Saved meta-learning models could not be loaded ({e}); they were probably '
                                 'written by an older library version. Train them again.')
            print(f"Warning: {self.load_warning}", file=sys.stderr)
            return
        if data.get('version') != STATE_VERSION:
            # Version 1 held models fit to random numbers (no market data): nothing to keep.
            print("Info: discarding meta-learning state from an older version (synthetic-data models)",
                  file=sys.stderr)
            return
        self.trained_models = data.get('models', {})
        self.evaluation_results = data.get('results', {})
        self.datasets = data.get('datasets', {})
        self.best_model_id = data.get('best_model')

    def _prune(self):
        """Keep the latest key per (model_id, task_type); drop unreferenced datasets."""
        latest: Dict[tuple, str] = {}
        for key, res in self.evaluation_results.items():
            k = (res.get('model_id'), res.get('task_type'))
            if k not in latest or res.get('trained_at', '') >= self.evaluation_results[latest[k]].get('trained_at', ''):
                latest[k] = key
        keep = set(latest.values())
        for key in list(self.evaluation_results):
            if key not in keep:
                self.evaluation_results.pop(key, None)
                self.trained_models.pop(key, None)
        for key in list(self.trained_models):
            if key not in self.evaluation_results:
                self.trained_models.pop(key, None)
        used = {res.get('dataset_id') for res in self.evaluation_results.values()}
        for ds in list(self.datasets):
            if ds not in used:
                self.datasets.pop(ds, None)
        if self.best_model_id not in self.evaluation_results:
            self.best_model_id = None

    def _save_state(self):
        self._prune()
        cache_file = self.CACHE_DIR / 'meta_state.pkl'
        tmp = cache_file.with_suffix('.tmp')
        try:
            with open(tmp, 'wb') as f:
                pickle.dump({'version': STATE_VERSION, 'models': self.trained_models,
                             'results': self.evaluation_results, 'datasets': self.datasets,
                             'best_model': self.best_model_id}, f)
            os.replace(tmp, cache_file)
        except Exception as e:
            print(f"Warning: Could not save state: {e}", file=sys.stderr)

    def _dataset(self, spec: Dict) -> Dict:
        ds = _load_dataset(spec)
        info = ds['info']
        ds_id = _dataset_id(info)
        self.datasets[ds_id] = {'X_train': ds['X_train'], 'y_train': ds['y_train'],
                                'X_test': ds['X_test'], 'y_test': ds['y_test'], 'info': info}
        return dict(self.datasets[ds_id], id=ds_id)

    def _store(self, model_id: str, model, task_type: str, metrics: Dict, ds: Dict, extra: Optional[Dict] = None) -> str:
        key = f"{model_id}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        while key in self.evaluation_results:      # two stores within one second
            key += '_'
        self.trained_models[key] = model
        self.evaluation_results[key] = {
            'model_id': model_id,
            'task_type': task_type,
            'metrics': metrics,
            'trained_at': datetime.now().isoformat(),
            'n_train_samples': int(len(ds['X_train'])),
            'n_test_samples': int(len(ds['X_test'])),
            'dataset_id': ds['id'],
            'data': _public_data(ds['info']),
            **(extra or {}),
        }
        return key

    # -- catalogue --
    def get_available_models(self) -> Dict[str, Any]:
        catalogue = [
            ('random_forest', 'Random Forest', 'ensemble', 'scikit-learn', SKLEARN_AVAILABLE,
             'Ensemble of decision trees with bagging'),
            ('lightgbm', 'LightGBM', 'gradient_boosting', 'lightgbm', LIGHTGBM_AVAILABLE,
             'Fast gradient boosting framework by Microsoft'),
            ('xgboost', 'XGBoost', 'gradient_boosting', 'xgboost', XGBOOST_AVAILABLE, 'Extreme gradient boosting'),
            ('catboost', 'CatBoost', 'gradient_boosting', 'catboost', CATBOOST_AVAILABLE,
             'Gradient boosting with ordered boosting'),
        ]
        models = [{'id': i, 'name': n, 'type': t, 'library': lib, 'description': d}
                  for i, n, t, lib, ok, d in catalogue if ok and SKLEARN_AVAILABLE]
        return {
            'success': True,
            'models': models,
            'count': len(models),
            'sklearn_available': SKLEARN_AVAILABLE,
            'lightgbm_available': LIGHTGBM_AVAILABLE,
            'xgboost_available': XGBOOST_AVAILABLE,
            'catboost_available': CATBOOST_AVAILABLE,
            'qlib_available': QLIB_AVAILABLE,
        }

    @staticmethod
    def _create_model(model_id: str, task_type: str):
        """Estimator with an explicit thread cap (never all cores)."""
        clf = task_type == 'classification'
        if model_id == 'random_forest' and SKLEARN_AVAILABLE:
            from sklearn.ensemble import RandomForestClassifier, RandomForestRegressor
            cls = RandomForestClassifier if clf else RandomForestRegressor
            return cls(n_estimators=100, min_samples_leaf=5, random_state=42, n_jobs=_MAX_THREADS)
        if model_id == 'lightgbm' and LIGHTGBM_AVAILABLE:
            import lightgbm as lgb
            cls = lgb.LGBMClassifier if clf else lgb.LGBMRegressor
            return cls(n_estimators=100, random_state=42, verbose=-1, n_jobs=_MAX_THREADS)
        if model_id == 'xgboost' and XGBOOST_AVAILABLE:
            import xgboost as xgb
            cls = xgb.XGBClassifier if clf else xgb.XGBRegressor
            return cls(n_estimators=100, random_state=42, verbosity=0, n_jobs=_MAX_THREADS)
        if model_id == 'catboost' and CATBOOST_AVAILABLE:
            import catboost as cb
            cls = cb.CatBoostClassifier if clf else cb.CatBoostRegressor
            # allow_writing_files=False: CatBoost otherwise writes catboost_info/ into the cwd
            # (the scripts directory, read-only in an install).
            return cls(iterations=100, random_state=42, verbose=False, thread_count=_MAX_THREADS,
                       allow_writing_files=False)
        return None

    # -- commands --
    def run_model_selection(self, model_ids: List[str], task_type: str, data_params: Dict,
                            metric: str = 'auto') -> Dict[str, Any]:
        if not SKLEARN_AVAILABLE:
            return {'success': False, 'error': 'scikit-learn is not installed'}
        if task_type not in TASK_TYPES:
            return {'success': False, 'error': f"task_type must be one of {', '.join(TASK_TYPES)}"}
        model_ids = [str(m).strip().lower() for m in (model_ids or []) if str(m).strip()]
        if not model_ids:
            return {'success': False, 'error': 'No models specified'}
        unknown = [m for m in model_ids if m not in MODEL_IDS]
        if unknown:
            return {'success': False,
                    'error': f"Unknown model id(s): {', '.join(unknown)}. Available: {', '.join(MODEL_IDS)}"}

        ds = self._dataset(_data_spec(data_params, task_type))
        X_train, y_train, X_test, y_test = ds['X_train'], ds['y_train'], ds['X_test'], ds['y_test']
        baseline = _baseline(task_type, y_train, y_test)

        results, ranking = {}, []
        for model_id in dict.fromkeys(model_ids):
            model = self._create_model(model_id, task_type)
            if model is None:
                results[model_id] = {'trained': False, 'error': f'{model_id} is not installed'}
                continue
            try:
                model.fit(X_train, y_train)
                pred, proba = _predict(model, task_type, X_test)
                metrics = _metrics(task_type, y_test, pred, proba)
            except Exception as e:
                results[model_id] = {'trained': False, 'error': str(e)}
                continue
            key = self._store(model_id, model, task_type, metrics, ds)
            results[model_id] = {'trained': True, 'model_key': key, 'metrics': metrics}
            ranking.append({'model_id': model_id, 'model_key': key, 'score': _score(task_type, metrics),
                            'metrics': metrics})

        if not ranking:
            errors = '; '.join(f"{m}: {r.get('error')}" for m, r in results.items())
            return {'success': False, 'error': f'No model could be trained -- {errors}'}

        ranking.sort(key=lambda r: (r['score'] is not None, r['score'] if r['score'] is not None else 0.0),
                     reverse=True)
        self.best_model_id = ranking[0]['model_key']
        self._save_state()
        base_score = baseline['accuracy'] if task_type == 'classification' else baseline['r2_score']
        return {
            'success': True,
            'task_type': task_type,
            'score_metric': 'accuracy' if task_type == 'classification' else 'r2_score',
            'results': results,
            'ranking': ranking,
            'best_model': ranking[0]['model_id'],
            'best_model_key': ranking[0]['model_key'],
            'beats_baseline': bool(ranking[0]['score'] is not None and base_score is not None
                                   and ranking[0]['score'] > base_score),
            'trained_count': len(ranking),
            'baseline': baseline,
            'data': _public_data(ds['info']),
        }

    def _resolve_key(self, ref: str) -> Optional[str]:
        ref = str(ref).strip()
        if ref in self.evaluation_results:
            return ref
        matches = [k for k, r in self.evaluation_results.items() if r.get('model_id') == ref.lower()]
        if not matches:
            return None
        return max(matches, key=lambda k: self.evaluation_results[k].get('trained_at', ''))

    def create_ensemble(self, model_refs: List[str], method: str = 'voting') -> Dict[str, Any]:
        if not SKLEARN_AVAILABLE:
            return {'success': False, 'error': 'scikit-learn is not installed'}
        if method not in ('voting', 'stacking'):
            return {'success': False, 'error': f"Unknown ensemble method: {method} (voting or stacking)"}
        keys, missing = [], []
        for ref in model_refs or []:
            key = self._resolve_key(ref)
            if key is None:
                missing.append(str(ref))
            elif key not in keys:
                keys.append(key)
        if missing:
            have = ', '.join(sorted(self.evaluation_results)) or 'none -- run Model Selection first'
            return {'success': False, 'error': f"No trained model for: {', '.join(missing)}. Trained keys: {have}"}
        if len(keys) < 2:
            return {'success': False, 'error': 'An ensemble needs at least 2 different trained models'}
        if any(self.evaluation_results[k].get('model_id', '').startswith('ensemble') for k in keys):
            return {'success': False, 'error': 'Ensemble members must be single models, not ensembles'}
        ds_ids = {self.evaluation_results[k].get('dataset_id') for k in keys}
        if len(ds_ids) != 1 or None in ds_ids:
            detail = '; '.join(f"{k}: {self.evaluation_results[k].get('dataset_id')}" for k in keys)
            return {'success': False,
                    'error': f'Members were trained on different datasets (ticker / window / horizon / task) -- '
                             f'run Model Selection once with all of them. {detail}'}
        ds_id = ds_ids.pop()
        if ds_id not in self.datasets:
            return {'success': False, 'error': 'The dataset these models were trained on is no longer stored -- '
                                               'run Model Selection again'}
        ds = dict(self.datasets[ds_id], id=ds_id)
        task_type = ds['info']['task_type']
        X_train, y_train, X_test, y_test = ds['X_train'], ds['y_train'], ds['X_test'], ds['y_test']
        from sklearn.base import clone

        members = [(k, self.evaluation_results[k]['model_id']) for k in keys]
        if method == 'voting':
            from sklearn.ensemble import VotingClassifier, VotingRegressor
            estimators = [(f'm{i}', clone(self.trained_models[k])) for i, (k, _) in enumerate(members)]
            ensemble = (VotingClassifier(estimators=estimators, voting='soft') if task_type == 'classification'
                        else VotingRegressor(estimators=estimators))
            ensemble.fit(X_train, y_train)
        else:
            from sklearn.linear_model import LogisticRegression, RidgeCV
            cut = int(len(X_train) * 0.75)
            horizon = int(ds['info'].get('horizon', 0))
            fit_end = max(1, cut - horizon)          # purge: labels of these rows look into the meta window
            if fit_end < 60 or len(X_train) - cut < 30:
                return {'success': False, 'error': 'Too few training rows for stacking -- widen the date range'}
            stage1 = [clone(self.trained_models[k]) for k, _ in members]
            for m in stage1:
                m.fit(X_train[:fit_end], y_train[:fit_end])
            meta_X = _Blend(stage1, None, task_type)._base_outputs(X_train[cut:])
            final = LogisticRegression(max_iter=1000) if task_type == 'classification' else RidgeCV()
            final.fit(meta_X, y_train[cut:])
            full = [clone(self.trained_models[k]) for k, _ in members]
            for m in full:
                m.fit(X_train, y_train)
            ensemble = _Blend(full, final, task_type)

        pred, proba = _predict(ensemble, task_type, X_test)
        metrics = _metrics(task_type, y_test, pred, proba)
        baseline = _baseline(task_type, y_train, y_test)
        model_id = f'ensemble_{method}'
        key = self._store(model_id, ensemble, task_type, metrics, ds,
                          {'members': [k for k, _ in members], 'method': method})
        self._save_state()
        member_rows = [{'model_key': k, 'model_id': mid,
                        'metrics': self.evaluation_results.get(k, {}).get('metrics')} for k, mid in members]
        best_member = max((_score(task_type, r['metrics'] or {}) for r in member_rows
                           if _score(task_type, r['metrics'] or {}) is not None), default=None)
        score = _score(task_type, metrics)
        return {
            'success': True,
            'ensemble_id': key,
            'method': method,
            'task_type': task_type,
            'n_models': len(members),
            'model_keys': [k for k, _ in members],
            'members': member_rows,
            'metrics': metrics,
            'score': score,
            'score_metric': 'accuracy' if task_type == 'classification' else 'r2_score',
            'best_member_score': best_member,
            'beats_best_member': bool(score is not None and best_member is not None and score > best_member),
            'baseline': baseline,
            'data': _public_data(ds['info']),
            'message': f'{method} ensemble of {len(members)} models fitted on {ds["info"]["ticker"]} and scored '
                       f'on the hold-out window',
        }

    def hyperparameter_tuning(self, model_id: str, param_grid: Dict, task_type: str, search_method: str,
                              data_params: Dict, cv: int = 5) -> Dict[str, Any]:
        if not SKLEARN_AVAILABLE:
            return {'success': False, 'error': 'scikit-learn is not installed'}
        if task_type not in TASK_TYPES:
            return {'success': False, 'error': f"task_type must be one of {', '.join(TASK_TYPES)}"}
        if search_method not in ('grid', 'random'):
            return {'success': False, 'error': "search_method must be 'grid' or 'random'"}
        model_id = str(model_id or '').strip().lower()
        if model_id not in MODEL_IDS:
            return {'success': False, 'error': f"Unknown model id '{model_id}'. Available: {', '.join(MODEL_IDS)}"}
        if not isinstance(param_grid, dict) or not param_grid:
            return {'success': False, 'error': 'param_grid must be a non-empty object of name -> list of values'}
        grid = {}
        for name, values in param_grid.items():
            values = values if isinstance(values, list) else [values]
            if not values:
                return {'success': False, 'error': f"param_grid['{name}'] is empty"}
            grid[str(name)] = values
        try:
            cv = int(cv)
        except (TypeError, ValueError):
            cv = 5
        cv = max(2, min(cv, 10))

        model = self._create_model(model_id, task_type)
        if model is None:
            return {'success': False, 'error': f'{model_id} is not installed'}
        valid = set(model.get_params())
        bad = [n for n in grid if n not in valid]
        if bad:
            return {'success': False, 'error': f"{model_id} has no parameter(s) {', '.join(bad)}"}

        ds = self._dataset(_data_spec(data_params, task_type))
        X_train, y_train, X_test, y_test = ds['X_train'], ds['y_train'], ds['X_test'], ds['y_test']
        from sklearn.model_selection import GridSearchCV, RandomizedSearchCV, TimeSeriesSplit

        n_candidates = int(np.prod([len(v) for v in grid.values()]))
        # Chronological folds with a horizon-sized gap, so a validation fold never sees
        # labels that overlap its training rows. n_jobs=1: each estimator already runs
        # _MAX_THREADS threads; a process pool on top multiplied that by every core.
        splitter = TimeSeriesSplit(n_splits=cv, gap=int(ds['info']['horizon']))
        scoring = 'accuracy' if task_type == 'classification' else 'r2'
        if search_method == 'grid':
            search = GridSearchCV(model, grid, cv=splitter, scoring=scoring, n_jobs=1, error_score='raise')
        else:
            search = RandomizedSearchCV(model, grid, n_iter=min(10, n_candidates), cv=splitter, scoring=scoring,
                                        n_jobs=1, random_state=42, error_score='raise')
        search.fit(X_train, y_train)
        best_index = int(search.best_index_)
        fold_scores = [float(search.cv_results_[f'split{i}_test_score'][best_index]) for i in range(cv)]

        best_model = search.best_estimator_
        pred, proba = _predict(best_model, task_type, X_test)
        holdout = _metrics(task_type, y_test, pred, proba)
        key = self._store(model_id, best_model, task_type, holdout, ds,
                          {'tuned': True, 'best_params': search.best_params_})
        self.best_model_id = self.best_model_id or key
        self._save_state()
        return {
            'success': True,
            'model_id': model_id,
            'model_key': key,
            'task_type': task_type,
            'best_params': search.best_params_,
            'best_score': float(search.best_score_),
            'cv_scores': fold_scores,
            'scoring': scoring,
            'search_method': search_method,
            'cv_folds': cv,
            'cv_method': f'TimeSeriesSplit({cv}, gap={ds["info"]["horizon"]})',
            'n_candidates': n_candidates if search_method == 'grid' else min(10, n_candidates),
            'holdout_metrics': holdout,
            'baseline': _baseline(task_type, y_train, y_test),
            'data': _public_data(ds['info']),
        }

    def get_all_results(self) -> Dict[str, Any]:
        results = {k: {kk: vv for kk, vv in r.items() if kk != 'dataset_id'}
                   for k, r in self.evaluation_results.items()}
        out = {
            'success': True,
            'results': results,
            'best_model': self.best_model_id,
            'n_models': len(self.trained_models),
        }
        if self.load_warning:
            out['warning'] = self.load_warning
        return out


def main():
    if len(sys.argv) < 2:
        fincept_json.emit({'success': False, 'error': 'Usage: python qlib_meta_learning.py <command> [json]'})
        return
    command = sys.argv[1]
    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        if not isinstance(params, dict):
            raise ValueError('parameters must be a JSON object')
    except (json.JSONDecodeError, ValueError) as e:
        fincept_json.emit({'success': False, 'error': f'Invalid JSON parameters: {e}'})
        return

    try:
        manager = MetaLearningManager()
        if command == 'list_models':
            result = manager.get_available_models()
        elif command == 'run_selection':
            result = manager.run_model_selection(params.get('model_ids', []),
                                                 str(params.get('task_type', 'regression')), params,
                                                 params.get('metric', 'auto'))
        elif command == 'create_ensemble':
            result = manager.create_ensemble(params.get('model_keys', []), str(params.get('method', 'voting')))
        elif command == 'tune_hyperparameters':
            result = manager.hyperparameter_tuning(params.get('model_id', ''), params.get('param_grid', {}),
                                                   str(params.get('task_type', 'regression')),
                                                   str(params.get('search_method', 'grid')), params,
                                                   params.get('cv', 5))
        elif command == 'get_results':
            result = manager.get_all_results()
        else:
            result = {'success': False, 'error': f'Unknown command: {command}'}
    except ValueError as e:         # bad input / no data: a plain message is the whole story
        result = {'success': False, 'error': str(e)}
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)
        result = {'success': False, 'error': f'{command} failed: {e}'}

    fincept_json.emit(result)


if __name__ == '__main__':
    main()
