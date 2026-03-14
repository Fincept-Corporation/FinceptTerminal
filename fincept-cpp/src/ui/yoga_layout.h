#pragma once
// Yoga + ImGui layout bridge
// Provides a RAII C++ wrapper around Facebook Yoga's flexbox engine,
// producing computed rects that ImGui can use for positioning.
//
// Usage:
//   YogaLayout layout;
//   auto root = layout.node()
//       .direction(Row).padding(4).gap(4);
//   auto left = root.child().width_percent(18).min_width(160);
//   auto center = root.child().flex_grow(1).min_width(200);
//   auto right = root.child().width_percent(24).min_width(200);
//   layout.calculate(avail_w, avail_h);
//
//   // Use computed rects:
//   ImGui::SetCursorPos(left.pos());
//   ImGui::BeginChild("left", left.size());
//   ...

#include <yoga/YGNode.h>
#include <yoga/YGNodeStyle.h>
#include <yoga/YGNodeLayout.h>
#include <yoga/YGEnums.h>
#include <imgui.h>
#include <vector>
#include <cassert>

namespace fincept::ui {

// ============================================================================
// Rect — computed layout result
// ============================================================================
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;

    ImVec2 pos()  const { return ImVec2(x, y); }
    ImVec2 size() const { return ImVec2(w, h); }

    // Offset by a parent origin (for absolute screen positioning)
    Rect offset(float ox, float oy) const {
        return { x + ox, y + oy, w, h };
    }
};

// ============================================================================
// NodeRef — fluent builder wrapping a YGNodeRef
// ============================================================================
class NodeRef {
public:
    explicit NodeRef(YGNodeRef n) : node_(n) {}

    // --- Direction / Flex ---
    NodeRef& direction(YGFlexDirection dir) {
        YGNodeStyleSetFlexDirection(node_, dir);
        return *this;
    }
    NodeRef& row()    { return direction(YGFlexDirectionRow); }
    NodeRef& column() { return direction(YGFlexDirectionColumn); }

    NodeRef& flex_grow(float v) {
        YGNodeStyleSetFlexGrow(node_, v);
        return *this;
    }
    NodeRef& flex_shrink(float v) {
        YGNodeStyleSetFlexShrink(node_, v);
        return *this;
    }
    NodeRef& flex_basis(float v) {
        YGNodeStyleSetFlexBasis(node_, v);
        return *this;
    }
    NodeRef& flex_basis_percent(float v) {
        YGNodeStyleSetFlexBasisPercent(node_, v);
        return *this;
    }
    NodeRef& flex_wrap(YGWrap wrap = YGWrapWrap) {
        YGNodeStyleSetFlexWrap(node_, wrap);
        return *this;
    }

    // --- Sizing ---
    NodeRef& width(float v)           { YGNodeStyleSetWidth(node_, v); return *this; }
    NodeRef& width_percent(float v)   { YGNodeStyleSetWidthPercent(node_, v); return *this; }
    NodeRef& width_auto()             { YGNodeStyleSetWidthAuto(node_); return *this; }
    NodeRef& height(float v)          { YGNodeStyleSetHeight(node_, v); return *this; }
    NodeRef& height_percent(float v)  { YGNodeStyleSetHeightPercent(node_, v); return *this; }
    NodeRef& height_auto()            { YGNodeStyleSetHeightAuto(node_); return *this; }
    NodeRef& min_width(float v)       { YGNodeStyleSetMinWidth(node_, v); return *this; }
    NodeRef& min_height(float v)      { YGNodeStyleSetMinHeight(node_, v); return *this; }
    NodeRef& max_width(float v)       { YGNodeStyleSetMaxWidth(node_, v); return *this; }
    NodeRef& max_height(float v)      { YGNodeStyleSetMaxHeight(node_, v); return *this; }
    NodeRef& aspect_ratio(float v)    { YGNodeStyleSetAspectRatio(node_, v); return *this; }

    // --- Spacing ---
    NodeRef& padding(float v) {
        YGNodeStyleSetPadding(node_, YGEdgeAll, v);
        return *this;
    }
    NodeRef& padding(float h, float v) {
        YGNodeStyleSetPadding(node_, YGEdgeHorizontal, h);
        YGNodeStyleSetPadding(node_, YGEdgeVertical, v);
        return *this;
    }
    NodeRef& padding_left(float v)   { YGNodeStyleSetPadding(node_, YGEdgeLeft, v); return *this; }
    NodeRef& padding_right(float v)  { YGNodeStyleSetPadding(node_, YGEdgeRight, v); return *this; }
    NodeRef& padding_top(float v)    { YGNodeStyleSetPadding(node_, YGEdgeTop, v); return *this; }
    NodeRef& padding_bottom(float v) { YGNodeStyleSetPadding(node_, YGEdgeBottom, v); return *this; }

    NodeRef& margin(float v) {
        YGNodeStyleSetMargin(node_, YGEdgeAll, v);
        return *this;
    }
    NodeRef& margin(float h, float v) {
        YGNodeStyleSetMargin(node_, YGEdgeHorizontal, h);
        YGNodeStyleSetMargin(node_, YGEdgeVertical, v);
        return *this;
    }
    NodeRef& margin_left(float v)   { YGNodeStyleSetMargin(node_, YGEdgeLeft, v); return *this; }
    NodeRef& margin_right(float v)  { YGNodeStyleSetMargin(node_, YGEdgeRight, v); return *this; }
    NodeRef& margin_top(float v)    { YGNodeStyleSetMargin(node_, YGEdgeTop, v); return *this; }
    NodeRef& margin_bottom(float v) { YGNodeStyleSetMargin(node_, YGEdgeBottom, v); return *this; }

    NodeRef& gap(float v) {
        YGNodeStyleSetGap(node_, YGGutterAll, v);
        return *this;
    }
    NodeRef& gap_row(float v)    { YGNodeStyleSetGap(node_, YGGutterRow, v); return *this; }
    NodeRef& gap_column(float v) { YGNodeStyleSetGap(node_, YGGutterColumn, v); return *this; }

    // --- Alignment ---
    NodeRef& justify(YGJustify v)     { YGNodeStyleSetJustifyContent(node_, v); return *this; }
    NodeRef& align_items(YGAlign v)   { YGNodeStyleSetAlignItems(node_, v); return *this; }
    NodeRef& align_self(YGAlign v)    { YGNodeStyleSetAlignSelf(node_, v); return *this; }
    NodeRef& align_content(YGAlign v) { YGNodeStyleSetAlignContent(node_, v); return *this; }

    // --- Children ---
    // Add a child and return a NodeRef to it (the YGNode is owned by the layout)
    NodeRef child();

    // --- Read computed layout ---
    Rect rect() const {
        return {
            YGNodeLayoutGetLeft(node_),
            YGNodeLayoutGetTop(node_),
            YGNodeLayoutGetWidth(node_),
            YGNodeLayoutGetHeight(node_)
        };
    }

    // Absolute rect (walks up to root, summing offsets)
    Rect abs_rect() const {
        Rect r = rect();
        YGNodeRef parent = YGNodeGetParent(node_);
        while (parent) {
            r.x += YGNodeLayoutGetLeft(parent);
            r.y += YGNodeLayoutGetTop(parent);
            parent = YGNodeGetParent(parent);
        }
        return r;
    }

    ImVec2 pos()  const { return rect().pos(); }
    ImVec2 size() const { return rect().size(); }
    float computed_width()  const { return YGNodeLayoutGetWidth(node_); }
    float computed_height() const { return YGNodeLayoutGetHeight(node_); }

    YGNodeRef raw() const { return node_; }

private:
    YGNodeRef node_;
    friend class YogaLayout;
};

// ============================================================================
// YogaLayout — owns all YGNodes, RAII lifetime
// ============================================================================
class YogaLayout {
public:
    YogaLayout() {
        root_ = YGNodeNew();
        nodes_.push_back(root_);
    }

    ~YogaLayout() {
        // Free root recursively (frees all children too)
        if (root_) YGNodeFreeRecursive(root_);
    }

    // Non-copyable
    YogaLayout(const YogaLayout&) = delete;
    YogaLayout& operator=(const YogaLayout&) = delete;

    // Movable
    YogaLayout(YogaLayout&& other) noexcept
        : root_(other.root_), nodes_(std::move(other.nodes_)) {
        other.root_ = nullptr;
    }
    YogaLayout& operator=(YogaLayout&& other) noexcept {
        if (this != &other) {
            if (root_) YGNodeFreeRecursive(root_);
            root_ = other.root_;
            nodes_ = std::move(other.nodes_);
            other.root_ = nullptr;
        }
        return *this;
    }

    // Access root for building the tree
    NodeRef node() { return NodeRef(root_); }

    // Create an unattached node (call parent.child() instead for normal use)
    NodeRef create_node() {
        YGNodeRef n = YGNodeNew();
        nodes_.push_back(n);
        return NodeRef(n);
    }

    // Calculate layout given available width/height
    void calculate(float width, float height,
                   YGDirection dir = YGDirectionLTR) {
        YGNodeCalculateLayout(root_, width, height, dir);
    }

private:
    YGNodeRef root_ = nullptr;
    std::vector<YGNodeRef> nodes_;  // tracking for debug only; tree owns memory

    friend class NodeRef;
};

// --- NodeRef::child() implementation (needs YogaLayout to not be incomplete) ---
inline NodeRef NodeRef::child() {
    YGNodeRef c = YGNodeNew();
    YGNodeInsertChild(node_, c, YGNodeGetChildCount(node_));
    return NodeRef(c);
}

// ============================================================================
// Convenience: begin an ImGui child using a Yoga-computed rect
// ============================================================================
inline bool BeginYogaChild(const char* id, const NodeRef& node,
                           ImGuiChildFlags child_flags = 0,
                           ImGuiWindowFlags window_flags = 0) {
    ImGui::SetCursorPos(node.pos());
    return ImGui::BeginChild(id, node.size(), child_flags, window_flags);
}

// Same but with absolute positioning (for top-level windows)
inline void SetYogaWindowPosSize(const NodeRef& node, ImVec2 origin) {
    Rect r = node.rect();
    ImGui::SetNextWindowPos(ImVec2(origin.x + r.x, origin.y + r.y));
    ImGui::SetNextWindowSize(ImVec2(r.w, r.h));
}

} // namespace fincept::ui
