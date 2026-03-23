"""
Translation service — translates text via deep-translator (Google Translate free tier).
Falls back to returning original text if library unavailable.

Commands:
  batch <json_array> [source_lang] [target_lang]
  single <text> [source_lang] [target_lang]
  detect <text>
"""
import sys
import json

# Try importing translation library
_translator = None
_translator_type = "none"

try:
    from deep_translator import GoogleTranslator
    _translator_type = "deep_translator"
except ImportError:
    try:
        from googletrans import Translator
        _translator = Translator()
        _translator_type = "googletrans"
    except ImportError:
        pass


def detect_language(text):
    """Detect language of text using character analysis."""
    if not text:
        return "en"

    # CJK characters
    cjk = sum(1 for c in text if '\u4e00' <= c <= '\u9fff')
    # Japanese kana
    jp = sum(1 for c in text if '\u3040' <= c <= '\u30ff')
    # Korean hangul
    kr = sum(1 for c in text if '\uac00' <= c <= '\ud7af')
    # Arabic
    ar = sum(1 for c in text if '\u0600' <= c <= '\u06ff')
    # Cyrillic
    cy = sum(1 for c in text if '\u0400' <= c <= '\u04ff')
    # Devanagari (Hindi)
    dv = sum(1 for c in text if '\u0900' <= c <= '\u097f')

    total = len(text)
    if total == 0:
        return "en"

    if cjk / total > 0.1:
        return "zh"
    if jp / total > 0.1:
        return "ja"
    if kr / total > 0.1:
        return "ko"
    if ar / total > 0.1:
        return "ar"
    if cy / total > 0.1:
        return "ru"
    if dv / total > 0.1:
        return "hi"

    return "en"


def translate_single(text, source="auto", target="en"):
    """Translate a single text string."""
    if not text or not text.strip():
        return {"original": text, "translated": text, "detected_lang": "en"}

    detected = detect_language(text)

    # If already in target language, skip
    if detected == target and source in ("auto", target):
        return {"original": text, "translated": text, "detected_lang": detected}

    try:
        if _translator_type == "deep_translator":
            src = source if source != "auto" else "auto"
            translated = GoogleTranslator(source=src, target=target).translate(text)
            return {"original": text, "translated": translated or text, "detected_lang": detected}
        elif _translator_type == "googletrans":
            result = _translator.translate(text, src=source, dest=target)
            return {
                "original": text,
                "translated": result.text,
                "detected_lang": result.src if hasattr(result, 'src') else detected,
            }
        else:
            # No translator available — return original with detected language
            return {"original": text, "translated": text, "detected_lang": detected,
                    "note": "No translation library installed (pip install deep-translator)"}
    except Exception as e:
        return {"original": text, "translated": text, "detected_lang": detected,
                "error": str(e)}


def translate_batch(texts_json, source="auto", target="en"):
    """Translate a batch of texts."""
    try:
        texts = json.loads(texts_json) if isinstance(texts_json, str) else texts_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    translations = []
    for text in texts:
        if isinstance(text, dict):
            # Support {"text": "...", "id": "..."} format
            t = translate_single(text.get("text", ""), source, target)
            t["id"] = text.get("id", "")
        else:
            t = translate_single(str(text), source, target)
        translations.append(t)

    return {
        "success": True,
        "translations": translations,
        "translator": _translator_type,
        "target_lang": target,
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]

    if len(args) < 2:
        print(json.dumps({"success": False,
                          "error": "Usage: translate_text.py <batch|single|detect> <text_or_json> [source] [target]"}))
        return

    command = args[0]
    source = args[2] if len(args) > 2 else "auto"
    target = args[3] if len(args) > 3 else "en"

    if command == "batch":
        result = translate_batch(args[1], source, target)
    elif command == "single":
        t = translate_single(args[1], source, target)
        result = {"success": True, **t}
    elif command == "detect":
        lang = detect_language(args[1])
        result = {"success": True, "detected_lang": lang, "text": args[1][:100]}
    else:
        result = {"success": False, "error": f"Unknown command: {command}"}

    print(json.dumps(result, ensure_ascii=True))


if __name__ == "__main__":
    main()
