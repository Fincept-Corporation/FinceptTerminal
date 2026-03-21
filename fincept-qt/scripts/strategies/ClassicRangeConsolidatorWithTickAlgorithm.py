# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-5407C44C
# Category: Data Consolidation
# Description: Example algorithm of how to use ClassicRangeConsolidator with Tick resolution
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from RangeConsolidatorWithTickAlgorithm import RangeConsolidatorWithTickAlgorithm

### <summary>
### Example algorithm of how to use ClassicRangeConsolidator with Tick resolution
### </summary>
class ClassicRangeConsolidatorWithTickAlgorithm(RangeConsolidatorWithTickAlgorithm):
    def create_range_consolidator(self):
        return ClassicRangeConsolidator(self.get_range())
    
    def on_data_consolidated(self, sender, range_bar):
        super().on_data_consolidated(sender, range_bar)

        if range_bar.volume == 0:
            raise Exception("All RangeBar's should have non-zero volume, but this doesn't")
