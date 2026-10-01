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
import inspect

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


_loop = None


def _resolve(value):
    """googletrans >= 4.0 made Translator.translate() a coroutine; on that version
    the old synchronous call returned an un-awaited coroutine and every
    translation failed with "'coroutine' object has no attribute 'text'". Run it
    to completion on one persistent loop (a single httpx client must not hop
    between loops, which matters for the batch command)."""
    global _loop
    if inspect.isawaitable(value):
        import asyncio
        if _loop is None:
            _loop = asyncio.new_event_loop()
        return _loop.run_until_complete(value)
    return value


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


def _gtx_translate(text, source, target):
    """Translate through Google's public web endpoint using only the standard
    library (no extra package, no API key) and return (translated, detected_lang).

    This is the primary path now: deep-translator is not installed in the app's
    venv, and the googletrans 4.x that is installed gets answered by Google with
    the input echoed back unchanged (src="en", text identical), so every
    non-English headline "translated" to itself."""
    import urllib.parse
    import urllib.request

    url = "https://translate.googleapis.com/translate_a/single?client=gtx&dt=t&" + urllib.parse.urlencode(
        {"sl": source or "auto", "tl": target})
    body = urllib.parse.urlencode({"q": text}).encode("utf-8")  # POST: no URL length limit
    req = urllib.request.Request(url, data=body, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    translated = "".join(seg[0] for seg in data[0] if seg and seg[0])
    detected = data[2] if len(data) > 2 and isinstance(data[2], str) else "auto"
    return translated, detected


def translate_single(text, source="auto", target="en"):
    """Translate a single text string."""
    if not text or not text.strip():
        return {"original": text, "translated": text, "detected_lang": "en"}

    detected = detect_language(text)

    # Already in the target language: the caller said so explicitly...
    if source == target:
        return {"original": text, "translated": text, "detected_lang": detected}
    # ...or the script says so. detect_language() only separates NON-Latin scripts;
    # every Latin-script text (French, German, Spanish, Italian...) reads as "en",
    # so for target "en" the script proves nothing and the translator has to
    # decide. (This used to skip them all: "Bonjour le monde" came back untouched
    # and the TRANSLATE button was a no-op for every Latin-script language.)
    if detected == target and detected != "en":
        return {"original": text, "translated": text, "detected_lang": detected}

    try:
        if _translator_type == "deep_translator":
            src = source if source != "auto" else "auto"
            translated = GoogleTranslator(source=src, target=target).translate(text)
            translated = translated or text
            lang = detected
            if detected == "en":
                # Latin script: unchanged output means it already was the target
                # language; otherwise the source language is unknown to us.
                lang = "en" if translated.strip() == text.strip() else "auto"
            return {"original": text, "translated": translated, "detected_lang": lang}

        # Stdlib path (see _gtx_translate); googletrans is only a last resort.
        gtx_error = None
        try:
            translated, lang = _gtx_translate(text, source, target)
            return {"original": text, "translated": translated or text, "detected_lang": lang or detected}
        except Exception as e:  # network down, endpoint changed, bad JSON...
            gtx_error = e

        if _translator_type == "googletrans":
            result = _resolve(_translator.translate(text, src=source, dest=target))
            return {
                "original": text,
                "translated": result.text,
                "detected_lang": result.src if hasattr(result, 'src') else detected,
            }
        return {"original": text, "translated": text, "detected_lang": detected,
                "error": f"Translation request failed: {gtx_error}"}
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
        # A missing library or a failed request used to report success with the
        # ORIGINAL text as the "translation"; flag it so the UI can say so.
        failed = "error" in t or "note" in t
        result = {"success": not failed, **t}
        if failed and "error" not in t:
            result["error"] = t["note"]
    elif command == "detect":
        lang = detect_language(args[1])
        result = {"success": True, "detected_lang": lang, "text": args[1][:100]}
    else:
        result = {"success": False, "error": f"Unknown command: {command}"}

    print(json.dumps(result, ensure_ascii=True))


if __name__ == "__main__":
    main()
