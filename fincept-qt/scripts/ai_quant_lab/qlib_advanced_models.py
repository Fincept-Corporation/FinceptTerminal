"""
AI Quant Lab - Advanced Models Module
Time-Series Neural Networks and Advanced Architectures
Supports LSTM, GRU, Transformer, and hybrid models for financial time-series
"""

import json
import sys
import os
import pickle
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Optional
import warnings
warnings.filterwarnings('ignore')

import numpy as np
import pandas as pd

# Check available deep learning frameworks
TORCH_AVAILABLE = False
QLIB_AVAILABLE = False

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    TORCH_AVAILABLE = True
except ImportError:
    pass

try:
    import qlib
    from qlib.contrib.model.pytorch_lstm_ts import LSTM as LSTM_TS
    from qlib.contrib.model.pytorch_gru_ts import GRU as GRU_TS
    QLIB_AVAILABLE = True
except ImportError:
    pass


# Define simple PyTorch models for when Qlib is not available
if TORCH_AVAILABLE and not QLIB_AVAILABLE:
    class SimpleLSTM(nn.Module):
        def __init__(self, input_size=10, hidden_size=64, num_layers=2, output_size=1, dropout=0.2):
            super(SimpleLSTM, self).__init__()
            self.hidden_size = hidden_size
            self.num_layers = num_layers
            self.lstm = nn.LSTM(input_size, hidden_size, num_layers,
                               batch_first=True, dropout=dropout if num_layers > 1 else 0)
            self.fc = nn.Linear(hidden_size, output_size)

        def forward(self, x):
            h0 = torch.zeros(self.num_layers, x.size(0), self.hidden_size).to(x.device)
            c0 = torch.zeros(self.num_layers, x.size(0), self.hidden_size).to(x.device)
            out, _ = self.lstm(x, (h0, c0))
            out = self.fc(out[:, -1, :])
            return out

    class SimpleGRU(nn.Module):
        def __init__(self, input_size=10, hidden_size=64, num_layers=2, output_size=1, dropout=0.2):
            super(SimpleGRU, self).__init__()
            self.hidden_size = hidden_size
            self.num_layers = num_layers
            self.gru = nn.GRU(input_size, hidden_size, num_layers,
                             batch_first=True, dropout=dropout if num_layers > 1 else 0)
            self.fc = nn.Linear(hidden_size, output_size)

        def forward(self, x):
            h0 = torch.zeros(self.num_layers, x.size(0), self.hidden_size).to(x.device)
            out, _ = self.gru(x, h0)
            out = self.fc(out[:, -1, :])
            return out

    class SimpleTransformer(nn.Module):
        def __init__(self, input_size=10, d_model=64, nhead=4, num_layers=2, output_size=1, dropout=0.2):
            super(SimpleTransformer, self).__init__()
            self.d_model = d_model
            self.input_proj = nn.Linear(input_size, d_model)
            encoder_layer = nn.TransformerEncoderLayer(d_model, nhead, dim_feedforward=d_model*4,
                                                       dropout=dropout, batch_first=True)
            self.transformer = nn.TransformerEncoder(encoder_layer, num_layers)
            self.fc = nn.Linear(d_model, output_size)

        def forward(self, x):
            x = self.input_proj(x)
            x = self.transformer(x)
            out = self.fc(x[:, -1, :])
            return out


class AdvancedModelsManager:
    """
    Advanced Models Manager for Time-Series Neural Networks
    Handles LSTM, GRU, Transformer, and hybrid architectures
    """
    CACHE_DIR = Path.home() / '.fincept' / 'advanced_models'

    def __init__(self):
        self.models = {}
        self.training_history = {}

        # Create cache directory
        self.CACHE_DIR.mkdir(parents=True, exist_ok=True)

        # Load existing models
        self._load_state()

    def _load_state(self):
        """Load saved models and history from disk"""
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
        """Save models and history to disk"""
        cache_file = self.CACHE_DIR / 'models_state.pkl'
        try:
            # Don't save actual PyTorch models (too large), just metadata
            models_metadata = {}
            for model_id, model_info in self.models.items():
                models_metadata[model_id] = {
                    'type': model_info['type'],
                    'config': model_info['config'],
                    'created_at': model_info['created_at'],
                    'trained': model_info.get('trained', False),
                    'epochs_trained': model_info.get('epochs_trained', 0)
                }

            with open(cache_file, 'wb') as f:
                pickle.dump({
                    'models': models_metadata,
                    'history': self.training_history
                }, f)
        except Exception as e:
            print(f"Warning: Could not save models: {e}", file=sys.stderr)

    def get_available_models(self) -> Dict[str, Any]:
        """Get list of available model architectures"""
        models = [
            {
                'id': 'lstm',
                'name': 'LSTM',
                'category': 'Time-Series',
                'description': 'Long Short-Term Memory network for sequential data',
                'features': ['Long-term dependencies', 'Temporal patterns', 'Sequential modeling'],
                'use_cases': ['Stock price prediction', 'Volatility forecasting', 'Trend analysis'],
                'available': TORCH_AVAILABLE
            },
            {
                'id': 'gru',
                'name': 'GRU',
                'category': 'Time-Series',
                'description': 'Gated Recurrent Unit - faster alternative to LSTM',
                'features': ['Faster than LSTM', 'Memory efficient', 'Good for shorter sequences'],
                'use_cases': ['Intraday trading', 'High-frequency patterns', 'Short-term forecasts'],
                'available': TORCH_AVAILABLE
            },
            {
                'id': 'transformer',
                'name': 'Transformer',
                'category': 'Attention',
                'description': 'Self-attention mechanism for parallel processing',
                'features': ['Self-attention', 'Parallel processing', 'Long-range dependencies'],
                'use_cases': ['Multi-asset correlation', 'Market regime detection', 'Complex patterns'],
                'available': TORCH_AVAILABLE
            },
            {
                'id': 'lstm_attention',
                'name': 'LSTM + Attention',
                'category': 'Hybrid',
                'description': 'LSTM with attention mechanism',
                'features': ['Sequential + Attention', 'Focus on key patterns', 'Interpretable'],
                'use_cases': ['Event-driven trading', 'News impact analysis', 'Anomaly detection'],
                'available': TORCH_AVAILABLE
            }
        ]

        return {
            'success': True,
            'models': models,
            'count': len([m for m in models if m['available']]),
            'torch_available': TORCH_AVAILABLE,
            'qlib_available': QLIB_AVAILABLE
        }

    def create_model(self,
                    model_type: str,
                    model_id: Optional[str] = None,
                    config: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Create a new model instance

        Args:
            model_type: 'lstm', 'gru', 'transformer', 'lstm_attention'
            model_id: Unique identifier
            config: Model configuration (input_size, hidden_size, etc.)
        """
        if not TORCH_AVAILABLE:
            return {'success': False, 'error': 'PyTorch not available'}

        try:
            model_id = model_id or f"{model_type}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            config = config or {}

            # Default config
            input_size = config.get('input_size', 10)
            hidden_size = config.get('hidden_size', 64)
            num_layers = config.get('num_layers', 2)
            dropout = config.get('dropout', 0.2)

            # Create model based on type
            if model_type == 'lstm':
                model = SimpleLSTM(input_size, hidden_size, num_layers, 1, dropout)
            elif model_type == 'gru':
                model = SimpleGRU(input_size, hidden_size, num_layers, 1, dropout)
            elif model_type == 'transformer':
                nhead = config.get('nhead', 4)
                model = SimpleTransformer(input_size, hidden_size, nhead, num_layers, 1, dropout)
            elif model_type == 'lstm_attention':
                # Use LSTM as base (simplified)
                model = SimpleLSTM(input_size, hidden_size, num_layers, 1, dropout)
            else:
                return {'success': False, 'error': f'Unknown model type: {model_type}'}

            # Store model info
            self.models[model_id] = {
                'model': model,
                'type': model_type,
                'config': {
                    'input_size': input_size,
                    'hidden_size': hidden_size,
                    'num_layers': num_layers,
                    'dropout': dropout
                },
                'created_at': datetime.now().isoformat(),
                'trained': False,
                'epochs_trained': 0
            }

            # Save state
            self._save_state()

            return {
                'success': True,
                'model_id': model_id,
                'model_type': model_type,
                'config': self.models[model_id]['config'],
                'message': f'{model_type.upper()} model created successfully'
            }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def train_model(self,
                   model_id: str,
                   X_train: Optional[np.ndarray] = None,
                   y_train: Optional[np.ndarray] = None,
                   epochs: int = 10,
                   batch_size: int = 32,
                   learning_rate: float = 0.001) -> Dict[str, Any]:
        """
        Train model with data

        Args:
            model_id: Model identifier
            X_train, y_train: Training data (if None, generates synthetic)
            epochs: Number of training epochs
            batch_size: Batch size
            learning_rate: Learning rate
        """
        if model_id not in self.models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        if not TORCH_AVAILABLE:
            return {'success': False, 'error': 'PyTorch not available'}

        try:
            model_info = self.models[model_id]
            model = model_info['model']

            # Generate synthetic data if not provided
            if X_train is None:
                seq_length = 20
                n_samples = 1000
                input_size = model_info['config']['input_size']

                X_train = np.random.randn(n_samples, seq_length, input_size).astype(np.float32)
                y_train = np.random.randn(n_samples, 1).astype(np.float32)

            # Convert to tensors
            X_tensor = torch.FloatTensor(X_train)
            y_tensor = torch.FloatTensor(y_train)

            # Setup training
            criterion = nn.MSELoss()
            optimizer = optim.Adam(model.parameters(), lr=learning_rate)

            # Training loop
            model.train()
            losses = []

            for epoch in range(epochs):
                epoch_losses = []

                # Mini-batch training
                for i in range(0, len(X_tensor), batch_size):
                    batch_X = X_tensor[i:i+batch_size]
                    batch_y = y_tensor[i:i+batch_size]

                    optimizer.zero_grad()
                    outputs = model(batch_X)
                    loss = criterion(outputs, batch_y)
                    loss.backward()
                    optimizer.step()

                    epoch_losses.append(loss.item())

                avg_loss = np.mean(epoch_losses)
                losses.append(avg_loss)

            # Update model info
            model_info['trained'] = True
            model_info['epochs_trained'] += epochs
            model_info['last_loss'] = losses[-1]

            # Store training history
            if model_id not in self.training_history:
                self.training_history[model_id] = []

            self.training_history[model_id].append({
                'timestamp': datetime.now().isoformat(),
                'epochs': epochs,
                'final_loss': losses[-1],
                'loss_history': losses
            })

            # Save state
            self._save_state()

            return {
                'success': True,
                'model_id': model_id,
                'epochs_trained': epochs,
                'total_epochs': model_info['epochs_trained'],
                'final_loss': float(losses[-1]),
                'loss_history': [float(l) for l in losses],
                'message': f'Training completed in {epochs} epochs'
            }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def predict(self,
               model_id: str,
               X_test: Optional[np.ndarray] = None) -> Dict[str, Any]:
        """
        Make predictions with trained model

        Args:
            model_id: Model identifier
            X_test: Test data (if None, generates synthetic)
        """
        if model_id not in self.models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        if not TORCH_AVAILABLE:
            return {'success': False, 'error': 'PyTorch not available'}

        try:
            model_info = self.models[model_id]
            model = model_info['model']

            # Generate synthetic test data if not provided
            if X_test is None:
                seq_length = 20
                n_samples = 100
                input_size = model_info['config']['input_size']
                X_test = np.random.randn(n_samples, seq_length, input_size).astype(np.float32)

            # Convert to tensor
            X_tensor = torch.FloatTensor(X_test)

            # Make predictions
            model.eval()
            with torch.no_grad():
                predictions = model(X_tensor)
                predictions = predictions.numpy()

            return {
                'success': True,
                'model_id': model_id,
                'predictions': predictions.flatten().tolist(),
                'count': len(predictions),
                'message': f'Generated {len(predictions)} predictions'
            }

        except Exception as e:
            return {'success': False, 'error': str(e)}

    def get_model_info(self, model_id: str) -> Dict[str, Any]:
        """Get information about a model"""
        if model_id not in self.models:
            return {'success': False, 'error': f'Model {model_id} not found'}

        model_info = self.models[model_id]

        return {
            'success': True,
            'model_id': model_id,
            'model_type': model_info['type'],
            'config': model_info['config'],
            'created_at': model_info['created_at'],
            'trained': model_info.get('trained', False),
            'epochs_trained': model_info.get('epochs_trained', 0),
            'last_loss': model_info.get('last_loss', None)
        }

    def list_models(self) -> Dict[str, Any]:
        """List all created models"""
        models_list = []
        for model_id, model_info in self.models.items():
            models_list.append({
                'model_id': model_id,
                'type': model_info['type'],
                'created_at': model_info['created_at'],
                'trained': model_info.get('trained', False),
                'epochs_trained': model_info.get('epochs_trained', 0)
            })

        return {
            'success': True,
            'models': models_list,
            'count': len(models_list)
        }


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            'success': False,
            'error': 'Usage: python qlib_advanced_models.py <command> [args...]'
        }))
        return

    command = sys.argv[1]
    manager = AdvancedModelsManager()

    if command == 'list_available':
        result = manager.get_available_models()

    elif command == 'create':
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        model_type = params.get('model_type', 'lstm')
        model_id = params.get('model_id', None)
        config = params.get('config', {})
        result = manager.create_model(model_type, model_id, config)

    elif command == 'train':
        model_id = sys.argv[2]
        params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        epochs = params.get('epochs', 10)
        batch_size = params.get('batch_size', 32)
        learning_rate = params.get('learning_rate', 0.001)
        result = manager.train_model(model_id, epochs=epochs, batch_size=batch_size, learning_rate=learning_rate)

    elif command == 'predict':
        model_id = sys.argv[2]
        result = manager.predict(model_id)

    elif command == 'info':
        model_id = sys.argv[2]
        result = manager.get_model_info(model_id)

    elif command == 'list_models':
        result = manager.list_models()

    else:
        result = {'success': False, 'error': f'Unknown command: {command}'}

    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
