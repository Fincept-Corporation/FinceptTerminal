// Example 4: Responsive layout — adapts to window size
// Shows how breakpoints + Yoga work together for responsive UIs.

#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <cstdio>

using namespace fincept::ui;

void example_responsive() {
    // Simulate different screen widths
    float test_widths[] = { 500, 700, 1000, 1400 };

    for (float w : test_widths) {
        float h = 800;
        Breakpoint bp = get_breakpoint(w);

        const char* bp_name = "?";
        switch (bp) {
            case Breakpoint::Compact:  bp_name = "Compact (<600)";  break;
            case Breakpoint::Medium:   bp_name = "Medium (600-840)"; break;
            case Breakpoint::Expanded: bp_name = "Expanded (840-1200)"; break;
            case Breakpoint::Large:    bp_name = "Large (>1200)";   break;
        }

        printf("\n=== Width: %.0f → %s ===\n", w, bp_name);

        // The three_panel_layout function handles responsiveness automatically:
        // - Compact: stacks all panels vertically
        // - Medium: two panels (collapses right)
        // - Expanded/Large: full three panels
        auto panels = three_panel_layout(w, h);

        printf("Left:   w=%.0f h=%.0f\n", panels.left_w, panels.left_h);
        printf("Center: w=%.0f h=%.0f\n", panels.center_w, panels.center_h);
        printf("Right:  w=%.0f h=%.0f\n", panels.right_w, panels.right_h);

        // Grid also adapts:
        auto grid = responsive_grid(w, 280);
        printf("Grid:   %d columns, %.0fpx each\n", grid.columns, grid.panel_w);
    }
}

// When using in a real screen, you'd do:
//
// void MyScreen::render() {
//     ScreenFrame frame("##my_screen");
//     if (frame.begin()) {
//         if (frame.is_compact()) {
//             // Single column, simplified UI
//             render_compact(frame.width(), frame.height());
//         } else {
//             // Multi-column layout
//             auto panels = three_panel_layout(frame.width(), frame.height());
//             render_panels(panels);
//         }
//     }
//     frame.end();
// }
