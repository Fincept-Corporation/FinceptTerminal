# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-77C48AE3
# Category: Regression Test
# Description: Algorithm illustrating how to get a security's industry-standard identifier from its `Symbol`
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Algorithm illustrating how to get a security's industry-standard identifier from its `Symbol`
### </summary>
class IndustryStandardSecurityIdentifiersRegressionAlgorithm(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2014, 6, 5)
        self.set_end_date(2014, 6, 5)

        equity = self.add_equity("AAPL").symbol

        cusip = equity.cusip
        composite_figi = equity.composite_figi
        sedol = equity.sedol
        isin = equity.isin
        cik = equity.cik

        self.check_symbol_representation(cusip, "CUSIP")
        self.check_symbol_representation(composite_figi, "Composite FIGI")
        self.check_symbol_representation(sedol, "SEDOL")
        self.check_symbol_representation(isin, "ISIN")
        self.check_symbol_representation(f"{cik}", "CIK")

        # Check Symbol API vs QCAlgorithm API
        self.check_ap_is_symbol_representations(cusip, self.cusip(equity), "CUSIP")
        self.check_ap_is_symbol_representations(composite_figi, self.composite_figi(equity), "Composite FIGI")
        self.check_ap_is_symbol_representations(sedol, self.sedol(equity), "SEDOL")
        self.check_ap_is_symbol_representations(isin, self.isin(equity), "ISIN")
        self.check_ap_is_symbol_representations(f"{cik}", f"{self.cik(equity)}", "CIK")

        self.log(f"\n_aapl CUSIP: {cusip}"
                 f"\n_aapl Composite FIGI: {composite_figi}"
                 f"\n_aapl SEDOL: {sedol}"
                 f"\n_aapl ISIN: {isin}"
                 f"\n_aapl CIK: {cik}")

    def check_symbol_representation(self, symbol: str, standard: str) -> None:
        if not symbol:
            raise Exception(f"{standard} symbol representation is null or empty")

    def check_ap_is_symbol_representations(self, symbol_api_symbol: str, algorithm_api_symbol: str, standard: str) -> None:
        if symbol_api_symbol != algorithm_api_symbol:
            raise Exception(f"Symbol API {standard} symbol representation ({symbol_api_symbol}) does not match "
                            f"QCAlgorithm API {standard} symbol representation ({algorithm_api_symbol})")
