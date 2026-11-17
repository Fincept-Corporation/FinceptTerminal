#include "screens/dashboard/widgets/WorldClockWidget.h"

#include "core/market/MarketProfile.h"
#include "core/market/MarketSessions.h"
#include "ui/formatting/DateFormat.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHelpEvent>
#include <QJsonArray>
#include <QListWidget>
#include <QPushButton>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {

namespace wc_fmt = fincept::ui::formatting;
namespace wc_mkt = fincept::core::market;

const char* const kWcWorldVenues[] = {"NYSE", "LSE", "XETRA", "TSE", "HKEX", "SSE", "NSE", "ASX"};

QString wc_countdown(qint64 secs) {
    if (secs < 0)
        secs = 0;
    const qint64 m = secs / 60;
    if (m < 60)
        return QStringLiteral("%1m").arg(std::max<qint64>(1, m));
    const qint64 h = m / 60;
    if (h < 48)
        return QStringLiteral("%1h %2m").arg(h).arg(m % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1d %2h").arg(h / 24).arg(h % 24);
}

struct WcLayout {
    int name_w = 0;
    int time_w = 0;
    int state_w = 0;
    int x0 = 0;
    int x1 = 0;
    int header_h = 0;
};

WcLayout wc_layout(const QWidget* w) {
    const QFontMetrics fm(ui::fonts::font(-3));
    const int dw = std::max(6, fm.horizontalAdvance(QLatin1Char('0')));
    WcLayout l;
    l.name_w = 12 * dw;
    l.time_w = 6 * dw;
    l.state_w = std::min(18 * dw, std::max(10 * dw, w->width() / 4));
    l.x0 = l.name_w + l.time_w + 6;
    l.x1 = std::max(l.x0 + 40, w->width() - l.state_w - 6);
    l.header_h = QFontMetrics(ui::fonts::font(-5)).height() + 4;
    return l;
}

} // namespace

// ── Timeline ─────────────────────────────────────────────────────────────────

SessionTimeline::SessionTimeline(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void SessionTimeline::set_venues(const QStringList& venue_ids) {
    venues_ = venue_ids;
    updateGeometry();
    update();
}

int SessionTimeline::row_height() const {
    return QFontMetrics(ui::fonts::font(-3)).height() + 10;
}

QSize SessionTimeline::sizeHint() const {
    const auto l = wc_layout(this);
    return {420, l.header_h + row_height() * int(venues_.size()) + 4};
}

void SessionTimeline::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const auto l = wc_layout(this);
    const int rh = row_height();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    // The user's calendar day in the display zone, as a UTC window.
    const QTimeZone zone = wc_fmt::display_zone();
    const QDateTime local_now = now.toTimeZone(zone);
    const QDateTime day_start = QDateTime(local_now.date(), QTime(0, 0), zone).toUTC();
    const double span = 86400.0;
    auto x_of = [&](const QDateTime& t) {
        return l.x0 + (double(day_start.secsTo(t)) / span) * (l.x1 - l.x0);
    };

    // Hour grid + labels (every 3 h of the user's day).
    p.setFont(ui::fonts::font(-5));
    for (int h = 0; h <= 24; h += 3) {
        const double x = l.x0 + (h / 24.0) * (l.x1 - l.x0);
        p.setPen(QColor(ui::colors::BORDER_DIM()));
        p.drawLine(QPointF(x, l.header_h), QPointF(x, height()));
        p.setPen(QColor(ui::colors::TEXT_TERTIARY()));
        if (h < 24)
            p.drawText(QRectF(x + 2, 0, 40, l.header_h), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("%1").arg(h, 2, 10, QLatin1Char('0')));
    }
    p.drawText(QRectF(0, 0, l.x0 - 4, l.header_h), Qt::AlignLeft | Qt::AlignVCenter,
               wc_fmt::zone_label(now));

    QColor reg(ui::colors::SUCCESS());
    reg.setAlphaF(0.55f);
    QColor ext(ui::colors::WARNING());
    ext.setAlphaF(0.35f);
    const QFont name_font = ui::fonts::font(-3, QFont::DemiBold);
    const QFont small_font = ui::fonts::font(-4);

    for (int i = 0; i < venues_.size(); ++i) {
        const wc_mkt::Venue* v = wc_mkt::venue_by_code(venues_[i]);
        if (!v)
            continue;
        const int y = l.header_h + i * rh;
        const QRectF track(l.x0, y + rh * 0.28, l.x1 - l.x0, rh * 0.44);
        p.fillRect(track, QColor(ui::colors::BG_RAISED()));

        const auto st = wc_mkt::session_status(*v, now);
        const QDate td = st.trading_date.isValid() ? st.trading_date : wc_mkt::trading_date(*v, now);
        for (int d = -1; d <= 1; ++d) {
            const auto ds = wc_mkt::day_schedule(*v, td.addDays(d));
            if (!ds.trading)
                continue;
            auto seg = [&](const QDateTime& a, const QDateTime& b, const QColor& c) {
                if (!a.isValid() || !b.isValid() || a >= b)
                    return;
                const double xa = std::max<double>(l.x0, x_of(a));
                const double xb = std::min<double>(l.x1, x_of(b));
                if (xb > xa)
                    p.fillRect(QRectF(xa, track.top(), xb - xa, track.height()), c);
            };
            seg(ds.pre_open_utc, ds.open_utc, ext);
            if (ds.lunch_start_utc.isValid() && ds.lunch_end_utc.isValid()) {
                seg(ds.open_utc, ds.lunch_start_utc, reg);
                seg(ds.lunch_end_utc, ds.close_utc, reg);
            } else {
                seg(ds.open_utc, ds.close_utc, reg);
            }
            seg(ds.close_utc, ds.post_close_utc, ext);
        }

        // Name, venue-local time
        p.setFont(name_font);
        p.setPen(QColor(ui::colors::TEXT_PRIMARY()));
        p.drawText(QRectF(2, y, l.name_w - 4, rh), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(name_font).elidedText(v->id, Qt::ElideRight, l.name_w - 4));
        p.setFont(small_font);
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        p.drawText(QRectF(l.name_w, y, l.time_w, rh), Qt::AlignLeft | Qt::AlignVCenter,
                   st.venue_local_now.toString(QStringLiteral("HH:mm")));

        // State + countdown
        const bool open = st.state == wc_mkt::SessionState::Open;
        const bool holiday = st.state == wc_mkt::SessionState::Holiday;
        QString state = wc_mkt::session_state_label(st.state);
        if (st.next_change_utc.isValid())
            state += QStringLiteral(" · %1 %2").arg(wc_mkt::session_state_label(st.next_state).toLower(),
                                                     wc_countdown(now.secsTo(st.next_change_utc)));
        p.setPen(QColor(open      ? ui::colors::SUCCESS()
                        : holiday ? ui::colors::WARNING()
                                  : ui::colors::TEXT_SECONDARY()));
        p.drawText(QRectF(l.x1 + 6, y, width() - l.x1 - 8, rh), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(small_font).elidedText(state, Qt::ElideRight, width() - l.x1 - 8));
    }

    // Now line
    const double xn = x_of(now);
    p.setPen(QPen(QColor(ui::colors::AMBER()), 1.5));
    p.drawLine(QPointF(xn, l.header_h - 2), QPointF(xn, height()));
}

bool SessionTimeline::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const auto l = wc_layout(this);
        const int i = (he->pos().y() - l.header_h) / std::max(1, row_height());
        if (i >= 0 && i < venues_.size()) {
            if (const wc_mkt::Venue* v = wc_mkt::venue_by_code(venues_[i])) {
                const QDateTime now = QDateTime::currentDateTimeUtc();
                const auto st = wc_mkt::session_status(*v, now);
                const auto ds = wc_mkt::day_schedule(*v, st.trading_date);
                QStringList lines{wc_mkt::venue_display_name(*v),
                                  tr("Local time %1 (%2)").arg(st.venue_local_now.toString(QStringLiteral("HH:mm")),
                                                               v->tz_id)};
                if (ds.trading)
                    lines << tr("Session %1 – %2 (your time)").arg(wc_fmt::time(ds.open_utc), wc_fmt::time(ds.close_utc));
                if (!ds.holiday_name.isEmpty())
                    lines << ds.holiday_name;
                if (ds.early_close)
                    lines << tr("Early close");
                if (wc_mkt::holiday_coverage(*v, st.trading_date.year()) == wc_mkt::HolidayCoverage::None)
                    lines << tr("No holiday calendar for this venue — a holiday shows as a trading day");
                QToolTip::showText(he->globalPos(), lines.join(QLatin1Char('\n')), this);
                return true;
            }
        }
        QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

// ── Tile ─────────────────────────────────────────────────────────────────────

WorldClockWidget::WorldClockWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("WORLD MARKET CLOCK"), parent, ui::colors::CYAN()) {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    timeline_ = new SessionTimeline(scroll);
    scroll->setWidget(timeline_);
    content_layout()->addWidget(scroll, 1);

    tick_ = new QTimer(this);
    tick_->setInterval(15'000);
    connect(tick_, &QTimer::timeout, timeline_, qOverload<>(&QWidget::update));
    connect(&wc_mkt::MarketProfileStore::instance(), &wc_mkt::MarketProfileStore::profile_changed, this, [this]() { update_rows(); });
    connect(this, &BaseWidget::refresh_requested, timeline_, qOverload<>(&QWidget::update));
    set_refreshable(false); // computed locally, nothing to fetch
    set_configurable(true);
    apply_config(cfg);
}

QStringList WorldClockWidget::effective_venues() const {
    QStringList out;
    if (!venues_.isEmpty()) {
        out = venues_;
    } else {
        out = wc_mkt::MarketProfileStore::instance().current().venues;
        for (const char* v : kWcWorldVenues)
            if (!out.contains(QLatin1String(v)))
                out << QString::fromLatin1(v);
    }
    out.erase(std::remove_if(out.begin(), out.end(), [](const QString& id) { return !wc_mkt::venue_by_code(id); }), out.end());
    return out;
}

void WorldClockWidget::update_rows() {
    timeline_->set_venues(effective_venues());
}

QJsonObject WorldClockWidget::config() const {
    return venues_.isEmpty() ? QJsonObject{} : QJsonObject{{"venues", QJsonArray::fromStringList(venues_)}};
}

void WorldClockWidget::apply_config(const QJsonObject& cfg) {
    venues_.clear();
    for (const auto& v : cfg.value("venues").toArray())
        if (!v.toString().isEmpty())
            venues_ << v.toString();
    update_rows();
}

QDialog* WorldClockWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — World Market Clock"));
    dlg->setMinimumSize(360, 460);
    auto* vl = new QVBoxLayout(dlg);
    auto* list = new QListWidget(dlg);
    const QStringList current = effective_venues();
    for (const auto& v : wc_mkt::all_venues()) {
        auto* it = new QListWidgetItem(QStringLiteral("%1 — %2").arg(v.id, wc_mkt::venue_display_name(v)), list);
        it->setData(Qt::UserRole, v.id);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(current.contains(v.id) ? Qt::Checked : Qt::Unchecked);
    }
    vl->addWidget(list, 1);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, dlg);
    box->button(QDialogButtonBox::Reset)->setText(tr("Region defaults"));
    vl->addWidget(box);
    connect(box, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    connect(box->button(QDialogButtonBox::Reset), &QPushButton::clicked, dlg, [this, dlg]() {
        venues_.clear();
        update_rows();
        emit config_changed(config());
        dlg->accept();
    });
    connect(box, &QDialogButtonBox::accepted, dlg, [this, dlg, list]() {
        QStringList picked;
        // Keep the current order for venues still picked, append new ones.
        for (const auto& id : effective_venues())
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toString() == id && list->item(i)->checkState() == Qt::Checked)
                    picked << id;
        for (int i = 0; i < list->count(); ++i) {
            const QString id = list->item(i)->data(Qt::UserRole).toString();
            if (list->item(i)->checkState() == Qt::Checked && !picked.contains(id))
                picked << id;
        }
        venues_ = picked;
        update_rows();
        emit config_changed(config());
        dlg->accept();
    });
    return dlg;
}

void WorldClockWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    tick_->start();
    timeline_->update();
}

void WorldClockWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    tick_->stop();
}

void WorldClockWidget::on_theme_changed() {
    timeline_->updateGeometry();
    timeline_->update();
}

void WorldClockWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("WORLD MARKET CLOCK"));
    timeline_->update();
}

} // namespace fincept::screens::widgets
