#!/usr/bin/env python3
"""
Fincept Terminal Auto-Translation Script
Translates all English locale JSON files to target languages using googletrans.

Usage:
  python scripts/generate_translations.py                    # All languages, all namespaces
  python scripts/generate_translations.py --lang es fr       # Specific languages
  python scripts/generate_translations.py --ns common auth   # Specific namespaces
  python scripts/generate_translations.py --force            # Overwrite existing translations
  python scripts/generate_translations.py --dry-run          # Preview without writing
"""

import os
import sys
import json
import time
import asyncio
import argparse
from pathlib import Path

try:
    from googletrans import Translator
except ImportError:
    print("ERROR: googletrans not installed. Run: pip install googletrans")
    sys.exit(1)

# Configuration
LOCALES_DIR = Path(__file__).parent.parent / 'public' / 'locales'
SOURCE_LANG = 'en'
TARGET_LANGUAGES = [
    'ar', 'bn', 'de', 'es', 'fr', 'hi', 'id', 'it',
    'ja', 'ko', 'pl', 'pt', 'ro', 'ru', 'th', 'tr', 'uk', 'vi', 'zh'
]

# Rate limiting
BATCH_SIZE = 50
BATCH_DELAY = 0.3
ERROR_DELAY = 2.0
MAX_RETRIES = 3

async def get_translator():
    return Translator()


def load_json(filepath: Path) -> dict:
    if not filepath.exists():
        return {}
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError):
        return {}


def save_json(filepath: Path, data: dict):
    filepath.parent.mkdir(parents=True, exist_ok=True)
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write('\n')


def flatten_dict(d: dict, prefix: str = '') -> dict:
    items = {}
    for k, v in d.items():
        key = f"{prefix}.{k}" if prefix else k
        if isinstance(v, dict):
            items.update(flatten_dict(v, key))
        else:
            items[key] = v
    return items


def unflatten_dict(d: dict) -> dict:
    result = {}
    for key, value in d.items():
        parts = key.split('.')
        current = result
        for part in parts[:-1]:
            if part not in current:
                current[part] = {}
            current = current[part]
        current[parts[-1]] = value
    return result


def should_translate(value) -> bool:
    if not isinstance(value, str):
        return False
    if not value.strip():
        return False
    # Skip pure interpolation variables
    stripped = value.strip()
    if stripped.startswith('{{') and stripped.endswith('}}'):
        return False
    # Skip URLs
    if stripped.startswith('http://') or stripped.startswith('https://'):
        return False
    # Skip single characters or pure numbers
    if len(stripped) <= 1:
        return False
    return True


def is_placeholder(value: str, lang: str) -> bool:
    return isinstance(value, str) and value.startswith(f'[{lang.upper()}]')


async def translate_text(translator, text: str, target_lang: str) -> str:
    if not text or not isinstance(text, str):
        return text

    for attempt in range(MAX_RETRIES):
        try:
            result = await translator.translate(text, src=SOURCE_LANG, dest=target_lang)
            return result.text
        except Exception as e:
            if attempt < MAX_RETRIES - 1:
                await asyncio.sleep(ERROR_DELAY * (attempt + 1))
            else:
                print(f"    WARN: Failed '{text[:40]}...' -> {target_lang}: {e}")
                return text
    return text


async def translate_namespace(namespace: str, target_lang: str, force: bool = False, dry_run: bool = False) -> dict:
    en_file = LOCALES_DIR / SOURCE_LANG / f'{namespace}.json'
    target_file = LOCALES_DIR / target_lang / f'{namespace}.json'

    if not en_file.exists():
        return {'translated': 0, 'kept': 0}

    en_data = load_json(en_file)
    existing_data = load_json(target_file)

    en_flat = flatten_dict(en_data)
    existing_flat = flatten_dict(existing_data)

    result_flat = {}
    keys_to_translate = []

    for key, en_value in en_flat.items():
        if not should_translate(en_value):
            result_flat[key] = en_value
            continue

        existing_value = existing_flat.get(key)

        if existing_value and not force and not is_placeholder(existing_value, target_lang):
            result_flat[key] = existing_value
            continue

        keys_to_translate.append(key)

    translated_count = 0
    translator = await get_translator()

    if not dry_run and keys_to_translate:
        for i in range(0, len(keys_to_translate), BATCH_SIZE):
            batch = keys_to_translate[i:i + BATCH_SIZE]
            for key in batch:
                translated = await translate_text(translator, en_flat[key], target_lang)
                result_flat[key] = translated
                translated_count += 1

            if i + BATCH_SIZE < len(keys_to_translate):
                await asyncio.sleep(BATCH_DELAY)
    elif dry_run:
        translated_count = len(keys_to_translate)
        for key in keys_to_translate:
            result_flat[key] = f"[WOULD_TRANSLATE] {en_flat[key]}"

    if not dry_run and (translated_count > 0 or not target_file.exists()):
        save_json(target_file, unflatten_dict(result_flat))

    kept = len(en_flat) - translated_count
    return {'translated': translated_count, 'kept': kept}


async def main():
    parser = argparse.ArgumentParser(description='Auto-translate Fincept Terminal locale files')
    parser.add_argument('--lang', nargs='*', help='Target languages (default: all)')
    parser.add_argument('--ns', nargs='*', help='Namespaces to translate (default: all)')
    parser.add_argument('--force', action='store_true', help='Overwrite existing translations')
    parser.add_argument('--dry-run', action='store_true', help='Preview without writing')
    args = parser.parse_args()

    languages = args.lang if args.lang else TARGET_LANGUAGES

    en_dir = LOCALES_DIR / SOURCE_LANG
    if args.ns:
        namespaces = args.ns
    else:
        namespaces = [f.stem for f in en_dir.glob('*.json')]

    print(f"{'[DRY RUN] ' if args.dry_run else ''}Fincept Auto-Translation")
    print(f"Languages: {', '.join(languages)}")
    print(f"Namespaces: {len(namespaces)} files")
    print(f"Force overwrite: {args.force}")
    print("-" * 50)

    total_translated = 0

    for lang in languages:
        print(f"\n  {lang.upper()}:")
        lang_translated = 0

        for ns in sorted(namespaces):
            stats = await translate_namespace(ns, lang, force=args.force, dry_run=args.dry_run)
            lang_translated += stats['translated']

            if stats['translated'] > 0:
                print(f"    {ns}: {stats['translated']} translated, {stats['kept']} kept")

        total_translated += lang_translated
        if lang_translated == 0:
            print(f"    (all up to date)")

    print("\n" + "=" * 50)
    print(f"TOTAL: {total_translated} strings translated")
    if args.dry_run:
        print("(Dry run - no files modified)")


if __name__ == '__main__':
    asyncio.run(main())
