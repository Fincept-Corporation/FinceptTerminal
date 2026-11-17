// SymbolSearchBox.cpp — see SymbolSearchBox.h. File-scope names carry an ssb_ prefix
// (unity builds).

#include "ui/widgets/symbol/SymbolSearchBox.h"

#include "core/instrument/SymbolInput.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/InputMemory.h" // input_memory_detail::encode_symbol / decode_symbol (stored_value)

#include <QClipboard>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTimer>

#include <algorithm>

namespace fincept::ui {

namespace {

namespace ci = core::instrument;
namespace sv = services;

constexpr int kSsbCoalesceMs = 60;

QString ssb_quoted(const QString& text) {
    return QString(QChar(0x201C)) + text + QChar(0x201D);
}

bool ssb_is_separator(QChar c) {
    return c == QLatin1Char(',') || c == QLatin1Char(';') || c == QLatin1Char('\n') || c == QLatin1Char('\r') ||
           c == QLatin1Char('\t');
}

} // namespace

QString describe_instrument(const ci::Instrument& inst) {
    QStringList parts;
    if (!inst.symbol.isEmpty())
        parts << inst.symbol;
    else if (!inst.display.isEmpty())
        parts << inst.display + QStringLiteral(" (") + QCoreApplication::translate("SymbolSearchBox", "no quote data") +
                     QLatin1Char(')');
    if (!inst.name.isEmpty())
        parts << inst.name;
    if (!inst.venue.isEmpty())
        parts << inst.venue;
    else if (!inst.country.isEmpty())
        parts << inst.country;
    if (!inst.currency.isEmpty())
        parts << inst.currency;
    return parts.join(QString::fromUtf8(" \xc2\xb7 "));
}

// ── Construction ────────────────────────────────────────────────────────────

SymbolSearchBox::SymbolSearchBox(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("symbolSearchBox"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    edit_ = new QLineEdit(this);
    edit_->setObjectName(QStringLiteral("symbolSearchEdit"));
    edit_->setAccessibleName(tr("Symbol search"));
    edit_->setPlaceholderText(tr("Symbol or company name"));
    edit_->setAccessibleDescription(edit_->placeholderText());
    edit_->setClearButtonEnabled(false);
    edit_->installEventFilter(this);
    hl->addWidget(edit_);
    setFocusProxy(edit_);
    setAccessibleName(tr("Symbol search"));

    popup_ = new SymbolPickerPopup(this);
    popup_->hide();
    popup_->on_clicked = [this](int row) {
        pick_row(row);
        if (isVisible())
            edit_->setFocus(Qt::OtherFocusReason);
    };

    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(kSsbCoalesceMs);
    connect(search_timer_, &QTimer::timeout, this, &SymbolSearchBox::run_search);
    connect(edit_, &QLineEdit::textEdited, this, &SymbolSearchBox::on_text_edited);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

SymbolSearchBox::~SymbolSearchBox() {
    if (resolver_ || search_id_ || resolve_id_ || batch_id_) {
        auto* r = resolver();
        r->cancel(search_id_);
        r->cancel(resolve_id_);
        r->cancel(batch_id_);
    }
    if (watched_window_)
        watched_window_->removeEventFilter(this);
}

sv::InstrumentResolver* SymbolSearchBox::resolver() {
    return resolver_ ? resolver_.data() : &sv::InstrumentResolver::instance();
}

void SymbolSearchBox::set_resolver(sv::InstrumentResolver* resolver) {
    cancel_pending();
    resolver_ = resolver;
}

// ── Configuration ───────────────────────────────────────────────────────────

void SymbolSearchBox::set_placeholder(const QString& text) {
    edit_->setPlaceholderText(text);
    edit_->setAccessibleDescription(text);
}

void SymbolSearchBox::set_options(const sv::ResolveOptions& opts) {
    opts_ = opts;
    rows_text_.clear();
    if (popup_->isVisible() && !current_item().trimmed().isEmpty())
        run_search();
}

void SymbolSearchBox::set_multi(bool on) {
    multi_ = on;
}

void SymbolSearchBox::set_clear_on_pick(bool on) {
    clear_on_pick_ = on;
}

void SymbolSearchBox::set_row_filter(RowFilter filter) {
    row_filter_ = std::move(filter);
    rows_text_.clear();
}

void SymbolSearchBox::set_allow_unquoted(bool on) {
    allow_unquoted_ = on;
    rows_text_.clear();
}

void SymbolSearchBox::set_allow_inactive(bool on) {
    allow_inactive_ = on;
    rows_text_.clear();
}

void SymbolSearchBox::set_compact(bool on) {
    if (compact_ == on)
        return;
    compact_ = on;
    refresh_theme();
}

void SymbolSearchBox::set_clear_on_escape(bool on) {
    clear_on_escape_ = on;
}

void SymbolSearchBox::set_recent_key(const QString& key) {
    recent_key_ = key;
}

void SymbolSearchBox::set_raw_text_allowed(bool on) {
    raw_allowed_ = on;
}

void SymbolSearchBox::set_text(const QString& text) {
    cancel_pending();
    search_timer_->stop();
    edit_->setText(text);
    rows_text_.clear();
    close_popup();
}

QString SymbolSearchBox::text() const {
    return edit_->text();
}

void SymbolSearchBox::focus_and_select() {
    edit_->setFocus(Qt::OtherFocusReason);
    edit_->selectAll();
}

void SymbolSearchBox::set_instrument(const ci::Instrument& inst) {
    cancel_pending();
    search_timer_->stop();
    picked_ = sv::complete_instrument(inst);
    picked_text_ = picked_.symbol.isEmpty() ? picked_.display : picked_.symbol;
    edit_->setText(picked_text_);
    rows_text_.clear();
    close_popup();
}

ci::Instrument SymbolSearchBox::current_instrument() const {
    if (picked_.is_valid() && !picked_text_.isEmpty() && edit_->text() == picked_text_)
        return picked_;
    return {};
}

QVariant SymbolSearchBox::stored_value() const {
    input_memory_detail::SymbolValue v;
    v.text = edit_->text();
    const ci::Instrument inst = current_instrument();
    if (inst.is_valid()) {
        v.symbol = inst.symbol;
        v.instrument = inst.to_json();
    }
    return input_memory_detail::encode_symbol(v);
}

void SymbolSearchBox::restore_stored_value(const QVariant& value) {
    const input_memory_detail::SymbolValue v = input_memory_detail::decode_symbol(value);
    if (!v.instrument.isEmpty()) {
        const ci::Instrument inst = ci::Instrument::from_json(v.instrument);
        if (inst.is_valid()) {
            set_instrument(inst);
            return;
        }
    }
    set_text(v.symbol.isEmpty() ? v.text : v.symbol);
    if (v.symbol.isEmpty())
        return;
    // Only the symbol is known: the local index (no network), which never guesses.
    const QString shown = edit_->text();
    QPointer<SymbolSearchBox> self(this);
    resolver()->resolve_stored(v.symbol, opts_.venue_hint, this, [self, shown](const sv::ResolveResult& r) {
        if (!self || self->edit_->text() != shown || r.matches.isEmpty() || !r.exact)
            return; // the user typed meanwhile, or the index does not know it for sure
        self->set_instrument(r.matches.first());
    });
}

bool SymbolSearchBox::is_popup_visible() const {
    return popup_->isVisible();
}

QString SymbolSearchBox::status_text() const {
    return popup_->status_text();
}

QVector<ci::Instrument> SymbolSearchBox::listed_instruments() const {
    QVector<ci::Instrument> out;
    for (int i = 0; i < popup_->count(); ++i)
        out.push_back(popup_->row_at(i)->instrument);
    return out;
}

int SymbolSearchBox::current_row() const {
    return popup_->current();
}

QString SymbolSearchBox::recent_key() const {
    if (!recent_key_.isEmpty())
        return recent_key_;
    QStringList classes;
    for (ci::AssetClass c : opts_.ctx.allowed_classes)
        classes << ci::asset_class_slug(c);
    classes.removeAll(QString());
    classes.sort();
    classes.removeDuplicates();
    QString key = classes.isEmpty() ? QStringLiteral("any") : classes.join(QLatin1Char('+'));
    if (!opts_.venue_hint.trimmed().isEmpty())
        key += QLatin1Char('@') + opts_.venue_hint.trimmed().toUpper();
    return key;
}

QString SymbolSearchBox::current_item() const {
    const QString t = edit_->text();
    if (!multi_)
        return t;
    qsizetype i = t.size();
    while (i > 0 && !ssb_is_separator(t.at(i - 1)))
        --i;
    return t.mid(i).trimmed();
}

// ── Theme / size ────────────────────────────────────────────────────────────

void SymbolSearchBox::refresh_theme() {
    const QFontMetrics fm(fonts::font(0));
    const int hpad = std::max(4, fm.averageCharWidth() / 2 + (compact_ ? 1 : 3));
    const QString ss =
        QStringLiteral("QLineEdit#symbolSearchEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 2px;"
                       " padding: 0px %4px; font-size: %5px; selection-background-color: %6; selection-color: %7; }"
                       "QLineEdit#symbolSearchEdit:focus { border: 1px solid %8; }"
                       "QLineEdit#symbolSearchEdit:disabled { color: %9; background: %10; }")
            .arg(QString::fromLatin1(colors::BG_BASE()), QString::fromLatin1(colors::TEXT_PRIMARY()),
                 QString::fromLatin1(colors::BORDER_MED()), QString::number(hpad), fonts::px(0),
                 QString::fromLatin1(colors::AMBER()), QString::fromLatin1(colors::TEXT_ON_ACCENT()),
                 QString::fromLatin1(colors::AMBER()), QString::fromLatin1(colors::TEXT_TERTIARY()),
                 QString::fromLatin1(colors::BG_SURFACE()));
    if (edit_->styleSheet() != ss)
        edit_->setStyleSheet(ss);
    apply_height();
}

void SymbolSearchBox::apply_height() {
    const QFontMetrics fm(fonts::font(0));
    const int h = fm.height() + (compact_ ? 5 : 11);
    if (minimumHeight() != h || maximumHeight() != h)
        setFixedHeight(h);
}

void SymbolSearchBox::changeEvent(QEvent* event) {
    if (event->type() == QEvent::EnabledChange && !isEnabled())
        close_popup();
    QWidget::changeEvent(event);
}

void SymbolSearchBox::hideEvent(QHideEvent* event) {
    close_popup();
    QWidget::hideEvent(event);
}

// ── Popup ───────────────────────────────────────────────────────────────────

void SymbolSearchBox::watch_window() {
    QWidget* w = window();
    if (w == watched_window_)
        return;
    if (watched_window_)
        watched_window_->removeEventFilter(this);
    watched_window_ = w;
    if (w && w != this)
        w->installEventFilter(this);
}

void SymbolSearchBox::open_popup() {
    if (!isVisible() || !isEnabled())
        return;
    watch_window();
    popup_->place(this);
}

void SymbolSearchBox::close_popup() {
    if (popup_ && popup_->isVisible())
        popup_->hide();
}

void SymbolSearchBox::cancel_pending() {
    if (!resolver_ && !resolve_id_ && !batch_id_) {
        resolving_ = false;
        return;
    }
    auto* r = resolver();
    if (resolve_id_)
        r->cancel(std::exchange(resolve_id_, 0));
    if (batch_id_)
        r->cancel(std::exchange(batch_id_, 0));
    resolving_ = false;
}

// ── Rows ────────────────────────────────────────────────────────────────────

void SymbolSearchBox::evaluate(SymbolRow& row, const QVector<ci::Instrument>& all) const {
    const ci::Instrument& inst = row.instrument;
    QStringList markers;
    SymbolTone tone = SymbolTone::Neutral;
    auto raise = [&tone](SymbolTone t) {
        if (static_cast<int>(t) > static_cast<int>(tone))
            tone = t;
    };
    row.state = RowState::Pickable;

    if (!sv::is_quotable(inst)) {
        markers << tr("no quote data");
        raise(SymbolTone::Warning);
        row.state = allow_unquoted_ ? RowState::Marked : RowState::Disabled;
    }
    if (!inst.is_active()) {
        if (inst.status == ci::InstrumentStatus::Renamed) {
            QString successor;
            for (const ci::Instrument& o : all)
                if (!inst.successor_id.isEmpty() && o.id == inst.successor_id)
                    successor = o.display;
            markers << (successor.isEmpty() ? tr("renamed") : tr("renamed \xe2\x86\x92 now %1").arg(successor));
        } else {
            markers << tr("delisted");
        }
        raise(SymbolTone::Danger);
        if (row.state != RowState::Disabled)
            row.state = allow_inactive_ ? RowState::Marked : RowState::Disabled;
    }
    if (row.via_former_symbol && !row.matched_text.isEmpty())
        markers << tr("%1 \xe2\x86\x92 now %2").arg(row.matched_text.toUpper(), inst.display);

    if (row_filter_) {
        const RowCheck check = row_filter_(inst);
        if (check.state == RowState::Disabled) {
            row.state = RowState::Disabled;
            markers << (check.reason.isEmpty() ? tr("not available here") : check.reason);
            raise(SymbolTone::Danger);
        } else if (check.state == RowState::Marked) {
            if (row.state == RowState::Pickable)
                row.state = RowState::Marked;
            if (!check.reason.isEmpty())
                markers << check.reason;
            raise(SymbolTone::Warning);
        }
    }
    if (row.via_former_symbol && tone == SymbolTone::Neutral)
        tone = SymbolTone::Warning;
    row.marker = markers.join(QString::fromUtf8(" \xc2\xb7 "));
    row.marker_tone = tone;
}

QVector<SymbolRow> SymbolSearchBox::rows_from(const sv::ResolveResult& res) const {
    QVector<SymbolRow> rows;
    rows.reserve(res.matches.size());
    for (qsizetype i = 0; i < res.matches.size(); ++i) {
        SymbolRow row;
        row.instrument = res.matches.at(i);
        if (i < res.ranked.size()) {
            row.match = res.ranked.at(i).match;
            row.matched_text = res.ranked.at(i).matched_text;
            row.via_former_symbol = res.ranked.at(i).via_former_symbol;
        }
        evaluate(row, res.matches);
        rows.push_back(std::move(row));
    }
    return rows;
}

QString SymbolSearchBox::status_for(const sv::ResolveResult& res, int rows, SymbolTone* tone) const {
    const QString shown = ci::clean_symbol_text(res.query).left(40);
    if (!res.error.isEmpty()) {
        *tone = SymbolTone::Danger;
        return res.error;
    }
    if (!res.final) {
        if (!res.note.isEmpty()) {
            *tone = res.offline ? SymbolTone::Warning : SymbolTone::Neutral;
            return res.note;
        }
        *tone = SymbolTone::Busy;
        return rows == 0 ? tr("Searching\xe2\x80\xa6") : tr("Searching online for more\xe2\x80\xa6");
    }
    if (rows == 0) {
        *tone = SymbolTone::Warning;
        return res.note.isEmpty() ? tr("No instrument matches %1.").arg(ssb_quoted(shown)) : res.note;
    }
    if (!res.note.isEmpty()) {
        const bool former = !res.ranked.isEmpty() && res.ranked.first().via_former_symbol;
        *tone = (res.offline || former) ? SymbolTone::Warning : SymbolTone::Neutral;
        return res.note;
    }
    *tone = SymbolTone::Neutral;
    return {};
}

void SymbolSearchBox::show_rows(QVector<SymbolRow> rows, const QString& for_text, const QString& status,
                                SymbolTone tone) {
    showing_recent_ = false;
    rows_text_ = for_text;
    int cur = -1;
    for (qsizetype i = 0; i < rows.size(); ++i) {
        if (rows.at(i).state != RowState::Disabled) {
            cur = static_cast<int>(i);
            break;
        }
    }
    if (cur < 0 && !rows.isEmpty())
        cur = 0;
    const ci::ParsedInput p = ci::parse_symbol_input(for_text, opts_.venue_hint);
    QString code_q;
    switch (p.kind) {
        case ci::InputKind::Ticker:
        case ci::InputKind::YahooSymbol:
        case ci::InputKind::CryptoPair:
        case ci::InputKind::FxPair:
            code_q = p.base;
            break;
        case ci::InputKind::Alias:
        case ci::InputKind::Name:
        case ci::InputKind::Contract:
            code_q = p.cleaned;
            break;
        default:
            break;
    }
    const QString name_q = p.search_text.isEmpty() ? p.cleaned : p.search_text;
    popup_->set_rows(std::move(rows), code_q, name_q, cur);
    popup_->set_status(status, tone);
    if (edit_->hasFocus() || popup_->isVisible())
        open_popup();
}

void SymbolSearchBox::show_recent() {
    const QVector<ci::Instrument> recents = resolver()->recent_picks(recent_key(), 8);
    QVector<SymbolRow> rows;
    for (const ci::Instrument& inst : recents) {
        if (!opts_.ctx.allowed_classes.isEmpty() && !opts_.ctx.allowed_classes.contains(inst.asset_class))
            continue;
        SymbolRow row;
        row.instrument = inst;
        evaluate(row, recents);
        rows.push_back(std::move(row));
    }
    if (rows.isEmpty()) {
        close_popup();
        return;
    }
    show_rows(std::move(rows), QString(), tr("Recent"), SymbolTone::Neutral);
    showing_recent_ = true;
    open_popup();
}

// ── Typing and searching ────────────────────────────────────────────────────

void SymbolSearchBox::on_text_edited(const QString& text) {
    Q_UNUSED(text);
    cancel_pending();
    const QString item = current_item();
    if (item.trimmed().isEmpty()) {
        search_timer_->stop();
        rows_text_.clear();
        if (edit_->hasFocus() && edit_->text().trimmed().isEmpty())
            show_recent();
        else
            close_popup();
        return;
    }
    search_timer_->start();
}

void SymbolSearchBox::run_search() {
    const QString item = current_item();
    if (item.trimmed().isEmpty())
        return;
    search_id_ = resolver()->search(item, opts_, this,
                                    [this](const sv::ResolveResult& res) { on_search_result(res); });
}

void SymbolSearchBox::on_search_result(const sv::ResolveResult& res) {
    if (resolving_ || res.query != current_item())
        return; // stale, or Enter is being answered
    QVector<SymbolRow> rows = rows_from(res);
    SymbolTone tone = SymbolTone::Neutral;
    const QString status = status_for(res, static_cast<int>(rows.size()), &tone);
    show_rows(std::move(rows), res.query, status, tone);
}

// ── Committing ──────────────────────────────────────────────────────────────

void SymbolSearchBox::commit() {
    const QString text = edit_->text();
    if (multi_) {
        const QStringList items = ci::split_symbol_list(text);
        if (items.size() > 1) {
            commit_list(items);
            return;
        }
    }
    const bool list_matches = showing_recent_ ? text.trimmed().isEmpty() : (rows_text_ == current_item());
    if (popup_->isVisible() && popup_->count() > 0 && list_matches && !resolving_) {
        const int cur = popup_->current();
        if (cur >= 0) {
            pick_row(cur);
            return;
        }
    }
    if (current_item().trimmed().isEmpty())
        return;
    commit_single(current_item());
}

void SymbolSearchBox::commit_single(const QString& text) {
    search_timer_->stop();
    cancel_pending();
    resolving_ = true;
    if (rows_text_ != text)
        popup_->set_rows({}, QString(), QString(), -1);
    popup_->set_status(tr("Looking up %1\xe2\x80\xa6").arg(ssb_quoted(ci::clean_symbol_text(text).left(40))),
                       SymbolTone::Busy);
    open_popup();
    resolve_id_ = resolver()->resolve(text, opts_, this, [this, text](const sv::ResolveResult& res) {
        resolve_id_ = 0;
        resolving_ = false;
        if (current_item() != text)
            return; // the user typed on
        QVector<SymbolRow> rows = rows_from(res);
        if (res.exact && !rows.isEmpty() && rows.first().state != RowState::Disabled) {
            pick(rows.first().instrument);
            return;
        }
        if (rows.isEmpty()) {
            const QString shown = ci::clean_symbol_text(text).left(40);
            const QString msg = !res.error.isEmpty() ? res.error
                                : !res.note.isEmpty() ? res.note
                                                      : tr("No instrument matches %1.").arg(ssb_quoted(shown));
            show_rows({}, text, msg, res.error.isEmpty() ? SymbolTone::Warning : SymbolTone::Danger);
            open_popup();
            emit unresolved({text}, msg);
            if (raw_allowed_)
                emit text_committed_raw(text);
            return;
        }
        QString status = res.note;
        SymbolTone tone = res.offline ? SymbolTone::Warning : SymbolTone::Neutral;
        if (res.exact && rows.first().state == RowState::Disabled) {
            status = rows.first().marker;
            tone = SymbolTone::Danger;
        } else if (status.isEmpty()) {
            status = tr("Pick one of these.");
        }
        show_rows(std::move(rows), text, status, tone);
        open_popup();
    });
}

void SymbolSearchBox::commit_list(const QStringList& items) {
    search_timer_->stop();
    cancel_pending();
    resolving_ = true;
    popup_->set_rows({}, QString(), QString(), -1);
    popup_->set_status(tr("Looking up %n symbol(s)\xe2\x80\xa6", nullptr, static_cast<int>(items.size())), SymbolTone::Busy);
    open_popup();
    batch_id_ = resolver()->resolve_many(items, opts_, this, [this, items](const QVector<sv::ResolveResult>& results) {
        batch_id_ = 0;
        resolving_ = false;
        QVector<ci::Instrument> picked;
        QStringList bad;
        QStringList reasons;
        for (qsizetype i = 0; i < items.size() && i < results.size(); ++i) {
            const sv::ResolveResult& res = results.at(i);
            const QVector<SymbolRow> rows = rows_from(res);
            if (res.exact && !rows.isEmpty() && rows.first().state != RowState::Disabled) {
                const ci::Instrument& inst = rows.first().instrument;
                const bool dup = std::any_of(picked.cbegin(), picked.cend(),
                                             [&inst](const ci::Instrument& o) { return o.same_instrument(inst); });
                if (!dup)
                    picked.push_back(inst);
                continue;
            }
            bad << items.at(i);
            if (!res.error.isEmpty())
                reasons << QStringLiteral("%1 (%2)").arg(items.at(i), res.error);
            else if (rows.isEmpty())
                reasons << tr("%1 (no match)").arg(items.at(i));
            else if (res.exact)
                reasons << QStringLiteral("%1 (%2)").arg(items.at(i), rows.first().marker);
            else
                reasons << tr("%1 (several matches \xe2\x80\x94 type it alone to choose)").arg(items.at(i));
        }
        for (const ci::Instrument& inst : std::as_const(picked))
            resolver()->remember_pick(recent_key(), inst);
        if (!picked.isEmpty())
            emit instruments_picked(picked);
        if (!bad.isEmpty()) {
            const QString msg = tr("%n item(s) could not be added: ", nullptr, static_cast<int>(bad.size())) +
                                reasons.join(QStringLiteral(", "));
            edit_->setText(bad.join(QStringLiteral(", ")));
            show_rows({}, edit_->text(), msg, SymbolTone::Warning);
            open_popup();
            emit unresolved(bad, msg);
            return;
        }
        if (clear_on_pick_)
            edit_->clear();
        rows_text_.clear();
        close_popup();
    });
}

void SymbolSearchBox::complete_current() {
    const SymbolRow* row = popup_->current_row();
    if (!row)
        return;
    const QString t = row->instrument.symbol.isEmpty() ? row->instrument.display : row->instrument.symbol;
    if (multi_) {
        const QString all = edit_->text();
        qsizetype i = all.size();
        while (i > 0 && !ssb_is_separator(all.at(i - 1)))
            --i;
        const QString head = all.left(i);
        edit_->setText(head + (head.isEmpty() || head.endsWith(QLatin1Char(' ')) ? QString() : QStringLiteral(" ")) + t);
    } else {
        edit_->setText(t);
    }
    edit_->setCursorPosition(static_cast<int>(edit_->text().size()));
    search_timer_->stop();
    run_search();
}

void SymbolSearchBox::pick_row(int row) {
    const SymbolRow* r = popup_->row_at(row);
    if (!r)
        return;
    if (r->state == RowState::Disabled) {
        popup_->set_status(r->marker.isEmpty() ? tr("This instrument cannot be picked here.") : r->marker,
                           SymbolTone::Danger);
        open_popup();
        return;
    }
    if (multi_ && !showing_recent_ && current_item() != edit_->text().trimmed()) {
        // A list is being typed: complete this item and keep going (Enter adds the list).
        popup_->set_current(row);
        complete_current();
        edit_->setText(edit_->text() + QStringLiteral(", "));
        edit_->setCursorPosition(static_cast<int>(edit_->text().size()));
        rows_text_.clear();
        close_popup();
        return;
    }
    pick(r->instrument);
}

void SymbolSearchBox::pick(const ci::Instrument& inst_in) {
    const ci::Instrument inst = sv::complete_instrument(inst_in);
    search_timer_->stop();
    cancel_pending();
    resolver()->remember_pick(recent_key(), inst);
    picked_ = inst;
    close_popup();
    rows_text_.clear();
    showing_recent_ = false;
    if (clear_on_pick_) {
        edit_->clear();
        picked_text_.clear();
    } else {
        picked_text_ = inst.symbol.isEmpty() ? inst.display : inst.symbol;
        edit_->setText(picked_text_);
        edit_->setCursorPosition(static_cast<int>(picked_text_.size()));
    }
    emit instrument_picked(inst);
}

bool SymbolSearchBox::handle_paste() {
    const QClipboard* cb = QGuiApplication::clipboard();
    if (!cb)
        return false;
    const QStringList items = ci::split_symbol_list(cb->text());
    if (items.size() <= 1)
        return false; // a single item: the line edit pastes it as usual
    edit_->insert(items.join(QStringLiteral(", ")));
    const int total = static_cast<int>(ci::split_symbol_list(edit_->text()).size());
    search_timer_->stop();
    rows_text_.clear();
    popup_->set_rows({}, QString(), QString(), -1);
    popup_->set_status(tr("%n symbol(s) entered \xe2\x80\x94 press Enter to add them.", nullptr, total), SymbolTone::Neutral);
    open_popup();
    return true;
}

// ── Keyboard / focus ────────────────────────────────────────────────────────

bool SymbolSearchBox::handle_key(QKeyEvent* e) {
    const bool visible = popup_->isVisible();
    switch (e->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up: {
            if (!visible) {
                if (edit_->text().trimmed().isEmpty())
                    show_recent();
                else if (rows_text_ == current_item() && popup_->count() > 0)
                    open_popup();
                else {
                    search_timer_->stop();
                    run_search();
                }
                return true;
            }
            popup_->move_current(e->key() == Qt::Key_Down ? 1 : -1);
            return true;
        }
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            if (!visible)
                return false;
            popup_->move_current((e->key() == Qt::Key_PageDown ? 1 : -1) * std::max(1, popup_->visible_rows() - 1));
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            commit();
            return true;
        case Qt::Key_Tab:
            if (visible && popup_->current_row() && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
                complete_current();
                return true;
            }
            return false;
        case Qt::Key_Escape:
            if (visible) {
                close_popup();
                return true;
            }
            if (clear_on_escape_ && !edit_->text().isEmpty()) {
                cancel_pending();
                search_timer_->stop();
                edit_->clear();
                picked_text_.clear();
                rows_text_.clear();
                return true;
            }
            return false;
        default:
            break;
    }
    if (multi_ && e->matches(QKeySequence::Paste))
        return handle_paste();
    return false;
}

bool SymbolSearchBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == edit_) {
        switch (event->type()) {
            case QEvent::KeyPress:
                if (handle_key(static_cast<QKeyEvent*>(event)))
                    return true;
                break;
            case QEvent::ShortcutOverride: {
                // Keep window shortcuts from taking the keys the list needs.
                auto* ke = static_cast<QKeyEvent*>(event);
                const int k = ke->key();
                const bool list_key = k == Qt::Key_Escape || k == Qt::Key_Up || k == Qt::Key_Down ||
                                      k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Tab ||
                                      k == Qt::Key_PageUp || k == Qt::Key_PageDown;
                if ((popup_->isVisible() && list_key) ||
                    (k == Qt::Key_Escape && clear_on_escape_ && !edit_->text().isEmpty())) {
                    event->accept();
                    return true;
                }
                break;
            }
            case QEvent::FocusIn:
                QTimer::singleShot(0, this, [this]() {
                    if (edit_->hasFocus() && edit_->text().trimmed().isEmpty() && !popup_->isVisible())
                        show_recent();
                });
                break;
            case QEvent::FocusOut:
                QTimer::singleShot(0, this, [this]() {
                    if (!edit_->hasFocus() && !popup_->underMouse())
                        close_popup();
                });
                break;
            default:
                break;
        }
    } else if (watched == watched_window_) {
        switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize:
                if (popup_->isVisible())
                    popup_->place(this);
                break;
            case QEvent::Hide:
                close_popup();
                break;
            case QEvent::WindowDeactivate:
                QTimer::singleShot(0, this, [this]() {
                    if (!popup_->underMouse() && !edit_->hasFocus())
                        close_popup();
                });
                break;
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace fincept::ui
