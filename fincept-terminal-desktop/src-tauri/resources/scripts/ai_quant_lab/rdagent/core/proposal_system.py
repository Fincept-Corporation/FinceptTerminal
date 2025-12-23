"""
RD-Agent Advanced Proposal System
Intelligent experiment selection and checkpoint management

Features:
- CheckpointSelector: Select best checkpoints from experiments
- SOTASelector: Select state-of-the-art experiments
- ExperimentRanker: Rank experiments by multiple criteria
- AdaptiveProposer: Adaptive proposal generation
"""

import json
import sys
from typing import Dict, List, Any, Optional, Tuple
from datetime import datetime
import warnings
warnings.filterwarnings('ignore')

try:
    import numpy as np
    import pandas as pd
    NUMPY_AVAILABLE = True
except ImportError:
    NUMPY_AVAILABLE = False
    np = None
    pd = None


class CheckpointSelector:
    """
    Intelligent checkpoint selection system.

    Selects best model checkpoints based on multiple criteria.
    """

    def __init__(self):
        self.checkpoints = {}

    def add_checkpoint(self,
                      checkpoint_id: str,
                      metrics: Dict[str, float],
                      metadata: Optional[Dict[str, Any]] = None):
        """
        Add a checkpoint to the selector.

        Args:
            checkpoint_id: Unique checkpoint identifier
            metrics: Performance metrics (ic, sharpe, etc.)
            metadata: Additional metadata
        """
        self.checkpoints[checkpoint_id] = {
            "id": checkpoint_id,
            "metrics": metrics,
            "metadata": metadata or {},
            "timestamp": datetime.now().isoformat()
        }

    def select_best(self,
                   metric: str = "sharpe",
                   top_k: int = 1,
                   filters: Optional[Dict[str, Any]] = None) -> List[Dict[str, Any]]:
        """
        Select best checkpoints by metric.

        Args:
            metric: Metric to optimize ('sharpe', 'ic', 'win_rate', etc.)
            top_k: Number of checkpoints to return
            filters: Optional filters (e.g., {'min_ic': 0.03})

        Returns:
            List of top-k checkpoints
        """
        if not self.checkpoints:
            return []

        # Apply filters
        filtered = self.checkpoints.copy()

        if filters:
            for key, value in filters.items():
                if key.startswith('min_'):
                    metric_name = key[4:]
                    filtered = {
                        k: v for k, v in filtered.items()
                        if v['metrics'].get(metric_name, float('-inf')) >= value
                    }
                elif key.startswith('max_'):
                    metric_name = key[4:]
                    filtered = {
                        k: v for k, v in filtered.items()
                        if v['metrics'].get(metric_name, float('inf')) <= value
                    }

        # Sort by metric
        sorted_checkpoints = sorted(
            filtered.values(),
            key=lambda x: x['metrics'].get(metric, float('-inf')),
            reverse=True
        )

        return sorted_checkpoints[:top_k]

    def select_pareto_optimal(self,
                             metrics: List[str] = ['sharpe', 'ic']) -> List[Dict[str, Any]]:
        """
        Select Pareto-optimal checkpoints.

        A checkpoint is Pareto-optimal if no other checkpoint
        is better in all metrics.

        Args:
            metrics: List of metrics to consider

        Returns:
            Pareto-optimal checkpoints
        """
        if not NUMPY_AVAILABLE or not self.checkpoints:
            return []

        # Extract metric values
        checkpoint_list = list(self.checkpoints.values())
        metric_matrix = np.array([
            [cp['metrics'].get(m, 0) for m in metrics]
            for cp in checkpoint_list
        ])

        # Find Pareto front
        is_pareto = np.ones(len(checkpoint_list), dtype=bool)

        for i in range(len(checkpoint_list)):
            # Check if any other point dominates this point
            for j in range(len(checkpoint_list)):
                if i != j:
                    if np.all(metric_matrix[j] >= metric_matrix[i]) and \
                       np.any(metric_matrix[j] > metric_matrix[i]):
                        is_pareto[i] = False
                        break

        pareto_checkpoints = [
            checkpoint_list[i] for i in range(len(checkpoint_list))
            if is_pareto[i]
        ]

        return pareto_checkpoints

    def select_diverse(self,
                      num_checkpoints: int = 5,
                      diversity_metric: str = 'euclidean') -> List[Dict[str, Any]]:
        """
        Select diverse checkpoints to ensure exploration.

        Args:
            num_checkpoints: Number of checkpoints to select
            diversity_metric: Distance metric ('euclidean', 'manhattan')

        Returns:
            Diverse set of checkpoints
        """
        if not NUMPY_AVAILABLE or len(self.checkpoints) <= num_checkpoints:
            return list(self.checkpoints.values())

        checkpoint_list = list(self.checkpoints.values())

        # Extract all metrics as feature vector
        all_metrics = set()
        for cp in checkpoint_list:
            all_metrics.update(cp['metrics'].keys())

        feature_matrix = np.array([
            [cp['metrics'].get(m, 0) for m in all_metrics]
            for cp in checkpoint_list
        ])

        # Normalize
        feature_matrix = (feature_matrix - feature_matrix.mean(axis=0)) / (feature_matrix.std(axis=0) + 1e-8)

        # Greedy diversity selection
        selected_indices = []

        # Start with best checkpoint
        best_idx = np.argmax([cp['metrics'].get('sharpe', 0) for cp in checkpoint_list])
        selected_indices.append(best_idx)

        # Iteratively add most diverse checkpoint
        for _ in range(num_checkpoints - 1):
            max_min_distance = -1
            best_candidate = -1

            for i in range(len(checkpoint_list)):
                if i in selected_indices:
                    continue

                # Calculate min distance to selected points
                distances = [
                    np.linalg.norm(feature_matrix[i] - feature_matrix[j])
                    for j in selected_indices
                ]
                min_distance = min(distances)

                if min_distance > max_min_distance:
                    max_min_distance = min_distance
                    best_candidate = i

            if best_candidate != -1:
                selected_indices.append(best_candidate)

        return [checkpoint_list[i] for i in selected_indices]


class SOTASelector:
    """
    State-of-the-Art experiment selector.

    Identifies and tracks SOTA experiments.
    """

    def __init__(self):
        self.experiments = {}
        self.sota_history = []

    def add_experiment(self,
                      experiment_id: str,
                      results: Dict[str, float],
                      config: Optional[Dict[str, Any]] = None):
        """
        Add experiment results.

        Args:
            experiment_id: Unique experiment ID
            results: Performance results
            config: Experiment configuration
        """
        self.experiments[experiment_id] = {
            "id": experiment_id,
            "results": results,
            "config": config or {},
            "timestamp": datetime.now().isoformat()
        }

        # Check if this is new SOTA
        self._update_sota(experiment_id, results)

    def _update_sota(self, experiment_id: str, results: Dict[str, float]):
        """Update SOTA tracking"""
        # Check each metric
        for metric, value in results.items():
            # Is this the best for this metric?
            current_sota = self._get_current_sota(metric)

            if current_sota is None or value > current_sota['value']:
                self.sota_history.append({
                    "metric": metric,
                    "value": value,
                    "experiment_id": experiment_id,
                    "timestamp": datetime.now().isoformat()
                })

    def _get_current_sota(self, metric: str) -> Optional[Dict[str, Any]]:
        """Get current SOTA for a metric"""
        sota_for_metric = [
            entry for entry in self.sota_history
            if entry['metric'] == metric
        ]

        if not sota_for_metric:
            return None

        return max(sota_for_metric, key=lambda x: x['value'])

    def get_sota_experiments(self,
                            metrics: Optional[List[str]] = None) -> Dict[str, Dict[str, Any]]:
        """
        Get SOTA experiments for each metric.

        Args:
            metrics: List of metrics to check (None = all)

        Returns:
            Dict mapping metric -> SOTA experiment
        """
        if metrics is None:
            # Get all metrics
            all_metrics = set()
            for exp in self.experiments.values():
                all_metrics.update(exp['results'].keys())
            metrics = list(all_metrics)

        sota_map = {}
        for metric in metrics:
            sota = self._get_current_sota(metric)
            if sota:
                exp_id = sota['experiment_id']
                sota_map[metric] = {
                    "experiment_id": exp_id,
                    "value": sota['value'],
                    "experiment": self.experiments.get(exp_id)
                }

        return sota_map

    def get_sota_ensemble(self,
                         num_experiments: int = 5,
                         weights: Optional[Dict[str, float]] = None) -> List[Dict[str, Any]]:
        """
        Select top experiments for ensemble.

        Args:
            num_experiments: Number of experiments to include
            weights: Metric weights for scoring

        Returns:
            Top experiments for ensemble
        """
        if not self.experiments:
            return []

        weights = weights or {'sharpe': 0.4, 'ic': 0.4, 'win_rate': 0.2}

        # Score each experiment
        scored_experiments = []
        for exp_id, exp in self.experiments.items():
            score = sum(
                exp['results'].get(metric, 0) * weight
                for metric, weight in weights.items()
            )

            scored_experiments.append({
                "experiment_id": exp_id,
                "score": score,
                "experiment": exp
            })

        # Sort by score
        scored_experiments.sort(key=lambda x: x['score'], reverse=True)

        return scored_experiments[:num_experiments]


class ExperimentRanker:
    """
    Multi-criteria experiment ranking system.
    """

    def __init__(self):
        self.ranking_methods = {
            'weighted_sum': self._weighted_sum_rank,
            'pareto': self._pareto_rank,
            'topsis': self._topsis_rank
        }

    def rank_experiments(self,
                        experiments: List[Dict[str, Any]],
                        criteria: List[str],
                        weights: Optional[Dict[str, float]] = None,
                        method: str = 'weighted_sum') -> List[Dict[str, Any]]:
        """
        Rank experiments by multiple criteria.

        Args:
            experiments: List of experiments with metrics
            criteria: List of metric names to rank by
            weights: Optional weights for each criterion
            method: Ranking method ('weighted_sum', 'pareto', 'topsis')

        Returns:
            Ranked experiments
        """
        if method not in self.ranking_methods:
            method = 'weighted_sum'

        return self.ranking_methods[method](experiments, criteria, weights)

    def _weighted_sum_rank(self,
                          experiments: List[Dict[str, Any]],
                          criteria: List[str],
                          weights: Optional[Dict[str, float]]) -> List[Dict[str, Any]]:
        """Weighted sum ranking"""
        if not weights:
            weights = {c: 1.0/len(criteria) for c in criteria}

        # Normalize weights
        total_weight = sum(weights.values())
        weights = {k: v/total_weight for k, v in weights.items()}

        # Score each experiment
        for exp in experiments:
            score = sum(
                exp.get('results', {}).get(criterion, 0) * weights.get(criterion, 0)
                for criterion in criteria
            )
            exp['rank_score'] = score

        # Sort by score
        experiments.sort(key=lambda x: x.get('rank_score', 0), reverse=True)

        return experiments

    def _pareto_rank(self,
                    experiments: List[Dict[str, Any]],
                    criteria: List[str],
                    weights: Optional[Dict[str, float]]) -> List[Dict[str, Any]]:
        """Pareto ranking (non-dominated sorting)"""
        if not NUMPY_AVAILABLE:
            return self._weighted_sum_rank(experiments, criteria, weights)

        # Extract metric matrix
        metric_matrix = np.array([
            [exp.get('results', {}).get(c, 0) for c in criteria]
            for exp in experiments
        ])

        # Pareto fronts
        fronts = []
        remaining = list(range(len(experiments)))

        while remaining:
            front = []
            for i in remaining:
                is_dominated = False
                for j in remaining:
                    if i != j:
                        if np.all(metric_matrix[j] >= metric_matrix[i]) and \
                           np.any(metric_matrix[j] > metric_matrix[i]):
                            is_dominated = True
                            break

                if not is_dominated:
                    front.append(i)

            fronts.append(front)
            remaining = [i for i in remaining if i not in front]

        # Assign ranks
        for rank, front in enumerate(fronts):
            for idx in front:
                experiments[idx]['pareto_rank'] = rank

        # Sort by rank
        experiments.sort(key=lambda x: x.get('pareto_rank', float('inf')))

        return experiments

    def _topsis_rank(self,
                    experiments: List[Dict[str, Any]],
                    criteria: List[str],
                    weights: Optional[Dict[str, float]]) -> List[Dict[str, Any]]:
        """TOPSIS (Technique for Order of Preference by Similarity to Ideal Solution)"""
        if not NUMPY_AVAILABLE:
            return self._weighted_sum_rank(experiments, criteria, weights)

        if not weights:
            weights = {c: 1.0/len(criteria) for c in criteria}

        # Extract metric matrix
        metric_matrix = np.array([
            [exp.get('results', {}).get(c, 0) for c in criteria]
            for exp in experiments
        ])

        # Normalize matrix
        normalized = metric_matrix / np.sqrt((metric_matrix**2).sum(axis=0))

        # Apply weights
        weight_vector = np.array([weights.get(c, 1.0/len(criteria)) for c in criteria])
        weighted = normalized * weight_vector

        # Ideal and anti-ideal solutions
        ideal = weighted.max(axis=0)
        anti_ideal = weighted.min(axis=0)

        # Calculate distances
        dist_ideal = np.sqrt(((weighted - ideal)**2).sum(axis=1))
        dist_anti_ideal = np.sqrt(((weighted - anti_ideal)**2).sum(axis=1))

        # Calculate similarity to ideal
        similarity = dist_anti_ideal / (dist_ideal + dist_anti_ideal + 1e-10)

        # Assign scores
        for i, exp in enumerate(experiments):
            exp['topsis_score'] = float(similarity[i])

        # Sort by similarity
        experiments.sort(key=lambda x: x.get('topsis_score', 0), reverse=True)

        return experiments


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "check_status":
            result = {
                "success": True,
                "numpy_available": NUMPY_AVAILABLE,
                "components": [
                    "CheckpointSelector",
                    "SOTASelector",
                    "ExperimentRanker"
                ]
            }
            print(json.dumps(result))
        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
