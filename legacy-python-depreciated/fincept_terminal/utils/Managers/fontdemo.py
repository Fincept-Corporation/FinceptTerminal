import dearpygui.dearpygui as dpg
import os

dpg.create_context()

# Check if font files exist
oswald_exists = os.path.exists("oswald.ttf")
oswald2_exists = os.path.exists("oswald2222.ttf")

print(f"oswald.ttf exists: {oswald_exists}")
print(f"oswald2222.ttf exists: {oswald2_exists}")

try:
    with dpg.font_registry():
        # Load fonts if they exist
        if oswald_exists:
            oswald_font = dpg.add_font("oswald.ttf", 16)
            oswald_large = dpg.add_font("oswald.ttf", 24)

        if oswald2_exists:
            oswald2_font = dpg.add_font("oswald2222.ttf", 16)
            oswald2_large = dpg.add_font("oswald2222.ttf", 24)

    with dpg.window(label="Font Demo", width=400, height=300):
        dpg.add_text("Default Font: The quick brown fox jumps over the lazy dog")

        if oswald_exists:
            dpg.add_text("Oswald 16px: The quick brown fox jumps over the lazy dog")
            dpg.bind_item_font(dpg.last_item(), oswald_font)

            dpg.add_text("Oswald 24px: Financial Terminal Text")
            dpg.bind_item_font(dpg.last_item(), oswald_large)
        else:
            dpg.add_text("oswald.ttf NOT FOUND", color=[255, 0, 0])

        if oswald2_exists:
            dpg.add_text("Oswald2 16px: The quick brown fox jumps over the lazy dog")
            dpg.bind_item_font(dpg.last_item(), oswald2_font)

            dpg.add_text("Oswald2 24px: Financial Terminal Text")
            dpg.bind_item_font(dpg.last_item(), oswald2_large)
        else:
            dpg.add_text("oswald2222.ttf NOT FOUND", color=[255, 0, 0])

except Exception as e:
    with dpg.window(label="Font Error", width=400, height=200):
        dpg.add_text(f"Font loading error: {str(e)}", color=[255, 100, 100])
        dpg.add_text("Check if font files are in the same directory")

dpg.create_viewport(title="Font Test", width=500, height=400)
dpg.setup_dearpygui()
dpg.show_viewport()
dpg.start_dearpygui()
dpg.destroy_context()