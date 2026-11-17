// src/ui/widgets/ScreenStatusBar.cpp
#include "ui/widgets/ScreenStatusBar.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>

namespace fincept::ui {

namespace {

QString sbar_sheet() {
    namespace c = fincept::ui::colors;
    return QStringLiteral("QWidget#uiStatusBar{background:%1;border:none;border-top:1px solid %2;}"
                          "QLabel{background:transparent;color:%3;font-family:%4;font-size:%5px;}"
                          "QLabel[tone=\"dim\"]{color:%6;}"
                          "QLabel[tone=\"accent\"]{color:%7;font-weight:700;}"
                          "QLabel[tone=\"ok\"]{color:%8;}"
                          "QLabel[tone=\"warn\"]{color:%9;}"
                          "QLabel[tone=\"danger\"]{color:%10;font-weight:700;}"
                          "QLabel[tone=\"pos\"]{color:%11;}"
                          "QLabel[tone=\"neg\"]{color:%12;}"
                          "QLabel[tone=\"info\"]{color:%13;}"
                          "QLabel[chip=\"true\"]{border:1px solid %14;padding:0px 5px;}"
                          "QLabel[chip=\"true\"][tone=\"ok\"]{border-color:%8;}"
                          "QLabel[chip=\"true\"][tone=\"warn\"]{border-color:%9;}"
                          "QLabel[chip=\"true\"][tone=\"danger\"]{border-color:%10;}"
                          "QLabel[chip=\"true\"][tone=\"accent\"]{border-color:%7;}"
                          "QLabel#uiStatusSep{color:%14;}"
                          "QLabel#uiStatusClock{color:%13;font-weight:700;}"
                          "QPushButton#uiStatusAction{background:transparent;border:1px solid %7;color:%7;"
                          "font-family:%4;font-size:%5px;font-weight:700;padding:0px 8px;}"
                          "QPushButton#uiStatusAction:hover{background:%15;}")
        .arg(c::BG_BASE(), c::BORDER_DIM(), c::TEXT_SECONDARY(), QString(fonts::DATA_FAMILY))
        .arg(fonts::px(-4))
        .arg(c::TEXT_TERTIARY(), c::AMBER(), c::SUCCESS(), c::WARNING(), c::DANGER(), c::POSITIVE(), c::NEGATIVE(),
             c::CYAN())
        .arg(c::BORDER_MED(), c::ACCENT_BG());
}

} // namespace

ScreenStatusBar::ScreenStatusBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("uiStatusBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(sbar_sheet()); // before any child exists
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    row_ = new QHBoxLayout(this);
    row_->setContentsMargins(8, 0, 8, 0);
    row_->setSpacing(6);
    left_ = new QHBoxLayout;
    left_->setSpacing(6);
    centre_ = new QHBoxLayout;
    centre_->setSpacing(6);
    right_ = new QHBoxLayout;
    right_->setSpacing(6);
    row_->addLayout(left_);
    row_->addStretch(1);
    row_->addLayout(centre_);
    row_->addStretch(1);
    row_->addLayout(right_);

    notice_ = new QLabel(this);
    notice_->setObjectName(QStringLiteral("uiStatusNotice"));
    notice_->setTextFormat(Qt::PlainText);
    notice_->setVisible(false);
    centre_->addWidget(notice_);
    notice_btn_ = new QPushButton(this);
    notice_btn_->setObjectName(QStringLiteral("uiStatusAction"));
    notice_btn_->setCursor(Qt::PointingHandCursor);
    notice_btn_->setVisible(false);
    centre_->addWidget(notice_btn_);
    connect(notice_btn_, &QPushButton::clicked, this, [this]() { trigger_notice_action(); });
    notice_timer_ = new QTimer(this);
    notice_timer_->setSingleShot(true);
    connect(notice_timer_, &QTimer::timeout, this, &ScreenStatusBar::finish_notice);

    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &ScreenStatusBar::update_clock);

    fit_metrics();
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() {
        fit_metrics();
        relayout();
    });
}

ScreenStatusBar::~ScreenStatusBar() = default;

const char* ScreenStatusBar::tone_key(Tone t) {
    switch (t) {
    case Tone::Normal:
        return "";
    case Tone::Dim:
        return "dim";
    case Tone::Accent:
        return "accent";
    case Tone::Ok:
        return "ok";
    case Tone::Warn:
        return "warn";
    case Tone::Danger:
        return "danger";
    case Tone::Positive:
        return "pos";
    case Tone::Negative:
        return "neg";
    case Tone::Info:
        return "info";
    }
    return "";
}

void ScreenStatusBar::apply_property(QWidget* w, const char* name, const QString& value) {
    if (!w || w->property(name).toString() == value)
        return;
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

ScreenStatusBar::Entry* ScreenStatusBar::find(const QString& id) {
    for (auto& e : entries_)
        if (e.id == id)
            return &e;
    return nullptr;
}

const ScreenStatusBar::Entry* ScreenStatusBar::find(const QString& id) const {
    for (const auto& e : entries_)
        if (e.id == id)
            return &e;
    return nullptr;
}

QHBoxLayout* ScreenStatusBar::layout_for(Segment s) const {
    switch (s) {
    case Segment::Left:
        return left_;
    case Segment::Centre:
        return centre_;
    case Segment::Right:
        return right_;
    }
    return left_;
}

QLabel* ScreenStatusBar::add_item(const QString& id, Segment segment, const QString& text, Tone tone, int priority) {
    if (Entry* existing = find(id))
        return existing->label;
    Entry e;
    e.id = id;
    e.segment = segment;
    e.priority = priority;
    e.order = next_order_++;
    e.sep = new QLabel(QStringLiteral("|"), this);
    e.sep->setObjectName(QStringLiteral("uiStatusSep"));
    e.label = new QLabel(text, this);
    e.label->setObjectName(QStringLiteral("uiStatusItem"));
    e.label->setTextFormat(Qt::PlainText);
    apply_property(e.label, "tone", QString::fromLatin1(tone_key(tone)));
    QHBoxLayout* lay = layout_for(segment);
    if (segment == Segment::Centre) {
        // Centre items go before the notice.
        const int at = lay->indexOf(notice_);
        lay->insertWidget(at < 0 ? lay->count() : at, e.sep);
        lay->insertWidget(at < 0 ? lay->count() : at + 1, e.label);
    } else if (segment == Segment::Right && clock_) {
        const int at = lay->indexOf(clock_sep_);
        lay->insertWidget(at, e.sep);
        lay->insertWidget(at + 1, e.label);
    } else {
        lay->addWidget(e.sep);
        lay->addWidget(e.label);
    }
    entries_.append(e);
    relayout();
    return e.label;
}

QLabel* ScreenStatusBar::item(const QString& id) const {
    const Entry* e = find(id);
    return e ? e->label : nullptr;
}

void ScreenStatusBar::set_text(const QString& id, const QString& text) {
    Entry* e = find(id);
    if (!e || e->label->text() == text)
        return;
    const int before = e->label->sizeHint().width();
    e->label->setText(text);
    if (e->label->sizeHint().width() > before)
        relayout(); // a longer text may no longer fit
}

void ScreenStatusBar::set_tone(const QString& id, Tone tone) {
    if (Entry* e = find(id))
        apply_property(e->label, "tone", QString::fromLatin1(tone_key(tone)));
}

void ScreenStatusBar::set_item(const QString& id, const QString& text, Tone tone) {
    set_text(id, text);
    set_tone(id, tone);
}

void ScreenStatusBar::set_tooltip(const QString& id, const QString& tooltip) {
    if (Entry* e = find(id))
        if (e->label->toolTip() != tooltip)
            e->label->setToolTip(tooltip);
}

void ScreenStatusBar::set_chip(const QString& id, bool chip) {
    if (Entry* e = find(id))
        apply_property(e->label, "chip", chip ? QStringLiteral("true") : QString());
}

void ScreenStatusBar::set_item_visible(const QString& id, bool visible) {
    Entry* e = find(id);
    if (!e || e->user_visible == visible)
        return;
    e->user_visible = visible;
    relayout();
}

// ── Clock ────────────────────────────────────────────────────────────────────

void ScreenStatusBar::set_clock(bool enabled, bool seconds) {
    clock_seconds_ = seconds;
    if (enabled && !clock_) {
        clock_sep_ = new QLabel(QStringLiteral("|"), this);
        clock_sep_->setObjectName(QStringLiteral("uiStatusSep"));
        clock_ = new QLabel(this);
        clock_->setObjectName(QStringLiteral("uiStatusClock"));
        clock_->setTextFormat(Qt::PlainText);
        right_->addWidget(clock_sep_);
        right_->addWidget(clock_);
        update_clock();
    }
    if (clock_) {
        clock_->setVisible(enabled);
        clock_sep_->setVisible(enabled && !entries_.isEmpty());
    }
    clock_timer_->setInterval(seconds ? 1000 : 20000);
    relayout();
}

void ScreenStatusBar::start_clock() {
    if (!clock_)
        return;
    update_clock();
    clock_timer_->start();
}

void ScreenStatusBar::stop_clock() {
    clock_timer_->stop();
}

void ScreenStatusBar::update_clock() {
    if (!clock_)
        return;
    const QDateTime now = QDateTime::currentDateTime();
    // The zone abbreviation is an expensive lookup that changes twice a year at most:
    // refresh it once an hour, not on every tick.
    const qint64 hour = now.toSecsSinceEpoch() / 3600;
    if (tz_abbrev_.isEmpty() || hour != tz_hour_) {
        tz_hour_ = hour;
        tz_abbrev_ = now.timeZone().abbreviation(now);
    }
    const QString t = now.toString(clock_seconds_ ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm")) +
                      (tz_abbrev_.isEmpty() ? QString() : QLatin1Char(' ') + tz_abbrev_);
    if (clock_->text() != t)
        clock_->setText(t);
}

// ── Notice ───────────────────────────────────────────────────────────────────

void ScreenStatusBar::show_notice(const QString& text, const QString& action_label, std::function<void()> action,
                                  int ms, Tone tone, std::function<void()> on_expire) {
    // A notice still showing finishes first (e.g. its delete becomes final).
    if (notice_active())
        finish_notice();
    notice_action_ = std::move(action);
    notice_expire_ = std::move(on_expire);
    notice_->setText(text);
    apply_property(notice_, "tone", QString::fromLatin1(tone_key(tone)));
    QString label = action_label;
    label.replace(QLatin1Char('&'), QStringLiteral("&&"));
    notice_btn_->setText(label);
    notice_btn_->setAccessibleName(action_label);
    notice_->setVisible(true);
    notice_btn_->setVisible(!action_label.isEmpty() && static_cast<bool>(notice_action_));
    notice_timer_->start(std::max(1000, ms));
}

void ScreenStatusBar::finish_notice() {
    if (!notice_active())
        return;
    auto expire = std::move(notice_expire_);
    clear_notice();
    if (expire)
        expire();
}

void ScreenStatusBar::clear_notice() {
    notice_timer_->stop();
    notice_action_ = nullptr;
    notice_expire_ = nullptr;
    if (notice_->isVisibleTo(this))
        notice_->setVisible(false);
    if (notice_btn_->isVisibleTo(this))
        notice_btn_->setVisible(false);
}

bool ScreenStatusBar::trigger_notice_action() {
    if (!notice_active() || !notice_action_)
        return false;
    auto action = std::move(notice_action_);
    clear_notice();
    action();
    return true;
}

bool ScreenStatusBar::notice_active() const {
    return notice_timer_->isActive();
}

bool ScreenStatusBar::notice_has_action() const {
    return notice_active() && static_cast<bool>(notice_action_);
}

// ── Layout ───────────────────────────────────────────────────────────────────

void ScreenStatusBar::fit_metrics() {
    const QFontMetrics fm(fonts::font(-4));
    const int h = std::max(24, fm.height() + 10);
    if (minimumHeight() != h)
        setMinimumHeight(h);
    notice_btn_->setFixedHeight(std::max(18, fm.height() + 4));
}

void ScreenStatusBar::relayout() {
    // Drop the least important items until the bar fits, then show a separator
    // between neighbouring visible items of each segment.
    const int avail = width();
    QVector<Entry*> order;
    for (auto& e : entries_) {
        e.dropped = false;
        order << &e;
    }
    if (avail > 0) {
        const QFontMetrics fm(fonts::font(-4));
        const int sep_w = fm.horizontalAdvance(QStringLiteral("|")) + row_->spacing();
        int needed = row_->contentsMargins().left() + row_->contentsMargins().right();
        for (Entry* e : order)
            if (e->user_visible)
                needed += e->label->sizeHint().width() + sep_w + row_->spacing();
        if (notice_->isVisibleTo(this))
            needed += notice_->sizeHint().width() + notice_btn_->sizeHint().width() + 2 * row_->spacing();
        if (clock_ && clock_->isVisibleTo(this))
            needed += clock_->sizeHint().width() + sep_w;
        std::sort(order.begin(), order.end(), [](const Entry* a, const Entry* b) {
            if (a->priority != b->priority)
                return a->priority < b->priority;
            return a->order > b->order;
        });
        for (Entry* e : order) {
            if (needed <= avail)
                break;
            if (!e->user_visible)
                continue;
            e->dropped = true;
            needed -= e->label->sizeHint().width() + sep_w + row_->spacing();
        }
    }
    bool any_right = false;
    for (Segment seg : {Segment::Left, Segment::Centre, Segment::Right}) {
        bool first = true;
        QVector<Entry*> in_seg;
        for (auto& e : entries_)
            if (e.segment == seg)
                in_seg << &e;
        std::sort(in_seg.begin(), in_seg.end(), [](const Entry* a, const Entry* b) { return a->order < b->order; });
        for (Entry* e : in_seg) {
            const bool show = e->user_visible && !e->dropped;
            if (e->label->isVisibleTo(this) != show)
                e->label->setVisible(show);
            const bool sep = show && !first;
            if (e->sep->isVisibleTo(this) != sep)
                e->sep->setVisible(sep);
            if (show) {
                first = false;
                if (seg == Segment::Right)
                    any_right = true;
            }
        }
    }
    if (clock_sep_) {
        const bool sep = clock_->isVisibleTo(this) && any_right;
        if (clock_sep_->isVisibleTo(this) != sep)
            clock_sep_->setVisible(sep);
    }
}

QSize ScreenStatusBar::minimumSizeHint() const {
    return {0, QWidget::minimumSizeHint().height()};
}

void ScreenStatusBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout(); // visibility changes here (and after appearance_settled) only
}

void ScreenStatusBar::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    relayout();
}

void ScreenStatusBar::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
}

} // namespace fincept::ui
