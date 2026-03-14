#pragma once
// Production-ready Yoga+ImGui integration helpers
// Provides responsive screen bootstrapping, breakpoint system,
// and common layout patterns. Every screen should use these
// instead of manual viewport math.
//
// Key concepts:
//   ScreenFrame — Computes work area, opens the ImGui window, provides Yoga root
//   Breakpoint  — Responsive width categories (compact/medium/expanded/large)
//   Grid        — Responsive column grid with flex-wrap

#include "ui/yoga_layout.h"
#include "ui/theme.h"
#include <imgui.h>
#include <algorithm>

namespace fincept::ui {

// ============================================================================
// Responsive breakpoints (matches Material Design 3 window classes)
// ============================================================================
enum class Breakpoint { Compact, Medium, Expanded, Large };

inline Breakpoint get_breakpoint(float width) {
    if (width < 600)  return Breakpoint::Compact;   // phone / very narrow
    if (width < 840)  return Breakpoint::Medium;     // small tablet / half-screen
    if (width < 1200) return Breakpoint::Expanded;   // normal desktop
    return Breakpoint::Large;                         // wide desktop / ultrawide
}

inline Breakpoint current_breakpoint() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    return get_breakpoint(vp->WorkSize.x);
}

// How many grid columns for a given breakpoint
inline int grid_columns(Breakpoint bp) {
    switch (bp) {
        case Breakpoint::Compact:  return 1;
        case Breakpoint::Medium:   return 2;
        case Breakpoint::Expanded: return 3;
        case Breakpoint::Large:    return 4;
    }
    return 3;
}

// ============================================================================
// ScreenFrame — replaces ALL manual viewport math
//
// Usage:
//   ScreenFrame frame("##my_screen");
//   if (frame.begin()) {
//       // frame.width(), frame.height() = available content area
//       // frame.breakpoint() = current responsive class
//       // frame.root() = Yoga NodeRef for the work area
//       // ... render content ...
//   }
//   frame.end();
// ============================================================================
class ScreenFrame {
public:
    explicit ScreenFrame(const char* id,
                         ImVec2 padding = ImVec2(0, 0),
                         ImVec4 bg_color = theme::colors::BG_DARKEST)
        : id_(id), padding_(padding), bg_color_(bg_color) {}

    // Computes work area, opens ImGui window. Returns false if collapsed.
    bool begin() {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float tab_bar_h = ImGui::GetFrameHeight() + 4;
        float footer_h  = ImGui::GetFrameHeight();

        origin_ = ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_bar_h);
        size_   = ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_bar_h - footer_h);
        bp_     = get_breakpoint(size_.x);

        ImGui::SetNextWindowPos(origin_);
        ImGui::SetNextWindowSize(size_);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding_);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color_);
        style_pushed_ = true;

        bool open = ImGui::Begin(id_, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus);

        content_size_ = ImGui::GetContentRegionAvail();
        return open;
    }

    void end() {
        ImGui::End();
        if (style_pushed_) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            style_pushed_ = false;
        }
    }

    // Accessors
    float width()  const { return content_size_.x; }
    float height() const { return content_size_.y; }
    ImVec2 content_size() const { return content_size_; }
    ImVec2 origin() const { return origin_; }
    Breakpoint breakpoint() const { return bp_; }
    bool is_compact()  const { return bp_ == Breakpoint::Compact; }
    bool is_medium()   const { return bp_ == Breakpoint::Medium; }
    bool is_expanded() const { return bp_ == Breakpoint::Expanded; }
    bool is_large()    const { return bp_ == Breakpoint::Large; }

private:
    const char* id_;
    ImVec2 padding_;
    ImVec4 bg_color_;
    ImVec2 origin_{};
    ImVec2 size_{};
    ImVec2 content_size_{};
    Breakpoint bp_ = Breakpoint::Expanded;
    bool style_pushed_ = false;
};

// ============================================================================
// CenteredFrame — for auth screens (login, register, etc.)
// Uses Yoga justify:center + align:center for perfect centering on any screen.
// ============================================================================
class CenteredFrame {
public:
    explicit CenteredFrame(const char* id, float max_width = 460.0f,
                           ImVec4 bg_color = theme::colors::BG_PANEL)
        : id_(id), max_width_(max_width), bg_color_(bg_color) {}

    bool begin() {
        ImVec2 display = ImGui::GetIO().DisplaySize;
        Breakpoint bp = get_breakpoint(display.x);

        // Responsive width: use percentage on small screens, fixed on large
        float panel_w;
        switch (bp) {
            case Breakpoint::Compact:  panel_w = display.x * 0.94f; break;
            case Breakpoint::Medium:   panel_w = display.x * 0.70f; break;
            default:                   panel_w = max_width_; break;
        }
        panel_w = std::min(panel_w, max_width_);

        // Yoga: center the card on screen
        YogaLayout layout;
        auto root = layout.node()
            .row()
            .width(display.x)
            .height(display.y)
            .justify(YGJustifyCenter)
            .align_items(YGAlignCenter);

        auto card = root.child()
            .width(panel_w)
            .height(last_height_);

        layout.calculate(display.x, display.y);

        float card_x = card.rect().x;
        float card_y = std::max(20.0f, card.rect().y);

        ImGui::SetNextWindowPos(ImVec2(card_x, card_y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panel_w, 0), ImGuiCond_Always);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color_);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(36, 32));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        style_pushed_ = true;

        return ImGui::Begin(id_, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize);
    }

    void end() {
        // Capture window height for next frame's vertical centering
        last_height_ = ImGui::GetWindowHeight();
        ImGui::End();
        if (style_pushed_) {
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            style_pushed_ = false;
        }
    }

private:
    const char* id_;
    float max_width_;
    ImVec4 bg_color_;
    bool style_pushed_ = false;
    float last_height_ = 400.0f;  // initial estimate for first frame
};

// ============================================================================
// Three-panel layout (sidebar | main | detail) via Yoga
// Returns computed widths; caller uses ImGui::SameLine between panels
// ============================================================================
struct ThreePanelSizes {
    float left_w, left_h;
    float center_w, center_h;
    float right_w, right_h;
    float gap;
};

inline ThreePanelSizes three_panel_layout(
    float avail_w, float avail_h,
    float left_pct = 18, float right_pct = 24,
    float min_left = 160, float min_right = 200, float min_center = 200,
    float gap_px = 4)
{
    Breakpoint bp = get_breakpoint(avail_w);

    // On compact screens, stack vertically (return full width for each)
    if (bp == Breakpoint::Compact) {
        float third = avail_h / 3.0f;
        return { avail_w, third, avail_w, third, avail_w, third, 0 };
    }

    // On medium, collapse right panel into center
    if (bp == Breakpoint::Medium) {
        YogaLayout yoga;
        auto root   = yoga.node().row().width(avail_w).height(avail_h).gap(gap_px);
        auto left   = root.child().width_percent(25).min_width(min_left);
        auto center = root.child().flex_grow(1).min_width(min_center);
        yoga.calculate(avail_w, avail_h);
        return {
            left.computed_width(), left.computed_height(),
            center.computed_width(), center.computed_height(),
            0, 0,  // no right panel
            gap_px
        };
    }

    // Expanded / Large: full three-panel
    YogaLayout yoga;
    auto root   = yoga.node().row().width(avail_w).height(avail_h).gap(gap_px);
    auto left   = root.child().width_percent(left_pct).min_width(min_left);
    auto center = root.child().flex_grow(1).min_width(min_center);
    auto right  = root.child().width_percent(right_pct).min_width(min_right);
    yoga.calculate(avail_w, avail_h);

    return {
        left.computed_width(), left.computed_height(),
        center.computed_width(), center.computed_height(),
        right.computed_width(), right.computed_height(),
        gap_px
    };
}

// ============================================================================
// Two-panel layout (main | sidebar) via Yoga — for dashboard grid+pulse
// ============================================================================
struct TwoPanelSizes {
    float main_w, main_h;
    float side_w, side_h;
};

inline TwoPanelSizes two_panel_layout(
    float avail_w, float avail_h,
    bool side_visible,
    float side_pct = 25, float min_side = 280, float max_side = 340)
{
    if (!side_visible) {
        return { avail_w, avail_h, 0, 0 };
    }

    YogaLayout yoga;
    auto root = yoga.node().row().width(avail_w).height(avail_h);
    auto main_node = root.child().flex_grow(1).min_width(200);
    auto side_node = root.child().width_percent(side_pct)
                         .min_width(min_side).max_width(max_side);
    yoga.calculate(avail_w, avail_h);

    return {
        main_node.computed_width(), main_node.computed_height(),
        side_node.computed_width(), side_node.computed_height()
    };
}

// ============================================================================
// Responsive grid — wrapping flex container for market panels, widgets, etc.
// Returns panel width given available width and desired min panel size
// ============================================================================
struct GridSizes {
    float panel_w;
    int columns;
};

inline GridSizes responsive_grid(float avail_w, float min_panel_w = 280, float gap_px = 8) {
    int cols = std::max(1, (int)((avail_w + gap_px) / (min_panel_w + gap_px)));
    float panel_w = (avail_w - gap_px * (cols - 1)) / (float)cols;
    return { panel_w, cols };
}

// ============================================================================
// Vertical stack layout via Yoga — for screens with header + toolbar + content
// ============================================================================
struct VStackSizes {
    float widths;  // all same (full width)
    std::vector<float> heights;
};

inline VStackSizes vstack_layout(
    float avail_w, float avail_h,
    const std::vector<float>& fixed_heights,  // -1 means flex_grow(1)
    float gap_px = 0)
{
    YogaLayout yoga;
    auto root = yoga.node().column().width(avail_w).height(avail_h).gap(gap_px);

    std::vector<NodeRef> nodes;
    for (float h : fixed_heights) {
        auto child = root.child().width_percent(100);
        if (h < 0) {
            // Negative value = flex_grow weight (e.g. -1.5 means flex_grow(1.5))
            child.flex_grow(-h).min_height(50);
        } else {
            child.height(h);
        }
        nodes.push_back(child);
    }
    yoga.calculate(avail_w, avail_h);

    VStackSizes result;
    result.widths = avail_w;
    for (auto& n : nodes) {
        result.heights.push_back(n.computed_height());
    }
    return result;
}

} // namespace fincept::ui
