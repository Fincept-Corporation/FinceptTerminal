# -*- coding: utf-8 -*-
# wiki_tab.py

import dearpygui.dearpygui as dpg
import threading
import wikipedia
import requests
from PIL import Image
import io
import numpy as np
from datetime import datetime
import webbrowser
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import info, error, warning, debug, monitor_performance, operation


class WikipediaTab(BaseTab):

    def __init__(self, app):
        super().__init__(app)

        # Colors matching terminal theme
        self.ORANGE = [255, 165, 0]
        self.WHITE = [255, 255, 255]
        self.GREEN = [0, 200, 0]
        self.BLUE = [0, 128, 255]
        self.GRAY = [120, 120, 120]
        self.YELLOW = [255, 255, 0]
        self.RED = [255, 100, 100]

        # State management
        self.loading = False
        self.loaded_images = {}
        self.current_article = None
        self.search_history = []
        self.bookmarked_articles = []
        self._search_cache = {}  # Cache for search results
        self._article_cache = {}  # Cache for article content

        # Initialize Wikipedia settings
        wikipedia.set_lang("en")

        info("Wikipedia tab initialized", context={"language": "en"})

    def get_label(self):
        return "Wikipedia"

    def create_content(self):
        """Create Wikipedia interface content"""
        try:
            self.add_section_header("📚 Wikipedia Search & Research")

            # Status and control panel
            self.create_status_panel()
            dpg.add_spacer(height=10)

            # Search interface
            self.create_search_panel()
            dpg.add_spacer(height=15)

            # Main layout with proper 20-60-20 split
            self.create_main_layout()

            info("Wikipedia tab content created successfully")

        except Exception as e:
            error("Error creating Wikipedia tab content", context={"error": str(e)}, exc_info=True)
            # Fallback UI
            dpg.add_text("Error loading Wikipedia interface", color=self.RED)
            dpg.add_button(label="Retry", callback=lambda: self.create_content())

    def create_status_panel(self):
        """Show Wikipedia status and statistics"""
        with dpg.group():
            dpg.add_text("📊 Wikipedia Status:", color=self.YELLOW)

            with dpg.group(horizontal=True):
                dpg.add_text("Language: English", color=self.GRAY)
                dpg.add_spacer(width=20)
                dpg.add_text("Articles Searched: 0", tag="search_count", color=self.GRAY)
                dpg.add_spacer(width=20)
                dpg.add_text("Current Article: None", tag="current_article_status", color=self.GRAY)
                dpg.add_spacer(width=20)
                dpg.add_text("Bookmarks: 0", tag="bookmark_count", color=self.GRAY)

    def create_search_panel(self):
        """Create search controls and options"""
        # Main search bar
        with dpg.group(horizontal=True):
            dpg.add_text("🔍 Search:", color=self.ORANGE)
            dpg.add_input_text(
                hint="Enter search term...",
                width=400,
                tag="wiki_search_input",
                callback=self.on_search_enter,
                on_enter=True
            )
            dpg.add_button(label="Search", width=80, callback=self.execute_search)
            dpg.add_button(label="Clear", width=60, callback=self.clear_search)

        dpg.add_spacer(height=10)

        # Quick search categories and controls
        with dpg.group(horizontal=True):
            dpg.add_text("Quick:", color=self.YELLOW)
            for term in ["Technology", "Science", "History", "Finance", "Medicine", "Geography"]:
                dpg.add_button(
                    label=term, width=80,
                    callback=lambda s, a, t=term: self.quick_search(t)
                )

        dpg.add_spacer(height=10)

        # Advanced options
        with dpg.group(horizontal=True):
            dpg.add_checkbox(label="Auto-load images", tag="auto_load_images", default_value=True)
            dpg.add_spacer(width=20)
            dpg.add_combo(["English", "Spanish", "French", "German", "Italian"],
                          default_value="English", tag="wiki_language",
                          callback=self.on_language_changed, width=100)
            dpg.add_spacer(width=20)
            dpg.add_button(label="📖 History", callback=self.show_search_history)
            dpg.add_button(label="⭐ Bookmarks", callback=self.show_bookmarks)
            dpg.add_button(label="📤 Export", callback=self.export_article)

    def create_main_layout(self):
        """Create the main 20-60-20 layout"""
        usable_width, content_height = self.get_dimensions()
        results_width, article_width, suggestions_width = self.calculate_panel_widths()

        with dpg.group(horizontal=True):
            # Left panel - Search Results (20%)
            self.create_results_panel(results_width, content_height)
            dpg.add_spacer(width=2)

            # Middle panel - Article Content (60%)
            self.create_article_panel(article_width, content_height)
            dpg.add_spacer(width=2)

            # Right panel - Suggestions & Tools (20%)
            self.create_suggestions_panel(suggestions_width, content_height)

    def get_dimensions(self):
        """Get proper dimensions with padding consideration"""
        try:
            viewport_width = dpg.get_viewport_width()
            viewport_height = dpg.get_viewport_height()
        except:
            viewport_width = 1200
            viewport_height = 800

        usable_width = viewport_width - 40
        content_height = viewport_height - 200  # More space for headers

        return usable_width, content_height

    def calculate_panel_widths(self):
        """Calculate exact panel widths to prevent overflow"""
        usable_width, _ = self.get_dimensions()

        border_space = 6
        gap_space = 4
        available_width = usable_width - border_space - gap_space

        results_width = int(available_width * 0.20)
        article_width = int(available_width * 0.60)
        suggestions_width = available_width - results_width - article_width

        return results_width, article_width, suggestions_width

    def create_results_panel(self, width, height):
        """Create search results panel"""
        with self.create_child_window(tag="results_panel", width=width, height=height):
            dpg.add_text("🔍 SEARCH RESULTS", color=self.ORANGE)
            dpg.add_separator()
            dpg.add_text("Enter search term to begin", tag="results_status", color=self.GRAY)
            dpg.add_child_window(height=-1, border=False, tag="results_list")

    def create_article_panel(self, width, height):
        """Create article content panel"""
        with self.create_child_window(tag="article_panel", width=width, height=height):
            dpg.add_text("📄 ARTICLE CONTENT", color=self.ORANGE)
            dpg.add_separator()

            # Article header with tools
            with dpg.group(horizontal=True):
                dpg.add_text("Select an article from results", tag="article_title", color=self.WHITE)
                dpg.add_spacer(width=20)
                dpg.add_button(label="⭐", width=30, tag="bookmark_btn",
                               callback=self.bookmark_article, show=False)
                dpg.add_button(label="🔗", width=30, tag="open_web_btn",
                               callback=self.open_in_browser, show=False)

            dpg.add_separator()
            dpg.add_child_window(height=-1, border=False, tag="article_content")

    def create_suggestions_panel(self, width, height):
        """Create suggestions and tools panel"""
        with self.create_child_window(tag="suggestions_panel", width=width, height=height):
            dpg.add_text("🔗 RELATED & TOOLS", color=self.ORANGE)
            dpg.add_separator()

            # Tools section
            dpg.add_text("📊 Article Stats:", color=self.YELLOW)
            dpg.add_text("Word count: 0", tag="word_count", color=self.GRAY)
            dpg.add_text("Read time: 0 min", tag="read_time", color=self.GRAY)
            dpg.add_text("Last updated: N/A", tag="last_updated", color=self.GRAY)

            dpg.add_spacer(height=10)
            dpg.add_separator()

            dpg.add_text("Related articles will appear here", tag="suggestions_status", color=self.GRAY)
            dpg.add_child_window(height=-1, border=False, tag="suggestions_list")

    # Search functionality
    def on_search_enter(self, sender, app_data):
        """Handle search input enter key"""
        self.execute_search()

    def quick_search(self, term):
        """Execute quick search for predefined terms"""
        dpg.set_value("wiki_search_input", term)
        self.execute_search()
        debug(f"Quick search executed: {term}")

    def clear_search(self):
        """Clear search input and results"""
        dpg.set_value("wiki_search_input", "")
        dpg.delete_item("results_list", children_only=True)
        dpg.set_value("results_status", "Search cleared")
        info("Search cleared")

    @monitor_performance
    def execute_search(self):
        """Execute Wikipedia search with threading and caching"""
        if self.loading:
            warning("Search already in progress")
            return

        search_term = dpg.get_value("wiki_search_input")
        if not search_term or not search_term.strip():
            dpg.set_value("results_status", "⚠️ Please enter a search term")
            return

        def search_thread():
            try:
                self.loading = True

                with operation("wikipedia_search", context={"search_term": search_term}):
                    dpg.set_value("results_status", f"🔍 Searching '{search_term}'...")
                    dpg.delete_item("results_list", children_only=True)

                    # Clear suggestions
                    dpg.delete_item("suggestions_list", children_only=True)
                    dpg.set_value("suggestions_status", "Search for an article first")

                    # Check cache first (10 minute cache for searches)
                    cache_key = search_term.lower()
                    if cache_key in self._search_cache:
                        cached_time, cached_results = self._search_cache[cache_key]
                        if datetime.now().timestamp() - cached_time < 600:  # 10 minutes
                            results = cached_results
                            info("Wikipedia search loaded from cache", context={
                                "search_term": search_term,
                                "results_count": len(results)
                            })
                        else:
                            # Cache expired, remove it
                            del self._search_cache[cache_key]
                            results = None
                    else:
                        results = None

                    # Perform search if not cached
                    if results is None:
                        results = wikipedia.search(search_term, results=15)
                        # Cache the results
                        self._search_cache[cache_key] = (datetime.now().timestamp(), results)

                    # Add to search history
                    if search_term not in self.search_history:
                        self.search_history.append(search_term)
                        if len(self.search_history) > 20:
                            self.search_history.pop(0)

                    if results:
                        dpg.set_value("results_status", f"✅ Found {len(results)} results")
                        for i, result in enumerate(results):
                            self.create_result_item(result, i)

                        # Update search count
                        current_count = len(self.search_history)
                        dpg.set_value("search_count", f"Articles Searched: {current_count}")

                        info("Wikipedia search completed successfully", context={
                            "search_term": search_term,
                            "results_count": len(results)
                        })
                    else:
                        dpg.set_value("results_status", "❌ No results found")
                        info("Wikipedia search returned no results", context={"search_term": search_term})

            except wikipedia.exceptions.DisambiguationError as e:
                dpg.set_value("results_status", "🔀 Multiple matches found")
                dpg.delete_item("results_list", children_only=True)
                for i, option in enumerate(e.options[:15]):
                    self.create_result_item(option, i)
                info("Wikipedia disambiguation handled", context={
                    "search_term": search_term,
                    "options_count": len(e.options[:15])
                })
            except Exception as e:
                error_msg = f"❌ Search error: {str(e)[:30]}"
                dpg.set_value("results_status", error_msg)
                error("Wikipedia search error", context={
                    "search_term": search_term,
                    "error": str(e)
                }, exc_info=True)
            finally:
                self.loading = False

        threading.Thread(target=search_thread, daemon=True).start()
        info(f"Wikipedia search started", context={"search_term": search_term})

    def create_result_item(self, title, index):
        """Create search result item"""
        with dpg.group(parent="results_list"):
            display_title = f"{index + 1}. {title}"

            dpg.add_button(
                label=display_title,
                callback=lambda: self.load_article(title),
                width=-1, height=40
            )
            dpg.add_spacer(height=3)

    @monitor_performance
    def load_article(self, title):
        """Load Wikipedia article with comprehensive error handling and caching"""
        if self.loading:
            warning("Article loading already in progress")
            return

        def load_thread():
            try:
                self.loading = True
                self.current_article = title

                with operation("load_wikipedia_article", context={"title": title}):
                    dpg.set_value("article_title", f"📄 Loading {title}...")
                    dpg.delete_item("article_content", children_only=True)

                    # Update current article status
                    dpg.set_value("current_article_status", f"Current Article: {title[:30]}...")

                    # Check article cache first (30 minute cache)
                    cache_key = title.lower()
                    if cache_key in self._article_cache:
                        cached_time, cached_page = self._article_cache[cache_key]
                        if datetime.now().timestamp() - cached_time < 1800:  # 30 minutes
                            page = cached_page
                            info("Wikipedia article loaded from cache", context={"title": title})
                        else:
                            # Cache expired, remove it
                            del self._article_cache[cache_key]
                            page = None
                    else:
                        page = None

                    # Load page if not cached
                    if page is None:
                        # Get page with fallback handling
                        try:
                            page = wikipedia.page(title)
                        except wikipedia.exceptions.DisambiguationError as e:
                            page = wikipedia.page(e.options[0])
                            info("Wikipedia disambiguation resolved", context={
                                "original_title": title,
                                "resolved_title": e.options[0]
                            })
                        except wikipedia.exceptions.PageError:
                            search_results = wikipedia.search(title, results=1)
                            if search_results:
                                page = wikipedia.page(search_results[0])
                                info("Wikipedia page found via search", context={
                                    "original_title": title,
                                    "found_title": search_results[0]
                                })
                            else:
                                raise

                        # Cache the page
                        self._article_cache[cache_key] = (datetime.now().timestamp(), page)

                    # Update article title with tools
                    dpg.set_value("article_title", f"📄 {page.title}")
                    dpg.configure_item("bookmark_btn", show=True)
                    dpg.configure_item("open_web_btn", show=True)

                    # Store page URL for browser opening
                    self.current_page_url = page.url

                    dpg.delete_item("article_content", children_only=True)

                    with dpg.group(parent="article_content"):
                        # Article metadata
                        dpg.add_text("📊 Article Information:", color=self.BLUE)
                        dpg.add_text(f"URL: {page.url}", color=self.WHITE, wrap=0)

                        # Calculate article stats
                        content_length = len(page.content)
                        word_count = len(page.content.split())
                        read_time = max(1, word_count // 200)  # Average reading speed

                        dpg.set_value("word_count", f"Word count: {word_count:,}")
                        dpg.set_value("read_time", f"Read time: {read_time} min")

                        dpg.add_spacer(height=10)

                        # Load main image if enabled
                        if dpg.get_value("auto_load_images"):
                            self.load_main_image(page)

                        # Summary section
                        dpg.add_text("📝 SUMMARY", color=self.YELLOW)
                        try:
                            summary = wikipedia.summary(page.title, sentences=4)
                            dpg.add_text(summary, color=self.WHITE, wrap=0)
                        except:
                            dpg.add_text("Summary not available", color=self.GRAY)

                        dpg.add_spacer(height=15)

                        # Full content
                        dpg.add_text("📖 FULL CONTENT", color=self.YELLOW)
                        self.process_content(page)

                        # Load related content
                        self.load_suggestions(page)

                    info("Wikipedia article loaded successfully", context={
                        "title": page.title,
                        "word_count": word_count,
                        "read_time": read_time
                    })

            except Exception as e:
                dpg.delete_item("article_content", children_only=True)
                with dpg.group(parent="article_content"):
                    dpg.add_text(f"❌ Error loading article: {str(e)}", color=self.RED)
                error("Error loading Wikipedia article", context={
                    "title": title,
                    "error": str(e)
                }, exc_info=True)
            finally:
                self.loading = False

        threading.Thread(target=load_thread, daemon=True).start()
        info(f"Wikipedia article loading started", context={"title": title})

    def load_suggestions(self, page):
        """Load related articles and categories with error handling"""
        try:
            dpg.delete_item("suggestions_list", children_only=True)
            dpg.set_value("suggestions_status", "Loading related content...")

            with dpg.group(parent="suggestions_list"):
                # Related articles
                if hasattr(page, 'links') and page.links:
                    dpg.add_text("🔗 RELATED ARTICLES", color=self.GREEN)
                    dpg.add_separator()

                    for i, link in enumerate(page.links[:10]):
                        dpg.add_button(
                            label=link,
                            callback=lambda l=link: self.load_article(l),
                            width=-1, height=35
                        )
                        dpg.add_spacer(height=3)

                dpg.add_spacer(height=10)

                # Categories
                if hasattr(page, 'categories') and page.categories:
                    dpg.add_text("📂 CATEGORIES", color=self.YELLOW)
                    dpg.add_separator()

                    for category in page.categories[:8]:
                        cat_name = category.replace("Category:", "")
                        dpg.add_text(f"• {cat_name}", color=self.WHITE, wrap=0)
                        dpg.add_spacer(height=3)

            dpg.set_value("suggestions_status", "✅ Related content loaded")
            debug("Wikipedia suggestions loaded successfully", context={
                "links_count": len(page.links[:10]) if hasattr(page, 'links') and page.links else 0,
                "categories_count": len(page.categories[:8]) if hasattr(page, 'categories') and page.categories else 0
            })

        except Exception as e:
            dpg.set_value("suggestions_status", "❌ No suggestions available")
            error("Error loading Wikipedia suggestions", context={"error": str(e)}, exc_info=True)

    def load_main_image(self, page):
        """Load and display main image with caching"""
        try:
            if hasattr(page, 'images') and page.images:
                for img_url in page.images[:3]:
                    if self.is_suitable_image(img_url):
                        dpg.add_text("🖼️ FEATURED IMAGE", color=self.GREEN)
                        _, article_width, _ = self.calculate_panel_widths()
                        max_img_width = min(400, article_width - 40)
                        self.load_and_display_image(img_url, max_img_width, 250)
                        dpg.add_spacer(height=10)
                        break
        except Exception as e:
            error("Error loading Wikipedia image", context={"error": str(e)}, exc_info=True)

    def is_suitable_image(self, img_url):
        """Check if image is suitable for display"""
        if not img_url:
            return False

        unsuitable = ['.svg', 'commons-logo', 'edit-icon', 'wikimedia', 'mediawiki']
        img_lower = img_url.lower()

        if any(pattern in img_lower for pattern in unsuitable):
            return False

        return any(fmt in img_lower for fmt in ['.jpg', '.jpeg', '.png', '.gif', '.webp'])

    def load_and_display_image(self, img_url, max_width=400, max_height=250):
        """Load and display image with caching and error handling"""
        try:
            # Check cache
            if img_url in self.loaded_images:
                texture_tag = self.loaded_images[img_url]
                if dpg.does_item_exist(texture_tag):
                    dpg.add_image(texture_tag)
                    return

            # Download and process
            response = requests.get(img_url, timeout=5, stream=True)
            response.raise_for_status()

            img = Image.open(io.BytesIO(response.content)).convert('RGBA')

            if img.width > max_width or img.height > max_height:
                img.thumbnail((max_width, max_height), Image.Resampling.LANCZOS)

            # Convert for DPG
            img_array = np.array(img)
            img_flat = img_array.flatten().astype(np.float32) / 255.0

            # Create texture
            texture_tag = f"wiki_img_texture_{len(self.loaded_images)}"

            if not dpg.does_item_exist("texture_registry"):
                dpg.add_texture_registry(tag="texture_registry")

            dpg.add_static_texture(
                width=img.width, height=img.height,
                default_value=img_flat, tag=texture_tag,
                parent="texture_registry"
            )

            self.loaded_images[img_url] = texture_tag
            dpg.add_image(texture_tag)
            dpg.add_text(f"Size: {img.width}x{img.height}", color=self.GRAY)

            debug("Wikipedia image loaded successfully", context={
                "img_url": img_url[:100],
                "size": f"{img.width}x{img.height}"
            })

        except Exception as e:
            dpg.add_text("❌ Failed to load image", color=self.RED)
            error("Error loading Wikipedia image", context={"img_url": img_url[:100], "error": str(e)}, exc_info=True)

    def process_content(self, page):
        """Process and display article content with error handling"""
        try:
            content = page.content
            paragraphs = content.split('\n\n')

            displayed_count = 0
            for paragraph in paragraphs:
                if paragraph.strip() and displayed_count < 12:
                    if paragraph.startswith('==') and paragraph.endswith('=='):
                        # Section headers
                        section = paragraph.replace('=', '').strip()
                        dpg.add_spacer(height=10)
                        dpg.add_text(f"📋 {section.upper()}", color=self.ORANGE)
                        dpg.add_separator()
                    else:
                        # Regular content
                        clean = paragraph.strip()
                        if len(clean) > 30:  # Filter out very short paragraphs
                            dpg.add_text(clean, color=self.WHITE, wrap=0)
                            dpg.add_spacer(height=8)
                            displayed_count += 1

                            if displayed_count >= 10:
                                dpg.add_text("... [Content truncated for display] ...", color=self.GRAY)
                                break
        except Exception as e:
            dpg.add_text("❌ Content not available", color=self.GRAY)
            error("Error processing Wikipedia content", context={"error": str(e)}, exc_info=True)

    # Callback methods
    def on_language_changed(self, sender, app_data):
        """Handle language change"""
        lang_map = {
            "English": "en",
            "Spanish": "es",
            "French": "fr",
            "German": "de",
            "Italian": "it"
        }

        lang_code = lang_map.get(app_data, "en")
        wikipedia.set_lang(lang_code)

        # Clear caches when language changes
        self._search_cache.clear()
        self._article_cache.clear()

        info(f"Wikipedia language changed", context={"language": app_data, "code": lang_code})

    def bookmark_article(self):
        """Bookmark current article"""
        if self.current_article:
            if self.current_article not in self.bookmarked_articles:
                self.bookmarked_articles.append(self.current_article)
                dpg.set_value("bookmark_count", f"Bookmarks: {len(self.bookmarked_articles)}")
                info(f"Article bookmarked", context={"title": self.current_article})
            else:
                info(f"Article already bookmarked", context={"title": self.current_article})

    def open_in_browser(self):
        """Open current article in web browser"""
        if hasattr(self, 'current_page_url'):
            try:
                webbrowser.open(self.current_page_url)
                info(f"Article opened in browser", context={"url": self.current_page_url})
            except Exception as e:
                error("Failed to open article in browser", context={"url": self.current_page_url, "error": str(e)},
                      exc_info=True)

    def show_search_history(self):
        """Show search history"""
        history_count = len(self.search_history)
        info("Search history requested", context={"history_count": history_count})

    def show_bookmarks(self):
        """Show bookmarked articles"""
        bookmarks_count = len(self.bookmarked_articles)
        info("Bookmarks requested", context={"bookmarks_count": bookmarks_count})

    def export_article(self):
        """Export current article"""
        if self.current_article:
            info(f"Article export requested", context={"title": self.current_article})

    def update_layout(self):
        """Update layout on window resize"""
        try:
            _, content_height = self.get_dimensions()
            results_width, article_width, suggestions_width = self.calculate_panel_widths()

            # Update panel dimensions
            if dpg.does_item_exist("results_panel"):
                dpg.configure_item("results_panel", width=results_width, height=content_height)
            if dpg.does_item_exist("article_panel"):
                dpg.configure_item("article_panel", width=article_width, height=content_height)
            if dpg.does_item_exist("suggestions_panel"):
                dpg.configure_item("suggestions_panel", width=suggestions_width, height=content_height)

        except Exception as e:
            error("Error updating Wikipedia layout", context={"error": str(e)}, exc_info=True)

    def cleanup(self):
        """Clean up Wikipedia tab resources"""
        try:
            info("Starting Wikipedia tab cleanup")

            # Clean up loaded images
            for texture_tag in self.loaded_images.values():
                if dpg.does_item_exist(texture_tag):
                    dpg.delete_item(texture_tag)
            self.loaded_images.clear()

            # Clear caches to free memory
            self._search_cache.clear()
            self._article_cache.clear()

            # Clear data
            self.search_history.clear()
            self.bookmarked_articles.clear()
            self.current_article = None

            info("Wikipedia tab cleanup completed", context={
                "images_cleared": len(self.loaded_images),
                "history_items": len(self.search_history),
                "bookmarks": len(self.bookmarked_articles)
            })
        except Exception as e:
            error("Error during Wikipedia cleanup", context={"error": str(e)}, exc_info=True)