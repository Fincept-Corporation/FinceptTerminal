// Example 2: Three-panel layout (sidebar | content | detail)
// This is the most common pattern in the terminal for research/analysis screens.

#include "ui/yoga_layout.h"
#include <imgui.h>
#include <cstdio>

using namespace fincept::ui;

void example_three_panel() {
    float w = 1400, h = 800;

    YogaLayout layout;
    auto root = layout.node()
        .row()                      // horizontal layout
        .width(w).height(h)
        .padding(4)
        .gap(4);

    // Left sidebar: 18% width, minimum 160px
    auto left = root.child()
        .width_percent(18)
        .min_width(160);

    // Center: flexible, takes remaining space
    auto center = root.child()
        .flex_grow(1)
        .min_width(200);

    // Right detail panel: 24% width, minimum 200px
    auto right = root.child()
        .width_percent(24)
        .min_width(200);

    layout.calculate(w, h);

    printf("=== Three Panel (%.0f x %.0f) ===\n", w, h);
    printf("Left:   w=%.0f h=%.0f\n", left.computed_width(), left.computed_height());
    printf("Center: w=%.0f h=%.0f\n", center.computed_width(), center.computed_height());
    printf("Right:  w=%.0f h=%.0f\n", right.computed_width(), right.computed_height());

    // With ImGui — the pattern every screen follows:
    //
    // ImGui::SetCursorPos(left.pos());
    // if (ImGui::BeginChild("##left", left.size())) {
    //     // watchlist, navigation, etc.
    // }
    // ImGui::EndChild();
    //
    // ImGui::SetCursorPos(center.pos());
    // if (ImGui::BeginChild("##center", center.size())) {
    //     // main content: charts, tables, etc.
    // }
    // ImGui::EndChild();
    //
    // ImGui::SetCursorPos(right.pos());
    // if (ImGui::BeginChild("##right", right.size())) {
    //     // detail panel: stock info, news feed, etc.
    // }
    // ImGui::EndChild();
}
