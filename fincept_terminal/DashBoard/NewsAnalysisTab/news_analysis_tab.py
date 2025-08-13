"""
Real-time News Analysis Tab for Fincept Terminal
Clean, efficient implementation with proper article display
"""

import dearpygui.dearpygui as dpg
import requests
import xml.etree.ElementTree as ET
import threading
import time
import re
from urllib.parse import urlparse
import duckdb
from pathlib import Path
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import logger

# Try to import required libraries
try:
    import newspaper
    NEWSPAPER_AVAILABLE = True
except ImportError:
    NEWSPAPER_AVAILABLE = False
    logger.warning("Newspaper4k not available")

try:
    from playwright.sync_api import sync_playwright
    PLAYWRIGHT_AVAILABLE = True
except ImportError:
    PLAYWRIGHT_AVAILABLE = False
    logger.warning("Playwright not available")

class NewsAnalysisTab(BaseTab):
    """Real-time News Analysis Dashboard with RSS Feed Integration"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app

        # Bloomberg Terminal Colors
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 149, 237]

        self.news_sources = {}
        self.refresh_threads = {}
        self.conn = None
        self.ui_initialized = False

        self.headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Connection': 'keep-alive',
        }

        self.setup_database()
        threading.Thread(target=self.load_user_settings, daemon=True).start()

    def get_label(self):
        return "News"

    def _get_config_directory(self) -> Path:
        config_dir = Path.home() / '.fincept' / 'news'
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir

    def setup_database(self):
        try:
            config_dir = self._get_config_directory()
            db_path = config_dir / "news_settings.db"
            self.conn = duckdb.connect(str(db_path))
            self.conn.execute("""
                CREATE TABLE IF NOT EXISTS news_sources (
                    id INTEGER PRIMARY KEY,
                    website_url VARCHAR,
                    refresh_interval INTEGER,
                    source_name VARCHAR,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            """)
        except Exception as e:
            logger.error(f"Database setup failed: {e}")
            self.conn = duckdb.connect(':memory:')

    def resolve_url(self, url):
        """Resolve Google News URLs using Playwright"""
        if not PLAYWRIGHT_AVAILABLE or 'news.google.com' not in url:
            return url

        try:
            with sync_playwright() as p:
                browser = p.chromium.launch(headless=True)
                page = browser.new_page()
                page.goto(url, wait_until='networkidle', timeout=15000)
                final_url = page.url
                browser.close()
                return final_url if 'news.google.com' not in final_url else url
        except Exception:
            return url

    def validate_news_website(self, url):
        if not url.startswith(('http://', 'https://')):
            url = 'https://' + url

        domain = urlparse(url).netloc.lower().replace('www.', '')
        if not domain:
            return False, "Invalid URL format"

        # Try common RSS endpoints
        rss_endpoints = [
            f"https://{domain}/rss",
            f"https://{domain}/feed",
            f"https://{domain}/rss.xml",
            f"https://{domain}/feed.xml"
        ]

        for rss_url in rss_endpoints:
            try:
                response = requests.get(rss_url, headers=self.headers, timeout=8)
                if response.status_code == 200:
                    try:
                        root = ET.fromstring(response.content)
                        if root.tag in ['rss', 'feed'] or 'rss' in root.tag.lower():
                            return True, "Direct RSS feed found"
                    except ET.ParseError:
                        continue
            except requests.RequestException:
                continue

        # Check for RSS indicators on main page
        try:
            response = requests.get(f"https://{domain}", headers=self.headers, timeout=10)
            if response.status_code == 200:
                content = response.text.lower()
                if any(indicator in content for indicator in ['application/rss+xml', '/rss', '/feed']):
                    return True, "RSS feed detected"
        except requests.RequestException:
            pass

        # Fallback to Google News RSS
        try:
            google_rss_url = f"https://news.google.com/rss/search?q=site%3A{domain}&hl=en-US&gl=US&ceid=US%3Aen"
            response = requests.get(google_rss_url, headers=self.headers, timeout=10)
            if response.status_code == 200:
                root = ET.fromstring(response.content)
                if len(root.findall('.//item')) > 0:
                    return True, "Google News RSS available"
        except Exception:
            pass

        return False, f"No RSS feed found for {domain}"

    def generate_rss_url(self, website_url):
        if not website_url.startswith(('http://', 'https://')):
            website_url = 'https://' + website_url

        domain = urlparse(website_url).netloc.replace('www.', '')

        # Try direct RSS feeds first
        for path in ['/rss', '/feed', '/rss.xml', '/feed.xml']:
            rss_url = f"https://{domain}{path}"
            try:
                response = requests.get(rss_url, headers=self.headers, timeout=8)
                if response.status_code == 200:
                    root = ET.fromstring(response.content)
                    if len(root.findall('.//item')) > 0:
                        return rss_url
            except Exception:
                continue

        # Fallback to Google News RSS
        return f"https://news.google.com/rss/search?q=site%3A{domain}&hl=en-US&gl=US&ceid=US%3Aen"

    def fetch_rss_feed(self, rss_url, source_id=None):
        try:
            response = requests.get(rss_url, headers=self.headers, timeout=15)
            response.raise_for_status()
            root = ET.fromstring(response.content)
            articles = []

            items = root.findall('.//item') or root.findall('.//{http://www.w3.org/2005/Atom}entry')

            for item in items[:10]:
                title = item.find('title')
                link = item.find('link')
                pub_date = item.find('pubDate')
                description = item.find('description')

                # Atom format fallback
                if title is None:
                    title = item.find('.//{http://www.w3.org/2005/Atom}title')
                if link is None:
                    link_elem = item.find('.//{http://www.w3.org/2005/Atom}link')
                    if link_elem is not None:
                        link = type('obj', (object,), {'text': link_elem.get('href')})
                if pub_date is None:
                    pub_date = item.find('.//{http://www.w3.org/2005/Atom}published') or item.find('.//{http://www.w3.org/2005/Atom}updated')
                if description is None:
                    description = item.find('.//{http://www.w3.org/2005/Atom}summary')

                article_url = link.text if link is not None and hasattr(link, 'text') and link.text else ''

                articles.append({
                    'title': title.text if title is not None and title.text else 'No title',
                    'link': article_url,
                    'pub_date': pub_date.text if pub_date is not None and pub_date.text else '',
                    'description': re.sub('<[^<]+?>', '', description.text) if description is not None and description.text else ''
                })

            return articles
        except Exception as e:
            logger.error(f"RSS fetch error: {e}")
            return []

    def extract_article_content(self, article_url):
        """Extract article content using newspaper4k with debugging"""
        final_url = self.resolve_url(article_url)

        # First try newspaper4k
        if NEWSPAPER_AVAILABLE:
            try:
                article = newspaper.article(final_url)

                if article and hasattr(article, 'text'):
                    article_data = {
                        'title': getattr(article, 'title', 'No title'),
                        'text': getattr(article, 'text', ''),
                        'authors': getattr(article, 'authors', []),
                        'publish_date': getattr(article, 'publish_date', None),
                        'summary': '',
                        'top_image': getattr(article, 'top_image', ''),
                        'final_url': final_url
                    }

                    # Try NLP processing
                    try:
                        article.nlp()
                        if hasattr(article, 'summary') and article.summary:
                            article_data['summary'] = article.summary
                        if hasattr(article, 'keywords') and article.keywords:
                            article_data['keywords'] = getattr(article, 'keywords', [])
                    except Exception:
                        pass

                    # Check if we got meaningful content
                    if len(article_data['text'].strip()) > 100:  # Require at least 100 characters
                        return article_data, None
            except Exception:
                pass

        # Fallback to requests-based extraction with better content detection
        try:
            response = requests.get(final_url, headers=self.headers, timeout=20)

            if response.status_code != 200:
                return None, f"HTTP {response.status_code} error"

            html_content = response.text

            # More aggressive content extraction
            # Remove scripts, styles, and navigation
            clean_html = re.sub(r'<script[^>]*>.*?</script>', '', html_content, flags=re.DOTALL | re.IGNORECASE)
            clean_html = re.sub(r'<style[^>]*>.*?</style>', '', clean_html, flags=re.DOTALL | re.IGNORECASE)
            clean_html = re.sub(r'<nav[^>]*>.*?</nav>', '', clean_html, flags=re.DOTALL | re.IGNORECASE)
            clean_html = re.sub(r'<header[^>]*>.*?</header>', '', clean_html, flags=re.DOTALL | re.IGNORECASE)
            clean_html = re.sub(r'<footer[^>]*>.*?</footer>', '', clean_html, flags=re.DOTALL | re.IGNORECASE)

            # Extract title
            title_match = re.search(r'<title[^>]*>(.*?)</title>', clean_html, re.IGNORECASE | re.DOTALL)
            title = title_match.group(1) if title_match else 'Article Title'
            title = re.sub(r'<[^>]+>', '', title).strip()

            # Try multiple patterns for article content
            content_patterns = [
                # Look for article tags
                r'<article[^>]*>(.*?)</article>',
                # Look for content divs
                r'<div[^>]*class="[^"]*(?:content|article|story|post-content|entry-content)[^"]*"[^>]*>(.*?)</div>',
                # Look for main content areas
                r'<main[^>]*>(.*?)</main>',
                # Look for specific content IDs
                r'<div[^>]*id="[^"]*(?:content|article|story|main)[^"]*"[^>]*>(.*?)</div>',
                # Look for paragraph containers
                r'<div[^>]*class="[^"]*(?:text|paragraph|body)[^"]*"[^>]*>(.*?)</div>',
            ]

            article_content = ""
            for pattern in content_patterns:
                matches = re.findall(pattern, clean_html, re.DOTALL | re.IGNORECASE)
                if matches:
                    # Take the longest match
                    potential_content = max(matches, key=len)
                    # Clean HTML tags
                    cleaned = re.sub(r'<[^>]+>', '', potential_content)
                    cleaned = re.sub(r'\s+', ' ', cleaned).strip()

                    # Check if this looks like real article content
                    if len(cleaned) > 200 and len(cleaned.split()) > 30:
                        article_content = cleaned
                        break

            # If no good content found, try extracting all paragraph text
            if not article_content or len(article_content) < 200:
                # Extract all paragraphs
                paragraphs = re.findall(r'<p[^>]*>(.*?)</p>', clean_html, re.DOTALL | re.IGNORECASE)
                if paragraphs:
                    # Clean and combine paragraphs
                    cleaned_paragraphs = []
                    for p in paragraphs:
                        cleaned = re.sub(r'<[^>]+>', '', p)
                        cleaned = re.sub(r'\s+', ' ', cleaned).strip()
                        if len(cleaned) > 20:  # Only keep substantial paragraphs
                            cleaned_paragraphs.append(cleaned)

                    if cleaned_paragraphs:
                        article_content = '\n\n'.join(cleaned_paragraphs)

            # Final fallback: extract all text and try to find article portion
            if not article_content or len(article_content) < 200:
                all_text = re.sub(r'<[^>]+>', '', clean_html)
                all_text = re.sub(r'\s+', ' ', all_text).strip()

                # Try to find the main content by looking for the title in the text
                title_words = title.split()[:3]  # First 3 words of title
                if title_words:
                    title_pattern = '.*?'.join(re.escape(word) for word in title_words)
                    match = re.search(title_pattern, all_text, re.IGNORECASE)
                    if match:
                        # Take text starting from title match
                        start_pos = match.start()
                        article_content = all_text[start_pos:start_pos + 5000]  # Limit to 5000 chars
                    else:
                        # Just take a middle portion
                        text_parts = all_text.split()
                        if len(text_parts) > 100:
                            start_idx = len(text_parts) // 4
                            end_idx = 3 * len(text_parts) // 4
                            article_content = ' '.join(text_parts[start_idx:end_idx])
                        else:
                            article_content = all_text

            # Limit content length
            if len(article_content) > 8000:
                article_content = article_content[:8000] + "..."

            return {
                'title': title,
                'text': article_content,
                'authors': [],
                'publish_date': None,
                'summary': '',
                'final_url': final_url
            }, None

        except Exception as e:
            return None, f"Content extraction failed: {str(e)}"

    def extract_with_requests(self, article_url):
        """Fallback extraction using requests"""
        try:
            final_url = self.resolve_url(article_url)
            response = requests.get(final_url, headers=self.headers, timeout=20)

            if response.status_code == 403:
                return None, "Website blocked access (403 Forbidden)"
            elif response.status_code == 404:
                return None, "Article not found (404)"
            elif response.status_code != 200:
                return None, f"Website returned error {response.status_code}"

            html_content = response.text

            # Simple text extraction
            clean_html = re.sub(r'<script[^>]*>.*?</script>', '', html_content, flags=re.DOTALL | re.IGNORECASE)
            clean_html = re.sub(r'<style[^>]*>.*?</style>', '', clean_html, flags=re.DOTALL | re.IGNORECASE)

            # Extract title
            title_match = re.search(r'<title[^>]*>(.*?)</title>', clean_html, re.IGNORECASE | re.DOTALL)
            title = title_match.group(1) if title_match else 'Article Title'
            title = re.sub(r'<[^>]+>', '', title).strip()

            # Extract article content
            clean_text = re.sub(r'<[^>]+>', '', clean_html)
            clean_text = re.sub(r'\s+', ' ', clean_text).strip()

            # Try to find article content
            for pattern in [
                r'<article[^>]*>(.*?)</article>',
                r'<div[^>]*class="[^"]*article[^"]*"[^>]*>(.*?)</div>',
                r'<div[^>]*class="[^"]*content[^"]*"[^>]*>(.*?)</div>',
                r'<main[^>]*>(.*?)</main>'
            ]:
                matches = re.findall(pattern, html_content, re.DOTALL | re.IGNORECASE)
                if matches:
                    article_content = max(matches, key=len)
                    article_content = re.sub(r'<[^>]+>', '', article_content)
                    article_content = re.sub(r'\s+', ' ', article_content).strip()
                    if len(article_content) > 100:
                        clean_text = article_content
                        break

            # Limit content
            if len(clean_text) > 100:
                text_parts = clean_text.split()
                start_idx = len(text_parts) // 4
                end_idx = 3 * len(text_parts) // 4
                clean_text = ' '.join(text_parts[start_idx:end_idx])

            if len(clean_text) > 5000:
                clean_text = clean_text[:5000] + "..."

            return {
                'title': title,
                'text': clean_text,
                'authors': [],
                'publish_date': None,
                'summary': '',
                'final_url': final_url
            }, None

        except Exception as e:
            return None, f"Content extraction failed: {str(e)}"

    def update_status_message(self, message, color=None):
        try:
            status_tag = f"news_status_{id(self)}"
            if dpg.does_item_exist(status_tag):
                dpg.set_value(status_tag, message)
                if color:
                    dpg.configure_item(status_tag, color=color)
        except Exception:
            pass

    def add_news_source(self):
        website_url = dpg.get_value(f"news_website_input_{id(self)}")
        refresh_interval = dpg.get_value(f"news_refresh_input_{id(self)}")

        if not website_url or refresh_interval < 1:
            self.update_status_message("Please enter valid website URL and refresh interval", self.BLOOMBERG_RED)
            return

        self.update_status_message(f"Validating {website_url}...", self.BLOOMBERG_YELLOW)

        def validation_worker():
            try:
                is_valid, message = self.validate_news_website(website_url)
                if not is_valid:
                    self.update_status_message(f"Error: {message}", self.BLOOMBERG_RED)
                    return

                rss_url = self.generate_rss_url(website_url)
                if not rss_url:
                    self.update_status_message("Could not generate RSS URL", self.BLOOMBERG_RED)
                    return

                self.update_status_message("Testing RSS feed...", self.BLOOMBERG_YELLOW)
                test_articles = self.fetch_rss_feed(rss_url)

                if not test_articles:
                    self.update_status_message("No articles found", self.BLOOMBERG_RED)
                    return

                source_name = urlparse(website_url if website_url.startswith(('http://', 'https://')) else 'https://' + website_url).netloc.replace('www.', '')

                max_id_result = self.conn.execute("SELECT COALESCE(MAX(id), 0) FROM news_sources").fetchone()
                source_id = max_id_result[0] + 1

                self.conn.execute("INSERT INTO news_sources (id, website_url, refresh_interval, source_name) VALUES (?, ?, ?, ?)",
                                (source_id, website_url, refresh_interval, source_name))

                self.news_sources[source_id] = {
                    'url': website_url,
                    'rss_url': rss_url,
                    'timer': refresh_interval,
                    'source_name': source_name,
                    'articles': test_articles,
                    'last_update': time.time(),
                    'status': 'Active'
                }

                self.start_refresh_timer(source_id)
                self.refresh_news_display()

                dpg.set_value(f"news_website_input_{id(self)}", "")
                dpg.set_value(f"news_refresh_input_{id(self)}", 5)
                self.update_status_message(f"Added: {source_name} - {len(test_articles)} articles", self.BLOOMBERG_GREEN)

            except Exception as e:
                self.update_status_message(f"Error: {str(e)}", self.BLOOMBERG_RED)

        threading.Thread(target=validation_worker, daemon=True).start()

    def start_refresh_timer(self, source_id):
        def refresh_worker():
            try:
                while source_id in self.news_sources:
                    time.sleep(self.news_sources[source_id]['timer'] * 60)

                    if source_id not in self.news_sources:
                        break

                    self.news_sources[source_id]['status'] = 'Updating...'
                    articles = self.fetch_rss_feed(self.news_sources[source_id]['rss_url'], source_id)

                    if source_id in self.news_sources:
                        if articles:
                            self.news_sources[source_id]['articles'] = articles
                            self.news_sources[source_id]['last_update'] = time.time()
                            self.news_sources[source_id]['status'] = 'Active'
                        else:
                            self.news_sources[source_id]['status'] = 'Error'

                        self.refresh_news_display()

            except Exception:
                if source_id in self.news_sources:
                    self.news_sources[source_id]['status'] = 'Error'

        refresh_thread = threading.Thread(target=refresh_worker, daemon=True)
        refresh_thread.start()
        self.refresh_threads[source_id] = refresh_thread

    def refresh_single_source(self, source_id):
        try:
            if source_id not in self.news_sources:
                return

            self.update_status_message(f"Refreshing {self.news_sources[source_id]['source_name']}...", self.BLOOMBERG_YELLOW)

            def refresh_worker():
                try:
                    self.news_sources[source_id]['status'] = 'Updating...'
                    self.refresh_news_display()

                    articles = self.fetch_rss_feed(self.news_sources[source_id]['rss_url'], source_id)

                    if source_id in self.news_sources:
                        if articles:
                            self.news_sources[source_id]['articles'] = articles
                            self.news_sources[source_id]['last_update'] = time.time()
                            self.news_sources[source_id]['status'] = 'Active'
                            self.update_status_message(f"Refreshed {self.news_sources[source_id]['source_name']} - {len(articles)} articles", self.BLOOMBERG_GREEN)
                        else:
                            self.news_sources[source_id]['status'] = 'Error'
                            self.update_status_message(f"Failed to refresh {self.news_sources[source_id]['source_name']}", self.BLOOMBERG_RED)

                        self.refresh_news_display()

                except Exception:
                    if source_id in self.news_sources:
                        self.news_sources[source_id]['status'] = 'Error'
                        self.update_status_message(f"Error refreshing {self.news_sources[source_id]['source_name']}", self.BLOOMBERG_RED)
                        self.refresh_news_display()

            threading.Thread(target=refresh_worker, daemon=True).start()

        except Exception:
            self.update_status_message("Refresh failed", self.BLOOMBERG_RED)

    def delete_news_source(self, source_id):
        try:
            if self.conn:
                self.conn.execute("DELETE FROM news_sources WHERE id = ?", (source_id,))

            if source_id in self.news_sources:
                source_name = self.news_sources[source_id]['source_name']
                del self.news_sources[source_id]

            if source_id in self.refresh_threads:
                del self.refresh_threads[source_id]

            self.refresh_news_display()
            self.update_status_message(f"Deleted {source_name}", self.BLOOMBERG_GREEN)

        except Exception:
            self.update_status_message("Error deleting source", self.BLOOMBERG_RED)

    def load_user_settings(self):
        try:
            if not self.conn:
                return

            sources = self.conn.execute("SELECT * FROM news_sources ORDER BY id").fetchall()

            for source in sources:
                source_id, website_url, refresh_interval, source_name, *_ = source
                rss_url = self.generate_rss_url(website_url)

                self.news_sources[source_id] = {
                    'url': website_url,
                    'rss_url': rss_url,
                    'timer': refresh_interval,
                    'source_name': source_name,
                    'articles': [],
                    'last_update': 0,
                    'status': 'Loading...'
                }

                def load_source(sid, rss):
                    try:
                        articles = self.fetch_rss_feed(rss, sid)
                        if sid in self.news_sources:
                            if articles:
                                self.news_sources[sid]['articles'] = articles
                                self.news_sources[sid]['last_update'] = time.time()
                                self.news_sources[sid]['status'] = 'Active'
                            else:
                                self.news_sources[sid]['status'] = 'Error'
                            self.refresh_news_display()
                    except Exception:
                        if sid in self.news_sources:
                            self.news_sources[sid]['status'] = 'Error'

                threading.Thread(target=load_source, args=(source_id, rss_url), daemon=True).start()
                self.start_refresh_timer(source_id)

        except Exception:
            pass

    def wrap_text(self, text, width=80):
        """Wrap text to specified width"""
        words = text.split()
        lines = []
        current_line = ""

        for word in words:
            if len(current_line + " " + word) <= width:
                current_line += (" " + word) if current_line else word
            else:
                if current_line:
                    lines.append(current_line)
                current_line = word

        if current_line:
            lines.append(current_line)

        return "\n".join(lines)

    def open_full_article(self, article_url, article_title):
        """Open full article with optimized 600x600 window and robust error handling"""
        def fetch_article_worker():
            window_id = f"article_window_{hash(article_url)}"
            content_tag = f"article_content_{hash(article_url)}"

            try:
                if dpg.does_item_exist(content_tag):
                    dpg.set_value(content_tag, "🔄 Starting extraction...\n\nInitializing article loader...")

                # Add timeout and better error tracking
                import signal

                def timeout_handler(signum, frame):
                    raise TimeoutError("Article extraction timed out after 30 seconds")

                # Set 30 second timeout
                if hasattr(signal, 'SIGALRM'):  # Unix systems
                    signal.signal(signal.SIGALRM, timeout_handler)
                    signal.alarm(30)

                try:
                    if dpg.does_item_exist(content_tag):
                        dpg.set_value(content_tag, "🔄 Resolving URL...\n\nChecking if Google News redirect...")

                    # Test URL resolution first
                    final_url = article_url
                    if 'news.google.com' in article_url:
                        if dpg.does_item_exist(content_tag):
                            dpg.set_value(content_tag, "🔄 Resolving Google News URL...\n\nUsing Playwright to get actual article URL...")

                        final_url = self.resolve_url(article_url)

                        if dpg.does_item_exist(content_tag):
                            dpg.set_value(content_tag, f"🔄 URL resolved to:\n{final_url}\n\nExtracting content...")

                    # Try extraction with progress updates
                    if dpg.does_item_exist(content_tag):
                        dpg.set_value(content_tag, f"🔄 Extracting content...\n\nUsing {'Newspaper4k' if NEWSPAPER_AVAILABLE else 'Fallback'} method...")

                    article_data, error = self.extract_article_content(article_url)

                    # Cancel timeout
                    if hasattr(signal, 'SIGALRM'):
                        signal.alarm(0)

                    if error or not article_data:
                        error_msg = f"❌ EXTRACTION FAILED\n\n"
                        error_msg += f"Title: {article_title}\n"
                        error_msg += f"Original URL: {article_url}\n"
                        if final_url != article_url:
                            error_msg += f"Resolved URL: {final_url}\n"
                        error_msg += f"Method: {'Newspaper4k' if NEWSPAPER_AVAILABLE else 'Fallback'}\n"
                        error_msg += f"Playwright: {'Available' if PLAYWRIGHT_AVAILABLE else 'Not Available'}\n\n"
                        if error:
                            error_msg += f"Error: {error}\n\n"
                        error_msg += "💡 Try 'Browser' button to read the full article."

                        if dpg.does_item_exist(content_tag):
                            dpg.set_value(content_tag, error_msg)
                        return

                    # Success - format content
                    if dpg.does_item_exist(content_tag):
                        dpg.set_value(content_tag, "🔄 Formatting content...\n\nPreparing article display...")

                    # Format content for 600x600 window
                    content = ""

                    # Title
                    title = article_data.get('title', 'No Title')
                    content += f"📰 {title}\n"
                    content += "=" * min(len(title) + 4, 70) + "\n\n"

                    # Metadata section
                    content += "📋 ARTICLE INFO:\n"
                    content += "-" * 40 + "\n"

                    # Publish Date
                    if article_data.get('publish_date'):
                        pub_date = str(article_data['publish_date'])
                        content += f"📅 Published: {pub_date}\n"
                    else:
                        content += f"📅 Published: Not available\n"

                    # Authors
                    if article_data.get('authors'):
                        authors_list = article_data['authors']
                        if len(authors_list) > 3:
                            authors_str = ', '.join(authors_list[:3]) + f" + {len(authors_list)-3} more"
                        else:
                            authors_str = ', '.join(authors_list)
                        content += f"✍️  Authors: {authors_str}\n"
                    else:
                        content += f"✍️  Authors: Not available\n"

                    # Source URLs
                    final_url = article_data.get('final_url', article_url)
                    content += f"🔗 Source: {final_url}\n"
                    if final_url != article_url:
                        content += f"🌐 Original: {article_url}\n"

                    content += "\n"

                    # Keywords
                    if article_data.get('keywords'):
                        keywords = article_data['keywords'][:8]  # Limit to 8 keywords
                        content += f"🏷️  Keywords: {', '.join(keywords)}\n\n"

                    # Summary
                    if article_data.get('summary'):
                        content += "📝 SUMMARY:\n"
                        content += "-" * 40 + "\n"
                        summary_wrapped = self.wrap_text(article_data['summary'], 65)
                        content += summary_wrapped + "\n\n"

                    # Full Text
                    content += "📖 FULL ARTICLE:\n"
                    content += "=" * 40 + "\n\n"

                    if article_data.get('text') and len(article_data['text'].strip()) > 50:
                        # Clean text
                        clean_text = re.sub(r'\n\s*\n', '\n\n', article_data['text'].strip())
                        clean_text = re.sub(r'\n{3,}', '\n\n', clean_text)
                        clean_text = re.sub(r' {2,}', ' ', clean_text)

                        # Split into paragraphs and wrap each
                        paragraphs = clean_text.split('\n\n')
                        wrapped_paragraphs = []

                        for paragraph in paragraphs:
                            if paragraph.strip():
                                wrapped_paragraphs.append(self.wrap_text(paragraph.strip(), 65))

                        content += '\n\n'.join(wrapped_paragraphs)

                        # Article Statistics
                        word_count = len(clean_text.split())
                        char_count = len(clean_text)
                        reading_time = max(1, word_count // 200)

                        content += f"\n\n" + "=" * 40
                        content += f"\n📊 STATS: {word_count:,} words • {char_count:,} chars • ~{reading_time} min read"

                    else:
                        content += "⚠️  Article content could not be extracted.\n\n"
                        content += "Common reasons:\n"
                        content += "• JavaScript-heavy content\n"
                        content += "• Paywall protection\n"
                        content += "• Anti-scraping measures\n\n"
                        content += "💡 Use 'Browser' button to read directly."

                    if dpg.does_item_exist(content_tag):
                        dpg.set_value(content_tag, content)

                except TimeoutError:
                    if hasattr(signal, 'SIGALRM'):
                        signal.alarm(0)
                    error_msg = f"⏰ TIMEOUT ERROR\n\n"
                    error_msg += f"Article extraction timed out after 30 seconds.\n\n"
                    error_msg += f"Title: {article_title}\n"
                    error_msg += f"URL: {article_url}\n\n"
                    error_msg += "This usually happens when:\n"
                    error_msg += "• Website is very slow to respond\n"
                    error_msg += "• Complex JavaScript processing\n"
                    error_msg += "• Network connectivity issues\n\n"
                    error_msg += "💡 Try 'Browser' button or reload."

                    if dpg.does_item_exist(content_tag):
                        dpg.set_value(content_tag, error_msg)

            except Exception as e:
                if hasattr(signal, 'SIGALRM'):
                    signal.alarm(0)

                error_msg = f"❌ UNEXPECTED ERROR\n\n"
                error_msg += f"Title: {article_title}\n"
                error_msg += f"URL: {article_url}\n"
                error_msg += f"Error: {str(e)}\n"
                error_msg += f"Error Type: {type(e).__name__}\n\n"
                error_msg += "💡 Try 'Browser' button to read the article."

                if dpg.does_item_exist(content_tag):
                    dpg.set_value(content_tag, error_msg)

        # Create optimized 600x600 article window
        window_id = f"article_window_{hash(article_url)}"
        content_tag = f"article_content_{hash(article_url)}"

        if dpg.does_item_exist(window_id):
            dpg.delete_item(window_id)

        try:
            # Truncate title for window header
            display_title = article_title[:45] + "..." if len(article_title) > 45 else article_title

            with dpg.window(label=f"📰 {display_title}",
                           tag=window_id, width=600, height=600,
                           pos=[100, 100], modal=False):

                # Compact header with essential buttons
                with dpg.group(horizontal=True):
                    dpg.add_button(label="❌",
                                 callback=lambda: dpg.delete_item(window_id),
                                 width=30, height=30)

                    dpg.add_button(label="🌐 Browser",
                                 callback=lambda: self.open_in_browser(article_url),
                                 width=80, height=30)

                    dpg.add_button(label="🔄",
                                 callback=lambda: threading.Thread(target=fetch_article_worker, daemon=True).start(),
                                 width=30, height=30)

                    dpg.add_spacer(width=10)
                    dpg.add_text("💡 Full article reader", color=self.BLOOMBERG_YELLOW)

                dpg.add_separator()

                # Main content area optimized for 600x600
                dpg.add_input_text(tag=content_tag,
                                 default_value="🔄 Initializing...\n\nStarting article extraction process...",
                                 multiline=True,
                                 width=580,  # Leave margin for scrollbar
                                 height=520, # Leave space for header and buttons
                                 readonly=True)  # Removed wrap=True parameter

            # Start extraction immediately
            threading.Thread(target=fetch_article_worker, daemon=True).start()

        except Exception as e:
            # Fallback simple window if creation fails
            try:
                with dpg.window(label="Article Error", width=400, height=200, pos=[200, 200]):
                    dpg.add_text(f"Failed to create article window: {str(e)}")
                    dpg.add_button(label="Open in Browser",
                                 callback=lambda: self.open_in_browser(article_url))
            except Exception:
                pass

    def open_in_browser(self, url):
        try:
            import webbrowser
            webbrowser.open(url)
        except Exception:
            pass

    def refresh_news_display(self):
        try:
            container_tag = f"news_container_{id(self)}"
            parent_tag = f"news_main_window_{id(self)}"

            if not dpg.does_item_exist(parent_tag):
                return

            if dpg.does_item_exist(container_tag):
                dpg.delete_item(container_tag)

            with dpg.group(tag=container_tag, parent=parent_tag):
                if not self.news_sources:
                    with dpg.group():
                        dpg.add_text("📰 No news sources configured", color=self.BLOOMBERG_GRAY)
                        dpg.add_text("Add websites above to start receiving live news feeds", color=self.BLOOMBERG_YELLOW)
                        dpg.add_spacer(height=10)
                        dpg.add_text("💡 Supported: Most major news websites with RSS feeds", color=self.BLOOMBERG_WHITE)
                else:
                    self.create_news_grid()

        except Exception:
            pass

    def create_news_grid(self):
        try:
            sources = list(self.news_sources.items())

            for i in range(0, len(sources), 2):
                with dpg.group(horizontal=True):
                    if i < len(sources):
                        source_id, source_data = sources[i]
                        self.create_news_panel(source_id, source_data, 750, 400)

                    if i + 1 < len(sources):
                        source_id, source_data = sources[i + 1]
                        self.create_news_panel(source_id, source_data, 750, 400)

                dpg.add_spacer(height=10)

        except Exception:
            pass

    def create_news_panel(self, source_id, source_data, width, height):
        try:
            with dpg.child_window(width=width, height=height, border=True):
                # Panel header
                with dpg.group(horizontal=True):
                    dpg.add_text(f"📰 {source_data['source_name'].upper()}", color=self.BLOOMBERG_ORANGE)
                    dpg.add_text(" • ", color=self.BLOOMBERG_GRAY)

                    # Status indicator
                    status = source_data.get('status', 'Unknown')
                    status_color = self.BLOOMBERG_GREEN if status == 'Active' else \
                                 self.BLOOMBERG_YELLOW if status in ['Loading...', 'Updating...'] else \
                                 self.BLOOMBERG_RED
                    dpg.add_text(f"{status}", color=status_color)

                    dpg.add_text(" • ", color=self.BLOOMBERG_GRAY)
                    dpg.add_text(f"⏱️ {source_data['timer']}min", color=self.BLOOMBERG_GRAY)

                    if source_data['last_update']:
                        last_update = time.strftime('%H:%M:%S', time.localtime(source_data['last_update']))
                        dpg.add_text(f" • 🔄 {last_update}", color=self.BLOOMBERG_GREEN)

                    # Action buttons
                    dpg.add_spacer(width=10)

                    def create_refresh_callback(source_id_to_refresh):
                        def callback(sender, app_data, user_data):
                            self.refresh_single_source(source_id_to_refresh)
                        return callback

                    dpg.add_button(label="🔄",
                                 callback=create_refresh_callback(source_id),
                                 width=30, height=25)

                    def create_delete_callback(source_id_to_delete):
                        def callback(sender, app_data, user_data):
                            self.delete_news_source(source_id_to_delete)
                        return callback

                    dpg.add_button(label="🗑️",
                                 callback=create_delete_callback(source_id),
                                 width=30, height=25)

                dpg.add_separator()

                # Articles table
                articles = source_data.get('articles', [])

                with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                             scrollY=True, scrollX=True, height=height - 80):
                    dpg.add_table_column(label="📄 Title", width_fixed=True, init_width_or_weight=500)
                    dpg.add_table_column(label="📅 Published", width_fixed=True, init_width_or_weight=120)
                    dpg.add_table_column(label="⚡ Action", width_fixed=True, init_width_or_weight=80)

                    if not articles:
                        with dpg.table_row():
                            dpg.add_text("Loading articles...", color=self.BLOOMBERG_YELLOW)
                            dpg.add_text("", color=self.BLOOMBERG_GRAY)
                            dpg.add_text("", color=self.BLOOMBERG_GRAY)
                    else:
                        for article in articles:
                            with dpg.table_row():
                                # Title with wrapping
                                title = article['title']

                                with dpg.group():
                                    if len(title) > 60:
                                        words = title.split()
                                        current_line = ""
                                        lines = []

                                        for word in words:
                                            if len(current_line + " " + word) <= 60:
                                                current_line += (" " + word) if current_line else word
                                            else:
                                                if current_line:
                                                    lines.append(current_line)
                                                current_line = word

                                        if current_line:
                                            lines.append(current_line)

                                        for i, line in enumerate(lines[:3]):
                                            if i == 2 and len(lines) > 3:
                                                dpg.add_text(line + "...", color=self.BLOOMBERG_WHITE)
                                            else:
                                                dpg.add_text(line, color=self.BLOOMBERG_WHITE)
                                    else:
                                        dpg.add_text(title, color=self.BLOOMBERG_WHITE)

                                # Published date
                                pub_date = article.get('pub_date', '')
                                if pub_date:
                                    date_str = pub_date[:16] if len(pub_date) > 16 else pub_date
                                    dpg.add_text(date_str, color=self.BLOOMBERG_GRAY)
                                else:
                                    dpg.add_text("Unknown", color=self.BLOOMBERG_GRAY)

                                # View button
                                def create_article_callback(article_data):
                                    def callback(sender, app_data, user_data):
                                        self.open_full_article(article_data['link'], article_data['title'])
                                    return callback

                                dpg.add_button(label="👁️",
                                             callback=create_article_callback(article),
                                             width=60, height=20)

        except Exception:
            dpg.add_text(f"Error displaying {source_data.get('source_name', 'Unknown')}", color=self.BLOOMBERG_RED)

    def create_header_bar(self):
        try:
            with dpg.group(horizontal=True):
                dpg.add_text("📰 FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("NEWS TERMINAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" • ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(f"Sources: {len(self.news_sources)}", color=self.BLOOMBERG_YELLOW)
                dpg.add_text(" • ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(time.strftime('%Y-%m-%d %H:%M:%S'), color=self.BLOOMBERG_WHITE)
        except Exception:
            pass

    def create_control_panel(self):
        try:
            unique_id = id(self)

            with dpg.group(horizontal=True):
                dpg.add_text("🌐 Website:", color=self.BLOOMBERG_WHITE)
                dpg.add_input_text(tag=f"news_website_input_{unique_id}",
                                 width=250,
                                 hint="e.g., reuters.com, bbc.com")

                dpg.add_text("⏱️ Refresh (min):", color=self.BLOOMBERG_WHITE)
                dpg.add_input_int(tag=f"news_refresh_input_{unique_id}",
                                default_value=5,
                                width=80,
                                min_value=1,
                                max_value=1440)

                dpg.add_button(label="➕ ADD SOURCE",
                             callback=self.add_news_source,
                             width=120,
                             height=30)

                dpg.add_button(label="🔄 REFRESH ALL",
                             callback=self.refresh_all_sources,
                             width=120,
                             height=30)

            dpg.add_text("Ready to add news sources",
                       tag=f"news_status_{unique_id}",
                       color=self.BLOOMBERG_YELLOW)

            # Quick add buttons
            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_text("⚡ Quick Add:", color=self.BLOOMBERG_BLUE)

                popular_sources = [
                    ("Reuters", "reuters.com"),
                    ("BBC", "bbc.com"),
                    ("CNN", "cnn.com"),
                    ("TechCrunch", "techcrunch.com"),
                    ("Bloomberg", "bloomberg.com")
                ]

                for name, url in popular_sources:
                    def create_quick_add_callback(source_url):
                        def callback(sender, app_data, user_data):
                            self.quick_add_source(source_url)
                        return callback

                    dpg.add_button(label=name,
                                 callback=create_quick_add_callback(url),
                                 width=80,
                                 height=25)

        except Exception:
            pass

    def create_content(self):
        try:
            unique_id = id(self)

            self.create_header_bar()
            dpg.add_separator()

            self.create_control_panel()
            dpg.add_separator()

            with dpg.child_window(tag=f"news_main_window_{unique_id}", height=-50, border=False):
                dpg.add_text("📰 REAL-TIME NEWS FEEDS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                with dpg.group(tag=f"news_container_{unique_id}"):
                    if not self.news_sources:
                        with dpg.group():
                            dpg.add_text("📰 No news sources configured", color=self.BLOOMBERG_GRAY)
                            dpg.add_text("Add websites above to start receiving live news feeds", color=self.BLOOMBERG_YELLOW)
                            dpg.add_spacer(height=10)
                            dpg.add_text("💡 Supported: Most major news websites with RSS feeds", color=self.BLOOMBERG_WHITE)

            dpg.add_separator()
            self.create_status_bar()

            self.ui_initialized = True

        except Exception as e:
            dpg.add_text("📰 NEWS TERMINAL - ERROR", color=self.BLOOMBERG_RED)
            dpg.add_separator()
            dpg.add_text(f"Error loading interface: {str(e)}", color=self.BLOOMBERG_WHITE)

    def create_status_bar(self):
        try:
            with dpg.group(horizontal=True):
                dpg.add_text("📊 STATUS:", color=self.BLOOMBERG_GRAY)
                dpg.add_text("ACTIVE", color=self.BLOOMBERG_GREEN)

                dpg.add_text(" • ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("SOURCES:", color=self.BLOOMBERG_GRAY)

                active_sources = sum(1 for source in self.news_sources.values()
                                   if source.get('status') == 'Active')
                total_articles = sum(len(source.get('articles', []))
                                   for source in self.news_sources.values())

                dpg.add_text(f"{active_sources}/{len(self.news_sources)}", color=self.BLOOMBERG_YELLOW)

                dpg.add_text(" • ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("ARTICLES:", color=self.BLOOMBERG_GRAY)
                dpg.add_text(f"{total_articles}", color=self.BLOOMBERG_WHITE)

                dpg.add_text(" • ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("AUTO-REFRESH:", color=self.BLOOMBERG_GRAY)
                dpg.add_text("ON", color=self.BLOOMBERG_GREEN)

        except Exception:
            pass

    def quick_add_source(self, url):
        try:
            dpg.set_value(f"news_website_input_{id(self)}", url)
            dpg.set_value(f"news_refresh_input_{id(self)}", 5)
            self.add_news_source()
        except Exception:
            pass

    def refresh_all_sources(self):
        try:
            self.update_status_message("Refreshing all sources...", self.BLOOMBERG_YELLOW)

            def refresh_worker():
                refreshed_count = 0
                for source_id in list(self.news_sources.keys()):
                    try:
                        if source_id in self.news_sources:
                            self.news_sources[source_id]['status'] = 'Updating...'
                            articles = self.fetch_rss_feed(self.news_sources[source_id]['rss_url'], source_id)

                            if articles:
                                self.news_sources[source_id]['articles'] = articles
                                self.news_sources[source_id]['last_update'] = time.time()
                                self.news_sources[source_id]['status'] = 'Active'
                                refreshed_count += 1
                            else:
                                self.news_sources[source_id]['status'] = 'Error'
                    except Exception:
                        if source_id in self.news_sources:
                            self.news_sources[source_id]['status'] = 'Error'

                self.refresh_news_display()
                self.update_status_message(f"Refreshed {refreshed_count} sources", self.BLOOMBERG_GREEN)

            threading.Thread(target=refresh_worker, daemon=True).start()

        except Exception:
            self.update_status_message("Refresh failed", self.BLOOMBERG_RED)

    def cleanup(self):
        try:
            source_ids = list(self.news_sources.keys())
            for source_id in source_ids:
                if source_id in self.news_sources:
                    del self.news_sources[source_id]

            self.refresh_threads.clear()

            if self.conn:
                self.conn.close()
                self.conn = None

        except Exception:
            pass

    def __del__(self):
        self.cleanup()