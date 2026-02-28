"""
Extract first 3 pages of Bayer Annual Report 2024 using unstructured + pypdf.
Outputs: bayer_pages_1_3.txt, bayer_pages_1_3.json, bayer_pages_1_3.csv
"""

import os
import json
import pypdf
from unstructured.documents.elements import Title, NarrativeText, Text, Table
from unstructured.cleaners.core import clean_extra_whitespace, group_broken_paragraphs
from unstructured.staging.base import convert_to_dataframe

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
PDF_PATH = r"C:\Users\Tilak\Downloads\bayer-annual-report-2024.pdf"
PAGES = [0, 1, 2]  # 0-indexed = pages 1, 2, 3


def classify_text(text: str):
    """Heuristic: short lines are titles, longer ones are narrative."""
    text = text.strip()
    if not text:
        return None
    if len(text) < 80 and not text.endswith("."):
        return "Title"
    return "NarrativeText"


def extract_pages():
    reader = pypdf.PdfReader(PDF_PATH)
    total = len(reader.pages)
    print(f"PDF has {total} pages. Extracting pages 1-3...")

    all_elements = []

    for page_idx in PAGES:
        page_num = page_idx + 1
        page = reader.pages[page_idx]
        raw_text = page.extract_text() or ""

        # Split on newlines first, then clean each line individually
        lines = [clean_extra_whitespace(l).strip() for l in raw_text.split("\n") if l.strip()]

        page_elements = []
        for line in lines:
            kind = classify_text(line)
            if kind == "Title":
                el = {"type": "Title", "text": line, "page": page_num}
            elif kind == "NarrativeText":
                el = {"type": "NarrativeText", "text": line, "page": page_num}
            else:
                continue
            page_elements.append(el)

        print(f"  Page {page_num}: {len(page_elements)} elements extracted")
        all_elements.extend(page_elements)

    return all_elements


def save_outputs(elements):
    # --- TXT ---
    txt_path = os.path.join(OUT_DIR, "bayer_pages_1_3.txt")
    with open(txt_path, "w", encoding="utf-8") as f:
        current_page = None
        for el in elements:
            if el["page"] != current_page:
                current_page = el["page"]
                f.write(f"\n{'='*60}\n PAGE {current_page}\n{'='*60}\n\n")
            f.write(f"[{el['type']}]\n{el['text']}\n\n")
    print(f"\nSaved TXT  -> {txt_path}")

    # --- JSON ---
    json_path = os.path.join(OUT_DIR, "bayer_pages_1_3.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(elements, f, indent=2, ensure_ascii=False)
    print(f"Saved JSON -> {json_path}")

    # --- CSV ---
    import csv
    csv_path = os.path.join(OUT_DIR, "bayer_pages_1_3.csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["page", "type", "text"])
        writer.writeheader()
        writer.writerows(elements)
    print(f"Saved CSV  -> {csv_path}")

    print(f"\nTotal elements: {len(elements)}")
    print(f"Breakdown: { {t: sum(1 for e in elements if e['type']==t) for t in set(e['type'] for e in elements)} }")


if __name__ == "__main__":
    elements = extract_pages()
    save_outputs(elements)
    print("\nDone. Preview of first 5 elements:")
    for el in elements[:5]:
        print(f"  [{el['type']}] Page {el['page']}: {el['text'][:100]}")
