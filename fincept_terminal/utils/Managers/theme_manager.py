# -*- coding: utf-8 -*-
# themes/theme_manager.py

import dearpygui.dearpygui as dpg
from typing import Dict, Any, Optional, Tuple
from fincept_terminal.utils.Logging.logger import (
    info, error, debug, warning, critical, monitor_performance, operation
)

class AutomaticThemeManager:
    """OPTIMIZED: Theme manager with lazy loading and minimal overhead"""

    def __init__(self):
        self.current_theme = "bloomberg_terminal"
        self.themes = {}
        self.theme_applied = False
        self.cleanup_performed = False
        self.themes_initialized = False
        self._initialization_attempted = False  # Add lazy loading flag
        self.themes_available = True
        self.terminal_font = None  # Add this
        info("Bloomberg Terminal theme manager initialized", module="theme")

    def _lazy_initialize(self) -> bool:
        """PERFORMANCE: Only initialize when first used"""
        if self._initialization_attempted:
            return self.themes_initialized

        self._initialization_attempted = True
        try:
            # Simple context check
            try:
                dpg.get_viewport_width()
            except Exception:
                warning("DearPyGUI context not ready", module="theme")
                return False

            # CREATE FONT REGISTRY FIRST
            self._setup_font_registry()

            info("Creating authentic Bloomberg Terminal themes", module="theme")
            self._create_bloomberg_terminal_theme()
            self._create_dark_gold_theme()
            self._create_green_terminal_theme()
            self._create_default_theme()
            self.themes_initialized = True

            theme_count = len(self.themes)
            info("Bloomberg themes creation completed", module="theme",
                 context={'themes_created': theme_count})
            return True

        except Exception as e:
            error("Error creating themes", module="theme",
                  context={'error': str(e)}, exc_info=True)
            return False

    def _setup_font_registry(self):
        """Setup font registry and load custom fonts - SAFE VERSION"""
        try:
            import os
            font_path = os.path.join(os.path.dirname(__file__), "oswald2.ttf")
            print(f"[FONT DEBUG] Looking for font at: {font_path}")
            print(f"[FONT DEBUG] Font exists: {os.path.exists(font_path)}")

            if os.path.exists(font_path):
                try:
                    with dpg.font_registry():
                        self.terminal_font = dpg.add_font(font_path, 18)
                    print(f"[FONT DEBUG] Font created with ID: {self.terminal_font}")
                    info("Oswald2 font loaded successfully", module="theme")
                except Exception as font_error:
                    print(f"[FONT DEBUG] Font creation failed: {font_error}")
                    self.terminal_font = None
            else:
                print("[FONT DEBUG] Font file not found")
                self.terminal_font = None

        except Exception as e:
            print(f"[FONT DEBUG] Font setup failed: {str(e)}")
            self.terminal_font = None

    def _ensure_themes_created(self) -> bool:
        """Create themes only when DearPyGUI context is ready"""
        return self._lazy_initialize()

    def setup_fonts(self):
        """Setup Oswald2 font for terminal - DEPRECATED, use _setup_font_registry instead"""
        # This method is now handled by _setup_font_registry during theme initialization
        # Just ensure font is applied if it exists
        if hasattr(self, 'terminal_font') and self.terminal_font:
            try:
                dpg.bind_font(self.terminal_font)
                info("Font re-applied via setup_fonts", module="theme")
                return True
            except Exception as e:
                warning(f"setup_fonts failed: {str(e)}", module="theme")
                return False
        return False

    def _create_green_terminal_theme(self):
        """Modern Green Terminal theme with #48f050 primary color"""
        try:
            if dpg.does_item_exist("green_terminal_theme"):
                dpg.delete_item("green_terminal_theme")

            with dpg.theme(tag="green_terminal_theme") as theme:
                # Core styling with proper category specification
                with dpg.theme_component(dpg.mvAll):
                    # Green Terminal Colors
                    TERMINAL_BLACK = [10, 10, 10, 255]  # Deep black background
                    TERMINAL_DARK_GRAY = [25, 30, 25, 255]  # Dark panels with subtle green tint
                    TERMINAL_MEDIUM_GRAY = [40, 45, 40, 255]  # Medium elements with green tint
                    GREEN_PRIMARY = [72, 240, 80, 255]  # #48f050
                    GREEN_HOVER = [92, 255, 100, 255]  # Brighter green for hover
                    GREEN_ACTIVE = [52, 220, 60, 255]  # Darker green for active
                    GREEN_BRIGHT = [100, 255, 110, 255]  # Bright accent green
                    TERMINAL_WHITE = [240, 255, 245, 255]  # Slightly green-tinted white
                    TERMINAL_GRAY_TEXT = [180, 220, 185, 255]  # Green-tinted secondary text
                    TERMINAL_DISABLED = [120, 140, 125, 255]  # Green-tinted disabled elements
                    TERMINAL_RED = [255, 100, 100, 255]  # Error/negative values
                    TERMINAL_YELLOW = [255, 255, 120, 255]  # Warning/attention
                    TERMINAL_BLUE = [120, 200, 255, 255]  # Information/links
                    TERMINAL_BORDER = [60, 80, 60, 255]  # Green-tinted borders
                    TERMINAL_SEPARATOR = [80, 120, 85, 255]  # Green-tinted separators

                    # Main backgrounds - Deep black with green tinting
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, TERMINAL_BLACK, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, TERMINAL_BLACK, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PopupBg, TERMINAL_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, TERMINAL_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ModalWindowDimBg, [0, 0, 0, 180], category=dpg.mvThemeCat_Core)

                    # Text colors - High contrast with green tinting
                    dpg.add_theme_color(dpg.mvThemeCol_Text, TERMINAL_WHITE, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, TERMINAL_DISABLED, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TextSelectedBg, [72, 240, 80, 100], category=dpg.mvThemeCat_Core)

                    # Button styling - Green terminal style
                    dpg.add_theme_color(dpg.mvThemeCol_Button, TERMINAL_MEDIUM_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [72, 240, 80, 120], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [72, 240, 80, 180], category=dpg.mvThemeCat_Core)

                    # Input fields - Terminal look with green accents
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, TERMINAL_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, [72, 240, 80, 60], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, [72, 240, 80, 100], category=dpg.mvThemeCat_Core)

                    # Table styling - Critical for financial data display
                    dpg.add_theme_color(dpg.mvThemeCol_Header, [72, 240, 80, 150], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, [72, 240, 80, 180], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, [72, 240, 80, 220], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableHeaderBg, TERMINAL_MEDIUM_GRAY,
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableBorderStrong, TERMINAL_SEPARATOR,
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableBorderLight, TERMINAL_BORDER, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBg, [0, 0, 0, 0], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBgAlt, [15, 25, 15, 255], category=dpg.mvThemeCat_Core)

                    # Selection highlighting
                    dpg.add_theme_color(dpg.mvThemeCol_NavHighlight, [72, 240, 80, 200], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_NavWindowingHighlight, [72, 240, 80, 150],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_NavWindowingDimBg, [60, 80, 60, 100],
                                        category=dpg.mvThemeCat_Core)

                    # Tab styling - Terminal workstation look
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, TERMINAL_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, [72, 240, 80, 120], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, [72, 240, 80, 180], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocused, [20, 30, 20, 255], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocusedActive, [35, 50, 35, 255],
                                        category=dpg.mvThemeCat_Core)

                    # Borders and separators - Green tinted
                    dpg.add_theme_color(dpg.mvThemeCol_Border, TERMINAL_BORDER, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_BorderShadow, [0, 0, 0, 0], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_Separator, TERMINAL_SEPARATOR, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SeparatorHovered, [72, 240, 80, 150],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SeparatorActive, [72, 240, 80, 200],
                                        category=dpg.mvThemeCat_Core)

                    # Scrollbars - Minimal green styling
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, TERMINAL_BLACK, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, [72, 240, 80, 120], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, [72, 240, 80, 160],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, [72, 240, 80, 200],
                                        category=dpg.mvThemeCat_Core)

                    # Form controls - Green accent
                    dpg.add_theme_color(dpg.mvThemeCol_CheckMark, GREEN_PRIMARY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrab, [72, 240, 80, 180], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, [72, 240, 80, 220],
                                        category=dpg.mvThemeCat_Core)

                    # Resize grips - Functional green
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGrip, [72, 240, 80, 80], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGripHovered, [72, 240, 80, 120],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGripActive, [72, 240, 80, 160],
                                        category=dpg.mvThemeCat_Core)

                    # Title bars - Professional appearance
                    dpg.add_theme_color(dpg.mvThemeCol_TitleBg, TERMINAL_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive, TERMINAL_MEDIUM_GRAY,
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TitleBgCollapsed, TERMINAL_DARK_GRAY,
                                        category=dpg.mvThemeCat_Core)

                    # Docking colors for professional layout
                    dpg.add_theme_color(dpg.mvThemeCol_DockingPreview, [72, 240, 80, 100], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_DockingEmptyBg, TERMINAL_BLACK, category=dpg.mvThemeCat_Core)

                    # Plot colors for financial charts
                    dpg.add_theme_color(dpg.mvThemeCol_PlotLines, GREEN_PRIMARY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PlotLinesHovered, GREEN_HOVER, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PlotHistogram, GREEN_PRIMARY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PlotHistogramHovered, GREEN_HOVER, category=dpg.mvThemeCat_Core)

                    # Terminal styling - Sharp, professional appearance
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_PopupRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ScrollbarRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_GrabRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_TabRounding, 0, category=dpg.mvThemeCat_Core)

                    # Border sizes - Visible but not overwhelming
                    dpg.add_theme_style(dpg.mvStyleVar_WindowBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_PopupBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_TabBorderSize, 1, category=dpg.mvThemeCat_Core)

                    # Spacing and padding - Professional layout
                    dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 8, 8, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_FramePadding, 6, 4, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 6, 4, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemInnerSpacing, 4, 4, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_CellPadding, 4, 2, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_IndentSpacing, 20, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ScrollbarSize, 14, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_GrabMinSize, 12, category=dpg.mvThemeCat_Core)

                    # Professional spacing and alignment
                    dpg.add_theme_style(dpg.mvStyleVar_WindowTitleAlign, 0.0, 0.5, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ButtonTextAlign, 0.5, 0.5, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_SelectableTextAlign, 0.0, 0.0, category=dpg.mvThemeCat_Core)

                    # Additional professional styling
                    dpg.add_theme_style(dpg.mvStyleVar_Alpha, 1.0, category=dpg.mvThemeCat_Core)  # Full opacity

            self.themes["green_terminal"] = theme
            info("Green Terminal theme created with primary color", module="theme",
                 context={'primary_color': '#48f050'})

        except Exception as e:
            error("Error creating Green Terminal theme", module="theme",
                  context={'error': str(e)}, exc_info=True)

    def _create_bloomberg_terminal_theme(self):
        """Authentic Bloomberg Terminal theme - Precise color matching"""
        try:
            if dpg.does_item_exist("bloomberg_terminal_theme"):
                dpg.delete_item("bloomberg_terminal_theme")

            with dpg.theme(tag="bloomberg_terminal_theme") as theme:
                # Core styling with proper category specification
                with dpg.theme_component(dpg.mvAll):
                    # Authentic Bloomberg Terminal Colors (RGB values from actual terminal)
                    BLOOMBERG_BLACK = [0, 0, 0, 255]  # Pure black background
                    BLOOMBERG_DARK_GRAY = [40, 40, 40, 255]  # Dark panels
                    BLOOMBERG_MEDIUM_GRAY = [60, 60, 60, 255]  # Medium elements
                    BLOOMBERG_ORANGE = [255, 140, 0, 255]  # Bloomberg orange (primary)
                    BLOOMBERG_ORANGE_HOVER = [255, 165, 0, 255]  # Brighter orange for hover
                    BLOOMBERG_ORANGE_ACTIVE = [255, 120, 0, 255]  # Darker orange for active
                    BLOOMBERG_WHITE = [255, 255, 255, 255]  # Pure white text
                    BLOOMBERG_GRAY_TEXT = [192, 192, 192, 255]  # Secondary text
                    BLOOMBERG_DISABLED = [128, 128, 128, 255]  # Disabled elements
                    BLOOMBERG_RED = [255, 80, 80, 255]  # Error/negative values
                    BLOOMBERG_GREEN = [0, 255, 100, 255]  # Success/positive values
                    BLOOMBERG_YELLOW = [255, 255, 100, 255]  # Warning/attention
                    BLOOMBERG_BLUE = [100, 180, 255, 255]  # Information/links
                    BLOOMBERG_BORDER = [80, 80, 80, 255]  # Subtle borders
                    BLOOMBERG_SEPARATOR = [100, 100, 100, 255]  # Visible separators

                    # Main backgrounds - Pure Bloomberg black with subtle variations
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PopupBg, BLOOMBERG_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, BLOOMBERG_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ModalWindowDimBg, [0, 0, 0, 180], category=dpg.mvThemeCat_Core)

                    # Text colors - High contrast for readability
                    dpg.add_theme_color(dpg.mvThemeCol_Text, BLOOMBERG_WHITE, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, BLOOMBERG_DISABLED, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TextSelectedBg, [255, 140, 0, 100], category=dpg.mvThemeCat_Core)

                    # Button styling - Bloomberg orange with proper states
                    dpg.add_theme_color(dpg.mvThemeCol_Button, BLOOMBERG_MEDIUM_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [255, 140, 0, 120], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [255, 140, 0, 180], category=dpg.mvThemeCat_Core)

                    # Input fields - Professional terminal look
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, BLOOMBERG_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, [255, 140, 0, 60], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, [255, 140, 0, 100], category=dpg.mvThemeCat_Core)

                    # Table styling - Critical for financial data display
                    dpg.add_theme_color(dpg.mvThemeCol_Header, [255, 140, 0, 150], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, [255, 140, 0, 180], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, [255, 140, 0, 220], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableHeaderBg, BLOOMBERG_MEDIUM_GRAY,
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableBorderStrong, BLOOMBERG_SEPARATOR,
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableBorderLight, BLOOMBERG_BORDER, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBg, [0, 0, 0, 0], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBgAlt, [20, 20, 20, 255], category=dpg.mvThemeCat_Core)

                    # Selection highlighting
                    dpg.add_theme_color(dpg.mvThemeCol_NavHighlight, [255, 140, 0, 200], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_NavWindowingHighlight, [255, 140, 0, 150],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_NavWindowingDimBg, [80, 80, 80, 100],
                                        category=dpg.mvThemeCat_Core)

                    # Tab styling - Professional workstation look
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, BLOOMBERG_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, [255, 140, 0, 120], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, [255, 140, 0, 180], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocused, [30, 30, 30, 255], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TabUnfocusedActive, [50, 50, 50, 255],
                                        category=dpg.mvThemeCat_Core)

                    # Borders and separators - Subtle but visible
                    dpg.add_theme_color(dpg.mvThemeCol_Border, BLOOMBERG_BORDER, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_BorderShadow, [0, 0, 0, 0], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_Separator, BLOOMBERG_SEPARATOR, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SeparatorHovered, [255, 140, 0, 150],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SeparatorActive, [255, 140, 0, 200],
                                        category=dpg.mvThemeCat_Core)

                    # Scrollbars - Minimal and functional
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, [255, 140, 0, 120], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, [255, 140, 0, 160],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, [255, 140, 0, 200],
                                        category=dpg.mvThemeCat_Core)

                    # Form controls - Consistent with theme
                    dpg.add_theme_color(dpg.mvThemeCol_CheckMark, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrab, [255, 140, 0, 180], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, [255, 140, 0, 220],
                                        category=dpg.mvThemeCat_Core)

                    # Resize grips - Subtle but functional
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGrip, [255, 140, 0, 80], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGripHovered, [255, 140, 0, 120],
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_ResizeGripActive, [255, 140, 0, 160],
                                        category=dpg.mvThemeCat_Core)

                    # Title bars - Professional appearance
                    dpg.add_theme_color(dpg.mvThemeCol_TitleBg, BLOOMBERG_DARK_GRAY, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive, BLOOMBERG_MEDIUM_GRAY,
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_TitleBgCollapsed, BLOOMBERG_DARK_GRAY,
                                        category=dpg.mvThemeCat_Core)

                    # Docking colors for professional layout
                    dpg.add_theme_color(dpg.mvThemeCol_DockingPreview, [255, 140, 0, 100], category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_DockingEmptyBg, BLOOMBERG_BLACK, category=dpg.mvThemeCat_Core)

                    # Plot colors for financial charts
                    dpg.add_theme_color(dpg.mvThemeCol_PlotLines, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PlotLinesHovered, BLOOMBERG_ORANGE_HOVER,
                                        category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PlotHistogram, BLOOMBERG_ORANGE, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_color(dpg.mvThemeCol_PlotHistogramHovered, BLOOMBERG_ORANGE_HOVER,
                                        category=dpg.mvThemeCat_Core)

                    # Bloomberg Terminal styling - Sharp, professional appearance
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_PopupRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ScrollbarRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_GrabRounding, 0, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_TabRounding, 0, category=dpg.mvThemeCat_Core)

                    # Border sizes - Visible but not overwhelming
                    dpg.add_theme_style(dpg.mvStyleVar_WindowBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_PopupBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameBorderSize, 1, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_TabBorderSize, 1, category=dpg.mvThemeCat_Core)

                    # Spacing and padding - Professional layout
                    dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 8, 8, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_FramePadding, 6, 4, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 6, 4, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemInnerSpacing, 4, 4, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_CellPadding, 4, 2, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_IndentSpacing, 20, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ScrollbarSize, 14, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_GrabMinSize, 12, category=dpg.mvThemeCat_Core)

                    # Professional spacing and alignment
                    dpg.add_theme_style(dpg.mvStyleVar_WindowTitleAlign, 0.0, 0.5, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_ButtonTextAlign, 0.5, 0.5, category=dpg.mvThemeCat_Core)
                    dpg.add_theme_style(dpg.mvStyleVar_SelectableTextAlign, 0.0, 0.0, category=dpg.mvThemeCat_Core)

                    # Additional professional styling
                    dpg.add_theme_style(dpg.mvStyleVar_Alpha, 1.0, category=dpg.mvThemeCat_Core)  # Full opacity

            self.themes["bloomberg_terminal"] = theme
            info("Authentic Bloomberg Terminal theme created", module="theme")

        except Exception as e:
            error("Error creating Bloomberg Terminal theme", module="theme",
                  context={'error': str(e)}, exc_info=True)

    def _create_dark_gold_theme(self):
        """Enhanced dark theme with premium gold accents"""
        try:
            if dpg.does_item_exist("dark_gold_theme"):
                dpg.delete_item("dark_gold_theme")

            with dpg.theme(tag="dark_gold_theme") as theme:
                with dpg.theme_component(dpg.mvAll):
                    # Enhanced dark color palette
                    DARK_BG = [18, 18, 18, 255]
                    DARK_PANEL = [28, 28, 28, 255]
                    DARK_ELEMENT = [38, 38, 38, 255]
                    GOLD_PRIMARY = [255, 215, 0, 255]
                    GOLD_HOVER = [255, 235, 59, 255]
                    GOLD_ACTIVE = [255, 193, 7, 255]
                    WHITE_TEXT = [255, 255, 255, 255]
                    GRAY_TEXT = [180, 180, 180, 255]
                    DISABLED_TEXT = [120, 120, 120, 255]

                    # Background colors
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, DARK_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, DARK_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_PopupBg, DARK_PANEL)
                    dpg.add_theme_color(dpg.mvThemeCol_MenuBarBg, DARK_PANEL)

                    # Text colors
                    dpg.add_theme_color(dpg.mvThemeCol_Text, WHITE_TEXT)
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, DISABLED_TEXT)
                    dpg.add_theme_color(dpg.mvThemeCol_TextSelectedBg, [255, 215, 0, 100])

                    # Interactive elements
                    dpg.add_theme_color(dpg.mvThemeCol_Button, DARK_ELEMENT)
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [255, 215, 0, 120])
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [255, 215, 0, 180])

                    dpg.add_theme_color(dpg.mvThemeCol_FrameBg, DARK_ELEMENT)
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgHovered, [255, 215, 0, 60])
                    dpg.add_theme_color(dpg.mvThemeCol_FrameBgActive, [255, 215, 0, 100])

                    # Headers and tables
                    dpg.add_theme_color(dpg.mvThemeCol_Header, [255, 215, 0, 150])
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderHovered, [255, 215, 0, 180])
                    dpg.add_theme_color(dpg.mvThemeCol_HeaderActive, [255, 215, 0, 220])
                    dpg.add_theme_color(dpg.mvThemeCol_TableHeaderBg, DARK_ELEMENT)
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBg, [0, 0, 0, 0])
                    dpg.add_theme_color(dpg.mvThemeCol_TableRowBgAlt, [25, 25, 25, 255])

                    # Tabs
                    dpg.add_theme_color(dpg.mvThemeCol_Tab, DARK_ELEMENT)
                    dpg.add_theme_color(dpg.mvThemeCol_TabHovered, [255, 215, 0, 120])
                    dpg.add_theme_color(dpg.mvThemeCol_TabActive, [255, 215, 0, 180])

                    # Borders and separators
                    dpg.add_theme_color(dpg.mvThemeCol_Border, [70, 70, 70, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_Separator, [100, 100, 100, 255])

                    # Scrollbars
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarBg, DARK_BG)
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrab, [255, 215, 0, 120])
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabHovered, [255, 215, 0, 160])
                    dpg.add_theme_color(dpg.mvThemeCol_ScrollbarGrabActive, [255, 215, 0, 200])

                    # Form controls
                    dpg.add_theme_color(dpg.mvThemeCol_CheckMark, GOLD_PRIMARY)
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrab, [255, 215, 0, 180])
                    dpg.add_theme_color(dpg.mvThemeCol_SliderGrabActive, [255, 215, 0, 220])

                    # Styling
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 3)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 3)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 3)
                    dpg.add_theme_style(dpg.mvStyleVar_TabRounding, 3)
                    dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 10, 10)
                    dpg.add_theme_style(dpg.mvStyleVar_FramePadding, 6, 4)
                    dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing, 6, 4)

            self.themes["dark_gold"] = theme
            info("Enhanced Dark Gold theme created", module="theme")

        except Exception as e:
            error("Error creating Dark Gold theme", module="theme",
                  context={'error': str(e)}, exc_info=True)

    def _create_default_theme(self):
        """Improved default theme"""
        try:
            if dpg.does_item_exist("default_theme"):
                dpg.delete_item("default_theme")

            with dpg.theme(tag="default_theme") as theme:
                with dpg.theme_component(dpg.mvAll):
                    # Clean default styling
                    dpg.add_theme_color(dpg.mvThemeCol_WindowBg, [15, 15, 15, 240])
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, [20, 20, 20, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_Text, [255, 255, 255, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_TextDisabled, [128, 128, 128, 255])

                    # Clean button styling
                    dpg.add_theme_color(dpg.mvThemeCol_Button, [60, 60, 60, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, [80, 80, 80, 255])
                    dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, [100, 100, 100, 255])

                    # Standard rounding
                    dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 5)
                    dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 3)

            self.themes["default"] = theme
            info("Improved Default theme created", module="theme")

        except Exception as e:
            error("Error creating Default theme", module="theme",
                  context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def apply_theme_globally(self, theme_name: str) -> bool:
        """Apply theme with enhanced error handling and feedback - NOW WITH LAZY LOADING"""
        try:
            # Enhanced theme mapping
            theme_map = {
                "finance_terminal": "bloomberg_terminal",
                "bloomberg_terminal": "bloomberg_terminal",
                "bloomberg": "bloomberg_terminal",
                "terminal": "bloomberg_terminal",
                "green_terminal": "green_terminal",
                "green": "green_terminal",
                "matrix": "green_terminal",
                "dark_gold": "dark_gold",
                "gold": "dark_gold",
                "default": "default",
                "standard": "default"
            }

            actual_theme = theme_map.get(theme_name.lower(), "bloomberg_terminal")

            # LAZY LOADING: Initialize themes only when needed
            if not self._lazy_initialize():
                warning("Cannot apply theme - DearPyGUI context not ready", module="theme",
                        context={'requested_theme': theme_name})
                return False

            if actual_theme not in self.themes:
                available_themes = list(self.themes.keys())
                warning("Theme not found in available themes", module="theme",
                        context={'requested_theme': actual_theme, 'available_themes': available_themes})
                return False

            # Unbind previous theme if applied
            if self.theme_applied:
                try:
                    dpg.bind_theme(0)  # Unbind current theme
                    debug("Unbound previous theme", module="theme")
                except Exception as e:
                    warning("Warning unbinding previous theme", module="theme",
                            context={'error': str(e)})

            # Apply new theme
            dpg.bind_theme(self.themes[actual_theme])
            self.current_theme = actual_theme
            self.theme_applied = True

            # CRITICAL: Apply custom font AFTER theme is bound
            if hasattr(self, 'terminal_font') and self.terminal_font:
                try:
                    dpg.bind_font(self.terminal_font)
                    print(f"[FONT DEBUG] Applied custom font after theme: {self.terminal_font}")
                except Exception as e:
                    print(f"[FONT DEBUG] Font application after theme failed: {e}")

            theme_info = self.get_theme_info()
            info("Successfully applied theme", module="theme",
                 context={'theme_name': theme_info['name'], 'description': theme_info['description']})
            return True

        except Exception as e:
            error("Critical error applying theme", module="theme",
                  context={'theme_name': theme_name, 'error': str(e)}, exc_info=True)
            return False

    def ensure_font_applied(self):
        """Ensure custom font is applied"""
        try:
            if hasattr(self, 'terminal_font') and self.terminal_font:
                dpg.bind_font(self.terminal_font)
                print(f"[FONT DEBUG] Applied font: {self.terminal_font}")
                return True
            else:
                print("[FONT DEBUG] No font to apply")
                return False
        except Exception as e:
            print(f"[FONT DEBUG] Font application failed: {e}")
            return False

    def get_available_themes(self) -> Dict[str, str]:
        """Get comprehensive list of available themes"""
        return {
            "bloomberg_terminal": "Bloomberg Terminal - Authentic black/orange professional theme",
            "green_terminal": "Green Terminal - Modern terminal with bright green (#48f050) accents",
            "dark_gold": "Dark Gold - Premium dark theme with gold accents",
            "default": "Default - Clean standard interface theme"
        }

    def get_current_theme(self) -> Dict[str, Any]:
        """Get current theme name with status"""
        return {
            "theme": self.current_theme,
            "applied": self.theme_applied,
            "initialized": self.themes_initialized
        }

    def create_theme_selector_callback(self, sender, app_data):
        """Enhanced callback for theme selector with error handling"""
        try:
            success = self.apply_theme_globally(app_data)
            if not success:
                error("Failed to apply theme from selector", module="theme",
                      context={'theme': app_data})
        except Exception as e:
            error("Theme selector callback error", module="theme",
                  context={'error': str(e)}, exc_info=True)

    def get_theme_info(self) -> Dict[str, Any]:
        """Get comprehensive information about current theme"""
        theme_info = {
            "bloomberg_terminal": {
                "name": "Bloomberg Terminal",
                "description": "Authentic Bloomberg Terminal theme with precise black background and orange accents",
                "style": "Professional financial terminal",
                "colors": {
                    "primary": "Bloomberg Orange (#FF8C00)",
                    "background": "Pure Black (#000000)",
                    "text": "White (#FFFFFF)",
                    "accent": "Orange variations"
                }
            },
            "green_terminal": {
                "name": "Green Terminal",
                "description": "Modern terminal theme with bright green primary color and dark background",
                "style": "Matrix-style financial terminal",
                "colors": {
                    "primary": "Bright Green (#48f050)",
                    "background": "Deep Black (#0A0A0A)",
                    "text": "Green-tinted White (#F0FFF5)",
                    "accent": "Green variations"
                }
            },
            "dark_gold": {
                "name": "Dark Gold",
                "description": "Premium dark theme with luxurious gold accents and enhanced readability",
                "style": "Luxury financial interface",
                "colors": {
                    "primary": "Gold (#FFD700)",
                    "background": "Dark Gray (#121212)",
                    "text": "White (#FFFFFF)",
                    "accent": "Gold variations"
                }
            },
            "default": {
                "name": "Default",
                "description": "Clean and professional standard interface theme",
                "style": "Standard modern interface",
                "colors": {
                    "primary": "Gray (#606060)",
                    "background": "Dark Gray (#0F0F0F)",
                    "text": "White (#FFFFFF)",
                    "accent": "Gray variations"
                }
            }
        }
        return theme_info.get(self.current_theme, theme_info["bloomberg_terminal"])

    def get_theme_colors(self) -> Dict[str, list]:
        """Get current theme color palette for external use"""
        if self.current_theme == "bloomberg_terminal":
            return {
                "background": [0, 0, 0, 255],
                "primary": [255, 140, 0, 255],
                "text": [255, 255, 255, 255],
                "secondary": [192, 192, 192, 255],
                "accent": [255, 165, 0, 255],
                "success": [0, 255, 100, 255],
                "warning": [255, 255, 100, 255],
                "error": [255, 80, 80, 255]
            }
        elif self.current_theme == "green_terminal":
            return {
                "background": [10, 10, 10, 255],
                "primary": [72, 240, 80, 255],
                "text": [240, 255, 245, 255],
                "secondary": [180, 220, 185, 255],
                "accent": [100, 255, 110, 255],
                "success": [72, 240, 80, 255],
                "warning": [255, 255, 120, 255],
                "error": [255, 100, 100, 255]
            }
        elif self.current_theme == "dark_gold":
            return {
                "background": [18, 18, 18, 255],
                "primary": [255, 215, 0, 255],
                "text": [255, 255, 255, 255],
                "secondary": [180, 180, 180, 255],
                "accent": [255, 235, 59, 255]
            }
        else:
            return {
                "background": [15, 15, 15, 255],
                "primary": [60, 60, 60, 255],
                "text": [255, 255, 255, 255],
                "secondary": [128, 128, 128, 255]
            }

    def cleanup(self):
        """Enhanced cleanup with better error handling"""
        if self.cleanup_performed:
            return

        try:
            info("Cleaning up Bloomberg Terminal themes", module="theme")
            self.cleanup_performed = True

            # Unbind current theme
            if self.theme_applied:
                try:
                    dpg.bind_theme(0)
                    self.theme_applied = False
                    info("Theme unbound successfully", module="theme")
                except Exception as e:
                    warning("Warning unbinding theme", module="theme",
                            context={'error': str(e)})

            # Delete all themes
            themes_deleted = 0
            for theme_name, theme in self.themes.items():
                try:
                    if dpg.does_item_exist(theme):
                        dpg.delete_item(theme)
                        themes_deleted += 1
                except Exception as e:
                    warning("Warning deleting theme", module="theme",
                            context={'theme_name': theme_name, 'error': str(e)})

            self.themes.clear()
            self.themes_initialized = False
            info("Themes cleaned up successfully", module="theme",
                 context={'themes_deleted': themes_deleted})

        except Exception as e:
            error("Theme cleanup error", module="theme",
                  context={'error': str(e)}, exc_info=True)

    def __del__(self):
        """Enhanced destructor with error handling"""
        try:
            if not self.cleanup_performed:
                self.cleanup()
        except Exception as e:
            warning("Warning in theme manager destructor", module="theme",
                    context={'error': str(e)})

    def reset_to_default(self) -> bool:
        """Reset to default theme safely"""
        try:
            return self.apply_theme_globally("default")
        except Exception as e:
            error("Error resetting to default theme", module="theme",
                  context={'error': str(e)}, exc_info=True)
            return False

    def validate_theme_integrity(self) -> Tuple[bool, str]:
        """Validate that themes are properly configured"""
        try:
            if not self.themes_initialized:
                return False, "Themes not initialized"

            required_themes = ["bloomberg_terminal", "green_terminal", "dark_gold", "default"]
            missing_themes = [t for t in required_themes if t not in self.themes]

            if missing_themes:
                return False, f"Missing themes: {missing_themes}"

            return True, "All themes validated successfully"
        except Exception as e:
            return False, f"Validation error: {e}"