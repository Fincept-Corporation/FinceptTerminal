// Trade Visualization — Trade Flow style
#include "screens/trade_viz/TradeVizScreen.h"

#include "core/session/ScreenStateManager.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// ── Style constants ─────────────────────────────────────────────────────────

static const char* FONT = "'Consolas','Courier New',monospace";

static QString combo_ss() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3;"
                   "  padding: 2px 8px; font-size: 11px; font-family: 'Consolas','Courier New',monospace;"
                   "  min-width: 100px; }"
                   "QComboBox:hover { border-color: %4; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox::down-arrow { image: none; border-left: 4px solid transparent;"
                   "  border-right: 4px solid transparent; border-top: 5px solid %5; }"
                   "QComboBox QAbstractItemView { background: %6; color: %2;"
                   "  border: 1px solid %3; selection-background-color: %7;"
                   "  selection-color: %8; font-family: 'Consolas','Courier New',monospace; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT(),
             ui::colors::TEXT_SECONDARY(), ui::colors::BG_BASE(), ui::colors::BG_HOVER(), ui::colors::AMBER());
}

static QString table_ss() {
    return QString("QTableWidget { background: %1; color: %2; border: none;"
                   "  gridline-color: %3; font-size: 13px; font-family: 'Consolas','Courier New',monospace; }"
                   "QTableWidget::item { padding: 3px 10px; border-bottom: 1px solid %3; }"
                   "QTableWidget::item:selected { background: %4; color: %2; }"
                   "QHeaderView::section { background: %5; color: %6; font-size: 12px;"
                   "  font-weight: bold; border: none; border-bottom: 1px solid %7;"
                   "  padding: 6px 10px; font-family: 'Consolas','Courier New',monospace; }"
                   "QScrollBar:vertical { width: 5px; background: transparent; }"
                   "QScrollBar::handle:vertical { background: %8; }"
                   "QScrollBar::handle:vertical:hover { background: %9; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(), ui::colors::BG_HOVER(),
             ui::colors::BG_BASE(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::BORDER_MED(),
             ui::colors::BORDER_BRIGHT());
}

// ── Trade data ──────────────────────────────────────────────────────────────

struct TradePartner {
    const char* name;
    const char* abbrev;
    double imports; // $M — imports from this partner
    double exports; // $M — exports to this partner
};

// US bilateral trade data (2024 estimates, $M)
static const TradePartner g_partners[] = {
    {"Mexico", "Mex.", 437898.0, 345098.0},      {"Canada", "Can.", 406282.0, 297329.0},
    {"China", "China", 427230.0, 177093.0},      {"Germany", "Ger.", 145632.0, 90342.5},
    {"Japan", "Jpn.", 138420.0, 94699.0},        {"South Korea", "S.Kor.", 115210.0, 88830.2},
    {"Vietnam", "Viet.", 109450.0, 42081.8},     {"United Kingdom", "UK", 67342.0, 73428.6},
    {"India", "India", 87120.0, 45012.1},        {"Ireland", "Ire.", 82340.0, 44515.1},
    {"Netherlands", "Neth.", 56120.0, 52126.2},  {"France", "Fr.", 62340.0, 45550.2},
    {"Italy", "Itl.", 67230.0, 37358.7},         {"Singapore", "Sing.", 48120.0, 51015.2},
    {"Switzerland", "Switz.", 54230.0, 38722.6},
};
static constexpr int NUM_PARTNERS = 15;

// ── Chord diagram widget ────────────────────────────────────────────────────

class TradeFlowChordWidget : public QWidget {
  public:
    explicit TradeFlowChordWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int w = width();
        const int h = height();
        const int cx = w / 2;
        const int cy = h / 2;
        const int radius = std::min(cx, cy) - 65;
        if (radius < 60)
            return;

        // ── Concentric grid rings ────────────────────────────────────────────
        QPen grid_pen(QColor(26, 26, 26), 1);
        p.setPen(grid_pen);
        for (int r = 1; r <= 5; ++r) {
            p.drawEllipse(QPointF(cx, cy), radius * r / 5.0, radius * r / 5.0);
        }

        // ── Radial spokes ────────────────────────────────────────────────────
        for (int i = 0; i < NUM_PARTNERS; ++i) {
            double angle = 2.0 * M_PI * i / NUM_PARTNERS - M_PI / 2.0;
            int x2 = cx + static_cast<int>((radius + 8) * std::cos(angle));
            int y2 = cy + static_cast<int>((radius + 8) * std::sin(angle));
            p.setPen(QPen(QColor(22, 22, 22), 1));
            p.drawLine(cx, cy, x2, y2);
        }

        // ── Normalization ────────────────────────────────────────────────────
        double max_val = 0;
        for (int i = 0; i < NUM_PARTNERS; ++i) {
            max_val = std::max(max_val, std::max(g_partners[i].imports, g_partners[i].exports));
        }
        if (max_val < 1.0)
            max_val = 1.0;

        // ── Import arcs (partner → US center) — amber/orange bundles ─────────
        // Draw back-to-front so top partners render on top
        for (int i = NUM_PARTNERS - 1; i >= 0; --i) {
            double angle = 2.0 * M_PI * i / NUM_PARTNERS - M_PI / 2.0;
            int px = cx + static_cast<int>(radius * std::cos(angle));
            int py = cy + static_cast<int>(radius * std::sin(angle));

            double mid_x = (cx + px) / 2.0;
            double mid_y = (cy + py) / 2.0;
            double dx = px - cx;
            double dy = py - cy;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0)
                continue;
            double nx = -dy / len;
            double ny = dx / len;

            double strength = g_partners[i].imports / max_val;

            // Draw multiple sub-arcs for thick "bundle" effect
            int num_strands = 2 + static_cast<int>(4 * strength);
            for (int s = 0; s < num_strands; ++s) {
                double offset = (s - num_strands / 2.0) * 3.0;
                double curve = 25.0 + 20.0 * strength + offset * 2.0;

                int alpha = 100 + static_cast<int>(120 * strength);
                // Orange/amber color with slight variation per strand
                QColor col(217, 119 + s * 5, 6, alpha);
                p.setPen(QPen(col, 1.5));

                QPainterPath path;
                path.moveTo(px, py);
                path.quadTo(mid_x + nx * curve, mid_y + ny * curve, cx, cy);
                p.drawPath(path);
            }
        }

        // ── Export arcs (US → partner) — magenta/purple bundles (top 5) ──────
        for (int i = 4; i >= 0; --i) {
            double angle = 2.0 * M_PI * i / NUM_PARTNERS - M_PI / 2.0;
            int px = cx + static_cast<int>(radius * std::cos(angle));
            int py = cy + static_cast<int>(radius * std::sin(angle));

            double mid_x = (cx + px) / 2.0;
            double mid_y = (cy + py) / 2.0;
            double dx = px - cx;
            double dy = py - cy;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0)
                continue;
            double nx = -dy / len;
            double ny = dx / len;

            double strength = g_partners[i].exports / max_val;

            int num_strands = 2 + static_cast<int>(4 * strength);
            for (int s = 0; s < num_strands; ++s) {
                double offset = (s - num_strands / 2.0) * 3.0;
                double curve = -(25.0 + 20.0 * strength + offset * 2.0);

                int alpha = 100 + static_cast<int>(130 * strength);
                // Purple to magenta gradient
                int rv = 180 + static_cast<int>(50 * (1.0 - strength)) + s * 3;
                QColor col(rv, 30, 180 - s * 8, alpha);
                p.setPen(QPen(col, 1.5));

                QPainterPath path;
                path.moveTo(cx, cy);
                path.quadTo(mid_x + nx * curve, mid_y + ny * curve, px, py);
                p.drawPath(path);
            }
        }

        // ── Country nodes — small gray rectangles at circle edge ─────────────
        for (int i = 0; i < NUM_PARTNERS; ++i) {
            double angle = 2.0 * M_PI * i / NUM_PARTNERS - M_PI / 2.0;
            int px = cx + static_cast<int>(radius * std::cos(angle));
            int py = cy + static_cast<int>(radius * std::sin(angle));

            // Node rectangle
            QRect node(px - 14, py - 9, 28, 18);
            p.fillRect(node, QColor(40, 40, 40));
            p.setPen(QPen(QColor(60, 60, 60), 1));
            p.drawRect(node);

            // Country abbreviation label — outside the circle
            double label_r = radius + 40;
            int lx = cx + static_cast<int>(label_r * std::cos(angle));
            int ly = cy + static_cast<int>(label_r * std::sin(angle));

            QFont lf(FONT, 9);
            lf.setBold(true);
            p.setFont(lf);
            p.setPen(QColor(ui::colors::TEXT_PRIMARY()));
            QRect lr(lx - 32, ly - 9, 64, 18);
            p.drawText(lr, Qt::AlignCenter, g_partners[i].abbrev);
        }

        // ── Center "US" node ─────────────────────────────────────────────────
        QRect us_rect(cx - 16, cy - 12, 32, 24);
        p.fillRect(us_rect, QColor(30, 30, 30));
        p.setPen(QPen(QColor(ui::colors::AMBER()), 1));
        p.drawRect(us_rect);

        QFont us_font(FONT, 10);
        us_font.setBold(true);
        p.setFont(us_font);
        p.setPen(QColor(ui::colors::AMBER()));
        p.drawText(us_rect, Qt::AlignCenter, "US");

        // ── Scale axis labels ────────────────────────────────────────────────
        QFont sf(FONT, 7);
        p.setFont(sf);
        p.setPen(QColor(ui::colors::TEXT_DIM()));

        // Import scale — above center
        int sx = cx - 50;
        int sy = cy - radius / 5;
        p.drawText(sx, sy - 2, "Imports ($M)");
        for (int i = 0; i <= 4; ++i) {
            double val = max_val * i / 4.0 / 1000.0;
            int tx = sx + i * 22;
            p.drawText(tx, sy + 10, QString("%1M").arg(val / 1000.0, 0, 'f', 2));
        }

        // Export scale — below center
        sy = cy + radius / 5;
        p.drawText(sx, sy + 14, "Exports ($M)");

        // ── Color legend bar (bottom-left) ───────────────────────────────────
        int bx = 10;
        int by = h - 18;
        for (int i = 0; i < 90; ++i) {
            double t = i / 90.0;
            int r_comp = static_cast<int>(217 * t);
            int g_comp = static_cast<int>(119 * t);
            int b_comp = static_cast<int>(6 * t);
            p.fillRect(bx + i, by, 1, 8, QColor(r_comp, g_comp, b_comp));
        }
        p.setFont(QFont(FONT, 7));
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        double lo = max_val * 0.1;
        double mid = max_val * 0.5;
        double hi = max_val;
        p.drawText(bx, by - 2, QString("%1k").arg(lo / 1000.0, 0, 'f', 1));
        p.drawText(bx + 38, by - 2, QString("%1k").arg(mid / 1000.0, 0, 'f', 1));
        p.drawText(bx + 72, by - 2, QString("%1k").arg(hi / 1000.0, 0, 'f', 1));
    }
};

// ============================================================================
// Tab bar — "21) Table  22) Settings  23) Export  24) Notes"
// ============================================================================

QWidget* TradeVizScreen::build_tab_bar() {
    auto* bar = new QWidget(this);
    bar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                           .arg(ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT()));
    bar->setFixedHeight(28);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    struct Tab {
        const char* num;
        const char* label;
        bool active;
    };
    Tab tabs[] = {
        {"21)", "Table", false},
        {"22)", "Settings", false},
        {"23)", "Export", false},
        {"24)", "Notes", false},
    };

    for (const auto& t : tabs) {
        auto* btn = new QWidget(this);
        btn->setFixedHeight(28);
        auto* bl = new QHBoxLayout(btn);
        bl->setContentsMargins(14, 0, 14, 0);
        bl->setSpacing(4);

        auto* num = new QLabel(t.num);
        num->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                                   " font-family: 'Consolas','Courier New',monospace;")
                               .arg(ui::colors::TEXT_SECONDARY()));
        bl->addWidget(num);

        auto* lbl = new QLabel(t.label);
        lbl->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;"
                                   " font-family: 'Consolas','Courier New',monospace;")
                               .arg(ui::colors::TEXT_PRIMARY()));
        bl->addWidget(lbl);

        hl->addWidget(btn);
    }

    hl->addStretch();

    // Right side: "Trade Flow" title
    auto* title = new QLabel("Trade Flow");
    title->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold; background: transparent;"
                                 " padding-right: 14px; font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER()));
    hl->addWidget(title);

    return bar;
}

// ============================================================================
// Filter bar — country, order by, periodicity, year
// ============================================================================

QWidget* TradeVizScreen::build_filter_bar() {
    auto* bar = new QWidget(this);
    bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    bar->setFixedHeight(32);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(10);

    // Country selector
    country_combo_ = new QComboBox;
    country_combo_->setStyleSheet(combo_ss());
    country_combo_->addItems({"United States", "China", "Germany", "Japan", "United Kingdom", "France", "India",
                              "Italy", "Canada", "South Korea", "Mexico", "Brazil", "Australia", "Netherlands",
                              "Switzerland"});
    country_combo_->setCurrentIndex(0);
    hl->addWidget(country_combo_);

    // Browse button
    auto* browse = new QLabel("20) Browse");
    browse->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;"
                                  " font-family: 'Consolas','Courier New',monospace;")
                              .arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(browse);

    hl->addSpacing(10);

    // Order by
    auto* order_lbl = new QLabel("Order by");
    order_lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;"
                                     " font-family: 'Consolas','Courier New',monospace;")
                                 .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(order_lbl);

    order_combo_ = new QComboBox;
    order_combo_->setStyleSheet(combo_ss());
    order_combo_->addItems({"Total Trade", "Imports", "Exports", "Trade Balance", "% of GDP"});
    hl->addWidget(order_combo_);

    hl->addSpacing(10);

    // Periodicity
    auto* period_lbl = new QLabel("Periodicity");
    period_lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;"
                                      " font-family: 'Consolas','Courier New',monospace;")
                                  .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(period_lbl);

    period_combo_ = new QComboBox;
    period_combo_->setStyleSheet(combo_ss());
    period_combo_->addItems({"Yearly", "Quarterly", "Monthly"});
    hl->addWidget(period_combo_);

    hl->addSpacing(10);

    // Year navigation
    auto* prev_btn = new QLabel("<<");
    prev_btn->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;"
                                    " font-family: 'Consolas','Courier New',monospace;")
                                .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(prev_btn);

    year_combo_ = new QComboBox;
    year_combo_->setStyleSheet(combo_ss());
    year_combo_->setFixedWidth(80);
    for (int y = 2024; y >= 2010; --y) {
        year_combo_->addItem(QString::number(y));
    }
    hl->addWidget(year_combo_);

    auto* next_btn = new QLabel(">>");
    next_btn->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;"
                                    " font-family: 'Consolas','Courier New',monospace;")
                                .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(next_btn);

    hl->addStretch();

    // Clock
    clock_label_ = new QLabel(QDateTime::currentDateTime().toString("HH:mm:ss"));
    clock_label_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                                        " font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(clock_label_);

    // Persist filter selections so the user returns to the same country/period
    // after restarting the app.
    auto on_filter = [this]() { fincept::ScreenStateManager::instance().notify_changed(this); };
    connect(country_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_filter);
    connect(order_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_filter);
    connect(period_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_filter);
    connect(year_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_filter);

    return bar;
}

// ============================================================================
// Partner ranking table
// ============================================================================

QWidget* TradeVizScreen::build_partner_table() {
    auto* panel = new QWidget(this);
    panel->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    partner_table_ = new QTableWidget;
    partner_table_->setStyleSheet(table_ss());
    partner_table_->setColumnCount(3);
    partner_table_->setHorizontalHeaderLabels({"", "Trading Partner", "Total Trade ($M)"});
    partner_table_->verticalHeader()->setVisible(false);
    partner_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    partner_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    partner_table_->setShowGrid(false);
    partner_table_->setAlternatingRowColors(true);
    partner_table_->setStyleSheet(
        partner_table_->styleSheet() +
        QString(" QTableWidget { alternate-background-color: %1; }").arg(ui::colors::BG_SURFACE()));

    auto* hdr = partner_table_->horizontalHeader();
    hdr->setSectionResizeMode(0, QHeaderView::Fixed);
    hdr->resizeSection(0, 35);
    hdr->setSectionResizeMode(1, QHeaderView::Stretch);
    hdr->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    partner_table_->verticalHeader()->setDefaultSectionSize(28);

    vl->addWidget(partner_table_, 1);
    return panel;
}

void TradeVizScreen::populate_partner_table() {
    partner_table_->setRowCount(NUM_PARTNERS);
    for (int i = 0; i < NUM_PARTNERS; ++i) {
        double total = g_partners[i].imports + g_partners[i].exports;

        auto* rank = new QTableWidgetItem(QString("%1)").arg(i + 1));
        rank->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        rank->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rank->setFont(QFont(FONT, 12));
        partner_table_->setItem(i, 0, rank);

        auto* name = new QTableWidgetItem(g_partners[i].name);
        name->setForeground(QColor(ui::colors::POSITIVE()));
        name->setFont(QFont(FONT, 13, QFont::Bold));
        partner_table_->setItem(i, 1, name);

        auto* val = new QTableWidgetItem(QString::number(total, 'f', 2));
        val->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        val->setFont(QFont(FONT, 13, QFont::Bold));
        val->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        partner_table_->setItem(i, 2, val);
    }
}

// ============================================================================
// Timer
// ============================================================================

void TradeVizScreen::update_clock() {
    if (clock_label_) {
        clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
    }
}

// ============================================================================
// Visibility — P3
// ============================================================================

void TradeVizScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (clock_timer_)
        clock_timer_->start();
}

void TradeVizScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (clock_timer_)
        clock_timer_->stop();
}

// ============================================================================
// Constructor + UI setup
// ============================================================================

TradeVizScreen::TradeVizScreen(QWidget* parent) : QWidget(parent) {
    setup_ui();
    populate_partner_table();

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &TradeVizScreen::update_clock);
}

void TradeVizScreen::setup_ui() {
    setObjectName("tradeVizScreen");
    setStyleSheet(QString("QWidget#tradeVizScreen { background: %1; }").arg(ui::colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Tab bar ──────────────────────────────────────────────────────────────
    root->addWidget(build_tab_bar());

    // ── Filter bar ───────────────────────────────────────────────────────────
    root->addWidget(build_filter_bar());

    // ── Main content: chord diagram (left) + partner table (right) ───────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet(QString("QSplitter { background: %1; }"
                                    "QSplitter::handle { background: %2; width: 1px; }")
                                .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    splitter->addWidget(new TradeFlowChordWidget);
    splitter->addWidget(build_partner_table());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    root->addWidget(splitter, 1);
}

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap TradeVizScreen::save_state() const {
    QVariantMap s;
    if (country_combo_)
        s.insert("country", country_combo_->currentIndex());
    if (order_combo_)
        s.insert("order", order_combo_->currentIndex());
    if (period_combo_)
        s.insert("period", period_combo_->currentIndex());
    if (year_combo_)
        s.insert("year", year_combo_->currentIndex());
    return s;
}

void TradeVizScreen::restore_state(const QVariantMap& state) {
    auto apply = [&](QComboBox* box, const char* key) {
        if (!box)
            return;
        const int v = state.value(key, -1).toInt();
        if (v >= 0 && v < box->count())
            box->setCurrentIndex(v);
    };
    apply(country_combo_, "country");
    apply(order_combo_, "order");
    apply(period_combo_, "period");
    apply(year_combo_, "year");
}

} // namespace fincept::screens
