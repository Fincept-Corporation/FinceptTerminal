"""
AI Quant Lab - Online Learning Module
Real-time model updates, incremental learning, and online prediction for live trading
Supports rolling updates, concept drift detection, and adaptive retraining
"""

import json
import sys
import os
import pickle
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Any, Optional, Union
import warnings
warnings.filterwarnings('ignore')

import numpy as np
import pandas as pd

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import fincept_json

# Qlib online learning imports -- loaded on demand. Importing qlib costs several
# seconds (it pulls gym, mlflow, ...), and create_model / train / predict /
# performance only need river. Only initialize pays for it.
#
# The status used to import OnlineManager from qlib.contrib.online.manager, which has
# no such class in pyqlib 0.9.x (it lives in qlib.workflow.online.manager), so the
# panel reported QLIB ONLINE: MISSING with qlib installed.
ONLINE_LEARNING_AVAILABLE = None   # unknown until _load_qlib_online()
ONLINE_ERROR = None


def _load_qlib_online() -> bool:
    global ONLINE_LEARNING_AVAILABLE, ONLINE_ERROR, qlib, REG_CN, REG_US
    if ONLINE_LEARNING_AVAILABLE is None:
        try:
            import qlib
            from qlib.workflow.online.manager import OnlineManager  # noqa: F401
            from qlib.config import REG_CN, REG_US
            ONLINE_LEARNING_AVAILABLE = True
        except Exception as e:  # ImportError, or a broken optional dependency
            ONLINE_LEARNING_AVAILABLE = False
            ONLINE_ERROR = str(e)
    return ONLINE_LEARNING_AVAILABLE


def _qlib_online_installed() -> bool:
    """qlib and its online-serving module are installed -- from the files, without the
    multi-second import (list_models runs every time the panel lists its models)."""
    try:
        import importlib.util
        spec = importlib.util.find_spec("qlib")
        if spec is None or not spec.submodule_search_locations:
            return False
        base = Path(list(spec.submodule_search_locations)[0])
        return (base / "workflow" / "online" / "manager.py").exists()
    except (ImportError, ValueError):
        return False


def _qlib_dataset():
    """The local qlib market dataset initialize() would use (the freshest one), or None."""
    try:
        sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))
        import qlib_datasets
        info, _err = qlib_datasets.resolve(None)
        return qlib_datasets.brief(info) if info else None
    except Exception as e:
        print(f"Warning: could not list qlib datasets: {e}", file=sys.stderr)
        return None

# River for online/incremental learning
RIVER_AVAILABLE = False
try:
    from river import linear_model, tree, ensemble, metrics
    from river import preprocessing, feature_extraction, drift
    RIVER_AVAILABLE = True
except ImportError:
    pass


class OnlineLearningManager:
    """
    Online Learning Manager for real-time model updates
    Handles incremental training, concept drift detection, and model adaptation
    Models are kept in <data root>/script-data/online_learning (fincept_paths).
    """

    def __init__(self):
        self.qlib_initialized = False
        self.online_models = {}
        self.drift_detectors = {}
        self.performance_history = []
        self.update_schedule = None
        self.load_warning = None

        # Model folder, resolved here (not at import); the ~/.fincept/online_learning of
        # releases is copied in once, never moved.
        import fincept_paths
        self.CACHE_DIR = fincept_paths.user_data_dir('online_learning', legacy='online_learning')

        # Load existing models from disk
        self._load_models()

    def _load_models(self):
        """Load all saved models from disk"""
        cache_file = self.CACHE_DIR / 'models.pkl'
        if cache_file.exists():
            try:
                with open(cache_file, 'rb') as f:
                    data = pickle.load(f)
                    self.online_models = data.get('models', {})
                    self.drift_detectors = data.get('detectors', {})
            except Exception as e:
                # Usually a river upgrade: the old pickles no longer load. Say so in the
                # payload (it used to be a stderr line followed by an empty model list),
                # and keep the file aside instead of overwriting it on the next save.
                print(f"Warning: Could not load cached models: {e}", file=sys.stderr)
                self.load_warning = (f"Saved online models could not be loaded ({type(e).__name__}: {e}); "
                                     "they were probably created with an older library version. "
                                     "The file is kept as models.pkl.unreadable; create the models again.")

    def _save_models(self):
        """Save all models to disk"""
        cache_file = self.CACHE_DIR / 'models.pkl'
        if self.load_warning and cache_file.exists():
            try:
                cache_file.replace(self.CACHE_DIR / 'models.pkl.unreadable')
            except OSError as e:
                print(f"Warning: could not set the unreadable model file aside: {e}", file=sys.stderr)
        try:
            with open(cache_file, 'wb') as f:
                pickle.dump({
                    'models': self.online_models,
                    'detectors': self.drift_detectors
                }, f)
        except Exception as e:
            print(f"Warning: Could not save models: {e}", file=sys.stderr)

    def initialize(self,
                  provider_uri: Optional[str] = None,
                  region: Optional[str] = None,
                  dataset: Optional[str] = None) -> Dict[str, Any]:
        """Initialize Qlib for online learning. Without a provider_uri: the local dataset
        named by `dataset`, else the freshest one (qlib_datasets)."""
        if not _load_qlib_online():
            return {'success': False, 'error': f'Qlib online learning not available: {ONLINE_ERROR}'}
        if not provider_uri:
            sys.path.insert(1, os.path.dirname(os.path.abspath(__file__)))
            import qlib_datasets
            info, err = qlib_datasets.resolve(dataset)
            if info is None:
                return {'success': False, 'error': err or qlib_datasets.no_dataset_message()}
            provider_uri, region = info['provider_uri'], info['region']
        region = (region or "us").lower()

        try:
            if region == "cn":
                qlib.init(provider_uri=provider_uri, region=REG_CN)
            else:
                qlib.init(provider_uri=provider_uri, region=REG_US)

            self.qlib_initialized = True
            return {
                'success': True,
                'message': 'Qlib initialized for online learning',
                'river_available': RIVER_AVAILABLE
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def create_online_model(self,
                           model_type: str = 'linear',
                           model_id: Optional[str] = None,
                           **kwargs) -> Dict[str, Any]:
        """
        Create online learning model

        Args:
            model_type: 'linear', 'bayesian_linear', 'pa', 'tree', 'adaptive_tree',
                       'isoup_tree', 'bagging', 'ewa', 'srp'
            model_id: Unique identifier for model
            **kwargs: Model-specific parameters
        """
        if not RIVER_AVAILABLE:
            return {'success': False, 'error': 'River library not available for online learning'}

        try:
            model_id = model_id or f"online_model_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

            # Create model based on type.
            # Linear models are SGD-style learners: on raw price-scale features
            # (close ~ 100, rsi ~ 50) a 0.01 step makes them diverge within a few
            # samples (predictions 4.7 -> -1e3 -> 2.6e5 -> ...). A running
            # StandardScaler in front keeps every input at unit scale.
            def scaled(m):
                return preprocessing.StandardScaler() | m

            if model_type == 'linear':
                model = scaled(linear_model.LinearRegression())
            elif model_type == 'bayesian_linear':
                model = scaled(linear_model.BayesianLinearRegression())
            elif model_type == 'pa':
                model = scaled(linear_model.PARegressor())

            # Tree models
            elif model_type == 'tree':
                model = tree.HoeffdingTreeRegressor()
            elif model_type == 'adaptive_tree':
                model = tree.HoeffdingAdaptiveTreeRegressor()
            elif model_type == 'isoup_tree':
                model = tree.iSOUPTreeRegressor()

            # Ensemble models
            elif model_type == 'bagging':
                model = ensemble.BaggingRegressor(
                    model=tree.HoeffdingTreeRegressor(),
                    n_models=kwargs.get('n_models', 10)
                )
            elif model_type == 'ewa':
                model = ensemble.EWARegressor(
                    [scaled(linear_model.LinearRegression()) for _ in range(3)]
                )
            elif model_type == 'srp':
                model = ensemble.SRPRegressor(
                    model=tree.HoeffdingTreeRegressor(),
                    n_models=kwargs.get('n_models', 10)
                )

            # Legacy aliases
            elif model_type == 'adaptive_random_forest':
                model = ensemble.BaggingRegressor(
                    model=tree.HoeffdingTreeRegressor(),
                    n_models=kwargs.get('n_models', 10)
                )
            else:
                return {'success': False, 'error': f'Unknown model type: {model_type}'}

            # Add drift detector
            drift_detector = drift.ADWIN(delta=0.002)

            self.online_models[model_id] = {
                'model': model,
                'type': model_type,
                # 2 = linear learners standardise their inputs (earlier ones diverged).
                'pipeline_version': 2,
                'created_at': datetime.now().isoformat(),
                'samples_trained': 0,
                'last_updated': None,
                'metrics': metrics.MAE()
            }

            self.drift_detectors[model_id] = drift_detector

            # Save to disk
            self._save_models()

            return {
                'success': True,
                'model_id': model_id,
                'model_type': model_type,
                'message': 'Online learning model created'
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def incremental_train(self,
                         model_id: str,
                         features: Union[Dict, pd.DataFrame],
                         target: Union[float, pd.Series]) -> Dict[str, Any]:
        """
        Incrementally train model with new data point(s)

        Args:
            model_id: Model identifier
            features: Feature dict or DataFrame
            target: Target value(s)
        """
        if model_id not in self.online_models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        try:
            model_info = self.online_models[model_id]
            model = model_info['model']
            metric = model_info['metrics']
            drift_detector = self.drift_detectors[model_id]

            # Handle batch or single sample
            if isinstance(features, pd.DataFrame):
                samples_trained = 0
                drift_detected = False

                for idx, row in features.iterrows():
                    x_dict = row.to_dict()
                    y = target.iloc[idx] if isinstance(target, pd.Series) else target

                    # Make prediction before update
                    y_pred = model.predict_one(x_dict)

                    # Update model
                    model.learn_one(x_dict, y)

                    # Update metrics
                    if y_pred is not None:
                        metric.update(y, y_pred)

                    # Check for drift
                    error = abs(y - y_pred) if y_pred is not None else 0
                    drift_detector.update(error)
                    if drift_detector.drift_detected:
                        drift_detected = True

                    samples_trained += 1

                model_info['samples_trained'] += samples_trained
                model_info['last_updated'] = datetime.now().isoformat()

                # Save to disk
                self._save_models()

                return {
                    'success': True,
                    'model_id': model_id,
                    'samples_trained': samples_trained,
                    'total_samples': model_info['samples_trained'],
                    'current_mae': float(metric.get()),
                    'drift_detected': drift_detected
                }

            else:  # Single sample
                # Make prediction before update
                y_pred = model.predict_one(features)

                # Update model
                model.learn_one(features, target)

                # Update metrics
                if y_pred is not None:
                    metric.update(target, y_pred)

                # Check for drift
                error = abs(target - y_pred) if y_pred is not None else 0
                drift_detector.update(error)
                drift_detected = drift_detector.drift_detected

                model_info['samples_trained'] += 1
                model_info['last_updated'] = datetime.now().isoformat()

                # Save to disk
                self._save_models()

                # NOT "error": a top-level "error" member marks the whole run as
                # failed in the terminal (PythonRunner), although the sample was learned.
                return {
                    'success': True,
                    'model_id': model_id,
                    'prediction': float(y_pred) if y_pred is not None else None,
                    'actual': float(target),
                    'abs_error': float(error) if y_pred is not None else None,
                    'residual': float(target - y_pred) if y_pred is not None else None,
                    'samples_trained': model_info['samples_trained'],
                    'current_mae': float(metric.get()),
                    'drift_detected': drift_detected
                }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def predict_online(self,
                      model_id: str,
                      features: Union[Dict, pd.DataFrame]) -> Dict[str, Any]:
        """Make online prediction with current model state"""
        if model_id not in self.online_models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        try:
            model = self.online_models[model_id]['model']

            if isinstance(features, pd.DataFrame):
                predictions = []
                for idx, row in features.iterrows():
                    pred = model.predict_one(row.to_dict())
                    predictions.append(float(pred) if pred is not None else None)

                return {
                    'success': True,
                    'model_id': model_id,
                    'predictions': predictions,
                    'count': len(predictions)
                }
            else:
                prediction = model.predict_one(features)
                return {
                    'success': True,
                    'model_id': model_id,
                    'prediction': float(prediction) if prediction is not None else None
                }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def get_model_performance(self, model_id: str) -> Dict[str, Any]:
        """Get current model performance metrics"""
        if model_id not in self.online_models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        try:
            model_info = self.online_models[model_id]
            metric = model_info['metrics']
            drift_detector = self.drift_detectors[model_id]

            return {
                'success': True,
                'model_id': model_id,
                'model_type': model_info['type'],
                'samples_trained': model_info['samples_trained'],
                'current_mae': float(metric.get()),
                'last_updated': model_info['last_updated'],
                'created_at': model_info['created_at'],
                'drift_detected': drift_detector.drift_detected
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def setup_rolling_update(self,
                            model_id: str,
                            update_frequency: str = 'daily',
                            retrain_window: int = 252,
                            min_samples: int = 100) -> Dict[str, Any]:
        """
        Setup rolling update schedule for model

        Args:
            model_id: Model identifier
            update_frequency: 'hourly', 'daily', 'weekly'
            retrain_window: Number of periods to use for retraining
            min_samples: Minimum samples before retraining
        """
        if model_id not in self.online_models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        try:
            self.update_schedule = {
                'model_id': model_id,
                'frequency': update_frequency,
                'retrain_window': retrain_window,
                'min_samples': min_samples,
                'next_update': self._calculate_next_update(update_frequency),
                'enabled': True
            }

            return {
                'success': True,
                'model_id': model_id,
                'update_schedule': self.update_schedule,
                'message': 'Rolling update schedule configured'
            }
        except Exception as e:
            return {'success': False, 'error': str(e)}

    def _calculate_next_update(self, frequency: str) -> str:
        """Calculate next update time"""
        now = datetime.now()
        if frequency == 'hourly':
            next_update = now + timedelta(hours=1)
        elif frequency == 'daily':
            next_update = now + timedelta(days=1)
        elif frequency == 'weekly':
            next_update = now + timedelta(weeks=1)
        else:
            next_update = now + timedelta(days=1)

        return next_update.isoformat()

    def handle_concept_drift(self,
                            model_id: str,
                            strategy: str = 'retrain') -> Dict[str, Any]:
        """
        Handle concept drift detection

        Args:
            model_id: Model identifier
            strategy: 'retrain', 'ensemble', 'adaptive'
        """
        if model_id not in self.online_models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        try:
            if strategy == 'retrain':
                # Reset model to fresh state
                model_type = self.online_models[model_id]['type']
                result = self.create_online_model(model_type, model_id)

                return {
                    'success': True,
                    'model_id': model_id,
                    'strategy': strategy,
                    'action': 'Model reset for retraining',
                    'timestamp': datetime.now().isoformat()
                }

            elif strategy == 'adaptive':
                # Increase learning rate temporarily
                return {
                    'success': True,
                    'model_id': model_id,
                    'strategy': strategy,
                    'action': 'Adaptive learning rate adjustment',
                    'timestamp': datetime.now().isoformat()
                }

            else:
                return {'success': False, 'error': f'Unknown drift handling strategy: {strategy}'}

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def get_all_models(self) -> Dict[str, Any]:
        """Get information about all online models"""
        models_info = []
        legacy = []
        for model_id, model_info in self.online_models.items():
            models_info.append({
                'model_id': model_id,
                'type': model_info['type'],
                'samples_trained': model_info['samples_trained'],
                'created_at': model_info['created_at'],
                'last_updated': model_info['last_updated']
            })
            if (model_info['type'] in ('linear', 'bayesian_linear', 'pa', 'ewa')
                    and model_info.get('pipeline_version', 1) < 2
                    and not _has_scaler(model_info.get('model'))):
                legacy.append(model_id)

        result = {
            'success': True,
            'models': models_info,
            'count': len(models_info),
            'river_available': RIVER_AVAILABLE,
            # The library (qlib with its online module) and, separately, whether there is a
            # local market dataset for it to read.
            'qlib_online_available': _qlib_online_installed(),
        }
        ds = _qlib_dataset()
        result['qlib_dataset_available'] = ds is not None
        result['qlib_dataset'] = ds
        warnings_out = [w for w in (self.load_warning,) if w]
        if legacy:
            warnings_out.append("Linear model(s) created before input scaling was added can diverge to huge "
                                "predictions: " + ", ".join(legacy) + ". Create them again.")
        if warnings_out:
            result['warning'] = " ".join(warnings_out)
        return result


def _has_scaler(model) -> bool:
    """True when the (pickled) model standardises its inputs: a river Pipeline, or an
    ensemble whose members all are."""
    try:
        from river import compose
        if isinstance(model, compose.Pipeline):
            return True
        members = list(getattr(model, 'models', None) or (model if isinstance(model, list) else []))
        return bool(members) and all(isinstance(m, compose.Pipeline) for m in members)
    except Exception:
        return False


def _numeric_features(features) -> Dict[str, float]:
    """The panel's feature dict as {name: float}; raises ValueError on anything else."""
    if not isinstance(features, dict) or not features:
        raise ValueError('features must be a non-empty object of name -> number, e.g. {"close": 95.0, "rsi": 55}')
    out = {}
    for k, v in features.items():
        if isinstance(v, bool) or not isinstance(v, (int, float)):
            raise ValueError(f'feature "{k}" must be a number, got {v!r}')
        if not np.isfinite(v):
            raise ValueError(f'feature "{k}" is not a finite number')
        out[str(k)] = float(v)
    return out


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
    """Main CLI interface"""
    if len(sys.argv) < 2:
        fincept_json.emit({
            'success': False,
            'error': 'Usage: python qlib_online_learning.py <command> [args...]'
        })
        return

    command = sys.argv[1]
    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    except (json.JSONDecodeError, ValueError) as e:
        fincept_json.emit({'success': False, 'error': f'Invalid JSON parameters: {e}'})
        return
    manager = OnlineLearningManager()

    try:
        if command == 'initialize':
            result = manager.initialize(
                params.get('provider_uri'),
                params.get('region'),
                params.get('dataset')
            )

        elif command == 'create_model':
            result = manager.create_online_model(**params)

        elif command == 'train':
            if 'target' not in params:
                raise ValueError('target is required (the realised value to learn from)')
            target = float(params['target'])
            if not np.isfinite(target):
                raise ValueError('target is not a finite number')
            result = manager.incremental_train(
                model_id=params.get('model_id', ''),
                features=_numeric_features(params.get('features', {})),
                target=target
            )

        elif command == 'predict':
            result = manager.predict_online(
                model_id=params.get('model_id', ''),
                features=_numeric_features(params.get('features', {}))
            )

        elif command == 'performance':
            result = manager.get_model_performance(params.get('model_id', ''))

        elif command == 'setup_rolling':
            result = manager.setup_rolling_update(**params)

        elif command == 'handle_drift':
            result = manager.handle_concept_drift(**params)

        elif command == 'list_models':
            result = manager.get_all_models()

        else:
            result = {'success': False, 'error': f'Unknown command: {command}'}

    except Exception as e:
        result = {'success': False, 'error': str(e)}

    fincept_json.emit(_json_safe(result))


if __name__ == '__main__':
    main()
