#include "SurfaceTableWidget.h"

#include "ui/theme/Theme.h"

#include <QColor>
#include <QFontMetrics>
#include <QHeaderView>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace fincept::surface {

using namespace fincept::ui;

namespace {
// Value range of a z grid for the heatmap scale. The loops this replaces were seeded
// with +/-999, so any grid whose values all sit beyond that (futures prices) never
// moved the seed and was coloured against a range that isn't in the data. Non-finite
// cells are skipped; an empty / all-NaN grid keeps the caller's 0..1 default.
void surface_table_minmax(const std::vector<std::vector<float>>& z, float& mn, float& mx) {
    float lo = std::numeric_limits<float>::infinity();
    float hi = -std::numeric_limits<float>::infinity();
    for (const auto& row : z)
        for (float v : row) {
            if (!std::isfinite(v))
                continue;
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    if (lo <= hi) {
        mn = lo;
        mx = hi;
    }
}
} // namespace

SurfaceTableWidget::SurfaceTableWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    table_ = new QTableWidget(this);
    table_->setStyleSheet(QString("QTableWidget {"
                                  "  background:%1; color:%2;"
                                  "  gridline-color:%3; border:none;"
                                  "  font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                                  "QHeaderView::section {"
                                  "  background:%4; color:%5;"
                                  "  border:none; border-right:1px solid %6;"
                                  "  border-bottom:1px solid %6;"
                                  "  padding:0 6px; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:bold; }"
                                  "QTableWidget::item {"
                                  "  padding:0 6px; border-bottom:1px solid %3; }"
                                  "QScrollBar:vertical { width:5px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%7; }"
                                  "QScrollBar::handle:vertical:hover { background:%8; }"
                                  "QScrollBar:horizontal { height:5px; background:transparent; }"
                                  "QScrollBar::handle:horizontal { background:%7; }"
                                  "QScrollBar::handle:horizontal:hover { background:%8; }")
                              .arg(colors::BG_BASE())
                              .arg(colors::TEXT_PRIMARY())
                              .arg(colors::BG_RAISED())
                              .arg(colors::BG_SURFACE())
                              .arg(colors::TEXT_DIM())
                              .arg(colors::BORDER_DIM())
                              .arg(colors::BORDER_MED())
                              .arg(colors::BORDER_BRIGHT()));
    table_->horizontalHeader()->setDefaultSectionSize(72);
    table_->horizontalHeader()->setMinimumSectionSize(48);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(true);
    layout->addWidget(table_);

    refresh_theme();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
}

namespace {
// Item-data roles of a heat-mapped cell: its position on the scale (0..1) and
// whether the scale is diverging, so the colours can be recomputed for a theme.
constexpr int kHeatPosRole = Qt::UserRole + 1;
constexpr int kHeatDivRole = Qt::UserRole + 2;
} // namespace

QColor SurfaceTableWidget::heat_color(float t, bool diverging) {
    t = std::max(0.0f, std::min(1.0f, t));
    // The theme's data scales: diverging negative → neutral → positive (t = 0.5
    // is the midpoint), sequential from the surface to the strongest magnitude.
    if (diverging)
        return colors::diverging_color(2.0 * t - 1.0);
    return colors::sequential_color(t);
}

void SurfaceTableWidget::paint_cell(QTableWidgetItem* item) {
    if (!item || !item->data(kHeatPosRole).isValid())
        return;
    const QColor bg = heat_color(item->data(kHeatPosRole).toFloat(), item->data(kHeatDivRole).toBool());
    item->setBackground(bg);
    item->setForeground(colors::contrast_text_on(bg)); // ink that reads on this cell's fill
}

void SurfaceTableWidget::apply_heatmap(float value, float min_val, float max_val, bool diverging, int row, int col) {
    float range = (max_val > min_val) ? (max_val - min_val) : 1.0f;
    float t = (value - min_val) / range;

    auto* item = table_->item(row, col);
    if (item) {
        item->setData(kHeatPosRole, t);
        item->setData(kHeatDivRole, diverging);
        paint_cell(item);
    }
}

void SurfaceTableWidget::refresh_theme() {
    const int row_h = std::max(22, QFontMetrics(fonts::font(-3)).height() + 6);
    table_->verticalHeader()->setMinimumSectionSize(row_h);
    table_->verticalHeader()->setDefaultSectionSize(row_h);
    for (int r = 0; r < table_->rowCount(); ++r)
        for (int c = 0; c < table_->columnCount(); ++c)
            paint_cell(table_->item(r, c));
}

void SurfaceTableWidget::show_generic_matrix(const std::vector<std::string>& row_labels,
                                             const std::vector<std::string>& col_labels,
                                             const std::vector<std::vector<float>>& z, float min_val, float max_val,
                                             bool diverging) {
    int nr = (int)z.size();
    int nc = z.empty() ? 0 : (int)z[0].size();

    // Drop the previous surface's cells first: a ragged row below only fills the columns
    // it has, so the rest would otherwise keep showing the old surface's values.
    table_->clearContents();
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
    float mn = 0.0f, mx = 1.0f;
    surface_table_minmax(d.z, mn, mx);

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
    float mn = 0.0f, mx = 1.0f;
    surface_table_minmax(d.z, mn, mx);
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
    const int n = (int)d.assets.size();
    // z rows are FLATTENED n×n matrices, one per time slice; the last is most
    // recent. Guard the size — a truncated slice (basket edited between the
    // generate and the render) would otherwise read past the end of the vector.
    const auto& last = d.z.back();
    if (n <= 0 || (int)last.size() < n * n)
        return;
    std::vector<std::vector<float>> mat(n, std::vector<float>(n));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            mat[i][j] = last[i * n + j];
    show_generic_matrix(d.assets, d.assets, mat, -1.0f, 1.0f, true);
}

void SurfaceTableWidget::show_yield(const YieldCurveData& d) {
    if (d.z.empty())
        return;
    float mn = 0.0f, mx = 1.0f;
    surface_table_minmax(d.z, mn, mx);
    std::vector<std::string> r_labels, c_labels;
    if (d.time_labels.size() == d.z.size()) {
        r_labels = d.time_labels; // fetched data: settlement dates
    } else {
        for (int t : d.time_points) {
            std::string s = "D";
            s += std::to_string(t);
            r_labels.push_back(std::move(s));
        }
    }
    for (int m : d.maturities) {
        std::string s = std::to_string(m);
        s += "M";
        c_labels.push_back(std::move(s));
    }
    show_generic_matrix(r_labels, c_labels, d.z, mn, mx, false);
}

void SurfaceTableWidget::show_pca(const PCAData& d) {
    if (d.z.empty())
        return;
    float mn = 0.0f, mx = 1.0f;
    surface_table_minmax(d.z, mn, mx);
    show_generic_matrix(d.assets, d.factors, d.z, mn, mx, true);
}

} // namespace fincept::surface
