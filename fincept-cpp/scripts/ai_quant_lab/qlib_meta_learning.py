"""
AI Quant Lab - Meta Learning Module
AutoML, Model Selection, Hyperparameter Tuning, and Ensemble Methods
Supports multiple ML frameworks: scikit-learn, LightGBM, XGBoost, CatBoost
"""

import json
import sys
import os
import pickle
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Optional, Union
import warnings
warnings.filterwarnings('ignore')

import numpy as np
import pandas as pd

# Check available ML libraries
SKLEARN_AVAILABLE = False
LIGHTGBM_AVAILABLE = False
XGBOOST_AVAILABLE = False
CATBOOST_AVAILABLE = False
QLIB_AVAILABLE = False

try:
    from sklearn.model_selection import train_test_split, GridSearchCV, RandomizedSearchCV
    from sklearn.metrics import accuracy_score, f1_score, roc_auc_score, mean_squared_error, r2_score
    from sklearn.ensemble import RandomForestClassifier, RandomForestRegressor
    from sklearn.ensemble import VotingClassifier, VotingRegressor, StackingClassifier, StackingRegressor
    SKLEARN_AVAILABLE = True
except ImportError:
    pass

try:
    import lightgbm as lgb
    LIGHTGBM_AVAILABLE = True
except ImportError:
    pass

try:
    import xgboost as xgb
    XGBOOST_AVAILABLE = True
except ImportError:
    pass

try:
    import catboost as cb
    CATBOOST_AVAILABLE = True
except ImportError:
    pass

try:
    import qlib
    from qlib.contrib.meta.data_selection import ICDatasetSelector
    from qlib.utils import init_instance_by_config
    QLIB_AVAILABLE = True
except ImportError:
    pass


class MetaLearningManager:
    """
    Meta Learning Manager for AutoML and Model Selection
    Handles model comparison, hyperparameter tuning, and ensemble creation
    """
    CACHE_DIR = Path.home() / '.fincept' / 'meta_learning'

    def __init__(self):
        self.trained_models = {}
        self.evaluation_results = {}
        self.best_model_id = None
        self.ensemble = None

        # Create cache directory
        self.CACHE_DIR.mkdir(parents=True, exist_ok=True)

        # Load existing results
        self._load_state()

    def _load_state(self):
        """Load saved models and results from disk"""
        cache_file = self.CACHE_DIR / 'meta_state.pkl'
        if cache_file.exists():
            try:
                with open(cache_file, 'rb') as f:
                    data = pickle.load(f)
                    self.trained_models = data.get('models', {})
                    self.evaluation_results = data.get('results', {})
                    self.best_model_id = data.get('best_model', None)
            except Exception as e:
                print(f"Warning: Could not load cached state: {e}", file=sys.stderr)

    def _save_state(self):
        """Save models and results to disk"""
        cache_file = self.CACHE_DIR / 'meta_state.pkl'
        try:
            with open(cache_file, 'wb') as f:
                pickle.dump({
                    'models': self.trained_models,
                    'results': self.evaluation_results,
                    'best_model': self.best_model_id
                }, f)
        except Exception as e:
            print(f"Warning: Could not save state: {e}", file=sys.stderr)

    def get_available_models(self) -> Dict[str, Any]:
        """Get list of available model types"""
        models = []

        if SKLEARN_AVAILABLE:
            models.append({
                'id': 'random_forest',
                'name': 'Random Forest',
                'type': 'ensemble',
                'library': 'scikit-learn',
                'description': 'Ensemble of decision trees with bagging'
            })

        if LIGHTGBM_AVAILABLE:
            models.append({
                'id': 'lightgbm',
                'name': 'LightGBM',
                'type': 'gradient_boosting',
                'library': 'lightgbm',
                'description': 'Fast gradient boosting framework by Microsoft'
            })

        if XGBOOST_AVAILABLE:
            models.append({
                'id': 'xgboost',
                'name': 'XGBoost',
                'type': 'gradient_boosting',
                'library': 'xgboost',
                'description': 'Extreme gradient boosting'
            })

        if CATBOOST_AVAILABLE:
            models.append({
                'id': 'catboost',
                'name': 'CatBoost',
                'type': 'gradient_boosting',
                'library': 'catboost',
                'description': 'Gradient boosting with categorical features support'
            })

        return {
            'success': True,
            'models': models,
            'count': len(models),
            'sklearn_available': SKLEARN_AVAILABLE,
            'lightgbm_available': LIGHTGBM_AVAILABLE,
            'xgboost_available': XGBOOST_AVAILABLE,
            'catboost_available': CATBOOST_AVAILABLE,
            'qlib_available': QLIB_AVAILABLE
        }

    def run_model_selection(self,
                           model_ids: List[str],
                           X_train: Optional[np.ndarray] = None,
                           y_train: Optional[np.ndarray] = None,
                           X_test: Optional[np.ndarray] = None,
                           y_test: Optional[np.ndarray] = None,
                           task_type: str = 'classification',
                           metric: str = 'auto') -> Dict[str, Any]:
        """
        Run model selection by training and evaluating multiple models

        Args:
            model_ids: List of model IDs to evaluate
            X_train, y_train, X_test, y_test: Training/test data
            task_type: 'classification' or 'regression'
            metric: Evaluation metric ('auto', 'accuracy', 'f1', 'auc', 'mse', 'r2')
        """
        if not model_ids:
            return {'success': False, 'error': 'No models specified'}

        # Generate synthetic data if not provided (for demo/testing)
        if X_train is None:
            n_samples = 1000
            n_features = 20
            X_train = np.random.randn(n_samples, n_features)
            if task_type == 'classification':
                y_train = np.random.randint(0, 2, n_samples)
            else:
                y_train = np.random.randn(n_samples)

            X_test = np.random.randn(200, n_features)
            if task_type == 'classification':
                y_test = np.random.randint(0, 2, 200)
            else:
                y_test = np.random.randn(200)

        results = {}
        trained_count = 0

        for model_id in model_ids:
            try:
                # Create model
                model = self._create_model(model_id, task_type)
                if model is None:
                    results[model_id] = {
                        'error': f'Model {model_id} not available',
                        'trained': False
                    }
                    continue

                # Train model
                model.fit(X_train, y_train)

                # Evaluate
                y_pred = model.predict(X_test)

                metrics = {}
                if task_type == 'classification':
                    metrics['accuracy'] = float(accuracy_score(y_test, y_pred))
                    if SKLEARN_AVAILABLE:
                        metrics['f1_score'] = float(f1_score(y_test, y_pred, average='weighted'))
                        try:
                            y_pred_proba = model.predict_proba(X_test)
                            if y_pred_proba.shape[1] == 2:
                                metrics['auc_roc'] = float(roc_auc_score(y_test, y_pred_proba[:, 1]))
                            else:
                                metrics['auc_roc'] = float(roc_auc_score(y_test, y_pred_proba, multi_class='ovr'))
                        except:
                            pass
                else:
                    metrics['mse'] = float(mean_squared_error(y_test, y_pred))
                    metrics['rmse'] = float(np.sqrt(metrics['mse']))
                    metrics['r2_score'] = float(r2_score(y_test, y_pred))

                # Store model and results
                model_key = f"{model_id}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
                self.trained_models[model_key] = model
                self.evaluation_results[model_key] = {
                    'model_id': model_id,
                    'task_type': task_type,
                    'metrics': metrics,
                    'trained_at': datetime.now().isoformat(),
                    'n_train_samples': len(X_train),
                    'n_test_samples': len(X_test)
                }

                results[model_id] = {
                    'model_key': model_key,
                    'metrics': metrics,
                    'trained': True
                }
                trained_count += 1

            except Exception as e:
                results[model_id] = {
                    'error': str(e),
                    'trained': False
                }

        # Determine best model
        if trained_count > 0:
            self._update_best_model(metric, task_type)

        # Save state
        self._save_state()

        # Prepare ranking
        ranking = []
        for model_id, result in results.items():
            if result.get('trained'):
                metrics = result['metrics']
                if task_type == 'classification':
                    score = metrics.get('accuracy', metrics.get('f1_score', 0))
                else:
                    score = metrics.get('r2_score', -metrics.get('mse', float('inf')))

                ranking.append({
                    'model_id': model_id,
                    'model_key': result['model_key'],
                    'score': score,
                    'metrics': metrics
                })

        ranking.sort(key=lambda x: x['score'], reverse=True)

        return {
            'success': True,
            'results': results,
            'ranking': ranking,
            'best_model': ranking[0]['model_id'] if ranking else None,
            'trained_count': trained_count,
            'task_type': task_type
        }

    def _create_model(self, model_id: str, task_type: str):
        """Create model instance based on ID and task type"""
        if model_id == 'random_forest' and SKLEARN_AVAILABLE:
            if task_type == 'classification':
                return RandomForestClassifier(n_estimators=100, random_state=42, n_jobs=-1)
            else:
                return RandomForestRegressor(n_estimators=100, random_state=42, n_jobs=-1)

        elif model_id == 'lightgbm' and LIGHTGBM_AVAILABLE:
            if task_type == 'classification':
                return lgb.LGBMClassifier(n_estimators=100, random_state=42, verbose=-1)
            else:
                return lgb.LGBMRegressor(n_estimators=100, random_state=42, verbose=-1)

        elif model_id == 'xgboost' and XGBOOST_AVAILABLE:
            if task_type == 'classification':
                return xgb.XGBClassifier(n_estimators=100, random_state=42, verbosity=0)
            else:
                return xgb.XGBRegressor(n_estimators=100, random_state=42, verbosity=0)

        elif model_id == 'catboost' and CATBOOST_AVAILABLE:
            if task_type == 'classification':
                return cb.CatBoostClassifier(iterations=100, random_state=42, verbose=False)
            else:
                return cb.CatBoostRegressor(iterations=100, random_state=42, verbose=False)

        return None

    def _update_best_model(self, metric: str, task_type: str):
        """Update best model based on metric"""
        if not self.evaluation_results:
            return

        best_score = float('-inf')
        best_key = None

        for model_key, result in self.evaluation_results.items():
            metrics = result['metrics']

            if metric == 'auto':
                if task_type == 'classification':
                    score = metrics.get('accuracy', metrics.get('f1_score', 0))
                else:
                    score = metrics.get('r2_score', 0)
            else:
                score = metrics.get(metric, 0)

            # For MSE/RMSE, lower is better
            if metric in ['mse', 'rmse']:
                score = -score

            if score > best_score:
                best_score = score
                best_key = model_key

        self.best_model_id = best_key

    def create_ensemble(self,
                       model_keys: List[str],
                       ensemble_method: str = 'voting',
                       task_type: str = 'classification') -> Dict[str, Any]:
        """
        Create ensemble from trained models

        Args:
            model_keys: List of trained model keys to ensemble
            ensemble_method: 'voting' or 'stacking'
            task_type: 'classification' or 'regression'
        """
        if not SKLEARN_AVAILABLE:
            return {'success': False, 'error': 'scikit-learn not available'}

        if not model_keys:
            return {'success': False, 'error': 'No models specified'}

        # Get models
        models = []
        for key in model_keys:
            if key in self.trained_models:
                models.append((key, self.trained_models[key]))

        if len(models) < 2:
            return {'success': False, 'error': 'Need at least 2 trained models for ensemble'}

        try:
            if ensemble_method == 'voting':
                if task_type == 'classification':
                    self.ensemble = VotingClassifier(estimators=models, voting='soft')
                else:
                    self.ensemble = VotingRegressor(estimators=models)
            elif ensemble_method == 'stacking':
                if task_type == 'classification':
                    self.ensemble = StackingClassifier(estimators=models)
                else:
                    self.ensemble = StackingRegressor(estimators=models)
            else:
                return {'success': False, 'error': f'Unknown ensemble method: {ensemble_method}'}

            ensemble_id = f"ensemble_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

            return {
                'success': True,
                'ensemble_id': ensemble_id,
                'method': ensemble_method,
                'n_models': len(models),
                'model_keys': model_keys
            }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def hyperparameter_tuning(self,
                             model_id: str,
                             param_grid: Dict,
                             X_train: Optional[np.ndarray] = None,
                             y_train: Optional[np.ndarray] = None,
                             task_type: str = 'classification',
                             search_method: str = 'grid',
                             cv: int = 5) -> Dict[str, Any]:
        """
        Perform hyperparameter tuning

        Args:
            model_id: Model to tune
            param_grid: Parameter grid
            search_method: 'grid' or 'random'
            cv: Cross-validation folds
        """
        if not SKLEARN_AVAILABLE:
            return {'success': False, 'error': 'scikit-learn not available'}

        # Generate synthetic data if not provided
        if X_train is None:
            n_samples = 1000
            n_features = 20
            X_train = np.random.randn(n_samples, n_features)
            if task_type == 'classification':
                y_train = np.random.randint(0, 2, n_samples)
            else:
                y_train = np.random.randn(n_samples)

        try:
            # Create base model
            model = self._create_model(model_id, task_type)
            if model is None:
                return {'success': False, 'error': f'Model {model_id} not available'}

            # Choose search method
            if search_method == 'grid':
                search = GridSearchCV(model, param_grid, cv=cv, n_jobs=-1)
            else:
                search = RandomizedSearchCV(model, param_grid, n_iter=10, cv=cv, n_jobs=-1)

            # Fit
            search.fit(X_train, y_train)

            return {
                'success': True,
                'model_id': model_id,
                'best_params': search.best_params_,
                'best_score': float(search.best_score_),
                'search_method': search_method,
                'cv_folds': cv
            }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def get_all_results(self) -> Dict[str, Any]:
        """Get all evaluation results"""
        return {
            'success': True,
            'results': self.evaluation_results,
            'best_model': self.best_model_id,
            'n_models': len(self.trained_models)
        }


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            'success': False,
            'error': 'Usage: python qlib_meta_learning.py <command> [args...]'
        }))
        return

    command = sys.argv[1]
    manager = MetaLearningManager()

    if command == 'list_models':
        result = manager.get_available_models()

    elif command == 'run_selection':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        model_ids = params.get('model_ids', [])
        task_type = params.get('task_type', 'classification')
        metric = params.get('metric', 'auto')
        result = manager.run_model_selection(model_ids, task_type=task_type, metric=metric)

    elif command == 'create_ensemble':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        model_keys = params.get('model_keys', [])
        ensemble_method = params.get('method', 'voting')
        task_type = params.get('task_type', 'classification')
        result = manager.create_ensemble(model_keys, ensemble_method, task_type)

    elif command == 'tune_hyperparameters':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        model_id = params.get('model_id', '')
        param_grid = params.get('param_grid', {})
        task_type = params.get('task_type', 'classification')
        search_method = params.get('search_method', 'grid')
        result = manager.hyperparameter_tuning(model_id, param_grid, task_type=task_type, search_method=search_method)

    elif command == 'get_results':
        result = manager.get_all_results()

    else:
        result = {'success': False, 'error': f'Unknown command: {command}'}

    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
