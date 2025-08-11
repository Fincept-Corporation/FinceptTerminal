"""
Real-time News Analysis Tab for Fincept Terminal
Simplified version with better error handling and no complex async/playwright
"""

import dearpygui.dearpygui as dpg
import requests
import xml.etree.ElementTree as ET
import threading
import time
import re
from urllib.parse import urlparse
import duckdb
import os
from pathlib import Path
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import logger

# Try to import newspaper4k with proper error handling
try:
    import newspaper
    NEWSPAPER_AVAILABLE = True
    logger.info("Newspaper4k library loaded successfully")
except ImportError as e:
    NEWSPAPER_AVAILABLE = False
    logger.warning(f"Newspaper4k not available: {e}")

# Try to import playwright with proper error handling
try:
    from playwright.sync_api import sync_playwright
    PLAYWRIGHT_AVAILABLE = True
    logger.info("Playwright library loaded successfully")
except ImportError as e:
    PLAYWRIGHT_AVAILABLE = False
    logger.warning(f"Playwright not available: {e}")

class NewsAnalysisTab(BaseTab):
    """Real-time News Analysis Dashboard with RSS Feed Integration - Simplified Version"""

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

        self.news_sources = {}
        self.refresh_threads = {}
        self.conn = None
        self.ui_initialized = False

        # Common headers for requests
        self.headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
        }

        self.setup_database()
        # Load settings in background to avoid blocking startup
        threading.Thread(target=self.load_user_settings, daemon=True).start()

    def get_label(self):
        return "News"

    def _get_config_directory(self) -> Path:
        """Get platform-specific config directory"""
        config_dir = Path.home() / '.fincept' / 'news'
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir

    def setup_database(self):
        """Initialize DuckDB for user settings storage"""
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
            logger.info(f"Database initialized at: {db_path}")
        except Exception as e:
            logger.error(f"Database setup failed: {e}")
            self.conn = duckdb.connect(':memory:')
            self.conn.execute("""
                CREATE TABLE IF NOT EXISTS news_sources (
                    id INTEGER PRIMARY KEY,
                    website_url VARCHAR,
                    refresh_interval INTEGER,
                    source_name VARCHAR,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            """)

    def validate_news_website(self, url):
        """Simple validation for news websites"""
        try:
            if not url.startswith(('http://', 'https://')):
                url = 'https://' + url

            domain = urlparse(url).netloc.lower().replace('www.', '')

            if not domain:
                return False, "Invalid URL format"

            logger.info(f"Validating website: {domain}")

            # Try common RSS endpoints
            rss_endpoints = [
                f"https://{domain}/rss",
                f"https://{domain}/feed",
                f"https://{domain}/rss.xml",
                f"https://{domain}/feed.xml",
                f"https://www.{domain}/rss",
                f"https://www.{domain}/feed"
            ]

            for rss_url in rss_endpoints:
                try:
                    response = requests.get(rss_url, headers=self.headers, timeout=10)
                    if response.status_code == 200:
                        try:
                            root = ET.fromstring(response.content)
                            if root.tag in ['rss', 'feed'] or 'rss' in root.tag.lower():
                                logger.info(f"Found direct RSS feed: {rss_url}")
                                return True, "Direct RSS feed found"
                        except ET.ParseError:
                            continue
                except requests.RequestException:
                    continue

            # Check main website for RSS indicators
            try:
                main_url = f"https://{domain}"
                response = requests.get(main_url, headers=self.headers, timeout=10)
                if response.status_code == 200:
                    content = response.text.lower()
                    rss_indicators = [
                        'application/rss+xml',
                        'application/atom+xml',
                        '/rss',
                        '/feed',
                        'rss.xml'
                    ]
                    if any(indicator in content for indicator in rss_indicators):
                        logger.info(f"RSS indicators found on {domain}")
                        return True, "RSS feed detected"
            except requests.RequestException:
                pass

            # Try Google News RSS as fallback
            try:
                google_rss_url = f"https://news.google.com/rss/search?q=site%3A{domain}&hl=en-US&gl=US&ceid=US%3Aen"
                response = requests.get(google_rss_url, headers=self.headers, timeout=10)
                if response.status_code == 200:
                    root = ET.fromstring(response.content)
                    items = root.findall('.//item')
                    if len(items) > 0:
                        logger.info(f"Google News RSS works for {domain}")
                        return True, "Google News RSS available"
            except Exception:
                pass

            return False, f"No RSS feed found for {domain}. Please try a different news website."

        except Exception as e:
            logger.error(f"Validation error: {e}")
            return False, f"Unable to validate website: {str(e)}"

    def generate_rss_url(self, website_url):
        """Generate RSS URL for the website"""
        try:
            if not website_url.startswith(('http://', 'https://')):
                website_url = 'https://' + website_url

            domain = urlparse(website_url).netloc.replace('www.', '')

            # Try common RSS endpoints first
            common_rss_paths = [
                f"https://{domain}/rss",
                f"https://{domain}/feed",
                f"https://{domain}/rss.xml",
                f"https://{domain}/feed.xml"
            ]

            for rss_url in common_rss_paths:
                try:
                    response = requests.get(rss_url, headers=self.headers, timeout=8)
                    if response.status_code == 200:
                        try:
                            root = ET.fromstring(response.content)
                            if len(root.findall('.//item')) > 0:
                                logger.info(f"Using direct RSS: {rss_url}")
                                return rss_url
                        except ET.ParseError:
                            continue
                except requests.RequestException:
                    continue

            # Fallback to Google News RSS
            google_rss_url = f"https://news.google.com/rss/search?q=site%3A{domain}&hl=en-US&gl=US&ceid=US%3Aen"
            logger.info(f"Using Google News RSS for {domain}")
            return google_rss_url

        except Exception as e:
            logger.error(f"RSS URL generation failed: {e}")
            return None

    def fetch_rss_feed(self, rss_url, source_id=None):
        """Fetch and parse RSS feed"""
        try:
            response = requests.get(rss_url, headers=self.headers, timeout=15)
            response.raise_for_status()

            root = ET.fromstring(response.content)
            articles = []

            # Handle both RSS and Atom feeds
            items = root.findall('.//item') or root.findall('.//{http://www.w3.org/2005/Atom}entry')

            for item in items[:10]:  # Get top 10 articles
                # RSS format
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
                    description = item.find('.//{http://www.w3.org/2005/Atom}summary') or item.find('.//{http://www.w3.org/2005/Atom}content')

                article_url = link.text if link is not None and hasattr(link, 'text') and link.text else ''

                article = {
                    'title': title.text if title is not None and title.text else 'No title',
                    'link': article_url,
                    'pub_date': pub_date.text if pub_date is not None and pub_date.text else '',
                    'description': re.sub('<[^<]+?>', '', description.text) if description is not None and description.text else ''
                }
                articles.append(article)

            logger.info(f"Fetched {len(articles)} articles for source {source_id}")
            return articles
        except Exception as e:
            logger.error(f"RSS fetch error for {rss_url}: {e}")
            return []

    def get_final_url(self, url):
        """Get final redirected URL using Playwright"""
        if not PLAYWRIGHT_AVAILABLE:
            logger.warning("Playwright not available, using original URL")
            return url

        try:
            logger.info(f"Resolving URL with Playwright: {url}")
            with sync_playwright() as p:
                browser = p.chromium.launch(headless=True)
                page = browser.new_page()

                # Set a reasonable timeout and wait for network to be idle
                page.goto(url, wait_until='networkidle', timeout=15000)
                final_url = page.url

                browser.close()

                logger.info(f"URL resolved: {url} -> {final_url}")
                return final_url

        except Exception as e:
            logger.error(f"Playwright URL resolution failed: {e}")
            return url

    def extract_article_content(self, article_url):
        """Extract article content using newspaper4k with final URL resolution"""
        if not NEWSPAPER_AVAILABLE:
            return self.extract_with_requests(article_url)

        try:
            # First resolve the final URL if it's a Google News redirect
            final_url = article_url
            if 'news.google.com' in article_url and '/articles/' in article_url:
                logger.info(f"Resolving Google News URL: {article_url}")
                final_url = self.get_final_url(article_url)

                # Check if we successfully got the final URL
                if 'google.com' in final_url or 'Error:' in final_url:
                    logger.warning(f"Failed to resolve Google News URL: {final_url}")
                    return None, "Failed to resolve Google News redirect URL. The article may be behind a paywall or the redirect failed."

                logger.info(f"Successfully resolved to: {final_url}")

            # Use newspaper4k with the final URL
            article = newspaper.article(final_url)

            if not article:
                return self.extract_with_requests(final_url)

            article_data = {
                'title': getattr(article, 'title', 'No title'),
                'text': getattr(article, 'text', ''),
                'authors': getattr(article, 'authors', []),
                'publish_date': getattr(article, 'publish_date', None),
                'summary': '',
                'top_image': getattr(article, 'top_image', ''),
                'final_url': final_url  # Store the final URL for reference
            }

            # Try to get summary with NLP
            try:
                article.nlp()
                if hasattr(article, 'summary') and article.summary:
                    article_data['summary'] = article.summary
                if hasattr(article, 'keywords') and article.keywords:
                    article_data['keywords'] = getattr(article, 'keywords', [])
            except Exception as e:
                logger.warning(f"NLP processing failed: {e}")

            logger.info(f"Successfully extracted article content: {len(article_data['text'])} characters")
            return article_data, None

        except Exception as e:
            logger.error(f"Newspaper4k extraction failed: {e}")
            # Try fallback with final URL if we have it
            fallback_url = final_url if 'final_url' in locals() else article_url
            return self.extract_with_requests(fallback_url)

    def extract_with_requests(self, article_url):
        """Fallback extraction using requests and simple text parsing"""
        try:
            logger.info(f"Using fallback extraction for: {article_url}")

            # For Google News URLs, try to resolve them first
            final_url = article_url
            if 'news.google.com' in article_url and '/articles/' in article_url:
                logger.info(f"Attempting to resolve Google News URL with Playwright")
                final_url = self.get_final_url(article_url)

                if 'google.com' in final_url or 'Error:' in final_url:
                    return None, "Could not resolve Google News redirect URL. The article may be protected or the redirect failed."

            response = requests.get(final_url, headers=self.headers, timeout=20)

            if response.status_code == 403:
                return None, "Website blocked access (403 Forbidden). This site may use anti-bot protection."
            elif response.status_code == 404:
                return None, "Article not found (404). The URL may be invalid or the article may have been removed."
            elif response.status_code != 200:
                return None, f"Website returned error {response.status_code}. The site may be down or blocking requests."

            html_content = response.text

            # Simple text extraction
            # Remove script and style elements
            clean_html = re.sub(r'<script[^>]*>.*?</script>', '', html_content, flags=re.DOTALL | re.IGNORECASE)
            clean_html = re.sub(r'<style[^>]*>.*?</style>', '', clean_html, flags=re.DOTALL | re.IGNORECASE)

            # Extract title
            title_match = re.search(r'<title[^>]*>(.*?)</title>', clean_html, re.IGNORECASE | re.DOTALL)
            title = title_match.group(1) if title_match else 'Article Title'
            title = re.sub(r'<[^>]+>', '', title).strip()

            # Remove HTML tags
            clean_text = re.sub(r'<[^>]+>', '', clean_html)

            # Clean up whitespace
            clean_text = re.sub(r'\s+', ' ', clean_text).strip()

            # Try to extract article content (look for common article patterns)
            article_patterns = [
                r'<article[^>]*>(.*?)</article>',
                r'<div[^>]*class="[^"]*article[^"]*"[^>]*>(.*?)</div>',
                r'<div[^>]*class="[^"]*content[^"]*"[^>]*>(.*?)</div>',
                r'<main[^>]*>(.*?)</main>',
            ]

            article_content = ""
            for pattern in article_patterns:
                matches = re.findall(pattern, html_content, re.DOTALL | re.IGNORECASE)
                if matches:
                    # Take the longest match
                    article_content = max(matches, key=len)
                    article_content = re.sub(r'<[^>]+>', '', article_content)
                    article_content = re.sub(r'\s+', ' ', article_content).strip()
                    break

            # If no specific article content found, use a portion of the cleaned text
            if not article_content or len(article_content) < 100:
                # Try to find content between common article markers
                text_parts = clean_text.split()
                if len(text_parts) > 100:
                    # Take middle portion (skip navigation, headers, footers)
                    start_idx = len(text_parts) // 4
                    end_idx = 3 * len(text_parts) // 4
                    article_content = ' '.join(text_parts[start_idx:end_idx])
                else:
                    article_content = clean_text

            # Limit content length
            if len(article_content) > 5000:
                article_content = article_content[:5000] + "..."

            article_data = {
                'title': title,
                'text': article_content,
                'authors': [],
                'publish_date': None,
                'summary': '',
                'final_url': final_url
            }

            return article_data, None

        except requests.exceptions.Timeout:
            return None, "Request timed out. The website is taking too long to respond."
        except requests.exceptions.ConnectionError:
            return None, "Connection failed. Check your internet connection or the website may be down."
        except requests.exceptions.RequestException as e:
            return None, f"Request failed: {str(e)}"
        except Exception as e:
            logger.error(f"Fallback extraction error: {e}")
            return None, f"Content extraction failed: {str(e)}"

    def update_status_message(self, message, color=None):
        """Update status message with thread safety"""
        try:
            status_tag = f"news_status_{id(self)}"
            if dpg.does_item_exist(status_tag):
                dpg.set_value(status_tag, message)
                if color:
                    dpg.configure_item(status_tag, color=color)
        except Exception as e:
            logger.error(f"Status update error: {e}")

    def add_news_source(self):
        """Add new news source"""
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
                    self.update_status_message("No articles found - check website or network", self.BLOOMBERG_RED)
                    return

                source_name = urlparse(website_url if website_url.startswith(('http://', 'https://')) else 'https://' + website_url).netloc.replace('www.', '')

                # Generate auto-increment ID
                max_id_result = self.conn.execute("SELECT COALESCE(MAX(id), 0) FROM news_sources").fetchone()
                source_id = max_id_result[0] + 1

                # Save to database
                self.conn.execute("INSERT INTO news_sources (id, website_url, refresh_interval, source_name) VALUES (?, ?, ?, ?)",
                                (source_id, website_url, refresh_interval, source_name))

                # Add to memory
                self.news_sources[source_id] = {
                    'url': website_url,
                    'rss_url': rss_url,
                    'timer': refresh_interval,
                    'source_name': source_name,
                    'articles': test_articles,
                    'last_update': time.time(),
                    'status': 'Active'
                }

                # Start refresh timer
                self.start_refresh_timer(source_id)

                # Update UI
                self.refresh_news_display()

                # Clear inputs
                dpg.set_value(f"news_website_input_{id(self)}", "")
                dpg.set_value(f"news_refresh_input_{id(self)}", 5)
                self.update_status_message(f"Added: {source_name} - {len(test_articles)} articles", self.BLOOMBERG_GREEN)

            except Exception as e:
                logger.error(f"Add source error: {e}")
                self.update_status_message(f"Error: {str(e)}", self.BLOOMBERG_RED)

        threading.Thread(target=validation_worker, daemon=True).start()

    def start_refresh_timer(self, source_id):
        """Start refresh timer for a news source"""
        def refresh_worker():
            try:
                while source_id in self.news_sources:
                    time.sleep(self.news_sources[source_id]['timer'] * 60)

                    if source_id not in self.news_sources:
                        break

                    logger.info(f"Refreshing source {source_id}")
                    self.news_sources[source_id]['status'] = 'Updating...'

                    articles = self.fetch_rss_feed(self.news_sources[source_id]['rss_url'], source_id)

                    if source_id in self.news_sources:
                        if articles:
                            self.news_sources[source_id]['articles'] = articles
                            self.news_sources[source_id]['last_update'] = time.time()
                            self.news_sources[source_id]['status'] = 'Active'
                            logger.info(f"Updated source {source_id} with {len(articles)} articles")
                        else:
                            self.news_sources[source_id]['status'] = 'Error'

                        self.refresh_news_display()

            except Exception as e:
                logger.error(f"Refresh worker error for source {source_id}: {e}")
                if source_id in self.news_sources:
                    self.news_sources[source_id]['status'] = 'Error'

        refresh_thread = threading.Thread(target=refresh_worker, daemon=True)
        refresh_thread.start()
        self.refresh_threads[source_id] = refresh_thread

    def refresh_single_source(self, source_id):
        """Refresh a single news source"""
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
                            logger.info(f"Refreshed source {source_id} with {len(articles)} articles")
                        else:
                            self.news_sources[source_id]['status'] = 'Error'
                            self.update_status_message(f"Failed to refresh {self.news_sources[source_id]['source_name']}", self.BLOOMBERG_RED)

                        self.refresh_news_display()

                except Exception as e:
                    logger.error(f"Single refresh error for source {source_id}: {e}")
                    if source_id in self.news_sources:
                        self.news_sources[source_id]['status'] = 'Error'
                        self.update_status_message(f"Error refreshing {self.news_sources[source_id]['source_name']}", self.BLOOMBERG_RED)
                        self.refresh_news_display()

            threading.Thread(target=refresh_worker, daemon=True).start()

        except Exception as e:
            logger.error(f"Refresh single source error: {e}")
            self.update_status_message("Refresh failed", self.BLOOMBERG_RED)
        """Delete news source with proper cleanup"""
        try:
            if self.conn:
                self.conn.execute("DELETE FROM news_sources WHERE id = ?", (source_id,))

            if source_id in self.news_sources:
                del self.news_sources[source_id]

            if source_id in self.refresh_threads:
                del self.refresh_threads[source_id]

            self.refresh_news_display()
            self.update_status_message(f"Deleted news source", self.BLOOMBERG_GREEN)

        except Exception as e:
            logger.error(f"Delete source error: {e}")
            self.update_status_message(f"Error deleting source", self.BLOOMBERG_RED)

    def load_user_settings(self):
        """Load user settings from database"""
        try:
            if not self.conn:
                return

            sources = self.conn.execute("SELECT * FROM news_sources ORDER BY id").fetchall()
            logger.info(f"Loading {len(sources)} saved sources")

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
                    except Exception as e:
                        logger.error(f"Load source error for {sid}: {e}")
                        if sid in self.news_sources:
                            self.news_sources[sid]['status'] = 'Error'

                threading.Thread(target=load_source, args=(source_id, rss_url), daemon=True).start()
                self.start_refresh_timer(source_id)

        except Exception as e:
            logger.error(f"Load settings error: {e}")

    def open_full_article(self, article_url, article_title):
        """Open full article with simplified extraction"""
        def fetch_article_worker():
            window_id = f"article_window_{hash(article_url)}"
            content_tag = f"article_content_{hash(article_url)}"

            try:
                if dpg.does_item_exist(content_tag):
                    dpg.set_value(content_tag, f"Loading article: {article_title}\n\nExtracting content from: {article_url}\n\nPlease wait...")

                # Extract article content
                article_data, error = self.extract_article_content(article_url)

                if error or not article_data:
                    error_msg = f"❌ Failed to extract article content\n\n"
                    error_msg += f"TITLE: {article_title}\n"
                    error_msg += f"URL: {article_url}\n\n"
                    if error:
                        error_msg += f"ERROR: {error}\n\n"
                    error_msg += "COMMON CAUSES:\n"
                    error_msg += "• Website requires JavaScript or has anti-bot protection\n"
                    error_msg += "• Content is behind a paywall or login\n"
                    error_msg += "• Website is blocking automated access\n"
                    error_msg += "• Network connectivity issues\n\n"
                    error_msg += "SOLUTIONS:\n"
                    error_msg += "1. Click 'Open in Browser' to read the article directly\n"
                    error_msg += "2. Try a different news source\n"
                    error_msg += "3. Check if the website requires subscription\n"
                    error_msg += f"\nDirect link: {article_url}"

                    if dpg.does_item_exist(content_tag):
                        dpg.set_value(content_tag, error_msg)
                    return

                # Format article content
                content = f"TITLE: {article_data['title']}\n\n"

                if article_data['publish_date']:
                    content += f"PUBLISHED: {article_data['publish_date']}\n"

                if article_data['authors']:
                    content += f"AUTHORS: {', '.join(article_data['authors'])}\n"

                # Show both original and final URLs if different
                content += f"SOURCE: {article_data.get('final_url', article_url)}\n"
                if article_data.get('final_url') and article_data['final_url'] != article_url:
                    content += f"ORIGINAL: {article_url}\n"
                content += "\n"

                if article_data['summary']:
                    content += f"SUMMARY:\n{article_data['summary']}\n\n"

                # Show keywords if available
                if article_data.get('keywords'):
                    content += f"KEYWORDS: {', '.join(article_data['keywords'])}\n\n"

                content += "ARTICLE CONTENT:\n" + "="*70 + "\n\n"

                if article_data['text'] and len(article_data['text'].strip()) > 50:
                    clean_text = re.sub(r'\n\s*\n', '\n\n', article_data['text'].strip())
                    clean_text = re.sub(r'\n{3,}', '\n\n', clean_text)
                    clean_text = re.sub(r' {2,}', ' ', clean_text)

                    content += clean_text

                    word_count = len(clean_text.split())
                    char_count = len(clean_text)
                    content += f"\n\n" + "="*70
                    content += f"\nSTATS: {word_count} words, {char_count} characters"

                else:
                    content += "⚠️ No article content could be extracted.\n\n"
                    content += "This is likely because:\n"
                    content += "• The website uses dynamic content loading\n"
                    content += "• Content is behind a paywall\n"
                    content += "• The site has anti-scraping protection\n\n"
                    content += "💡 Click 'Open in Browser' to read the full article."

                if dpg.does_item_exist(content_tag):
                    dpg.set_value(content_tag, content)

                logger.info(f"Successfully processed article: {len(article_data.get('text', ''))} characters")

            except Exception as e:
                logger.error(f"Article fetch error: {e}")
                error_msg = f"❌ Unexpected error loading article\n\n"
                error_msg += f"TITLE: {article_title}\n"
                error_msg += f"URL: {article_url}\n\n"
                error_msg += f"ERROR: {str(e)}\n\n"
                error_msg += "Please try:\n"
                error_msg += "1. Clicking 'Open in Browser'\n"
                error_msg += "2. Checking your internet connection\n"
                error_msg += "3. Trying again later\n"

                if dpg.does_item_exist(content_tag):
                    dpg.set_value(content_tag, error_msg)

        # Create article window
        window_id = f"article_window_{hash(article_url)}"
        content_tag = f"article_content_{hash(article_url)}"

        if dpg.does_item_exist(window_id):
            dpg.delete_item(window_id)

        try:
            with dpg.window(label=f"Article: {article_title[:50]}{'...' if len(article_title) > 50 else ''}",
                           tag=window_id, width=1000, height=750,
                           pos=[100, 100], modal=False):

                with dpg.group(horizontal=True):
                    dpg.add_button(label="❌ Close",
                                 callback=lambda: dpg.delete_item(window_id),
                                 width=80, height=30)

                    dpg.add_button(label="🌐 Open in Browser",
                                 callback=lambda: self.open_in_browser(article_url),
                                 width=150, height=30)

                    dpg.add_button(label="🔄 Reload",
                                 callback=lambda: threading.Thread(target=fetch_article_worker, daemon=True).start(),
                                 width=80, height=30)

                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                    dpg.add_text("TIP: If content fails to load, try 'Open in Browser'", color=self.BLOOMBERG_YELLOW)

                dpg.add_separator()

                dpg.add_input_text(tag=content_tag,
                                 default_value="🔄 Loading article...\n\nPlease wait while we extract the content.",
                                 multiline=True,
                                 width=-1,
                                 height=-50,
                                 readonly=True)

            threading.Thread(target=fetch_article_worker, daemon=True).start()

        except Exception as e:
            logger.error(f"Article window creation error: {e}")

    def open_in_browser(self, url):
        """Open URL in default browser"""
        try:
            import webbrowser
            webbrowser.open(url)
        except Exception as e:
            logger.error(f"Browser open error: {e}")

    def refresh_news_display(self):
        """Refresh the news display"""
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
                        dpg.add_text("No news sources added yet", color=self.BLOOMBERG_GRAY)
                        dpg.add_text("Add a website above to start receiving news", color=self.BLOOMBERG_YELLOW)
                else:
                    self.create_news_grid()

        except Exception as e:
            logger.error(f"Display refresh error: {e}")

    def create_news_grid(self):
        """Create news grid layout"""
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

        except Exception as e:
            logger.error(f"News grid creation error: {e}")

    def create_news_panel(self, source_id, source_data, width, height):
        """Create individual news panel"""
        try:
            with dpg.child_window(width=width, height=height, border=True):
                with dpg.group(horizontal=True):
                    dpg.add_text(f"{source_data['source_name'].upper()}", color=self.BLOOMBERG_ORANGE)
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)

                    status = source_data.get('status', 'Unknown')
                    status_color = self.BLOOMBERG_GREEN if status == 'Active' else \
                                 self.BLOOMBERG_YELLOW if status in ['Loading...', 'Updating...'] else \
                                 self.BLOOMBERG_RED
                    dpg.add_text(f"{status}", color=status_color)

                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                    dpg.add_text(f"Refresh: {source_data['timer']}min", color=self.BLOOMBERG_GRAY)

                    if source_data['last_update']:
                        last_update = time.strftime('%H:%M:%S', time.localtime(source_data['last_update']))
                        dpg.add_text(f" | Updated: {last_update}", color=self.BLOOMBERG_GREEN)

                    def create_delete_callback(source_id_to_delete):
                        def callback(sender, app_data, user_data):
                            self.delete_news_source(source_id_to_delete)
                        return callback

                    dpg.add_button(label="DELETE",
                                 callback=create_delete_callback(source_id),
                                 width=60, height=20)

                dpg.add_separator()

                articles = source_data.get('articles', [])

                with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                             scrollY=True, scrollX=True, height=height - 80):
                    dpg.add_table_column(label="Title", width_fixed=True, init_width_or_weight=500)
                    dpg.add_table_column(label="Published", width_fixed=True, init_width_or_weight=120)
                    dpg.add_table_column(label="Action", width_fixed=True, init_width_or_weight=80)

                    if not articles:
                        with dpg.table_row():
                            dpg.add_text("Loading articles...", color=self.BLOOMBERG_YELLOW)
                            dpg.add_text("", color=self.BLOOMBERG_GRAY)
                            dpg.add_text("", color=self.BLOOMBERG_GRAY)
                    else:
                        for article in articles:
                            with dpg.table_row():
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

                                pub_date = article.get('pub_date', '')
                                if pub_date:
                                    date_str = pub_date[:16] if len(pub_date) > 16 else pub_date
                                    dpg.add_text(date_str, color=self.BLOOMBERG_GRAY)
                                else:
                                    dpg.add_text("Unknown", color=self.BLOOMBERG_GRAY)

                                def create_article_callback(article_data):
                                    def callback(sender, app_data, user_data):
                                        self.open_full_article(article_data['link'], article_data['title'])
                                    return callback

                                dpg.add_button(label="VIEW",
                                             callback=create_article_callback(article),
                                             width=60, height=20)

        except Exception as e:
            logger.error(f"Panel creation error: {e}")
            dpg.add_text(f"Error displaying {source_data.get('source_name', 'Unknown')}", color=self.BLOOMBERG_RED)

    def create_header_bar(self):
        """Create header bar"""
        try:
            with dpg.group(horizontal=True):
                dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("NEWS TERMINAL", color=self.BLOOMBERG_WHITE)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(f"SOURCES: {len(self.news_sources)}", color=self.BLOOMBERG_YELLOW)
                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(time.strftime('%Y-%m-%d %H:%M:%S'), color=self.BLOOMBERG_WHITE)
        except Exception as e:
            logger.error(f"Failed to create header bar: {e}")

    def create_control_panel(self):
        """Create control panel"""
        try:
            unique_id = id(self)

            with dpg.group(horizontal=True):
                dpg.add_text("Website:", color=self.BLOOMBERG_WHITE)
                dpg.add_input_text(tag=f"news_website_input_{unique_id}",
                                 width=250,
                                 hint="e.g., reuters.com, bbc.com")

                dpg.add_text("Refresh (min):", color=self.BLOOMBERG_WHITE)
                dpg.add_input_int(tag=f"news_refresh_input_{unique_id}",
                                default_value=5,
                                width=80,
                                min_value=1,
                                max_value=1440)

                dpg.add_button(label="ADD SOURCE",
                             callback=self.add_news_source,
                             width=100,
                             height=25)

                dpg.add_button(label="REFRESH ALL",
                             callback=self.refresh_all_sources,
                             width=100,
                             height=25)

            dpg.add_text("Ready to add news sources",
                       tag=f"news_status_{unique_id}",
                       color=self.BLOOMBERG_YELLOW)

            dpg.add_spacer(height=5)
            with dpg.group(horizontal=True):
                dpg.add_text("Quick Add:", color=self.BLOOMBERG_GRAY)

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
                                 height=20)

        except Exception as e:
            logger.error(f"Failed to create control panel: {e}")

    def create_content(self):
        """Create the news analysis dashboard"""
        try:
            unique_id = id(self)

            self.create_header_bar()
            dpg.add_separator()

            self.create_control_panel()
            dpg.add_separator()

            with dpg.child_window(tag=f"news_main_window_{unique_id}", height=-50, border=False):
                dpg.add_text("REAL-TIME NEWS FEEDS", color=self.BLOOMBERG_ORANGE)
                dpg.add_separator()

                with dpg.group(tag=f"news_container_{unique_id}"):
                    if not self.news_sources:
                        with dpg.group():
                            dpg.add_text("No news sources configured", color=self.BLOOMBERG_GRAY)
                            dpg.add_text("Add websites above to start receiving live news feeds", color=self.BLOOMBERG_YELLOW)
                            dpg.add_spacer(height=10)
                            dpg.add_text("Supported sources: Most major news websites with RSS feeds", color=self.BLOOMBERG_WHITE)
                            dpg.add_text("Examples: reuters.com, bbc.com, cnn.com, techcrunch.com", color=self.BLOOMBERG_GRAY)

            dpg.add_separator()
            self.create_status_bar()

            self.ui_initialized = True
            logger.info("News tab content created successfully")

        except Exception as e:
            logger.error(f"Failed to create news content: {e}")
            dpg.add_text("NEWS TERMINAL - ERROR", color=self.BLOOMBERG_RED)
            dpg.add_separator()
            dpg.add_text(f"Error loading news interface: {str(e)}", color=self.BLOOMBERG_WHITE)

    def create_status_bar(self):
        """Create status bar"""
        try:
            with dpg.group(horizontal=True):
                dpg.add_text("NEWS STATUS:", color=self.BLOOMBERG_GRAY)
                dpg.add_text("ACTIVE", color=self.BLOOMBERG_GREEN)

                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("SOURCES:", color=self.BLOOMBERG_GRAY)

                active_sources = sum(1 for source in self.news_sources.values()
                                   if source.get('status') == 'Active')
                total_articles = sum(len(source.get('articles', []))
                                   for source in self.news_sources.values())

                dpg.add_text(f"{active_sources}/{len(self.news_sources)}", color=self.BLOOMBERG_YELLOW)

                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("ARTICLES:", color=self.BLOOMBERG_GRAY)
                dpg.add_text(f"{total_articles}", color=self.BLOOMBERG_WHITE)

                dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text("AUTO-REFRESH:", color=self.BLOOMBERG_GRAY)
                dpg.add_text("ON", color=self.BLOOMBERG_GREEN)

        except Exception as e:
            logger.error(f"Failed to create status bar: {e}")

    def quick_add_source(self, url):
        """Quick add popular news sources"""
        try:
            dpg.set_value(f"news_website_input_{id(self)}", url)
            dpg.set_value(f"news_refresh_input_{id(self)}", 5)
            self.add_news_source()
        except Exception as e:
            logger.error(f"Quick add error: {e}")

    def refresh_all_sources(self):
        """Manually refresh all sources"""
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
                    except Exception as e:
                        logger.error(f"Error refreshing source {source_id}: {e}")
                        if source_id in self.news_sources:
                            self.news_sources[source_id]['status'] = 'Error'

                self.refresh_news_display()
                self.update_status_message(f"Refreshed {refreshed_count} sources", self.BLOOMBERG_GREEN)

            threading.Thread(target=refresh_worker, daemon=True).start()

        except Exception as e:
            logger.error(f"Refresh all error: {e}")
            self.update_status_message("Refresh failed", self.BLOOMBERG_RED)

    def cleanup(self):
        """Enhanced cleanup with proper resource management"""
        try:
            logger.info("Starting news analysis tab cleanup")

            source_ids = list(self.news_sources.keys())
            for source_id in source_ids:
                if source_id in self.news_sources:
                    del self.news_sources[source_id]

            self.refresh_threads.clear()

            if self.conn:
                self.conn.close()
                self.conn = None

            logger.info("News analysis tab cleanup completed")

        except Exception as e:
            logger.error(f"Cleanup error: {e}")

    def __del__(self):
        """Destructor to ensure cleanup"""
        self.cleanup()