import feedparser
from fincept_terminal.utils.themes import console


def fetch_rss_feed_news(rss_feed_url):
    """
    Fetch and parse the latest 10 news articles from the given RSS feed URL.

    Parameters:
    rss_feed_url (str): The RSS feed URL.

    Returns:
    list: A list of up to 10 news articles, each represented as a dictionary with 'title' and 'link' keys.
    """
    try:
        console.print(f"[cyan]Fetching RSS feed from URL: {rss_feed_url}[/cyan]")
        feed = feedparser.parse(rss_feed_url)

        # Check for parsing errors
        if feed.bozo:
            console.print(f"[bold red]Error parsing RSS feed: {feed.bozo_exception}[/bold red]")
            return []

        # Check if the feed contains entries
        if not feed.entries:
            console.print(f"[bold yellow]No articles found in the RSS feed: {rss_feed_url}[/bold yellow]")
            return []

        # Extract up to 10 articles
        articles = []
        for entry in feed.entries[:10]:  # Limit to 10 articles
            articles.append({"title": entry.title, "link": entry.link})

        # Return the latest 10 articles
        return articles

    except Exception as e:
        console.print(f"[bold red]Error fetching RSS feed: {str(e)}[/bold red]")
        return []