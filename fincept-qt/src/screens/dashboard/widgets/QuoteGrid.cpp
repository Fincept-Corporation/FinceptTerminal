#include "screens/dashboard/widgets/QuoteGrid.h"

#include "core/market/MarketSessions.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/RangeBarDelegate.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QHeaderView>
#include <QPainter>
#include <QResizeEvent>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace qg_fmt = fincept::ui::formatting;
using fincept::services::QuoteData;

constexpr qint64 kQuoteGridFlashMs = 700;

qg_fmt::InstrumentFmt qg_instrument(const QString& sym, const QuoteData& q) {
    auto inst = qg_fmt::instrument_for_symbol(sym, q.currency);
    if (q.price_digits >= 0)
        inst.decimals = q.price_digits;
    return inst;
}

/// Session state of the row's venue: the session calendar when the venue is
/// known (holidays included), else what the quote said.
const fincept::core::market::Venue* qg_venue(const QString& sym, const QString& exchange, const QString& hint) {
    if (!hint.isEmpty())
        if (const auto* v = fincept::core::market::venue_by_code(hint))
            return v;
    return fincept::core::market::venue_for_symbol(sym, exchange);
}

/// "45m", "3h 12m", "2d 4h".
QString qg_countdown(qint64 secs) {
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

QColor qg_state_color(const QString& sym, const QuoteData& q, QString* label, const QString& hint = {}) {
    using namespace fincept::core::market;
    if (const Venue* v = qg_venue(sym, q.exchange, hint)) {
        const auto st = session_status(*v, QDateTime::currentDateTimeUtc());
        if (label)
            *label = session_state_label(st.state) + QStringLiteral(" · ") + venue_display_name(*v);
        switch (st.state) {
            case SessionState::Open:
                return QColor(ui::colors::SUCCESS());
            case SessionState::Pre:
            case SessionState::Post:
            case SessionState::Lunch:
                return QColor(ui::colors::WARNING());
            case SessionState::Closed:
            case SessionState::Holiday:
                return QColor(ui::colors::TEXT_TERTIARY());
        }
    }
    const QString s = q.market_state.toUpper();
    if (s.isEmpty())
        return {};
    if (label)
        *label = s;
    if (s == QLatin1String("REGULAR"))
        return QColor(ui::colors::SUCCESS());
    if (s == QLatin1String("PRE") || s == QLatin1String("POST"))
        return QColor(ui::colors::WARNING());
    return QColor(ui::colors::TEXT_TERTIARY());
}

void qg_set_text(QTableWidgetItem* it, const QString& text) {
    if (it && it->text() != text)
        it->setText(text);
}

QString qg_tone(double v) {
    if (!std::isfinite(v) || v == 0.0)
        return ui::colors::TEXT_SECONDARY();
    return v > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
}

} // namespace

// ── Delegate ─────────────────────────────────────────────────────────────────

void QuoteGridDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    if (idx.column() == QuoteGrid::ColName) {
        const QColor dot = idx.data(QuoteGrid::StateRole).value<QColor>();
        if (dot.isValid()) {
            const int d = std::max(6, opt.fontMetrics.height() / 3);
            const int pad = d + 6;
            QStyleOptionViewItem bg(opt);
            initStyleOption(&bg, idx);
            bg.text.clear();
            QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &bg, p, opt.widget);
            QStyleOptionViewItem t(opt);
            t.rect.adjust(pad, 0, 0, 0);
            QStyledItemDelegate::paint(p, t, idx);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            p->setBrush(dot);
            p->drawEllipse(QRectF(opt.rect.left() + 5, opt.rect.center().y() - d / 2.0 + 0.5, d, d));
            p->restore();
            return;
        }
    }
    QStyledItemDelegate::paint(p, opt, idx);
    const int col = idx.column();
    if (col != QuoteGrid::ColLast && col != QuoteGrid::ColChg && col != QuoteGrid::ColPct)
        return;
    const QModelIndex name = idx.sibling(idx.row(), QuoteGrid::ColName);
    const qint64 at = name.data(QuoteGrid::FlashTimeRole).toLongLong();
    if (at <= 0)
        return;
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - at;
    if (age < 0 || age >= kQuoteGridFlashMs)
        return;
    QColor wash = name.data(QuoteGrid::FlashUpRole).toBool() ? QColor(ui::colors::POSITIVE())
                                                             : QColor(ui::colors::NEGATIVE());
    wash.setAlphaF(static_cast<float>(0.32 * (1.0 - double(age) / double(kQuoteGridFlashMs))));
    p->fillRect(opt.rect, wash);
}

// ── Grid ─────────────────────────────────────────────────────────────────────

QuoteGrid::QuoteGrid(QWidget* parent) : ui::DataTable(parent) {
    set_headers({tr("NAME"), tr("LAST"), tr("CHG"), tr("CHG%"), tr("DAY RANGE"), tr("5D"), tr("52W RANGE"),
                 tr("VOL"), tr("SESSION")});
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    horizontalHeader()->setMinimumSectionSize(24);
    setWordWrap(false);
    setTextElideMode(Qt::ElideRight);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setItemDelegate(new QuoteGridDelegate(this));
    auto price_fmt = [](double v) { return qg_fmt::price(v); };
    auto* day = new ui::RangeBarDelegate(this);
    day->set_formatter(price_fmt);
    day->set_show_labels(false);
    setItemDelegateForColumn(ColDay, day);
    auto* wk = new ui::RangeBarDelegate(this);
    wk->set_formatter(price_fmt);
    wk->set_show_labels(false);
    setItemDelegateForColumn(Col52w, wk);
    auto* spark = new ui::SparklineDelegate(this);
    spark->set_baseline_role(BaselineRole);
    setItemDelegateForColumn(ColTrend, spark);

    flash_timer_ = new QTimer(this);
    flash_timer_->setInterval(40);
    connect(flash_timer_, &QTimer::timeout, this, &QuoteGrid::on_flash_tick);
    session_timer_ = new QTimer(this);
    session_timer_->setInterval(20'000);
    connect(session_timer_, &QTimer::timeout, this, &QuoteGrid::render_sessions);

    apply_column_visibility();
}

void QuoteGrid::set_optional_columns(int flags) {
    optional_ = flags;
    if (optional_ & OptSession)
        session_timer_->start();
    else
        session_timer_->stop();
    apply_column_visibility();
    render_sessions();
}

void QuoteGrid::set_venue_hints(const QHash<QString, QString>& hints) {
    venue_hints_ = hints;
    rerender();
}

void QuoteGrid::render_sessions() {
    if (!(optional_ & OptSession))
        return;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const auto& e : entries_) {
        const int r = row_of_.value(e.symbol, -1);
        if (r < 0)
            continue;
        const auto q = quotes_.constFind(e.symbol);
        const auto* v = qg_venue(e.symbol, q != quotes_.constEnd() ? q->exchange : QString(),
                                 venue_hints_.value(e.symbol));
        auto* it = cell(r, ColSession);
        if (!v) {
            qg_set_text(it, qg_fmt::placeholder());
            continue;
        }
        const auto st = fincept::core::market::session_status(*v, now);
        QString text = fincept::core::market::session_state_label(st.state);
        if (st.next_change_utc.isValid())
            text += QStringLiteral(" · ") + qg_countdown(now.secsTo(st.next_change_utc));
        qg_set_text(it, text);
        it->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        const bool open = st.state == fincept::core::market::SessionState::Open;
        it->setForeground(QColor(open ? ui::colors::SUCCESS() : ui::colors::TEXT_SECONDARY()));
        QString tip = fincept::core::market::venue_display_name(*v) + QStringLiteral("\n") +
                      tr("Local time %1").arg(st.venue_local_now.toString(QStringLiteral("HH:mm")));
        if (st.next_change_utc.isValid())
            tip += QStringLiteral("\n") + tr("%1 at %2")
                                              .arg(fincept::core::market::session_state_label(st.next_state),
                                                   qg_fmt::time(st.next_change_utc));
        const QString hol = fincept::core::market::holiday_name(*v, st.trading_date);
        if (!hol.isEmpty())
            tip += QStringLiteral("\n") + hol;
        it->setToolTip(tip);
    }
}

QTableWidgetItem* QuoteGrid::cell(int row, int col) {
    auto* it = item(row, col);
    if (!it) {
        it = new QTableWidgetItem;
        it->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        if (col != ColName)
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        setItem(row, col, it);
    }
    return it;
}

void QuoteGrid::set_entries(const QVector<QuoteGridEntry>& entries) {
    entries_ = entries;
    QSet<QString> keep;
    for (const auto& e : entries_)
        keep.insert(e.symbol);
    for (auto it = quotes_.begin(); it != quotes_.end();)
        it = keep.contains(it.key()) ? std::next(it) : quotes_.erase(it);
    for (auto it = sparks_.begin(); it != sparks_.end();)
        it = keep.contains(it.key()) ? std::next(it) : sparks_.erase(it);
    for (auto it = errors_.begin(); it != errors_.end();)
        it = keep.contains(it.key()) ? std::next(it) : errors_.erase(it);
    rebuild_rows();
}

void QuoteGrid::rebuild_rows() {
    const bool had_updates = updatesEnabled();
    setUpdatesEnabled(false);
    clearSpans();
    setRowCount(0);
    row_of_.clear();
    QString last_group;
    int rows = 0;
    for (const auto& e : entries_) {
        if (!e.group.isEmpty() && e.group != last_group) {
            last_group = e.group;
            ++rows; // header row
        }
        ++rows;
    }
    setRowCount(rows);
    last_group.clear();
    int r = 0;
    for (const auto& e : entries_) {
        if (!e.group.isEmpty() && e.group != last_group) {
            last_group = e.group;
            auto* h = new QTableWidgetItem(e.group.toUpper());
            h->setFlags(Qt::ItemIsEnabled);
            h->setForeground(QColor(ui::colors::AMBER()));
            QFont f = font();
            f.setBold(true);
            f.setPixelSize(ui::fonts::font_px(-4));
            h->setFont(f);
            setItem(r, ColName, h);
            setSpan(r, 0, 1, ColCount);
            ++r;
        }
        auto* name = cell(r, ColName);
        name->setText(e.label.isEmpty() ? e.symbol : e.label);
        name->setData(SymbolRole, e.symbol);
        for (int c = ColLast; c < ColCount; ++c)
            cell(r, c);
        row_of_.insert(e.symbol, r);
        ++r;
    }
    for (const auto& e : entries_)
        render_row(e.symbol);
    render_sessions();
    setUpdatesEnabled(had_updates);
}

QString QuoteGrid::symbol_at(int row) const {
    if (auto* it = item(row, ColName))
        return it->data(SymbolRole).toString();
    return {};
}

void QuoteGrid::update_quote(const QString& symbol, const services::QuoteData& q) {
    const int r = row_of_.value(symbol, -1);
    if (r < 0)
        return;
    const auto prev = quotes_.constFind(symbol);
    if (prev != quotes_.constEnd() && prev->price > 0 && q.price > 0 && prev->price != q.price) {
        auto* name = cell(r, ColName);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        name->setData(FlashTimeRole, now);
        name->setData(FlashUpRole, q.price > prev->price);
        last_flash_ms_ = now;
        if (!flash_timer_->isActive())
            flash_timer_->start();
    }
    quotes_.insert(symbol, q);
    errors_.remove(symbol);
    render_row(symbol);
}

void QuoteGrid::update_sparkline(const QString& symbol, const QVector<double>& closes) {
    if (!row_of_.contains(symbol))
        return;
    sparks_.insert(symbol, closes);
    render_row(symbol);
}

void QuoteGrid::set_row_error(const QString& symbol, const QString& tooltip) {
    if (tooltip.isEmpty())
        errors_.remove(symbol);
    else
        errors_.insert(symbol, tooltip);
    render_row(symbol);
}

void QuoteGrid::rerender() {
    for (const auto& e : entries_)
        render_row(e.symbol);
    viewport()->update();
}

void QuoteGrid::render_row(const QString& symbol) {
    const int r = row_of_.value(symbol, -1);
    if (r < 0)
        return;
    auto* name = cell(r, ColName);
    const QString err = errors_.value(symbol);
    const auto it = quotes_.constFind(symbol);
    if (it == quotes_.constEnd()) {
        const QString t = err.isEmpty() ? QString() : qg_fmt::placeholder();
        for (int c = ColLast; c < ColCount; ++c) {
            if (c == ColSession)
                continue; // the session does not depend on the quote
            auto* ci = cell(r, c);
            qg_set_text(ci, (c == ColDay || c == ColTrend || c == Col52w) ? QString() : t);
            ci->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            ci->setToolTip(err);
        }
        name->setData(StateRole, QVariant());
        name->setToolTip(err.isEmpty() ? symbol : symbol + QStringLiteral("\n") + err);
        return;
    }
    const QuoteData& q = it.value();
    const auto inst = qg_instrument(symbol, q);

    QString state_label;
    const QColor dot = qg_state_color(symbol, q, &state_label, venue_hints_.value(symbol));
    name->setData(StateRole, dot.isValid() ? QVariant(dot) : QVariant());

    QStringList tip{symbol};
    if (!q.name.isEmpty())
        tip << q.name;
    QStringList venue_bits;
    if (!q.exchange_name.isEmpty())
        venue_bits << q.exchange_name;
    if (!q.currency.isEmpty())
        venue_bits << q.currency;
    if (!venue_bits.isEmpty())
        tip << venue_bits.join(QStringLiteral(" · "));
    if (!state_label.isEmpty())
        tip << state_label;
    if (q.market_time > 0) {
        QString asof = tr("As of %1").arg(qg_fmt::date_time(q.market_time, qg_fmt::DateStyle::Short));
        if (q.exchange_delay_min > 0)
            asof += QStringLiteral(" · ") + tr("delayed %1 min").arg(q.exchange_delay_min);
        tip << asof;
    }
    if (!err.isEmpty())
        tip << err;
    name->setToolTip(tip.join(QLatin1Char('\n')));

    const QString tone = qg_tone(q.change_pct);
    auto* last = cell(r, ColLast);
    qg_set_text(last, q.price > 0 ? qg_fmt::price(q.price, inst) : qg_fmt::placeholder());
    last->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
    auto* chg = cell(r, ColChg);
    qg_set_text(chg, qg_fmt::price_change(q.change, q.price, inst));
    chg->setForeground(QColor(tone));
    auto* pct = cell(r, ColPct);
    qg_set_text(pct, qg_fmt::pct(q.change_pct));
    pct->setForeground(QColor(tone));
    for (auto* ci : {last, chg, pct})
        ci->setToolTip(err);

    auto* day = cell(r, ColDay);
    if (q.low > 0 && q.high >= q.low && q.price > 0) {
        day->setData(ui::RangeBarDelegate::LowRole, q.low);
        day->setData(ui::RangeBarDelegate::HighRole, q.high);
        day->setData(ui::RangeBarDelegate::ValueRole, q.price);
    } else {
        day->setData(ui::RangeBarDelegate::LowRole, QVariant());
        day->setData(ui::RangeBarDelegate::HighRole, QVariant());
        day->setData(ui::RangeBarDelegate::ValueRole, QVariant());
    }
    auto* wk = cell(r, Col52w);
    if (std::isfinite(q.fifty_two_week_low) && std::isfinite(q.fifty_two_week_high) &&
        q.fifty_two_week_high > q.fifty_two_week_low && q.price > 0) {
        wk->setData(ui::RangeBarDelegate::LowRole, q.fifty_two_week_low);
        wk->setData(ui::RangeBarDelegate::HighRole, q.fifty_two_week_high);
        wk->setData(ui::RangeBarDelegate::ValueRole, q.price);
    }
    auto* trend = cell(r, ColTrend);
    const auto sp = sparks_.constFind(symbol);
    if (sp != sparks_.constEnd() && sp->size() >= 2) {
        trend->setData(ui::SparklineDelegate::kDefaultRole, QVariant::fromValue(sp.value()));
        if (std::isfinite(q.previous_close) && q.previous_close > 0)
            trend->setData(BaselineRole, q.previous_close);
    }
    auto* vol = cell(r, ColVol);
    qg_set_text(vol, q.volume > 0 ? qg_fmt::volume(q.volume) : qg_fmt::placeholder());
    vol->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
}

qint64 QuoteGrid::latest_market_time() const {
    qint64 t = 0;
    for (const auto& q : quotes_)
        t = std::max(t, q.market_time);
    return t;
}

int QuoteGrid::max_delay_min() const {
    int d = -1;
    for (const auto& q : quotes_)
        d = std::max(d, q.exchange_delay_min);
    return d;
}

void QuoteGrid::on_flash_tick() {
    if (QDateTime::currentMSecsSinceEpoch() - last_flash_ms_ > kQuoteGridFlashMs + 60)
        flash_timer_->stop();
    viewport()->update();
}

void QuoteGrid::resizeEvent(QResizeEvent* e) {
    ui::DataTable::resizeEvent(e);
    apply_column_visibility();
}

void QuoteGrid::apply_column_visibility() {
    const int w = viewport()->width() > 0 ? viewport()->width() : width();
    const int dw = std::max(6, fontMetrics().horizontalAdvance(QLatin1Char('0')));
    // Cells carry 8 px of padding each side (DataTable's ::item rule): a column of
    // N characters needs N digits + 16 px + a pixel of grid, or it elides
    // ("172,458.…", "-1,480…").
    const auto col_px = [dw](int chars) { return chars * dw + 18; };
    // NAME is the stretch column and was what gave way: at the default tile width the
    // fixed columns took everything and the names collapsed to a dot. Reserve room
    // for a readable name first, then add the optional columns in priority order
    // while they fit.
    int budget = w - col_px(10);
    struct Col {
        int col;
        bool wanted;
        int chars;
    };
    const Col cols[] = {{ColLast, true, 11},
                        {ColPct, true, 8},
                        {ColDay, (optional_ & OptDay) != 0, 10},
                        {ColChg, true, 9},
                        {ColSession, (optional_ & OptSession) != 0, 15},
                        {ColTrend, (optional_ & OptTrend) != 0, 10},
                        {ColVol, (optional_ & OptVolume) != 0, 8},
                        {Col52w, (optional_ & Opt52w) != 0, 10}};
    for (const auto& c : cols) {
        const bool essential = c.col == ColLast || c.col == ColPct;
        const bool show = c.wanted && (essential || col_px(c.chars) <= budget);
        if (show)
            budget -= col_px(c.chars);
        setColumnHidden(c.col, !show);
        if (show) {
            horizontalHeader()->setSectionResizeMode(c.col, QHeaderView::Fixed);
            setColumnWidth(c.col, col_px(c.chars));
        }
    }
}

} // namespace fincept::screens::widgets
