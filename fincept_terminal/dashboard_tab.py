# -*- coding: utf-8 -*-

import dearpygui.dearpygui as dpg
from base_tab import BaseTab


class DashboardTab(BaseTab):
    """Dashboard tab with the main layout"""

    def get_label(self):
        return "📊 Dashboard"

    def create_content(self):
        """Create the dashboard layout"""
        try:
            sizes = self.get_app_sizes()

            # Top section: Left (tall) + Right section (2x2 grid)
            with dpg.group(horizontal=True):
                # Left panel - spans full height of top section
                with self.create_child_window(tag="left_panel",
                                              width=sizes['left_width'],
                                              height=sizes['top_height']):
                    self.create_left_panel()

                # Right section - 2x2 grid
                with dpg.group():
                    # Top row of right section
                    with dpg.group(horizontal=True):
                        with self.create_child_window(tag="top_center",
                                                      width=sizes['center_width'],
                                                      height=sizes['cell_height']):
                            self.create_top_center_panel()

                        with self.create_child_window(tag="top_right",
                                                      width=sizes['right_width'],
                                                      height=sizes['cell_height']):
                            self.create_top_right_panel()

                    # Bottom row of right section
                    with dpg.group(horizontal=True):
                        with self.create_child_window(tag="middle_center",
                                                      width=sizes['center_width'],
                                                      height=sizes['cell_height']):
                            self.create_middle_center_panel()

                        with self.create_child_window(tag="middle_right",
                                                      width=sizes['right_width'],
                                                      height=sizes['cell_height']):
                            self.create_middle_right_panel()

            # Bottom section: 3 equal panels
            with dpg.group(horizontal=True):
                with self.create_child_window(tag="bottom_left",
                                              width=sizes['left_width'],
                                              height=sizes['bottom_height']):
                    self.create_bottom_left_panel()

                with self.create_child_window(tag="bottom_center",
                                              width=sizes['center_width'],
                                              height=sizes['bottom_height']):
                    self.create_bottom_center_panel()

                with self.create_child_window(tag="bottom_right",
                                              width=sizes['right_width'],
                                              height=sizes['bottom_height']):
                    self.create_bottom_right_panel()
        except Exception as e:
            print(f"Dashboard layout error: {e}")
            # Fallback simple layout
            dpg.add_text("Dashboard", color=[255, 200, 100])
            dpg.add_text("Layout error occurred, using simplified view")
            dpg.add_button(label="Refresh Dashboard", callback=lambda: print("Dashboard refresh requested"))

    def create_left_panel(self):
        """Create left panel content"""
        self.add_section_header("Left Panel (Spans 2 rows)")
        dpg.add_button(label="Button 1", width=-1, callback=self.button1_callback)
        dpg.add_spacer(height=10)
        dpg.add_text("More content here...")
        dpg.add_input_text(width=-1, tag="left_input")
        dpg.add_spacer(height=20)
        dpg.add_text("Left Panel (Continued)")
        dpg.add_button(label="Button 2", width=-1, callback=self.button2_callback)
        dpg.add_spacer(height=10)
        dpg.add_slider_float(label="Value", width=-1, default_value=0.5, tag="left_slider")

    def create_top_center_panel(self):
        """Create top center panel content"""
        self.add_section_header("Large Center Panel")
        dpg.add_button(label="Center Button", width=-1, callback=self.center_button_callback)
        dpg.add_spacer(height=5)
        dpg.add_text("Center content area")
        dpg.add_input_text(width=-1, multiline=True, height=80, tag="center_text")

    def create_top_right_panel(self):
        """Create top right panel content"""
        self.add_section_header("Top Right")
        dpg.add_button(label="TR Button", width=-1, callback=self.tr_button_callback)
        dpg.add_spacer(height=5)
        dpg.add_checkbox(label="Option 1", tag="option1")
        dpg.add_checkbox(label="Option 2", tag="option2")

    def create_middle_center_panel(self):
        """Create middle center panel content"""
        self.add_section_header("Center Continued")
        dpg.add_input_text(width=-1, hint="Enter text here", tag="middle_input")
        dpg.add_button(label="Process", width=-1, callback=self.process_callback)

    def create_middle_right_panel(self):
        """Create middle right panel content"""
        self.add_section_header("Bottom Right")
        dpg.add_button(label="BR Button", width=-1, callback=self.br_button_callback)
        dpg.add_spacer(height=5)
        dpg.add_combo(["Option A", "Option B", "Option C"], width=-1, tag="combo_select")

    def create_bottom_left_panel(self):
        """Create bottom left panel content"""
        self.add_section_header("Bottom Left")
        dpg.add_button(label="BL Button", width=-1, callback=self.bl_button_callback)
        dpg.add_spacer(height=5)
        dpg.add_progress_bar(default_value=0.6, width=-1, tag="progress_bar")

    def create_bottom_center_panel(self):
        """Create bottom center panel content"""
        self.add_section_header("Bottom Center")
        dpg.add_button(label="BC Button", width=-1, callback=self.bc_button_callback)
        dpg.add_spacer(height=5)
        dpg.add_input_text(width=-1, hint="Search...", tag="search_input")

    def create_bottom_right_panel(self):
        """Create bottom right panel content"""
        self.add_section_header("Bottom Right 2")
        dpg.add_button(label="BR2 Button", width=-1, callback=self.br2_button_callback)
        dpg.add_spacer(height=5)
        dpg.add_listbox(["Item 1", "Item 2", "Item 3"], width=-1, num_items=2, tag="item_list")

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Resize dashboard components"""
        try:
            updates = [
                ("left_panel", {"width": left_width, "height": top_height}),
                ("top_center", {"width": center_width, "height": cell_height}),
                ("top_right", {"width": right_width, "height": cell_height}),
                ("middle_center", {"width": center_width, "height": cell_height}),
                ("middle_right", {"width": right_width, "height": cell_height}),
                ("bottom_left", {"width": left_width, "height": bottom_height}),
                ("bottom_center", {"width": center_width, "height": bottom_height}),
                ("bottom_right", {"width": right_width, "height": bottom_height})
            ]

            for item_id, config in updates:
                if dpg.does_item_exist(item_id):
                    dpg.configure_item(item_id, **config)
        except Exception as e:
            print(f"Dashboard resize error: {e}")

    # Callback methods
    def button1_callback(self):
        print("Button 1 clicked")
        if dpg.does_item_exist("left_input"):
            value = dpg.get_value("left_input")
            print(f"Left input value: {value}")

    def button2_callback(self):
        print("Button 2 clicked")
        if dpg.does_item_exist("left_slider"):
            value = dpg.get_value("left_slider")
            print(f"Slider value: {value}")

    def center_button_callback(self):
        print("Center button clicked")
        if dpg.does_item_exist("center_text"):
            text = dpg.get_value("center_text")
            print(f"Center text: {text}")

    def tr_button_callback(self):
        print("Top right button clicked")
        opt1 = dpg.get_value("option1") if dpg.does_item_exist("option1") else False
        opt2 = dpg.get_value("option2") if dpg.does_item_exist("option2") else False
        print(f"Options: {opt1}, {opt2}")

    def process_callback(self):
        print("Process button clicked")
        if dpg.does_item_exist("middle_input"):
            text = dpg.get_value("middle_input")
            print(f"Processing: {text}")

    def br_button_callback(self):
        print("Bottom right button clicked")
        if dpg.does_item_exist("combo_select"):
            selection = dpg.get_value("combo_select")
            print(f"Selected: {selection}")

    def bl_button_callback(self):
        print("Bottom left button clicked")
        if dpg.does_item_exist("progress_bar"):
            progress = dpg.get_value("progress_bar")
            print(f"Progress: {progress}")

    def bc_button_callback(self):
        print("Bottom center button clicked")
        if dpg.does_item_exist("search_input"):
            search = dpg.get_value("search_input")
            print(f"Search term: {search}")

    def br2_button_callback(self):
        print("Bottom right 2 button clicked")
        if dpg.does_item_exist("item_list"):
            selected = dpg.get_value("item_list")
            print(f"Selected item: {selected}")

    def cleanup(self):
        """Clean up dashboard resources"""
        # Clean up any dashboard-specific resources
        pass