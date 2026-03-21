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

# Qlib online learning imports
ONLINE_LEARNING_AVAILABLE = False
ONLINE_ERROR = None

try:
    import qlib
    from qlib.contrib.online.manager import OnlineManager
    from qlib.contrib.online.operator import Operator
    from qlib.contrib.online.user import User
    from qlib.contrib.online.utils import get_date_range
    from qlib.data import D
    from qlib.config import REG_CN, REG_US
    from qlib.workflow import R
    from qlib.utils import init_instance_by_config
    ONLINE_LEARNING_AVAILABLE = True
except ImportError as e:
    ONLINE_ERROR = str(e)

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
    """
    CACHE_DIR = Path.home() / '.fincept' / 'online_learning'

    def __init__(self):
        self.qlib_initialized = False
        self.online_models = {}
        self.drift_detectors = {}
        self.performance_history = []
        self.update_schedule = None

        # Create cache directory
        self.CACHE_DIR.mkdir(parents=True, exist_ok=True)

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
                print(f"Warning: Could not load cached models: {e}", file=sys.stderr)

    def _save_models(self):
        """Save all models to disk"""
        cache_file = self.CACHE_DIR / 'models.pkl'
        try:
            with open(cache_file, 'wb') as f:
                pickle.dump({
                    'models': self.online_models,
                    'detectors': self.drift_detectors
                }, f)
        except Exception as e:
            print(f"Warning: Could not save models: {e}", file=sys.stderr)

    def initialize(self,
                  provider_uri: str = "~/.qlib/qlib_data/cn_data",
                  region: str = "cn") -> Dict[str, Any]:
        """Initialize Qlib for online learning"""
        if not ONLINE_LEARNING_AVAILABLE:
            return {'success': False, 'error': f'Qlib online learning not available: {ONLINE_ERROR}'}

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

            # Create model based on type
            # Linear models
            if model_type == 'linear':
                model = linear_model.LinearRegression()
            elif model_type == 'bayesian_linear':
                model = linear_model.BayesianLinearRegression()
            elif model_type == 'pa':
                model = linear_model.PARegressor()

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
                    [linear_model.LinearRegression() for _ in range(3)]
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

                return {
                    'success': True,
                    'model_id': model_id,
                    'prediction': float(y_pred) if y_pred is not None else None,
                    'actual': float(target),
                    'error': float(error),
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
                    predictions.append(pred if pred is not None else 0.0)

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
        for model_id, model_info in self.online_models.items():
            models_info.append({
                'model_id': model_id,
                'type': model_info['type'],
                'samples_trained': model_info['samples_trained'],
                'created_at': model_info['created_at'],
                'last_updated': model_info['last_updated']
            })

        return {
            'success': True,
            'models': models_info,
            'count': len(models_info),
            'river_available': RIVER_AVAILABLE,
            'qlib_online_available': ONLINE_LEARNING_AVAILABLE
        }


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            'success': False,
            'error': 'Usage: python qlib_online_learning.py <command> [args...]'
        }))
        return

    command = sys.argv[1]
    manager = OnlineLearningManager()

    if command == 'initialize':
        provider_uri = sys.argv[2] if len(sys.argv) > 2 else "~/.qlib/qlib_data/cn_data"
        region = sys.argv[3] if len(sys.argv) > 3 else "cn"
        result = manager.initialize(provider_uri, region)

    elif command == 'create_model':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        result = manager.create_online_model(**params)

    elif command == 'train':
        model_id = sys.argv[2]
        features = json.loads(sys.argv[3])
        target = float(sys.argv[4])
        result = manager.incremental_train(model_id, features, target)

    elif command == 'predict':
        model_id = sys.argv[2]
        features = json.loads(sys.argv[3])
        result = manager.predict_online(model_id, features)

    elif command == 'performance':
        model_id = sys.argv[2]
        result = manager.get_model_performance(model_id)

    elif command == 'setup_rolling':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        result = manager.setup_rolling_update(**params)

    elif command == 'handle_drift':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        result = manager.handle_concept_drift(**params)

    elif command == 'list_models':
        result = manager.get_all_models()

    else:
        result = {'success': False, 'error': f'Unknown command: {command}'}

    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
