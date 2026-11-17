# ============================================================================
# Fincept Terminal - Strategy Engine: universe types (LEAN's universe API)
#
# What a strategy sees of a dynamic universe:
#   * UniverseSettings (resolution, leverage, minimum_time_in_universe, asynchronous,
#     schedule.on(date_rule), ...), copied with UniverseSettings(other)
#   * the universes it adds: FundamentalUniverse.usa(selector) / coarse + fine,
#     ETF constituents, user-defined (name, selector(datetime)), ScheduledUniverse,
#     manual lists - each with .symbol, .members, .selected, .universe_settings
#   * Universe.UNCHANGED (also Universe.unchanged / Universe.unchanged())
#   * the selection data: Fundamental objects (one object answers the coarse fields
#     and LEAN's fine attribute tree) and ETFConstituentData objects
#   * UniverseDefinitions behind algorithm.universe (.etf(...), .qc_500, .top(n),
#     .dollar_volume.top(n))
#
# Missing fundamental values look the way LEAN shows them: double -> NaN,
# long/int -> 0, string -> None, date -> datetime(1, 1, 1), bool -> False.
# A LEAN field the engine has no source for raises NotSupportedByEngine instead of
# returning an invented value. The data behind these objects (pool bars, Yahoo
# fundamentals snapshot, ETF holdings) lives in universe_data.py; the selection
# loop (when each universe selects, additions/removals) in universe_selection.py.
# ============================================================================

import itertools
import math
import re
from datetime import datetime, timedelta

from .enums import Resolution, SecurityType, Market, DataNormalizationMode
from .errors import not_supported
from .types import Symbol, SecurityIdentifier

NaN = float('nan')
NO_DATE = datetime(1, 1, 1)          # LEAN's default(DateTime): a missing date
ONE_DAY = timedelta(days=1)

# ----------------------------------------------------------------------------
# Morningstar classification codes (LEAN AssetClassificationHelper)
# ----------------------------------------------------------------------------

_SNAKE = re.compile(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])')


def _upper_snake(name: str) -> str:
    return _SNAKE.sub('_', name).upper()


def _code_class(class_name, pairs, doc):
    """A class with each code as PascalCase and UPPER_SNAKE attributes (LEAN's Python names)."""
    attrs = {'__doc__': doc, '_PAIRS': tuple(pairs)}
    for n, v in pairs:
        attrs[n] = v
        attrs[_upper_snake(n)] = v
    return type(class_name, (), attrs)


MorningstarEconomySphereCode = _code_class('MorningstarEconomySphereCode', (
    ('Cyclical', 1), ('Defensive', 2), ('Sensitive', 3)), "Morningstar super sector (economy sphere) codes.")

MorningstarSectorCode = _code_class('MorningstarSectorCode', (
    ('BasicMaterials', 101), ('ConsumerCyclical', 102), ('FinancialServices', 103), ('RealEstate', 104),
    ('ConsumerDefensive', 205), ('Healthcare', 206), ('Utilities', 207), ('CommunicationServices', 308),
    ('Energy', 309), ('Industrials', 310), ('Technology', 311)), "Morningstar sector codes.")

MorningstarIndustryGroupCode = _code_class('MorningstarIndustryGroupCode', (
    ('Agriculture', 10110), ('BuildingMaterials', 10120), ('Chemicals', 10130), ('ForestProducts', 10140),
    ('MetalsAndMining', 10150), ('Steel', 10160), ('VehiclesAndParts', 10200), ('Furnishings', 10220),
    ('FixturesAndAppliances', 10220), ('HomebuildingAndConstruction', 10230),
    ('ManufacturingApparelAndAccessories', 10240), ('PackagingAndContainers', 10250),
    ('PersonalServices', 10260), ('Restaurants', 10270), ('RetailCyclical', 10280),
    ('TravelAndLeisure', 10290), ('AssetManagement', 10310), ('Banks', 10320), ('CapitalMarkets', 10330),
    ('Insurance', 10340), ('DiversifiedFinancialServices', 10350), ('CreditServices', 10360),
    ('RealEstate', 10410), ('REITs', 10420), ('BeveragesAlcoholic', 20510), ('BeveragesNonAlcoholic', 20520),
    ('ConsumerPackagedGoods', 20525), ('Education', 20540), ('RetailDefensive', 20550),
    ('TobaccoProducts', 20560), ('Biotechnology', 20610), ('DrugManufacturers', 20620),
    ('HealthcarePlans', 20630), ('HealthcareProvidersAndServices', 20645),
    ('MedicalDevicesAndInstruments', 20650), ('MedicalDiagnosticsAndResearch', 20660),
    ('MedicalDistribution', 20670), ('UtilitiesIndependentPowerProducers', 20710),
    ('UtilitiesRegulated', 20720), ('TelecommunicationServices', 30810), ('MediaDiversified', 30820),
    ('InteractiveMedia', 30830), ('OilAndGas', 30910), ('OtherEnergySources', 30920),
    ('AerospaceAndDefense', 31010), ('BusinessServices', 31020), ('Conglomerates', 31030),
    ('Construction', 31040), ('FarmAndHeavyConstructionMachinery', 31050), ('IndustrialDistribution', 31060),
    ('IndustrialProducts', 31070), ('Transportation', 31080), ('WasteManagement', 31090),
    ('Software', 31110), ('Hardware', 31120), ('Semiconductors', 31130)),
    "Morningstar industry group codes.")
MorningstarIndustryGroupCode.REITS = MorningstarIndustryGroupCode.REITs

MorningstarIndustryCode = _code_class('MorningstarIndustryCode', (
    ('AgriculturalInputs', 10110010), ('BuildingMaterials', 10120010), ('Chemicals', 10130010),
    ('SpecialtyChemicals', 10130020), ('LumberAndWoodProduction', 10140010),
    ('PaperAndPaperProducts', 10140020), ('Aluminum', 10150010), ('Copper', 10150020),
    ('OtherIndustrialMetalsAndMining', 10150030), ('Gold', 10150040), ('Silver', 10150050),
    ('OtherPreciousMetalsAndMining', 10150060), ('CokingCoal', 10160010), ('Steel', 10160020),
    ('AutoAndTruckDealerships', 10200010), ('AutoManufacturers', 10200020), ('AutoParts', 10200030),
    ('RecreationalVehicles', 10200040), ('Furnishings', 10220010), ('FixturesAndAppliances', 10220010),
    ('ResidentialConstruction', 10230010), ('TextileManufacturing', 10240010),
    ('ApparelManufacturing', 10240020), ('FootwearAndAccessories', 10240030),
    ('PackagingAndContainers', 10250010), ('PersonalServices', 10260010), ('Restaurants', 10270010),
    ('ApparelRetail', 10280010), ('DepartmentStores', 10280020), ('HomeImprovementRetail', 10280030),
    ('LuxuryGoods', 10280040), ('InternetRetail', 10280050), ('SpecialtyRetail', 10280060),
    ('Gambling', 10290010), ('Leisure', 10290020), ('Lodging', 10290030), ('ResortsAndCasinos', 10290040),
    ('TravelServices', 10290050), ('AssetManagement', 10310010), ('BanksDiversified', 10320010),
    ('BanksRegional', 10320020), ('MortgageFinance', 10320030), ('CapitalMarkets', 10330010),
    ('FinancialDataAndStockExchanges', 10330020), ('InsuranceLife', 10340010),
    ('InsurancePropertyAndCasualty', 10340020), ('InsuranceReinsurance', 10340030),
    ('InsuranceSpecialty', 10340040), ('InsuranceBrokers', 10340050), ('InsuranceDiversified', 10340060),
    ('ShellCompanies', 10350010), ('FinancialConglomerates', 10350020), ('CreditServices', 10360010),
    ('RealEstateDevelopment', 10410010), ('RealEstateServices', 10410020),
    ('RealEstateDiversified', 10410030), ('REITHealthcareFacilities', 10420010),
    ('REITHotelAndMotel', 10420020), ('REITIndustrial', 10420030), ('REITOffice', 10420040),
    ('REITResidential', 10420050), ('REITRetail', 10420060), ('REITMortgage', 10420070),
    ('REITSpecialty', 10420080), ('REITDiversified', 10420090), ('BeveragesBrewers', 20510010),
    ('BeveragesWineriesAndDistilleries', 20510020), ('BeveragesNonAlcoholic', 20520010),
    ('Confectioners', 20525010), ('FarmProducts', 20525020), ('HouseholdAndPersonalProducts', 20525030),
    ('PackagedFoods', 20525040), ('EducationAndTrainingServices', 20540010), ('DiscountStores', 20550010),
    ('FoodDistribution', 20550020), ('GroceryStores', 20550030), ('Tobacco', 20560010),
    ('Biotechnology', 20610010), ('DrugManufacturersGeneral', 20620010),
    ('DrugManufacturersSpecialtyAndGeneric', 20620020), ('HealthcarePlans', 20630010),
    ('MedicalCareFacilities', 20645010), ('PharmaceuticalRetailers', 20645020),
    ('HealthInformationServices', 20645030), ('MedicalDevices', 20650010),
    ('MedicalInstrumentsAndSupplies', 20650020), ('DiagnosticsAndResearch', 20660010),
    ('MedicalDistribution', 20670010), ('UtilitiesIndependentPowerProducers', 20710010),
    ('UtilitiesRenewable', 20710020), ('UtilitiesRegulatedWater', 20720010),
    ('UtilitiesRegulatedElectric', 20720020), ('UtilitiesRegulatedGas', 20720030),
    ('UtilitiesDiversified', 20720040), ('TelecomServices', 30810010), ('AdvertisingAgencies', 30820010),
    ('Publishing', 30820020), ('Broadcasting', 30820030), ('Entertainment', 30820040),
    ('InternetContentAndInformation', 30830010), ('ElectronicGamingAndMultimedia', 30830020),
    ('OilAndGasDrilling', 30910010), ('OilAndGasEAndP', 30910020), ('OilAndGasIntegrated', 30910030),
    ('OilAndGasMidstream', 30910040), ('OilAndGasRefiningAndMarketing', 30910050),
    ('OilAndGasEquipmentAndServices', 30910060), ('ThermalCoal', 30920010), ('Uranium', 30920020),
    ('AerospaceAndDefense', 31010010), ('SpecialtyBusinessServices', 31020010),
    ('ConsultingServices', 31020020), ('RentalAndLeasingServices', 31020030),
    ('SecurityAndProtectionServices', 31020040), ('StaffingAndEmploymentServices', 31020050),
    ('Conglomerates', 31030010), ('EngineeringAndConstruction', 31040010),
    ('InfrastructureOperations', 31040020), ('BuildingProductsAndEquipment', 31040030),
    ('FarmAndHeavyConstructionMachinery', 31050010), ('IndustrialDistribution', 31060010),
    ('BusinessEquipmentAndSupplies', 31070010), ('SpecialtyIndustrialMachinery', 31070020),
    ('MetalFabrication', 31070030), ('PollutionAndTreatmentControls', 31070040),
    ('ToolsAndAccessories', 31070050), ('ElectricalEquipmentAndParts', 31070060),
    ('AirportsAndAirServices', 31080010), ('Airlines', 31080020), ('Railroads', 31080030),
    ('MarineShipping', 31080040), ('Trucking', 31080050), ('IntegratedFreightAndLogistics', 31080060),
    ('WasteManagement', 31090010), ('InformationTechnologyServices', 31110010),
    ('SoftwareApplication', 31110020), ('SoftwareInfrastructure', 31110030),
    ('CommunicationEquipment', 31120010), ('ComputerHardware', 31120020), ('ConsumerElectronics', 31120030),
    ('ElectronicComponents', 31120040), ('ElectronicsAndComputerDistribution', 31120050),
    ('ScientificAndTechnicalInstruments', 31120060), ('SemiconductorEquipmentAndMaterials', 31130010),
    ('Semiconductors', 31130020), ('Solar', 31130030)),
    "Morningstar industry codes.")

# ----------------------------------------------------------------------------
# Multi-period fundamental fields (LEAN MultiPeriodField)
# ----------------------------------------------------------------------------

PERIODS = ('one_month', 'two_months', 'three_months', 'six_months', 'nine_months', 'twelve_months',
           'one_year', 'two_years', 'three_years', 'five_years', 'ten_years')
_PERIOD_CODE = {'one_month': '1M', 'two_months': '2M', 'three_months': '3M', 'six_months': '6M',
                'nine_months': '9M', 'twelve_months': '12M', 'one_year': '1Y', 'two_years': '2Y',
                'three_years': '3Y', 'five_years': '5Y', 'ten_years': '10Y'}
_PERIOD_ALIASES = {}
for _p, _c in _PERIOD_CODE.items():
    _PERIOD_ALIASES[_p] = _p
    _PERIOD_ALIASES[_c] = _p
    _PERIOD_ALIASES[_p.replace('_', '')] = _p
    _PERIOD_ALIASES[''.join(w.capitalize() for w in _p.split('_'))] = _p


def _period_name(period) -> str:
    p = str(period or '').strip()
    return _PERIOD_ALIASES.get(p) or _PERIOD_ALIASES.get(p.lower()) or p


def _finite(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    return f if math.isfinite(f) else None


class MultiPeriodField:
    """A fundamental value reported for several periods (three_months, twelve_months, one_year...).

    A period the source does not report reads NaN (LEAN's missing double). `.value` is the
    field's default period, else the first period that has a value, else NaN."""

    __slots__ = ('_values', '_default', '_name')

    def __init__(self, values=None, default_period='twelve_months', name=''):
        vals = {}
        for k, v in (values or {}).items():
            f = _finite(v)
            if f is not None:
                vals[_period_name(k)] = f
        self._values = vals
        self._default = _period_name(default_period)
        self._name = name

    def __getattr__(self, name):
        p = _PERIOD_ALIASES.get(name)
        if p is not None:
            return self._values.get(p, NaN)
        raise AttributeError(f"'{type(self).__name__}' object has no attribute '{name}'")

    @property
    def value(self):
        v = self._values.get(self._default)
        if v is not None:
            return v
        for p in PERIODS:
            if p in self._values:
                return self._values[p]
        return NaN

    Value = value

    @property
    def has_value(self) -> bool:
        return self._default in self._values

    HasValue = has_value

    def has_values(self) -> bool:
        return bool(self._values)

    def get_period_value(self, period):
        return self._values.get(_period_name(period), NaN)

    def has_period_value(self, period) -> bool:
        return _period_name(period) in self._values

    def get_period_values(self):
        return {_PERIOD_CODE[p]: self._values[p] for p in PERIODS if p in self._values}

    def get_period_names(self):
        return list(self.get_period_values().keys())

    GetPeriodValue = get_period_value
    HasPeriodValue = has_period_value
    GetPeriodValues = get_period_values
    GetPeriodNames = get_period_names

    def __float__(self):
        return float(self.value)

    def __repr__(self):
        return ';'.join(f"{k}:{v}" for k, v in self.get_period_values().items())

    __str__ = __repr__

    # arithmetic / comparisons read the default value (LEAN's implicit conversion)
    def _v(self, other):
        return float(other.value) if isinstance(other, MultiPeriodField) else other

    def __add__(self, o): return self.value + self._v(o)
    def __radd__(self, o): return self._v(o) + self.value
    def __sub__(self, o): return self.value - self._v(o)
    def __rsub__(self, o): return self._v(o) - self.value
    def __mul__(self, o): return self.value * self._v(o)
    def __rmul__(self, o): return self._v(o) * self.value
    def __truediv__(self, o): return self.value / self._v(o)
    def __rtruediv__(self, o): return self._v(o) / self.value
    def __neg__(self): return -self.value
    def __lt__(self, o): return self.value < self._v(o)
    def __le__(self, o): return self.value <= self._v(o)
    def __gt__(self, o): return self.value > self._v(o)
    def __ge__(self, o): return self.value >= self._v(o)


class FinancialValue(MultiPeriodField):
    """A multi-period value set directly (kept from the earlier engine API): every period = value."""

    __slots__ = ()

    def __init__(self, value: float = 0):
        super().__init__({p: value for p in PERIODS}, 'twelve_months')


# ----------------------------------------------------------------------------
# The fine attribute tree (LEAN Fundamental sub-objects)
# ----------------------------------------------------------------------------

_DOUBLE, _LONG, _INT, _STRING, _DATE, _BOOL = 'double', 'long', 'int', 'string', 'date', 'bool'
_MISSING = {_DOUBLE: NaN, _LONG: 0, _INT: 0, _STRING: None, _DATE: NO_DATE, _BOOL: False}


def _multi(default_period):
    return ('multi', default_period)


class _Section:
    """One LEAN fundamental sub-object. Fields come from `values` (filled by the data
    provider); a field the source lacks for this security reads LEAN's missing value; a
    LEAN field the engine has no source for raises NotSupportedByEngine."""

    _section = ''
    _fields = {}       # name -> kind | ('multi', default period)
    _aliases = {}      # older engine attribute names -> LEAN names
    _children = {}     # name -> _Section subclass (financial_statements)

    def __init__(self, values=None):
        object.__setattr__(self, '_values', dict(values or {}))

    def __getattr__(self, name):
        if name.startswith('_'):
            raise AttributeError(name)
        cls = type(self)
        key = cls._aliases.get(name, name)
        child = cls._children.get(key)
        if child is not None:
            obj = child(self._values.get(key))
            object.__setattr__(self, key, obj)
            return obj
        spec = cls._fields.get(key)
        if spec is None:
            raise not_supported(f"the fundamental field {cls._section}.{name}",
                                f"Fundamental data comes from a Yahoo Finance snapshot; available fields: "
                                f"{available_fields_text()}.")
        v = self._values.get(key)
        if isinstance(spec, tuple):
            if isinstance(v, MultiPeriodField):
                return v
            return MultiPeriodField(v, spec[1], key)
        if v is None:
            return _MISSING[spec]
        return v

    def __repr__(self):
        shown = {k: v for k, v in self._values.items() if not isinstance(v, dict)}
        return f"{type(self).__name__}({shown})"


def _section(class_name, section, fields, aliases=None, children=None):
    return type(class_name, (_Section,), {'_section': section, '_fields': dict(fields),
                                          '_aliases': dict(aliases or {}), '_children': dict(children or {})})


ValuationRatios = _section('ValuationRatios', 'valuation_ratios', {
    'pe_ratio': _DOUBLE, 'forward_pe_ratio': _DOUBLE, 'pb_ratio': _DOUBLE, 'ps_ratio': _DOUBLE,
    'pcf_ratio': _DOUBLE, 'fcf_ratio': _DOUBLE, 'peg_ratio': _DOUBLE, 'ev_to_ebitda': _DOUBLE,
    'ev_to_revenue': _DOUBLE, 'price_to_ebitda': _DOUBLE, 'earning_yield': _DOUBLE,
    'forward_earning_yield': _DOUBLE, 'sales_yield': _DOUBLE, 'book_value_yield': _DOUBLE,
    'cf_yield': _DOUBLE, 'fcf_yield': _DOUBLE, 'trailing_dividend_yield': _DOUBLE,
    'forward_dividend_yield': _DOUBLE, 'forward_dividend': _DOUBLE, 'payout_ratio': _DOUBLE,
    'book_value_per_share': _DOUBLE, 'sales_per_share': _DOUBLE,
    'cash_cash_equivalents_and_short_term_investments_per_share': _DOUBLE,
}, aliases={'forward_pe': 'forward_pe_ratio', 'earnings_yield': 'earning_yield',
            'dividend_yield': 'trailing_dividend_yield'})

OperationRatios = _section('OperationRatios', 'operation_ratios', {
    'roe': _multi('one_year'), 'roa': _multi('one_year'), 'gross_margin': _multi('one_year'),
    'operation_margin': _multi('one_year'), 'net_margin': _multi('one_year'),
    'ebitda_margin': _multi('one_year'), 'revenue_growth': _multi('one_year'),
    'net_income_growth': _multi('one_year'), 'current_ratio': _multi('one_year'),
    'quick_ratio': _multi('one_year'), 'total_debt_equity_ratio': _multi('one_year'),
}, aliases={'operating_margin': 'operation_margin'})

EarningRatios = _section('EarningRatios', 'earning_ratios', {
    'diluted_eps_growth': _multi('one_year'),
})

EarningReports = _section('EarningReports', 'earning_reports', {
    'basic_eps': _multi('twelve_months'), 'diluted_eps': _multi('twelve_months'),
    'basic_average_shares': _multi('twelve_months'), 'diluted_average_shares': _multi('twelve_months'),
    'dividend_per_share': _multi('twelve_months'),
})

IncomeStatement = _section('IncomeStatement', 'financial_statements.income_statement', {
    'total_revenue': _multi('twelve_months'), 'gross_profit': _multi('twelve_months'),
    'ebitda': _multi('twelve_months'), 'net_income': _multi('twelve_months'),
    'net_income_common_stockholders': _multi('twelve_months'),
})

BalanceSheet = _section('BalanceSheet', 'financial_statements.balance_sheet', {
    'total_debt': _multi('twelve_months'),
    'cash_cash_equivalents_and_short_term_investments': _multi('twelve_months'),
})

CashFlowStatement = _section('CashFlowStatement', 'financial_statements.cash_flow_statement', {
    'operating_cash_flow': _multi('twelve_months'), 'free_cash_flow': _multi('twelve_months'),
})

FinancialStatements = _section('FinancialStatements', 'financial_statements', {}, children={
    'income_statement': IncomeStatement, 'balance_sheet': BalanceSheet,
    'cash_flow_statement': CashFlowStatement})

CompanyReference = _section('CompanyReference', 'company_reference', {
    'country_id': _STRING, 'primary_exchange_id': _STRING, 'industry_template_code': _STRING,
    'short_name': _STRING, 'standard_name': _STRING, 'legal_name': _STRING, 'primary_symbol': _STRING,
    'business_country_id': _STRING, 'is_reit': _BOOL,
})

SecurityReference = _section('SecurityReference', 'security_reference', {
    'security_symbol': _STRING, 'exchange_id': _STRING, 'currency_id': _STRING, 'ipo_date': _DATE,
}, aliases={'listing_exchange': 'exchange_id'})

CompanyProfile = _section('CompanyProfile', 'company_profile', {
    'headquarter_address_line1': _STRING, 'headquarter_city': _STRING, 'headquarter_province': _STRING,
    'headquarter_country': _STRING, 'headquarter_postal_code': _STRING, 'headquarter_phone': _STRING,
    'headquarter_homepage': _STRING, 'total_employee_number': _INT, 'full_time_employee_number': _INT,
    'shares_outstanding': _LONG, 'market_cap': _LONG, 'enterprise_value': _LONG,
    'long_description': _STRING,
})

AssetClassification = _section('AssetClassification', 'asset_classification', {
    'morningstar_sector_code': _INT, 'morningstar_industry_group_code': _INT,
    'morningstar_industry_code': _INT, 'morningstar_economy_sphere_code': _INT,
})

SECTION_CLASSES = {
    'company_reference': CompanyReference, 'security_reference': SecurityReference,
    'financial_statements': FinancialStatements, 'earning_reports': EarningReports,
    'operation_ratios': OperationRatios, 'earning_ratios': EarningRatios,
    'valuation_ratios': ValuationRatios, 'asset_classification': AssetClassification,
    'company_profile': CompanyProfile,
}


def available_fields_text() -> str:
    parts = []
    for name, cls in SECTION_CLASSES.items():
        if cls._children:
            for cn, cc in cls._children.items():
                parts.append(f"{name}.{cn}.{{{', '.join(cc._fields)}}}")
        else:
            parts.append(f"{name}.{{{', '.join(cls._fields)}}}")
    return '; '.join(parts) + '; market_cap'


# ----------------------------------------------------------------------------
# Selection data: Fundamental (coarse + fine in one object) and ETF constituents
# ----------------------------------------------------------------------------

class CoarseFundamental:
    """LEAN CoarseFundamental: one security's previous-session price data.

    value / price: the raw (unadjusted) close; volume: raw shares traded; dollar_volume:
    price x volume; adjusted_price = price x price_factor x split_factor;
    time: the session date; end_time = time + 1 day (when the data became known)."""

    def __init__(self, symbol=None, dollar_volume=0.0, price=0.0, volume=0, has_fundamental_data=False,
                 time=None, price_factor=1.0, split_factor=1.0, end_time=None):
        self.symbol = symbol
        self.time = time
        self._value = float(price or 0.0)
        self.dollar_volume = float(dollar_volume or 0.0)
        self.volume = int(volume or 0)
        self.has_fundamental_data = bool(has_fundamental_data)
        self.price_factor = float(price_factor)
        self.split_factor = float(split_factor)
        self._end_time = end_time

    # ---- values ----
    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, v):
        self._value = float(v or 0.0)

    price = value

    @property
    def end_time(self):
        if self._end_time is not None:
            return self._end_time
        return self.time + ONE_DAY if self.time is not None else None

    @end_time.setter
    def end_time(self, v):
        self._end_time = v

    @property
    def price_scale_factor(self):
        return self.price_factor * self.split_factor

    @property
    def adjusted_price(self):
        return self._value * self.price_factor * self.split_factor

    @property
    def market(self):
        sym = self.symbol
        sid = getattr(sym, 'id', None)
        return getattr(sid, 'market', None) or getattr(sym, 'market', None) or Market.USA

    # PascalCase
    Symbol = property(lambda self: self.symbol)
    Value = property(lambda self: self._value)
    Price = property(lambda self: self._value)
    Volume = property(lambda self: self.volume)
    DollarVolume = property(lambda self: self.dollar_volume)
    HasFundamentalData = property(lambda self: self.has_fundamental_data)
    AdjustedPrice = property(lambda self: self.adjusted_price)
    EndTime = property(lambda self: self.end_time)

    def __repr__(self):
        return f"{self.symbol}: {self._value}"


class FineFundamental(CoarseFundamental):
    """LEAN FineFundamental: the coarse fields plus the Morningstar-style attribute tree,
    loaded on first access to a fine attribute."""

    _loader = None

    def __init__(self, symbol=None, *args, **kwargs):
        super().__init__(symbol, *args, **kwargs)

    def _sections(self):
        secs = self.__dict__.get('_secs')
        if secs is None:
            values = {}
            loader = self.__dict__.get('_loader') or type(self)._loader
            if self.has_fundamental_data and loader is not None:
                values = loader(self) or {}
            secs = {name: cls(values.get(name)) for name, cls in SECTION_CLASSES.items()}
            self._secs = secs
        return secs

    def _get_section(self, name):
        return self._sections()[name]

    def _set_section(self, name, value):
        self._sections()[name] = value

    company_reference = property(lambda self: self._get_section('company_reference'),
                                 lambda self, v: self._set_section('company_reference', v))
    security_reference = property(lambda self: self._get_section('security_reference'),
                                  lambda self, v: self._set_section('security_reference', v))
    financial_statements = property(lambda self: self._get_section('financial_statements'),
                                    lambda self, v: self._set_section('financial_statements', v))
    earning_reports = property(lambda self: self._get_section('earning_reports'),
                               lambda self, v: self._set_section('earning_reports', v))
    operation_ratios = property(lambda self: self._get_section('operation_ratios'),
                                lambda self, v: self._set_section('operation_ratios', v))
    earning_ratios = property(lambda self: self._get_section('earning_ratios'),
                              lambda self, v: self._set_section('earning_ratios', v))
    valuation_ratios = property(lambda self: self._get_section('valuation_ratios'),
                                lambda self, v: self._set_section('valuation_ratios', v))
    asset_classification = property(lambda self: self._get_section('asset_classification'),
                                    lambda self, v: self._set_section('asset_classification', v))
    company_profile = property(lambda self: self._get_section('company_profile'),
                               lambda self, v: self._set_section('company_profile', v))

    @property
    def market_cap(self):
        """LEAN FineFundamental.MarketCap = company_profile.market_cap (long; 0 when missing)."""
        if '_market_cap' in self.__dict__:
            return self.__dict__['_market_cap']
        return self.company_profile.market_cap

    @market_cap.setter
    def market_cap(self, v):
        self.__dict__['_market_cap'] = v

    MarketCap = property(lambda self: self.market_cap)


class Fundamental(FineFundamental):
    """LEAN Fundamental: one object per security and session that answers both the coarse
    fields (price, volume, dollar_volume, has_fundamental_data, ...) and the fine attribute
    tree (valuation_ratios.pe_ratio, asset_classification.morningstar_sector_code, ...).

    Fundamental(time, symbol) as in LEAN; keyword values are accepted too (price, volume,
    dollar_volume, has_fundamental_data, market_cap, end_time, valuation_ratios, ...)."""

    def __init__(self, time=None, symbol=None, **kwargs):
        if isinstance(time, (Symbol, str)) and not isinstance(symbol, (Symbol, str)):
            time, symbol = symbol, time
        symbol = kwargs.pop('symbol', symbol)
        price = kwargs.pop('price', kwargs.pop('value', 0.0))
        super().__init__(symbol, kwargs.pop('dollar_volume', 0.0), price, kwargs.pop('volume', 0),
                         kwargs.pop('has_fundamental_data', True), time,
                         kwargs.pop('price_factor', 1.0), kwargs.pop('split_factor', 1.0),
                         kwargs.pop('end_time', None))
        if 'market_cap' in kwargs:
            self.market_cap = kwargs.pop('market_cap')
        for name in list(kwargs):
            if name in SECTION_CLASSES:
                self._set_section(name, kwargs.pop(name))
        for k, v in kwargs.items():
            setattr(self, k, v)

    @classmethod
    def _make(cls, symbol, time, price, volume, dollar_volume, has_fundamental_data, price_factor,
              split_factor, loader, ref_close=None):
        """Fast constructor used by the data provider (hundreds per selection day)."""
        obj = cls.__new__(cls)
        d = obj.__dict__
        d['symbol'] = symbol
        d['time'] = time
        d['_value'] = price
        d['dollar_volume'] = dollar_volume
        d['volume'] = volume
        d['has_fundamental_data'] = has_fundamental_data
        d['price_factor'] = price_factor
        d['split_factor'] = split_factor
        d['_end_time'] = None
        d['_loader'] = loader
        d['_ref_close'] = ref_close
        return obj

    @staticmethod
    def for_date(time, symbol):
        f = Fundamental(time, symbol)
        f.end_time = time
        return f

    ForDate = for_date


class ETFConstituentData:
    """LEAN ETFConstituentUniverse: one holding of an ETF. Missing values are None (LEAN decimal?)."""

    def __init__(self, symbol=None, weight=None, shares_held=None, market_value=None, last_update=None,
                 time=None, name=None):
        self.symbol = symbol
        self.weight = weight
        self.shares_held = shares_held
        self.market_value = market_value
        self.last_update = last_update
        self.time = time
        self.name = name
        self.period = ONE_DAY

    @property
    def end_time(self):
        return self.time + self.period if self.time is not None else None

    @property
    def value(self):
        return self.weight if self.weight is not None else 0.0

    Symbol = property(lambda self: self.symbol)
    Weight = property(lambda self: self.weight)
    SharesHeld = property(lambda self: self.shares_held)
    MarketValue = property(lambda self: self.market_value)
    LastUpdate = property(lambda self: self.last_update)

    def __repr__(self):
        return f"ETFConstituentData({self.symbol}, weight={self.weight})"


ETFConstituentUniverse = ETFConstituentData


class FundamentalUniverse:
    """LEAN's fundamental universe data type; FundamentalUniverse.usa(selector, settings)
    builds a fundamental universe (as in LEAN)."""

    @staticmethod
    def usa(selector=None, universe_settings=None):
        return FundamentalUniverseFactory(Market.USA, universe_settings, selector)

    USA = usa


# ----------------------------------------------------------------------------
# Universe settings and schedules
# ----------------------------------------------------------------------------

class Schedule:
    """UniverseSettings.schedule: schedule.on(date_rule) makes a fundamental universe select
    only on the days of the rule (LEAN Schedule)."""

    def __init__(self):
        self._on_rule = None
        self._at_rule = None

    def on(self, date_rule=None):
        self._on_rule = date_rule
        return self

    def at(self, time_rule=None):
        self._at_rule = time_rule
        return self

    @property
    def initialized(self) -> bool:
        return self._on_rule is not None

    @property
    def date_rule(self):
        return self._on_rule

    def get(self, start, end, calendar=None):
        """Days in [start, end] the rule fires on (weekdays; month/week rules need `calendar`)."""
        if self._on_rule is None:
            return []
        out, d = [], datetime(start.year, start.month, start.day)
        while d <= end:
            if d.weekday() < 5 and _rule_matches(self._on_rule, d, calendar):
                out.append(d)
            d += ONE_DAY
        return out

    def clone(self):
        s = Schedule()
        s._on_rule, s._at_rule = self._on_rule, self._at_rule
        return s

    On = on
    Initialized = initialized


_ScheduleSettings = Schedule   # name used by the earlier engine


def _rule_matches(rule, day, calendar):
    fn = getattr(rule, 'matches_day', None)
    if callable(fn):
        return bool(fn(day, calendar))
    fn = getattr(rule, 'matches', None)
    return bool(fn(day)) if callable(fn) else True


class UniverseSettings:
    """Settings of the securities a universe adds (LEAN UniverseSettings).

    UniverseSettings() - defaults; UniverseSettings(other) - copy; LEAN's positional form
    (resolution, leverage, fill_forward, extended_market_hours, minimum_time_in_universe,
    data_normalization_mode, data_mapping_mode, contract_depth_offset, asynchronous,
    selection_date_rule) and keywords are accepted."""

    _ORDER = ('resolution', 'leverage', 'fill_forward', 'extended_market_hours', 'minimum_time_in_universe',
              'data_normalization_mode', 'data_mapping_mode', 'contract_depth_offset', 'asynchronous')

    def __init__(self, *args, **kwargs):
        if args and isinstance(args[0], UniverseSettings):
            o = args[0]
            self.resolution = o.resolution
            self.leverage = o.leverage
            self.fill_forward = o.fill_forward
            self.extended_market_hours = o.extended_market_hours
            self.minimum_time_in_universe = o.minimum_time_in_universe
            self.data_normalization_mode = o.data_normalization_mode
            self.data_mapping_mode = o.data_mapping_mode
            self.contract_depth_offset = o.contract_depth_offset
            self.subscription_data_types = o.subscription_data_types
            self.asynchronous = o.asynchronous
            self.schedule = o.schedule.clone()
            return
        self.resolution = Resolution.MINUTE
        self.leverage = 0.0             # LEAN's Security.NullLeverage: each security's default leverage
        self.fill_forward = True
        self.extended_market_hours = False
        self.minimum_time_in_universe = ONE_DAY
        self.data_normalization_mode = DataNormalizationMode.ADJUSTED
        self.data_mapping_mode = 0
        self.contract_depth_offset = 0
        self.subscription_data_types = None
        self.asynchronous = False
        self.schedule = Schedule()
        for name, v in zip(self._ORDER, args):
            setattr(self, name, v)
        if len(args) > len(self._ORDER) and args[len(self._ORDER)] is not None:
            self.schedule.on(args[len(self._ORDER)])
        rule = kwargs.pop('selection_date_rule', None)
        for k, v in kwargs.items():
            setattr(self, k, v)
        if rule is not None:
            self.schedule.on(rule)

    # Kept from the earlier engine, where algorithm.universe returned the settings object:
    # self.universe.etf(...), .qc_500 and .top(n) resolve here when no algorithm is bound.
    @property
    def etf(self):
        return UniverseDefinitions(None, self).etf

    @property
    def qc_500(self):
        return UniverseDefinitions(None, self).qc_500

    @property
    def top(self):
        return UniverseDefinitions(None, self).top

    def __repr__(self):
        return (f"UniverseSettings(resolution={self.resolution!r}, leverage={self.leverage}, "
                f"minimum_time_in_universe={self.minimum_time_in_universe}, asynchronous={self.asynchronous})")


# ----------------------------------------------------------------------------
# Universes
# ----------------------------------------------------------------------------

class _Unchanged:
    """Universe.UNCHANGED: a selector returns it to keep the current selection. It is an empty
    iterable (as in LEAN) and calling it returns itself (Universe.unchanged() also works)."""

    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def __iter__(self):
        return iter(())

    def __len__(self):
        return 0

    def __call__(self):
        return self

    def __repr__(self):
        return 'Universe.UNCHANGED'


UNCHANGED = _Unchanged()


class _CountSet(set):
    @property
    def count(self):
        return len(self)

    Count = count


class _CountDict(dict):
    @property
    def count(self):
        return len(self)

    Count = count


class Member:
    """A universe member: when it was added, its security, internal flag."""

    __slots__ = ('added', 'security', 'is_internal')

    def __init__(self, added, security, is_internal=False):
        self.added = added
        self.security = security
        self.is_internal = is_internal

    Added = property(lambda self: self.added)
    Security = property(lambda self: self.security)


_counter = itertools.count(1)


def _universe_symbol(name: str) -> Symbol:
    return Symbol(name, SecurityType.BASE, Market.USA)


def _is_date_rule(x) -> bool:
    return hasattr(x, 'matches_day') or (hasattr(x, 'rule_type') and not hasattr(x, 'time'))


def _is_time_rule(x) -> bool:
    return hasattr(x, 'rule_type') and hasattr(x, 'time')


def _as_equity_symbol(x, security_type=SecurityType.EQUITY, market=Market.USA):
    if isinstance(x, Symbol):
        return x
    if isinstance(x, SecurityIdentifier):
        return Symbol.create(x.symbol, x.security_type, x.market)
    return Symbol.create(str(x).strip().upper(), security_type, market)


class Universe:
    """A universe: selects symbols; its members are the securities it currently holds in the
    algorithm (LEAN Universe). Selection is driven by universe_selection.UniverseManager."""

    UNCHANGED = UNCHANGED
    Unchanged = UNCHANGED
    unchanged = UNCHANGED      # LEAN Python accepts Universe.unchanged as the value
    data_type = None
    _kind = 'custom'

    def __init__(self, *args, **kwargs):
        settings = kwargs.get('universe_settings', kwargs.get('settings'))
        if settings is None:
            settings = next((a for a in args if isinstance(a, UniverseSettings)), None)
        self._settings = settings
        sym = kwargs.get('symbol')
        if sym is None:
            sym = next((a for a in args if isinstance(a, Symbol)), None)
        self.symbol = sym if sym is not None else _universe_symbol(f"QC-UNIVERSE-{type(self).__name__.upper()}-{next(_counter)}")
        self.securities = _CountDict()     # symbol -> Member
        self.selected = _CountSet()
        self.dispose_requested = False
        self._previous = None              # previous selection (set of keys) - LEAN PerformSelection
        self._selected_order = []          # the latest selection, in the selector's order

    # ---- settings ----
    @property
    def universe_settings(self):
        return self._settings

    @universe_settings.setter
    def universe_settings(self, v):
        self._settings = v

    settings = universe_settings

    @property
    def asynchronous(self):
        s = self.universe_settings
        return bool(getattr(s, 'asynchronous', False)) if s is not None else False

    @asynchronous.setter
    def asynchronous(self, v):
        if self.universe_settings is not None:
            self.universe_settings.asynchronous = v

    # ---- members ----
    @property
    def members(self):
        return _CountDict((m.security.symbol if getattr(m.security, 'symbol', None) is not None else k, m.security)
                          for k, m in self.securities.items())

    Members = members
    Selected = property(lambda self: self.selected)
    Symbol = property(lambda self: self.symbol)

    @property
    def security_type(self):
        return getattr(self.symbol, 'security_type', SecurityType.BASE)

    @property
    def market(self):
        return getattr(self.symbol, 'market', Market.USA)

    def contains_member(self, symbol) -> bool:
        return symbol in self.securities

    ContainsMember = contains_member

    def can_remove_member(self, utc_time, security) -> bool:
        """LEAN CanRemoveMember: after dispose, or when the member has been in the universe for
        at least universe_settings.minimum_time_in_universe (rounded to the day/hour/...)."""
        if self.dispose_requested:
            return True
        m = self.securities.get(getattr(security, 'symbol', security))
        if m is None:
            return False
        return _time_in_universe_ok(utc_time - m.added, getattr(self.universe_settings, 'minimum_time_in_universe', None))

    CanRemoveMember = can_remove_member

    def select_symbols(self, utc_time, data):
        raise NotImplementedError("Types deriving from 'Universe' must implement select_symbols(utc_time, data).")

    SelectSymbols = select_symbols

    def dispose(self):
        self.dispose_requested = True

    Dispose = dispose

    def __repr__(self):
        return f"{type(self).__name__}({self.symbol})"


def _time_in_universe_ok(elapsed: timedelta, minimum) -> bool:
    if minimum is None:
        return True
    if not isinstance(minimum, timedelta):
        try:
            minimum = timedelta(days=float(minimum))
        except (TypeError, ValueError):
            return True
    if minimum <= timedelta(0):
        return True
    for unit in (ONE_DAY, timedelta(hours=1), timedelta(minutes=1), timedelta(seconds=1)):
        if minimum >= unit:
            break
    else:
        unit = None
    if unit is not None:
        n = elapsed / unit
        elapsed = unit * float(round(n))   # round half to even, as TimeSpan.Round
    return elapsed >= minimum


class FundamentalUniverseFactory(Universe):
    """Fundamental (single-pass) universe: selector(list of Fundamental) -> symbols.
    FundamentalUniverseFactory(market, settings, selector) as in LEAN; (selector[, settings]) too."""

    _kind = 'fundamental'
    data_type = FundamentalUniverse

    def __init__(self, *args, **kwargs):
        selector = kwargs.pop('selector', None)
        market = kwargs.pop('market', None)
        rest = []
        for a in args:
            if isinstance(a, UniverseSettings) or isinstance(a, Symbol):
                rest.append(a)
            elif isinstance(a, str) and market is None:
                market = a
            elif callable(a) and selector is None:
                selector = a
        super().__init__(*rest, **kwargs)
        self.market_name = market or Market.USA
        self._selector = selector

    def select_symbols(self, utc_time, data):
        if self._selector is None:
            return [f.symbol for f in data]
        return self._selector(data)


class CoarseFundamentalUniverse(FundamentalUniverseFactory):
    """LEAN CoarseFundamentalUniverse(settings, selector): coarse selection over the same data."""

    _kind = 'coarse'


class FineFundamentalUniverse(FundamentalUniverseFactory):
    """The fine step of a coarse + fine pair (LEAN FineFundamentalUniverse(settings, selector))."""

    _kind = 'fine'


class FineFundamentalFilteredUniverse(Universe):
    """Coarse universe whose selection is filtered by a fine selector (LEAN). The fine step sees
    the Fundamental objects of the coarse-selected symbols that have fundamental data."""

    _kind = 'coarse_fine'
    data_type = FundamentalUniverse

    def __init__(self, universe, fine_selector):
        if isinstance(universe, CoarseFundamentalUniverse) and universe.asynchronous:
            raise ValueError("Asynchronous universe setting is not supported for coarse & fine selections, "
                             "please use the new Fundamental single pass selection")
        super().__init__(symbol=universe.symbol)
        self.universe = universe
        self.fine_fundamental_universe = FineFundamentalUniverse(universe.universe_settings, fine_selector,
                                                                 symbol=universe.symbol)

    @property
    def universe_settings(self):
        return self.universe.universe_settings

    @universe_settings.setter
    def universe_settings(self, v):
        self.universe.universe_settings = v
        self.fine_fundamental_universe.universe_settings = v

    settings = universe_settings

    def select_symbols(self, utc_time, data):
        return self.universe.select_symbols(utc_time, data)

    FineFundamentalUniverse = property(lambda self: self.fine_fundamental_universe)


class FundamentalFilteredUniverse(FineFundamentalFilteredUniverse):
    """A universe whose selection is filtered by a Fundamental selector (LEAN
    AddUniverse(universe, selector))."""

    _kind = 'filtered'

    def __init__(self, universe, fundamental_selector):
        Universe.__init__(self, symbol=universe.symbol)
        self.universe = universe
        self.fine_fundamental_universe = FundamentalUniverseFactory(Market.USA, universe.universe_settings,
                                                                    fundamental_selector, symbol=universe.symbol)

    FundamentalUniverse = property(lambda self: self.fine_fundamental_universe)


class ETFConstituentsUniverse(Universe):
    """ETF constituents universe: filter(list of ETFConstituentData) -> symbols
    (LEAN ETFConstituentsUniverseFactory). Constituents are the ETF's current top holdings."""

    _kind = 'etf'
    _is_etf = True                    # recognised by the earlier engine's add_universe
    data_type = ETFConstituentData

    def __init__(self, symbol, universe_settings=None, universe_filter_func=None, **kwargs):
        if callable(universe_settings) and not isinstance(universe_settings, UniverseSettings) \
                and universe_filter_func is None:
            universe_settings, universe_filter_func = None, universe_settings
        etf = _as_equity_symbol(symbol)
        super().__init__(universe_settings=universe_settings,
                         symbol=_universe_symbol(f"QC-UNIVERSE-ETF-CONSTITUENTS-{etf.value}-{next(_counter)}"))
        self.etf_symbol = etf
        self._filter_func = universe_filter_func

    @property
    def filter_function(self):
        return self._filter_func

    def select_symbols(self, utc_time, data):
        if self._filter_func is None:
            return [c.symbol for c in data]
        return self._filter_func(data)


ETFConstituentsUniverseFactory = ETFConstituentsUniverse


class UserDefinedUniverse(Universe):
    """A universe of tickers chosen by selector(datetime) (LEAN UserDefinedUniverse /
    add_universe(name, resolution, selector)). Selection runs once per trading day."""

    _kind = 'user'

    def __init__(self, *args, name=None, security_type=SecurityType.EQUITY, market=Market.USA,
                 resolution=Resolution.DAILY, universe_settings=None, selector=None, interval=None,
                 symbols=None, **kwargs):
        for a in args:
            if isinstance(a, UniverseSettings) and universe_settings is None:
                universe_settings = a
            elif isinstance(a, timedelta) and interval is None:
                interval = a
            elif callable(a) and selector is None and not isinstance(a, type):
                selector = a
            elif isinstance(a, (list, tuple, set)) and symbols is None:
                symbols = a
        name = name or f"qc-universe-userdefined-{str(market).lower()}-{getattr(security_type, 'name', security_type)}"
        # the universe's own symbol is never priced: BASE type (LEAN uses the members' type)
        super().__init__(universe_settings=universe_settings,
                         symbol=kwargs.get('symbol') or _universe_symbol(str(name)))
        self.name = name
        self.security_type_of_members = security_type
        self.market_of_members = market
        self.resolution = resolution
        self.interval = interval or ONE_DAY
        self._selector = selector
        self._fixed = [_as_equity_symbol(s, security_type, market) for s in (symbols or [])]

    def add(self, symbol):
        s = _as_equity_symbol(symbol, self.security_type_of_members, self.market_of_members)
        if s not in self._fixed:
            self._fixed.append(s)
            return True
        return False

    def remove(self, symbol):
        s = _as_equity_symbol(symbol, self.security_type_of_members, self.market_of_members)
        if s in self._fixed:
            self._fixed.remove(s)
            return True
        return False

    Add = add
    Remove = remove

    def select_symbols(self, utc_time, data=None):
        if self._selector is None:
            return list(self._fixed)
        return self._selector(utc_time)


class CustomUniverse(UserDefinedUniverse):
    """LEAN CustomUniverse (framework CustomUniverseSelectionModel)."""


class ManualUniverse(Universe):
    """A fixed list of symbols, subscribed when the universe is added (LEAN ManualUniverse)."""

    _kind = 'manual'

    def __init__(self, symbols=None, universe_settings=None, **kwargs):
        super().__init__(universe_settings=universe_settings, **kwargs)
        self.fixed_symbols = [_as_equity_symbol(s) for s in (symbols or [])]

    def select_symbols(self, utc_time, data=None):
        return list(self.fixed_symbols)


class ScheduledUniverse(Universe):
    """Selection on a date rule and time rule: selector(datetime) -> symbols (LEAN).
    ScheduledUniverse(date_rule, time_rule, selector[, settings]) or with a leading time zone."""

    _kind = 'scheduled'

    def __init__(self, *args, **kwargs):
        a = list(args)
        if a and not _is_date_rule(a[0]) and len(a) > 1 and _is_date_rule(a[1]):
            self.time_zone = a.pop(0)
        else:
            self.time_zone = kwargs.pop('time_zone', None)
        date_rule = a[0] if len(a) > 0 else kwargs.pop('date_rule', None)
        time_rule = a[1] if len(a) > 1 else kwargs.pop('time_rule', None)
        selector = a[2] if len(a) > 2 else kwargs.pop('selector', None)
        settings = a[3] if len(a) > 3 else kwargs.pop('settings', kwargs.pop('universe_settings', None))
        if not callable(selector):
            raise ValueError("Unable to create a scheduled universe selector: it is not a function. Please "
                             "provide a function that takes the firing date time and returns the selected symbols.")
        name = f"{getattr(date_rule, 'name', 'date')}_{getattr(time_rule, 'name', 'time')}-{next(_counter)}"
        super().__init__(universe_settings=settings if isinstance(settings, UniverseSettings) else None,
                         symbol=_universe_symbol(name))
        self.date_rule = date_rule
        self.time_rule = time_rule
        self._selector = selector

    def select_symbols(self, utc_time, data=None):
        return self._selector(utc_time)


class ConstituentsUniverse(Universe):
    """A universe read from constituents data files (LEAN ConstituentsUniverse). The engine has no
    such files: adding it raises NotSupportedByEngine."""

    _kind = 'constituents'

    def __init__(self, symbol=None, universe_settings=None, universe_filter_func=None, *args, **kwargs):
        if callable(universe_settings) and not isinstance(universe_settings, UniverseSettings):
            universe_settings, universe_filter_func = None, universe_settings
        super().__init__(universe_settings=universe_settings,
                         symbol=symbol if isinstance(symbol, Symbol) else None)
        self.constituents_symbol = symbol
        self._filter = universe_filter_func

    def select_symbols(self, utc_time, data):
        raise not_supported("a constituents-file universe (ConstituentsUniverse)",
                            "It reads custom universe data files the engine does not have.")


# ----------------------------------------------------------------------------
# algorithm.universe (LEAN UniverseDefinitions)
# ----------------------------------------------------------------------------

def _algo_settings(algorithm, settings):
    if settings is not None:
        return settings
    return getattr(algorithm, 'universe_settings', None) if algorithm is not None else None


class _DollarVolumeDefinitions:
    def __init__(self, defs):
        self._defs = defs

    def top(self, count, universe_settings=None):
        return self._defs.top(count, universe_settings)

    Top = top


class UniverseDefinitions:
    """Helpers behind algorithm.universe: etf(...), index(...), qc_500, top(n),
    dollar_volume.top(n), unchanged (LEAN UniverseDefinitions)."""

    def __init__(self, algorithm=None, settings=None):
        self._algorithm = algorithm
        self._settings = settings
        self.dollar_volume = _DollarVolumeDefinitions(self)

    DollarVolume = property(lambda self: self.dollar_volume)

    @property
    def unchanged(self):
        return UNCHANGED

    Unchanged = unchanged

    def _defaults(self):
        return _algo_settings(self._algorithm, self._settings)

    def etf(self, etf, *args, **kwargs):
        """etf(symbol_or_ticker[, market][, universe_settings][, universe_filter_func])."""
        market = kwargs.pop('market', None)
        settings = kwargs.pop('universe_settings', None)
        func = kwargs.pop('universe_filter_func', None)
        for a in args:
            if isinstance(a, UniverseSettings):
                settings = a
            elif isinstance(a, str):
                market = a
            elif callable(a) and not isinstance(a, type):
                func = a
        if isinstance(etf, Symbol):
            symbol = etf
        else:
            symbol = Symbol.create(str(etf).strip().upper(), SecurityType.EQUITY, market or Market.USA)
        return ETFConstituentsUniverse(symbol, settings or self._defaults(), func)

    def index(self, index, *args, **kwargs):
        raise not_supported(f"index constituents universes ({index})",
                            "Only ETF constituents (current top holdings) are available: use universe.etf().")

    @property
    def qc_500(self):
        return self.etf(Symbol.create("SPY", SecurityType.EQUITY, Market.USA))

    def top(self, count, universe_settings=None):
        n = int(count)

        def select(fundamental):
            return [f.symbol for f in sorted(fundamental, key=lambda f: f.dollar_volume, reverse=True)[:n]]

        return FundamentalUniverseFactory(Market.USA, universe_settings or self._defaults(), select)

    @property
    def constituent(self):
        raise not_supported("constituent universe definitions (custom constituents data files)")

    ETF = etf
    Index = index
    QC500 = qc_500
    Top = top
