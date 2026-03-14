// Example 3: Nested layouts — header + toolbar + content area with inner columns
// Shows how to compose vertical and horizontal layouts together.

#include "ui/yoga_layout.h"
#include <imgui.h>
#include <cstdio>

using namespace fincept::ui;

void example_nested() {
    float w = 1200, h = 800;

    YogaLayout layout;

    // Outer container: vertical stack
    auto root = layout.node()
        .column()                    // vertical: children stack top-to-bottom
        .width(w).height(h);

    // Fixed-height header (40px)
    auto header = root.child()
        .width_percent(100)
        .height(40);

    // Fixed-height toolbar (32px)
    auto toolbar = root.child()
        .width_percent(100)
        .height(32);

    // Content area: fills remaining vertical space
    auto content = root.child()
        .width_percent(100)
        .flex_grow(1);               // KEY: takes all remaining height

    layout.calculate(w, h);

    printf("=== Nested Layout (%.0f x %.0f) ===\n", w, h);
    printf("Header:  h=%.0f (fixed 40)\n", header.computed_height());
    printf("Toolbar: h=%.0f (fixed 32)\n", toolbar.computed_height());
    printf("Content: h=%.0f (fills remaining = %.0f - 40 - 32 = %.0f)\n",
           content.computed_height(), h, h - 40 - 32);

    // Now you can create a SECOND YogaLayout for the content area's inner layout:
    float content_w = content.computed_width();
    float content_h = content.computed_height();

    YogaLayout inner;
    auto inner_root = inner.node()
        .row()                       // horizontal split inside content
        .width(content_w).height(content_h)
        .gap(4);

    auto list_panel = inner_root.child()
        .width_percent(30)
        .min_width(200);

    auto detail_panel = inner_root.child()
        .flex_grow(1);

    inner.calculate(content_w, content_h);

    printf("\nInner content area (%.0f x %.0f):\n", content_w, content_h);
    printf("  List:   w=%.0f\n", list_panel.computed_width());
    printf("  Detail: w=%.0f\n", detail_panel.computed_width());

    // With ImGui, you'd set cursor positions relative to each section's origin:
    //
    // ImGui::SetCursorPos(header.pos());
    // ImGui::BeginChild("header", header.size());
    //   ImGui::Text("FINCEPT TERMINAL");
    // ImGui::EndChild();
    //
    // ImGui::SetCursorPos(toolbar.pos());
    // ImGui::BeginChild("toolbar", toolbar.size());
    //   if (ImGui::Button("Refresh")) { ... }
    // ImGui::EndChild();
    //
    // ImGui::SetCursorPos(content.pos());
    // ImGui::BeginChild("content", content.size());
    //   // Now use inner layout positions RELATIVE to this child window:
    //   ImGui::SetCursorPos(list_panel.pos());
    //   ImGui::BeginChild("list", list_panel.size());
    //     ...
    //   ImGui::EndChild();
    //   ImGui::SetCursorPos(detail_panel.pos());
    //   ImGui::BeginChild("detail", detail_panel.size());
    //     ...
    //   ImGui::EndChild();
    // ImGui::EndChild();
}
