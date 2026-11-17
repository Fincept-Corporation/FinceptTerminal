"""
spreadsheet.py — unified local + Google Sheets I/O for the Fincept workflow node.

Usage:
    python spreadsheet.py --args '<json>'
    python spreadsheet.py --args -          (the JSON object on stdin — how the workflow node
                                             sends it, since rows to write can exceed the
                                             ~32 KB Windows command-line limit)

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


def _is_dict_rows(data):
    return bool(data) and all(isinstance(r, dict) for r in data)


def _dict_keys(data):
    keys = []
    for r in data:
        for k in r:
            if k not in keys:
                keys.append(k)
    return keys


def _is_number_text(v):
    try:
        float(str(v).strip().replace(",", ""))
        return True
    except ValueError:
        return False


def _is_header_row(row):
    """A row that looks like column names: at least one cell, every non-empty cell non-numeric
    text (CSV cells are always strings, so numeric-looking text counts as data)."""
    cells = [v for v in (row or []) if v not in (None, "")]
    return bool(cells) and all(isinstance(v, str) and not _is_number_text(v) for v in cells)


def _trim(row):
    row = list(row or [])
    while row and row[-1] in (None, ""):
        row.pop()
    return row


def _normalise_rows(data, existing_first_row=None, target_has_rows=False):
    """
    Rows to write, plus the header row to rewrite in place (or None).

    Returns (rows, header_update, added_columns).
      * list rows                          -> as given; nothing else changes.
      * dict rows into an empty target     -> [header] + value rows (header from the keys).
      * dict rows appended to a target whose first row is a header
                                           -> value rows aligned to THAT header's column
                                              order, no second header row; keys the header
                                              lacks become new columns at its end
                                              (header_update is the extended header).
      * dict rows appended to a target with rows but no header row
                                           -> value rows only, in first-seen key order.
    """
    if not _is_dict_rows(data):
        return [r if isinstance(r, list) else [r] for r in data], None, []
    keys = _dict_keys(data)
    if not target_has_rows:
        return [keys] + [[r.get(k) for k in keys] for r in data], None, []
    header = _trim(existing_first_row)
    if not _is_header_row(header):
        return [[r.get(k) for k in keys] for r in data], None, []
    header = ["" if h is None else str(h) for h in header]
    added = [k for k in keys if str(k) not in header]
    columns = header + [str(k) for k in added]
    by_name = {str(k): k for k in keys}
    rows = [[r.get(by_name[c]) if c in by_name else None for c in columns] for r in data]
    return rows, (columns if added else None), added


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


_DEFAULT_SHEET = "Sheet1"  # the workflow node's default Sheet Name


def _sheet_not_found(sheet_name, sheetnames):
    return {"error": "Sheet '%s' not found; sheets: %s" % (sheet_name, list(sheetnames))}


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
    note = None
    try:
        if sheet_name in wb.sheetnames:
            ws = wb[sheet_name]
        elif not sheet_name or sheet_name == _DEFAULT_SHEET:
            # The node's untouched default: read the workbook's active sheet, and say so.
            ws = wb.active
            note = "Sheet '%s' not found; read the active sheet '%s'" % (sheet_name or _DEFAULT_SHEET, ws.title)
        else:
            return _sheet_not_found(sheet_name, wb.sheetnames)
        rows = []
        for row in ws.iter_rows(min_row=r1, max_row=r2, min_col=c1, max_col=c2, values_only=True):
            if any(v is not None for v in row):
                rows.append([_cell_json(v) for v in row])
        used_sheet = ws.title
    finally:
        wb.close()
    result = {"source": "local", "operation": "read", "path": file_path, "sheet": used_sheet,
              "rows": len(rows), "data": rows}
    if note:
        result["note"] = note
    return result


def _has_values(row):
    return any(v not in (None, "") for v in (row or []))


def _last_used_row(ws):
    """Last row holding a value (ws.max_row also counts formatted-but-empty rows)."""
    for r in range(ws.max_row or 0, 0, -1):
        if any(c.value not in (None, "") for c in ws[r]):
            return r
    return 0


def local_write(file_path, sheet_name, range_str, data, append=False):
    parsed = parse_range(range_str)
    r1, c1 = (parsed[0], parsed[1]) if parsed else (1, 1)

    if _is_csv(file_path):
        # CSV used to be saved through openpyxl — an XLSX zip written into a
        # file named .csv, destroying the original.
        existing = _read_csv_rows(file_path) if os.path.exists(file_path) else []
        if append:
            while existing and not _has_values(existing[-1]):
                existing.pop()
            rows_in, header_update, added = _normalise_rows(
                data, existing[0] if existing else None, target_has_rows=bool(existing))
            if header_update:
                existing[0] = header_update
            start = len(existing)
        else:
            rows_in, header_update, added = _normalise_rows(data)
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
        result = {"source": "local", "operation": "append" if append else "write",
                  "path": file_path, "rows_written": len(rows_in)}
        if added:
            result["added_columns"] = added
        return result

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
    added = []
    if append:
        last = _last_used_row(ws)
        first_row = [c.value for c in ws[1]] if last else None
        rows_in, header_update, added = _normalise_rows(data, first_row, target_has_rows=bool(last))
        if header_update:
            for ci, name in enumerate(header_update):
                ws.cell(row=1, column=1 + ci, value=name)
        start_row = last + 1
        start_col = 1
    else:
        rows_in, _, _ = _normalise_rows(data)

    for ri, row in enumerate(rows_in):
        for ci, val in enumerate(row):
            ws.cell(row=start_row + ri, column=start_col + ci, value=val)

    wb.save(file_path)
    result = {"source": "local", "operation": "append" if append else "write",
              "path": file_path, "sheet": ws.title, "rows_written": len(rows_in)}
    if added:
        result["added_columns"] = added
    return result


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
    if sheet_name not in wb.sheetnames:
        # Clearing is destructive: never fall back to a sheet the user did not name.
        return _sheet_not_found(sheet_name, wb.sheetnames)
    ws = wb[sheet_name]
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
    added = []
    if append:
        # Same header rule as local append: dict rows follow the sheet's existing header
        # instead of adding a second header row before every batch.
        first_row = ws.row_values(1) if _is_dict_rows(data) else None
        rows, header_update, added = _normalise_rows(data, first_row, target_has_rows=bool(first_row))
        if header_update:
            ws.update("A1", [header_update], value_input_option="USER_ENTERED")
        result = ws.append_rows(rows, value_input_option="USER_ENTERED",
                                insert_data_option="INSERT_ROWS")
        updated = result.get("updates", {}).get("updatedCells", len(rows))
    else:
        rows, _, _ = _normalise_rows(data)
        result = ws.update(range_str, rows, value_input_option="USER_ENTERED")
        updated = result.get("updatedCells", len(rows))
    out = {"source": "google", "operation": "append" if append else "write",
           "sheet_id": spreadsheet_id, "updated_cells": updated}
    if added:
        out["added_columns"] = added
    return out


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
    parser.add_argument("--args", required=True, help="JSON object, or '-' to read it from stdin")
    parsed = parser.parse_args()

    try:
        raw = sys.stdin.buffer.read().decode("utf-8-sig") if parsed.args == "-" else parsed.args
        args = json.loads(raw)
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        print(json.dumps({"error": f"Invalid --args JSON: {e}"}))
        sys.exit(1)
    if not isinstance(args, dict):
        print(json.dumps({"error": "--args must be a JSON object"}))
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
