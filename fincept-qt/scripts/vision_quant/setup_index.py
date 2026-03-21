"""
VisionQuant Index Builder - First-run setup
Downloads data, generates candlestick images, trains model, builds FAISS index.

CLI Protocol:
    python setup_index.py build '{"symbols":["AAPL","MSFT"],"start":"20200101","stride":5}'
    python setup_index.py status '{}'

Progress is streamed as JSON lines to stdout.
"""

import sys
import json
import os
import time
import numpy as np
import pandas as pd
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from utils import (
    get_data_dir, get_model_path, get_index_path, get_meta_path,
    fetch_ohlcv, generate_kline_image, ensure_dirs,
    json_response, output_json, output_progress, parse_args,
)

# Default US market symbols (S&P 500 top 50)
DEFAULT_SYMBOLS = [
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK-B", "JPM", "V",
    "UNH", "JNJ", "XOM", "PG", "MA", "HD", "CVX", "MRK", "ABBV", "PEP",
    "KO", "COST", "AVGO", "LLY", "WMT", "MCD", "CSCO", "TMO", "ACN", "ABT",
    "DHR", "NEE", "LIN", "TXN", "PM", "UNP", "LOW", "INTC", "COP", "AMGN",
    "RTX", "HON", "NKE", "BA", "CAT", "GS", "IBM", "MMM", "DIS", "AXP",
]


def build_index(symbols=None, start_date="2020-01-01", stride=5, window=60, epochs=30,
                batch_size=32, chart_style="international", learning_rate=1e-3):
    """
    Full pipeline: download -> generate images -> train model -> build FAISS index.

    Args:
        symbols: List of ticker symbols
        start_date: Start date for historical data
        stride: Step size between consecutive images (days)
        window: Bars per image
        epochs: Training epochs for the CAE model
    """
    if symbols is None or len(symbols) == 0:
        symbols = DEFAULT_SYMBOLS

    base_dir = ensure_dirs()
    img_dir = os.path.join(base_dir, "images")
    data_dir = os.path.join(base_dir, "data")
    model_path = get_model_path()
    index_path = get_index_path()
    meta_path = get_meta_path()

    total_steps = 4
    current_step = 0

    # =====================================================================
    # Step 1: Download OHLCV data
    # =====================================================================
    current_step += 1
    output_progress(f"Step {current_step}/{total_steps}: Downloading market data...", pct=0)

    all_data = {}
    for i, sym in enumerate(symbols):
        pct = int(i / len(symbols) * 25)
        output_progress(f"Downloading {sym} ({i+1}/{len(symbols)})", pct=pct)
        try:
            df = fetch_ohlcv(sym, start=start_date)
            if df is not None and len(df) >= window + 1:
                # Cache to CSV
                csv_path = os.path.join(data_dir, f"{sym}.csv")
                df.to_csv(csv_path)
                all_data[sym] = df
        except Exception as e:
            output_progress(f"Warning: Failed to download {sym}: {e}")
            continue

    if len(all_data) == 0:
        output_json(json_response("error", error="No data downloaded for any symbol"))
        return

    output_progress(f"Downloaded data for {len(all_data)} symbols", pct=25)

    # =====================================================================
    # Step 2: Generate candlestick images
    # =====================================================================
    current_step += 1
    output_progress(f"Step {current_step}/{total_steps}: Generating K-line images...", pct=25)

    metadata = []  # (symbol, date, image_path)
    total_images = 0

    for sym_idx, (sym, df) in enumerate(all_data.items()):
        sym_dir = os.path.join(img_dir, sym)
        os.makedirs(sym_dir, exist_ok=True)

        n_bars = len(df)
        for i in range(0, n_bars - window, stride):
            slice_df = df.iloc[i:i + window]
            date_str = slice_df.index[-1].strftime("%Y%m%d")
            img_path = os.path.join(sym_dir, f"{sym}_{date_str}.png")

            result = generate_kline_image(slice_df, img_path, window=window, style=chart_style)
            if result:
                metadata.append({
                    "symbol": sym,
                    "date": date_str,
                    "path": img_path,
                })
                total_images += 1

        pct = 25 + int(sym_idx / len(all_data) * 25)
        output_progress(f"Images for {sym}: {total_images} total so far", pct=pct)

    output_progress(f"Generated {total_images} images", pct=50)

    if total_images < 10:
        output_json(json_response("error", error=f"Too few images generated ({total_images})"))
        return

    # =====================================================================
    # Step 3: Train AttentionCAE model
    # =====================================================================
    current_step += 1
    output_progress(f"Step {current_step}/{total_steps}: Training AttentionCAE model...", pct=50)

    import torch
    from torchvision import transforms
    from torch.utils.data import DataLoader, Dataset
    from PIL import Image
    from models.attention_cae import AttentionCAE, AttentionCAETrainer

    class KlineImageDataset(Dataset):
        def __init__(self, image_paths, transform):
            self.paths = [p for p in image_paths if os.path.exists(p)]
            self.transform = transform

        def __len__(self):
            return len(self.paths)

        def __getitem__(self, idx):
            img = Image.open(self.paths[idx]).convert("RGB")
            tensor = self.transform(img)
            return tensor, 0  # label unused for autoencoder

    transform = transforms.Compose([
        transforms.Resize((224, 224)),
        transforms.ToTensor(),
    ])

    image_paths = [m["path"] for m in metadata]
    dataset = KlineImageDataset(image_paths, transform)
    dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True, num_workers=0)

    model = AttentionCAE(latent_dim=1024, num_attention_heads=8)
    trainer = AttentionCAETrainer(model, lr=learning_rate)

    best_loss = float("inf")
    for epoch in range(epochs):
        loss = trainer.train_epoch(dataloader)
        pct = 50 + int(epoch / epochs * 25)
        output_progress(f"Epoch {epoch+1}/{epochs}, Loss: {loss:.6f}", pct=pct)

        if loss < best_loss:
            best_loss = loss
            os.makedirs(os.path.dirname(model_path), exist_ok=True)
            torch.save(model.state_dict(), model_path)

    output_progress(f"Model trained. Best loss: {best_loss:.6f}", pct=75)

    # =====================================================================
    # Step 4: Encode all images and build FAISS index
    # =====================================================================
    current_step += 1
    output_progress(f"Step {current_step}/{total_steps}: Building FAISS index...", pct=75)

    import faiss

    # Load best model
    model.load_state_dict(torch.load(model_path, map_location="cpu", weights_only=True))
    model.eval()
    device = torch.device("cpu")

    vectors = []
    valid_meta = []

    for i, meta in enumerate(metadata):
        img_path = meta["path"]
        if not os.path.exists(img_path):
            continue
        try:
            img = Image.open(img_path).convert("RGB")
            tensor = transform(img).unsqueeze(0).to(device)
            with torch.no_grad():
                vec = model.encode(tensor).cpu().numpy().flatten()
            vectors.append(vec)
            valid_meta.append(meta)
        except Exception:
            continue

        if (i + 1) % 100 == 0:
            pct = 75 + int(i / len(metadata) * 25)
            output_progress(f"Encoded {i+1}/{len(metadata)} images", pct=pct)

    if len(vectors) < 10:
        output_json(json_response("error", error=f"Too few vectors encoded ({len(vectors)})"))
        return

    # Build FAISS index (Inner Product for cosine similarity on L2-normalized vectors)
    dim = len(vectors[0])
    matrix = np.array(vectors, dtype="float32")
    faiss.normalize_L2(matrix)

    index = faiss.IndexFlatIP(dim)
    index.add(matrix)

    os.makedirs(os.path.dirname(index_path), exist_ok=True)
    faiss.write_index(index, index_path)

    # Save metadata CSV
    meta_df = pd.DataFrame(valid_meta)
    meta_df.to_csv(meta_path, index=False)

    output_progress("Index built successfully!", pct=100)

    output_json(json_response("success", data={
        "symbols_count": len(all_data),
        "images_count": total_images,
        "vectors_count": len(vectors),
        "index_dim": dim,
        "model_path": model_path,
        "index_path": index_path,
        "meta_path": meta_path,
        "best_loss": round(best_loss, 6),
    }))


def check_status():
    """Check if the index is already built."""
    model_exists = os.path.exists(get_model_path())
    index_exists = os.path.exists(get_index_path())
    meta_exists = os.path.exists(get_meta_path())

    n_records = 0
    if meta_exists:
        try:
            n_records = sum(1 for _ in open(get_meta_path())) - 1
        except Exception:
            pass

    return json_response("success", data={
        "index_ready": model_exists and index_exists and meta_exists,
        "model_exists": model_exists,
        "index_exists": index_exists,
        "meta_exists": meta_exists,
        "n_records": n_records,
        "data_dir": get_data_dir(),
    })


def main():
    command, params = parse_args()

    if command == "build":
        symbols = params.get("symbols", DEFAULT_SYMBOLS)
        start = params.get("start", params.get("start_date", "2020-01-01"))
        if len(start) == 8 and "-" not in start:
            start = f"{start[:4]}-{start[4:6]}-{start[6:8]}"
        stride = int(params.get("stride", 5))
        window = int(params.get("window", 60))
        epochs = int(params.get("epochs", 30))
        batch_size = int(params.get("batch_size", 32))
        chart_style = params.get("chart_style", "international")
        learning_rate = float(params.get("learning_rate", 1e-3))
        build_index(symbols, start, stride, window, epochs, batch_size, chart_style, learning_rate)

    elif command == "status":
        output_json(check_status())

    else:
        output_json(json_response("error", error=f"Unknown command: {command}"))


if __name__ == "__main__":
    main()
