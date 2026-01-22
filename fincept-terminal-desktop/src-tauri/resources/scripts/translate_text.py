"""
Text Translation Script using googletrans
Translates text from Chinese to English or any other language pair
Optimized with connection reuse and error handling
"""

import sys
import json
import asyncio
from googletrans import Translator

# Reusable translator instance for better performance
_translator = None

def get_translator():
    """Get or create a reusable translator instance"""
    global _translator
    if _translator is None:
        _translator = Translator()
    return _translator

async def translate_text_async(text, source_lang='auto', target_lang='en'):
    """
    Translate text using googletrans library (async)

    Args:
        text: Text to translate
        source_lang: Source language code (default: 'auto' for auto-detect)
        target_lang: Target language code (default: 'en')

    Returns:
        dict with success status and translated text
    """
    try:
        translator = get_translator()
        result = await translator.translate(text, src=source_lang, dest=target_lang)

        return {
            "success": True,
            "translated_text": result.text,
            "detected_lang": result.src,
            "confidence": getattr(result, 'confidence', None)
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "original_text": text
        }

async def translate_batch_async(texts, source_lang='auto', target_lang='en'):
    """
    Translate multiple texts in batch (async)

    Args:
        texts: List of texts to translate
        source_lang: Source language code
        target_lang: Target language code

    Returns:
        dict with success status and list of translations
    """
    try:
        translator = get_translator()
        translations = []

        # Translate each text individually for better reliability
        for text in texts:
            try:
                result = await translator.translate(text, src=source_lang, dest=target_lang)
                translations.append({
                    "original": text,
                    "translated": result.text if result else text,
                    "detected_lang": result.src if result else source_lang
                })
            except Exception as e:
                # If individual translation fails, keep original text
                translations.append({
                    "original": text,
                    "translated": text,
                    "detected_lang": source_lang,
                    "error": str(e)
                })

        return {
            "success": True,
            "translations": translations,
            "count": len(translations)
        }
    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "count": 0
        }

def translate_text(text, source_lang='auto', target_lang='en'):
    """Sync wrapper for async translate_text"""
    return asyncio.run(translate_text_async(text, source_lang, target_lang))

def translate_batch(texts, source_lang='auto', target_lang='en'):
    """Sync wrapper for async translate_batch"""
    return asyncio.run(translate_batch_async(texts, source_lang, target_lang))

def main():
    """Main function to handle command-line translation requests"""
    # Reconfigure stdout to UTF-8
    if sys.platform == 'win32':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Missing parameters. Usage: translate_text.py <mode> <text_or_json> [source_lang] [target_lang]"
        }))
        sys.exit(1)

    mode = sys.argv[1]  # 'single' or 'batch'

    if mode == 'single':
        # Single text translation
        if len(sys.argv) < 3:
            print(json.dumps({
                "success": False,
                "error": "Missing text parameter"
            }))
            sys.exit(1)

        text = sys.argv[2]
        source_lang = sys.argv[3] if len(sys.argv) > 3 else 'auto'
        target_lang = sys.argv[4] if len(sys.argv) > 4 else 'en'

        result = translate_text(text, source_lang, target_lang)
        print(json.dumps(result, ensure_ascii=False))

    elif mode == 'batch':
        # Batch translation
        if len(sys.argv) < 3:
            print(json.dumps({
                "success": False,
                "error": "Missing texts parameter"
            }))
            sys.exit(1)

        try:
            texts = json.loads(sys.argv[2])
            if not isinstance(texts, list):
                raise ValueError("Texts must be a JSON array")
        except Exception as e:
            print(json.dumps({
                "success": False,
                "error": f"Invalid JSON: {str(e)}"
            }))
            sys.exit(1)

        source_lang = sys.argv[3] if len(sys.argv) > 3 else 'auto'
        target_lang = sys.argv[4] if len(sys.argv) > 4 else 'en'

        result = translate_batch(texts, source_lang, target_lang)
        print(json.dumps(result, ensure_ascii=False))

    else:
        print(json.dumps({
            "success": False,
            "error": f"Invalid mode: {mode}. Use 'single' or 'batch'"
        }))
        sys.exit(1)

if __name__ == "__main__":
    main()
