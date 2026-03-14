# ============================================================================
# Fincept Terminal - Strategy Engine
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
# https://github.com/Fincept-Corporation/FinceptTerminal
#
# Strategy ID: FCT-9C878F2C
# Category: Benchmark
# Description: Benchmark Algorithm: Loading and synchronization of 500 equity minute symbols and their options
# Compatibility: Backtesting | Paper Trading | Live Deployment
# ============================================================================
from AlgorithmImports import *

### <summary>
### Benchmark Algorithm: Loading and synchronization of 500 equity minute symbols and their options.
### </summary>
class EmptyEquityAndOptions400Benchmark(QCAlgorithm):

    def initialize(self):
        self.set_start_date(2022, 5, 11)
        self.set_end_date(2022, 5, 12)
        self.equity_symbols = [

"MARK", "TSN", "DT", "RDW", "CVE", "NXPI", "FIVN", "CLX", "SPXL", "BKSY", "NUGT", "CF", "NEGG",
"RH", "SIRI", "ITUB", "CSX", "AUR", "LIDR", "CMPS", "DHI", "GLW", "NTES", "CIFR", "S", "HSBC",
"HIPO", "WTRH", "AMRN", "BIIB", "RIO", "EDIT", "TEAM", "CNK", "BUD", "MILE", "AEHR", "DOCN",
"CLSK", "BROS", "MLCO", "SBLK", "ICLN", "OPK", "CNC", "SKX", "SESN", "VRM", "ASML", "BBAI",
"HON", "MRIN", "BLMN", "NTNX", "POWW", "FOUR", "HOG", "GOGO", "MGNI", "GENI", "XPDI",
"DG", "PSX", "RRC", "CORT", "MET", "UMC", "INMD", "RBAC", "ISRG", "BOX", "DVAX", "CRVS", "HLT",
"BKNG", "BENE", "CLVS", "ESSC", "PTRA", "BE", "FPAC", "YETI", "DOCS", "DB", "EBON", "RDS.B",
"ERIC", "BSIG", "INTU", "MNTS", "BCTX", "BLU", "FIS", "MAC", "WMB", "TTWO", "ARDX", "SWBI",
"ELY", "INDA", "REAL", "ACI", "APRN", "BHP", "CPB", "SLQT", "ARKF", "TSP", "OKE", "NVTA", "META",
"CSTM", "KMX", "IBB", "AGEN", "WOOF", "MJ", "HYZN", "RSI", "JCI", "EXC", "HPE", "SI", "WPM",
"PRTY", "BBD", "FVRR", "CANO", "INDI", "MDLZ", "KOLD", "AMBA", "SOXS", "RSX", "ZEN", "PUBM",
"VLDR", "CI", "ISEE", "GEO", "BKR", "DHR", "GRPN", "NRXP", "ACN", "MAT", "BODY", "ENDP",
"SHPW", "AVIR", "GPN", "BILL", "BZ", "CERN", "ARVL", "DNMR", "NTR", "FSM", "BMBL", "PAAS",
"INVZ", "ANF", "CL", "XP", "CS", "KD", "WW", "AHT", "GRTX", "XLC", "BLDP", "HTA", "APT", "BYSI",
"ENB", "TRIT", "VTNR", "AVCT", "SLI", "CP", "CAH", "ALLY", "FIGS", "PXD", "TPX", "ZI", "BKLN", "SKIN",
"LNG", "NU", "CX", "GSM", "NXE", "REI", "MNDT", "IP", "BLOK", "IAA", "TIP", "MCHP", "EVTL", "BIGC",
"IGV", "LOTZ", "EWC", "DRI", "PSTG", "APLS", "KIND", "BBIO", "APPH", "FIVE", "LSPD", "SHAK",
"COMM", "NAT", "VFC", "AMT", "VRTX", "RGS", "DD", "GBIL", "LICY", "ACHR", "FLR", "HGEN", "TECL",
"SEAC", "NVS", "NTAP", "ML", "SBSW", "XRX", "UA", "NNOX", "SFT", "FE", "APP", "KEY", "CDEV",
"DPZ", "BARK", "SPR", "CNQ", "XL", "AXSM", "ECH", "RNG", "AMLP", "ENG", "BTI", "REKR",
"STZ", "BK", "HEAR", "LEV", "SKT", "HBI", "ALB", "CAG", "MNKD", "NMM", "BIRD", "CIEN", "SILJ",
"STNG", "GUSH", "GIS", "PRPL", "SDOW", "GNRC", "ERX", "GES", "CPE", "FBRX", "WM", "ESTC",
"GOED", "STLD", "LILM", "JNK", "BOIL", "ALZN", "IRBT", "KOPN", "AU", "TPR", "RWLK", "TROX",
"TMO", "AVDL", "XSPA", "JKS", "PACB", "LOGI", "BLK", "REGN", "CFVI", "EGHT", "ATNF", "PRU",
"URBN", "KMB", "SIX", "CME", "ENVX", "NVTS", "CELH", "CSIQ", "GSL", "PAA", "WU", "MOMO",
"TOL", "WEN", "GTE", "EXAS", "GDRX", "PVH", "BFLY", "SRTY", "UDOW", "NCR", "ALTO", "CRTD",
"GOCO", "ALK", "TTM", "DFS", "VFF", "ANTM", "FREY", "WY", "ACWI", "PNC", "SYY", "SNY", "CRK",
"SO", "XXII", "PBF", "AER", "RKLY", "SOL", "CND", "MPLX", "JNPR", "FTCV", "CLR", "XHB", "YY",
"POSH", "HIMS", "LIFE", "XENE", "ADM", "ROST", "MIR", "NRG", "AAP", "SSYS", "KBH", "KKR", "PLAN",
"DUK", "WIMI", "DBRG", "WSM", "LTHM", "OVV", "CFLT", "EWT", "UNFI", "TX", "EMR", "IMGN", "K",
"ONON", "UNIT", "LEVI", "ADTX", "UPWK", "DBA", "VOO", "FATH", "URI", "MPW", "JNUG", "RDFN",
"OSCR", "WOLF", "SYF", "GOGL", "HES", "PHM", "CWEB", "ALDX", "BTWN", "AFL", "PPL", "CIM"

        ]

        self.set_warm_up(TimeSpan.from_days(1))
        for ticker in self.equity_symbols:
            option = self.add_option(ticker)
            option.set_filter(1, 7, timedelta(0), timedelta(90))
        
        self.add_equity("SPY")

    def on_data(self, slice):
        if self.is_warming_up: return
        self.quit("The end!")
