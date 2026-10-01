// Trade Visualization — Trade Flow style
#include "screens/trade_viz/TradeVizScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/economics/EconomicsService.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTextStream>
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

// Reporting countries offered by the Country filter. The combo index is persisted
// (save_state), so only ever append. `code` is the UN M49 numeric code UN Comtrade
// expects as reporterCode; `label` is the short tag drawn on the chord's centre node.
struct TvReporter {
    const char* name;
    int code;
    const char* label;
};
static const TvReporter kTvReporters[] = {
    {"United States", 842, "US"}, {"China", 156, "CN"},     {"Germany", 276, "DE"},     {"Japan", 392, "JP"},
    {"United Kingdom", 826, "UK"}, {"France", 251, "FR"},   {"India", 699, "IN"},       {"Italy", 380, "IT"},
    {"Canada", 124, "CA"},         {"South Korea", 410, "KR"}, {"Mexico", 484, "MX"},   {"Brazil", 76, "BR"},
    {"Australia", 36, "AU"},       {"Netherlands", 528, "NL"}, {"Switzerland", 757, "CH"},
};
static constexpr int kTvReporterCount = static_cast<int>(sizeof(kTvReporters) / sizeof(kTvReporters[0]));

// How many partners the chord / table show.
static constexpr int kTvMaxPartners = 15;

// UN Comtrade script + request-id scheme used with EconomicsService.
static constexpr const char* kTvScript = "un_comtrade_data.py";
static constexpr const char* kTvSourceId = "trade_viz";

// "Order by" combo indices (the "% of GDP" entry is not backed by any data and is disabled).
enum TvOrder { TvTotal = 0, TvImports, TvExports, TvBalance, TvGdp };

// Static US bilateral trade snapshot (2024 estimates, $M) — the offline fallback.
struct TvStaticRow {
    const char* name;
    const char* abbrev;
    double imports;
    double exports;
};
static const TvStaticRow kTvStaticUs2024[] = {
    {"Mexico", "Mex.", 437898.0, 345098.0},     {"Canada", "Can.", 406282.0, 297329.0},
    {"China", "China", 427230.0, 177093.0},     {"Germany", "Ger.", 145632.0, 90342.5},
    {"Japan", "Jpn.", 138420.0, 94699.0},       {"South Korea", "S.Kor.", 115210.0, 88830.2},
    {"Vietnam", "Viet.", 109450.0, 42081.8},    {"United Kingdom", "UK", 67342.0, 73428.6},
    {"India", "India", 87120.0, 45012.1},       {"Ireland", "Ire.", 82340.0, 44515.1},
    {"Netherlands", "Neth.", 56120.0, 52126.2}, {"France", "Fr.", 62340.0, 45550.2},
    {"Italy", "Itl.", 67230.0, 37358.7},        {"Singapore", "Sing.", 48120.0, 51015.2},
    {"Switzerland", "Switz.", 54230.0, 38722.6},
};

// Fold UN Comtrade "X" (exports) and "M" (imports) row sets into per-partner USD-million
// totals. Real countries only: Comtrade also returns World (W00) and "nes" areas (S19, ...)
// as partners, and none of those has an ISO code of three plain letters.
static QVector<TradePartner> tv_aggregate(const QJsonArray& export_rows, const QJsonArray& import_rows) {
    QHash<QString, TradePartner> by_iso;
    auto fold = [&by_iso](const QJsonArray& rows, bool is_export) {
        for (const QJsonValue& v : rows) {
            const QJsonObject o = v.toObject();
            // Totals only: skip per-transport / per-customs-procedure / second-partner splits so a
            // partner is never counted twice.
            if (o.value(QStringLiteral("motCode")).toInt(0) != 0 ||
                o.value(QStringLiteral("partner2Code")).toInt(0) != 0)
                continue;
            const QString customs = o.value(QStringLiteral("customsCode")).toString(QStringLiteral("C00"));
            if (customs != QLatin1String("C00"))
                continue;
            const QString iso = o.value(QStringLiteral("partnerISO")).toString();
            bool alpha = iso.size() == 3;
            for (const QChar ch : iso)
                alpha = alpha && ch.isLetter();
            if (!alpha)
                continue;
            const double usd = o.value(QStringLiteral("primaryValue")).toDouble(0.0);
            if (!(usd > 0.0)) // also rejects NaN
                continue;
            TradePartner& partner = by_iso[iso];
            if (partner.code.isEmpty()) {
                partner.code = iso;
                partner.name = o.value(QStringLiteral("partnerDesc")).toString(iso);
            }
            (is_export ? partner.exports : partner.imports) += usd / 1.0e6;
        }
    };
    fold(export_rows, true);
    fold(import_rows, false);
    return by_iso.values();
}

// Order partners by the "Order by" filter, largest first (trade balance by magnitude, so the
// biggest surplus AND deficit partners both surface), and keep the top kTvMaxPartners.
static QVector<TradePartner> tv_rank(QVector<TradePartner> v, int order) {
    auto metric = [order](const TradePartner& p) {
        switch (order) {
            case TvImports:
                return p.imports;
            case TvExports:
                return p.exports;
            case TvBalance:
                return std::fabs(p.balance());
            default:
                return p.total();
        }
    };
    std::stable_sort(v.begin(), v.end(), [&metric](const TradePartner& a, const TradePartner& b) {
        const double ma = metric(a);
        const double mb = metric(b);
        if (ma != mb)
            return ma > mb;
        return a.name < b.name; // deterministic on ties
    });
    if (v.size() > kTvMaxPartners)
        v.resize(kTvMaxPartners);
    return v;
}

// USD millions with thousands grouping, e.g. 480,051.
static QString tv_money(double usd_millions, bool signed_value = false) {
    const qlonglong rounded = std::llround(usd_millions);
    const QString txt = QLocale(QLocale::English).toString(rounded); // carries its own "-" for negatives
    return (signed_value && rounded > 0 ? QStringLiteral("+") : QString()) + txt;
}

// Grey out one entry of a combo (it stays listed but cannot be picked) and say why in its tooltip.
static void tv_disable_combo_item(QComboBox* combo, int index, const QString& why) {
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    if (!model || index < 0 || index >= model->rowCount())
        return;
    if (QStandardItem* item = model->item(index)) {
        item->setEnabled(false);
        item->setToolTip(why);
    }
}

// ── Chord diagram widget ────────────────────────────────────────────────────

class TradeFlowChordWidget : public QWidget {
  public:
    explicit TradeFlowChordWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));
    }

    // Replace what is drawn. `note` is the provenance / status line (top-left); `empty_message`
    // is shown in the middle when there are no partners (loading, error, no data).
    void set_data(const QVector<TradePartner>& partners, const QString& center_label, const QString& note,
                  const QString& empty_message) {
        partners_ = partners;
        center_label_ = center_label;
        note_ = note;
        empty_message_ = empty_message;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int w = width();
        const int h = height();
        const int cx = w / 2;
        const int cy = h / 2;

        // ── Provenance note ──────────────────────────────────────────────────
        // Always say where the numbers come from (UN Comtrade vs the static offline snapshot).
        {
            QFont nf(FONT, 8);
            nf.setBold(true);
            p.setFont(nf);
            p.setPen(QColor(ui::colors::AMBER()));
            p.drawText(QRect(10, 6, w - 20, 16), Qt::AlignLeft | Qt::AlignTop, note_);
        }

        const int n = static_cast<int>(partners_.size());
        if (n == 0) {
            QFont mf(FONT, 11);
            p.setFont(mf);
            p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
            p.drawText(QRect(20, 30, w - 40, h - 60), Qt::AlignCenter | Qt::TextWordWrap, empty_message_);
            return;
        }

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
        for (int i = 0; i < n; ++i) {
            double angle = 2.0 * M_PI * i / n - M_PI / 2.0;
            int x2 = cx + static_cast<int>((radius + 8) * std::cos(angle));
            int y2 = cy + static_cast<int>((radius + 8) * std::sin(angle));
            p.setPen(QPen(QColor(22, 22, 22), 1));
            p.drawLine(cx, cy, x2, y2);
        }

        // ── Normalization ────────────────────────────────────────────────────
        double max_val = 0;
        for (int i = 0; i < n; ++i) {
            max_val = std::max(max_val, std::max(partners_[i].imports, partners_[i].exports));
        }
        if (max_val < 1.0)
            max_val = 1.0;

        // ── Import arcs (partner → reporter centre) — amber/orange bundles ───
        // Draw back-to-front so top partners render on top
        for (int i = n - 1; i >= 0; --i) {
            double angle = 2.0 * M_PI * i / n - M_PI / 2.0;
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

            double strength = partners_[i].imports / max_val;

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

        // ── Export arcs (reporter → partner) — magenta/purple bundles (top 5) ─
        for (int i = std::min(5, n) - 1; i >= 0; --i) {
            double angle = 2.0 * M_PI * i / n - M_PI / 2.0;
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

            double strength = partners_[i].exports / max_val;

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
        for (int i = 0; i < n; ++i) {
            double angle = 2.0 * M_PI * i / n - M_PI / 2.0;
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
            p.drawText(lr, Qt::AlignCenter, partners_[i].code);
        }

        // ── Centre node — the reporting country ──────────────────────────────
        QRect us_rect(cx - 16, cy - 12, 32, 24);
        p.fillRect(us_rect, QColor(30, 30, 30));
        p.setPen(QPen(QColor(ui::colors::AMBER()), 1));
        p.drawRect(us_rect);

        QFont us_font(FONT, 10);
        us_font.setBold(true);
        p.setFont(us_font);
        p.setPen(QColor(ui::colors::AMBER()));
        p.drawText(us_rect, Qt::AlignCenter, center_label_);

        // ── Scale axis labels (values are USD millions → shown in $B) ────────
        QFont sf(FONT, 7);
        p.setFont(sf);
        p.setPen(QColor(ui::colors::TEXT_DIM()));

        // Import scale — above center
        int sx = cx - 50;
        int sy = cy - radius / 5;
        p.drawText(sx, sy - 2, QCoreApplication::translate("TradeFlowChordWidget", "Imports ($B)"));
        for (int i = 0; i <= 4; ++i) {
            const double billions = max_val * i / 4.0 / 1000.0;
            int tx = sx + i * 26;
            p.drawText(tx, sy + 10, QString("%1B").arg(billions, 0, 'f', billions < 10.0 ? 1 : 0));
        }

        // Export scale — below center
        sy = cy + radius / 5;
        p.drawText(sx, sy + 14, QCoreApplication::translate("TradeFlowChordWidget", "Exports ($B)"));

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
        p.drawText(bx, by - 2, QString("%1B").arg(lo / 1000.0, 0, 'f', 1));
        p.drawText(bx + 38, by - 2, QString("%1B").arg(mid / 1000.0, 0, 'f', 1));
        p.drawText(bx + 72, by - 2, QString("%1B").arg(hi / 1000.0, 0, 'f', 1));
    }

  private:
    QVector<TradePartner> partners_;
    QString center_label_;
    QString note_;
    QString empty_message_;
};

// ============================================================================
// Tab bar — "21) Table  22) Export"
// ============================================================================

QWidget* TradeVizScreen::build_tab_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("tvTabBar"));
    // One sheet for the whole bar (P7). The first rule is the bar's original bare declaration
    // block written as a selector rule (it applied to the bar and every descendant); the rest
    // style the function-number labels, the view label and the Export button by object name.
    bar->setStyleSheet(
        QString("#tvTabBar, #tvTabBar * { background: %1; border-bottom: 1px solid %2; }"
                "QLabel#tvTabNum { color: %3; font-size: 11px; background: transparent;"
                " font-family: 'Consolas','Courier New',monospace; }"
                "QLabel#tvTabName { color: %4; font-size: 13px; font-weight: bold; background: transparent;"
                " font-family: 'Consolas','Courier New',monospace; }"
                "QPushButton#tvTabExport { color: %4; font-size: 13px; font-weight: bold;"
                " background: transparent; border: none; padding: 0;"
                " font-family: 'Consolas','Courier New',monospace; }"
                "QPushButton#tvTabExport:hover { color: %5; }"
                "QPushButton#tvTabExport:disabled { color: %6; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(), ui::colors::TEXT_DIM()));
    bar->setFixedHeight(28);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // One cell per entry: the "NN)" function number followed by the caller's widget.
    // Only entries that do something are listed — the former "Settings" and "Notes"
    // cells were plain labels with no handler behind them.
    auto make_cell = [&](const char* num) {
        auto* cell = new QWidget(this);
        cell->setFixedHeight(28);
        auto* bl = new QHBoxLayout(cell);
        bl->setContentsMargins(14, 0, 14, 0);
        bl->setSpacing(4);

        auto* num_lbl = new QLabel(QString::fromLatin1(num));
        num_lbl->setObjectName(QStringLiteral("tvTabNum"));
        bl->addWidget(num_lbl);
        hl->addWidget(cell);
        return bl;
    };

    // 21) Table — the view shown below.
    {
        auto* bl = make_cell("21)");
        auto* lbl = new QLabel(tr("Table"));
        lbl->setObjectName(QStringLiteral("tvTabName"));
        bl->addWidget(lbl);
        tab_labels_.append(lbl);
    }

    // 22) Export — the partner table as CSV.
    {
        auto* bl = make_cell("22)");
        export_btn_ = new QPushButton(tr("Export"));
        export_btn_->setObjectName(QStringLiteral("tvTabExport"));
        export_btn_->setFlat(true);
        export_btn_->setCursor(Qt::PointingHandCursor);
        export_btn_->setToolTip(tr("Save the partner table as a CSV file"));
        export_btn_->setEnabled(false); // until a dataset is shown
        connect(export_btn_, &QPushButton::clicked, this, &TradeVizScreen::export_csv);
        bl->addWidget(export_btn_);
    }

    hl->addStretch();

    // Right side: "Trade Flow" title
    flow_title_ = new QLabel(tr("Trade Flow"));
    flow_title_->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold; background: transparent;"
                                       " padding-right: 14px; font-family: 'Consolas','Courier New',monospace;")
                                   .arg(ui::colors::AMBER()));
    hl->addWidget(flow_title_);

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

    // Country selector — items are country data names (selector keys), not translated.
    country_combo_ = new QComboBox;
    country_combo_->setStyleSheet(combo_ss());
    for (const TvReporter& reporter : kTvReporters)
        country_combo_->addItem(QString::fromLatin1(reporter.name));
    country_combo_->setCurrentIndex(0);
    hl->addWidget(country_combo_);

    // Browse button
    browse_label_ = new QLabel(tr("20) Browse"));
    browse_label_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;"
                                         " font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(browse_label_);

    hl->addSpacing(10);

    // Order by
    order_caption_ = new QLabel(tr("Order by"));
    order_caption_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;"
                                          " font-family: 'Consolas','Courier New',monospace;")
                                      .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(order_caption_);

    order_combo_ = new QComboBox;
    order_combo_->setStyleSheet(combo_ss());
    order_combo_->addItems({tr("Total Trade"), tr("Imports"), tr("Exports"), tr("Trade Balance"), tr("% of GDP")});
    // Ranking by % of GDP needs a GDP series that is not wired in — offer it greyed out
    // instead of a choice that silently re-sorts nothing.
    tv_disable_combo_item(order_combo_, TvGdp, tr("Not available — no GDP series is connected to this view"));
    hl->addWidget(order_combo_);

    hl->addSpacing(10);

    // Periodicity
    period_caption_ = new QLabel(tr("Periodicity"));
    period_caption_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;"
                                           " font-family: 'Consolas','Courier New',monospace;")
                                       .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(period_caption_);

    period_combo_ = new QComboBox;
    period_combo_->setStyleSheet(combo_ss());
    period_combo_->addItems({tr("Yearly"), tr("Quarterly"), tr("Monthly")});
    // UN Comtrade merchandise data is wired here at annual frequency only.
    tv_disable_combo_item(period_combo_, 1, tr("Not available — annual data only"));
    tv_disable_combo_item(period_combo_, 2, tr("Not available — annual data only"));
    hl->addWidget(period_combo_);

    hl->addSpacing(10);

    // Year navigation — "<<" steps to the previous (earlier) year, ">>" to the next. The
    // combo lists years newest-first, so earlier == higher index.
    const QString nav_style = QString("QToolButton { color: %1; font-size: 12px; font-weight: bold;"
                                      " background: transparent; border: none; padding: 0 4px;"
                                      " font-family: 'Consolas','Courier New',monospace; }"
                                      "QToolButton:hover { color: %2; }"
                                      "QToolButton:disabled { color: %3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::AMBER(), ui::colors::TEXT_DIM());
    prev_year_btn_ = new QToolButton;
    prev_year_btn_->setText(QStringLiteral("<<"));
    prev_year_btn_->setCursor(Qt::PointingHandCursor);
    prev_year_btn_->setToolTip(tr("Previous year"));
    prev_year_btn_->setStyleSheet(nav_style);
    hl->addWidget(prev_year_btn_);

    year_combo_ = new QComboBox;
    year_combo_->setStyleSheet(combo_ss());
    year_combo_->setFixedWidth(80);
    for (int y = 2024; y >= 2010; --y) {
        year_combo_->addItem(QString::number(y));
    }
    hl->addWidget(year_combo_);

    next_year_btn_ = new QToolButton;
    next_year_btn_->setText(QStringLiteral(">>"));
    next_year_btn_->setCursor(Qt::PointingHandCursor);
    next_year_btn_->setToolTip(tr("Next year"));
    next_year_btn_->setStyleSheet(nav_style);
    hl->addWidget(next_year_btn_);

    connect(prev_year_btn_, &QToolButton::clicked, this, [this]() {
        year_combo_->setCurrentIndex(std::min(year_combo_->currentIndex() + 1, year_combo_->count() - 1));
    });
    connect(next_year_btn_, &QToolButton::clicked, this,
            [this]() { year_combo_->setCurrentIndex(std::max(year_combo_->currentIndex() - 1, 0)); });
    // Grey the arrows out at the ends of the range.
    auto sync_year_nav = [this]() {
        prev_year_btn_->setEnabled(year_combo_->currentIndex() < year_combo_->count() - 1);
        next_year_btn_->setEnabled(year_combo_->currentIndex() > 0);
    };
    connect(year_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, sync_year_nav);
    sync_year_nav();

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

    // Country / year pick a different dataset (cache-aware fetch); the ordering only
    // re-sorts what is already loaded.
    connect(country_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refresh_data(); });
    connect(year_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refresh_data(); });
    connect(order_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { rebuild_views(); });

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
    partner_table_->setColumnCount(6);
    partner_table_->setHorizontalHeaderLabels({QString(), tr("Trading Partner"), tr("Total ($M)"), tr("Imports ($M)"),
                                               tr("Exports ($M)"), tr("Balance ($M)")});
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
    for (int col = 2; col < 6; ++col)
        hdr->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    partner_table_->verticalHeader()->setDefaultSectionSize(28);

    vl->addWidget(partner_table_, 1);
    return panel;
}

// ============================================================================
// Dataset → views
// ============================================================================

void TradeVizScreen::show_message(const QString& note, const QString& message) {
    partners_.clear();
    dataset_note_ = note;
    empty_message_ = message;
    rebuild_views();
}

void TradeVizScreen::set_dataset(const QVector<TradePartner>& all, const QString& note) {
    partners_ = all;
    dataset_note_ = note;
    empty_message_.clear();
    rebuild_views();
}

void TradeVizScreen::rebuild_views() {
    const int order = order_combo_ ? order_combo_->currentIndex() : static_cast<int>(TvTotal);
    // "% of GDP" is greyed out in the combo, but a restored/legacy index could still land on it.
    shown_ = tv_rank(partners_, order == TvGdp ? static_cast<int>(TvTotal) : order);

    const QColor pos(ui::colors::POSITIVE());
    const QColor neg(ui::colors::NEGATIVE());
    const QColor primary(ui::colors::TEXT_PRIMARY());
    // Bold the column the table is ranked by so the ordering is legible at a glance.
    const int ranked_col = (order == TvImports) ? 3 : (order == TvExports) ? 4 : (order == TvBalance) ? 5 : 2;

    partner_table_->setUpdatesEnabled(false);
    partner_table_->setRowCount(static_cast<int>(shown_.size()));
    for (int i = 0; i < shown_.size(); ++i) {
        const TradePartner& partner = shown_[i];

        auto* rank = new QTableWidgetItem(QString("%1)").arg(i + 1));
        rank->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        rank->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rank->setFont(QFont(FONT, 12));
        partner_table_->setItem(i, 0, rank);

        auto* name = new QTableWidgetItem(partner.name);
        name->setForeground(pos);
        name->setFont(QFont(FONT, 13, QFont::Bold));
        name->setToolTip(partner.code);
        partner_table_->setItem(i, 1, name);

        auto num_item = [&](int col, double usd_millions, const QColor& colour, bool signed_value) {
            auto* item = new QTableWidgetItem(tv_money(usd_millions, signed_value));
            item->setForeground(colour);
            item->setFont(QFont(FONT, 12, col == ranked_col ? QFont::Bold : QFont::Normal));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            partner_table_->setItem(i, col, item);
        };
        num_item(2, partner.total(), primary, false);
        num_item(3, partner.imports, primary, false);
        num_item(4, partner.exports, primary, false);
        num_item(5, partner.balance(), partner.balance() >= 0.0 ? pos : neg, true);
    }
    partner_table_->setUpdatesEnabled(true);

    if (export_btn_)
        export_btn_->setEnabled(!shown_.isEmpty());

    if (auto* chord = static_cast<TradeFlowChordWidget*>(chord_widget_.data())) {
        const int idx = country_combo_ ? country_combo_->currentIndex() : 0;
        const QString label = (idx >= 0 && idx < kTvReporterCount) ? QString::fromLatin1(kTvReporters[idx].label)
                                                                    : QString();
        chord->set_data(shown_, label, dataset_note_, empty_message_);
    }
}

void TradeVizScreen::show_static_fallback(const QString& why) {
    QVector<TradePartner> rows;
    rows.reserve(static_cast<qsizetype>(sizeof(kTvStaticUs2024) / sizeof(kTvStaticUs2024[0])));
    for (const TvStaticRow& r : kTvStaticUs2024) {
        TradePartner partner;
        partner.name = QString::fromLatin1(r.name);
        partner.code = QString::fromLatin1(r.abbrev);
        partner.imports = r.imports;
        partner.exports = r.exports;
        rows.append(partner);
    }
    set_dataset(rows, tr("Illustrative — US bilateral trade, 2024 estimates (static). %1").arg(why));
}

// ============================================================================
// Live data — UN Comtrade via EconomicsService
// ============================================================================

void TradeVizScreen::refresh_data() {
    // Not shown yet (a restored selection fires the combo signals during construction): the
    // first showEvent fetches, so a screen nobody opens never spawns a Python process (P2).
    if (!isVisible() || !country_combo_ || !year_combo_)
        return;
    const int idx = country_combo_->currentIndex();
    if (idx < 0 || idx >= kTvReporterCount)
        return;

    const int code = kTvReporters[idx].code;
    const QString year = year_combo_->currentText();
    const QString key = QString("%1_%2").arg(code).arg(year);
    if (key == shown_key_ || key == loading_key_)
        return; // already on screen / already asked for

    loading_key_ = key;
    flow_rows_.clear();
    shown_key_.clear();
    show_message(tr("UN Comtrade — %1 — %2").arg(country_combo_->currentText(), year),
                 tr("Loading UN Comtrade trade flows…"));

    // Exports and imports are separate calls: one combined call would hit the free tier's
    // 500-row cap (~230 partners x 2 flows). EconomicsService caches both for 10 minutes and
    // may answer synchronously on a hit, so all state above is set BEFORE the first execute().
    auto& svc = services::EconomicsService::instance();
    for (const char* flow : {"X", "M"}) {
        svc.execute(QString::fromLatin1(kTvSourceId), QString::fromLatin1(kTvScript), QStringLiteral("trade_data"),
                    {QString::number(code), year, QString::fromLatin1(flow), QStringLiteral("TOTAL")},
                    QString("tradeviz_%1_%2_%3").arg(code).arg(year).arg(QString::fromLatin1(flow)));
        if (loading_key_ != key)
            return; // a synchronous failure already ended this load
    }
}

void TradeVizScreen::on_econ_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != QLatin1String(kTvSourceId))
        return;
    // request id: tradeviz_<un code>_<year>_<flow>
    const QStringList parts = request_id.split(QLatin1Char('_'));
    if (parts.size() != 4 || parts[0] != QLatin1String("tradeviz"))
        return;
    const QString key = parts[1] + QLatin1Char('_') + parts[2];
    if (key != loading_key_)
        return; // a reply for a country/year the user has already moved on from

    const QString title = tr("UN Comtrade — %1 — %2").arg(country_combo_->currentText(), year_combo_->currentText());

    if (!result.success) {
        loading_key_.clear(); // ends the load; the other flow's reply is now ignored as stale
        LOG_WARN("TradeViz", QString("UN Comtrade fetch failed for %1: %2").arg(key, result.error));
        // Offline / rate-limited: the only offline data we have is the US snapshot.
        if (key == QStringLiteral("842_2024"))
            show_static_fallback(tr("(live data unavailable: %1)").arg(result.error));
        else
            show_message(title, tr("Could not load UN Comtrade data:\n%1").arg(result.error));
        return;
    }

    flow_rows_.insert(parts[3], result.data.value(QStringLiteral("data")).toArray());
    if (!flow_rows_.contains(QStringLiteral("X")) || !flow_rows_.contains(QStringLiteral("M")))
        return; // wait for the other flow

    const QVector<TradePartner> all = tv_aggregate(flow_rows_.value(QStringLiteral("X")),
                                                   flow_rows_.value(QStringLiteral("M")));
    loading_key_.clear();
    flow_rows_.clear();
    if (all.isEmpty()) {
        // Comtrade publishes with a lag — recent years are often missing for a reporter.
        show_message(title, tr("No UN Comtrade records for this country and year yet.\n"
                               "Try an earlier year (data is published with a delay)."));
        return;
    }
    shown_key_ = key;
    set_dataset(all, tr("%1 · goods, USD millions").arg(title));
}

void TradeVizScreen::export_csv() {
    if (shown_.isEmpty())
        return;
    const QString country = country_combo_ ? country_combo_->currentText() : QString();
    const QString year = year_combo_ ? year_combo_->currentText() : QString();
    QString suggested = QStringLiteral("trade_partners_%1_%2.csv").arg(country, year);
    suggested.replace(QLatin1Char(' '), QLatin1Char('_'));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export trade partners"), suggested,
                                                      tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export trade partners"),
                             tr("Could not write %1:\n%2").arg(path, file.errorString()));
        return;
    }
    QTextStream out(&file);
    // Partner names can contain commas ("China, Hong Kong SAR"), so quote them.
    auto quoted = [](const QString& s) -> QString {
        QString q = s;
        q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QStringLiteral("\"") + q + QStringLiteral("\"");
    };
    out << "rank,partner,code,total_usd_m,imports_usd_m,exports_usd_m,balance_usd_m\n";
    for (int i = 0; i < shown_.size(); ++i) {
        const TradePartner& p = shown_[i];
        out << (i + 1) << ',' << quoted(p.name) << ',' << quoted(p.code) << ',' << QString::number(p.total(), 'f', 2)
            << ',' << QString::number(p.imports, 'f', 2) << ',' << QString::number(p.exports, 'f', 2) << ','
            << QString::number(p.balance(), 'f', 2) << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok || file.error() != QFileDevice::NoError)
        QMessageBox::warning(this, tr("Export trade partners"),
                             tr("Writing %1 failed:\n%2").arg(path, file.errorString()));
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
    // First show (or a selection restored while hidden): load the chosen country/year. A
    // no-op when that dataset is already on screen or in flight.
    refresh_data();
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
    // Until the first live fetch lands (showEvent), draw the static US snapshot rather than
    // an empty panel (P11) — its note says it is illustrative.
    show_static_fallback(tr("Loading live data…"));

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &TradeVizScreen::update_clock);

    // Replies for every economics source arrive on this one signal; on_econ_result keeps
    // only this screen's requests (source id "trade_viz").
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &TradeVizScreen::on_econ_result);
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

    auto* chord = new TradeFlowChordWidget;
    chord_widget_ = chord;
    splitter->addWidget(chord);
    splitter->addWidget(build_partner_table());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    root->addWidget(splitter, 1);
}

// ── Live language switch ──────────────────────────────────────────────────────

void TradeVizScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void TradeVizScreen::retranslateUi() {
    // Tab labels — fixed order matches build_tab_bar().
    if (tab_labels_.size() == 1)
        tab_labels_[0]->setText(tr("Table"));
    if (export_btn_) {
        export_btn_->setText(tr("Export"));
        export_btn_->setToolTip(tr("Save the partner table as a CSV file"));
    }
    if (prev_year_btn_)
        prev_year_btn_->setToolTip(tr("Previous year"));
    if (next_year_btn_)
        next_year_btn_->setToolTip(tr("Next year"));
    if (flow_title_)
        flow_title_->setText(tr("Trade Flow"));
    if (browse_label_)
        browse_label_->setText(tr("20) Browse"));
    if (order_caption_)
        order_caption_->setText(tr("Order by"));
    if (period_caption_)
        period_caption_->setText(tr("Periodicity"));

    // Combo option labels — re-set by index (selection index drives logic, not text).
    if (order_combo_ && order_combo_->count() >= 5) {
        order_combo_->setItemText(0, tr("Total Trade"));
        order_combo_->setItemText(1, tr("Imports"));
        order_combo_->setItemText(2, tr("Exports"));
        order_combo_->setItemText(3, tr("Trade Balance"));
        order_combo_->setItemText(4, tr("% of GDP"));
        tv_disable_combo_item(order_combo_, TvGdp, tr("Not available — no GDP series is connected to this view"));
    }
    if (period_combo_ && period_combo_->count() >= 3) {
        period_combo_->setItemText(0, tr("Yearly"));
        period_combo_->setItemText(1, tr("Quarterly"));
        period_combo_->setItemText(2, tr("Monthly"));
        tv_disable_combo_item(period_combo_, 1, tr("Not available — annual data only"));
        tv_disable_combo_item(period_combo_, 2, tr("Not available — annual data only"));
    }

    if (partner_table_)
        partner_table_->setHorizontalHeaderLabels({QString(), tr("Trading Partner"), tr("Total ($M)"),
                                                   tr("Imports ($M)"), tr("Exports ($M)"), tr("Balance ($M)")});

    // Chord diagram draws translatable axis labels in paintEvent — force a repaint.
    if (chord_widget_)
        chord_widget_->update();
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
    apply(year_combo_, "year");
    // "% of GDP" and the non-annual periodicities are disabled entries (nothing backs them),
    // so a selection saved while they were inert placeholders is not restored.
    if (order_combo_ && order_combo_->currentIndex() == TvGdp)
        order_combo_->setCurrentIndex(TvTotal);
    // The period combo always stays on Yearly; "period" is still written by save_state for
    // forward compatibility but deliberately not applied here.
}

} // namespace fincept::screens
