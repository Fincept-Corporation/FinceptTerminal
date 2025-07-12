import dearpygui.dearpygui as dpg
import requests
import xml.etree.ElementTree as ET
import webbrowser
from base_tab import BaseTab

class RssTab(BaseTab):
    """Tab for fetching and displaying NSE RSS announcements."""

    DEFAULT_RSS_URL = "https://nsearchives.nseindia.com/content/RSS/Online_announcements.xml"
    HEADERS = {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) '
                      'AppleWebKit/537.36 (KHTML, like Gecko) '
                      'Chrome/115.0.0.0 Safari/537.36',
        'Accept': 'application/rss+xml, application/xml;q=0.9, */*;q=0.8',
        'Accept-Language': 'en-US,en;q=0.9',
        'Connection': 'keep-alive'
    }

    def __init__(self, app):
        super().__init__(app)
        self.feed_items = []
        self.current_url = self.DEFAULT_RSS_URL
        self.fetch_feed(self.current_url)

    def get_label(self):
        return "Announcements"

    def fetch_feed(self, url):
        """Fetch and parse RSS feed items from the given URL."""
        try:
            response = requests.get(url, headers=self.HEADERS, timeout=10)
            response.raise_for_status()
            root = ET.fromstring(response.content)
            channel = root.find('channel') or root
            items = []
            for item in channel.findall('item'):
                items.append({
                    'title': item.findtext('title', ''),
                    'link': item.findtext('link', ''),
                    'description': item.findtext('description', ''),
                    'pubDate': item.findtext('pubDate', '')
                })
            self.feed_items = items
        except Exception as e:
            print(f"❌ RSS fetch error: {e}")
            self.feed_items = []

    def create_content(self):
        """Create UI for RSS feed display with custom URL input."""
        self.add_section_header("Latest NSE Announcements")

        # Control panel: URL input and refresh
        with dpg.group(horizontal=True):
            dpg.add_input_text(label="RSS URL", tag="rss_url_input", width=600,
                               default_value=self.current_url)
            dpg.add_button(label="Load Feed", callback=self._on_refresh)
            dpg.add_spacer(width=20)
            dpg.add_text(f"Items: {len(self.feed_items)}", tag="rss_count_text")

        # Scrollable feed list
        with self.create_child_window(tag="rss_feed_window", width=-1, height=-1):
            self._populate_feed_window()

    def _populate_feed_window(self):
        """Helper to add feed items to the child window."""
        for item in self.feed_items:
            dpg.add_separator(parent="rss_feed_window")
            dpg.add_text(item['title'], wrap=600, parent="rss_feed_window")
            dpg.add_text(item['pubDate'], color=[150, 150, 150], parent="rss_feed_window")
            dpg.add_button(label="Open Link",
                           callback=lambda s, a, u=item['link']: webbrowser.open(u),
                           parent="rss_feed_window")
            dpg.add_text(item['description'], wrap=600, parent="rss_feed_window")

    def _on_refresh(self):
        """Callback to refresh feed with custom URL and update UI."""
        url = dpg.get_value("rss_url_input")
        self.current_url = url or self.DEFAULT_RSS_URL
        print(f"🔄 Loading RSS feed from: {self.current_url}")
        self.fetch_feed(self.current_url)

        # Clear existing entries
        dpg.delete_item("rss_feed_window", children_only=True)
        # Update count
        dpg.set_value("rss_count_text", f"Items: {len(self.feed_items)}")
        # Repopulate feed
        self._populate_feed_window()

    def cleanup(self):
        """Cleanup resources."""
        self.feed_items = None
        self.current_url = None
