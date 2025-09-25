import dearpygui.dearpygui as dpg
import random

# Create context and viewport
dpg.create_context()
dpg.create_viewport(title="Infinite Scroll Grid Layout", width=1000, height=700)

# Grid configuration
COLS = 4  # Number of columns
ROW_HEIGHT = 80  # Fixed row height
MARGIN = 2  # Margin between grid items

# Infinite scroll variables
current_batch = 0
batch_size = 20
loaded_items = []


def generate_random_container():
    """Generate a random container configuration"""
    labels = [
        "Stock Chart", "Order Book", "News Feed", "Portfolio", "Watchlist",
        "Market Data", "Analytics", "Trading Panel", "Risk Meter", "Balance",
        "Economic Calendar", "Alerts", "Research", "Screener", "Heat Map",
        "Sentiment", "Volatility", "Options Flow", "Crypto", "Forex",
        "Bonds", "Commodities", "Earnings", "IPO Tracker", "Dividends"
    ]

    col_span = random.choice([1, 1, 1, 2, 2, 3, 4])  # Weighted toward smaller spans
    row_span = random.choice([1, 1, 1, 2, 2])  # Mostly 1-2 rows
    label = random.choice(labels) + f" #{current_batch * batch_size + len(loaded_items) + 1}"

    # Generate random color
    color = [
        random.randint(80, 220),
        random.randint(80, 220),
        random.randint(80, 220),
        255
    ]

    return (col_span, row_span, label, color)


def load_more_items():
    """Load more items for infinite scroll"""
    global current_batch, loaded_items

    new_items = []
    for _ in range(batch_size):
        new_items.append(generate_random_container())

    loaded_items.extend(new_items)
    current_batch += 1
    print(f"Loaded batch {current_batch}, total items: {len(loaded_items)}")


def find_next_available_position(occupied_grid, col_span, row_span, start_row=0):
    """Find the next available position in the grid that can fit the item"""
    row = start_row

    while True:
        for col in range(COLS - col_span + 1):  # Only check positions where item can fit
            # Check if this position and all required cells are free
            can_place = True
            for r in range(row, row + row_span):
                for c in range(col, col + col_span):
                    if occupied_grid.get((r, c), False):
                        can_place = False
                        break
                if not can_place:
                    break

            if can_place:
                return row, col

        row += 1  # Move to next row if no space found in current row


def mark_cells_occupied(occupied_grid, row, col, row_span, col_span):
    """Mark cells as occupied in the grid"""
    for r in range(row, row + row_span):
        for c in range(col, col + col_span):
            occupied_grid[(r, c)] = True


def check_scroll_position():
    """Check if we need to load more content based on scroll position"""
    if not dpg.does_item_exist("grid_container"):
        return

    # Simple check - if we have fewer than 100 items, keep loading
    if len(loaded_items) < 100:
        # Get scroll position to determine if we need more content
        try:
            scroll_max_y = dpg.get_y_scroll_max("grid_container")
            scroll_y = dpg.get_y_scroll("grid_container")

            # Load more if we're near the bottom (within 2 screens of content)
            viewport_height = dpg.get_viewport_height()
            if scroll_max_y - scroll_y < viewport_height * 2:
                load_more_items()
                create_grid_layout()
        except:
            # Fallback: load more based on item count
            estimated_rows = (len(loaded_items) // COLS + 1)
            viewport_height = dpg.get_viewport_height()
            if estimated_rows * ROW_HEIGHT < viewport_height * 3:
                load_more_items()
                create_grid_layout()


def create_grid_layout():
    """Create a responsive grid layout with proper positioning"""

    # Clear existing items
    if dpg.does_item_exist("grid_container"):
        dpg.delete_item("grid_container")

    viewport_width = dpg.get_viewport_width()
    viewport_height = dpg.get_viewport_height()

    # Calculate column width to fill screen width properly
    total_margin_width = MARGIN * (COLS + 1)  # Margins between columns + edges
    available_width = viewport_width - total_margin_width
    col_width = available_width // COLS

    # Create scrollable container
    with dpg.child_window(tag="grid_container", parent="main_window",
                          width=viewport_width, height=viewport_height, pos=[0, 0],
                          horizontal_scrollbar=False,
                          no_scrollbar=False,
                          border=False):

        # Track occupied cells in grid
        occupied_grid = {}
        max_row_used = 0

        for i, (col_span, row_span, label, color) in enumerate(loaded_items):
            # Ensure spans don't exceed grid bounds
            col_span = min(col_span, COLS)

            # Find next available position
            row, col = find_next_available_position(occupied_grid, col_span, row_span)

            # Mark cells as occupied
            mark_cells_occupied(occupied_grid, row, col, row_span, col_span)

            # Track maximum row used
            max_row_used = max(max_row_used, row + row_span - 1)

            # Calculate position and size
            x_pos = MARGIN + col * (col_width + MARGIN)
            y_pos = MARGIN + row * (ROW_HEIGHT + MARGIN)

            box_width = col_span * col_width + (col_span - 1) * MARGIN
            box_height = row_span * ROW_HEIGHT + (row_span - 1) * MARGIN

            # Create themed box
            with dpg.theme() as box_theme:
                with dpg.theme_component(dpg.mvChildWindow):
                    dpg.add_theme_color(dpg.mvThemeCol_ChildBg, color)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 3)
                    dpg.add_theme_style(dpg.mvStyleVar_ChildBorderSize, 1)
                    dpg.add_theme_color(dpg.mvThemeCol_Border, [255, 255, 255, 100])

            # Create box container
            with dpg.child_window(
                    width=box_width,
                    height=box_height,
                    pos=[x_pos, y_pos],
                    border=True,
                    tag=f"container_{i}"
            ) as box:
                dpg.bind_item_theme(box, box_theme)

                # Add content with proper positioning
                dpg.add_text(label, pos=[8, 8])
                dpg.add_text(f"{col_span}x{row_span} | Item {i + 1}", pos=[8, 28])

                # Add interactive elements based on available space
                if box_height > 60:
                    content_y = 48
                    content_width = box_width - 16  # Account for margins

                    if content_width > 100:  # Only add complex elements if there's space
                        if "Chart" in label:
                            value = (i * 0.1) % 1.0
                            dpg.add_progress_bar(default_value=value,
                                                 pos=[8, content_y],
                                                 width=min(content_width, 200))
                        elif "Order" in label or "Trading" in label:
                            dpg.add_input_float(label="", default_value=100.0 + i,
                                                pos=[8, content_y],
                                                width=min(content_width, 120))
                        elif "News" in label or "Feed" in label:
                            if box_height > 100:  # Only add listbox if enough height
                                items = [f"Item {j + 1}" for j in range(3)]
                                list_height = min(3, (box_height - content_y - 16) // 20)
                                dpg.add_listbox(items, pos=[8, content_y],
                                                width=min(content_width, 150),
                                                num_items=max(1, list_height))
                        else:
                            dpg.add_button(label=f"Action", pos=[8, content_y],
                                           width=min(content_width, 100))

        # Add "Load More" section at the bottom
        if loaded_items:
            load_more_y = MARGIN + (max_row_used + 2) * (ROW_HEIGHT + MARGIN)

            with dpg.child_window(
                    width=viewport_width - 2 * MARGIN,
                    height=60,
                    pos=[MARGIN, load_more_y],
                    border=True
            ) as load_more_container:
                with dpg.theme() as load_more_theme:
                    with dpg.theme_component(dpg.mvChildWindow):
                        dpg.add_theme_color(dpg.mvThemeCol_ChildBg, [60, 60, 60, 255])
                        dpg.add_theme_style(dpg.mvStyleVar_ChildRounding, 3)

                dpg.bind_item_theme(load_more_container, load_more_theme)

                dpg.add_text(f"Loaded {len(loaded_items)} items - Scroll down or click to load more",
                             pos=[10, 10])
                dpg.add_button(label="Load More Now", pos=[10, 30],
                               callback=lambda: [load_more_items(), create_grid_layout()])


def resize_callback():
    """Recreate grid and resize main window when viewport is resized"""
    viewport_width = dpg.get_viewport_width()
    viewport_height = dpg.get_viewport_height()

    dpg.set_item_width("main_window", viewport_width)
    dpg.set_item_height("main_window", viewport_height)
    dpg.set_item_pos("main_window", [0, 0])

    # Recreate grid layout with new dimensions
    create_grid_layout()


# Main window
with dpg.window(
        label="Infinite Scroll Grid",
        tag="main_window",
        no_move=True,
        no_resize=True,
        no_collapse=True,
        no_title_bar=True
):
    pass

# Create dark theme
with dpg.theme() as main_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_WindowBg, [30, 30, 30, 255])
        dpg.add_theme_color(dpg.mvThemeCol_Text, [255, 255, 255, 255])

dpg.bind_theme(main_theme)

# Load initial content
load_more_items()  # Load first batch
load_more_items()  # Load second batch for initial content

# Set up viewport and callbacks
dpg.setup_dearpygui()
dpg.show_viewport()
dpg.set_viewport_resize_callback(resize_callback)

# Initial window sizing
initial_width = dpg.get_viewport_width()
initial_height = dpg.get_viewport_height()
dpg.set_item_width("main_window", initial_width)
dpg.set_item_height("main_window", initial_height)
dpg.set_item_pos("main_window", [0, 0])

# Create initial grid
create_grid_layout()

# Main loop with infinite scroll checking
frame_count = 0
while dpg.is_dearpygui_running():
    frame_count += 1

    # Check for more content loading every 60 frames (about once per second)
    if frame_count % 60 == 0:
        check_scroll_position()

    dpg.render_dearpygui_frame()

# Cleanup
dpg.destroy_context()