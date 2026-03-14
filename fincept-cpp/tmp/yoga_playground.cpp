// Yoga Playground — standalone console app to learn Yoga layout
// Prints computed layout values so you can see exactly how flexbox works.

#include <yoga/YGNode.h>
#include <yoga/YGNodeStyle.h>
#include <yoga/YGNodeLayout.h>
#include <yoga/YGEnums.h>
#include <cstdio>
#include <vector>

// Helper to print a node's computed rect
void print_node(const char* name, YGNodeRef node) {
    printf("  %-20s  x=%6.1f  y=%6.1f  w=%6.1f  h=%6.1f\n",
           name,
           YGNodeLayoutGetLeft(node),
           YGNodeLayoutGetTop(node),
           YGNodeLayoutGetWidth(node),
           YGNodeLayoutGetHeight(node));
}

void separator(const char* title) {
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n\n");
}

// ============================================================================
// Example 1: Two columns — fixed + flexible
// ============================================================================
void example_two_columns() {
    separator("1. TWO COLUMNS (fixed sidebar + flexible main)");

    printf("  Concept: flex_grow(1) means 'take all remaining space'\n");
    printf("  Container: 800 x 600, Row direction\n\n");

    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
    YGNodeStyleSetWidth(root, 800);
    YGNodeStyleSetHeight(root, 600);

    YGNodeRef sidebar = YGNodeNew();
    YGNodeStyleSetWidth(sidebar, 200);  // fixed 200px

    YGNodeRef main_area = YGNodeNew();
    YGNodeStyleSetFlexGrow(main_area, 1);  // takes remaining 600px

    YGNodeInsertChild(root, sidebar, 0);
    YGNodeInsertChild(root, main_area, 1);

    YGNodeCalculateLayout(root, 800, 600, YGDirectionLTR);

    print_node("sidebar (w=200)", sidebar);
    print_node("main (grow=1)", main_area);

    printf("\n  -> Sidebar is fixed at 200px\n");
    printf("  -> Main fills remaining: 800 - 200 = 600px\n");

    YGNodeFreeRecursive(root);
}

// ============================================================================
// Example 2: Three columns with percentages
// ============================================================================
void example_three_columns() {
    separator("2. THREE COLUMNS (percentage widths)");

    printf("  Concept: width_percent distributes space proportionally\n");
    printf("  Container: 1200 x 800, Row direction\n\n");

    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
    YGNodeStyleSetWidth(root, 1200);
    YGNodeStyleSetHeight(root, 800);
    YGNodeStyleSetGap(root, YGGutterAll, 4);  // 4px gap between children

    YGNodeRef left = YGNodeNew();
    YGNodeStyleSetWidthPercent(left, 20);  // 20%

    YGNodeRef center = YGNodeNew();
    YGNodeStyleSetFlexGrow(center, 1);     // fills remaining

    YGNodeRef right = YGNodeNew();
    YGNodeStyleSetWidthPercent(right, 25); // 25%

    YGNodeInsertChild(root, left, 0);
    YGNodeInsertChild(root, center, 1);
    YGNodeInsertChild(root, right, 2);

    YGNodeCalculateLayout(root, 1200, 800, YGDirectionLTR);

    print_node("left (20%)", left);
    print_node("center (grow=1)", center);
    print_node("right (25%)", right);

    printf("\n  -> Left: 20%% of 1200 = ~240px\n");
    printf("  -> Right: 25%% of 1200 = ~300px\n");
    printf("  -> Center: remaining space (minus gaps)\n");

    YGNodeFreeRecursive(root);
}

// ============================================================================
// Example 3: Vertical stack (column direction)
// ============================================================================
void example_vertical_stack() {
    separator("3. VERTICAL STACK (header + content + footer)");

    printf("  Concept: Column direction stacks top-to-bottom\n");
    printf("  flex_grow on content = fill remaining vertical space\n");
    printf("  Container: 800 x 600\n\n");

    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionColumn);
    YGNodeStyleSetWidth(root, 800);
    YGNodeStyleSetHeight(root, 600);

    YGNodeRef header = YGNodeNew();
    YGNodeStyleSetHeight(header, 40);  // fixed 40px

    YGNodeRef content = YGNodeNew();
    YGNodeStyleSetFlexGrow(content, 1);  // fills remaining

    YGNodeRef footer = YGNodeNew();
    YGNodeStyleSetHeight(footer, 30);  // fixed 30px

    YGNodeInsertChild(root, header, 0);
    YGNodeInsertChild(root, content, 1);
    YGNodeInsertChild(root, footer, 2);

    YGNodeCalculateLayout(root, 800, 600, YGDirectionLTR);

    print_node("header (h=40)", header);
    print_node("content (grow=1)", content);
    print_node("footer (h=30)", footer);

    printf("\n  -> Header: fixed 40px at top\n");
    printf("  -> Content: 600 - 40 - 30 = 530px\n");
    printf("  -> Footer: fixed 30px at bottom\n");

    YGNodeFreeRecursive(root);
}

// ============================================================================
// Example 4: Padding and gaps
// ============================================================================
void example_padding_and_gaps() {
    separator("4. PADDING AND GAPS");

    printf("  Concept: Padding = inner space, Gap = space between children\n");
    printf("  Container: 800 x 600, padding=16, gap=8\n\n");

    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
    YGNodeStyleSetWidth(root, 800);
    YGNodeStyleSetHeight(root, 600);
    YGNodeStyleSetPadding(root, YGEdgeAll, 16);  // 16px inner padding
    YGNodeStyleSetGap(root, YGGutterAll, 8);     // 8px between children

    YGNodeRef a = YGNodeNew();
    YGNodeStyleSetFlexGrow(a, 1);

    YGNodeRef b = YGNodeNew();
    YGNodeStyleSetFlexGrow(b, 1);

    YGNodeRef c = YGNodeNew();
    YGNodeStyleSetFlexGrow(c, 1);

    YGNodeInsertChild(root, a, 0);
    YGNodeInsertChild(root, b, 1);
    YGNodeInsertChild(root, c, 2);

    YGNodeCalculateLayout(root, 800, 600, YGDirectionLTR);

    print_node("child A", a);
    print_node("child B", b);
    print_node("child C", c);

    float usable = 800 - 16*2 - 8*2;  // total - padding*2 - gap*2
    printf("\n  -> Usable width: 800 - 32 (padding) - 16 (gaps) = %.0f\n", usable);
    printf("  -> Each child: %.0f / 3 = %.1f\n", usable, usable / 3);
    printf("  -> Notice x positions offset by padding (16) and gaps (8)\n");

    YGNodeFreeRecursive(root);
}

// ============================================================================
// Example 5: min/max constraints
// ============================================================================
void example_min_max() {
    separator("5. MIN/MAX WIDTH CONSTRAINTS");

    printf("  Concept: min_width/max_width set bounds on computed size\n\n");

    // Test with wide container
    printf("  --- Wide container (1400px) ---\n");
    {
        YGNodeRef root = YGNodeNew();
        YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
        YGNodeStyleSetWidth(root, 1400);
        YGNodeStyleSetHeight(root, 600);

        YGNodeRef sidebar = YGNodeNew();
        YGNodeStyleSetWidthPercent(sidebar, 30);
        YGNodeStyleSetMinWidth(sidebar, 150);
        YGNodeStyleSetMaxWidth(sidebar, 300);

        YGNodeRef main_area = YGNodeNew();
        YGNodeStyleSetFlexGrow(main_area, 1);

        YGNodeInsertChild(root, sidebar, 0);
        YGNodeInsertChild(root, main_area, 1);

        YGNodeCalculateLayout(root, 1400, 600, YGDirectionLTR);

        print_node("sidebar (30%, max=300)", sidebar);
        print_node("main", main_area);
        printf("  -> 30%% of 1400 = 420, but max=300, so clamped to 300\n");

        YGNodeFreeRecursive(root);
    }

    // Test with narrow container
    printf("\n  --- Narrow container (400px) ---\n");
    {
        YGNodeRef root = YGNodeNew();
        YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
        YGNodeStyleSetWidth(root, 400);
        YGNodeStyleSetHeight(root, 600);

        YGNodeRef sidebar = YGNodeNew();
        YGNodeStyleSetWidthPercent(sidebar, 30);
        YGNodeStyleSetMinWidth(sidebar, 150);
        YGNodeStyleSetMaxWidth(sidebar, 300);

        YGNodeRef main_area = YGNodeNew();
        YGNodeStyleSetFlexGrow(main_area, 1);

        YGNodeInsertChild(root, sidebar, 0);
        YGNodeInsertChild(root, main_area, 1);

        YGNodeCalculateLayout(root, 400, 600, YGDirectionLTR);

        print_node("sidebar (30%, min=150)", sidebar);
        print_node("main", main_area);
        printf("  -> 30%% of 400 = 120, but min=150, so bumped to 150\n");

        YGNodeFreeRecursive(root);
    }
}

// ============================================================================
// Example 6: Nested layouts (vertical outer, horizontal inner)
// ============================================================================
void example_nested() {
    separator("6. NESTED LAYOUT (vertical outer, horizontal inner)");

    printf("  Concept: Children can have their own flex direction\n");
    printf("  Outer: column (header + body), Inner body: row (list + detail)\n\n");

    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionColumn);
    YGNodeStyleSetWidth(root, 1000);
    YGNodeStyleSetHeight(root, 700);

    // Header: fixed 50px
    YGNodeRef header = YGNodeNew();
    YGNodeStyleSetHeight(header, 50);

    // Body: fills remaining, has its own row children
    YGNodeRef body = YGNodeNew();
    YGNodeStyleSetFlexGrow(body, 1);
    YGNodeStyleSetFlexDirection(body, YGFlexDirectionRow);  // inner row!
    YGNodeStyleSetGap(body, YGGutterAll, 4);

    // Body's children
    YGNodeRef list = YGNodeNew();
    YGNodeStyleSetWidthPercent(list, 30);

    YGNodeRef detail = YGNodeNew();
    YGNodeStyleSetFlexGrow(detail, 1);

    YGNodeInsertChild(body, list, 0);
    YGNodeInsertChild(body, detail, 1);

    YGNodeInsertChild(root, header, 0);
    YGNodeInsertChild(root, body, 1);

    YGNodeCalculateLayout(root, 1000, 700, YGDirectionLTR);

    printf("  Outer (column):\n");
    print_node("header (h=50)", header);
    print_node("body (grow=1)", body);
    printf("\n  Inner body (row):\n");
    print_node("list (30%%)", list);
    print_node("detail (grow=1)", detail);

    printf("\n  -> Header: 50px tall, full width\n");
    printf("  -> Body: 700-50 = 650px tall\n");
    printf("  -> List: 30%% of 1000 = 300px wide, 650px tall\n");
    printf("  -> Detail: remaining width, 650px tall\n");
    printf("\n  NOTE: list and detail y=0 because they're relative to body,\n");
    printf("  not to root. In ImGui you'd nest BeginChild calls.\n");

    YGNodeFreeRecursive(root);
}

// ============================================================================
// Example 7: flex_grow ratios
// ============================================================================
void example_flex_grow_ratios() {
    separator("7. FLEX GROW RATIOS");

    printf("  Concept: flex_grow values are RATIOS, not absolute sizes\n");
    printf("  Container: 900px wide\n\n");

    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
    YGNodeStyleSetWidth(root, 900);
    YGNodeStyleSetHeight(root, 100);

    YGNodeRef a = YGNodeNew();
    YGNodeStyleSetFlexGrow(a, 1);  // 1 part

    YGNodeRef b = YGNodeNew();
    YGNodeStyleSetFlexGrow(b, 2);  // 2 parts

    YGNodeRef c = YGNodeNew();
    YGNodeStyleSetFlexGrow(c, 3);  // 3 parts

    YGNodeInsertChild(root, a, 0);
    YGNodeInsertChild(root, b, 1);
    YGNodeInsertChild(root, c, 2);

    YGNodeCalculateLayout(root, 900, 100, YGDirectionLTR);

    print_node("A (grow=1)", a);
    print_node("B (grow=2)", b);
    print_node("C (grow=3)", c);

    printf("\n  -> Total grow = 1+2+3 = 6\n");
    printf("  -> A = 900 * 1/6 = 150px\n");
    printf("  -> B = 900 * 2/6 = 300px\n");
    printf("  -> C = 900 * 3/6 = 450px\n");

    YGNodeFreeRecursive(root);
}

// ============================================================================
// Example 8: Alignment
// ============================================================================
void example_alignment() {
    separator("8. ALIGNMENT (justify + align_items)");

    printf("  justify_content = main axis (horizontal in row)\n");
    printf("  align_items     = cross axis (vertical in row)\n\n");

    // Center both axes
    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
    YGNodeStyleSetWidth(root, 400);
    YGNodeStyleSetHeight(root, 400);
    YGNodeStyleSetJustifyContent(root, YGJustifyCenter);
    YGNodeStyleSetAlignItems(root, YGAlignCenter);

    YGNodeRef box = YGNodeNew();
    YGNodeStyleSetWidth(box, 100);
    YGNodeStyleSetHeight(box, 100);

    YGNodeInsertChild(root, box, 0);

    YGNodeCalculateLayout(root, 400, 400, YGDirectionLTR);

    print_node("box (centered)", box);

    printf("\n  -> Box should be at (150, 150) — dead center of 400x400\n");
    printf("  -> justify:center → x = (400-100)/2 = 150\n");
    printf("  -> align:center   → y = (400-100)/2 = 150\n");

    YGNodeFreeRecursive(root);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║    YOGA LAYOUT PLAYGROUND                ║\n");
    printf("║    Learn flexbox by seeing the numbers    ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    example_two_columns();
    example_three_columns();
    example_vertical_stack();
    example_padding_and_gaps();
    example_min_max();
    example_nested();
    example_flex_grow_ratios();
    example_alignment();

    printf("\n========================================\n");
    printf("  Done! All layouts computed successfully.\n");
    printf("  These are the same computations that\n");
    printf("  happen every frame in the terminal app.\n");
    printf("========================================\n\n");

    return 0;
}
