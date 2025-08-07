import dearpygui.dearpygui as dpg
import requests
import xml.etree.ElementTree as ET
import webbrowser
import threading
from datetime import datetime
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import info, error, warning, monitor_performance, operation


class RssTab(BaseTab):
    """Tab for fetching and displaying NSE RSS announcements with performance optimizations."""

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
        self._is_loading = False
        self._cache = {}  # Cache for feed data

        info("RSS tab initialized", context={"default_url": self.DEFAULT_RSS_URL})

        # Initial feed fetch in background
        threading.Thread(target=self._initial_feed_load, daemon=True).start()

    def get_label(self):
        return "Announcements"

    def _initial_feed_load(self):
        """Load initial feed data in background"""
        try:
            self.fetch_feed(self.current_url)
        except Exception as e:
            error("Initial feed load failed", context={"error": str(e)}, exc_info=True)

    @monitor_performance
    def fetch_feed(self, url):
        """Fetch and parse RSS feed items from the given URL with caching."""
        try:
            with operation("fetch_rss_feed", context={"url": url}):
                # Check cache first (5 minute cache)
                cache_key = url
                if cache_key in self._cache:
                    cached_time, cached_items = self._cache[cache_key]
                    if datetime.now().timestamp() - cached_time < 300:  # 5 minutes
                        self.feed_items = cached_items
                        info("RSS feed loaded from cache", context={"items_count": len(cached_items)})
                        return

                response = requests.get(url, headers=self.HEADERS, timeout=15)
                response.raise_for_status()

                root = ET.fromstring(response.content)
                channel = root.find('channel') or root
                items = []

                for item in channel.findall('item'):
                    items.append({
                        'title': item.findtext('title', '').strip(),
                        'link': item.findtext('link', '').strip(),
                        'description': item.findtext('description', '').strip(),
                        'pubDate': item.findtext('pubDate', '').strip()
                    })

                # Cache the results
                self._cache[cache_key] = (datetime.now().timestamp(), items)
                self.feed_items = items

                info("RSS feed fetched successfully", context={
                    "url": url,
                    "items_count": len(items)
                })

        except requests.exceptions.Timeout:
            error("RSS feed request timeout", context={"url": url})
            self.feed_items = []
        except requests.exceptions.RequestException as e:
            error("RSS feed request failed", context={"url": url, "error": str(e)}, exc_info=True)
            self.feed_items = []
        except ET.ParseError as e:
            error("RSS feed XML parsing failed", context={"url": url, "error": str(e)}, exc_info=True)
            self.feed_items = []
        except Exception as e:
            error("RSS feed fetch error", context={"url": url, "error": str(e)}, exc_info=True)
            self.feed_items = []

    def create_content(self):
        """Create UI for RSS feed display with custom URL input."""
        try:
            self.add_section_header("Latest NSE Announcements")

            # Control panel: URL input and refresh
            with dpg.group(horizontal=True):
                dpg.add_input_text(label="RSS URL", tag="rss_url_input", width=600,
                                   default_value=self.current_url)
                dpg.add_button(label="Load Feed", callback=self._on_refresh)
                dpg.add_spacer(width=20)
                dpg.add_text(f"Items: {len(self.feed_items)}", tag="rss_count_text")
                dpg.add_spacer(width=10)
                dpg.add_text("Ready", tag="rss_status", color=[0, 255, 0])

            dpg.add_spacer(height=10)

            # Scrollable feed list
            with self.create_child_window(tag="rss_feed_window", width=-1, height=-1):
                self._populate_feed_window()

            info("RSS tab content created successfully")

        except Exception as e:
            error("Error creating RSS tab content", context={"error": str(e)}, exc_info=True)
            # Fallback UI
            dpg.add_text("Error loading RSS feed interface", color=[255, 0, 0])
            dpg.add_button(label="Retry", callback=self._on_refresh)

    def _populate_feed_window(self):
        """Helper to add feed items to the child window with error handling."""
        try:
            if not self.feed_items:
                dpg.add_text("No RSS items available", parent="rss_feed_window", color=[150, 150, 150])
                dpg.add_text("Click 'Load Feed' to fetch announcements", parent="rss_feed_window",
                             color=[150, 150, 150])
                return

            for i, item in enumerate(self.feed_items):
                try:
                    # Add separator between items (except for first item)
                    if i > 0:
                        dpg.add_separator(parent="rss_feed_window")

                    # Title with better formatting
                    title = item.get('title', 'No Title')[:200]  # Limit length
                    dpg.add_text(title, wrap=600, parent="rss_feed_window")

                    # Publication date with better formatting
                    pub_date = item.get('pubDate', '')
                    if pub_date:
                        dpg.add_text(f"Published: {pub_date}", color=[150, 150, 150], parent="rss_feed_window")

                    # Open link button with error handling
                    link = item.get('link', '')
                    if link:
                        dpg.add_button(
                            label="Open Link",
                            callback=lambda s, a, u=link: self._safe_open_link(u),
                            parent="rss_feed_window"
                        )

                    # Description with length limiting
                    description = item.get('description', '')[:500]  # Limit length
                    if description:
                        dpg.add_text(description, wrap=600, parent="rss_feed_window", color=[200, 200, 200])

                    dpg.add_spacer(height=10, parent="rss_feed_window")

                except Exception as e:
                    error(f"Error rendering RSS item {i}", context={"error": str(e)})
                    continue

            info("RSS feed window populated", context={"items_displayed": len(self.feed_items)})

        except Exception as e:
            error("Error populating RSS feed window", context={"error": str(e)}, exc_info=True)

    def _safe_open_link(self, url):
        """Safely open URL in browser with error handling."""
        try:
            if url and url.startswith(('http://', 'https://')):
                webbrowser.open(url)
                info("Opened RSS link in browser", context={"url": url[:100]})
            else:
                warning("Invalid URL for browser opening", context={"url": url})
        except Exception as e:
            error("Failed to open URL in browser", context={"url": url, "error": str(e)}, exc_info=True)

    def _on_refresh(self):
        """Callback to refresh feed with custom URL and update UI."""
        if self._is_loading:
            warning("RSS refresh already in progress")
            return

        try:
            url = dpg.get_value("rss_url_input")
            self.current_url = url.strip() if url else self.DEFAULT_RSS_URL

            info("RSS feed refresh requested", context={"url": self.current_url})

            self._is_loading = True
            self._update_status("Loading...", [255, 255, 0])

            # Run fetch in background thread
            threading.Thread(target=self._refresh_feed_async, daemon=True).start()

        except Exception as e:
            error("Error starting RSS refresh", context={"error": str(e)}, exc_info=True)
            self._is_loading = False
            self._update_status("Error", [255, 0, 0])

    def _refresh_feed_async(self):
        """Async refresh of RSS feed with UI updates."""
        try:
            # Fetch new data
            self.fetch_feed(self.current_url)

            # Schedule UI update on main thread
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                self._update_ui_after_refresh
            )

        except Exception as e:
            error("Error in async RSS refresh", context={"error": str(e)}, exc_info=True)
            dpg.set_frame_callback(
                dpg.get_frame_count() + 1,
                lambda: self._update_status("Error", [255, 0, 0])
            )
        finally:
            self._is_loading = False

    def _update_ui_after_refresh(self):
        """Update UI elements after successful feed refresh."""
        try:
            # Clear existing entries
            if dpg.does_item_exist("rss_feed_window"):
                dpg.delete_item("rss_feed_window", children_only=True)

            # Update count
            if dpg.does_item_exist("rss_count_text"):
                dpg.set_value("rss_count_text", f"Items: {len(self.feed_items)}")

            # Update status
            self._update_status("Ready", [0, 255, 0])

            # Repopulate feed
            self._populate_feed_window()

            info("RSS UI updated after refresh", context={"items_count": len(self.feed_items)})

        except Exception as e:
            error("Error updating RSS UI after refresh", context={"error": str(e)}, exc_info=True)
            self._update_status("UI Error", [255, 0, 0])

    def _update_status(self, message, color=None):
        """Update status text safely."""
        try:
            if dpg.does_item_exist("rss_status"):
                dpg.set_value("rss_status", message)
                if color:
                    dpg.configure_item("rss_status", color=color)
        except Exception as e:
            error("Error updating RSS status", context={"message": message, "error": str(e)})

    def cleanup(self):
        """Cleanup resources with logging."""
        try:
            info("Starting RSS tab cleanup")
            self.feed_items = []
            self.current_url = None
            self._cache.clear()
            self._is_loading = False
            info("RSS tab cleanup completed")
        except Exception as e:
            error("Error during RSS tab cleanup", context={"error": str(e)}, exc_info=True)