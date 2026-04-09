#include "SurfaceTableWidget.h"

#include "ui/theme/Theme.h"

#include <QColor>
#include <QHeaderView>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace fincept::surface {

using namespace fincept::ui;

SurfaceTableWidget::SurfaceTableWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    table_ = new QTableWidget(this);
    table_->setStyleSheet(QString("QTableWidget {"
                          "  background:%1; color:%2;"
                          "  gridline-color:%3; border:none;"
                          "  font-size:11px; font-family:'Consolas','Courier New',monospace; }"
                          "QHeaderView::section {"
                          "  background:%4; color:%5;"
                          "  border:none; border-right:1px solid %6;"
                          "  border-bottom:1px solid %6;"
                          "  padding:0 6px; font-size:11px; font-weight:bold;"
                          "  font-family:'Consolas','Courier New',monospace; }"
                          "QTableWidget::item {"
                          "  padding:0 6px; border-bottom:1px solid %3; }"
                          "QScrollBar:vertical { width:5px; background:transparent; }"
                          "QScrollBar::handle:vertical { background:%7; }"
                          "QScrollBar::handle:vertical:hover { background:%8; }"
                          "QScrollBar:horizontal { height:5px; background:transparent; }"
                          "QScrollBar::handle:horizontal { background:%7; }"
                          "QScrollBar::handle:horizontal:hover { background:%8; }")
                          .arg(colors::BG_BASE).arg(colors::TEXT_PRIMARY)
                          .arg(colors::BG_RAISED).arg(colors::BG_SURFACE)
                          .arg(colors::TEXT_DIM).arg(colors::BORDER_DIM)
                          .arg(colors::BORDER_MED).arg(colors::BORDER_BRIGHT));
    table_->horizontalHeader()->setDefaultSectionSize(72);
    table_->verticalHeader()->setDefaultSectionSize(22);
    table_->horizontalHeader()->setMinimumSectionSize(48);
    table_->verticalHeader()->setMinimumSectionSize(22);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(true);
    layout->addWidget(table_);
}

QColor SurfaceTableWidget::heat_color(float t, bool diverging) {
    t = std::max(0.0f, std::min(1.0f, t));
    auto lerp = [](QColor a, QColor b, float f) -> QColor {
        return QColor((int)(a.red() + (b.red() - a.red()) * f), (int)(a.green() + (b.green() - a.green()) * f),
                      (int)(a.blue() + (b.blue() - a.blue()) * f));
    };
    if (diverging) {
        // Obsidian diverging: red → dark base → green (muted, functional)
        static const QColor stops[] = {
            {127, 29, 29}, // deep red
            {153, 27, 27}, // red
            {14, 14, 14},  // base (neutral)
            {14, 14, 14},  // base (neutral)
            {20, 83, 45},  // dark green
            {22, 101, 52}, // green
        };
        float idx = t * 5.0f;
        int lo = std::max(0, std::min(4, (int)idx));
        return lerp(stops[lo], stops[lo + 1], idx - (float)lo);
    }
    // Sequential: dark base → amber → light (Obsidian brand colors)
    static const QColor stops[] = {
        {8, 8, 8},      // BG_BASE
        {28, 18, 4},    // very dark amber
        {78, 52, 10},   // dark amber
        {120, 80, 15},  // amber-brown
        {180, 110, 20}, // amber
        {217, 119, 6},  // AMBER
        {229, 160, 60}, // light amber
    };
    float idx = t * 6.0f;
    int lo = std::max(0, std::min(5, (int)idx));
    return lerp(stops[lo], stops[lo + 1], idx - (float)lo);
}

void SurfaceTableWidget::apply_heatmap(float value, float min_val, float max_val, bool diverging, int row, int col) {
    float range = (max_val > min_val) ? (max_val - min_val) : 1.0f;
    float t = (value - min_val) / range;
    QColor bg = heat_color(t, diverging);
    // Light text on dark cells; dark text only on bright amber peaks
    QColor fg = (t > 0.80f) ? QColor(20, 20, 20) : QColor(229, 229, 229);

    auto* item = table_->item(row, col);
    if (item) {
        item->setBackground(bg);
        item->setForeground(fg);
    }
}

void SurfaceTableWidget::show_generic_matrix(const std::vector<std::string>& row_labels,
                                             const std::vector<std::string>& col_labels,
                                             const std::vector<std::vector<float>>& z, float min_val, float max_val,
                                             bool diverging) {
    int nr = (int)z.size();
    int nc = z.empty() ? 0 : (int)z[0].size();

    table_->setRowCount(nr);
    table_->setColumnCount(nc);

    QStringList h_headers, v_headers;
    for (const auto& s : col_labels)
        h_headers << QString::fromStdString(s);
    for (const auto& s : row_labels)
        v_headers << QString::fromStdString(s);
    if (h_headers.size() == nc)
        table_->setHorizontalHeaderLabels(h_headers);
    if (v_headers.size() == nr)
        table_->setVerticalHeaderLabels(v_headers);

    for (int r = 0; r < nr; r++) {
        for (int c = 0; c < (int)z[r].size() && c < nc; c++) {
            float v = z[r][c];
            QString text = QString::number((double)v, 'f', 2);
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            table_->setItem(r, c, item);
            apply_heatmap(v, min_val, max_val, diverging, r, c);
        }
    }
    table_->resizeColumnsToContents();
}

void SurfaceTableWidget::show_vol(const VolatilitySurfaceData& d) {
    if (d.z.empty())
        return;
    float mn = 999, mx = -999;
    for (const auto& row : d.z)
        for (float v : row) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }

    std::vector<std::string> r_labels, c_labels;
    for (int e : d.expirations)
        r_labels.push_back(std::to_string(e) + "D");
    for (float s : d.strikes) {
        std::ostringstream ss;
        ss << "$" << (int)s;
        c_labels.push_back(ss.str());
    }
    show_generic_matrix(r_labels, c_labels, d.z, mn, mx, false);
}

void SurfaceTableWidget::show_greeks(const GreeksSurfaceData& d) {
    if (d.z.empty())
        return;
    float mn = 999, mx = -999;
    for (const auto& row : d.z)
        for (float v : row) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    bool div = (d.greek_name == "Theta");
    std::vector<std::string> r_labels, c_labels;
    for (int e : d.expirations)
        r_labels.push_back(std::to_string(e) + "D");
    for (float s : d.strikes)
        c_labels.push_back(std::to_string((int)s));
    show_generic_matrix(r_labels, c_labels, d.z, mn, mx, div);
}

void SurfaceTableWidget::show_correlation(const CorrelationMatrixData& d) {
    if (d.z.empty())
        return;
    int n = (int)d.assets.size();
    // Last time slice is most recent
    const auto& last = d.z.back();
    std::vector<std::vector<float>> mat(n, std::vector<float>(n));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            mat[i][j] = last[i * n + j];
    show_generic_matrix(d.assets, d.assets, mat, -1.0f, 1.0f, true);
}

void SurfaceTableWidget::show_yield(const YieldCurveData& d) {
    if (d.z.empty())
        return;
    float mn = 999, mx = -999;
    for (const auto& row : d.z)
        for (float v : row) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    std::vector<std::string> r_labels, c_labels;
    for (int t : d.time_points)
        r_labels.push_back("D" + std::to_string(t));
    for (int m : d.maturities)
        c_labels.push_back(std::to_string(m) + "M");
    show_generic_matrix(r_labels, c_labels, d.z, mn, mx, false);
}

void SurfaceTableWidget::show_pca(const PCAData& d) {
    if (d.z.empty())
        return;
    float mn = 999, mx = -999;
    for (const auto& row : d.z)
        for (float v : row) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    show_generic_matrix(d.assets, d.factors, d.z, mn, mx, true);
}

} // namespace fincept::surface
