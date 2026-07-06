# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-F172EB67
# Category: Data Consolidation
# Description: Example algorithm of how to use ClassicRangeConsolidator
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *
from RangeConsolidatorAlgorithm import RangeConsolidatorAlgorithm

### <summary>
### Example algorithm of how to use ClassicRangeConsolidator
### </summary>
class ClassicRangeConsolidatorAlgorithm(RangeConsolidatorAlgorithm):
    def create_range_consolidator(self):
        return ClassicRangeConsolidator(self.get_range())
    
    def on_data_consolidated(self, sender, range_bar):
        super().on_data_consolidated(sender, range_bar)

        if range_bar.volume == 0:
            raise Exception("All RangeBar's should have non-zero volume, but this doesn't")

