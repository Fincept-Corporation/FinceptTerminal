// Example 5: Common mistakes and how to fix them
// Read through these — they're the gotchas you'll hit.

#include "ui/yoga_layout.h"
#include <imgui.h>

using namespace fincept::ui;

// ❌ MISTAKE 1: Forgetting to set root size
void mistake_no_root_size() {
    YogaLayout layout;
    auto root = layout.node().row();  // no width/height!
    auto child = root.child().flex_grow(1);
    layout.calculate(800, 600);       // passes w/h here but root has no constraint

    // Fix: always set root width/height
    // auto root = layout.node().row().width(800).height(600);
}

// ❌ MISTAKE 2: Reading layout BEFORE calculate()
void mistake_read_before_calculate() {
    YogaLayout layout;
    auto root = layout.node().row().width(800).height(600);
    auto child = root.child().flex_grow(1);

    // BAD: reading before calculate — all values will be 0!
    float w = child.computed_width();  // 0 !!

    layout.calculate(800, 600);
    w = child.computed_width();        // NOW it's correct
}

// ❌ MISTAKE 3: Using absolute positions without SetCursorPos
void mistake_no_cursor_pos() {
    // If you just do:
    //   ImGui::BeginChild("panel", node.size());
    // without SetCursorPos first, ImGui places it at the current cursor
    // position, which is NOT where Yoga computed it should be.

    // Fix: ALWAYS pair them:
    //   ImGui::SetCursorPos(node.pos());
    //   ImGui::BeginChild("panel", node.size());
}

// ❌ MISTAKE 4: Mixing Yoga layout with ImGui::SameLine
void mistake_mixing_sameline() {
    // Don't do this:
    //   ImGui::SetCursorPos(left.pos());
    //   ImGui::BeginChild("left", left.size());
    //   ImGui::EndChild();
    //   ImGui::SameLine();  // ❌ Yoga already computed the position!
    //   ImGui::BeginChild("right", right.size());

    // Fix: use SetCursorPos for EACH child:
    //   ImGui::SetCursorPos(left.pos());
    //   ImGui::BeginChild("left", left.size());
    //   ImGui::EndChild();
    //   ImGui::SetCursorPos(right.pos());  // ✅ Let Yoga handle positioning
    //   ImGui::BeginChild("right", right.size());
    //   ImGui::EndChild();
}

// ❌ MISTAKE 5: Using width_percent without parent having a defined width
void mistake_percent_no_parent() {
    YogaLayout layout;
    auto root = layout.node().row();   // root has NO width set
    auto child = root.child().width_percent(50);  // 50% of what?? → 0

    // Fix: root must have a defined width for percent children to work
    // auto root = layout.node().row().width(800).height(600);
}

// ❌ MISTAKE 6: Forgetting EndChild() — causes ImGui stack corruption
void mistake_unmatched_begin_end() {
    // EVERY BeginChild MUST have a matching EndChild, even if Begin returns false:
    //
    //   ImGui::SetCursorPos(node.pos());
    //   if (ImGui::BeginChild("panel", node.size())) {
    //       // draw stuff
    //   }
    //   ImGui::EndChild();  // ✅ ALWAYS called, even if BeginChild returned false
}

// ✅ CORRECT: Complete pattern for a Yoga-powered screen
void correct_pattern() {
    // This is the pattern to follow for every screen:

    // 1. ScreenFrame handles viewport math
    ScreenFrame frame("##example");
    if (frame.begin()) {

        // 2. Create Yoga layout
        YogaLayout layout;
        auto root = layout.node()
            .row()
            .width(frame.width())
            .height(frame.height())
            .padding(8).gap(4);

        auto left  = root.child().width_percent(25).min_width(200);
        auto right = root.child().flex_grow(1);

        // 3. Calculate
        layout.calculate(frame.width(), frame.height());

        // 4. Render with computed positions
        ImGui::SetCursorPos(left.pos());
        if (ImGui::BeginChild("##left", left.size())) {
            // sidebar content
        }
        ImGui::EndChild();

        ImGui::SetCursorPos(right.pos());
        if (ImGui::BeginChild("##right", right.size())) {
            // main content
        }
        ImGui::EndChild();
    }
    frame.end();  // 5. Always end the frame
}
