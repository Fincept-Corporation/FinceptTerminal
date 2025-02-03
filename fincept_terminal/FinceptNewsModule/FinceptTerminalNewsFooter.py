import feedparser
import asyncio
import random
from textual.screen import Screen
from textual.widgets import DataTable

# Set a custom User-Agent to prevent RSS feeds from blocking requests
feedparser.USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"


class NewsManager:
    """Manages fetching and displaying RSS headlines asynchronously."""

    def __init__(self):
        self.latest_items = []  # Stores fetched news items
        self.feeds = [
            "https://www.business-standard.com/rss/home_page_top_stories.rss",
            "https://www.business-standard.com/rss/latest.rss",
            "https://www.business-standard.com/rss/markets-106.rss",
        ]
        self.is_running = False  # Prevents multiple loops from running

    async def fetch_news(self):
        """Fetch RSS feeds asynchronously."""

        async def fetch_feed(feed_url):
            """Fetch a single feed in a separate thread."""
            return await asyncio.to_thread(feedparser.parse, feed_url)

        try:
            tasks = [fetch_feed(feed) for feed in self.feeds]
            parsed_feeds = await asyncio.gather(*tasks)

            all_items = []
            for parsed in parsed_feeds:
                for entry in parsed.entries:
                    title = entry.get("title", "No Title").strip()
                    desc = (entry.get("description") or entry.get("summary") or "").strip()
                    date = (entry.get("pubDate") or entry.get("published") or "").strip()
                    all_items.append((title, desc, date))

            # Return a random selection of 5 articles, or all if fewer than 5
            return random.sample(all_items, 5) if len(all_items) > 5 else all_items

        except Exception as e:
            print(f"⚠️ Error fetching news: {e}")
            return []

    async def display_in_table(self, screen: Screen, items: list):
        """Update the DataTable with fresh news."""
        try:
            data_table = screen.query_one("#news-table", DataTable)

            # If table exists, clear it before adding new data
            data_table.clear(columns=True)
            data_table.add_columns("Title", "Description", "Date")

            if not items:
                data_table.add_row("No news found", "", "")
            else:
                for (title, desc, date) in items:
                    data_table.add_row(title, desc, date)

        except Exception as e:
            print(f"⚠️ Error updating table: {e}")

    async def auto_refresh_news(self, screen: Screen):
        """Background task to refresh news every 10 seconds."""
        if self.is_running:
            return  # Prevent multiple loops from running
        self.is_running = True

        while True:
            fetched = await self.fetch_news()

            if fetched and fetched != self.latest_items:
                self.latest_items = fetched  # Store new items
                await self.display_in_table(screen, self.latest_items)

            await asyncio.sleep(10)  # Refresh every 10 seconds

    def start_background_refresh(self, screen: Screen):
        """Start fetching news in the background without blocking UI."""
        asyncio.create_task(self.auto_refresh_news(screen))  # Runs in parallel
