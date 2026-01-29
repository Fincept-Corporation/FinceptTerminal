"""
Simple translation script - returns text as-is (English is default)
For production: integrate Google Translate API or other translation service
"""
import sys
import json

def main():
    if len(sys.argv) < 3:
        print(json.dumps({"success": False, "error": "Usage: translate_text.py batch <json_array> <source_lang> <target_lang>"}))
        return

    mode = sys.argv[1]

    if mode == "batch":
        texts_json = sys.argv[2]
        source_lang = sys.argv[3] if len(sys.argv) > 3 else "auto"
        target_lang = sys.argv[4] if len(sys.argv) > 4 else "en"

        try:
            texts = json.loads(texts_json)

            # For now, return text as-is (no translation)
            # In production, integrate with translation API
            translations = []
            for text in texts:
                translations.append({
                    "original": text,
                    "translated": text,  # No translation for now
                    "detected_lang": "zh" if any('\u4e00' <= c <= '\u9fff' for c in str(text)) else "en"
                })

            result = {
                "success": True,
                "translations": translations,
                "error": None
            }

            print(json.dumps(result, ensure_ascii=True))
        except Exception as e:
            print(json.dumps({"success": False, "error": str(e)}))
    else:
        print(json.dumps({"success": False, "error": f"Unknown mode: {mode}"}))

if __name__ == "__main__":
    main()
