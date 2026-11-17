// Trade Visualization — Trade Flow style
#include "screens/trade_viz/TradeVizScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/common/CommandRows.h"
#include "services/economics/EconomicsService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadGate.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

// ── Style constants ─────────────────────────────────────────────────────────


static QString combo_ss() {
    return QString("QComboBox { background: %1; color: %2; border: 1px solid %3;"
                   "  padding: 2px 8px; font-size: " + fincept::ui::fonts::px(-3) + "px;"
                   "  min-width: 100px; }"
                   "QComboBox:hover { border-color: %4; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox::down-arrow { image: none; border-left: 4px solid transparent;"
                   "  border-right: 4px solid transparent; border-top: 5px solid %5; }"
                   "QComboBox QAbstractItemView { background: %6; color: %2;"
                   "  border: 1px solid %3; selection-background-color: %7;"
                   "  selection-color: %8; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT(),
             ui::colors::TEXT_SECONDARY(), ui::colors::BG_BASE(), ui::colors::BG_HOVER(), ui::colors::AMBER());
}

static QString table_ss() {
    return QString("QTableWidget { background: %1; color: %2; border: none;"
                   "  gridline-color: %3; font-size: " + fincept::ui::fonts::px(-1) + "px; }"
                   "QTableWidget::item { padding: 3px 10px; border-bottom: 1px solid %3; }"
                   "QTableWidget::item:selected { background: %4; color: %2; }"
                   "QHeaderView::section { background: %5; color: %6; font-size: " + fincept::ui::fonts::px(-2) + "px;"
                   "  font-weight: bold; border: none; border-bottom: 1px solid %7;"
                   "  padding: 6px 10px; }"
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
            const QFont nf = ui::fonts::font(-3, QFont::Bold);
            p.setFont(nf);
            p.setPen(QColor(ui::colors::AMBER()));
            p.drawText(QRect(10, 6, w - 20, std::max(16, QFontMetrics(nf).height() + 2)), Qt::AlignLeft | Qt::AlignTop,
                       note_);
        }

        const int n = static_cast<int>(partners_.size());
        if (n == 0) {
            p.setFont(ui::fonts::font());
            p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
            p.drawText(QRect(20, 30, w - 40, h - 60), Qt::AlignCenter | Qt::TextWordWrap, empty_message_);
            return;
        }

        const int radius = std::min(cx, cy) - 65;
        if (radius < 60)
            return;

        // ── Concentric grid rings ────────────────────────────────────────────
        QPen grid_pen(QColor(ui::colors::BORDER_DIM()), 1);
        p.setPen(grid_pen);
        for (int r = 1; r <= 5; ++r) {
            p.drawEllipse(QPointF(cx, cy), radius * r / 5.0, radius * r / 5.0);
        }

        // ── Radial spokes ────────────────────────────────────────────────────
        for (int i = 0; i < n; ++i) {
            double angle = 2.0 * M_PI * i / n - M_PI / 2.0;
            int x2 = cx + static_cast<int>((radius + 8) * std::cos(angle));
            int y2 = cy + static_cast<int>((radius + 8) * std::sin(angle));
            p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
            p.drawLine(cx, cy, x2, y2);
        }

        // Flow colours: imports in the accent, exports in a categorical magenta.
        const QColor import_col(ui::colors::AMBER());
        const QColor export_col = ui::colors::chart_color(7);

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
                // Accent colour with slight variation per strand
                QColor col = import_col.lighter(100 + s * 4);
                col.setAlpha(alpha);
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
                // Export colour, a touch lighter for weaker flows and per strand
                QColor col = export_col.lighter(100 + static_cast<int>(15 * (1.0 - strength)) + s * 3);
                col.setAlpha(alpha);
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
            p.fillRect(node, QColor(ui::colors::BG_RAISED()));
            p.setPen(QPen(QColor(ui::colors::BORDER_MED()), 1));
            p.drawRect(node);

            // Country abbreviation label — outside the circle
            double label_r = radius + 40;
            int lx = cx + static_cast<int>(label_r * std::cos(angle));
            int ly = cy + static_cast<int>(label_r * std::sin(angle));

            const QFont lf = ui::fonts::font(-2, QFont::Bold);
            p.setFont(lf);
            p.setPen(QColor(ui::colors::TEXT_PRIMARY()));
            const QFontMetrics lfm(lf);
            const int lw = std::max(64, lfm.horizontalAdvance(partners_[i].code) + 8);
            const int lh = std::max(18, lfm.height() + 2);
            QRect lr(lx - lw / 2, ly - lh / 2, lw, lh);
            p.drawText(lr, Qt::AlignCenter, partners_[i].code);
        }

        // ── Centre node — the reporting country ──────────────────────────────
        const QFont us_font = ui::fonts::font(-1, QFont::Bold);
        const QFontMetrics us_fm(us_font);
        const int us_w = std::max(32, us_fm.horizontalAdvance(center_label_) + 8);
        const int us_h = std::max(24, us_fm.height() + 6);
        QRect us_rect(cx - us_w / 2, cy - us_h / 2, us_w, us_h);
        p.fillRect(us_rect, QColor(ui::colors::BG_RAISED()));
        p.setPen(QPen(QColor(ui::colors::AMBER()), 1));
        p.drawRect(us_rect);

        p.setFont(us_font);
        p.setPen(QColor(ui::colors::AMBER()));
        p.drawText(us_rect, Qt::AlignCenter, center_label_);

        // ── Scale axis labels (values are USD millions → shown in $B) ────────
        const QFont sf = ui::fonts::font(-4);
        const QFontMetrics sfm(sf);
        p.setFont(sf);
        p.setPen(QColor(ui::colors::TEXT_DIM()));

        // Import scale — above center
        int sx = cx - 50;
        int sy = cy - radius / 5;
        const int tick_step = std::max(26, sfm.horizontalAdvance(QStringLiteral("000.0B")) + 4);
        p.drawText(sx, sy - 2, QCoreApplication::translate("TradeFlowChordWidget", "Imports ($B)"));
        for (int i = 0; i <= 4; ++i) {
            const double billions = max_val * i / 4.0 / 1000.0;
            int tx = sx + i * tick_step;
            p.drawText(tx, sy + std::max(10, sfm.ascent() + 2),
                       QString("%1B").arg(billions, 0, 'f', billions < 10.0 ? 1 : 0));
        }

        // Export scale — below center
        sy = cy + radius / 5;
        p.drawText(sx, sy + std::max(14, sfm.height() + 2), QCoreApplication::translate("TradeFlowChordWidget", "Exports ($B)"));

        // ── Color legend bar (bottom-left): page background → import colour ──
        const int bar_w = std::max(90, 3 * (sfm.horizontalAdvance(QStringLiteral("000.0B")) + 6));
        int bx = 10;
        int by = h - 18;
        const QColor bar_lo(ui::colors::BG_BASE());
        for (int i = 0; i < bar_w; ++i) {
            const double t = static_cast<double>(i) / bar_w;
            p.fillRect(bx + i, by, 1, 8,
                       QColor::fromRgbF(bar_lo.redF() + (import_col.redF() - bar_lo.redF()) * t,
                                        bar_lo.greenF() + (import_col.greenF() - bar_lo.greenF()) * t,
                                        bar_lo.blueF() + (import_col.blueF() - bar_lo.blueF()) * t));
        }
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        double lo = max_val * 0.1;
        double mid = max_val * 0.5;
        double hi = max_val;
        p.drawText(bx, by - 2, QString("%1B").arg(lo / 1000.0, 0, 'f', 1));
        p.drawText(bx + bar_w * 38 / 90, by - 2, QString("%1B").arg(mid / 1000.0, 0, 'f', 1));
        p.drawText(bx + bar_w * 72 / 90, by - 2, QString("%1B").arg(hi / 1000.0, 0, 'f', 1));
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
                "QLabel#tvTabNum { color: %3; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent;"
                " }"
                "QLabel#tvTabName { color: %4; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: bold; background: transparent;"
                " }"
                "QPushButton#tvTabExport { color: %4; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: bold;"
                " background: transparent; border: none; padding: 0;"
                " }"
                "QPushButton#tvTabExport:hover { color: %5; }"
                "QPushButton#tvTabExport:disabled { color: %6; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(), ui::colors::TEXT_DIM()));
    bar->setMinimumHeight(28);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // One cell per entry: the "NN)" function number followed by the caller's widget.
    // Only entries that do something are listed — the former "Settings" and "Notes"
    // cells were plain labels with no handler behind them.
    auto make_cell = [&](const char* num) {
        auto* cell = new QWidget(this);
        cell->setMinimumHeight(28);
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
    flow_title_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(2) + "px; font-weight: bold; background: transparent;"
                                       " padding-right: 14px;")
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
    bar->setMinimumHeight(32);
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
    browse_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: bold; background: transparent;"
                                         "")
                                     .arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(browse_label_);

    hl->addSpacing(10);

    // Order by
    order_caption_ = new QLabel(tr("Order by"));
    order_caption_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; background: transparent;"
                                          "")
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
    period_caption_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; background: transparent;"
                                           "")
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
    const QString nav_style = QString("QToolButton { color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: bold;"
                                      " background: transparent; border: none; padding: 0 4px;"
                                      " }"
                                      "QToolButton:hover { color: %2; }"
                                      "QToolButton:disabled { color: %3; }")
                                  .arg(ui::colors::TEXT_SECONDARY(), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY());
    prev_year_btn_ = new QToolButton;
    prev_year_btn_->setText(QStringLiteral("<<"));
    prev_year_btn_->setCursor(Qt::PointingHandCursor);
    prev_year_btn_->setToolTip(tr("Previous year"));
    prev_year_btn_->setStyleSheet(nav_style);
    hl->addWidget(prev_year_btn_);

    year_combo_ = new QComboBox;
    year_combo_->setStyleSheet(combo_ss());
    year_combo_->setFixedWidth(std::max(80, QFontMetrics(ui::fonts::font(-3)).horizontalAdvance(QStringLiteral("0000")) + 52));
    // Newest selectable year = last calendar year (UN Comtrade publishes annual data
    // with a lag, so the default selection is the year before that).
    const int newest_year = QDate::currentDate().year() - 1;
    for (int y = newest_year; y >= 2010; --y)
        year_combo_->addItem(QString::number(y));
    year_combo_->setCurrentIndex(year_combo_->count() > 1 ? 1 : 0);
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
    clock_label_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent;"
                                        "")
                                    .arg(ui::colors::TEXT_SECONDARY()));
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

    // Provenance above the table too: the chord's note is easy to miss, and the
    // static snapshot's numbers must never read as live data in the table.
    source_note_ = new QLabel(panel);
    source_note_->setObjectName(QStringLiteral("tvSourceNote"));
    source_note_->setWordWrap(true);
    source_note_->setTextFormat(Qt::PlainText);
    vl->addWidget(source_note_);

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
    hdr->resizeSection(0, std::max(35, QFontMetrics(ui::fonts::font(1)).horizontalAdvance(QStringLiteral("00)")) + 14));
    hdr->setSectionResizeMode(1, QHeaderView::Stretch);
    // Interactive, fitted once after each fill (rebuild_views) — never ResizeToContents
    // while the rows go in.
    for (int col = 2; col < 6; ++col)
        hdr->setSectionResizeMode(col, QHeaderView::Interactive);
    partner_table_->verticalHeader()->setDefaultSectionSize(std::max(28, QFontMetrics(ui::fonts::font(2, QFont::Bold)).height() + 8));

    vl->addWidget(partner_table_, 1);
    return panel;
}

// ============================================================================
// Dataset → views
// ============================================================================

void TradeVizScreen::show_message(const QString& note, const QString& message) {
    partners_.clear();
    illustrative_ = false;
    dataset_note_ = note;
    empty_message_ = message;
    rebuild_views();
}

void TradeVizScreen::set_dataset(const QVector<TradePartner>& all, const QString& note, bool illustrative) {
    partners_ = all;
    illustrative_ = illustrative;
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
        rank->setFont(ui::fonts::font(1));
        partner_table_->setItem(i, 0, rank);

        auto* name = new QTableWidgetItem(partner.name);
        name->setForeground(QColor(ui::colors::CYAN()));
        name->setFont(ui::fonts::font(2, QFont::Bold));
        name->setToolTip(partner.code);
        partner_table_->setItem(i, 1, name);

        auto num_item = [&](int col, double usd_millions, const QColor& colour, bool signed_value) {
            auto* item = new QTableWidgetItem(tv_money(usd_millions, signed_value));
            item->setForeground(colour);
            item->setFont(ui::fonts::font(1, col == ranked_col ? QFont::Bold : QFont::Normal));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            partner_table_->setItem(i, col, item);
        };
        num_item(2, partner.total(), primary, false);
        num_item(3, partner.imports, primary, false);
        num_item(4, partner.exports, primary, false);
        num_item(5, partner.balance(), partner.balance() >= 0.0 ? pos : neg, true);
    }
    for (int col = 2; col < 6; ++col)
        partner_table_->resizeColumnToContents(col); // once, after the fill
    partner_table_->setUpdatesEnabled(true);

    if (export_btn_) {
        export_btn_->setEnabled(!shown_.isEmpty() && !illustrative_);
        export_btn_->setToolTip(illustrative_ ? tr("Export is disabled for the illustrative static snapshot — "
                                                   "load live UN Comtrade data first")
                                              : tr("Save the partner table as a CSV file"));
    }
    if (source_note_) {
        source_note_->setText(illustrative_ ? tr("ILLUSTRATIVE — not live data. %1").arg(dataset_note_)
                                            : dataset_note_);
        source_note_->setProperty("illustrative", illustrative_);
        source_note_->style()->unpolish(source_note_);
        source_note_->style()->polish(source_note_);
    }

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
    set_dataset(rows, tr("Illustrative — US bilateral trade, 2024 estimates (static). %1").arg(why),
                /*illustrative=*/true);
}

// ============================================================================
// Live data — UN Comtrade via EconomicsService
// ============================================================================

void TradeVizScreen::refresh_data(bool automatic) {
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

    cancel_fetch(); // another country / year still loading: its answer is no longer wanted
    loading_key_ = key;
    flow_rows_.clear();
    shown_key_.clear();
    loading_->set_message(tr("Loading UN Comtrade trade flows — %1, %2…").arg(country_combo_->currentText(), year));
    body_stack_->setCurrentWidget(loading_);

    if (!automatic) {
        issue_fetch(code, year);
        return;
    }
    // The default load of the visible screen waits for a slot in the app-wide gate (two
    // automatic loads at a time; the user's own picks never wait).
    QPointer<TradeVizScreen> self(this);
    quint64 ticket = 0;
    const bool now = ui::AutoLoadGate::instance().enter(
        this,
        [self, key, code, year]() {
            if (!self)
                return;
            if (self->loading_key_ != key || !self->isVisible()) {
                self->release_gate();
                return;
            }
            self->loading_->set_message(tr("Loading UN Comtrade trade flows — %1, %2…")
                                            .arg(self->country_combo_->currentText(), year));
            self->issue_fetch(code, year);
        },
        0, &ticket);
    gate_ticket_ = ticket;
    if (now)
        issue_fetch(code, year);
    else
        loading_->set_message(tr("Queued — UN Comtrade trade flows load after the other loads running now…"));
}

void TradeVizScreen::issue_fetch(int code, const QString& year) {
    // Exports and imports are separate calls: one combined call would hit the free tier's
    // 500-row cap (~230 partners x 2 flows). EconomicsService caches both for 10 minutes and
    // answers on its own thread (never from inside execute()).
    auto& svc = services::EconomicsService::instance();
    for (const char* flow : {"X", "M"})
        svc.execute(QString::fromLatin1(kTvSourceId), QString::fromLatin1(kTvScript), QStringLiteral("trade_data"),
                    {QString::number(code), year, QString::fromLatin1(flow), QStringLiteral("TOTAL")},
                    QString("tradeviz_%1_%2_%3").arg(code).arg(year).arg(QString::fromLatin1(flow)));
}

void TradeVizScreen::cancel_fetch() {
    if (!loading_key_.isEmpty()) {
        auto& svc = services::EconomicsService::instance();
        for (const char* flow : {"X", "M"})
            svc.cancel(QStringLiteral("tradeviz_") + loading_key_ + QLatin1Char('_') + QString::fromLatin1(flow));
    }
    loading_key_.clear();
    flow_rows_.clear();
    release_gate();
}

void TradeVizScreen::release_gate() {
    if (gate_ticket_ != 0)
        ui::AutoLoadGate::instance().leave(std::exchange(gate_ticket_, 0));
}

void TradeVizScreen::show_failure(const QString& title, const QString& message, const QString& details) {
    ui::EmptyState::Spec spec = ui::EmptyState::error_spec(message, title, QString(), details);
    spec.illustration = true;
    spec.illustration_shape = ui::LoadingSkeleton::Shape::Result;
    state_->apply(spec);
    state_action_ = QStringLiteral("retry");
    body_stack_->setCurrentWidget(state_);
}

void TradeVizScreen::show_no_records(const QString& title) {
    const bool has_prev = year_combo_ && year_combo_->currentIndex() + 1 < year_combo_->count();
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("No UN Comtrade records yet"),
        tr("%1: the reporter has not published this year's partner data yet (UN Comtrade publishes with a delay).")
            .arg(title),
        has_prev ? tr("TRY %1").arg(year_combo_->itemText(year_combo_->currentIndex() + 1)) : QString());
    spec.glyph = QStringLiteral("◌");
    spec.illustration = true;
    spec.illustration_shape = ui::LoadingSkeleton::Shape::Result;
    state_->apply(spec);
    state_action_ = QStringLiteral("prev_year");
    body_stack_->setCurrentWidget(state_);
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
        cancel_fetch(); // ends the load (the other flow is stopped / ignored) and frees the gate
        LOG_WARN("TradeViz", QString("UN Comtrade fetch failed for %1: %2").arg(key, result.error));
        // Offline / rate-limited: the only offline data we have is the US snapshot.
        if (key == QStringLiteral("842_2024")) {
            show_static_fallback(tr("(live data unavailable: %1)").arg(result.error));
            body_stack_->setCurrentIndex(0);
            return;
        }
        const QString code = result.error_code;
        const QString message =
            code == QLatin1String("rate_limited")
                ? tr("UN Comtrade's free service is rate-limited right now. Wait a minute, then RETRY.")
            : code == QLatin1String("upstream_down") ? tr("UN Comtrade did not answer. RETRY in a moment.")
            : code == QLatin1String("no_data")       ? QString()
                                                     : tr("The UN Comtrade request failed.");
        if (message.isEmpty())
            show_no_records(title);
        else
            show_failure(tr("Could not load trade flows"), message, result.error);
        return;
    }

    flow_rows_.insert(parts[3], result.data.value(QStringLiteral("data")).toArray());
    if (!flow_rows_.contains(QStringLiteral("X")) || !flow_rows_.contains(QStringLiteral("M")))
        return; // wait for the other flow

    const QVector<TradePartner> all = tv_aggregate(flow_rows_.value(QStringLiteral("X")),
                                                   flow_rows_.value(QStringLiteral("M")));
    loading_key_.clear();
    flow_rows_.clear();
    release_gate();
    if (all.isEmpty()) {
        // Comtrade publishes with a lag — recent years are often missing for a reporter.
        show_no_records(title);
        return;
    }
    shown_key_ = key;
    const QDateTime at = result.fetched_at.isValid() ? result.fetched_at : QDateTime::currentDateTimeUtc();
    set_dataset(all, tr("%1 · goods, USD millions · %2%3")
                         .arg(title, ui::AutoLoadPane::as_of_text(at.toLocalTime()),
                              result.from_cache ? tr(" (cached)") : QString()));
    body_stack_->setCurrentIndex(0);
}

void TradeVizScreen::export_csv() {
    if (shown_.isEmpty() || illustrative_)
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
    refresh_data(/*automatic=*/true);
}

void TradeVizScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (clock_timer_)
        clock_timer_->stop();
    // Leaving the screen stops a load in flight; the next show starts it again.
    cancel_fetch();
}

// ============================================================================
// Constructor + UI setup
// ============================================================================

TradeVizScreen::TradeVizScreen(QWidget* parent) : QWidget(parent) {
    setup_ui();
    // Until the first live fetch lands (showEvent), the chord + table area shows its
    // loading skeleton (the static US snapshot is only the offline fallback).

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &TradeVizScreen::update_clock);

    // Replies for every economics source arrive on this one signal; on_econ_result keeps
    // only this screen's requests (source id "trade_viz").
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &TradeVizScreen::on_econ_result);

    // The partner table's text colours and fonts are set per item; rebuild them
    // (and the font-derived row height) for the new theme / font — once the appearance
    // change has settled (sizing work never runs inside a theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        if (partner_table_) {
            partner_table_->verticalHeader()->setDefaultSectionSize(
                std::max(28, QFontMetrics(ui::fonts::font(2, QFont::Bold)).height() + 8));
            partner_table_->horizontalHeader()->resizeSection(
                0, std::max(35, QFontMetrics(ui::fonts::font(1)).horizontalAdvance(QStringLiteral("00)")) + 14));
        }
        rebuild_views();
    });
}

void TradeVizScreen::setup_ui() {
    setObjectName("tradeVizScreen");
    setStyleSheet(QString("QWidget#tradeVizScreen { background: %1; }"
                          "QLabel#tvSourceNote { color: %2; background: %1; font-size: " + fincept::ui::fonts::px(-3) + "px; padding: 4px 10px;"
                          " border-bottom: 1px solid %3; }"
                          "QLabel#tvSourceNote[illustrative=\"true\"] { color: %4; font-weight: bold; }")
                      .arg(ui::colors::BG_BASE(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                           ui::colors::WARNING()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Tab bar ──────────────────────────────────────────────────────────────
    root->addWidget(build_tab_bar());

    // ── Filter bar ───────────────────────────────────────────────────────────
    root->addWidget(build_filter_bar());

    // ── Main content: chord diagram (left) + partner table (right) ───────────
    body_stack_ = new QStackedWidget(this);
    auto* splitter = new QSplitter(Qt::Horizontal, body_stack_);
    splitter->setStyleSheet(QString("QSplitter { background: %1; }"
                                    "QSplitter::handle { background: %2; width: 1px; }")
                                .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* chord = new TradeFlowChordWidget;
    chord_widget_ = chord;
    splitter->addWidget(chord);
    splitter->addWidget(build_partner_table());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    body_stack_->addWidget(splitter); // 0

    // 1 — while a country / year loads: the shape of the chord + ranking that is coming.
    loading_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Result, body_stack_);
    loading_->set_kpi_count(0);
    loading_->set_row_count(10);
    loading_->set_message(tr("Loading UN Comtrade trade flows…"));
    body_stack_->addWidget(loading_);

    // 2 — a failure (RETRY) or no records for the year (TRY the previous year).
    state_ = new ui::EmptyState(body_stack_);
    connect(state_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (state_action_ == QLatin1String("prev_year") && year_combo_ &&
            year_combo_->currentIndex() + 1 < year_combo_->count()) {
            year_combo_->setCurrentIndex(year_combo_->currentIndex() + 1); // → refresh_data()
            return;
        }
        shown_key_.clear();
        refresh_data();
    });
    body_stack_->addWidget(state_);
    body_stack_->setCurrentWidget(loading_);

    root->addWidget(body_stack_, 1);
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
    if (year_combo_) {
        s.insert("year", year_combo_->currentIndex());
        // The year list now grows each January, so restore by value, not index.
        s.insert("year_value", year_combo_->currentText());
    }
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
    if (year_combo_) {
        // "year_value" (current) or a legacy index into the old fixed 2024..2010 list.
        QString year = state.value("year_value").toString();
        if (year.isEmpty() && state.contains("year")) {
            const int legacy_idx = state.value("year").toInt();
            if (legacy_idx >= 0 && legacy_idx <= 14)
                year = QString::number(2024 - legacy_idx);
        }
        const int idx = year.isEmpty() ? -1 : year_combo_->findText(year);
        if (idx >= 0)
            year_combo_->setCurrentIndex(idx);
    }
    // "% of GDP" and the non-annual periodicities are disabled entries (nothing backs them),
    // so a selection saved while they were inert placeholders is not restored.
    if (order_combo_ && order_combo_->currentIndex() == TvGdp)
        order_combo_->setCurrentIndex(TvTotal);
    // The period combo always stays on Yearly; "period" is still written by save_state for
    // forward compatibility but deliberately not applied here.
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

bool TradeVizScreen::open_section(const QString& section, const QString& focus) {
    const QString key = section.trimmed();
    QString wanted;
    if (key.isEmpty()) {
        wanted = focus.trimmed();
        if (wanted.isEmpty())
            return true;
    } else if (!command_rows::split_kind(key, "country", &wanted) || wanted.isEmpty()) {
        return false;
    }
    // The reporter by label, name or UN code (exact, then any case).
    int idx = -1;
    for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
        for (int i = 0; i < kTvReporterCount && idx < 0; ++i) {
            const TvReporter& r = kTvReporters[i];
            if (wanted.compare(QLatin1String(r.label), cs) == 0 || wanted.compare(QLatin1String(r.name), cs) == 0 ||
                wanted == QString::number(r.code))
                idx = i;
        }
    }
    if (idx < 0)
        return key.isEmpty(); // "" + an unknown focus: nothing to do
    if (country_combo_ && country_combo_->currentIndex() != idx)
        country_combo_->setCurrentIndex(idx); // the combo pick (filters, saved state, its data load)
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> tv_command_rows() {
    namespace rows = command_rows;
    const QString where = TradeVizScreen::tr("Trade Visualization");
    QList<command::CommandItem> out;
    for (const TvReporter& r : kTvReporters) {
        const QString name = QString::fromLatin1(r.name);
        out.append(rows::section(QStringLiteral("trade_viz"), QStringLiteral("country:") + QString::fromLatin1(r.label),
                                 TradeVizScreen::tr("Trade flows: %1").arg(name),
                                 {name, QString::fromLatin1(r.label), QStringLiteral("trade partners"),
                                  QStringLiteral("imports"), QStringLiteral("exports"), QStringLiteral("comtrade")},
                                 where));
    }
    return out;
}

void tv_install_commands() {
    command_rows::install(QStringLiteral("trade_viz"), &tv_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(tv_install_commands)

} // namespace fincept::screens
