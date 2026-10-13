"""
spreadsheet.py — unified local + Google Sheets I/O for the Fincept workflow node.

Usage:
    python spreadsheet.py --args '<json>'

The JSON args object supports:
  source           : "local" | "google"
  operation        : "read" | "write" | "append" | "clear"
  file_path        : path to local .xlsx or .csv  (source=local)
  spreadsheet_id   : Google Sheet ID from URL     (source=google)
  sheet_name       : sheet/tab name (default "Sheet1")
  range            : cell range e.g. "A1:D100"    (default "A1:Z1000")
  credentials_path : path to service account credentials.json
                     leave empty for public sheets (source=google, read-only)
  data             : [[...], [...], ...]  rows to write/append

Outputs a single JSON object to stdout.
"""

import csv
import sys
import json
import argparse
import os


def parse_range(range_str):
    """Parse 'A1:D100' -> (r1, c1, r2, c2) 1-based. Returns None on failure."""
    try:
        parts = range_str.upper().split(":")
        if len(parts) != 2:
            return None

        def parse_cell(cell):
            col = 0
            i = 0
            while i < len(cell) and cell[i].isalpha():
                col = col * 26 + (ord(cell[i]) - ord("A") + 1)
                i += 1
            row = int(cell[i:])
            return row, col

        r1, c1 = parse_cell(parts[0].strip())
        r2, c2 = parse_cell(parts[1].strip())
        return r1, c1, r2, c2
    except Exception:
        return None


# ── LOCAL ─────────────────────────────────────────────────────────────────────

def _is_csv(file_path):
    return os.path.splitext(file_path)[1].lower() in (".csv", ".tsv", ".txt")


def _csv_delimiter(file_path):
    return "\t" if os.path.splitext(file_path)[1].lower() == ".tsv" else ","


def _normalise_rows(data):
    """Rows to write: a list of lists, or a list of dicts (header row added)."""
    if data and all(isinstance(r, dict) for r in data):
        header = []
        for r in data:
            for k in r:
                if k not in header:
                    header.append(k)
        return [header] + [[r.get(k) for k in header] for r in data]
    return [r if isinstance(r, list) else [r] for r in data]


def _cell_json(v):
    """JSON-safe cell value (openpyxl yields datetime/date/time/Decimal)."""
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    iso = getattr(v, "isoformat", None)
    if callable(iso):
        return iso()
    return str(v)


def _read_csv_rows(file_path):
    # csv module: quoted fields keep their commas and line breaks.
    with open(file_path, "r", encoding="utf-8-sig", newline="") as f:
        return [row for row in csv.reader(f, delimiter=_csv_delimiter(file_path))]


def _write_csv_rows(file_path, rows):
    with open(file_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f, delimiter=_csv_delimiter(file_path))
        for row in rows:
            writer.writerow(["" if v is None else v for v in row])


def _window(rows, r1, c1, r2, c2):
    out = []
    for row in rows[r1 - 1:r2]:
        cells = list(row[c1 - 1:c2])
        if any(v not in (None, "") for v in cells):
            out.append([_cell_json(v) for v in cells])
    return out


def local_read(file_path, sheet_name, range_str):
    parsed = parse_range(range_str)
    r1, c1, r2, c2 = parsed if parsed else (1, 1, 1000, 26)
    if _is_csv(file_path):
        rows = _window(_read_csv_rows(file_path), r1, c1, r2, c2)
        return {"source": "local", "operation": "read", "path": file_path, "rows": len(rows), "data": rows}

    import openpyxl
    # data_only=True returns the values Excel cached for formulas; a workbook
    # saved by a tool that does not calculate stores none (they read as null).
    wb = openpyxl.load_workbook(file_path, read_only=True, data_only=True)
    try:
        if sheet_name in wb.sheetnames:
            ws = wb[sheet_name]
        else:
            ws = wb.active
        rows = []
        for row in ws.iter_rows(min_row=r1, max_row=r2, min_col=c1, max_col=c2, values_only=True):
            if any(v is not None for v in row):
                rows.append([_cell_json(v) for v in row])
        used_sheet = ws.title
    finally:
        wb.close()
    return {"source": "local", "operation": "read", "path": file_path, "sheet": used_sheet,
            "rows": len(rows), "data": rows}


def local_write(file_path, sheet_name, range_str, data, append=False):
    parsed = parse_range(range_str)
    r1, c1 = (parsed[0], parsed[1]) if parsed else (1, 1)
    rows_in = _normalise_rows(data)

    if _is_csv(file_path):
        # CSV used to be saved through openpyxl — an XLSX zip written into a
        # file named .csv, destroying the original.
        existing = _read_csv_rows(file_path) if os.path.exists(file_path) else []
        if append:
            start = len(existing)
        else:
            start = r1 - 1
        for ri, row in enumerate(rows_in):
            target = start + ri
            while len(existing) <= target:
                existing.append([])
            line = existing[target]
            for ci, val in enumerate(row):
                col = (0 if append else c1 - 1) + ci
                while len(line) <= col:
                    line.append("")
                line[col] = val
        _write_csv_rows(file_path, existing)
        return {"source": "local", "operation": "append" if append else "write",
                "path": file_path, "rows_written": len(rows_in)}

    import openpyxl
    if os.path.exists(file_path):
        # Write INTO the existing workbook. Creating a fresh one here wiped every
        # other sheet of the user's file on a plain "write".
        wb = openpyxl.load_workbook(file_path)
        ws = wb[sheet_name] if sheet_name in wb.sheetnames else wb.create_sheet(sheet_name)
    else:
        wb = openpyxl.Workbook()
        ws = wb.active
        ws.title = sheet_name or "Sheet1"

    start_row = r1
    start_col = c1
    if append:
        start_row = (ws.max_row + 1) if ws.max_row and any(c.value is not None for c in ws[ws.max_row]) else 1
        start_col = 1

    for ri, row in enumerate(rows_in):
        for ci, val in enumerate(row):
            ws.cell(row=start_row + ri, column=start_col + ci, value=val)

    wb.save(file_path)
    return {"source": "local", "operation": "append" if append else "write",
            "path": file_path, "sheet": ws.title, "rows_written": len(rows_in)}


def local_clear(file_path, sheet_name, range_str):
    parsed = parse_range(range_str)
    r1, c1, r2, c2 = parsed if parsed else (1, 1, 1000, 26)
    if _is_csv(file_path):
        rows = _read_csv_rows(file_path)
        for r in range(r1 - 1, min(r2, len(rows))):
            for c in range(c1 - 1, min(c2, len(rows[r]))):
                rows[r][c] = ""
        _write_csv_rows(file_path, rows)
        return {"source": "local", "operation": "clear", "path": file_path}

    import openpyxl
    wb = openpyxl.load_workbook(file_path)
    ws = wb[sheet_name] if sheet_name in wb.sheetnames else wb.active
    for r in range(r1, r2 + 1):
        for c in range(c1, c2 + 1):
            ws.cell(row=r, column=c).value = None
    wb.save(file_path)
    return {"source": "local", "operation": "clear", "path": file_path, "sheet": ws.title}


# ── GOOGLE SHEETS ─────────────────────────────────────────────────────────────

def google_read(spreadsheet_id, sheet_name, range_str, credentials_path):
    if credentials_path:
        import gspread
        gc = gspread.service_account(filename=credentials_path)
        sh = gc.open_by_key(spreadsheet_id)
        ws = sh.worksheet(sheet_name)
        rows = ws.get(range_str)
    else:
        # A sheet shared "anyone with the link can view" is readable without
        # credentials through its CSV export. (The Sheets v4 values API this used
        # requires an API key, so the no-credentials path always failed.)
        import io
        import urllib.parse
        import requests
        url = ("https://docs.google.com/spreadsheets/d/%s/gviz/tq?tqx=out:csv&sheet=%s"
               % (urllib.parse.quote(spreadsheet_id, safe=""), urllib.parse.quote(sheet_name or "", safe="")))
        resp = requests.get(url, timeout=30)
        if not resp.ok or "text/csv" not in resp.headers.get("Content-Type", ""):
            return {"error": "Could not read the sheet without credentials (HTTP %d). Share it as 'Anyone with "
                             "the link can view', or provide a service-account credentials JSON."
                             % resp.status_code}
        all_rows = list(csv.reader(io.StringIO(resp.content.decode("utf-8-sig"))))
        parsed = parse_range(range_str)
        r1, c1, r2, c2 = parsed if parsed else (1, 1, 1000, 26)
        rows = _window(all_rows, r1, c1, r2, c2)

    return {"source": "google", "operation": "read", "sheet_id": spreadsheet_id,
            "range": range_str, "rows": len(rows), "data": rows}


def google_write(spreadsheet_id, sheet_name, range_str, data, credentials_path, append=False):
    import gspread
    gc = gspread.service_account(filename=credentials_path)
    sh = gc.open_by_key(spreadsheet_id)
    ws = sh.worksheet(sheet_name)
    if append:
        result = ws.append_rows(data, value_input_option="USER_ENTERED",
                                insert_data_option="INSERT_ROWS")
        updated = result.get("updates", {}).get("updatedCells", len(data))
    else:
        result = ws.update(range_str, data, value_input_option="USER_ENTERED")
        updated = result.get("updatedCells", len(data))
    return {"source": "google", "operation": "append" if append else "write",
            "sheet_id": spreadsheet_id, "updated_cells": updated}


def google_clear(spreadsheet_id, sheet_name, range_str, credentials_path):
    import gspread
    gc = gspread.service_account(filename=credentials_path)
    sh = gc.open_by_key(spreadsheet_id)
    ws = sh.worksheet(sheet_name)
    ws.batch_clear([range_str])
    return {"source": "google", "operation": "clear", "sheet_id": spreadsheet_id}


# ── MAIN ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--args", required=True)
    parsed = parser.parse_args()

    try:
        args = json.loads(parsed.args)
    except json.JSONDecodeError as e:
        print(json.dumps({"error": f"Invalid --args JSON: {e}"}))
        sys.exit(1)

    source           = args.get("source", "local")
    operation        = args.get("operation", "read")
    file_path        = args.get("file_path", "")
    spreadsheet_id   = args.get("spreadsheet_id", "")
    sheet_name       = args.get("sheet_name", "Sheet1")
    range_str        = args.get("range", "A1:Z1000")
    credentials_path = args.get("credentials_path", "")
    data             = args.get("data", [])

    # Normalise data: may be a list of rows or a wrapped object
    if isinstance(data, dict) and "data" in data:
        data = data["data"]
    if not isinstance(data, list):
        data = []

    try:
        if source == "local":
            if not file_path:
                result = {"error": "file_path is required for source=local"}
            elif operation == "read":
                result = local_read(file_path, sheet_name, range_str)
            elif operation in ("write", "append"):
                result = local_write(file_path, sheet_name, range_str, data,
                                     append=(operation == "append"))
            elif operation == "clear":
                result = local_clear(file_path, sheet_name, range_str)
            else:
                result = {"error": f"Unknown operation: {operation}"}

        elif source == "google":
            if not spreadsheet_id:
                result = {"error": "spreadsheet_id is required for source=google"}
            elif operation == "read":
                result = google_read(spreadsheet_id, sheet_name, range_str, credentials_path)
            elif operation in ("write", "append"):
                if not credentials_path:
                    result = {"error": "credentials_path is required for write/append to Google Sheets"}
                else:
                    result = google_write(spreadsheet_id, sheet_name, range_str, data,
                                          credentials_path, append=(operation == "append"))
            elif operation == "clear":
                if not credentials_path:
                    result = {"error": "credentials_path is required for clear on Google Sheets"}
                else:
                    result = google_clear(spreadsheet_id, sheet_name, range_str, credentials_path)
            else:
                result = {"error": f"Unknown operation: {operation}"}
        else:
            result = {"error": f"Unknown source: {source}"}

    except Exception as e:
        result = {"error": str(e)}

    print(json.dumps(result, default=str))


if __name__ == "__main__":
    main()
