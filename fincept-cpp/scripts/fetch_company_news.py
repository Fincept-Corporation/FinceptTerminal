"""
Fetch company news using GNews
This script fetches latest news articles for a given company/symbol
"""

import sys
import json
from datetime import datetime
from gnews import GNews

def fetch_company_news(query, max_results=10, period='7d', language='en', country='US'):
    """
    Fetch news articles for a company

    Args:
        query: Company name or symbol to search
        max_results: Maximum number of articles to return (default: 10)
        period: Time period - format: '7d', '1M', '1y' (default: '7d')
        language: Language code (default: 'en')
        country: Country code (default: 'US')

    Returns:
        JSON string with news articles
    """
    try:
        # Initialize GNews
        google_news = GNews(
            language=language,
            country=country,
            period=period,
            max_results=max_results
        )

        # Fetch news
        news = google_news.get_news(query)

        if not news:
            return json.dumps({
                "success": True,
                "data": [],
                "query": query,
                "count": 0,
                "message": "No news found for this query"
            })

        # Process and format news articles
        articles = []
        for article in news:
            processed_article = {
                "title": article.get('title', 'N/A'),
                "description": article.get('description', 'N/A'),
                "url": article.get('url', '#'),
                "publisher": article.get('publisher', {}).get('title', 'Unknown'),
                "publisher_url": article.get('publisher', {}).get('href', '#'),
                "published_date": article.get('published date', 'Unknown'),
                "image_url": None  # GNews doesn't provide images directly
            }
            articles.append(processed_article)

        return json.dumps({
            "success": True,
            "data": articles,
            "query": query,
            "count": len(articles),
            "period": period,
            "language": language,
            "country": country
        })

    except Exception as e:
        return json.dumps({
            "success": False,
            "error": str(e),
            "query": query
        })


def fetch_news_by_topic(topic, max_results=10):
    """
    Fetch news by topic

    Args:
        topic: Topic to search (WORLD, NATION, BUSINESS, TECHNOLOGY, ENTERTAINMENT, SPORTS, SCIENCE, HEALTH)
        max_results: Maximum number of articles

    Returns:
        JSON string with news articles
    """
    try:
        google_news = GNews(max_results=max_results)
        news = google_news.get_news_by_topic(topic)

        if not news:
            return json.dumps({
                "success": True,
                "data": [],
                "topic": topic,
                "count": 0
            })

        articles = []
        for article in news:
            processed_article = {
                "title": article.get('title', 'N/A'),
                "description": article.get('description', 'N/A'),
                "url": article.get('url', '#'),
                "publisher": article.get('publisher', {}).get('title', 'Unknown'),
                "publisher_url": article.get('publisher', {}).get('href', '#'),
                "published_date": article.get('published date', 'Unknown'),
            }
            articles.append(processed_article)

        return json.dumps({
            "success": True,
            "data": articles,
            "topic": topic,
            "count": len(articles)
        })

    except Exception as e:
        return json.dumps({
            "success": False,
            "error": str(e),
            "topic": topic
        })


def get_full_article(url):
    """
    Get full article content from URL

    Args:
        url: Article URL

    Returns:
        JSON string with article content
    """
    try:
        google_news = GNews()
        article = google_news.get_full_article(url)

        if article:
            return json.dumps({
                "success": True,
                "data": {
                    "title": article.title if hasattr(article, 'title') else 'N/A',
                    "text": article.text if hasattr(article, 'text') else 'N/A',
                    "authors": article.authors if hasattr(article, 'authors') else [],
                    "publish_date": str(article.publish_date) if hasattr(article, 'publish_date') else 'Unknown',
                    "top_image": article.top_image if hasattr(article, 'top_image') else None,
                    "url": url
                }
            })
        else:
            return json.dumps({
                "success": False,
                "error": "Could not fetch article content",
                "url": url
            })

    except Exception as e:
        return json.dumps({
            "success": False,
            "error": str(e),
            "url": url
        })


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(json.dumps({
            "success": False,
            "error": "Usage: python fetch_company_news.py <command> <args>",
            "commands": {
                "search": "python fetch_company_news.py search <query> [max_results] [period] [language] [country]",
                "topic": "python fetch_company_news.py topic <topic> [max_results]",
                "article": "python fetch_company_news.py article <url>",
                "help": "python fetch_company_news.py help"
            }
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "help":
            help_text = {
                "description": "Company News Fetcher - Get latest news articles using Google News",
                "commands": {
                    "search": {
                        "description": "Search news by company name or symbol",
                        "usage": "python fetch_company_news.py search <query> [max_results] [period] [language] [country]",
                        "arguments": {
                            "query": "Company name or symbol (e.g., 'Apple', 'AAPL', 'Tesla Inc')",
                            "max_results": "Maximum articles to return (default: 10)",
                            "period": "Time period: '7d', '1M', '1y' (default: '7d')",
                            "language": "Language code (default: 'en')",
                            "country": "Country code (default: 'US')"
                        },
                        "examples": [
                            "python fetch_company_news.py search 'Apple Inc'",
                            "python fetch_company_news.py search TSLA 20 1M",
                            "python fetch_company_news.py search 'Microsoft' 15 7d en US"
                        ]
                    },
                    "topic": {
                        "description": "Get news by topic",
                        "usage": "python fetch_company_news.py topic <topic> [max_results]",
                        "arguments": {
                            "topic": "Topic name (WORLD, NATION, BUSINESS, TECHNOLOGY, ENTERTAINMENT, SPORTS, SCIENCE, HEALTH)",
                            "max_results": "Maximum articles to return (default: 10)"
                        },
                        "examples": [
                            "python fetch_company_news.py topic BUSINESS",
                            "python fetch_company_news.py topic TECHNOLOGY 20"
                        ]
                    },
                    "article": {
                        "description": "Get full article content from URL",
                        "usage": "python fetch_company_news.py article <url>",
                        "arguments": {
                            "url": "Full article URL"
                        },
                        "examples": [
                            "python fetch_company_news.py article 'https://example.com/news/article'"
                        ]
                    }
                }
            }
            print(json.dumps(help_text, indent=2))

        elif command == "search":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "success": False,
                    "error": "Usage: python fetch_company_news.py search <query> [max_results] [period] [language] [country]"
                }))
                sys.exit(1)

            query = sys.argv[2]
            max_results = int(sys.argv[3]) if len(sys.argv) > 3 else 10
            period = sys.argv[4] if len(sys.argv) > 4 else '7d'
            language = sys.argv[5] if len(sys.argv) > 5 else 'en'
            country = sys.argv[6] if len(sys.argv) > 6 else 'US'

            result = fetch_company_news(query, max_results, period, language, country)
            print(result)

        elif command == "topic":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "success": False,
                    "error": "Usage: python fetch_company_news.py topic <topic> [max_results]"
                }))
                sys.exit(1)

            topic = sys.argv[2]
            max_results = int(sys.argv[3]) if len(sys.argv) > 3 else 10

            result = fetch_news_by_topic(topic, max_results)
            print(result)

        elif command == "article":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "success": False,
                    "error": "Usage: python fetch_company_news.py article <url>"
                }))
                sys.exit(1)

            url = sys.argv[2]
            result = get_full_article(url)
            print(result)

        else:
            print(json.dumps({
                "success": False,
                "error": f"Unknown command: {command}. Use 'help' for available commands."
            }))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": str(e)
        }))
        sys.exit(1)
