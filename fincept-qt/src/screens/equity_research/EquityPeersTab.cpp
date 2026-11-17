// src/screens/equity_research/EquityPeersTab.cpp
#include "screens/equity_research/EquityPeersTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchLinks.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "services/equity/EquityResearchService.h"
#include "services/markets/FxRateService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/ChartStyle.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace peers_tab_detail {

namespace pt_uf = fincept::ui::formatting;
using PD = services::equity::PeerData;

constexpr int kPeerSymbolRole = Qt::UserRole + 1;
constexpr int kPeerPercentileRole = Qt::UserRole + 2;

/// A metric of the comparison: column header, value accessor, format.
struct PeerMetric {
    const char* key;
    const char* header;
    double (*get)(const PD&);
    enum Fmt { Multiple, Percent, PercentOfHundred, Ratio, Beta } fmt;
    bool scatter;
};

double pm_pe(const PD& p) { return p.pe_ratio; }
double pm_fpe(const PD& p) { return p.forward_pe; }
double pm_pb(const PD& p) { return p.price_to_book; }
double pm_ps(const PD& p) { return p.price_to_sales; }
double pm_eve(const PD& p) { return p.ev_to_ebitda; }
double pm_peg(const PD& p) { return p.peg_ratio; }
double pm_roe(const PD& p) { return p.roe; }
double pm_roa(const PD& p) { return p.roa; }
double pm_gm(const PD& p) { return p.gross_margin; }
double pm_om(const PD& p) { return p.operating_margin; }
double pm_nm(const PD& p) { return p.profit_margin; }
double pm_rg(const PD& p) { return p.revenue_growth; }
double pm_eg(const PD& p) { return p.earnings_growth; }
double pm_de(const PD& p) { return p.debt_to_equity / 100.0; } // yfinance: percent (152 = 1.52x)
double pm_cr(const PD& p) { return p.current_ratio; }
double pm_qr(const PD& p) { return p.quick_ratio; }
double pm_dy(const PD& p) { return p.dividend_yield / 100.0; } // yfinance: percent
double pm_beta(const PD& p) { return p.beta; }
double pm_mcap(const PD& p) { return p.market_cap_usd; }

const PeerMetric kPeerMetrics[] = {
    {"pe", QT_TRANSLATE_NOOP("EquityPeersTab", "P/E"), pm_pe, PeerMetric::Multiple, true},
    {"fpe", QT_TRANSLATE_NOOP("EquityPeersTab", "FWD P/E"), pm_fpe, PeerMetric::Multiple, true},
    {"pb", QT_TRANSLATE_NOOP("EquityPeersTab", "P/B"), pm_pb, PeerMetric::Multiple, true},
    {"ps", QT_TRANSLATE_NOOP("EquityPeersTab", "P/S"), pm_ps, PeerMetric::Multiple, true},
    {"eve", QT_TRANSLATE_NOOP("EquityPeersTab", "EV/EBITDA"), pm_eve, PeerMetric::Multiple, true},
    {"peg", QT_TRANSLATE_NOOP("EquityPeersTab", "PEG"), pm_peg, PeerMetric::Multiple, false},
    {"roe", QT_TRANSLATE_NOOP("EquityPeersTab", "ROE"), pm_roe, PeerMetric::Percent, true},
    {"roa", QT_TRANSLATE_NOOP("EquityPeersTab", "ROA"), pm_roa, PeerMetric::Percent, false},
    {"gm", QT_TRANSLATE_NOOP("EquityPeersTab", "GROSS MGN"), pm_gm, PeerMetric::Percent, true},
    {"om", QT_TRANSLATE_NOOP("EquityPeersTab", "OP MGN"), pm_om, PeerMetric::Percent, true},
    {"nm", QT_TRANSLATE_NOOP("EquityPeersTab", "NET MGN"), pm_nm, PeerMetric::Percent, true},
    {"rg", QT_TRANSLATE_NOOP("EquityPeersTab", "REV GR"), pm_rg, PeerMetric::Percent, true},
    {"eg", QT_TRANSLATE_NOOP("EquityPeersTab", "EPS GR"), pm_eg, PeerMetric::Percent, false},
    {"de", QT_TRANSLATE_NOOP("EquityPeersTab", "D/E"), pm_de, PeerMetric::Ratio, true},
    {"cr", QT_TRANSLATE_NOOP("EquityPeersTab", "CURRENT"), pm_cr, PeerMetric::Ratio, false},
    {"qr", QT_TRANSLATE_NOOP("EquityPeersTab", "QUICK"), pm_qr, PeerMetric::Ratio, false},
    {"dy", QT_TRANSLATE_NOOP("EquityPeersTab", "DIV YIELD"), pm_dy, PeerMetric::Percent, true},
    {"beta", QT_TRANSLATE_NOOP("EquityPeersTab", "BETA"), pm_beta, PeerMetric::Beta, true},
};
constexpr int kPeerFixedCols = 5; // SYMBOL, NAME, MKT CAP (display currency), PRICE, CHG%

/// MKT CAP cell. The service's USD figure makes caps listed in pounds, yen and rupees
/// comparable; it is shown converted into the user's display currency ("≈" when the rate is
/// stale, "—" until it is known — never 1:1), the local figure and the rate on hover. The
/// item's sort key stays the USD figure (same order); CSV export gets the shown amount.
void pm_fill_cap(QTableWidgetItem* cap, const PD& p) {
    QStringList tip;
    if (p.market_cap > 0)
        tip << QCoreApplication::translate("EquityPeersTab", "%1 in %2")
                   .arg(equity_fmt::money(p.market_cap, p.currency), equity_fmt::Listing{p.currency, {}}.quote_major());
    if (!(p.market_cap_usd > 0)) {
        cap->setText(equity_fmt::dash());
        cap->setData(er_ui::kRawRole, QString());
        cap->setToolTip(tip.join(QLatin1Char('\n')));
        return;
    }
    const pt_uf::ConvertedMoney c = services::fx::convert_for_display(p.market_cap_usd, QStringLiteral("USD"));
    cap->setText(pt_uf::converted_money(c, pt_uf::Compact::FromSettings));
    cap->setData(er_ui::kRawRole, c.ok() ? QVariant(c.amount) : QVariant(QString()));
    if (!c.ok())
        tip << QCoreApplication::translate("EquityPeersTab", "Waiting for the USD/%1 rate").arg(pt_uf::display_currency());
    else if (c.converted)
        tip << pt_uf::fx_note(c);
    cap->setToolTip(tip.join(QLatin1Char('\n')));
}

/// A ratio yfinance did not report comes as 0; multiples must be positive to compare.
bool pm_present(double v, PeerMetric::Fmt f) {
    if (!std::isfinite(v) || v == 0.0)
        return false;
    return f == PeerMetric::Multiple ? v > 0.0 : true;
}

QString pm_text(double v, PeerMetric::Fmt f) {
    switch (f) {
        case PeerMetric::Multiple:
            return equity_fmt::multiple(v, 1);
        case PeerMetric::Percent:
            return pt_uf::fraction_pct(v, 1, false);
        case PeerMetric::Ratio:
            return pt_uf::ratio(v, 2);
        default:
            return pt_uf::number(v, 2);
    }
}

double pm_median(QVector<double> v) {
    if (v.size() < 2)
        return std::nan("");
    std::sort(v.begin(), v.end());
    const qsizetype n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

} // namespace peers_tab_detail

using namespace peers_tab_detail;

// ── Scatter ──────────────────────────────────────────────────────────────────

/// Any two peer metrics against each other, the subject highlighted. No Q_OBJECT (no
/// signals of its own), so it needs no moc.
class PeerScatterChart : public ui::PaintedChart {
  public:
    struct Point {
        QString label;
        double x = 0, y = 0;
        bool subject = false;
    };
    explicit PeerScatterChart(QWidget* parent = nullptr) : ui::PaintedChart(parent) {
        setMouseTracking(true);
        setMinimumHeight(ui::metrics::height(220));
    }
    void set_points(const QVector<Point>& pts, const QString& x_name, const QString& y_name,
                    std::function<QString(double)> fx, std::function<QString(double)> fy) {
        pts_ = pts;
        x_name_ = x_name;
        y_name_ = y_name;
        fx_ = std::move(fx);
        fy_ = std::move(fy);
        invalidate();
    }
    QSize sizeHint() const override { return {420, 260}; }

  protected:
    bool has_data() const override { return pts_.size() >= 2; }
    void paint_chart(QPainter& p, const QRectF& r) override {
        namespace cs = ui::chart_style;
        double x0 = pts_.first().x, x1 = x0, y0 = pts_.first().y, y1 = y0;
        for (const auto& pt : pts_) {
            x0 = std::min(x0, pt.x);
            x1 = std::max(x1, pt.x);
            y0 = std::min(y0, pt.y);
            y1 = std::max(y1, pt.y);
        }
        const double px = (x1 - x0) * 0.08 + 1e-9, py = (y1 - y0) * 0.1 + 1e-9;
        double xl, xh, yl, yh;
        const auto xt = cs::nice_ticks(x0 - px, x1 + px, 5, &xl, &xh);
        const auto yt = cs::nice_ticks(y0 - py, y1 + py, 5, &yl, &yh);
        const qreal left = cs::axis_label_width(yt, fy_) + 10;
        const QFontMetrics fm(cs::axis_font());
        plot_ = QRectF(r.left() + left, r.top() + fm.height(), r.width() - left - 10, r.height() - 2.5 * fm.height() - 8);
        xl_ = xl;
        xh_ = xh;
        yl_ = yl;
        yh_ = yh;
        cs::draw_value_axis(p, plot_, yt, yl, yh, Qt::Vertical, fy_);
        cs::draw_value_axis(p, plot_, xt, xl, xh, Qt::Horizontal, fx_);
        p.setFont(cs::axis_font());
        p.setPen(cs::muted_color());
        p.drawText(QRectF(plot_.left(), r.bottom() - fm.height(), plot_.width(), fm.height()), Qt::AlignCenter, x_name_);
        p.drawText(QRectF(r.left(), r.top(), r.width(), fm.height()), Qt::AlignLeft | Qt::AlignVCenter, y_name_);
        p.setRenderHint(QPainter::Antialiasing, true);
        for (int i = 0; i < pts_.size(); ++i) {
            const QPointF c = to_px(pts_[i]);
            const QColor col = pts_[i].subject ? QColor(ui::colors::AMBER()) : ui::colors::chart_color(i);
            p.setPen(QPen(col, 1.5));
            p.setBrush(ui::colors::tint_color(col, 0.5));
            const qreal rad = pts_[i].subject ? 6 : 4.5;
            p.drawEllipse(c, rad, rad);
            p.setPen(pts_[i].subject ? QColor(ui::colors::AMBER()) : cs::label_color());
            p.setFont(cs::font(-3, pts_[i].subject ? QFont::Bold : QFont::Normal));
            p.drawText(c + QPointF(rad + 3, -rad), pts_[i].label);
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        ui::PaintedChart::mouseMoveEvent(e);
        if (!chart_visible())
            return;
        for (const auto& pt : pts_) {
            const QPointF c = to_px(pt);
            if (QLineF(c, e->position()).length() <= 8) {
                show_tip(e->position().toPoint(),
                         ui::chart_style::tooltip_html(pt.label, {{x_name_, fx_(pt.x)}, {y_name_, fy_(pt.y)}}),
                         QRect(c.toPoint() - QPoint(8, 8), QSize(16, 16)));
                return;
            }
        }
        hide_tip();
    }

  private:
    QPointF to_px(const Point& pt) const {
        const double tx = xh_ > xl_ ? (pt.x - xl_) / (xh_ - xl_) : 0.5;
        const double ty = yh_ > yl_ ? (pt.y - yl_) / (yh_ - yl_) : 0.5;
        return {plot_.left() + tx * plot_.width(), plot_.bottom() - ty * plot_.height()};
    }
    QVector<Point> pts_;
    QString x_name_, y_name_;
    std::function<QString(double)> fx_, fy_;
    QRectF plot_;
    double xl_ = 0, xh_ = 1, yl_ = 0, yh_ = 1;
};

// ── Tab ──────────────────────────────────────────────────────────────────────

EquityPeersTab::EquityPeersTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::peers_loaded, this, &EquityPeersTab::on_peers_loaded);
    connect(&svc, &services::equity::EquityResearchService::suggested_peers_loaded, this,
            [this](const QString& symbol, const QStringList& peers) {
                if (symbol.compare(current_symbol_, Qt::CaseInsensitive) != 0 || !awaiting_suggested_peers_)
                    return;
                awaiting_suggested_peers_ = false;
                if (peers.isEmpty()) {
                    loading_overlay_->hide_loading();
                    status_label_->setText(tr("No same-industry peers found for %1 — type tickers and press LOAD.")
                                               .arg(symbol));
                    status_label_->show();
                    return;
                }
                peers_edit_->setText(peers.join(QStringLiteral(", ")));
                on_load_clicked();
            });
    connect(peers_edit_, &QLineEdit::textEdited, this, [this]() { awaiting_suggested_peers_ = false; });
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& f) {
                if (f.leg != QLatin1String("Peers") || f.symbol != current_symbol_)
                    return;
                loading_overlay_->hide_loading();
                status_label_->setText(tr("Could not load peer data — press LOAD to try again."));
                status_label_->setToolTip(f.message);
                status_label_->show();
            });
    // Rebuild the table / summary / scatter from the peers held: after a theme or font change
    // once it has settled (the table re-sizes), and for a currency / number-format change.
    const auto rerender = [this]() {
        if (peers_loaded_) {
            populate_table(cached_peers_);
            update_relative_summary(cached_peers_);
            update_scatter();
        }
    };
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, rerender);
    ui::formatting::on_format_changed(this, rerender);
    // The MKT CAP column is converted into the display currency: re-fill it as rates land.
    connect(&services::FxRateService::instance(), &services::FxRateService::rates_changed, this, [this]() {
        if (!peers_loaded_ || pt_uf::display_currency() == QLatin1String("USD"))
            return;
        for (int r = 0; r < peer_table_->rowCount(); ++r) {
            const QTableWidgetItem* sym = peer_table_->item(r, 0);
            QTableWidgetItem* cap = peer_table_->item(r, 2);
            if (!sym || !cap)
                continue;
            const QString s = sym->data(kPeerSymbolRole).toString();
            for (const auto& p : std::as_const(cached_peers_))
                if (p.symbol == s) {
                    pm_fill_cap(cap, p);
                    break;
                }
        }
    });
}

void EquityPeersTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    peer_table_->setRowCount(0);
    peers_loaded_ = false;
    cached_peers_.clear();
    relative_lbl_->clear();
    relative_lbl_->hide();
    update_scatter();
    if (!pending_peers_.isEmpty()) {
        // A peer list restored from the saved session wins once over discovery.
        peers_edit_->setText(pending_peers_);
        pending_peers_.clear();
        loading_overlay_->show_loading(tr("LOADING PEERS…"));
        on_load_clicked();
        return;
    }
    find_peers();
}

void EquityPeersTab::find_peers() {
    if (current_symbol_.isEmpty())
        return;
    // Same industry on the subject's own market first (BT for Vodafone, Honda for Toyota),
    // then the industry's global leaders — no hard-coded lists, no market ETFs.
    awaiting_suggested_peers_ = true;
    peers_edit_->clear();
    loading_overlay_->show_loading(tr("FINDING INDUSTRY PEERS…"));
    services::equity::EquityResearchService::instance().fetch_suggested_peers(current_symbol_, 7);
}

void EquityPeersTab::build_ui() {
    setObjectName(QStringLiteral("equityPeersTab"));
    er_ui::apply_style(this, []() {
        return QStringLiteral("#equityPeersTab { background:%1; }"
                              "QLineEdit#peersEdit { background:%2; color:%3; border:1px solid %4; padding:4px 10px;"
                              " font-size:%5px; }"
                              "QComboBox { background:%2; color:%3; border:1px solid %4; padding:2px 6px; font-size:%5px; }")
                   .arg(ui::colors::BG_BASE(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                        ui::colors::BORDER_DIM(), ui::fonts::px(-3)) +
               er_ui::stylesheet();
    });
    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    auto* hl = new QHBoxLayout;
    hl->setSpacing(8);
    peers_caption_ = new QLabel(this);
    peers_caption_->setObjectName(QStringLiteral("erKey"));
    hl->addWidget(peers_caption_);
    peers_edit_ = new QLineEdit(this);
    peers_edit_->setObjectName(QStringLiteral("peersEdit"));
    connect(peers_edit_, &QLineEdit::returnPressed, this, &EquityPeersTab::on_load_clicked);
    hl->addWidget(peers_edit_, 1);
    load_btn_ = new QPushButton(this);
    load_btn_->setObjectName(QStringLiteral("erBtn"));
    load_btn_->setCursor(Qt::PointingHandCursor);
    connect(load_btn_, &QPushButton::clicked, this, &EquityPeersTab::on_load_clicked);
    hl->addWidget(load_btn_);
    find_btn_ = new QPushButton(this);
    find_btn_->setObjectName(QStringLiteral("erBtn"));
    find_btn_->setCursor(Qt::PointingHandCursor);
    connect(find_btn_, &QPushButton::clicked, this, &EquityPeersTab::find_peers);
    hl->addWidget(find_btn_);
    export_btn_ = new QPushButton(this);
    export_btn_->setObjectName(QStringLiteral("erBtn"));
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, [this]() {
        if (peer_table_->rowCount() == 0)
            return;
        QStringList headers;
        for (int c = 0; c < peer_table_->columnCount(); ++c)
            headers << peer_table_->horizontalHeaderItem(c)->text();
        QVector<QStringList> rows;
        for (int r = 0; r < peer_table_->rowCount(); ++r) {
            QStringList row;
            for (int c = 0; c < peer_table_->columnCount(); ++c) {
                auto* it = peer_table_->item(r, c);
                const QVariant raw = it ? it->data(er_ui::kRawRole) : QVariant();
                row << (raw.isValid() && raw.metaType().id() == QMetaType::Double ? er_ui::csv_number(raw.toDouble())
                                                                                   : (it ? it->text() : QString()));
            }
            rows << row;
        }
        er_ui::export_csv(this, current_symbol_ + QStringLiteral("_peers"), headers, rows);
    });
    hl->addWidget(export_btn_);
    vl->addLayout(hl);

    status_label_ = new QLabel(this);
    status_label_->setObjectName(QStringLiteral("erNote"));
    status_label_->setWordWrap(true);
    status_label_->hide();
    vl->addWidget(status_label_);

    auto* split = new QSplitter(Qt::Vertical, this);
    peer_table_ = new QTableWidget(split);
    peer_table_->setObjectName(QStringLiteral("erTable"));
    peer_table_->setAlternatingRowColors(true);
    peer_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    peer_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    peer_table_->verticalHeader()->hide();
    ui::metrics::follow_row_height(peer_table_, 26);
    peer_table_->horizontalHeader()->setStretchLastSection(false);
    peer_table_->setSortingEnabled(true);
    peer_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(peer_table_, &QWidget::customContextMenuRequested, this, &EquityPeersTab::show_row_menu);
    connect(peer_table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QString sym = symbol_at_row(row);
        if (!sym.isEmpty() && sym != current_symbol_)
            emit research_requested(sym, false);
    });
    split->addWidget(peer_table_);

    auto* lower = new QWidget(split);
    auto* lower_l = new QHBoxLayout(lower);
    lower_l->setContentsMargins(0, 0, 0, 0);
    lower_l->setSpacing(8);
    auto* left = new QVBoxLayout;
    relative_lbl_ = new QLabel(lower);
    relative_lbl_->setObjectName(QStringLiteral("erValue"));
    relative_lbl_->setWordWrap(true);
    relative_lbl_->setTextFormat(Qt::RichText);
    relative_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    relative_lbl_->hide();
    left->addWidget(relative_lbl_);
    units_lbl_ = new QLabel(lower);
    units_lbl_->setObjectName(QStringLiteral("erNote"));
    units_lbl_->setWordWrap(true);
    left->addWidget(units_lbl_);
    left->addStretch();
    lower_l->addLayout(left, 1);
    auto* right = new QVBoxLayout;
    auto* sel = new QHBoxLayout;
    scatter_x_ = new QComboBox(lower);
    scatter_y_ = new QComboBox(lower);
    sel->addWidget(scatter_x_);
    sel->addWidget(scatter_y_);
    sel->addStretch();
    right->addLayout(sel);
    scatter_ = new PeerScatterChart(lower);
    right->addWidget(scatter_, 1);
    lower_l->addLayout(right, 1);
    connect(scatter_x_, &QComboBox::currentIndexChanged, this, [this]() { update_scatter(); });
    connect(scatter_y_, &QComboBox::currentIndexChanged, this, [this]() { update_scatter(); });
    split->addWidget(lower);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    vl->addWidget(split, 1);
    retranslateUi();
}

void EquityPeersTab::on_load_clicked() {
    if (current_symbol_.isEmpty())
        return;
    QStringList peers;
    for (const auto& p : peers_edit_->text().split(QLatin1Char(',')))
        if (!p.trimmed().isEmpty())
            peers.append(p.trimmed().toUpper());
    peers.removeDuplicates();
    peers.removeAll(current_symbol_.toUpper());
    if (peers.isEmpty()) {
        loading_overlay_->hide_loading();
        status_label_->setText(tr("Enter at least one peer ticker, then press LOAD."));
        status_label_->show();
        return;
    }
    requested_symbols_ = QStringList{current_symbol_} + peers;
    loading_overlay_->show_loading(tr("LOADING PEERS…"));
    status_label_->setText(tr("Loading peer data…"));
    status_label_->setToolTip(QString());
    status_label_->show();
    services::equity::EquityResearchService::instance().fetch_peers(current_symbol_, peers);
}

void EquityPeersTab::on_peers_loaded(QVector<services::equity::PeerData> peers) {
    for (const auto& p : peers)
        if (!requested_symbols_.contains(p.symbol, Qt::CaseInsensitive))
            return; // another request's answer (Valuation's peer medians, an older list)
    loading_overlay_->hide_loading();
    cached_peers_ = peers;
    peers_loaded_ = true;
    populate_table(peers);
    update_relative_summary(peers);
    update_scatter();
    QStringList missing;
    for (const auto& s : requested_symbols_) {
        bool found = false;
        for (const auto& p : peers)
            found = found || p.symbol.compare(s, Qt::CaseInsensitive) == 0;
        if (!found)
            missing << s;
    }
    status_label_->setText(peers.isEmpty() ? tr("No data came back for %1 — check the tickers (Yahoo form, e.g. TCS.NS).")
                                                 .arg(requested_symbols_.join(QStringLiteral(", ")))
                                           : (missing.isEmpty() ? QString() : tr("No data for: %1").arg(missing.join(QStringLiteral(", ")))));
    status_label_->setVisible(peers.isEmpty() || !missing.isEmpty());
}

void EquityPeersTab::populate_table(const QVector<services::equity::PeerData>& peers) {
    QStringList headers = {tr("SYMBOL"), tr("NAME"), pt_uf::header(tr("MKT CAP")), tr("PRICE"), tr("CHG%")};
    for (const auto& m : kPeerMetrics)
        headers << QCoreApplication::translate("EquityPeersTab", m.header);
    peer_table_->setSortingEnabled(false);
    peer_table_->clear();
    peer_table_->setColumnCount(static_cast<int>(headers.size()));
    peer_table_->setRowCount(static_cast<int>(peers.size()));
    peer_table_->setHorizontalHeaderLabels(headers);

    // Percentile of each value within the group (0 = lowest, 1 = highest) for the bars.
    auto percentile = [&](double v, double (*get)(const PD&), PeerMetric::Fmt f) {
        int below = 0, n = 0;
        for (const auto& q : peers) {
            const double x = get(q);
            if (!pm_present(x, f))
                continue;
            ++n;
            if (x < v)
                ++below;
        }
        return n > 1 ? static_cast<double>(below) / (n - 1) : std::nan("");
    };
    auto num_item = [](const QString& text, double sort, double raw) {
        auto* item = new ui::NumericTableWidgetItem(text, sort);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setData(er_ui::kRawRole, std::isfinite(raw) ? QVariant(raw) : QVariant(QString()));
        return item;
    };
    QFont bold = peer_table_->font();
    bold.setBold(true);
    for (int r = 0; r < peers.size(); ++r) {
        const auto& p = peers[r];
        const bool subject = p.symbol.compare(current_symbol_, Qt::CaseInsensitive) == 0;
        auto* sym_item = new QTableWidgetItem(p.symbol);
        sym_item->setForeground(subject ? QColor(ui::colors::AMBER()) : QColor(ui::colors::CYAN()));
        sym_item->setFont(bold);
        sym_item->setData(kPeerSymbolRole, p.symbol);
        sym_item->setToolTip(QStringLiteral("%1\n%2 · %3 · %4").arg(p.name, p.industry, p.country, p.exchange));
        peer_table_->setItem(r, 0, sym_item);
        auto* name = new QTableWidgetItem(p.name);
        name->setToolTip(p.name);
        peer_table_->setItem(r, 1, name);
        // Market caps compare in one currency (pm_fill_cap); the local figure in the tooltip.
        auto* cap = num_item(QString(), p.market_cap_usd, std::nan(""));
        pm_fill_cap(cap, p);
        peer_table_->setItem(r, 2, cap);
        peer_table_->setItem(r, 3, num_item(p.price > 0 ? equity_fmt::price(p.price, p.currency) : equity_fmt::dash(),
                                            p.price, p.price > 0 ? p.price : std::nan("")));
        auto* chg = num_item(p.change_pct != 0.0 ? pt_uf::pct(p.change_pct, 2, true) : equity_fmt::dash(), p.change_pct,
                             p.change_pct);
        if (p.change_pct != 0.0)
            chg->setForeground(QColor(p.change_pct > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        peer_table_->setItem(r, 4, chg);
        for (int m = 0; m < static_cast<int>(std::size(kPeerMetrics)); ++m) {
            const auto& mt = kPeerMetrics[m];
            const double v = mt.get(p);
            const bool has = pm_present(v, mt.fmt);
            auto* item = num_item(has ? pm_text(v, mt.fmt) : equity_fmt::dash(), has ? v : -1e300, has ? v : std::nan(""));
            item->setData(kPeerPercentileRole, has ? percentile(v, mt.get, mt.fmt) : std::nan(""));
            if (subject)
                item->setFont(bold);
            peer_table_->setItem(r, kPeerFixedCols + m, item);
        }
    }
    // Percentile bars: rank within this peer group, neutral colour (no absolute thresholds).
    for (int c = 0; c < peer_table_->columnCount(); ++c)
        peer_table_->setItemDelegateForColumn(c, nullptr);
    qDeleteAll(bar_delegates_);
    bar_delegates_.clear();
    for (int m = 0; m < static_cast<int>(std::size(kPeerMetrics)); ++m) {
        auto* d = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, peer_table_);
        d->set_value_role(kPeerPercentileRole);
        d->set_range(0.0, 1.0);
        peer_table_->setItemDelegateForColumn(kPeerFixedCols + m, d);
        bar_delegates_ << d;
    }
    peer_table_->resizeColumnsToContents();
    peer_table_->setColumnWidth(1, std::min(peer_table_->columnWidth(1), ui::metrics::width(200)));
    peer_table_->setSortingEnabled(true);
}

void EquityPeersTab::update_relative_summary(const QVector<services::equity::PeerData>& peers) {
    const PD* subj = nullptr;
    QVector<const PD*> others;
    for (const auto& p : peers) {
        if (p.symbol.compare(current_symbol_, Qt::CaseInsensitive) == 0)
            subj = &p;
        else
            others.append(&p);
    }
    if (!subj || others.size() < 2) {
        relative_lbl_->clear();
        relative_lbl_->hide();
        return;
    }
    const QString good = ui::colors::SUCCESS(), bad = ui::colors::DANGER();
    QStringList parts;
    auto med = [&](double (*get)(const PD&), PeerMetric::Fmt f) {
        QVector<double> v;
        for (const auto* p : others)
            if (pm_present(get(*p), f))
                v << get(*p);
        return pm_median(v);
    };
    // Multiples: a premium to the median reads as "dearer".
    auto multiple = [&](const QString& label, double (*get)(const PD&)) {
        const double m = med(get, PeerMetric::Multiple), own = get(*subj);
        if (!std::isfinite(m) || own <= 0.0)
            return;
        const double prem = own / m - 1.0;
        parts << tr("<b>%1</b> %2 vs %3 <span style='color:%4'>(%5)</span>")
                     .arg(label, equity_fmt::multiple(own, 1), equity_fmt::multiple(m, 1), prem > 0 ? bad : good,
                          equity_fmt::pct(prem, 0, true));
    };
    auto rate = [&](const QString& label, double (*get)(const PD&)) {
        const double m = med(get, PeerMetric::Percent), own = get(*subj);
        if (!std::isfinite(m) || own == 0.0)
            return;
        const double diff = (own - m) * 100.0;
        parts << tr("<b>%1</b> %2 vs %3 <span style='color:%4'>(%5)</span>")
                     .arg(label, equity_fmt::pct(own, 1), equity_fmt::pct(m, 1), diff >= 0 ? good : bad, pt_uf::pp(diff, 1));
    };
    multiple(tr("P/E"), pm_pe);
    multiple(tr("Fwd P/E"), pm_fpe);
    multiple(tr("EV/EBITDA"), pm_eve);
    multiple(tr("P/B"), pm_pb);
    rate(tr("ROE"), pm_roe);
    rate(tr("Net margin"), pm_nm);
    rate(tr("Rev growth"), pm_rg);

    // Implied price on the peers' median multiples (each scales the price linearly), in the
    // subject's own quote currency.
    QStringList implied;
    double sum = 0.0;
    int n = 0;
    auto implied_at = [&](const QString& label, double (*get)(const PD&)) {
        const double m = med(get, PeerMetric::Multiple), own = get(*subj);
        if (!std::isfinite(m) || own <= 0.0 || subj->price <= 0.0)
            return;
        const double px = subj->price * m / own;
        implied << QStringLiteral("%1 %2").arg(label, equity_fmt::price(px, subj->currency));
        sum += px;
        ++n;
    };
    implied_at(tr("P/E"), pm_pe);
    implied_at(tr("Fwd P/E"), pm_fpe);
    implied_at(tr("EV/EBITDA"), pm_eve);
    QString text = tr("<b>%1 vs peer median</b> (%n peer(s)): ", "", static_cast<int>(others.size()))
                       .arg(subj->symbol.toHtmlEscaped()) +
                   parts.join(QStringLiteral(" · "));
    if (n > 0) {
        const double avg = sum / n, gap = avg / subj->price - 1.0;
        text += tr("<br><b>Implied price at peer-median multiples:</b> %1 — average <b>%2</b> "
                   "<span style='color:%3'>(%4 vs %5)</span>")
                    .arg(implied.join(QStringLiteral(" · ")), equity_fmt::price(avg, subj->currency), gap >= 0 ? good : bad,
                         equity_fmt::pct(gap, 1, true), equity_fmt::price(subj->price, subj->currency));
        text += tr("<br><i>EV/EBITDA scales the price only approximately (net debt is held fixed).</i>");
    }
    relative_lbl_->setText(text);
    relative_lbl_->setVisible(!parts.isEmpty() || n > 0);
}

void EquityPeersTab::update_scatter() {
    if (!scatter_ || !scatter_x_ || scatter_x_->count() == 0)
        return;
    const int xi = scatter_x_->currentData().toInt(), yi = scatter_y_->currentData().toInt();
    auto metric = [](int i) -> const PeerMetric* {
        return i >= 0 && i < static_cast<int>(std::size(kPeerMetrics)) ? &kPeerMetrics[i] : nullptr;
    };
    const PeerMetric* mx = metric(xi);
    const PeerMetric* my = metric(yi);
    if (!mx || !my)
        return;
    QVector<PeerScatterChart::Point> pts;
    for (const auto& p : cached_peers_) {
        const double x = mx->get(p), y = my->get(p);
        if (!pm_present(x, mx->fmt) || !pm_present(y, my->fmt))
            continue;
        pts.append({p.symbol, x, y, p.symbol.compare(current_symbol_, Qt::CaseInsensitive) == 0});
    }
    const auto fx = mx->fmt, fy = my->fmt;
    scatter_->set_points(pts, QCoreApplication::translate("EquityPeersTab", mx->header),
                         QCoreApplication::translate("EquityPeersTab", my->header),
                         [fx](double v) { return pm_text(v, fx); }, [fy](double v) { return pm_text(v, fy); });
    scatter_->set_state(pts.size() >= 2 ? ui::PaintedChart::Ready : ui::PaintedChart::Empty,
                        peers_loaded_ ? tr("Fewer than two peers report both metrics") : QString());
}

QString EquityPeersTab::symbol_at_row(int row) const {
    auto* it = peer_table_->item(row, 0);
    return it ? it->data(kPeerSymbolRole).toString() : QString();
}

void EquityPeersTab::show_row_menu(const QPoint& pos) {
    auto* item = peer_table_->itemAt(pos);
    if (!item)
        return;
    const QString sym = symbol_at_row(item->row());
    if (sym.isEmpty())
        return;
    QMenu menu(this);
    if (sym != current_symbol_) {
        connect(menu.addAction(tr("Research %1 here").arg(sym)), &QAction::triggered, this,
                [this, sym]() { emit research_requested(sym, false); });
        connect(menu.addAction(tr("Price chart for %1").arg(sym)), &QAction::triggered, this,
                [this, sym]() { emit research_requested(sym, true); });
        connect(menu.addAction(tr("Remove %1 from the list").arg(sym)), &QAction::triggered, this, [this, sym]() {
            QStringList keep;
            for (const auto& p : peers_edit_->text().split(QLatin1Char(',')))
                if (p.trimmed().compare(sym, Qt::CaseInsensitive) != 0 && !p.trimmed().isEmpty())
                    keep << p.trimmed();
            peers_edit_->setText(keep.join(QStringLiteral(", ")));
            on_load_clicked();
        });
        menu.addSeparator();
    }
    equity_links::add_open_in_actions(&menu, this, sym);
    menu.addSeparator();
    connect(menu.addAction(tr("Copy symbol")), &QAction::triggered, this, [sym]() { QApplication::clipboard()->setText(sym); });
    menu.exec(peer_table_->viewport()->mapToGlobal(pos));
}

void EquityPeersTab::apply_peers(const QString& text) {
    awaiting_suggested_peers_ = false;
    peers_edit_->setText(text);
    if (current_symbol_.isEmpty()) {
        pending_peers_ = text;
        return;
    }
    on_load_clicked();
}

void EquityPeersTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityPeersTab::retranslateUi() {
    peers_caption_->setText(tr("PEERS (comma-separated):"));
    peers_edit_->setPlaceholderText(tr("e.g. BT-A.L, AAF.L — then Enter"));
    load_btn_->setText(tr("LOAD"));
    find_btn_->setText(tr("FIND INDUSTRY PEERS"));
    find_btn_->setToolTip(tr("Same industry on this company's own market first, then the industry's global leaders"));
    export_btn_->setText(tr("EXPORT CSV"));
    units_lbl_->setText(tr("Trailing ratios from Yahoo Finance. Market caps compare in your display currency (local "
                           "figure on hover); "
                           "prices stay in each listing's own currency. Bars show each company's rank within this group "
                           "(longest = highest value), not a judgement."));
    for (QComboBox* box : {scatter_x_, scatter_y_}) {
        const QSignalBlocker block(box);
        const int keep = box->currentIndex();
        box->clear();
        for (int i = 0; i < static_cast<int>(std::size(kPeerMetrics)); ++i)
            if (kPeerMetrics[i].scatter)
                box->addItem(QCoreApplication::translate("EquityPeersTab", kPeerMetrics[i].header), i);
        box->setCurrentIndex(keep >= 0 ? keep : 0);
    }
    if (scatter_x_->currentIndex() < 0 || scatter_x_->currentData().toInt() == scatter_y_->currentData().toInt()) {
        const QSignalBlocker bx(scatter_x_);
        const QSignalBlocker by(scatter_y_);
        scatter_x_->setCurrentIndex(std::max(0, scatter_x_->findData(4))); // EV/EBITDA
        scatter_y_->setCurrentIndex(std::max(0, scatter_y_->findData(6))); // ROE
    }
    if (peers_loaded_) {
        populate_table(cached_peers_);
        update_relative_summary(cached_peers_);
    }
    update_scatter();
}

QString EquityPeersTab::peers_text() const {
    return peers_edit_ ? peers_edit_->text() : QString();
}

void EquityPeersTab::set_peers_text(const QString& text) {
    pending_peers_ = text.trimmed();
    if (peers_edit_)
        peers_edit_->setText(text);
}

} // namespace fincept::screens
