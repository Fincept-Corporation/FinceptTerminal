"""
Unstructured ETL Pipeline
=========================
Converts unstructured documents (PDF, HTML, TXT) into clean structured data.

Pipeline stages:
  1. EXTRACT   — Partition document into typed elements (Title, Table, NarrativeText, etc.)
  2. CLEAN     — Normalize text: whitespace, ligatures, bullets, unicode, broken words
  3. ENRICH    — Extract entities: emails, phones, dates, IPs, URLs
  4. FILTER    — Remove noise: headers, footers, page numbers, empty elements
  5. CHUNK     — Split into LLM-ready chunks by section title with overlap
  6. LOAD      — Export to JSON, CSV, Markdown, and plain text
"""

import os
import re
import json
import csv
import logging
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

# --- unstructured imports ---
import pypdf
from unstructured.documents.elements import (
    Title, NarrativeText, ListItem, Table, Image,
    Header, Footer, CodeSnippet, Address, EmailAddress,
    CheckBox, FigureCaption, PageBreak, CompositeElement,
    Text, Element
)
from unstructured.cleaners.core import (
    clean,
    clean_extra_whitespace,
    clean_bullets,
    clean_dashes,
    clean_ligatures,
    clean_non_ascii_chars,
    replace_unicode_quotes,
    clean_trailing_punctuation,
)
from unstructured.cleaners.extract import (
    extract_email_address,
    extract_us_phone_number,
    extract_datetimetz,
    extract_ip_address,
    extract_ordered_bullets,
    extract_text_after,
    extract_text_before,
    extract_image_urls_from_html,
)
from unstructured.nlp.patterns import (
    US_PHONE_NUMBERS_RE,
    EMAIL_ADDRESS_PATTERN_RE,
    IP_ADDRESS_PATTERN_RE,
    UNICODE_BULLETS_RE,
    NUMBERED_LIST_RE,
)
from unstructured.staging.base import (
    convert_to_dataframe,
    convert_to_dict,
    elements_to_json,
    elements_to_text,
    elements_to_md,
    filter_element_types,
    elements_to_dicts,
)
from unstructured.chunking.title import chunk_by_title
from unstructured.chunking.basic import chunk_elements

logging.basicConfig(level=logging.INFO, format="%(levelname)s | %(message)s")
log = logging.getLogger("etl_pipeline")


# ─────────────────────────────────────────────
# CONFIG
# ─────────────────────────────────────────────

@dataclass
class ETLConfig:
    # Output directory
    output_dir: str = "."

    # Extraction
    page_range: Optional[list] = None          # e.g. [1, 3] for pages 1-3, None = all
    include_page_breaks: bool = False

    # Cleaning flags (mirrors unstructured clean())
    clean_extra_whitespace: bool = True
    clean_dashes: bool = True
    clean_bullets: bool = True
    clean_ligatures: bool = True
    clean_non_ascii: bool = False              # keep unicode by default
    replace_unicode_quotes: bool = True
    fix_broken_words: bool = True             # custom: fix "sale s" → "sales"

    # Filter: element types to KEEP (None = keep all except noise)
    keep_types: list = field(default_factory=lambda: [
        "Title", "NarrativeText", "ListItem", "Table",
        "CodeSnippet", "Address", "EmailAddress", "FigureCaption", "Formula"
    ])
    # Filter: minimum text length to keep
    min_text_length: int = 3
    # Filter: remove pure numeric elements (page numbers)
    remove_page_numbers: bool = True

    # Chunking
    chunk_strategy: str = "by_title"          # "by_title" | "basic" | "none"
    chunk_max_chars: int = 1500
    chunk_overlap: int = 100
    chunk_combine_under_n_chars: int = 200    # combine tiny sections
    chunk_multipage_sections: bool = True

    # Export formats
    export_json: bool = True
    export_csv: bool = True
    export_markdown: bool = True
    export_txt: bool = True
    export_entities: bool = True              # separate entities JSON


# ─────────────────────────────────────────────
# STAGE 1: EXTRACT
# ─────────────────────────────────────────────

def extract(filepath: str, config: ETLConfig) -> list[dict]:
    """
    Partition PDF into raw line-level elements using pypdf.
    Returns list of dicts with: text, page, raw_type
    """
    log.info(f"[EXTRACT] Reading: {filepath}")
    reader = pypdf.PdfReader(filepath)
    total_pages = len(reader.pages)
    log.info(f"[EXTRACT] Total pages: {total_pages}")

    if config.page_range:
        start = max(0, config.page_range[0] - 1)
        end = min(total_pages, config.page_range[1])
        page_indices = list(range(start, end))
    else:
        page_indices = list(range(total_pages))

    log.info(f"[EXTRACT] Extracting pages: {[i+1 for i in page_indices]}")

    raw_elements = []
    for page_idx in page_indices:
        page_num = page_idx + 1
        page = reader.pages[page_idx]
        raw_text = page.extract_text() or ""

        lines = raw_text.split("\n")
        for line in lines:
            line = line.strip()
            if not line:
                continue
            raw_elements.append({
                "text": line,
                "page": page_num,
                "raw_type": _classify_raw(line),
            })

    log.info(f"[EXTRACT] Raw elements extracted: {len(raw_elements)}")
    return raw_elements


def _classify_raw(text: str) -> str:
    """Heuristic element type classification."""
    t = text.strip()

    # Pure page number
    if re.fullmatch(r"\d{1,4}", t):
        return "PageNumber"

    # Bullet/list item
    if UNICODE_BULLETS_RE.match(t) or NUMBERED_LIST_RE.match(t) or t.startswith("//"):
        return "ListItem"

    # Table row: has multiple whitespace-separated numeric columns
    tokens = t.split()
    numeric_count = sum(1 for tok in tokens if re.match(r"^[\d,.()\-%€$£]+$", tok))
    if len(tokens) >= 3 and numeric_count >= 2:
        return "Table"

    # Short line without period = likely a title/heading
    if len(t) < 80 and not t.endswith(".") and not t.endswith(","):
        return "Title"

    return "NarrativeText"


# ─────────────────────────────────────────────
# STAGE 2: CLEAN
# ─────────────────────────────────────────────

def clean_elements(elements: list[dict], config: ETLConfig) -> list[dict]:
    """Apply unstructured cleaners to each element's text."""
    log.info(f"[CLEAN] Cleaning {len(elements)} elements...")

    cleaned = []
    for el in elements:
        text = el["text"]

        if config.replace_unicode_quotes:
            text = replace_unicode_quotes(text)

        if config.clean_ligatures:
            text = clean_ligatures(text)

        if config.clean_extra_whitespace:
            text = clean_extra_whitespace(text)

        if config.clean_dashes and el["raw_type"] != "Table":
            text = clean_dashes(text)

        if config.clean_bullets and el["raw_type"] == "ListItem":
            text = clean_bullets(text)

        if config.clean_non_ascii:
            text = clean_non_ascii_chars(text)

        if config.fix_broken_words:
            # Fix broken words like "sale s" → "sales", "sh are" → "share"
            text = re.sub(r"(?<=[a-z])\s(?=[a-z]{1,3}\b)", "", text)

        text = text.strip()
        if text:
            cleaned.append({**el, "text": text})

    log.info(f"[CLEAN] After cleaning: {len(cleaned)} elements")
    return cleaned


# ─────────────────────────────────────────────
# STAGE 3: ENRICH
# ─────────────────────────────────────────────

def enrich_elements(elements: list[dict]) -> list[dict]:
    """
    Extract named entities from each element using unstructured extractors.
    Adds 'entities' dict to each element.
    """
    log.info(f"[ENRICH] Extracting entities from {len(elements)} elements...")

    for el in elements:
        text = el["text"]
        entities = {}

        emails = extract_email_address(text)
        if emails:
            entities["emails"] = emails

        phones = extract_us_phone_number(text)
        if phones:
            entities["phones"] = phones

        datetimes = extract_datetimetz(text)
        if datetimes:
            entities["datetimes"] = str(datetimes)

        ips = extract_ip_address(text)
        if ips:
            entities["ip_addresses"] = ips

        # Financial figures: extract numbers with currency symbols
        financials = re.findall(r"[€$£]\s?[\d,]+\.?\d*\s?(?:billion|million|trillion)?", text, re.IGNORECASE)
        if financials:
            entities["financial_figures"] = financials

        # Percentages
        percentages = re.findall(r"[-+]?\d+\.?\d*\s?%", text)
        if percentages:
            entities["percentages"] = percentages

        # Years
        years = re.findall(r"\b(19|20)\d{2}\b", text)
        if years:
            entities["years"] = list(set(years))

        el["entities"] = entities

    enriched_count = sum(1 for el in elements if el.get("entities"))
    log.info(f"[ENRICH] Elements with entities: {enriched_count}")
    return elements


# ─────────────────────────────────────────────
# STAGE 4: FILTER
# ─────────────────────────────────────────────

def filter_elements(elements: list[dict], config: ETLConfig) -> list[dict]:
    """Remove noise: page numbers, headers/footers, too-short elements, unwanted types."""
    log.info(f"[FILTER] Filtering {len(elements)} elements...")

    filtered = []
    removed = {"page_number": 0, "too_short": 0, "excluded_type": 0}

    for el in elements:
        # Remove pure page numbers
        if config.remove_page_numbers and el["raw_type"] == "PageNumber":
            removed["page_number"] += 1
            continue

        # Remove elements shorter than minimum
        if len(el["text"]) < config.min_text_length:
            removed["too_short"] += 1
            continue

        # Keep only specified types
        if config.keep_types and el["raw_type"] not in config.keep_types:
            removed["excluded_type"] += 1
            continue

        filtered.append(el)

    log.info(f"[FILTER] Removed → page_numbers: {removed['page_number']}, "
             f"too_short: {removed['too_short']}, excluded_type: {removed['excluded_type']}")
    log.info(f"[FILTER] Remaining: {len(filtered)} elements")
    return filtered


# ─────────────────────────────────────────────
# STAGE 5: CHUNK
# ─────────────────────────────────────────────

def chunk_elements_custom(elements: list[dict], config: ETLConfig) -> list[dict]:
    """
    Chunk elements using unstructured's chunking strategies.
    Wraps our dicts in unstructured Element objects, chunks, then unwraps.
    """
    if config.chunk_strategy == "none":
        log.info("[CHUNK] Chunking disabled, passing through.")
        for i, el in enumerate(elements):
            el["chunk_id"] = i
        return elements

    log.info(f"[CHUNK] Strategy: {config.chunk_strategy}, "
             f"max_chars={config.chunk_max_chars}, overlap={config.chunk_overlap}")

    # Wrap dicts into unstructured Element objects
    def make_element(el: dict) -> Element:
        type_map = {
            "Title": Title,
            "NarrativeText": NarrativeText,
            "ListItem": ListItem,
            "Table": Table,
        }
        cls = type_map.get(el["raw_type"], NarrativeText)
        obj = cls(text=el["text"])
        obj.metadata.page_number = el["page"]
        return obj

    unstructured_elements = [make_element(el) for el in elements]

    # Apply chunking
    if config.chunk_strategy == "by_title":
        chunks = chunk_by_title(
            unstructured_elements,
            max_characters=config.chunk_max_chars,
            overlap=config.chunk_overlap,
            combine_text_under_n_chars=config.chunk_combine_under_n_chars,
            multipage_sections=config.chunk_multipage_sections,
            include_orig_elements=True,
        )
    else:  # basic
        chunks = chunk_elements(
            unstructured_elements,
            max_characters=config.chunk_max_chars,
            overlap=config.chunk_overlap,
            include_orig_elements=True,
        )

    # Unwrap chunks back to dicts
    chunked = []
    for i, chunk in enumerate(chunks):
        orig = chunk.metadata.orig_elements or []
        pages = list(set(
            el.metadata.page_number for el in orig
            if el.metadata.page_number is not None
        ))
        chunked.append({
            "chunk_id": i,
            "chunk_type": type(chunk).__name__,
            "text": str(chunk),
            "char_count": len(str(chunk)),
            "pages": sorted(pages),
            "source_element_count": len(orig),
            "entities": {},
        })

    log.info(f"[CHUNK] Produced {len(chunked)} chunks")
    return chunked


# ─────────────────────────────────────────────
# STAGE 6: LOAD
# ─────────────────────────────────────────────

def load(
    elements: list[dict],
    chunks: list[dict],
    config: ETLConfig,
    source_name: str,
):
    """Export all outputs to the configured directory."""
    out = Path(config.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    base = source_name

    log.info(f"[LOAD] Writing outputs to: {out}/")

    # --- JSON (elements) ---
    if config.export_json:
        path = out / f"{base}_elements.json"
        with open(path, "w", encoding="utf-8") as f:
            json.dump(elements, f, indent=2, ensure_ascii=False, default=str)
        log.info(f"[LOAD] JSON elements  → {path} ({len(elements)} records)")

        path_chunks = out / f"{base}_chunks.json"
        with open(path_chunks, "w", encoding="utf-8") as f:
            json.dump(chunks, f, indent=2, ensure_ascii=False, default=str)
        log.info(f"[LOAD] JSON chunks    → {path_chunks} ({len(chunks)} chunks)")

    # --- CSV (elements) ---
    if config.export_csv:
        path = out / f"{base}_elements.csv"
        flat_elements = []
        for el in elements:
            flat = {k: v for k, v in el.items() if k != "entities"}
            flat["entities_json"] = json.dumps(el.get("entities", {}), ensure_ascii=False)
            flat_elements.append(flat)
        if flat_elements:
            with open(path, "w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=flat_elements[0].keys())
                writer.writeheader()
                writer.writerows(flat_elements)
        log.info(f"[LOAD] CSV elements   → {path}")

        path_chunks = out / f"{base}_chunks.csv"
        if chunks:
            with open(path_chunks, "w", newline="", encoding="utf-8") as f:
                flat_chunks = [{**c, "pages": str(c["pages"])} for c in chunks]
                writer = csv.DictWriter(f, fieldnames=flat_chunks[0].keys())
                writer.writeheader()
                writer.writerows(flat_chunks)
        log.info(f"[LOAD] CSV chunks     → {path_chunks}")

    # --- Markdown ---
    if config.export_markdown:
        path = out / f"{base}.md"
        with open(path, "w", encoding="utf-8") as f:
            f.write(f"# {base}\n\n")
            current_page = None
            for el in elements:
                if el["page"] != current_page:
                    current_page = el["page"]
                    f.write(f"\n---\n## Page {current_page}\n\n")
                if el["raw_type"] == "Title":
                    f.write(f"### {el['text']}\n\n")
                elif el["raw_type"] == "ListItem":
                    f.write(f"- {el['text']}\n")
                elif el["raw_type"] == "Table":
                    f.write(f"```\n{el['text']}\n```\n\n")
                else:
                    f.write(f"{el['text']}\n\n")
        log.info(f"[LOAD] Markdown       → {path}")

    # --- Plain text ---
    if config.export_txt:
        path = out / f"{base}.txt"
        with open(path, "w", encoding="utf-8") as f:
            current_page = None
            for el in elements:
                if el["page"] != current_page:
                    current_page = el["page"]
                    f.write(f"\n{'='*60}\nPAGE {current_page}\n{'='*60}\n\n")
                f.write(f"[{el['raw_type']}] {el['text']}\n\n")
        log.info(f"[LOAD] TXT            → {path}")

    # --- Entities ---
    if config.export_entities:
        all_entities = []
        for el in elements:
            ents = el.get("entities", {})
            if ents:
                all_entities.append({
                    "page": el["page"],
                    "text_snippet": el["text"][:80],
                    **ents,
                })
        path = out / f"{base}_entities.json"
        with open(path, "w", encoding="utf-8") as f:
            json.dump(all_entities, f, indent=2, ensure_ascii=False, default=str)
        log.info(f"[LOAD] Entities JSON  → {path} ({len(all_entities)} elements with entities)")

    # --- Summary report ---
    type_counts = {}
    for el in elements:
        type_counts[el["raw_type"]] = type_counts.get(el["raw_type"], 0) + 1

    summary = {
        "source": source_name,
        "total_elements": len(elements),
        "total_chunks": len(chunks),
        "element_type_breakdown": type_counts,
        "pages_processed": sorted(set(el["page"] for el in elements)),
        "avg_chunk_chars": round(sum(c["char_count"] for c in chunks) / max(len(chunks), 1)),
        "entities_found": sum(1 for el in elements if el.get("entities")),
        "outputs": {
            "json_elements": f"{base}_elements.json",
            "json_chunks": f"{base}_chunks.json",
            "csv_elements": f"{base}_elements.csv",
            "csv_chunks": f"{base}_chunks.csv",
            "markdown": f"{base}.md",
            "txt": f"{base}.txt",
            "entities": f"{base}_entities.json",
        }
    }

    path = out / f"{base}_summary.json"
    with open(path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    log.info(f"[LOAD] Summary        → {path}")

    return summary


# ─────────────────────────────────────────────
# PIPELINE RUNNER
# ─────────────────────────────────────────────

def run_pipeline(filepath: str, config: ETLConfig) -> dict:
    """Run the full ETL pipeline on a document."""
    source_name = Path(filepath).stem.replace(" ", "_")

    log.info("=" * 60)
    log.info(f"ETL PIPELINE START: {filepath}")
    log.info("=" * 60)

    # Stage 1: Extract
    raw = extract(filepath, config)

    # Stage 2: Clean
    cleaned = clean_elements(raw, config)

    # Stage 3: Enrich
    enriched = enrich_elements(cleaned)

    # Stage 4: Filter
    filtered = filter_elements(enriched, config)

    # Stage 5: Chunk
    chunks = chunk_elements_custom(filtered, config)

    # Stage 6: Load
    summary = load(filtered, chunks, config, source_name)

    log.info("=" * 60)
    log.info("ETL PIPELINE COMPLETE")
    log.info(f"  Elements : {summary['total_elements']}")
    log.info(f"  Chunks   : {summary['total_chunks']}")
    log.info(f"  Breakdown: {summary['element_type_breakdown']}")
    log.info(f"  Entities : {summary['entities_found']} elements with entities")
    log.info("=" * 60)

    return summary


# ─────────────────────────────────────────────
# MAIN
# ─────────────────────────────────────────────

if __name__ == "__main__":
    config = ETLConfig(
        output_dir=r"C:\windowsdisk\finceptTerminal\fincept-terminal-desktop\tmp",
        page_range=[1, 3],          # First 3 pages
        chunk_strategy="by_title",
        chunk_max_chars=1500,
        chunk_overlap=100,
        chunk_combine_under_n_chars=200,
        fix_broken_words=True,
        clean_ligatures=True,
        replace_unicode_quotes=True,
        remove_page_numbers=True,
        export_json=True,
        export_csv=True,
        export_markdown=True,
        export_txt=True,
        export_entities=True,
    )

    summary = run_pipeline(
        filepath=r"C:\Users\Tilak\Downloads\bayer-annual-report-2024.pdf",
        config=config,
    )

    print("\n=== PIPELINE SUMMARY ===")
    print(json.dumps(summary, indent=2))
