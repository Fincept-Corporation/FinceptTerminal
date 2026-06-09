"""
News NLP Pipeline — Entity extraction, semantic clustering, batch sentiment.
Called by NewsNlpService via PythonRunner.

Commands:
  extract_entities <json_headlines>
  cluster_semantic <json_headlines>
  analyze_sentiment_batch <json_headlines>
"""
import sys
import json
import re
import math
from collections import Counter

# ── Country/Region database ──────────────────────────────────────────────────

COUNTRIES = {
    "united states": "US", "usa": "US", "u.s.": "US", "america": "US",
    "china": "CN", "chinese": "CN", "beijing": "CN", "shanghai": "CN",
    "russia": "RU", "russian": "RU", "moscow": "RU", "kremlin": "RU",
    "ukraine": "UA", "ukrainian": "UA", "kyiv": "UA", "kiev": "UA",
    "united kingdom": "GB", "britain": "GB", "british": "GB", "london": "GB", "uk": "GB",
    "germany": "DE", "german": "DE", "berlin": "DE",
    "france": "FR", "french": "FR", "paris": "FR",
    "japan": "JP", "japanese": "JP", "tokyo": "JP",
    "india": "IN", "indian": "IN", "mumbai": "IN", "delhi": "IN",
    "brazil": "BR", "brazilian": "BR",
    "canada": "CA", "canadian": "CA",
    "australia": "AU", "australian": "AU",
    "south korea": "KR", "korean": "KR", "seoul": "KR",
    "north korea": "KP", "pyongyang": "KP",
    "iran": "IR", "iranian": "IR", "tehran": "IR",
    "israel": "IL", "israeli": "IL", "tel aviv": "IL",
    "palestine": "PS", "palestinian": "PS", "gaza": "PS", "west bank": "PS",
    "saudi arabia": "SA", "saudi": "SA", "riyadh": "SA",
    "turkey": "TR", "turkish": "TR", "ankara": "TR", "istanbul": "TR",
    "egypt": "EG", "egyptian": "EG", "cairo": "EG",
    "italy": "IT", "italian": "IT", "rome": "IT",
    "spain": "ES", "spanish": "ES", "madrid": "ES",
    "mexico": "MX", "mexican": "MX",
    "indonesia": "ID", "indonesian": "ID", "jakarta": "ID",
    "netherlands": "NL", "dutch": "NL",
    "switzerland": "CH", "swiss": "CH",
    "sweden": "SE", "swedish": "SE",
    "norway": "NO", "norwegian": "NO",
    "poland": "PL", "polish": "PL", "warsaw": "PL",
    "taiwan": "TW", "taiwanese": "TW", "taipei": "TW",
    "singapore": "SG",
    "hong kong": "HK",
    "south africa": "ZA",
    "nigeria": "NG", "nigerian": "NG", "lagos": "NG",
    "argentina": "AR",
    "colombia": "CO",
    "pakistan": "PK", "pakistani": "PK",
    "thailand": "TH", "thai": "TH", "bangkok": "TH",
    "vietnam": "VN", "vietnamese": "VN",
    "philippines": "PH", "filipino": "PH",
    "malaysia": "MY", "malaysian": "MY",
    "iraq": "IQ", "iraqi": "IQ", "baghdad": "IQ",
    "syria": "SY", "syrian": "SY", "damascus": "SY",
    "yemen": "YE", "yemeni": "YE",
    "libya": "LY", "libyan": "LY",
    "lebanon": "LB", "lebanese": "LB", "beirut": "LB",
    "afghanistan": "AF", "afghan": "AF", "kabul": "AF",
    "myanmar": "MM", "burmese": "MM",
    "ethiopia": "ET", "ethiopian": "ET",
    "sudan": "SD", "sudanese": "SD", "khartoum": "SD",
    "somalia": "SO", "somali": "SO",
    "venezuela": "VE", "venezuelan": "VE",
    "cuba": "CU", "cuban": "CU", "havana": "CU",
    "european union": "EU", "eu": "EU", "brussels": "EU",
}

# ── Organization patterns ────────────────────────────────────────────────────

ORGANIZATIONS = {
    "nato": "NATO", "united nations": "UN", "un": "UN",
    "imf": "IMF", "world bank": "World Bank",
    "federal reserve": "Federal Reserve", "the fed": "Federal Reserve",
    "ecb": "ECB", "european central bank": "ECB",
    "bank of england": "Bank of England", "boe": "Bank of England",
    "bank of japan": "Bank of Japan", "boj": "Bank of Japan",
    "peoples bank of china": "PBOC", "pboc": "PBOC",
    "sec": "SEC", "securities and exchange": "SEC",
    "opec": "OPEC", "opec+": "OPEC+",
    "world health organization": "WHO", "who": "WHO",
    "world trade organization": "WTO", "wto": "WTO",
    "pentagon": "Pentagon", "white house": "White House",
    "congress": "US Congress", "senate": "US Senate",
    "european commission": "European Commission",
    "iaea": "IAEA", "g7": "G7", "g20": "G20", "brics": "BRICS",
    "reuters": "Reuters", "bloomberg": "Bloomberg",
    "apple": "Apple", "google": "Google", "alphabet": "Alphabet",
    "microsoft": "Microsoft", "amazon": "Amazon", "meta": "Meta",
    "nvidia": "NVIDIA", "tesla": "Tesla", "openai": "OpenAI",
    "goldman sachs": "Goldman Sachs", "jpmorgan": "JPMorgan",
    "blackrock": "BlackRock", "berkshire hathaway": "Berkshire Hathaway",
    "hsbc": "HSBC", "ubs": "UBS", "credit suisse": "Credit Suisse",
    "deutsche bank": "Deutsche Bank", "barclays": "Barclays",
    "citigroup": "Citigroup", "morgan stanley": "Morgan Stanley",
}

# ── Person title patterns ────────────────────────────────────────────────────

PERSON_TITLES = [
    r"\b(president|prime minister|chancellor|king|queen|prince|princess)\s+([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)",
    r"\b(ceo|cfo|chairman|director|minister|secretary|governor|general)\s+([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)",
    r"\b(mr|mrs|ms|dr|prof)\.\s+([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)",
]

# Known leaders
KNOWN_PERSONS = {
    "biden": "Joe Biden", "trump": "Donald Trump", "obama": "Barack Obama",
    "putin": "Vladimir Putin", "xi jinping": "Xi Jinping", "xi": "Xi Jinping",
    "modi": "Narendra Modi", "macron": "Emmanuel Macron",
    "scholz": "Olaf Scholz", "sunak": "Rishi Sunak", "starmer": "Keir Starmer",
    "kishida": "Fumio Kishida", "trudeau": "Justin Trudeau",
    "zelensky": "Volodymyr Zelensky", "zelenskyy": "Volodymyr Zelensky",
    "netanyahu": "Benjamin Netanyahu",
    "erdogan": "Recep Tayyip Erdogan",
    "musk": "Elon Musk", "bezos": "Jeff Bezos", "zuckerberg": "Mark Zuckerberg",
    "cook": "Tim Cook", "nadella": "Satya Nadella", "altman": "Sam Altman",
    "dimon": "Jamie Dimon", "powell": "Jerome Powell", "lagarde": "Christine Lagarde",
    "yellen": "Janet Yellen",
}


def extract_entities(headlines_json):
    """Extract countries, organizations, people, and tickers from headlines."""
    try:
        articles = json.loads(headlines_json) if isinstance(headlines_json, str) else headlines_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    all_countries = Counter()
    all_orgs = Counter()
    all_people = Counter()
    all_tickers = Counter()
    per_article = []

    for article in articles:
        headline = article.get("headline", "")
        summary = article.get("summary", "")
        text = (headline + " " + summary).lower()
        text_orig = headline + " " + summary

        countries = []
        orgs = []
        people = []
        tickers = []

        # Country extraction — use word-boundary matching to avoid substring hits
        # (e.g. "rome" inside "jerome", "uk" inside "truck", "thai" inside "thailand")
        for pattern, code in COUNTRIES.items():
            if len(pattern) <= 4 or " " not in pattern:
                # Short or single-word patterns: require word boundaries
                if re.search(r'\b' + re.escape(pattern) + r'\b', text):
                    countries.append({"name": pattern.title(), "code": code})
                    all_countries[code] += 1
            else:
                # Multi-word patterns: simple substring is fine
                if pattern in text:
                    countries.append({"name": pattern.title(), "code": code})
                    all_countries[code] += 1

        # Organization extraction
        for pattern, name in ORGANIZATIONS.items():
            if pattern in text:
                orgs.append(name)
                all_orgs[name] += 1

        # Person extraction — known persons
        for pattern, name in KNOWN_PERSONS.items():
            if pattern in text:
                people.append(name)
                all_people[name] += 1

        # Person extraction — title patterns
        for pat in PERSON_TITLES:
            for match in re.finditer(pat, text_orig):
                name = match.group(2).strip()
                if len(name) > 3 and name not in people:
                    people.append(name)
                    all_people[name] += 1

        # Ticker extraction (uppercase 2-5 chars, filter common words)
        common = {"THE", "FOR", "AND", "BUT", "NOT", "FROM", "WITH", "THIS", "THAT",
                  "HAVE", "WILL", "BEEN", "THEY", "WERE", "SAID", "HAS", "ITS", "NEW",
                  "ARE", "WAS", "WHO", "HOW", "WHY", "ALL", "CAN", "MAY", "NOW", "SEC",
                  "GDP", "CEO", "CFO", "IPO", "ETF", "GDP", "CPI", "PMI"}
        for m in re.finditer(r'\b[A-Z]{2,5}\b', text_orig):
            t = m.group()
            if t not in common:
                tickers.append(t)
                all_tickers[t] += 1

        # Deduplicate
        countries = list({c["code"]: c for c in countries}.values())[:5]
        orgs = list(dict.fromkeys(orgs))[:5]
        people = list(dict.fromkeys(people))[:5]
        tickers = list(dict.fromkeys(tickers))[:5]

        per_article.append({
            "id": article.get("id", ""),
            "countries": countries,
            "organizations": orgs,
            "people": people,
            "tickers": tickers,
        })

    return {
        "success": True,
        "entities": per_article,
        "top_countries": [{"code": c, "count": n} for c, n in all_countries.most_common(10)],
        "top_organizations": [{"name": o, "count": n} for o, n in all_orgs.most_common(10)],
        "top_people": [{"name": p, "count": n} for p, n in all_people.most_common(10)],
        "top_tickers": [{"symbol": t, "count": n} for t, n in all_tickers.most_common(10)],
    }


def cluster_semantic(headlines_json):
    """Cluster headlines by TF-IDF cosine similarity."""
    try:
        articles = json.loads(headlines_json) if isinstance(headlines_json, str) else headlines_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    if len(articles) < 2:
        return {"success": True, "clusters": [], "method": "too_few_articles"}

    # Tokenize and build TF-IDF manually (no sklearn dependency required)
    # Synonym map: normalize common abbreviations/variants to a canonical form
    SYNONYMS = {
        "fed": "federal_reserve", "federal": "federal_reserve", "reserve": "federal_reserve",
        "ecb": "european_central_bank",
        "boj": "bank_of_japan",
        "boe": "bank_of_england",
        "pboc": "peoples_bank_china",
        "nato": "nato_alliance",
        "gop": "republican",
        "kyiv": "ukraine_capital", "kiev": "ukraine_capital",
        "sanctioned": "sanction", "sanctions": "sanction",
        "rates": "interest_rate", "rate": "interest_rate",
        "inflation": "inflation_cpi",
        "troops": "military_troops", "soldiers": "military_troops",
        "missiles": "missile", "strikes": "strike", "attacked": "attack", "attacks": "attack",
        "crashed": "crash", "crashes": "crash",
        "rallied": "rally", "rallies": "rally",
    }

    def tokenize(text):
        words = re.findall(r'[a-z]{3,}', text.lower())
        stop = {"the", "and", "for", "are", "but", "not", "you", "all", "can",
                "had", "her", "was", "one", "our", "out", "has", "its", "his",
                "how", "who", "oil", "new", "now", "old", "see", "way", "may",
                "day", "too", "any", "been", "have", "from", "with", "they",
                "will", "each", "make", "like", "been", "than", "them", "then",
                "what", "when", "that", "this", "said", "over", "into", "also",
                "more", "some", "very", "after", "about", "which", "their", "there"}
        return [SYNONYMS.get(w, w) for w in words if w not in stop]

    docs = []
    for a in articles:
        tokens = tokenize(a.get("headline", "") + " " + a.get("summary", ""))
        docs.append(tokens)

    # Build vocabulary
    vocab = {}
    df = Counter()
    for doc in docs:
        seen = set()
        for w in doc:
            if w not in vocab:
                vocab[w] = len(vocab)
            if w not in seen:
                df[w] += 1
                seen.add(w)

    n_docs = len(docs)
    if not vocab:
        return {"success": True, "clusters": [], "method": "empty_vocab"}

    # TF-IDF vectors
    def tfidf_vec(tokens):
        tf = Counter(tokens)
        vec = {}
        for w, count in tf.items():
            if w in vocab:
                idf = math.log((n_docs + 1) / (df.get(w, 0) + 1)) + 1
                vec[vocab[w]] = (count / max(len(tokens), 1)) * idf
        return vec

    vectors = [tfidf_vec(doc) for doc in docs]

    # Cosine similarity
    def cosine(a, b):
        keys = set(a.keys()) & set(b.keys())
        if not keys:
            return 0.0
        dot = sum(a[k] * b[k] for k in keys)
        norm_a = math.sqrt(sum(v * v for v in a.values()))
        norm_b = math.sqrt(sum(v * v for v in b.values()))
        if norm_a == 0 or norm_b == 0:
            return 0.0
        return dot / (norm_a * norm_b)

    # Single-pass clustering (threshold = 0.25 — catches more topically related articles)
    THRESHOLD = 0.25
    clusters = []
    assigned = set()

    for i in range(len(vectors)):
        if i in assigned:
            continue
        cluster = [i]
        assigned.add(i)
        for j in range(i + 1, len(vectors)):
            if j in assigned:
                continue
            if cosine(vectors[i], vectors[j]) >= THRESHOLD:
                cluster.append(j)
                assigned.add(j)
        clusters.append(cluster)

    result_clusters = []
    for indices in clusters:
        if len(indices) < 2:
            continue
        items = [{"id": articles[i].get("id", ""), "headline": articles[i].get("headline", "")} for i in indices]
        result_clusters.append({
            "primary": items[0],
            "items": items,
            "size": len(items),
        })

    result_clusters.sort(key=lambda c: c["size"], reverse=True)

    return {
        "success": True,
        "clusters": result_clusters[:20],
        "method": "tfidf_cosine",
        "total_articles": len(articles),
        "clustered_count": sum(c["size"] for c in result_clusters),
    }


# Finance keyword lexicon — doubles as the VADER lexicon nudge and the offline
# fallback scorer. Weights are valences on VADER's [-4, 4] scale.
_FINANCE_POSITIVE = {
    "surge": 3.0, "soar": 3.2, "skyrocket": 3.5, "breakthrough": 3.0, "boom": 2.8,
    "rally": 2.5, "gain": 2.0, "gains": 2.0, "rise": 1.8, "jump": 2.5,
    "jumps": 2.5, "climb": 2.0, "rebound": 2.2, "boost": 2.0, "beat": 2.2,
    "beats": 2.2, "exceed": 2.0, "outperform": 2.5, "upgrade": 2.5, "profit": 1.8,
    "growth": 1.8, "recover": 2.0, "record": 1.5, "strong": 1.5, "robust": 1.5,
    "bullish": 3.0, "optimism": 1.8, "milestone": 1.5, "approval": 1.8, "deal": 1.2,
}
_FINANCE_NEGATIVE = {
    "crash": -3.5, "plunge": -3.2, "collapse": -3.5, "meltdown": -3.5, "bankruptcy": -3.8,
    "fall": -1.8, "drop": -2.0, "decline": -2.0, "tumble": -2.5, "slump": -2.5,
    "miss": -2.0, "misses": -2.0, "fail": -2.2, "recession": -2.8, "crisis": -2.5,
    "sanction": -2.0, "tariff": -1.8, "escalat": -1.5, "layoff": -2.2, "layoffs": -2.2,
    "downgrade": -2.5, "fraud": -3.5, "scandal": -2.8, "selloff": -2.5, "default": -2.5,
    "lawsuit": -1.8, "probe": -1.5, "weak": -1.5, "loss": -1.5, "deficit": -1.2,
    "fear": -1.5, "threat": -1.5, "warning": -1.5, "bearish": -3.0, "halt": -1.5,
}

# Integer-weighted view used by the keyword fallback scorer (and confidence).
_KW_POSITIVE = {k: max(1, int(round(abs(v) / 1.6))) for k, v in _FINANCE_POSITIVE.items()}
_KW_NEGATIVE = {k: max(1, int(round(abs(v) / 1.6))) for k, v in _FINANCE_NEGATIVE.items()}

_VADER = None
_VADER_TRIED = False


def _get_vader():
    """Lazily build a finance-tuned VADER analyzer; None if unavailable."""
    global _VADER, _VADER_TRIED
    if _VADER_TRIED:
        return _VADER
    _VADER_TRIED = True
    try:
        from vaderSentiment.vaderSentiment import SentimentIntensityAnalyzer
        analyzer = SentimentIntensityAnalyzer()
        analyzer.lexicon.update(_FINANCE_POSITIVE)
        analyzer.lexicon.update(_FINANCE_NEGATIVE)
        _VADER = analyzer
    except Exception:
        _VADER = None
    return _VADER


def _keyword_signals(text):
    """Finance keyword hit counts (used for fallback scoring and confidence)."""
    pos = sum(w for p, w in _KW_POSITIVE.items() if p in text)
    neg = sum(w for p, w in _KW_NEGATIVE.items() if p in text)
    return pos, neg


def _label_from_score(score):
    if score >= 0.15:
        return "BULLISH"
    if score <= -0.15:
        return "BEARISH"
    return "NEUTRAL"


def analyze_sentiment_batch(headlines_json):
    """Batch sentiment analysis with confidence scores.

    Prefers VADER (finance-tuned) when ``vaderSentiment`` is installed; otherwise
    falls back to the offline keyword scorer. Output schema is stable across both
    engines; ``engine`` reports which path ran."""
    try:
        articles = json.loads(headlines_json) if isinstance(headlines_json, str) else headlines_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    analyzer = _get_vader()
    engine = "vader" if analyzer else "lexicon"

    results = []
    for article in articles:
        text = (article.get("headline", "") + " " + article.get("summary", "")).lower()
        pos_score, neg_score = _keyword_signals(text)
        total = pos_score + neg_score

        if analyzer is not None:
            score = analyzer.polarity_scores(text)["compound"]  # -1..1
            sentiment = _label_from_score(score)
            if sentiment == "NEUTRAL":
                confidence = round(min(0.2 + abs(score) * 0.8, 0.4), 3)
            else:
                confidence = round(min(0.45 + abs(score) * 0.45 + total * 0.02, 0.97), 3)
        else:
            net = pos_score - neg_score
            if total == 0:
                sentiment, score, confidence = "NEUTRAL", 0.0, 0.2
            elif net >= 2:
                sentiment = "BULLISH"
                score = min(net / max(total, 1), 1.0)
                confidence = min(0.4 + total * 0.05, 0.95)
            elif net <= -2:
                sentiment = "BEARISH"
                score = max(net / max(total, 1), -1.0)
                confidence = min(0.4 + total * 0.05, 0.95)
            else:
                sentiment = "NEUTRAL"
                score = net / max(total, 1)
                confidence = 0.3

        results.append({
            "id": article.get("id", ""),
            "sentiment": sentiment,
            "score": round(score, 3),
            "confidence": round(confidence, 3),
            "positive_signals": pos_score,
            "negative_signals": neg_score,
        })

    # Aggregate
    bull = sum(1 for r in results if r["sentiment"] == "BULLISH")
    bear = sum(1 for r in results if r["sentiment"] == "BEARISH")
    neut = sum(1 for r in results if r["sentiment"] == "NEUTRAL")

    return {
        "success": True,
        "engine": engine,
        "results": results,
        "aggregate": {"bullish": bull, "bearish": bear, "neutral": neut},
        "overall_score": round(sum(r["score"] for r in results) / max(len(results), 1), 3),
    }


def resolve_arg(arg):
    """If arg starts with '@', read content from that file path and delete it."""
    if arg and arg.startswith("@"):
        path = arg[1:]
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = f.read()
            try:
                import os
                os.remove(path)
            except Exception:
                pass
            return data
        except Exception as e:
            return arg  # fallback: return as-is
    return arg


def main(args=None):
    if args is None:
        args = sys.argv[1:]

    if len(args) < 2:
        print(json.dumps({"success": False, "error": "Usage: news_nlp.py <command> <json_data>"}))
        return

    command = args[0]
    data = resolve_arg(args[1])

    if command == "extract_entities":
        result = extract_entities(data)
    elif command == "cluster_semantic":
        result = cluster_semantic(data)
    elif command == "analyze_sentiment_batch":
        result = analyze_sentiment_batch(data)
    else:
        result = {"success": False, "error": f"Unknown command: {command}"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
