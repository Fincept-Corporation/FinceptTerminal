"""
FAOSTAT API Data Fetcher
Wrapper for the UN Food and Agriculture Organization (FAO) FAOSTAT database.
Covers crop production, trade, food security, emissions, prices, land use, and more.

Base URL:  https://fenixservices.fao.org/faostat/api/v1/en/
Auth:      None required - fully public API
Bulk CDN:  https://bulks-faostat.fao.org/production/datasets_E.json

Key domain codes:
  QCL  - Crops and Livestock Products
  TCL  - Trade: Crops and Livestock
  TM   - Bilateral Trade Matrix
  FBS  - Food Balances (2010+)
  FS   - Food Security Indicators
  RL   - Land Use
  RFN  - Fertilizers by Nutrient
  GLE  - GHG Emissions: Land Use
  GV   - GHG Emissions: Livestock
  PP   - Producer Prices
  CP   - Consumer Price Indices
  OA   - Population
  OEA  - Employment in Agriculture
  FO   - Forestry
  SDGB - SDG Indicators
"""

import sys
import json
import requests
from datetime import datetime, timezone
from typing import Dict, Any, Optional, List

BASE_URL = "https://fenixservices.fao.org/faostat/api/v1/en"
BULK_URL = "https://bulks-faostat.fao.org/production/datasets_E.json"

DOMAIN_CODES = {
    "QCL": "Crops and Livestock Products",
    "QI":  "Production Indices",
    "QV":  "Value of Agricultural Production",
    "TCL": "Trade: Crops and Livestock",
    "TM":  "Bilateral Trade Matrix",
    "FBS": "Food Balances (2010+)",
    "FBSH":"Food Balances (historical)",
    "SCL": "Supply Utilization Accounts",
    "FS":  "Food Security Indicators",
    "CAHD":"Cost and Affordability of Healthy Diets",
    "RL":  "Land Use",
    "RFN": "Fertilizers by Nutrient",
    "EP":  "Pesticides Use",
    "GLE": "GHG Emissions: Land Use",
    "GV":  "GHG Emissions: Livestock",
    "GCE": "GHG Emissions: Crop Residues",
    "GN":  "GHG Emissions: Synthetic Fertilizers",
    "EI":  "Emissions Intensities",
    "FO":  "Forestry Production and Trade",
    "PP":  "Producer Prices",
    "PA":  "Producer Prices (archive)",
    "CP":  "Consumer Price Indices",
    "OA":  "Population",
    "OEA": "Employment in Agriculture",
    "FA":  "Food Aid Shipments (WFP)",
    "MK":  "Macro-Statistics (GDP)",
    "CS":  "Credit to Agriculture",
    "IG":  "Government Investment in Agriculture",
    "SDGB":"SDG Indicators",
    "WCAD":"World Census of Agriculture",
}


class FAOSTATWrapper:
    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({"User-Agent": "Fincept-Terminal/1.0", "Accept": "application/json"})

    def _get(self, path: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        try:
            p = {k: v for k, v in (params or {}).items() if v is not None}
            p.setdefault("output_type", "objects")
            r = self.session.get(f"{BASE_URL}/{path}", params=p, timeout=45)
            r.raise_for_status()
            data = r.json()
            records = data.get("data", data) if isinstance(data, dict) else data
            return {
                "success": True, "endpoint": path, "data": records,
                "count": len(records) if isinstance(records, list) else None,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            return {"success": False, "error": f"HTTP {e.response.status_code}: {e.response.text[:200]}", "endpoint": path, "status_code": e.response.status_code}
        except Exception as e:
            return {"success": False, "error": str(e), "endpoint": path}

    def _fmt(self, v) -> Optional[str]:
        if v is None:
            return None
        return ",".join(str(x) for x in v) if isinstance(v, list) else str(v)

    def get_data(self, domain: str, areas=None, items=None, elements=None, years=None,
                 area_cs: str = "ISO3", limit: int = 10000, show_codes: bool = True) -> Dict[str, Any]:
        """Generic data query for any FAOSTAT domain."""
        result = self._get(f"data/{domain}", {
            "area": self._fmt(areas), "item": self._fmt(items),
            "element": self._fmt(elements), "year": self._fmt(years),
            "area_cs": area_cs, "limit": limit,
            "show_codes": 1 if show_codes else 0,
            "show_unit": 1, "null_values": 0,
        })
        if result.get("success"):
            result["domain"] = domain
            result["domain_name"] = DOMAIN_CODES.get(domain, domain)
        return result

    # --- Metadata ---
    def list_domains(self):
        r = self._get("groupsanddomains")
        r["domain_reference"] = DOMAIN_CODES
        return r

    def list_areas(self, domain: str):
        return self._get(f"area/{domain}")

    def list_items(self, domain: str):
        return self._get(f"item/{domain}")

    def list_elements(self, domain: str):
        return self._get(f"element/{domain}")

    def list_years(self, domain: str):
        return self._get(f"year/{domain}")

    def list_all_datasets(self):
        try:
            r = self.session.get(BULK_URL, timeout=30)
            r.raise_for_status()
            d = r.json()
            return {"success": True, "data": d, "count": len(d) if isinstance(d, list) else None,
                    "timestamp": int(datetime.now(timezone.utc).timestamp())}
        except Exception as e:
            return {"success": False, "error": str(e)}

    # --- Production ---
    def get_crop_production(self, countries=None, items=None, years=None, elements=None, limit=5000):
        """Crop and livestock production (QCL): Production(t), Area harvested(ha), Yield(hg/ha)."""
        return self.get_data("QCL", areas=countries, items=items,
                             elements=elements or [5510, 5312, 5419], years=years, limit=limit)

    def get_production_indices(self, countries=None, years=None):
        """Production indices base 2014-2016=100."""
        return self.get_data("QI", areas=countries, years=years)

    def get_production_value(self, countries=None, years=None):
        """Gross production value in USD."""
        return self.get_data("QV", areas=countries, years=years)

    # --- Trade ---
    def get_trade_data(self, countries=None, items=None, years=None, elements=None, limit=5000):
        """Export/import quantity and value (TCL)."""
        return self.get_data("TCL", areas=countries, items=items,
                             elements=elements or [5610, 5622, 5910, 5922], years=years, limit=limit)

    def get_bilateral_trade(self, reporter=None, years=None, items=None, limit=5000):
        """Bilateral trade matrix (TM) - reporter to all partners."""
        return self.get_data("TM", areas=[reporter] if reporter else None,
                             items=items, years=years, limit=limit)

    # --- Food Supply & Security ---
    def get_food_balances(self, countries=None, items=None, years=None, limit=5000):
        """Food Balance Sheets (FBS) - supply, utilization, per-capita availability."""
        return self.get_data("FBS", areas=countries, items=items, years=years, limit=limit)

    def get_food_security_indicators(self, countries=None, years=None):
        """Suite of Food Security Indicators (FS) - undernourishment, food insecurity."""
        return self.get_data("FS", areas=countries, years=years)

    def get_diet_cost_affordability(self, countries=None, years=None):
        """Cost and Affordability of a Healthy Diet (CAHD)."""
        return self.get_data("CAHD", areas=countries, years=years)

    def get_supply_utilization(self, countries=None, items=None, years=None, limit=5000):
        """Supply Utilization Accounts (SCL)."""
        return self.get_data("SCL", areas=countries, items=items, years=years, limit=limit)

    # --- Land & Inputs ---
    def get_land_use(self, countries=None, years=None):
        """Land use (RL) - agricultural area, arable land, permanent crops."""
        return self.get_data("RL", areas=countries, years=years)

    def get_fertilizer_data(self, countries=None, years=None):
        """Fertilizers by Nutrient (RFN) - N, P2O5, K2O use."""
        return self.get_data("RFN", areas=countries, years=years)

    def get_pesticide_data(self, countries=None, years=None):
        """Pesticides use (EP)."""
        return self.get_data("EP", areas=countries, years=years)

    # --- Emissions ---
    def get_emissions_land_use(self, countries=None, years=None, items=None, limit=5000):
        """GHG Emissions from land use change (GLE) - CO2, N2O, CH4."""
        return self.get_data("GLE", areas=countries, items=items, years=years, limit=limit)

    def get_emissions_livestock(self, countries=None, years=None):
        """GHG Emissions from livestock (GV) - enteric fermentation, manure."""
        return self.get_data("GV", areas=countries, years=years)

    def get_emissions_crops(self, countries=None, years=None):
        """GHG Emissions from crop residues (GCE)."""
        return self.get_data("GCE", areas=countries, years=years)

    def get_emission_intensities(self, countries=None, years=None):
        """Emissions intensities (EI) - GHG per tonne of product."""
        return self.get_data("EI", areas=countries, years=years)

    # --- Forestry, Prices, Population ---
    def get_forestry_data(self, countries=None, years=None, limit=5000):
        """Forestry production and trade (FO) - roundwood, sawnwood, paper."""
        return self.get_data("FO", areas=countries, years=years, limit=limit)

    def get_producer_prices(self, countries=None, items=None, years=None, limit=5000):
        """Annual producer prices (PP) - local currency and USD per tonne."""
        return self.get_data("PP", areas=countries, items=items, years=years, limit=limit)

    def get_consumer_price_index(self, countries=None, years=None):
        """Consumer Price Indices (CP) - food CPI, general CPI."""
        return self.get_data("CP", areas=countries, years=years)

    def get_population(self, countries=None, years=None):
        """Population (OA) - total, rural, urban, agricultural."""
        return self.get_data("OA", areas=countries, years=years)

    def get_employment_in_agriculture(self, countries=None, years=None):
        """Employment in agriculture (OEA) - by sex, share of total."""
        return self.get_data("OEA", areas=countries, years=years)

    # --- Macro / Aid / SDG ---
    def get_macro_statistics(self, countries=None, years=None):
        return self.get_data("MK", areas=countries, years=years)

    def get_credit_to_agriculture(self, countries=None, years=None):
        return self.get_data("CS", areas=countries, years=years)

    def get_government_investment(self, countries=None, years=None):
        return self.get_data("IG", areas=countries, years=years)

    def get_food_aid_shipments(self, countries=None, years=None):
        return self.get_data("FA", areas=countries, years=years)

    def get_sdg_indicators(self, countries=None, years=None, items=None, limit=5000):
        return self.get_data("SDGB", areas=countries, items=items, years=years, limit=limit)

    # --- Convenience ---
    def get_global_crop_summary(self, year: int, item_code: int = 56):
        """Global production for one crop. Default 56=Maize. Others: 15=Wheat,27=Rice,236=Soybeans."""
        return self.get_data("QCL", items=[item_code], elements=[5510], years=[year], limit=5000)

    def get_country_agri_profile(self, country_iso3: str, year: int):
        """Full agricultural profile combining production, trade, food balance, land use."""
        c, y = [country_iso3], [year]
        return {
            "success": True, "country": country_iso3, "year": year,
            "production":   self.get_crop_production(countries=c, years=y, limit=2000).get("data", []),
            "trade":        self.get_trade_data(countries=c, years=y, limit=2000).get("data", []),
            "food_balance": self.get_food_balances(countries=c, years=y, limit=1000).get("data", []),
            "land_use":     self.get_land_use(countries=c, years=y).get("data", []),
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_agri_emissions_summary(self, countries=None, years=None):
        """Combined agricultural GHG emissions across land use, livestock, and crops."""
        return {
            "success": True,
            "data": {
                "land_use":  self.get_emissions_land_use(countries=countries, years=years, limit=2000).get("data", []),
                "livestock": self.get_emissions_livestock(countries=countries, years=years).get("data", []),
                "crops":     self.get_emissions_crops(countries=countries, years=years).get("data", []),
            },
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


def _a(n, d=None):
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d

def _ls(n):
    v = _a(n)
    return v.split(",") if v else None

def _li(n):
    v = _a(n)
    return [int(x) for x in v.split(",")] if v else None


def main():
    w = FAOSTATWrapper()
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False, "error": "No command",
            "available_commands": [
                "list_domains", "list_datasets",
                "list_areas <domain>", "list_items <domain>",
                "list_elements <domain>", "list_years <domain>",
                "get_data <domain> [areas_csv] [years_csv] [items_csv] [elements_csv] [limit]",
                "crop_production [countries_csv] [years_csv] [items_csv]",
                "production_indices [countries_csv] [years_csv]",
                "production_value [countries_csv] [years_csv]",
                "trade_data [countries_csv] [years_csv] [items_csv]",
                "bilateral_trade [reporter_iso3] [years_csv]",
                "food_balances [countries_csv] [years_csv]",
                "food_security [countries_csv] [years_csv]",
                "diet_cost [countries_csv] [years_csv]",
                "land_use [countries_csv] [years_csv]",
                "fertilizers [countries_csv] [years_csv]",
                "pesticides [countries_csv] [years_csv]",
                "emissions_land [countries_csv] [years_csv]",
                "emissions_livestock [countries_csv] [years_csv]",
                "emissions_crops [countries_csv] [years_csv]",
                "emission_intensities [countries_csv] [years_csv]",
                "forestry [countries_csv] [years_csv]",
                "producer_prices [countries_csv] [years_csv] [items_csv]",
                "consumer_prices [countries_csv] [years_csv]",
                "population [countries_csv] [years_csv]",
                "employment_agri [countries_csv] [years_csv]",
                "macro_stats [countries_csv] [years_csv]",
                "food_aid [countries_csv] [years_csv]",
                "sdg_indicators [countries_csv] [years_csv]",
                "global_crop <year> [item_code]",
                "country_profile <iso3> <year>",
                "agri_emissions [countries_csv] [years_csv]",
            ],
            "domain_codes": DOMAIN_CODES,
        }, indent=2))
        sys.exit(1)

    cmd = sys.argv[1]
    try:
        dispatch = {
            "list_domains":         lambda: w.list_domains(),
            "list_datasets":        lambda: w.list_all_datasets(),
            "list_areas":           lambda: w.list_areas(_a(2, "QCL")),
            "list_items":           lambda: w.list_items(_a(2, "QCL")),
            "list_elements":        lambda: w.list_elements(_a(2, "QCL")),
            "list_years":           lambda: w.list_years(_a(2, "QCL")),
            "get_data":             lambda: w.get_data(_a(2), _ls(3), _ls(4), _ls(5), _ls(6), limit=int(_a(7, 10000))),
            "crop_production":      lambda: w.get_crop_production(_ls(2), _ls(3), _li(4)),
            "production_indices":   lambda: w.get_production_indices(_ls(2), _li(3)),
            "production_value":     lambda: w.get_production_value(_ls(2), _li(3)),
            "trade_data":           lambda: w.get_trade_data(_ls(2), _ls(3), _li(4)),
            "bilateral_trade":      lambda: w.get_bilateral_trade(_a(2), _li(3), _ls(4)),
            "food_balances":        lambda: w.get_food_balances(_ls(2), _ls(3), _li(4)),
            "food_security":        lambda: w.get_food_security_indicators(_ls(2), _li(3)),
            "diet_cost":            lambda: w.get_diet_cost_affordability(_ls(2), _li(3)),
            "land_use":             lambda: w.get_land_use(_ls(2), _li(3)),
            "fertilizers":          lambda: w.get_fertilizer_data(_ls(2), _li(3)),
            "pesticides":           lambda: w.get_pesticide_data(_ls(2), _li(3)),
            "emissions_land":       lambda: w.get_emissions_land_use(_ls(2), _li(3)),
            "emissions_livestock":  lambda: w.get_emissions_livestock(_ls(2), _li(3)),
            "emissions_crops":      lambda: w.get_emissions_crops(_ls(2), _li(3)),
            "emission_intensities": lambda: w.get_emission_intensities(_ls(2), _li(3)),
            "forestry":             lambda: w.get_forestry_data(_ls(2), _li(3)),
            "producer_prices":      lambda: w.get_producer_prices(_ls(2), _ls(3), _li(4)),
            "consumer_prices":      lambda: w.get_consumer_price_index(_ls(2), _li(3)),
            "population":           lambda: w.get_population(_ls(2), _li(3)),
            "employment_agri":      lambda: w.get_employment_in_agriculture(_ls(2), _li(3)),
            "macro_stats":          lambda: w.get_macro_statistics(_ls(2), _li(3)),
            "food_aid":             lambda: w.get_food_aid_shipments(_ls(2), _li(3)),
            "sdg_indicators":       lambda: w.get_sdg_indicators(_ls(2), _li(3)),
            "global_crop":          lambda: w.get_global_crop_summary(int(_a(2, 2022)), int(_a(3, 56))),
            "country_profile":      lambda: w.get_country_agri_profile(_a(2), int(_a(3, 2022))),
            "agri_emissions":       lambda: w.get_agri_emissions_summary(_ls(2), _li(3)),
        }
        if cmd not in dispatch:
            print(json.dumps({"success": False, "error": f"Unknown command: {cmd}"}))
            sys.exit(1)
        print(json.dumps(dispatch[cmd](), indent=2))
    except KeyboardInterrupt:
        print(json.dumps({"success": False, "error": "Cancelled"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
