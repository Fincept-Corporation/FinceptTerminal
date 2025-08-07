import dearpygui.dearpygui as dpg

from fincept_terminal.Utils.Logging.logger import logger

class BaseTab:
    """Base class for all tabs in the application"""

    def __init__(self, app):
        self.app = app

    def get_label(self):
        """Return the tab label - override in subclasses"""
        return "Base Tab"

    def create_content(self):
        """Create tab content - override in subclasses"""
        dpg.add_text("Base tab content")

    def add_section_header(self, text, color=None):
        """Add a section header with consistent styling"""
        if color is None:
            color = [200, 200, 255]

        dpg.add_spacer(height=10)
        dpg.add_text(text, color=color)
        dpg.add_separator()
        dpg.add_spacer(height=10)

    def create_child_window(self, tag=None, width=300, height=200, **kwargs):
        """Create a standardized child window"""
        return dpg.child_window(
            tag=tag,
            width=width,
            height=height,
            border=True,
            **kwargs
        )

    def show_message(self, message, message_type="info"):
        """Show a message with appropriate styling"""
        colors = {
            "info": [100, 150, 255],
            "success": [100, 255, 100],
            "warning": [255, 200, 100],
            "error": [255, 100, 100]
        }
        color = colors.get(message_type, colors["info"])
        dpg.add_text(message, color=color)

    # CRITICAL: This is what your dashboard tab needs
    def get_app_sizes(self):
        """Get application layout sizes for backward compatibility"""
        if hasattr(self.app, 'usable_width') and hasattr(self.app, 'usable_height'):
            return {
                'usable_width': self.app.usable_width,
                'usable_height': self.app.usable_height,
                'left_width': getattr(self.app, 'left_width', 300),
                'center_width': getattr(self.app, 'center_width', 500),
                'right_width': getattr(self.app, 'right_width', 300),
                'top_height': getattr(self.app, 'top_height', 400),
                'bottom_height': getattr(self.app, 'bottom_height', 200),
                'cell_height': getattr(self.app, 'cell_height', 180)
            }
        else:
            # Fallback default sizes that work with your dashboard
            return {
                'usable_width': 1180,
                'usable_height': 480,
                'left_width': 290,
                'center_width': 590,
                'right_width': 290,
                'top_height': 320,
                'bottom_height': 160,
                'cell_height': 160
            }

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle component resizing - override in subclasses if needed"""
        pass

    # Authentication helper methods for tabs that need them
    def get_auth_headers(self):
        """Get authentication headers for API calls"""
        if hasattr(self.app, 'get_auth_headers'):
            return self.app.get_auth_headers()
        return {}

    def is_user_authenticated(self):
        """Check if user is authenticated"""
        if hasattr(self.app, 'is_user_authenticated'):
            return self.app.is_user_authenticated()
        return False

    def get_user_type(self):
        """Get current user type"""
        if hasattr(self.app, 'get_user_type'):
            return self.app.get_user_type()
        return None

    def get_session_data(self):
        """Get session data"""
        if hasattr(self.app, 'get_session_data'):
            return self.app.get_session_data()
        return {}

    def cleanup(self):
        """Clean up tab resources - override in subclasses if needed"""
        pass