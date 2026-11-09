#include "screens/dashboard/widgets/FiiDiiFlowsWidget.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

using fincept::services::options::FiiDiiDay;

const QString kFiiFlowsTopic = QStringLiteral("fno:fii_dii:daily");
const QString kFiiFlowsFnoScreen = QStringLiteral("fno");
constexpr const char* kFiiFlowsTag = "FiiDiiFlows";
constexpr const char* kFiiFlowsToneProp = "fiiFlowsTone";
constexpr const char* kFiiFlowsStaleProp = "fiiFlowsStale";
constexpr int kFiiFlowsDefaultDays = 10;
constexpr int kFiiFlowsMinDays = 5;
constexpr int kFiiFlowsMaxDays = 30; // FiiDiiService publishes a rolling 30-day window

int fii_flows_clamp_days(int days) {
    return std::clamp(days, kFiiFlowsMinDays, kFiiFlowsMaxDays);
}

/// Signed, grouped ₹ Cr figure. The feed reports a missing value as 0, so an
/// exact zero reads as "--" (same convention as the F&O FII/DII tab).
QString fii_flows_signed(double v, int decimals) {
    if (!std::isfinite(v) || v == 0.0)
        return QStringLiteral("--");
    return (v > 0 ? QStringLiteral("+") : QStringLiteral("-")) + dash_fmt::grouped(std::abs(v), decimals);
}

const char* fii_flows_tone(double v) {
    if (!std::isfinite(v) || v == 0.0)
        return "flat";
    return v > 0 ? "pos" : "neg";
}

/// Set a dynamic property the tile stylesheet selects on; re-polish only when
/// it changed (no stylesheet re-parse per delivery).
void fii_flows_set_prop(QWidget* w, const char* prop, const QString& value) {
    if (!w || w->property(prop).toString() == value)
        return;
    w->setProperty(prop, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QString fii_flows_short_date(const QString& iso) {
    const QDate d = QDate::fromString(iso, Qt::ISODate);
    return d.isValid() ? d.toString(QStringLiteral("dd MMM")) : iso;
}

QColor fii_flows_value_color(double v) {
    if (!std::isfinite(v) || v == 0.0)
        return QColor(ui::colors::TEXT_TERTIARY());
    return v > 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
}

} // namespace

// ══════════════════════════════════════════════════════════════════════════════
//  FiiDiiFlowsChart
// ══════════════════════════════════════════════════════════════════════════════

FiiDiiFlowsChart::FiiDiiFlowsChart(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void FiiDiiFlowsChart::set_days(const QVector<FiiDiiDay>& days) {
    days_ = days;
    dirty_ = true;
    update();
}

void FiiDiiFlowsChart::refresh_theme() {
    dirty_ = true;
    update();
}

void FiiDiiFlowsChart::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    dirty_ = true;
}

void FiiDiiFlowsChart::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange || e->type() == QEvent::FontChange) {
        dirty_ = true;
        update();
    }
    QWidget::changeEvent(e);
}

void FiiDiiFlowsChart::paintEvent(QPaintEvent* /*e*/) {
    // A move to a screen with another scale factor invalidates the cache too.
    if (dirty_ || cache_.devicePixelRatio() != devicePixelRatioF()) {
        rebuild_cache();
        dirty_ = false;
    }
    if (cache_.isNull())
        return;
    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void FiiDiiFlowsChart::rebuild_cache() {
    const int w = width();
    const int h = height();
    plot_width_ = 0;
    if (w <= 0 || h <= 0) {
        cache_ = QPixmap();
        return;
    }
    const qreal dpr = devicePixelRatioF();
    cache_ = QPixmap(QSize(w, h) * dpr);
    cache_.setDevicePixelRatio(dpr);
    cache_.fill(Qt::transparent);
    if (days_.isEmpty())
        return;

    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing, false);
    QFont f = font();
    f.setPixelSize(9);
    p.setFont(f);
    const QFontMetrics fm(f);

    const QString fii_name = tr("FII");
    const QString dii_name = tr("DII");
    const int gutter = std::max(fm.horizontalAdvance(fii_name), fm.horizontalAdvance(dii_name)) + 8;
    const int axis_h = fm.height() + 2;
    const int lane_gap = 4;
    const int lane_h = (h - axis_h - lane_gap) / 2;
    if (lane_h < 8 || w - gutter < 10)
        return; // too small to say anything legible

    plot_left_ = gutter;
    plot_width_ = w - gutter;

    // One scale for both lanes, so FII and DII bars compare by height.
    double max_abs = 0;
    for (const FiiDiiDay& d : days_) {
        if (std::isfinite(d.fii_net))
            max_abs = std::max(max_abs, std::abs(d.fii_net));
        if (std::isfinite(d.dii_net))
            max_abs = std::max(max_abs, std::abs(d.dii_net));
    }
    if (!(max_abs > 0))
        max_abs = 1;

    const QColor pos_color(ui::colors::POSITIVE());
    const QColor neg_color(ui::colors::NEGATIVE());
    const QColor zero_color(ui::colors::BORDER_DIM());
    const QColor text_color(ui::colors::TEXT_TERTIARY());

    const int n = static_cast<int>(days_.size());
    const double slot = static_cast<double>(plot_width_) / n;
    const int bar_w = std::max(1, static_cast<int>(slot * 0.6));

    auto draw_lane = [&](int top, const QString& name, bool fii) {
        const int mid = top + lane_h / 2;
        const int half = std::max(1, lane_h / 2 - 1);
        p.setPen(zero_color);
        p.drawLine(plot_left_, mid, w - 1, mid);
        p.setPen(text_color);
        p.drawText(QRect(0, top, gutter - 4, lane_h), Qt::AlignRight | Qt::AlignVCenter, name);
        for (int i = 0; i < n; ++i) {
            const double v = fii ? days_.at(i).fii_net : days_.at(i).dii_net;
            if (!std::isfinite(v) || v == 0.0)
                continue;
            const int bar_h = std::max(1, static_cast<int>(std::lround(std::abs(v) / max_abs * half)));
            const int x = plot_left_ + static_cast<int>(i * slot + (slot - bar_w) / 2.0);
            if (v > 0)
                p.fillRect(x, mid - bar_h, bar_w, bar_h, pos_color);
            else
                p.fillRect(x, mid + 1, bar_w, bar_h, neg_color);
        }
    };
    draw_lane(0, fii_name, /*fii=*/true);
    draw_lane(lane_h + lane_gap, dii_name, /*fii=*/false);

    // Axis row: first / last session, the shared scale in between when it fits.
    const QRect axis(plot_left_, 2 * lane_h + lane_gap, plot_width_, axis_h);
    const QString first = fii_flows_short_date(days_.first().date_iso);
    const QString last = fii_flows_short_date(days_.last().date_iso);
    const QString scale = tr("±%1 Cr").arg(dash_fmt::grouped(max_abs, 0));
    p.setPen(text_color);
    p.drawText(axis, Qt::AlignLeft | Qt::AlignVCenter, first);
    int used = fm.horizontalAdvance(first);
    if (n > 1) {
        p.drawText(axis, Qt::AlignRight | Qt::AlignVCenter, last);
        used += fm.horizontalAdvance(last);
    }
    if (used + fm.horizontalAdvance(scale) + 16 <= plot_width_)
        p.drawText(axis, Qt::AlignHCenter | Qt::AlignVCenter, scale);
}

int FiiDiiFlowsChart::index_at(int x) const {
    if (plot_width_ <= 0 || days_.isEmpty() || x < plot_left_ || x >= plot_left_ + plot_width_)
        return -1;
    const int n = static_cast<int>(days_.size());
    const int i = static_cast<int>(static_cast<qint64>(x - plot_left_) * n / plot_width_);
    return std::clamp(i, 0, n - 1);
}

bool FiiDiiFlowsChart::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(e);
        const int i = index_at(help->pos().x());
        if (i < 0) {
            QToolTip::hideText();
            e->ignore();
            return true;
        }
        const FiiDiiDay& d = days_.at(i);
        QToolTip::showText(help->globalPos(),
                           tr("%1\nFII net: %2 Cr\nDII net: %3 Cr")
                               .arg(d.date_iso, fii_flows_signed(d.fii_net, 2), fii_flows_signed(d.dii_net, 2)),
                           this);
        return true;
    }
    return QWidget::event(e);
}

// ══════════════════════════════════════════════════════════════════════════════
//  FiiDiiFlowsWidget
// ══════════════════════════════════════════════════════════════════════════════

FiiDiiFlowsWidget::FiiDiiFlowsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("FII / DII FLOWS"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    root_ = new QWidget(this);
    root_->setObjectName("fiiFlowsRoot");
    auto* rl = new QVBoxLayout(root_);
    rl->setContentsMargins(8, 6, 8, 6);
    rl->setSpacing(4);

    // Window totals. Each caption/value pair sits alone in its own container so
    // it reads as one "FII NET: -12,345" line (M1).
    auto* sums = new QHBoxLayout;
    sums->setContentsMargins(0, 0, 0, 0);
    sums->setSpacing(12);
    auto make_sum = [this, sums](QLabel*& caption, QLabel*& value) {
        auto* box = new QWidget(root_);
        auto* bl = new QHBoxLayout(box);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(6);
        caption = new QLabel(box);
        caption->setObjectName("fiiFlowsCaption");
        value = new QLabel(QStringLiteral("--"), box);
        value->setObjectName("fiiFlowsValue");
        bl->addWidget(caption);
        bl->addWidget(value);
        bl->addStretch(1);
        sums->addWidget(box, 1);
    };
    make_sum(fii_caption_, fii_value_);
    make_sum(dii_caption_, dii_value_);
    rl->addLayout(sums);

    chart_ = new FiiDiiFlowsChart(root_);
    chart_->setMinimumHeight(56);
    rl->addWidget(chart_, 2);

    table_ = new QTableWidget(0, 3, root_);
    table_->setObjectName("fiiFlowsTable");
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(20);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    rl->addWidget(table_, 3);

    footer_label_ = new QLabel(root_);
    footer_label_->setObjectName("fiiFlowsFooter");
    footer_label_->setWordWrap(true);
    rl->addWidget(footer_label_);

    vl->addWidget(root_, 1);

    // Title-bar refresh: force a fetch (bypasses the 30 min min_interval; the
    // producer's 1 req/s cap still applies).
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (!have_data_ || is_showing_error())
            set_loading(true);
        datahub::DataHub::instance().request(kFiiFlowsTopic, /*force=*/true);
    });

    set_configurable(true);
    set_title(tr("FII / DII FLOWS"));
    apply_texts();
    apply_styles();
    apply_config(cfg);
    set_loading(true);
}

QJsonObject FiiDiiFlowsWidget::config() const {
    QJsonObject o;
    o.insert("days", days_);
    return o;
}

void FiiDiiFlowsWidget::apply_config(const QJsonObject& cfg) {
    const int days = fii_flows_clamp_days(cfg.value("days").toInt(kFiiFlowsDefaultDays));
    if (days == days_ && !all_days_.isEmpty())
        return;
    days_ = days;
    render();
}

// ── Hub ─────────────────────────────────────────────────────────────────────

void FiiDiiFlowsWidget::hub_subscribe() {
    auto& hub = datahub::DataHub::instance();
    QPointer<FiiDiiFlowsWidget> self = this;
    // subscribe() fires the cold-start fetch itself when nothing fresh is cached.
    hub.subscribe(this, kFiiFlowsTopic, [self](const QVariant& v) {
        if (self)
            self->on_data(v);
    });
    hub.subscribe_errors(this, kFiiFlowsTopic, [self](const QString& error) {
        if (self)
            self->on_error(error);
    });
    hub_active_ = true;

    // P11: paint the last payload even past its TTL while the refresh runs.
    if (!have_data_) {
        const QVariant cached = hub.peek_raw(kFiiFlowsTopic);
        if (cached.canConvert<QVector<FiiDiiDay>>() && !cached.value<QVector<FiiDiiDay>>().isEmpty())
            on_data(cached);
    }
}

void FiiDiiFlowsWidget::on_data(const QVariant& v) {
    if (!v.canConvert<QVector<FiiDiiDay>>())
        return;
    all_days_ = v.value<QVector<FiiDiiDay>>();
    refresh_failed_ = false;
    if (all_days_.isEmpty()) {
        have_data_ = false;
        render();
        set_error(tr("No FII / DII data yet.\nNSE publishes the day's flows after about 6 PM IST — click refresh to "
                     "retry."));
        return;
    }
    have_data_ = true;
    set_loading(false);
    schedule_render([this]() { render(); });
}

void FiiDiiFlowsWidget::on_error(const QString& error) {
    LOG_WARN(kFiiFlowsTag, QString("FII/DII refresh failed: %1").arg(error));
    if (have_data_) {
        // Last-known-good stays visible (P11); the footer flags the failure.
        refresh_failed_ = true;
        schedule_render([this]() { render(); });
        return;
    }
    set_error(tr("FII / DII feed failed: %1\nClick refresh to retry.").arg(error));
}

// ── Rendering ───────────────────────────────────────────────────────────────

void FiiDiiFlowsWidget::render() {
    const int total = static_cast<int>(all_days_.size());
    const int n = std::min(days_, total);
    const QVector<FiiDiiDay> window = all_days_.mid(total - n); // ascending, last n sessions

    double fii_sum = 0;
    double dii_sum = 0;
    for (const FiiDiiDay& d : window) {
        fii_sum += d.fii_net;
        dii_sum += d.dii_net;
    }
    fii_value_->setText(n > 0 ? fii_flows_signed(fii_sum, 0) : QStringLiteral("--"));
    dii_value_->setText(n > 0 ? fii_flows_signed(dii_sum, 0) : QStringLiteral("--"));
    fii_flows_set_prop(fii_value_, kFiiFlowsToneProp, QString::fromLatin1(fii_flows_tone(n > 0 ? fii_sum : 0.0)));
    fii_flows_set_prop(dii_value_, kFiiFlowsToneProp, QString::fromLatin1(fii_flows_tone(n > 0 ? dii_sum : 0.0)));

    chart_->set_days(window);

    // Table, newest session first. Colours ride on the items (no per-row CSS).
    table_->setUpdatesEnabled(false);
    table_->setRowCount(n);
    for (int r = 0; r < n; ++r) {
        const FiiDiiDay& d = window.at(n - 1 - r);
        auto* date = new QTableWidgetItem(fii_flows_short_date(d.date_iso));
        date->setToolTip(d.date_iso);
        date->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table_->setItem(r, 0, date);
        auto* fii = new QTableWidgetItem(fii_flows_signed(d.fii_net, 2));
        fii->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fii->setForeground(fii_flows_value_color(d.fii_net));
        table_->setItem(r, 1, fii);
        auto* dii = new QTableWidgetItem(fii_flows_signed(d.dii_net, 2));
        dii->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        dii->setForeground(fii_flows_value_color(d.dii_net));
        table_->setItem(r, 2, dii);
    }
    table_->setUpdatesEnabled(true);

    QString footer;
    if (n > 0) {
        footer = tr("Total of last %n session(s) · net = buy − sell, ₹ Cr · as of %1", nullptr, n)
                     .arg(fii_flows_short_date(window.last().date_iso));
        chart_->setAccessibleDescription(tr("Latest %1: FII net %2 Cr, DII net %3 Cr")
                                             .arg(window.last().date_iso, fii_flows_signed(window.last().fii_net, 2),
                                                  fii_flows_signed(window.last().dii_net, 2)));
    } else {
        chart_->setAccessibleDescription(QString());
    }
    if (refresh_failed_)
        footer += (footer.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("REFRESH FAILED — cached data shown");
    footer_label_->setText(footer);
    fii_flows_set_prop(footer_label_, kFiiFlowsStaleProp,
                       refresh_failed_ ? QStringLiteral("true") : QStringLiteral("false"));
}

void FiiDiiFlowsWidget::apply_texts() {
    fii_caption_->setText(tr("FII NET"));
    fii_caption_->setToolTip(tr("Foreign institutional investors — net buy (+) / sell (−) over the window, ₹ Cr"));
    dii_caption_->setText(tr("DII NET"));
    dii_caption_->setToolTip(tr("Domestic institutional investors — net buy (+) / sell (−) over the window, ₹ Cr"));
    table_->setHorizontalHeaderLabels({tr("DATE"), tr("FII NET"), tr("DII NET")});
    for (int c = 1; c < 3; ++c) {
        if (auto* item = table_->horizontalHeaderItem(c))
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    table_->setAccessibleName(tr("FII / DII daily net flows, ₹ crore"));
    chart_->setAccessibleName(tr("FII / DII net flow chart"));
    const QString open_tip = tr("Double-click to open F&O (full history in its FII/DII tab)");
    link_screen(root_, kFiiFlowsFnoScreen, open_tip);
    link_screen(table_->viewport(), kFiiFlowsFnoScreen, open_tip);
}

void FiiDiiFlowsWidget::apply_styles() {
    // One stylesheet for the whole tile; children are styled by objectName and
    // the tone/stale dynamic properties.
    root_->setStyleSheet(
        QString("#fiiFlowsRoot{background:transparent;}"
                "QLabel#fiiFlowsCaption{color:%1;font-size:9px;font-weight:700;letter-spacing:0.5px;"
                "background:transparent;}"
                "QLabel#fiiFlowsValue{color:%2;font-size:12px;font-weight:700;background:transparent;}"
                "QLabel#fiiFlowsValue[fiiFlowsTone=\"pos\"]{color:%3;}"
                "QLabel#fiiFlowsValue[fiiFlowsTone=\"neg\"]{color:%4;}"
                "QTableWidget#fiiFlowsTable{background:transparent;color:%5;border:none;font-size:10px;}"
                "QTableWidget#fiiFlowsTable::item{padding:0 4px;border-bottom:1px solid %6;}"
                "QTableWidget#fiiFlowsTable QHeaderView::section{background:%7;color:%1;border:none;"
                "border-bottom:1px solid %6;padding:2px 4px;font-size:9px;font-weight:700;}"
                "QTableWidget#fiiFlowsTable QScrollBar:vertical{width:4px;background:transparent;}"
                "QTableWidget#fiiFlowsTable QScrollBar::handle:vertical{background:%8;min-height:20px;}"
                "QTableWidget#fiiFlowsTable QScrollBar::add-line:vertical,"
                "QTableWidget#fiiFlowsTable QScrollBar::sub-line:vertical{height:0;}"
                "QLabel#fiiFlowsFooter{color:%1;font-size:9px;background:transparent;}"
                "QLabel#fiiFlowsFooter[fiiFlowsStale=\"true\"]{color:%9;}")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_PRIMARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::WARNING()));
}

void FiiDiiFlowsWidget::on_theme_changed() {
    apply_styles();
    chart_->refresh_theme();
    render(); // item colours are baked into the table items
}

void FiiDiiFlowsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("FII / DII FLOWS"));
    apply_texts();
    render();
}

// ── Visibility ──────────────────────────────────────────────────────────────

void FiiDiiFlowsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe();
}

void FiiDiiFlowsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e); // drops every hub subscription this tile owns
    hub_active_ = false;
}

// ── Config dialog ───────────────────────────────────────────────────────────

QDialog* FiiDiiFlowsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — FII / DII Flows"));
    auto* form = new QFormLayout(dlg);

    auto* days = new QSpinBox(dlg);
    days->setRange(kFiiFlowsMinDays, kFiiFlowsMaxDays);
    days->setValue(days_);
    days->setSuffix(tr(" sessions"));
    days->setAccessibleName(tr("Sessions shown"));
    form->addRow(tr("Sessions shown"), days);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, days]() {
        QJsonObject cfg;
        cfg.insert("days", days->value());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

} // namespace fincept::screens::widgets
