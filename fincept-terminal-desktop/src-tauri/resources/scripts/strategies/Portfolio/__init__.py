import sys, os, types
_parent = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _parent not in sys.path:
    sys.path.insert(0, _parent)

from AlgorithmImports import (
    PortfolioConstructionModel,
    EqualWeightingPortfolioConstructionModel,
    InsightWeightingPortfolioConstructionModel,
    MeanVarianceOptimizationPortfolioConstructionModel,
    BlackLittermanOptimizationPortfolioConstructionModel,
    AccumulativeInsightPortfolioConstructionModel,
    ConfidenceWeightedPortfolioConstructionModel,
    SectorWeightingPortfolioConstructionModel,
    PortfolioTarget, PortfolioBias
)

_submodules = {
    'EqualWeightingPortfolioConstructionModel': {'EqualWeightingPortfolioConstructionModel': EqualWeightingPortfolioConstructionModel},
    'InsightWeightingPortfolioConstructionModel': {'InsightWeightingPortfolioConstructionModel': InsightWeightingPortfolioConstructionModel},
    'MeanVarianceOptimizationPortfolioConstructionModel': {'MeanVarianceOptimizationPortfolioConstructionModel': MeanVarianceOptimizationPortfolioConstructionModel},
    'BlackLittermanOptimizationPortfolioConstructionModel': {'BlackLittermanOptimizationPortfolioConstructionModel': BlackLittermanOptimizationPortfolioConstructionModel},
    'AccumulativeInsightPortfolioConstructionModel': {'AccumulativeInsightPortfolioConstructionModel': AccumulativeInsightPortfolioConstructionModel},
    'ConfidenceWeightedPortfolioConstructionModel': {'ConfidenceWeightedPortfolioConstructionModel': ConfidenceWeightedPortfolioConstructionModel},
    'SectorWeightingPortfolioConstructionModel': {'SectorWeightingPortfolioConstructionModel': SectorWeightingPortfolioConstructionModel},
}

for _name, _attrs in _submodules.items():
    _mod = types.ModuleType(f'Portfolio.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    sys.modules[f'Portfolio.{_name}'] = _mod
