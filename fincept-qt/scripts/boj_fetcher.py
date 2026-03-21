import sys
import json
import warnings
from urllib.parse import urlencode
from datetime import datetime

import numpy as np
import pandas as pd
import requests
from bs4 import BeautifulSoup


warnings.filterwarnings("ignore", category=UserWarning)


class BOJWrapper:

    BASE_URL = "https://www.stat-search.boj.or.jp/ssi/"
    SEARCH_PATH = (
        "cgi-bin/famecgi2?"
        "cgi=%24nme_r030_en&"
        "chkfrq=MM&"
        "rdoheader=SIMPLE&"
        "rdodelimitar=COMMA&"
        "hdnYyyyFrom=&"
        "hdnYyyyTo=&"
        "sw_freq=NONE&"
        "sw_yearend=NONE&"
        "sw_observed=NONE&"
    )

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Mozilla/5.0"
        })

    def _fetch_csv_url(self, series_code: str) -> str:
        """
        Step 1: Call FAME CGI endpoint
        Step 2: Parse returned HTML
        Step 3: Extract CSV download link
        """
        series_encoded = urlencode({"hdncode": series_code})
        url = f"{self.BASE_URL}{self.SEARCH_PATH}{series_encoded}"

        response = self.session.get(url)
        response.raise_for_status()

        soup = BeautifulSoup(response.content, "lxml")
        nodes = soup.select("a[href*=csv]")

        if not nodes:
            raise ValueError(f"Could not find CSV file for series: {series_code}")

        csv_url = f"https://www.stat-search.boj.or.jp/{nodes[0]['href']}"
        return csv_url

    def _fetch_dataframe(self, series_code: str, skiprows: int = 0) -> pd.DataFrame:
        """
        Replicates bojpy.get_data_series logic
        """
        csv_url = self._fetch_csv_url(series_code)

        df = pd.read_csv(csv_url, skiprows=skiprows)

        # First row contains column suffix
        first_row = df.iloc[0]
        df.columns = df.columns + " " + first_row
        df = df.drop(index=0)

        # Convert date column
        df[df.columns[0]] = pd.to_datetime(df[df.columns[0]])
        df = df.replace({"ND": np.nan}, regex=True)

        # Rename first column
        df = df.rename(columns={df.columns[0]: ""})
        df = df.rename_axis("Date", axis=1)
        df = df.set_index(df.columns[0])

        df = df.astype(float)

        # Sort descending (newest first)
        if df.index.is_monotonic_increasing:
            df = df.sort_index(ascending=False)

        df = df.dropna()

        return df

    def get_series(self, series_code, start_date=None, end_date=None, max_records=5000):
        try:
            df = self._fetch_dataframe(series_code)

            # Apply date filters
            if start_date:
                start = pd.to_datetime(start_date)
                df = df[df.index >= start]

            if end_date:
                end = pd.to_datetime(end_date)
                df = df[df.index <= end]

            df = df.head(max_records)

            internal_title = df.columns[0] if len(df.columns) > 0 else series_code

            data = []
            for idx, row in df.iterrows():
                data.append({
                    "Date": idx.strftime("%Y-%m-%d"),
                    "Value": row.iloc[0]
                })

            return {
                "success": True,
                "series_code": series_code,
                "internal_title": internal_title,
                "total_fetched": len(data),
                "cap_reached": len(data) >= max_records,
                "data": data,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "series_code": series_code
            }


def main():
    if len(sys.argv) < 3:
        print(json.dumps({
            "success": False,
            "error": "Usage: series <series_code> [start_date] [end_date]"
        }))
        return

    command = sys.argv[1]
    series_code = sys.argv[2]
    start_date = sys.argv[3] if len(sys.argv) > 3 else None
    end_date = sys.argv[4] if len(sys.argv) > 4 else None

    wrapper = BOJWrapper()

    if command == "series":
        result = wrapper.get_series(series_code, start_date, end_date)
        print(json.dumps(result, ensure_ascii=False))
    else:
        print(json.dumps({
            "success": False,
            "error": "Unknown command"
        }))


if __name__ == "__main__":
    main()
