import logging
import pandas as pd

def preprocess_data(raw_data: pd.DataFrame) -> pd.DataFrame:
    logging.getLogger(__name__).info("Preprocessing data")
    d = raw_data.copy()
    d = d.fillna(method="ffill").fillna(method="bfill")
    d = d[d["Close"] > 0]
    return d
