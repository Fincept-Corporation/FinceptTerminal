# themes/theme_manager.py - Professional Finance Theme Manager
import dearpygui.dearpygui as dpg


class AutomaticThemeManager:
    """
    Professional finance theme manager inspired by Bloomberg Terminal.
    Clean, readable, and suitable for production environments.
    """

    def __init__(self):
        self.current_theme = "bloomberg_professional"
        self.themes = {}
        self.theme_applied = False
        self.cleanup_performed = False
        self.themes_initialized = False
        print("✅ Professional theme manager initialized")

    def _ensure_themes_created(self):
        """Create themes only when DearPyGUI context is ready"""
        if self.themes_initialized:
            return True

        try:
            # Simple context check
            try:
                dpg.get_viewport_width()
            except:
                print("⚠️ DearPyGUI context not ready")
                return False

            print("🎨 Creating professional themes...")
            self._create_bloomberg_professional_theme()
            self._create_financial_dark_theme()
            self._create_clean_light_theme()
            self.themes_initialized = True
            print(f"✅ {len(self.themes)} professional themes created")
            return True

        except Exception as e:
            print(f"❌ Error creating themes: {e}")
            return False

    def _create_bloomberg_professional_theme(self):
        """Bloomberg-inspired professional theme - Dark blue with orange accents"""
        try:
            if dpg.does_item_exist("bloomberg_theme"):
                dpg.delete_item("bloomberg_theme")

            with dpg.theme(tag="bloomberg_theme") as theme:
                with dpg.theme_component(dpg.mvAll):
                    # Bloomberg-style dark blue backgrounds
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (20, 30, 45, 255))  # Main background
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (25, 35, 50, 255))  # Panel background
                    dpg.add_theme_color(dpg.mvThemeCol_PopupBg, (30, 40, 55, 255))  # Popup background
                    dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, (15, 25, 40, 255))  # Menu bar

                    # Professional white text for excellent readability
                    dpg.add_theme_color(dpg.mvThemeCol_Text, (255, 255, 255, 255))  # White text
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, (150, 150, 150, 255))  # Gray disabled

                    # Bloomberg orange accent buttons
                    dpg.add_theme_color(dpg.mvThemeCol_Button, (45, 55, 70, 255))  # Button background
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (255, 120, 0, 100))  # Orange hover
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (255, 120, 0, 150))  # Orange active

                    # Input fields with subtle highlights
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (35, 45, 60, 255))  # Input background
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, (255, 120, 0, 50))  # Orange hover
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, (255, 120, 0, 80))  # Orange active

                    # Table headers with professional styling
                    dpg.add_theme_color(dpg.mvThemeCol_Header, (255, 120, 0, 120))  # Orange header
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, (255, 120, 0, 160))  # Orange hover
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, (255, 120, 0, 200))  # Orange active
                    dpg.add_theme_color(dpg.mvThemeCol_TableHeaderBg, (40, 50, 65, 255))  # Header background
                    dpg.add_theme_color(dpg.mvThemeCol_TableBorderStrong, (80, 90, 105, 255))  # Strong borders
                    dpg.add_theme_color(dpg.mvThemeCol_TableBorderLight, (60, 70, 85, 255))  # Light borders
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBg, (0, 0, 0, 0))  # Transparent
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBgAlt, (255, 120, 0, 20))  # Subtle orange

                    # Professional tab styling
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, (35, 45, 60, 255))  # Tab background
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, (255, 120, 0, 100))  # Orange hover
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, (255, 120, 0, 140))  # Orange active
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocused, (25, 35, 50, 255))  # Unfocused
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocusedActive, (40, 50, 65, 255))  # Unfocused active

                    # Subtle borders for clean separation
                    dpg.add_theme_color(dpg.mvThemeCol_Border, (80, 90, 105, 255))  # Border color
                    dpg.add_theme_color(dpg.mvThemeCol_Separator, (80, 90, 105, 255))  # Separator color

                    # Professional scrollbars
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, (25, 35, 50, 255))  # Scrollbar background
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, (255, 120, 0, 150))  # Orange grab
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, (255, 120, 0, 180))  # Orange hover
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, (255, 120, 0, 220))  # Orange active

                    # Form controls with orange accents
                    dpg.add_theme_color(dpg.mvThemeCol_CheckMark, (255, 120, 0, 255))  # Orange checkmark
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrab, (255, 120, 0, 200))  # Orange slider
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, (255, 120, 0, 255))  # Orange active

                    # Resize grips
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGrip, (255, 120, 0, 100))  # Orange grip
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGripHovered, (255, 120, 0, 150))  # Orange hover
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGripActive, (255, 120, 0, 200))  # Orange active

                    # Clean professional styling with minimal rounding
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 4)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 3)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 2)
                    dpg.add_theme_style(dpg.mvStyleVar_TabRounding, 2)
                    dpg.add_theme_style(dpg.mvStyleVar_ScrollbarRounding, 2)
                    dpg.add_theme_style(dpg.mvStyleVar_WindowBorderSize, 1)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildBorderSize, 1)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameBorderSize, 1)
                    dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 12, 12)
                    dpg.add_theme_style(dpg.mvStyleVar_FramePadding, 8, 6)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 10, 6)

            self.themes["bloomberg_professional"] = theme

        except Exception as e:
            print(f"❌ Error creating Bloomberg theme: {e}")

    def _create_financial_dark_theme(self):
        """Financial dark theme - Charcoal with blue accents"""
        try:
            if dpg.does_item_exist("financial_dark_theme"):
                dpg.delete_item("financial_dark_theme")

            with dpg.theme(tag="financial_dark_theme") as theme:
                with dpg.theme_component(dpg.mvAll):
                    # Sophisticated charcoal backgrounds
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (32, 32, 32, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (40, 40, 40, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_PopupBg, (48, 48, 48, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, (28, 28, 28, 255))

                    # High-contrast white text
                    dpg.add_theme_color(dpg.mvThemeCol_Text, (255, 255, 255, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, (160, 160, 160, 255))

                    # Professional blue accent buttons
                    dpg.add_theme_color(dpg.mvThemeCol_Button, (55, 55, 55, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (64, 128, 255, 100))
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (64, 128, 255, 150))

                    # Input fields
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (48, 48, 48, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, (64, 128, 255, 50))
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, (64, 128, 255, 80))

                    # Headers
                    dpg.add_theme_color(dpg.mvThemeCol_Header, (64, 128, 255, 120))
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, (64, 128, 255, 160))
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, (64, 128, 255, 200))

                    # Tabs
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, (48, 48, 48, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, (64, 128, 255, 100))
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, (64, 128, 255, 140))
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocused, (40, 40, 40, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocusedActive, (55, 55, 55, 255))

                    # Borders
                    dpg.add_theme_color(dpg.mvThemeCol_Border, (80, 80, 80, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_Separator, (80, 80, 80, 255))

                    # Scrollbars
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, (32, 32, 32, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, (64, 128, 255, 150))
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, (64, 128, 255, 180))
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, (64, 128, 255, 220))

                    # Form controls
                    dpg.add_theme_color(dpg.mvThemeCol_CheckMark, (64, 128, 255, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrab, (64, 128, 255, 200))
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, (64, 128, 255, 255))

                    # Professional styling
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 6)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 4)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 4)
                    dpg.add_theme_style(dpg.mvStyleVar_TabRounding, 3)

            self.themes["financial_dark"] = theme

        except Exception as e:
            print(f"❌ Error creating Financial Dark theme: {e}")

    def _create_clean_light_theme(self):
        """Clean light theme for daytime trading"""
        try:
            if dpg.does_item_exist("clean_light_theme"):
                dpg.delete_item("clean_light_theme")

            with dpg.theme(tag="clean_light_theme") as theme:
                with dpg.theme_component(dpg.mvAll):
                    # Clean light backgrounds
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (248, 248, 248, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, (255, 255, 255, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_PopupBg, (250, 250, 250, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, (240, 240, 240, 255))

                    # Dark text for excellent readability
                    dpg.add_theme_color(dpg.mvThemeCol_Text, (45, 45, 45, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, (140, 140, 140, 255))

                    # Professional blue buttons
                    dpg.add_theme_color(dpg.mvThemeCol_Button, (235, 235, 235, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (0, 120, 215, 100))
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (0, 120, 215, 150))

                    # Input fields
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (255, 255, 255, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, (0, 120, 215, 40))
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, (0, 120, 215, 60))

                    # Headers
                    dpg.add_theme_color(dpg.mvThemeCol_Header, (0, 120, 215, 100))
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, (0, 120, 215, 140))
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, (0, 120, 215, 180))

                    # Tabs
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, (230, 230, 230, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, (0, 120, 215, 80))
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, (0, 120, 215, 120))
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocused, (240, 240, 240, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocusedActive, (220, 220, 220, 255))

                    # Subtle borders
                    dpg.add_theme_color(dpg.mvThemeCol_Border, (200, 200, 200, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_Separator, (220, 220, 220, 255))

                    # Scrollbars
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, (245, 245, 245, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, (180, 180, 180, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, (0, 120, 215, 150))
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, (0, 120, 215, 200))

                    # Form controls
                    dpg.add_theme_color(dpg.mvThemeCol_CheckMark, (0, 120, 215, 255))
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrab, (0, 120, 215, 200))
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, (0, 120, 215, 255))

                    # Clean modern styling
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 8)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 6)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 6)
                    dpg.add_theme_style(dpg.mvStyleVar_TabRounding, 4)

            self.themes["clean_light"] = theme

        except Exception as e:
            print(f"❌ Error creating Clean Light theme: {e}")

    def apply_theme_globally(self, theme_name):
        """Apply theme with lazy loading"""
        try:
            if not self._ensure_themes_created():
                print(f"⚠️ Cannot apply theme '{theme_name}' - themes not ready")
                return False

            if theme_name not in self.themes:
                print(f"❌ Theme '{theme_name}' not found!")
                return False

            if self.theme_applied:
                try:
                    dpg.bind_theme(0)
                except:
                    pass

            dpg.bind_theme(self.themes[theme_name])
            self.current_theme = theme_name
            self.theme_applied = True

            theme_info = self.get_theme_info()
            print(f"✅ Applied '{theme_info['name']}' theme")
            return True

        except Exception as e:
            print(f"❌ Error applying theme '{theme_name}': {e}")
            return False

    def get_available_themes(self):
        """Get list of available theme names"""
        return ["bloomberg_professional", "financial_dark", "clean_light"]

    def get_current_theme(self):
        """Get current theme name"""
        return self.current_theme

    def create_theme_selector_callback(self, sender, app_data):
        """Safe callback for theme selector"""
        try:
            self.apply_theme_globally(app_data)
        except Exception as e:
            print(f"❌ Theme selector error: {e}")

    def get_theme_info(self):
        """Get information about current theme"""
        theme_info = {
            "bloomberg_professional": {
                "name": "💼 Bloomberg Professional",
                "description": "Professional dark blue theme with orange accents",
                "style": "Bloomberg Terminal inspired"
            },
            "financial_dark": {
                "name": "📊 Financial Dark",
                "description": "Sophisticated charcoal theme with blue accents",
                "style": "Modern financial dashboard"
            },
            "clean_light": {
                "name": "☀️ Clean Light",
                "description": "Clean white theme for daytime trading",
                "style": "Professional light mode"
            }
        }
        return theme_info.get(self.current_theme, theme_info["bloomberg_professional"])

    def cleanup(self):
        """Clean up theme resources"""
        if self.cleanup_performed:
            return

        try:
            print("🧹 Cleaning up themes...")
            self.cleanup_performed = True

            if self.theme_applied:
                try:
                    dpg.bind_theme(0)
                    self.theme_applied = False
                except:
                    pass

            for theme_name, theme in self.themes.items():
                try:
                    if dpg.does_item_exist(theme):
                        dpg.delete_item(theme)
                except:
                    pass

            self.themes.clear()
            self.themes_initialized = False
            print("✅ Theme cleanup complete")

        except Exception as e:
            print(f"❌ Theme cleanup error: {e}")

    def __del__(self):
        """Destructor to ensure cleanup"""
        try:
            if not self.cleanup_performed:
                self.cleanup()
        except:
            pass