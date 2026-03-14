"""
VisionQuant Pattern Search Engine
Core pattern recognition engine adapted from VisionQuant-Pro for Fincept Terminal.

CLI Protocol:
    python engine.py search '{"symbol":"AAPL","date":"20250115","top_k":10}'
    python engine.py status '{}'
    python engine.py encode '{"image_path":"/path/to/img.png"}'
"""

import os
import sys
import json
import bisect
import numpy as np
import pandas as pd
from datetime import datetime

os.environ["KMP_DUPLICATE_LIB_OK"] = "TRUE"
os.environ["OMP_NUM_THREADS"] = "1"

# Add parent for relative imports
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from utils import (
    get_data_dir, get_model_path, get_index_path, get_meta_path,
    fetch_ohlcv, generate_kline_image, ensure_dirs,
    json_response, output_json, parse_args,
)


# ---------------------------------------------------------------------------
# Helper functions for DTW and shape features
# ---------------------------------------------------------------------------

def _dtw_distance(s1, s2, window=5):
    """Fast DTW with Sakoe-Chiba band constraint. Returns normalized distance in [0, inf)."""
    try:
        n, m = len(s1), len(s2)
        if n == 0 or m == 0:
            return float("inf")
        s1 = np.asarray(s1, dtype=float)
        s2 = np.asarray(s2, dtype=float)
        s1 = (s1 - s1.min()) / (s1.max() - s1.min() + 1e-8)
        s2 = (s2 - s2.min()) / (s2.max() - s2.min() + 1e-8)

        dtw = np.full((n + 1, m + 1), float("inf"))
        dtw[0, 0] = 0
        for i in range(1, n + 1):
            j_start = max(1, i - window)
            j_end = min(m + 1, i + window + 1)
            for j in range(j_start, j_end):
                cost = abs(s1[i - 1] - s2[j - 1])
                dtw[i, j] = cost + min(dtw[i - 1, j], dtw[i, j - 1], dtw[i - 1, j - 1])
        return dtw[n, m] / max(n, m)
    except Exception:
        return float("inf")


def _extract_kline_features(prices):
    """
    Extract 8-dim shape feature vector:
    [trend, change, volatility, high_pos, low_pos, head_trend, mid_trend, tail_trend]
    """
    try:
        if len(prices) < 6:
            return None
        prices = np.asarray(prices, dtype=float)
        n = len(prices)

        trend = 1.0 if prices[-1] > prices[0] else -1.0
        change = (prices[-1] - prices[0]) / (prices[0] + 1e-8)
        change_norm = float(np.tanh(change * 10))

        returns = np.diff(prices) / (prices[:-1] + 1e-8)
        vol = float(np.std(returns) * np.sqrt(252))
        vol_norm = min(vol / 0.5, 1.0)

        high_pos = float(np.argmax(prices) / (n - 1))
        low_pos = float(np.argmin(prices) / (n - 1))

        seg = n // 3
        head = (prices[seg] - prices[0]) / (prices[0] + 1e-8)
        mid = (prices[2 * seg] - prices[seg]) / (prices[seg] + 1e-8)
        tail = (prices[-1] - prices[2 * seg]) / (prices[2 * seg] + 1e-8)

        return np.array([
            trend, change_norm, vol_norm,
            high_pos, low_pos,
            float(np.tanh(head * 10)),
            float(np.tanh(mid * 10)),
            float(np.tanh(tail * 10)),
        ])
    except Exception:
        return None


# ---------------------------------------------------------------------------
# VisionEngine - adapted for global markets via yfinance
# ---------------------------------------------------------------------------

class VisionEngine:
    def __init__(self):
        import torch
        from torchvision import transforms

        self.device = torch.device("cpu")
        self.model = None
        self.model_loaded = False

        self.preprocess = transforms.Compose([
            transforms.Resize((224, 224)),
            transforms.ToTensor(),
        ])

        self.index = None
        self.meta_data = []
        self._path_map = {}
        self._symbol_dates = {}

        self._try_load_model()

    def _try_load_model(self):
        import torch
        model_path = get_model_path()
        if not os.path.exists(model_path):
            return False
        try:
            from models.attention_cae import AttentionCAE
            self.model = AttentionCAE(latent_dim=1024, num_attention_heads=8).to(self.device)
            state_dict = torch.load(model_path, map_location=self.device, weights_only=True)
            self.model.load_state_dict(state_dict)
            self.model.eval()
            self.model_loaded = True
            return True
        except Exception as e:
            self.model_loaded = False
            return False

    def reload_index(self):
        import faiss
        index_file = get_index_path()
        meta_file = get_meta_path()

        if not os.path.exists(index_file):
            return False
        try:
            self.index = faiss.read_index(index_file)
        except Exception:
            return False

        if os.path.exists(meta_file):
            df = pd.read_csv(meta_file, dtype=str, engine="c", low_memory=False)
            self.meta_data = df.to_dict("records")
        else:
            return False

        self._build_path_index()
        return True

    def _build_path_index(self):
        self._path_map = {}
        self._symbol_dates = {}
        for info in self.meta_data:
            sym = str(info.get("symbol", ""))
            date_str = str(info.get("date", "")).replace("-", "")
            path = info.get("path")
            if not sym or not date_str or not path:
                continue
            self._path_map[(sym, date_str)] = path
            try:
                self._symbol_dates.setdefault(sym, []).append(int(date_str))
            except Exception:
                continue
        for dates in self._symbol_dates.values():
            dates.sort()

    def find_image_path(self, symbol, date_str, allow_nearest=True):
        date_n = str(date_str).replace("-", "")
        path = self._path_map.get((symbol, date_n))
        if path and os.path.exists(path):
            return path
        if allow_nearest:
            dates = self._symbol_dates.get(symbol)
            try:
                target = int(date_n)
            except Exception:
                return None
            if dates:
                idx = bisect.bisect_right(dates, target) - 1
                if idx >= 0:
                    nearest = f"{dates[idx]:08d}"
                    path = self._path_map.get((symbol, nearest))
                    if path and os.path.exists(path):
                        return path
        return None

    def _image_to_vector(self, img_path):
        import torch
        from PIL import Image
        try:
            with Image.open(img_path) as img:
                img = img.convert("RGB")
                tensor = self.preprocess(img).unsqueeze(0).to(self.device)
            with torch.no_grad():
                feature = self.model.encode(tensor)
                return feature.cpu().numpy().flatten()
        except Exception:
            return None

    def _vector_score_to_similarity(self, score):
        import faiss as _faiss
        try:
            if self.index is not None and self.index.metric_type == _faiss.METRIC_INNER_PRODUCT:
                sim = (float(score) + 1.0) / 2.0
            else:
                sim = 1.0 / (1.0 + max(float(score), 0.0))
            return float(np.clip(sim, 0.0, 1.0))
        except Exception:
            return 0.0

    def _parse_date(self, date_str):
        for fmt in ("%Y%m%d", "%Y-%m-%d"):
            try:
                return datetime.strptime(str(date_str), fmt)
            except Exception:
                continue
        return None

    # ------------------------------------------------------------------
    # Core search
    # ------------------------------------------------------------------

    def search_similar_patterns(self, symbol, date_str=None, top_k=10, lookback=60, config=None):
        """
        Search for similar K-line patterns.

        1. Fetch OHLCV via yfinance
        2. Generate candlestick image
        3. Encode to 1024-dim vector
        4. FAISS coarse search -> 2000 candidates
        5. DTW + correlation + shape feature re-ranking
        6. Triple Barrier labeling of each match
        7. Return top-K results with win-rate stats

        Args:
            config: dict with optional keys: isolation_days, dtw_window,
                    w_dtw, w_corr, w_shape, w_visual, trend_bonus,
                    faiss_multiplier, hq_dtw_threshold,
                    tb_upper, tb_lower, tb_max_hold

        Returns dict ready for JSON serialization.
        """
        import faiss as _faiss
        cfg = config or {}

        # Extract configurable constants
        isolation_days = int(cfg.get("isolation_days", 20))
        dtw_window = int(cfg.get("dtw_window", 5))
        w_dtw = float(cfg.get("w_dtw", 0.50))
        w_corr = float(cfg.get("w_corr", 0.30))
        w_shape = float(cfg.get("w_shape", 0.15))
        w_visual = float(cfg.get("w_visual", 0.05))
        trend_bonus = float(cfg.get("trend_bonus", 0.08))
        faiss_multiplier = int(cfg.get("faiss_multiplier", 200))
        hq_dtw_threshold = float(cfg.get("hq_dtw_threshold", 0.8))
        tb_upper = float(cfg.get("tb_upper", 0.05))
        tb_lower = float(cfg.get("tb_lower", 0.03))
        tb_max_hold = int(cfg.get("tb_max_hold", 20))

        if self.index is None:
            if not self.reload_index():
                return json_response("error", error="FAISS index not loaded. Run setup first.")

        if not self.model_loaded:
            if not self._try_load_model():
                return json_response("error", error="Model not loaded. Run setup first.")

        # 1. Fetch price data
        df = fetch_ohlcv(symbol, period="1y")
        if df is None or len(df) < lookback + 1:
            return json_response("error", error=f"Insufficient data for {symbol}")

        # Determine target date slice
        if date_str:
            target_dt = pd.to_datetime(date_str)
            mask = df.index <= target_dt
            df_slice = df[mask]
        else:
            df_slice = df

        if len(df_slice) < lookback:
            return json_response("error", error="Not enough historical bars before target date")

        query_df = df_slice.tail(lookback)
        query_prices = query_df["Close"].values

        # 2. Generate query image
        data_dir = ensure_dirs()
        img_dir = os.path.join(data_dir, "images", "query")
        os.makedirs(img_dir, exist_ok=True)
        img_path = os.path.join(img_dir, f"{symbol}_{date_str or 'latest'}.png")
        generate_kline_image(query_df, img_path, window=lookback)

        # 3. Encode to vector
        vec = self._image_to_vector(img_path)
        if vec is None:
            return json_response("error", error="Failed to encode query image")

        vec = vec.astype("float32").reshape(1, -1)
        _faiss.normalize_L2(vec)

        # 4. FAISS coarse search
        search_k = max(top_k * faiss_multiplier, 2000)
        D, I = self.index.search(vec, search_k)

        # 5. Re-rank with DTW + correlation + shape features
        query_20 = query_prices[-20:] if len(query_prices) >= 20 else query_prices
        query_feat = _extract_kline_features(query_20)
        query_trend = 1 if query_20[-1] > query_20[0] else -1

        candidates = []
        seen_dates = {}
        price_cache = {}

        for vector_score, idx in zip(D[0], I[0]):
            if idx >= len(self.meta_data):
                continue
            info = self.meta_data[idx]
            sym = str(info.get("symbol", ""))
            d_str = str(info.get("date", ""))
            dt = self._parse_date(d_str)
            if dt is None:
                continue

            # Time isolation
            conflict = False
            if sym in seen_dates:
                for ed in seen_dates[sym]:
                    if abs((dt - ed).days) < isolation_days:
                        conflict = True
                        break
            if conflict:
                continue

            # Compute price-based metrics
            dtw_sim = None
            corr_norm = 0.5
            feature_sim = 0.5
            trend_match = None
            match_prices = None

            try:
                if sym not in price_cache:
                    price_cache[sym] = fetch_ohlcv(sym, period="5y")
                mdf = price_cache[sym]
                if mdf is not None and not mdf.empty:
                    mdf_idx = pd.to_datetime(mdf.index)
                    closest = mdf_idx[mdf_idx <= dt]
                    if len(closest) >= 20:
                        loc = len(closest) - 1
                        match_prices = mdf.iloc[loc - 19:loc + 1]["Close"].values
                        if len(match_prices) == 20:
                            # DTW
                            dtw_dist = _dtw_distance(query_20, match_prices, window=dtw_window)
                            dtw_sim = max(0, 1.0 - dtw_dist)
                            # Correlation
                            qn = (query_20 - query_20.mean()) / (query_20.std() + 1e-8)
                            mn = (match_prices - match_prices.mean()) / (match_prices.std() + 1e-8)
                            corr = np.corrcoef(qn, mn)[0, 1]
                            if not np.isnan(corr):
                                corr_norm = (float(corr) + 1.0) / 2.0
                            # Shape features
                            m_feat = _extract_kline_features(match_prices)
                            if query_feat is not None and m_feat is not None:
                                cos = np.dot(query_feat, m_feat) / (
                                    np.linalg.norm(query_feat) * np.linalg.norm(m_feat) + 1e-8
                                )
                                feature_sim = (cos + 1.0) / 2.0
                                trend_match = 1 if m_feat[0] == query_trend else 0
            except Exception:
                pass

            sim_score = self._vector_score_to_similarity(vector_score)

            # Weighted score using configurable weights
            if dtw_sim is not None:
                combined = w_dtw * dtw_sim + w_corr * corr_norm + w_shape * feature_sim + w_visual * sim_score
            else:
                combined = sim_score * 0.3

            # Trend bonus/penalty
            if trend_match == 1:
                combined += trend_bonus
            elif trend_match == 0:
                combined -= trend_bonus

            candidates.append({
                "symbol": sym,
                "date": d_str,
                "score": round(float(combined), 4),
                "dtw_sim": round(float(dtw_sim), 4) if dtw_sim is not None else None,
                "correlation": round(float(corr_norm * 2 - 1), 4) if corr_norm != 0.5 else None,
                "feature_sim": round(float(feature_sim), 4),
                "visual_sim": round(float(sim_score), 4),
                "trend_match": trend_match,
                "path": info.get("path"),
            })

            seen_dates.setdefault(sym, []).append(dt)

            # Early exit if enough high-quality candidates
            hq = [c for c in candidates if c.get("dtw_sim") is not None and c["dtw_sim"] > hq_dtw_threshold]
            if len(hq) >= top_k * 3:
                break

        candidates.sort(key=lambda x: x["score"], reverse=True)
        top_matches = candidates[:top_k]

        # 6. Triple Barrier labeling for each match
        from models.triple_barrier import TripleBarrierLabeler, calculate_win_loss_ratio
        labeler = TripleBarrierLabeler(upper_barrier=tb_upper, lower_barrier=tb_lower, max_holding_period=tb_max_hold)

        tb_results = []
        for m in top_matches:
            try:
                sym = m["symbol"]
                if sym not in price_cache:
                    price_cache[sym] = fetch_ohlcv(sym, period="5y")
                mdf = price_cache[sym]
                if mdf is None or mdf.empty:
                    continue
                mdt = self._parse_date(m["date"])
                if mdt is None:
                    continue
                mdf_idx = pd.to_datetime(mdf.index)
                loc_mask = mdf_idx <= mdt
                if loc_mask.sum() == 0:
                    continue
                loc = loc_mask.sum() - 1
                if loc + labeler.max_hold >= len(mdf):
                    continue
                entry_price = float(mdf.iloc[loc]["Close"])
                future = mdf.iloc[loc:loc + labeler.max_hold + 1]["Close"]
                rets = (future.iloc[1:] - entry_price) / entry_price
                label, hit_day, hit_type = labeler._check_barriers(rets)
                tb_results.append({
                    "symbol": sym,
                    "date": m["date"],
                    "label": label,
                    "hit_day": hit_day,
                    "hit_type": hit_type,
                    "final_return": round(float(rets.iloc[-1]) * 100, 2) if len(rets) > 0 else 0,
                })
                m["tb_label"] = label
                m["tb_hit_day"] = hit_day
                m["tb_hit_type"] = hit_type
                m["final_return_pct"] = round(float(rets.iloc[-1]) * 100, 2) if len(rets) > 0 else 0
            except Exception:
                continue

        # 7. Compute win-rate stats
        total_tb = len(tb_results)
        bullish = sum(1 for r in tb_results if r["label"] == 1)
        bearish = sum(1 for r in tb_results if r["label"] == -1)
        neutral = sum(1 for r in tb_results if r["label"] == 0)
        win_rate, wl_ratio = calculate_win_loss_ratio(tb_results)

        avg_return = np.mean([r["final_return"] for r in tb_results]) if tb_results else 0
        avg_hit_day = np.mean([r["hit_day"] for r in tb_results]) if tb_results else 0

        return json_response("success", data={
            "matches": top_matches,
            "query_info": {
                "symbol": symbol,
                "date": date_str or "latest",
                "bars": lookback,
            },
            "win_rate_stats": {
                "total": total_tb,
                "bullish": bullish,
                "neutral": neutral,
                "bearish": bearish,
                "win_rate": round(win_rate * 100, 1),
                "win_loss_ratio": round(wl_ratio, 2),
                "avg_return_pct": round(float(avg_return), 2),
                "avg_holding_days": round(float(avg_hit_day), 1),
            },
        })

    def get_status(self):
        index_exists = os.path.exists(get_index_path())
        model_exists = os.path.exists(get_model_path())
        meta_exists = os.path.exists(get_meta_path())
        n_records = len(self.meta_data) if self.meta_data else 0
        if not n_records and meta_exists:
            try:
                n_records = sum(1 for _ in open(get_meta_path())) - 1
            except Exception:
                pass

        return json_response("success", data={
            "index_ready": index_exists and model_exists and meta_exists,
            "model_exists": model_exists,
            "index_exists": index_exists,
            "meta_exists": meta_exists,
            "n_records": n_records,
            "model_loaded": self.model_loaded,
            "data_dir": get_data_dir(),
        })

    def encode_image(self, image_path):
        if not self.model_loaded:
            if not self._try_load_model():
                return json_response("error", error="Model not loaded")
        vec = self._image_to_vector(image_path)
        if vec is None:
            return json_response("error", error="Failed to encode image")
        return json_response("success", data={
            "vector": vec.tolist(),
            "dim": len(vec),
        })


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    command, params = parse_args()

    if command == "status":
        engine = VisionEngine()
        output_json(engine.get_status())

    elif command == "search":
        engine = VisionEngine()
        symbol = params.get("symbol", "")
        date = params.get("date")
        top_k = int(params.get("top_k", 10))
        lookback = int(params.get("lookback", 60))
        if not symbol:
            output_json(json_response("error", error="symbol is required"))
            return
        result = engine.search_similar_patterns(symbol, date_str=date, top_k=top_k, lookback=lookback, config=params)
        output_json(result)

    elif command == "encode":
        engine = VisionEngine()
        image_path = params.get("image_path", "")
        if not image_path or not os.path.exists(image_path):
            output_json(json_response("error", error="Valid image_path is required"))
            return
        output_json(engine.encode_image(image_path))

    else:
        output_json(json_response("error", error=f"Unknown command: {command}"))


if __name__ == "__main__":
    main()
