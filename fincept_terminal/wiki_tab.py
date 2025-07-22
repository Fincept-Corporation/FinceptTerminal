# wikipedia_search_tab.py - Wikipedia Search Tab with Images
import dearpygui.dearpygui as dpg
import threading
import wikipedia
import requests
from PIL import Image
import io
import numpy as np
from fincept_terminal.Utils.base_tab import BaseTab


class WikipediaSearchTab(BaseTab):
    """Wikipedia Search Tab with Images Support"""

    def __init__(self, app):
        super().__init__(app)

        # Terminal Colors
        self.TERMINAL_ORANGE = [255, 165, 0]
        self.TERMINAL_WHITE = [255, 255, 255]
        self.TERMINAL_GREEN = [0, 200, 0]
        self.TERMINAL_BLUE = [0, 128, 255]
        self.TERMINAL_GRAY = [120, 120, 120]
        self.TERMINAL_YELLOW = [255, 255, 0]
        self.TERMINAL_RED = [255, 100, 100]

        # Search state
        self.search_results = []
        self.current_article = None
        self.loading = False
        self.loaded_images = {}  # Cache for loaded images

    def get_label(self):
        return "📚 Wikipedia"

    def create_content(self):
        """Create Wikipedia search interface"""
        # Header
        self.add_section_header("WIKIPEDIA SEARCH")

        # Search interface
        self.create_search_interface()
        dpg.add_spacer(height=10)

        # Main layout with horizontal panels
        with dpg.group(horizontal=True):
            # Left panel - Search results (400px)
            self.create_results_panel()
            dpg.add_spacer(width=10)

            # Right panel - Article content (800px)
            self.create_article_panel()

    def create_search_interface(self):
        """Create search bar and controls"""
        with dpg.group(horizontal=True):
            dpg.add_text("SEARCH:", color=self.TERMINAL_ORANGE)

            dpg.add_input_text(
                hint="Enter search term...",
                width=400,
                tag="wiki_search_input",
                callback=self.on_search_enter,
                on_enter=True
            )

            dpg.add_button(
                label="SEARCH",
                width=100,
                callback=self.execute_search
            )

        # Quick search buttons
        dpg.add_spacer(height=5)
        with dpg.group(horizontal=True):
            dpg.add_text("QUICK:", color=self.TERMINAL_YELLOW)
            quick_searches = ["Finance", "Technology", "Science", "History", "Art"]
            for term in quick_searches:
                dpg.add_button(
                    label=term,
                    width=80,
                    callback=lambda s, a, t=term: self.quick_search(t)
                )

    def create_results_panel(self):
        """Create search results panel"""
        with dpg.child_window(width=400, height=600, border=True, tag="results_panel"):
            dpg.add_text("SEARCH RESULTS", color=self.TERMINAL_ORANGE)
            dpg.add_separator()
            dpg.add_text("Enter search term above", tag="results_status", color=self.TERMINAL_GRAY)
            dpg.add_child_window(height=-1, border=False, tag="results_list")

    def create_article_panel(self):
        """Create article content panel"""
        with dpg.child_window(width=800, height=600, border=True, tag="article_panel"):
            dpg.add_text("ARTICLE CONTENT", color=self.TERMINAL_ORANGE)
            dpg.add_separator()
            dpg.add_text("Select an article from search results", tag="article_title", color=self.TERMINAL_WHITE)
            dpg.add_separator()
            dpg.add_child_window(height=-1, border=False, tag="article_content")

    def on_search_enter(self, sender, app_data):
        """Handle Enter key in search box"""
        self.execute_search()

    def quick_search(self, term):
        """Execute quick search"""
        dpg.set_value("wiki_search_input", term)
        self.execute_search()

    def execute_search(self):
        """Execute Wikipedia search"""
        if self.loading:
            return

        search_term = dpg.get_value("wiki_search_input")
        if not search_term.strip():
            return

        def search_thread():
            try:
                self.loading = True
                dpg.set_value("results_status", f"Searching for '{search_term}'...")
                dpg.delete_item("results_list", children_only=True)

                # Search Wikipedia
                wikipedia.set_lang("en")
                results = wikipedia.search(search_term, results=10)

                # Update status
                if results:
                    dpg.set_value("results_status", f"Found {len(results)} results")

                    # Display results
                    for i, result in enumerate(results):
                        self.create_result_item(result, i)
                else:
                    dpg.set_value("results_status", "No results found")

            except wikipedia.exceptions.DisambiguationError as e:
                dpg.set_value("results_status", f"Multiple matches found")
                dpg.delete_item("results_list", children_only=True)
                for i, option in enumerate(e.options[:10]):
                    self.create_result_item(option, i)

            except Exception as e:
                dpg.set_value("results_status", f"Search error: {str(e)}")
            finally:
                self.loading = False

        threading.Thread(target=search_thread, daemon=True).start()

    def create_result_item(self, title, index):
        """Create search result item"""
        with dpg.group(parent="results_list"):
            display_title = f"{index + 1}. {title}"
            if len(display_title) > 40:
                display_title = display_title[:37] + "..."

            dpg.add_button(
                label=display_title,
                callback=lambda: self.load_article(title),
                width=-1,
                height=30
            )
            dpg.add_spacer(height=3)

    def load_article(self, title):
        """Load Wikipedia article with images"""
        if self.loading:
            return

        def load_thread():
            try:
                self.loading = True
                dpg.set_value("article_title", f"Loading: {title}...")
                dpg.delete_item("article_content", children_only=True)

                with dpg.group(parent="article_content"):
                    dpg.add_text("Loading article...", color=self.TERMINAL_YELLOW)

                # Get Wikipedia page
                try:
                    page = wikipedia.page(title)
                except wikipedia.exceptions.DisambiguationError as e:
                    page = wikipedia.page(e.options[0])
                except wikipedia.exceptions.PageError:
                    search_results = wikipedia.search(title, results=1)
                    if search_results:
                        page = wikipedia.page(search_results[0])
                    else:
                        raise

                # Update title
                dpg.set_value("article_title", page.title)
                dpg.delete_item("article_content", children_only=True)

                with dpg.group(parent="article_content"):
                    # Article URL
                    dpg.add_text("URL:", color=self.TERMINAL_BLUE)
                    dpg.add_text(page.url, color=self.TERMINAL_WHITE, wrap=750)
                    dpg.add_spacer(height=15)

                    # Load and display main image
                    self.load_main_image(page)

                    # Summary
                    dpg.add_text("SUMMARY", color=self.TERMINAL_YELLOW)
                    try:
                        summary = wikipedia.summary(page.title, sentences=3)
                        dpg.add_text(summary, color=self.TERMINAL_WHITE, wrap=750)
                    except:
                        dpg.add_text("Summary not available", color=self.TERMINAL_GRAY)

                    dpg.add_spacer(height=20)

                    # Content sections
                    dpg.add_text("CONTENT", color=self.TERMINAL_YELLOW)
                    dpg.add_spacer(height=10)

                    # Process content with images
                    self.process_content_with_images(page)

                    # Load additional images
                    self.load_additional_images(page)

            except Exception as e:
                dpg.delete_item("article_content", children_only=True)
                with dpg.group(parent="article_content"):
                    dpg.add_text(f"Error loading article: {str(e)}", color=self.TERMINAL_RED)
            finally:
                self.loading = False

        threading.Thread(target=load_thread, daemon=True).start()

    def load_main_image(self, page):
        """Load and display main Wikipedia image"""
        try:
            if hasattr(page, 'images') and page.images:
                # Get the first suitable image
                main_image_url = None
                for img_url in page.images[:5]:  # Check first 5 images
                    if self.is_suitable_image(img_url):
                        main_image_url = img_url
                        break

                if main_image_url:
                    dpg.add_text("MAIN IMAGE", color=self.TERMINAL_GREEN)
                    self.load_and_display_image(main_image_url, max_width=400, max_height=300)
                    dpg.add_spacer(height=15)

        except Exception as e:
            print(f"Error loading main image: {e}")

    def load_additional_images(self, page):
        """Load additional images from the article"""
        try:
            if hasattr(page, 'images') and len(page.images) > 1:
                dpg.add_spacer(height=20)
                dpg.add_text("ADDITIONAL IMAGES", color=self.TERMINAL_GREEN)
                dpg.add_spacer(height=10)

                images_loaded = 0
                for img_url in page.images[1:6]:  # Skip first (main) image, load up to 5 more
                    if self.is_suitable_image(img_url) and images_loaded < 3:
                        self.load_and_display_image(img_url, max_width=300, max_height=200)
                        dpg.add_spacer(height=10)
                        images_loaded += 1

        except Exception as e:
            print(f"Error loading additional images: {e}")

    def is_suitable_image(self, img_url):
        """Check if image URL is suitable for display"""
        if not img_url:
            return False

        # Skip certain file types and URLs
        unsuitable_patterns = [
            '.svg', 'commons-logo', 'edit-icon', 'wikimedia',
            'mediawiki', 'template', 'portal:', 'category:'
        ]

        img_url_lower = img_url.lower()
        for pattern in unsuitable_patterns:
            if pattern in img_url_lower:
                return False

        # Prefer common image formats
        suitable_formats = ['.jpg', '.jpeg', '.png', '.gif', '.webp']
        return any(fmt in img_url_lower for fmt in suitable_formats)

    def load_and_display_image(self, img_url, max_width=400, max_height=300):
        """Load and display image from URL"""
        try:
            # Check cache first
            if img_url in self.loaded_images:
                texture_tag = self.loaded_images[img_url]
                if dpg.does_item_exist(texture_tag):
                    dpg.add_image(texture_tag)
                    return

            # Download image
            response = requests.get(img_url, timeout=10, stream=True)
            response.raise_for_status()

            # Load with PIL
            img = Image.open(io.BytesIO(response.content))
            img = img.convert('RGBA')

            # Resize if necessary
            if img.width > max_width or img.height > max_height:
                img.thumbnail((max_width, max_height), Image.Resampling.LANCZOS)

            # Convert to format for DearPyGUI
            img_array = np.array(img)
            img_flat = img_array.flatten().astype(np.float32) / 255.0

            # Create texture
            texture_tag = f"img_texture_{len(self.loaded_images)}"

            # Add texture to registry
            if not dpg.does_item_exist("texture_registry"):
                dpg.add_texture_registry(tag="texture_registry")

            dpg.add_static_texture(
                width=img.width,
                height=img.height,
                default_value=img_flat,
                tag=texture_tag,
                parent="texture_registry"
            )

            # Cache and display
            self.loaded_images[img_url] = texture_tag
            dpg.add_image(texture_tag)

            # Add image caption
            dpg.add_text(f"Image size: {img.width}x{img.height}",
                         color=self.TERMINAL_GRAY, wrap=750)

        except requests.exceptions.RequestException:
            dpg.add_text("Failed to load image (network error)", color=self.TERMINAL_RED)
        except Exception as e:
            dpg.add_text(f"Failed to load image: {str(e)}", color=self.TERMINAL_RED)

    def process_content_with_images(self, page):
        """Process and display article content"""
        try:
            content = page.content
            paragraphs = content.split('\n\n')

            paragraph_count = 0
            for paragraph in paragraphs[:15]:  # Limit to 15 paragraphs
                if paragraph.strip():
                    # Check if it's a section header
                    if paragraph.startswith('==') and paragraph.endswith('=='):
                        section_title = paragraph.replace('=', '').strip()
                        dpg.add_spacer(height=15)
                        dpg.add_text(section_title.upper(), color=self.TERMINAL_ORANGE)
                        dpg.add_separator()
                        dpg.add_spacer(height=8)
                    else:
                        # Regular paragraph
                        clean_paragraph = paragraph.strip()
                        if len(clean_paragraph) > 20:  # Skip very short paragraphs
                            dpg.add_text(clean_paragraph, color=self.TERMINAL_WHITE, wrap=750)
                            dpg.add_spacer(height=12)
                            paragraph_count += 1

                            if paragraph_count >= 10:  # Limit content
                                break

        except Exception as e:
            dpg.add_text(f"Error processing content: {str(e)}", color=self.TERMINAL_RED)

    def cleanup(self):
        """Clean up resources"""
        try:
            # Clean up textures
            for texture_tag in self.loaded_images.values():
                if dpg.does_item_exist(texture_tag):
                    dpg.delete_item(texture_tag)

            self.loaded_images.clear()
            self.search_results = []
            self.current_article = None
            self.loading = False

        except Exception as e:
            print(f"Error cleaning up Wikipedia tab: {e}")

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Resize components based on layout"""
        try:
            if dpg.does_item_exist("results_panel"):
                dpg.configure_item("results_panel", width=min(400, left_width + 100))

            if dpg.does_item_exist("article_panel"):
                dpg.configure_item("article_panel", width=center_width + right_width - 100)

        except Exception as e:
            print(f"Error resizing components: {e}")