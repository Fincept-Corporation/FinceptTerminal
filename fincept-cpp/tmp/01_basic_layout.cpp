// Example 1: Basic Yoga layout — two columns
// This shows the raw flow: create nodes → set styles → calculate → read results
//
// You don't need to compile this file. It's a reference showing
// exactly what happens under the hood.

#include "ui/yoga_layout.h"
#include <imgui.h>
#include <cstdio>

using namespace fincept::ui;

void example_basic_two_columns() {
    // Available space (imagine this comes from ImGui viewport)
    float avail_w = 1200;
    float avail_h = 800;

    // Step 1: Create the layout and build the tree
    YogaLayout layout;

    auto root = layout.node()
        .row()                  // children laid out horizontally
        .width(avail_w)         // root fills available width
        .height(avail_h)        // root fills available height
        .padding(8)             // 8px inner padding on all sides
        .gap(4);                // 4px gap between children

    auto sidebar = root.child()
        .width_percent(25)      // 25% of parent width
        .min_width(200);        // but never less than 200px

    auto main_area = root.child()
        .flex_grow(1);          // takes all remaining space

    // Step 2: Run the layout algorithm
    layout.calculate(avail_w, avail_h);

    // Step 3: Read computed values
    printf("=== Two Column Layout (%.0f x %.0f) ===\n", avail_w, avail_h);
    printf("Sidebar:  x=%.0f  y=%.0f  w=%.0f  h=%.0f\n",
           sidebar.rect().x, sidebar.rect().y,
           sidebar.rect().w, sidebar.rect().h);
    printf("Main:     x=%.0f  y=%.0f  w=%.0f  h=%.0f\n",
           main_area.rect().x, main_area.rect().y,
           main_area.rect().w, main_area.rect().h);

    // Expected output (approximately):
    //   Sidebar:  x=8   y=8   w=296  h=784    (25% of 1200 = 300, minus padding/gap)
    //   Main:     x=308 y=8   w=884  h=784    (remaining space)

    // Step 4: Use with ImGui (in your render function)
    // ImGui::SetCursorPos(sidebar.pos());
    // ImGui::BeginChild("sidebar", sidebar.size());
    //   ImGui::Text("Sidebar content here");
    // ImGui::EndChild();
    //
    // ImGui::SetCursorPos(main_area.pos());
    // ImGui::BeginChild("main", main_area.size());
    //   ImGui::Text("Main content here");
    // ImGui::EndChild();

    // Layout is automatically freed when `layout` goes out of scope (RAII)
}
