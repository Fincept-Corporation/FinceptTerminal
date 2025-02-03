import logging
import pandas as pd

def preprocess_data(raw_data: pd.DataFrame) -> pd.DataFrame:
    logger = logging.getLogger(__name__)
    logger.info("Preprocessing data")

    d = raw_data.copy()
    d.fillna(method="ffill", inplace=True)
    d.fillna(method="bfill", inplace=True)

    if "Close" in d.columns:
        d = d[d["Close"] > 0]
    else:
        logger.warning("DataFrame missing 'Close' column. Check your column names.")

    d.sort_index(inplace=True)
    return d
