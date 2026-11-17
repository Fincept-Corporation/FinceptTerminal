// src/screens/calendars/CalendarPanel.cpp
#include "screens/calendars/CalendarPanel.h"

#include "core/events/EventBus.h"
#include "screens/equity_research/EquityResearchFormat.h"
#include "services/billing/PlanAccess.h"
#include "services/fincept_data/FinceptDataParse.h"
#include "services/fincept_data/FinceptDataService.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fd = services::fincept_data;

namespace calendar_panel_detail {

constexpr int kPageSize = 50;      ///< events per page (the server's default; max 100)
constexpr int kDefaultDays = 7;    ///< default range: today .. today + 7 days
constexpr int kAgeTickMs = 30 * 1000;
constexpr int kSymbolRole = Qt::UserRole + 1;

void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// Table row height: 24px at the default font size, growing with the user's font.
int calendar_row_height() {
    return std::max(24, QFontMetrics(ui::fonts::font(-3)).height() + 8);
}

QString num(double v, int decimals = 2) {
    return fd::has(v) ? equity_fmt::grouped(v, decimals) : equity_fmt::dash();
}

QTableWidgetItem* text_item(const QString& text, const QColor& fg = {}) {
    auto* item = new QTableWidgetItem(text.isEmpty() ? equity_fmt::dash() : text);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (fg.isValid())
        item->setForeground(fg);
    return item;
}

/// Right-aligned number cell that sorts by value (missing values first).
QTableWidgetItem* number_item(double value, const QString& text, const QColor& fg = {}) {
    auto* item = new ui::NumericTableWidgetItem(text, fd::has(value) ? value : -std::numeric_limits<double>::infinity());
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (fg.isValid())
        item->setForeground(fg);
    return item;
}

/// Date / time cell that sorts chronologically. `with_time`: a real instant, shown in
/// local time (economic releases). Otherwise a calendar day: Yahoo writes those as
/// midnight UTC or midnight New York (05:00Z), so the UTC date is the day meant —
/// converting to local time would move it back a day west of UTC.
QTableWidgetItem* when_item(const QDateTime& utc, bool with_time) {
    if (!utc.isValid())
        return number_item(fd::kNaN, equity_fmt::dash());
    const QString text = with_time ? utc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                   : utc.date().toString(Qt::ISODate);
    auto* item = number_item(static_cast<double>(utc.toMSecsSinceEpoch()), text);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item->setToolTip(utc.toString(QStringLiteral("yyyy-MM-dd HH:mm 'UTC'")));
    return item;
}

QColor sign_color(double v) {
    if (!fd::has(v) || v == 0.0)
        return QColor(ui::colors::TEXT_TERTIARY());
    return QColor(v > 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
}

/// How a column takes its width (columns as filled in CalendarPanel::populate()).
enum class ColumnFit {
    Content, ///< dates, times, symbols, short labels: as wide as their content
    Number,  ///< numbers: as wide as their content, right-aligned header
    Fill,    ///< free text (company, event): shares whatever width is left
};

ColumnFit column_fit(fd::CalendarKind kind, int column) {
    switch (kind) {
    case fd::CalendarKind::Earnings: // DATE TIME SYMBOL COMPANY EVENT | EPS EST. EPS ACTUAL SURPRISE MARKET CAP
        return column == 3 || column == 4 ? ColumnFit::Fill : column >= 5 ? ColumnFit::Number : ColumnFit::Content;
    case fd::CalendarKind::Ipos: // DATE SYMBOL COMPANY EXCHANGE | PRICE RANGE OFFER PRICE SHARES | STATUS FILED AMENDED
        return column == 2 ? ColumnFit::Fill : (column >= 4 && column <= 6) ? ColumnFit::Number : ColumnFit::Content;
    case fd::CalendarKind::Splits: // PAYABLE SYMBOL COMPANY | RATIO | OPTIONABLE
        return column == 2 ? ColumnFit::Fill : column == 3 ? ColumnFit::Number : ColumnFit::Content;
    case fd::CalendarKind::Economic: // TIME REGION EVENT PERIOD | ACTUAL CONSENSUS PRIOR REVISED FROM
        return column == 2 ? ColumnFit::Fill : column >= 4 ? ColumnFit::Number : ColumnFit::Content;
    }
    return ColumnFit::Content;
}

} // namespace calendar_panel_detail

using namespace calendar_panel_detail;

namespace {

/// A remembered result older than this gets the pane's "may be out of date · REFRESH"
/// notice (nothing reloads by itself — every call is billed).
int calp_max_age_seconds(fd::CalendarKind kind) {
    switch (kind) {
    case fd::CalendarKind::Economic:
        return 3600; // releases land through the day
    case fd::CalendarKind::Earnings:
        return 4 * 3600;
    case fd::CalendarKind::Ipos:
    case fd::CalendarKind::Splits:
        return 12 * 3600;
    }
    return 4 * 3600;
}

/// The page delivered to the pane (and kept by it across sessions): the query it
/// answers, the envelope's data, and the meta the freshness text is made from.
QJsonObject calp_make_payload(const QDate& start, const QDate& end, const QString& symbol, const QString& sort,
                              const fd::DataResult& r) {
    const fd::DataMeta& m = r.meta;
    const fd::Pagination& p = m.pagination;
    return QJsonObject{
        {QStringLiteral("q"), QJsonObject{{QStringLiteral("start"), start.toString(Qt::ISODate)},
                                          {QStringLiteral("end"), end.toString(Qt::ISODate)},
                                          {QStringLiteral("symbol"), symbol},
                                          {QStringLiteral("sort"), sort}}},
        {QStringLiteral("data"), r.data},
        {QStringLiteral("meta"),
         QJsonObject{{QStringLiteral("as_of"), m.as_of.isValid() ? m.as_of.toString(Qt::ISODateWithMs) : QString()},
                     {QStringLiteral("age"), m.age_seconds},
                     {QStringLiteral("max_age"), m.max_age_seconds},
                     {QStringLiteral("stale"), m.stale},
                     {QStringLiteral("stale_reason"), m.stale_reason},
                     {QStringLiteral("source"), m.source_name},
                     {QStringLiteral("delayed"), m.delayed_minutes},
                     {QStringLiteral("received_ms"), static_cast<double>(r.received_ms)},
                     {QStringLiteral("free"), r.from_client_cache},
                     {QStringLiteral("paged"), p.present},
                     {QStringLiteral("page"), p.page},
                     {QStringLiteral("page_size"), p.page_size},
                     {QStringLiteral("total"), p.total},
                     {QStringLiteral("total_pages"), p.total_pages},
                     {QStringLiteral("has_more"), p.has_more}}}};
}

/// The DataResult a payload was made from (enough for parse_calendar and freshness_text).
fd::DataResult calp_result_from_payload(const QJsonObject& o) {
    fd::DataResult r;
    r.data = o.value(QStringLiteral("data")).toObject();
    const QJsonObject m = o.value(QStringLiteral("meta")).toObject();
    r.meta.as_of = QDateTime::fromString(m.value(QStringLiteral("as_of")).toString(), Qt::ISODateWithMs);
    if (r.meta.as_of.isValid())
        r.meta.as_of = r.meta.as_of.toUTC();
    r.meta.age_seconds = m.value(QStringLiteral("age")).toInt();
    r.meta.max_age_seconds = m.value(QStringLiteral("max_age")).toInt();
    r.meta.stale = m.value(QStringLiteral("stale")).toBool();
    r.meta.stale_reason = m.value(QStringLiteral("stale_reason")).toString();
    r.meta.source_name = m.value(QStringLiteral("source")).toString();
    r.meta.delayed_minutes = m.value(QStringLiteral("delayed")).toInt();
    r.received_ms = static_cast<qint64>(m.value(QStringLiteral("received_ms")).toDouble());
    r.from_client_cache = m.value(QStringLiteral("free")).toBool();
    fd::Pagination& p = r.meta.pagination;
    p.present = m.value(QStringLiteral("paged")).toBool();
    p.page = m.value(QStringLiteral("page")).toInt();
    p.page_size = m.value(QStringLiteral("page_size")).toInt();
    p.total = m.value(QStringLiteral("total")).toInt();
    p.total_pages = m.value(QStringLiteral("total_pages")).toInt();
    p.has_more = m.value(QStringLiteral("has_more")).toBool();
    return r;
}

} // namespace

CalendarPanel::CalendarPanel(fd::CalendarKind kind, QWidget* parent) : QWidget(parent), kind_(kind) {
    setObjectName(QStringLiteral("calendarPanel"));
    build_ui();
    build_pane();
    retranslate();
    age_timer_ = new QTimer(this);
    age_timer_->setInterval(kAgeTickMs);
    connect(age_timer_, &QTimer::timeout, this, &CalendarPanel::update_freshness);

    // The plan table arrived / changed: the price on the preview, the agent markers and
    // whether a load may run by itself (a plan that includes market data) follow it.
    connect(&billing::PlanAccess::instance(), &billing::PlanAccess::changed, this, [this]() {
        apply_plan_access();
        update_preview();
        if (has_data_ && data_.events.isEmpty())
            show_no_events(); // its action's marker
    });

    // Cell text colours and bold fonts are set per item: rebuild the rows (keeping
    // the user's sort) and re-fit the row height and column widths once the new
    // theme / font has settled (never inside theme_changed: the fit asks for size hints).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        table_->verticalHeader()->setDefaultSectionSize(calendar_row_height());
        if (!has_data_)
            return;
        const int column = sort_column_;
        const Qt::SortOrder order = sort_order_;
        populate();
        if (column >= 0 && column < table_->columnCount()) {
            table_->horizontalHeader()->setSortIndicatorShown(true);
            table_->horizontalHeader()->setSortIndicator(column, order);
            table_->sortItems(column, order);
        }
    });
}

CalendarPanel::~CalendarPanel() {
    // Withdraw pending callbacks; never construct the service during teardown.
    if (service_used_)
        fd::FinceptDataService::instance().cancel(this);
}

QString CalendarPanel::key() const {
    return fd::calendar_name(kind_);
}

QString CalendarPanel::plan_category() {
    // /data calls are priced like /market calls (the dashboard's /data tiles use the same key).
    return QStringLiteral("market_data");
}

bool CalendarPanel::plan_includes_loads() {
    const auto& access = billing::PlanAccess::instance();
    return access.has_table() && access.included(plan_category());
}

double CalendarPanel::list_credits() const {
    // The cost per call is in the dataset catalogue once the server has been asked.
    if (!service_used_)
        return 0.0;
    const auto& svc = fd::FinceptDataService::instance();
    const auto it = svc.catalog().datasets.constFind(fd::calendar_dataset_id(kind_));
    return it != svc.catalog().datasets.constEnd() ? it->credits_per_call : 0.0;
}

void CalendarPanel::apply_plan_access() {
    if (!pane_)
        return;
    const bool included = plan_includes_loads();
    // List price + category: the pane checks what this account really pays (0 when included).
    pane_->set_cost_credits(list_credits(), plan_category());
    // Every load is billed unless the plan includes it — also while the price is not known
    // yet (no catalogue): then the pane's own rule has no cost to go by.
    const QString kind = included ? QString() : QStringLiteral("spend");
    pane_->set_agent_risk(kind);
    for (QWidget* w : {static_cast<QWidget*>(apply_btn_), static_cast<QWidget*>(refresh_btn_),
                       static_cast<QWidget*>(prev_btn_), static_cast<QWidget*>(next_btn_),
                       static_cast<QWidget*>(symbol_edit_)})
        ui::EmptyState::mark_agent_risk(w, kind); // null-safe; empty removes the marker
    // Billing rule: nothing billed loads by itself. A plan that includes market data
    // makes a load free — the tab then opens like a free view (and refreshes when old).
    pane_->set_policy(included ? ui::AutoLoadPane::Policy::AutoRun : ui::AutoLoadPane::Policy::Manual);
}

void CalendarPanel::build_ui() {
    setStyleSheet(
        QString("#calendarPanel { background:%1; }"
                "QFrame#calCtrl { background:%2; border:1px solid %3; }"
                "QLabel { background:transparent; border:0; }"
                "QLabel#calCaption { color:%4; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:1px; }"
                "QLabel#calFresh, QLabel#calPage { color:%5; font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                "QLabel#calFresh[tone=\"stale\"] { color:%6; }"
                "QLabel#calMessage { color:%4; font-size:" + fincept::ui::fonts::px(-2) + "px; padding:10px 6px; }"
                "QLabel#calMessage[tone=\"warn\"] { color:%6; }"
                "QLabel#calMessage[tone=\"error\"] { color:%7; }"
                "QPushButton#calButton { background:transparent; color:%8; border:1px solid %9; padding:3px 12px;"
                " font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton#calButton:hover { background:%8; color:%12; }"
                "QPushButton#calButton:disabled { color:%5; border-color:%3; }"
                "QDateEdit, QLineEdit, QComboBox { background:%10; color:%11; border:1px solid %3; padding:2px 6px;"
                " font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "QLineEdit:focus, QDateEdit:focus { border-color:%8; }"
                "QTableWidget { background:%2; alternate-background-color:%1; gridline-color:%3; color:%11; border:0;"
                " font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "QHeaderView::section { background:%10; color:%4; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; padding:5px 4px;"
                " border:0; border-bottom:1px solid %3; letter-spacing:1px; }"
                "QTableWidget::item { padding:2px 6px; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::TEXT_TERTIARY(), ui::colors::WARNING(), ui::colors::DANGER(), ui::colors::AMBER(),
                 ui::colors::AMBER_DIM())
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_ON_ACCENT()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    // ── Controls ─────────────────────────────────────────────────────────────
    auto* ctrl = new QFrame(this);
    header_ = ctrl;
    ctrl->setObjectName(QStringLiteral("calCtrl"));
    ctrl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(ctrl);
    hl->setContentsMargins(12, 6, 12, 6);
    hl->setSpacing(8);
    auto caption = [&]() {
        auto* l = new QLabel(ctrl);
        l->setObjectName(QStringLiteral("calCaption"));
        hl->addWidget(l);
        return l;
    };
    auto date_edit = [&](const QDate& d) {
        auto* e = new QDateEdit(d, ctrl);
        e->setCalendarPopup(true);
        e->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        e->setMinimumWidth(110);
        hl->addWidget(e);
        return e;
    };
    const QDate today = QDate::currentDate();
    from_caption_ = caption();
    from_edit_ = date_edit(today);
    to_caption_ = caption();
    to_edit_ = date_edit(today.addDays(kDefaultDays));

    if (fd::calendar_supports_symbol(kind_)) {
        symbol_caption_ = caption();
        symbol_edit_ = new QLineEdit(ctrl);
        symbol_edit_->setClearButtonEnabled(true);
        symbol_edit_->setMaximumWidth(std::max(140, QFontMetrics(ui::fonts::font(-3)).averageCharWidth() * 23));
        connect(symbol_edit_, &QLineEdit::returnPressed, this, &CalendarPanel::apply);
        symbol_edit_->setProperty("fincept_agent_risk", "spend"); // Enter = APPLY, a billed load
        hl->addWidget(symbol_edit_);
    }
    if (kind_ == fd::CalendarKind::Earnings) {
        sort_caption_ = caption();
        sort_combo_ = new QComboBox(ctrl);
        sort_combo_->addItem(QString(), QStringLiteral("date"));
        sort_combo_->addItem(QString(), QStringLiteral("market_cap"));
        hl->addWidget(sort_combo_);
    }
    apply_btn_ = new QPushButton(ctrl);
    apply_btn_->setObjectName(QStringLiteral("calButton"));
    apply_btn_->setProperty("fincept_agent_risk", "spend"); // billed /data call
    apply_btn_->setCursor(Qt::PointingHandCursor);
    connect(apply_btn_, &QPushButton::clicked, this, &CalendarPanel::apply);
    hl->addWidget(apply_btn_);
    hl->addStretch(1);
    fresh_label_ = new QLabel(ctrl);
    fresh_label_->setObjectName(QStringLiteral("calFresh"));
    hl->addWidget(fresh_label_);
    refresh_btn_ = new QPushButton(ctrl);
    refresh_btn_->setObjectName(QStringLiteral("calButton"));
    refresh_btn_->setProperty("fincept_agent_risk", "spend"); // billed /data call
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &CalendarPanel::refresh);
    hl->addWidget(refresh_btn_);
    vl->addWidget(ctrl);

    // ── Message ──────────────────────────────────────────────────────────────
    message_ = new QLabel(this);
    message_->setObjectName(QStringLiteral("calMessage"));
    message_->setWordWrap(true);
    message_->setTextFormat(Qt::RichText);
    message_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    message_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    message_->hide();
    connect(message_, &QLabel::linkActivated, this, [this](const QString& link) {
        // "Did you mean" suggestion for the symbol filter.
        if (link.startsWith(QLatin1String("sym:")) && symbol_edit_) {
            symbol_edit_->setText(link.mid(4));
            apply();
        }
    });
    vl->addWidget(message_);

    // The input signature follows the controls: a page on screen made from other
    // inputs gets the pane's "Inputs changed — RUN AGAIN" notice (never a silent run).
    connect(from_edit_, &QDateEdit::dateChanged, this, &CalendarPanel::update_signature);
    connect(to_edit_, &QDateEdit::dateChanged, this, &CalendarPanel::update_signature);
    if (symbol_edit_)
        connect(symbol_edit_, &QLineEdit::textChanged, this, &CalendarPanel::update_signature);
    if (sort_combo_)
        connect(sort_combo_, &QComboBox::currentIndexChanged, this, &CalendarPanel::update_signature);
}

void CalendarPanel::build_pane() {
    auto* vl = qobject_cast<QVBoxLayout*>(layout());

    // ── Result area: remembered page / preview / skeleton / failure page ─────
    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::Manual, this);
    pane_->preview()->primary_button()->setProperty("fincept_agent_risk", "spend"); // LOAD: billed
    auto* result = new QWidget(pane_->content_parent());
    auto* rl = new QVBoxLayout(result);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(8);
    result_stack_ = new QStackedWidget(result);

    // ── Table ────────────────────────────────────────────────────────────────
    table_ = new QTableWidget(result_stack_);
    table_->setAlternatingRowColors(true);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setWordWrap(false);
    table_->verticalHeader()->setDefaultSectionSize(calendar_row_height());
    table_->verticalHeader()->hide();
    // Column widths are set per column in apply_column_fit(): the text columns fill
    // the width, the rest stay compact. (Stretching the last section parked all the
    // spare width in one numeric column.)
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setMinimumSectionSize(48);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionsClickable(true);
    // Rows stay in the server's order (by time, or by market cap) until a header is clicked.
    connect(table_->horizontalHeader(), &QHeaderView::sectionClicked, this, &CalendarPanel::sort_by);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { open_symbol_at(row); });
    result_stack_->addWidget(table_);
    rl->addWidget(result_stack_, 1);

    // ── Pager ────────────────────────────────────────────────────────────────
    pager_ = new QWidget(result);
    auto* pl = new QHBoxLayout(pager_);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(8);
    page_label_ = new QLabel(pager_);
    page_label_->setObjectName(QStringLiteral("calPage"));
    pl->addWidget(page_label_);
    pl->addStretch(1);
    prev_btn_ = new QPushButton(pager_);
    prev_btn_->setObjectName(QStringLiteral("calButton"));
    prev_btn_->setProperty("fincept_agent_risk", "spend"); // each page is a billed call
    next_btn_ = new QPushButton(pager_);
    next_btn_->setObjectName(QStringLiteral("calButton"));
    next_btn_->setProperty("fincept_agent_risk", "spend");
    // Paging pages the query on screen (edited, unapplied controls are put back first).
    connect(prev_btn_, &QPushButton::clicked, this, [this]() {
        if (pagination_.page <= 1)
            return;
        sync_controls_to_query();
        want_page_ = pagination_.page - 1;
        pane_->run();
    });
    connect(next_btn_, &QPushButton::clicked, this, [this]() {
        if (!pagination_.has_more)
            return;
        sync_controls_to_query();
        want_page_ = pagination_.page + 1;
        pane_->run();
    });
    pl->addWidget(prev_btn_);
    pl->addWidget(next_btn_);
    rl->addWidget(pager_);
    pane_->set_content(result);
    vl->addWidget(pane_, 1);

    pane_->set_cache_key(QStringLiteral("calendars/") + key(), 14);
    pane_->set_max_age(calp_max_age_seconds(kind_));
    pane_->set_timeout(90); // the service always answers; this only guards a lost reply
    pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);
    pane_->skeleton()->set_row_count(12);
    pane_->set_requirements({ui::Requirement::signed_in()});
    // A server without the /data API is known after the first call: say so instead of
    // offering a LOAD that can only fail (re-checked on every show; the service forgets
    // it when the signed-in account changes).
    pane_->set_requirement_check([this]() {
        ui::RequirementStatus st;
        if (service_used_ && fd::FinceptDataService::instance().server_lacks_api()) {
            st.ok = false;
            st.title = tr("Calendars are not available on this server");
            st.message = fd::FinceptDataService::unavailable_text();
        }
        return st;
    });
    pane_->set_start([this](quint64 id) { start_load(id); });
    // The service cannot withdraw a sent request; a cancelled run's answer is dropped
    // by its request id when it arrives.
    pane_->set_cancel([](quint64) {});
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool from_cache) { render_payload(p, from_cache); });
    // One request at a time from the buttons (a repeat in flight would be answered by
    // the same request anyway).
    connect(pane_, &ui::AutoLoadPane::running_changed, this, [this](bool running) {
        refresh_btn_->setEnabled(!running);
        apply_btn_->setEnabled(!running);
    });
    apply_plan_access();
    update_signature();
    update_pager();
}

QStringList CalendarPanel::headers() const {
    switch (kind_) {
    case fd::CalendarKind::Earnings:
        return {tr("DATE"), tr("TIME"), tr("SYMBOL"), tr("COMPANY"), tr("EVENT"), tr("EPS EST."), tr("EPS ACTUAL"),
                tr("SURPRISE"), tr("MARKET CAP")};
    case fd::CalendarKind::Ipos:
        return {tr("DATE"), tr("SYMBOL"), tr("COMPANY"), tr("EXCHANGE"), tr("PRICE RANGE"), tr("OFFER PRICE"),
                tr("SHARES"), tr("STATUS"), tr("FILED"), tr("AMENDED")};
    case fd::CalendarKind::Splits:
        return {tr("PAYABLE"), tr("SYMBOL"), tr("COMPANY"), tr("RATIO"), tr("OPTIONABLE")};
    case fd::CalendarKind::Economic:
        return {tr("TIME (LOCAL)"), tr("REGION"), tr("EVENT"),  tr("PERIOD"),
                tr("ACTUAL"),       tr("CONSENSUS"), tr("PRIOR"), tr("REVISED FROM")};
    }
    return {};
}

int CalendarPanel::symbol_column() const {
    switch (kind_) {
    case fd::CalendarKind::Earnings:
        return 2;
    case fd::CalendarKind::Ipos:
    case fd::CalendarKind::Splits:
        return 1;
    case fd::CalendarKind::Economic:
        return -1;
    }
    return -1;
}

void CalendarPanel::retranslate() {
    from_caption_->setText(tr("FROM"));
    to_caption_->setText(tr("TO"));
    if (symbol_caption_)
        symbol_caption_->setText(tr("SYMBOL"));
    if (symbol_edit_) {
        symbol_edit_->setPlaceholderText(tr("All"));
        symbol_edit_->setToolTip(tr("Only this company (a symbol filter allows a range of up to a year)"));
    }
    if (sort_caption_)
        sort_caption_->setText(tr("ORDER"));
    if (sort_combo_) {
        sort_combo_->setItemText(0, tr("By time"));
        sort_combo_->setItemText(1, tr("Largest first"));
    }
    apply_btn_->setText(tr("APPLY"));
    apply_btn_->setToolTip(tr("Load the first page for these dates"));
    refresh_btn_->setText(tr("⟳ REFRESH"));
    refresh_btn_->setToolTip(tr("Ask for newer data. Each data call is billed like a /market call; while the source "
                                "has nothing newer the answer comes from this terminal's copy, free."));
    prev_btn_->setText(tr("◀ PREV"));
    next_btn_->setText(tr("NEXT ▶"));
    table_->setToolTip(symbol_column() >= 0 ? tr("Double-click a row to open its symbol in Equity Research · click a "
                                                 "header to sort this page")
                                            : tr("Click a header to sort this page"));
    if (has_data_)
        populate();
    else
        table_->setHorizontalHeaderLabels(headers());
    if (has_data_ && data_.events.isEmpty())
        show_no_events();
    update_pager();
    update_freshness();
    update_preview();
}

// ── Queries ──────────────────────────────────────────────────────────────────

void CalendarPanel::activate() {
    // Nothing is requested here: the pane shows the last loaded page (with its
    // "as of") or the preview. The dataset catalogue — and with it the cost per
    // call — may have arrived since the preview was drawn.
    if (service_used_ && !service_connected_) {
        service_connected_ = true;
        connect(&fd::FinceptDataService::instance(), &fd::FinceptDataService::availability_changed, this, [this]() {
            apply_plan_access(); // the catalogue's price per call
            update_preview();
            pane_->recheck_requirements();
        });
    }
    apply_plan_access();
    update_preview();
}

bool CalendarPanel::inputs_valid(QString* why) const {
    const QDate start = from_edit_->date();
    const QDate end = to_edit_->date();
    const QString symbol = symbol_edit_ ? symbol_edit_->text().trimmed() : QString();
    // The server's limits (calendars.py): refuse locally rather than send a 422.
    if (end < start) {
        *why = tr("The end date is before the start date.");
        return false;
    }
    const int max_days = fd::calendar_max_days(kind_, !symbol.isEmpty());
    if (start.daysTo(end) > max_days) {
        *why = tr("The range may span at most %n day(s) for this calendar.", nullptr, max_days);
        return false;
    }
    return true;
}

void CalendarPanel::apply() {
    QString why;
    if (!inputs_valid(&why)) {
        set_message(why.toHtmlEscaped(), QStringLiteral("warn"));
        return;
    }
    set_message(QString(), QString());
    want_page_ = 1; // APPLY always starts at the first page
    want_refresh_ = false;
    pane_->run();
}

void CalendarPanel::refresh() {
    want_page_ = pagination_.present ? std::max(1, pagination_.page) : 1;
    want_refresh_ = has_data_;
    if (has_data_)
        sync_controls_to_query();
    pane_->run();
}

void CalendarPanel::start_load(quint64 id) {
    QString why;
    if (!inputs_valid(&why)) {
        set_message(why.toHtmlEscaped(), QStringLiteral("warn"));
        pane_->fail(id, why, tr("Check the dates"));
        return;
    }
    const QDate start = from_edit_->date();
    const QDate end = to_edit_->date();
    const QString symbol = symbol_edit_ ? symbol_edit_->text().trimmed().toUpper() : QString();
    const QString sort = sort_combo_ ? sort_combo_->currentData().toString() : QString();
    // The same query as the page on screen: PREV / NEXT / REFRESH apply (a run without
    // a page asked for — the pane's REFRESH / RUN AGAIN — keeps the page on screen);
    // new inputs start at page 1.
    const bool same = has_data_ && start == q_start_ && end == q_end_ && symbol == q_symbol_ && sort == q_sort_;
    const int page = !same ? 1 : want_page_ > 0 ? want_page_ : std::max(1, pagination_.page);
    const bool refresh = same && want_refresh_;
    want_page_ = 0;
    want_refresh_ = false;
    q_start_ = start;
    q_end_ = end;
    q_symbol_ = symbol;
    q_sort_ = sort;
    set_message(QString(), QString());

    auto& svc = fd::FinceptDataService::instance();
    service_used_ = true;
    if (!service_connected_)
        activate(); // follow the catalogue from now on
    svc.fetch_calendar(
        kind_, start, end, symbol, sort, page, kPageSize, this,
        [this, id, start, end, symbol, sort](const fd::DataResult& r) {
            if (!pane_->is_current(id))
                return; // another page / range was asked for meanwhile
            if (r.ok()) {
                pane_->deliver(id, calp_make_payload(start, end, symbol, sort, r));
                return;
            }
            on_answer(id, r);
        },
        refresh);
}

void CalendarPanel::on_answer(quint64 id, const fd::DataResult& r) {
    using fd::ErrorKind;
    const fd::DataError& e = r.error;
    const QString text = fd::FinceptDataService::error_text(e);
    QString technical;
    if (e.status > 0 || !e.code.isEmpty())
        technical = QStringLiteral("HTTP %1 %2 %3").arg(e.status).arg(e.code, e.message).trimmed();

    // A symbol the server does not know, or a parameter it refused: an input problem,
    // said next to the inputs (with "did you mean" links). The page on screen stays.
    if ((e.kind == ErrorKind::NotFound && !q_symbol_.isEmpty()) || e.kind == ErrorKind::Invalid) {
        QString plain = text;
        QString html = text.toHtmlEscaped();
        if (e.kind == ErrorKind::NotFound && !e.suggestions.isEmpty() && symbol_edit_) {
            const qsizetype at = plain.indexOf(QLatin1String("Did you mean"), 0, Qt::CaseInsensitive);
            if (at > 0)
                plain = plain.left(at).trimmed();
            if (!has_data_) {
                // Nothing on screen: the failure page lists the suggestions as rows — click
                // (or Enter) on one loads that symbol, the same as the links beside the inputs.
                ui::EmptyState::Spec page = ui::EmptyState::error_spec(
                    plain.isEmpty() ? tr("No company matches \"%1\".").arg(q_symbol_) : plain,
                    tr("Symbol not found"), tr("EDIT SYMBOL"), technical);
                page.glyph = QStringLiteral("?");
                page.glyph_tone = ui::EmptyState::Tone::Info;
                for (const QString& s : e.suggestions)
                    page.details.append({s, tr("Load this symbol instead")});
                page.details_heading = tr("DID YOU MEAN");
                pane_->fail(
                    id, page,
                    [this]() {
                        symbol_edit_->setFocus(Qt::OtherFocusReason);
                        symbol_edit_->selectAll();
                    },
                    ui::AutoLoadPane::ActionFn(),
                    [this](int, const QString& sym) {
                        if (sym.isEmpty())
                            return;
                        symbol_edit_->setText(sym);
                        apply();
                    },
                    // A row loads that symbol: a billed call unless the plan includes it.
                    plan_includes_loads() ? QString() : QStringLiteral("spend"));
                return;
            }
            QStringList links;
            for (const QString& s : e.suggestions)
                // No inline colour: the link takes the palette link colour, which follows the theme.
                links << QStringLiteral("<a href=\"sym:%1\">%1</a>").arg(s.toHtmlEscaped());
            html = plain.toHtmlEscaped() + QLatin1Char(' ') +
                   tr("Did you mean: %1?").arg(links.join(QStringLiteral(", ")));
        }
        set_message(html, e.kind == ErrorKind::NotFound ? QString() : QStringLiteral("warn"));
        pane_->cancel();
        return;
    }

    switch (e.kind) {
    case ErrorKind::ServerLacksApi:
    case ErrorKind::DatasetUnavailable:
    case ErrorKind::NotConfigured: {
        ui::EmptyState::Spec page;
        page.glyph = QStringLiteral("○");
        page.glyph_tone = ui::EmptyState::Tone::Info;
        page.title = e.kind == ErrorKind::ServerLacksApi ? tr("Calendars are not available on this server")
                                                         : tr("This calendar is not available on this server");
        page.message = text;
        page.primary = tr("TRY AGAIN");
        page.technical = technical;
        pane_->fail(id, page, ui::AutoLoadPane::ActionFn());
        return;
    }
    case ErrorKind::NoCredits: {
        ui::EmptyState::Spec page;
        page.glyph = QStringLiteral("¢");
        page.glyph_tone = ui::EmptyState::Tone::Warning;
        page.title = tr("Not enough credits");
        // "Market data is included with Pro …" when the server's plan table says so.
        page.message = billing::PlanAccess::instance().with_upgrade_note(text, plan_category());
        page.primary = tr("VIEW PLANS");
        page.secondary = tr("RETRY");
        page.secondary_agent_risk = QStringLiteral("spend"); // RETRY sends the billed call again
        page.technical = technical;
        pane_->fail(
            id, page,
            [this]() { ui::open_setup_link(ui::SetupLink{QStringLiteral("plans"), QString(), QString()}, this); },
            [this]() { pane_->run(); });
        return;
    }
    case ErrorKind::NotSignedIn:
        pane_->fail(id, text, tr("Sign in to load this calendar"), technical);
        pane_->recheck_requirements();
        return;
    case ErrorKind::RateLimited:
    case ErrorKind::Throttled:
        pane_->fail(id, text, tr("Try again shortly"), technical);
        return;
    case ErrorKind::NotFound:
        pane_->fail(id, text, tr("Nothing found"), technical);
        return;
    default:
        pane_->fail(id, text, tr("Could not load the calendar"), technical);
        return;
    }
}

void CalendarPanel::render_payload(const QJsonValue& payload, bool from_cache) {
    const QJsonObject o = payload.toObject();
    const QJsonObject q = o.value(QStringLiteral("q")).toObject();
    q_start_ = QDate::fromString(q.value(QStringLiteral("start")).toString(), Qt::ISODate);
    q_end_ = QDate::fromString(q.value(QStringLiteral("end")).toString(), Qt::ISODate);
    q_symbol_ = q.value(QStringLiteral("symbol")).toString();
    q_sort_ = q.value(QStringLiteral("sort")).toString();
    remembered_ = from_cache;
    if (from_cache)
        sync_controls_to_query(); // the controls say what the saved page is for

    last_ok_ = calp_result_from_payload(o);
    data_ = fd::parse_calendar(kind_, last_ok_.data);
    pagination_ = last_ok_.meta.pagination;
    has_data_ = true;
    sort_column_ = -1;
    populate();
    if (data_.events.isEmpty())
        show_no_events();
    else
        result_stack_->setCurrentWidget(table_);
    update_pager();
    update_freshness();
    if (isVisible())
        age_timer_->start();
}

QString CalendarPanel::input_signature() const {
    QVariantMap m;
    m.insert(QStringLiteral("start"), from_edit_->date().toString(Qt::ISODate));
    m.insert(QStringLiteral("end"), to_edit_->date().toString(Qt::ISODate));
    if (symbol_edit_)
        m.insert(QStringLiteral("symbol"), symbol_edit_->text().trimmed().toUpper());
    if (sort_combo_)
        m.insert(QStringLiteral("sort"), sort_combo_->currentData().toString());
    return ui::autoload::signature_of(m);
}

void CalendarPanel::update_signature() {
    if (!pane_ || syncing_controls_)
        return;
    pane_->set_signature(input_signature());
    update_preview();
}

void CalendarPanel::sync_controls_to_query() {
    if (!q_start_.isValid() || !q_end_.isValid())
        return;
    {
        syncing_controls_ = true;
        const QSignalBlocker b1(from_edit_);
        const QSignalBlocker b2(to_edit_);
        from_edit_->setDate(q_start_);
        to_edit_->setDate(q_end_);
        if (symbol_edit_) {
            const QSignalBlocker b3(symbol_edit_);
            symbol_edit_->setText(q_symbol_);
        }
        if (sort_combo_) {
            const QSignalBlocker b4(sort_combo_);
            const int idx = sort_combo_->findData(q_sort_);
            if (idx >= 0)
                sort_combo_->setCurrentIndex(idx);
        }
        syncing_controls_ = false;
    }
    update_signature();
}

void CalendarPanel::update_preview() {
    if (!pane_)
        return;
    QString title;
    QString what;
    QString run;
    QString loading;
    switch (kind_) {
    case fd::CalendarKind::Earnings:
        title = tr("Earnings calendar");
        what = tr("Company earnings dates for the chosen days, with the timing, EPS estimate, actual, surprise and "
                  "market cap — biggest companies first or by time.");
        run = tr("LOAD EARNINGS");
        loading = tr("Loading earnings dates…");
        break;
    case fd::CalendarKind::Ipos:
        title = tr("IPO calendar");
        what = tr("Upcoming and recent listings: exchange, price range, offer price, shares offered and deal status.");
        run = tr("LOAD IPOS");
        loading = tr("Loading IPOs…");
        break;
    case fd::CalendarKind::Splits:
        title = tr("Stock splits");
        what = tr("Forward and reverse splits with their payable date and ratio.");
        run = tr("LOAD SPLITS");
        loading = tr("Loading stock splits…");
        break;
    case fd::CalendarKind::Economic:
        title = tr("Economic calendar");
        what = tr("Macro releases by region in your local time, with actual, consensus, prior and revised values.");
        run = tr("LOAD ECONOMIC EVENTS");
        loading = tr("Loading economic releases…");
        break;
    }

    QList<QPair<QString, QString>> details;
    details.append({tr("From"), from_edit_->date().toString(Qt::ISODate)});
    details.append({tr("To"), to_edit_->date().toString(Qt::ISODate)});
    if (symbol_edit_) {
        const QString sym = symbol_edit_->text().trimmed().toUpper();
        details.append({tr("Symbol"), sym.isEmpty() ? tr("All companies") : sym});
    }
    if (sort_combo_)
        details.append({tr("Order"), sort_combo_->currentText()});
    details.append({tr("Page"), tr("First %1 events").arg(kPageSize)});

    // The cost per call is in the dataset catalogue once the server has been asked; what
    // this account pays comes from the server's plan table (Pro may pay less or nothing).
    // Without a table: the list price, as before.
    const double credits = list_credits();
    const auto& access = billing::PlanAccess::instance();
    QString note;
    if (plan_includes_loads()) {
        note = tr("Included with your plan — no credits used");
    } else if (access.has_table() && credits > 0.0) {
        note = tr("%1 credit(s) per load on your plan · a repeat while the source has nothing newer is free")
                   .arg(QString::number(access.effective_cost(plan_category(), credits), 'g', 3));
    } else if (const billing::PlanPrice* row = access.price(plan_category());
               row && credits <= 0.0 && !(access.is_paid() ? row->pro_price : row->free_price).isEmpty()) {
        note = tr("%1 on your plan · a repeat while the source has nothing newer is free")
                   .arg(access.is_paid() ? row->pro_price : row->free_price);
    } else {
        note = credits > 0.0 ? tr("%1 credit(s) per load · a repeat while the source has nothing newer is free")
                                   .arg(QString::number(credits, 'g', 3))
                             : tr("Each load is billed like a /market call · a repeat while the source has nothing "
                                  "newer is free");
    }
    pane_->preview()->apply(ui::EmptyState::preview_spec(title, what, run, note,
                                                         plan_includes_loads() ? ui::EmptyState::Tone::Neutral
                                                                               : ui::EmptyState::Tone::Warning,
                                                         ui::LoadingSkeleton::Shape::Table, details,
                                                         tr("WHAT WILL BE LOADED")));
    pane_->set_loading_text(loading);
}

void CalendarPanel::show_no_events() {
    if (!no_events_) {
        no_events_ = new ui::EmptyState(result_stack_);
        result_stack_->addWidget(no_events_);
        connect(no_events_, &ui::EmptyState::primary_clicked, this, [this]() {
            if (symbol_edit_ && !q_symbol_.isEmpty()) {
                symbol_edit_->clear(); // every company in the same range
            } else {
                const int days = std::min(30, fd::calendar_max_days(kind_, false));
                const QSignalBlocker block(from_edit_);
                from_edit_->setDate(q_start_.isValid() ? q_start_ : QDate::currentDate());
                to_edit_->setDate(from_edit_->date().addDays(days));
            }
            apply();
        });
    }
    const QString range = tr("%1 to %2").arg(q_start_.toString(Qt::ISODate), q_end_.toString(Qt::ISODate));
    QString message = q_symbol_.isEmpty() ? tr("Nothing is scheduled from %1.").arg(range)
                                          : tr("Nothing is scheduled for %1 from %2.").arg(q_symbol_, range);
    QString action;
    const int wide = std::min(30, fd::calendar_max_days(kind_, false));
    if (symbol_edit_ && !q_symbol_.isEmpty())
        action = tr("SHOW ALL COMPANIES");
    else if (q_start_.isValid() && q_start_.daysTo(q_end_) < wide)
        action = tr("NEXT %1 DAYS").arg(wide);
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(tr("No events in this range"), message, action);
    spec.glyph = QStringLiteral("◷");
    spec.primary_tooltip = action.isEmpty() ? QString() : tr("Loads again — each load is billed like a /market call");
    if (!action.isEmpty() && !plan_includes_loads())
        spec.primary_agent_risk = QStringLiteral("spend"); // the action loads again (billed)
    no_events_->apply(spec);
    result_stack_->setCurrentWidget(no_events_);
}

// ── Rendering ────────────────────────────────────────────────────────────────

QString CalendarPanel::timing_text(const fd::CalendarEvent& e) const {
    // Yahoo's startdatetimetype: before open, after close, time not supplied, or the time itself.
    if (e.timing == QLatin1String("BMO"))
        return tr("Before open");
    if (e.timing == QLatin1String("AMC"))
        return tr("After close");
    if (e.timing == QLatin1String("TNS"))
        return tr("Not supplied");
    if (e.when.isValid())
        return e.when.toLocalTime().toString(QStringLiteral("HH:mm"));
    return e.timing;
}

void CalendarPanel::populate() {
    const QStringList hdr = headers();
    table_->setUpdatesEnabled(false);
    table_->setSortingEnabled(false);
    table_->horizontalHeader()->setSortIndicatorShown(false);
    // No section may measure its contents while the rows go in: fill first, fit once.
    for (int c = 0; c < table_->columnCount(); ++c)
        table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Interactive);
    table_->clearContents();
    table_->setColumnCount(static_cast<int>(hdr.size()));
    table_->setHorizontalHeaderLabels(hdr);
    table_->setRowCount(static_cast<int>(data_.events.size()));

    const QColor sym_fg(ui::colors::CYAN());
    QFont bold = table_->font();
    bold.setBold(true);
    auto symbol_cell = [&](const QString& sym) {
        auto* item = text_item(sym, sym_fg);
        item->setFont(bold);
        item->setData(kSymbolRole, sym);
        if (!sym.isEmpty())
            item->setToolTip(tr("Double-click to open %1 in Equity Research").arg(sym));
        return item;
    };

    for (int r = 0; r < data_.events.size(); ++r) {
        const auto& e = data_.events[r];
        switch (kind_) {
        case fd::CalendarKind::Earnings: {
            table_->setItem(r, 0, when_item(e.when, false));
            auto* timing = text_item(timing_text(e));
            timing->setToolTip(e.timing);
            table_->setItem(r, 1, timing);
            table_->setItem(r, 2, symbol_cell(e.symbol));
            table_->setItem(r, 3, text_item(e.company));
            table_->setItem(r, 4, text_item(e.event));
            table_->setItem(r, 5, number_item(e.eps_estimate, num(e.eps_estimate)));
            table_->setItem(r, 6, number_item(e.eps_actual, num(e.eps_actual)));
            table_->setItem(r, 7, number_item(e.eps_surprise_pct,
                                              fd::has(e.eps_surprise_pct)
                                                  ? (e.eps_surprise_pct > 0 ? QStringLiteral("+") : QString()) +
                                                        QString::number(e.eps_surprise_pct, 'f', 2) + QLatin1Char('%')
                                                  : equity_fmt::dash(),
                                              sign_color(e.eps_surprise_pct)));
            table_->setItem(r, 8, number_item(e.market_cap, equity_fmt::large(e.market_cap)));
            break;
        }
        case fd::CalendarKind::Ipos: {
            QString range = equity_fmt::dash();
            double range_key = fd::kNaN;
            if (fd::has(e.price_from) && fd::has(e.price_to) && e.price_from != e.price_to) {
                range = QStringLiteral("%1 – %2").arg(num(e.price_from), num(e.price_to));
                range_key = e.price_from;
            } else if (fd::has(e.price_from) || fd::has(e.price_to)) {
                range_key = fd::has(e.price_from) ? e.price_from : e.price_to;
                range = num(range_key);
            }
            const QString ccy = e.currency.isEmpty() ? QString() : QLatin1Char(' ') + e.currency;
            table_->setItem(r, 0, when_item(e.when, false));
            table_->setItem(r, 1, symbol_cell(e.symbol));
            table_->setItem(r, 2, text_item(e.company));
            table_->setItem(r, 3, text_item(e.exchange));
            table_->setItem(r, 4, number_item(range_key, fd::has(range_key) ? range + ccy : range));
            table_->setItem(r, 5, number_item(e.offer_price, fd::has(e.offer_price) ? num(e.offer_price) + ccy
                                                                                     : equity_fmt::dash()));
            table_->setItem(r, 6, number_item(e.shares, fd::has(e.shares) ? equity_fmt::large(e.shares) : equity_fmt::dash()));
            table_->setItem(r, 7, text_item(e.deal_type));
            table_->setItem(r, 8, when_item(e.filing_date, false));
            table_->setItem(r, 9, when_item(e.amended_date, false));
            break;
        }
        case fd::CalendarKind::Splits: {
            // "5-for-1" (forward) / "1-for-30" (reverse); the ratio sorts by new ÷ old.
            QString ratio = equity_fmt::dash();
            double key = fd::kNaN;
            if (fd::has(e.new_shares) && fd::has(e.old_shares) && e.new_shares > 0 && e.old_shares > 0) {
                ratio = tr("%1-for-%2").arg(QString::number(e.new_shares, 'g', 6), QString::number(e.old_shares, 'g', 6));
                key = e.new_shares / e.old_shares;
            }
            table_->setItem(r, 0, when_item(e.when, false));
            table_->setItem(r, 1, symbol_cell(e.symbol));
            table_->setItem(r, 2, text_item(e.company));
            table_->setItem(r, 3, number_item(key, ratio,
                                              fd::has(key) ? QColor(key >= 1.0 ? ui::colors::POSITIVE()
                                                                               : ui::colors::WARNING())
                                                           : QColor()));
            table_->setItem(r, 4, text_item(e.optionable ? tr("Yes") : tr("No")));
            break;
        }
        case fd::CalendarKind::Economic: {
            table_->setItem(r, 0, when_item(e.when, true));
            table_->setItem(r, 1, text_item(e.region));
            table_->setItem(r, 2, text_item(e.event));
            table_->setItem(r, 3, text_item(e.period));
            auto* actual = number_item(e.actual, num(e.actual));
            actual->setFont(bold);
            table_->setItem(r, 4, actual);
            table_->setItem(r, 5, number_item(e.consensus, num(e.consensus)));
            table_->setItem(r, 6, number_item(e.prior, num(e.prior)));
            table_->setItem(r, 7, number_item(e.revised_from, num(e.revised_from)));
            break;
        }
        }
    }
    apply_column_fit();
    table_->setUpdatesEnabled(true);
}

void CalendarPanel::apply_column_fit() {
    // Dates, symbols and numbers are sized to their content once per fill (never in
    // ResizeToContents mode, which re-measures every row on every change) and the
    // free-text columns share the rest, so the table always spans the panel: at
    // 1440 px the company / event names get the room instead of an empty band on
    // the right. Number headers are right-aligned over their values.
    auto* hh = table_->horizontalHeader();
    for (int c = 0; c < table_->columnCount(); ++c) {
        const ColumnFit fit = column_fit(kind_, c);
        if (fit == ColumnFit::Fill) {
            hh->setSectionResizeMode(c, QHeaderView::Stretch);
        } else {
            hh->setSectionResizeMode(c, QHeaderView::Interactive);
            table_->resizeColumnToContents(c);
        }
        if (QTableWidgetItem* h = table_->horizontalHeaderItem(c))
            h->setTextAlignment((fit == ColumnFit::Number ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    }
}

void CalendarPanel::sort_by(int column) {
    if (column < 0 || table_->rowCount() == 0)
        return;
    sort_order_ = (column == sort_column_ && sort_order_ == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                                              : Qt::AscendingOrder;
    sort_column_ = column;
    table_->horizontalHeader()->setSortIndicatorShown(true);
    table_->horizontalHeader()->setSortIndicator(column, sort_order_);
    table_->sortItems(column, sort_order_);
}

void CalendarPanel::update_pager() {
    const auto& p = pagination_;
    if (!has_data_ || !p.present || data_.events.isEmpty()) {
        pager_->setVisible(false);
        return;
    }
    pager_->setVisible(true);
    const int pages = std::max(1, p.total_pages);
    page_label_->setText(tr("Page %1 of %2 · %n event(s)", nullptr, p.total).arg(p.page).arg(pages));
    prev_btn_->setEnabled(p.page > 1);
    next_btn_->setEnabled(p.has_more);
    prev_btn_->setVisible(pages > 1);
    next_btn_->setVisible(pages > 1);
}

void CalendarPanel::update_freshness() {
    if (!has_data_) {
        fresh_label_->clear();
        return;
    }
    fresh_label_->setText(fd::FinceptDataService::freshness_text(last_ok_, QDateTime::currentMSecsSinceEpoch()));
    const QString tone = last_ok_.meta.stale ? QStringLiteral("stale") : QString();
    if (fresh_label_->property("tone").toString() != tone) {
        fresh_label_->setProperty("tone", tone);
        repolish(fresh_label_);
    }
    fresh_label_->setToolTip(remembered_ ? tr("Saved from your last load — REFRESH asks for newer data (billed).")
                             : last_ok_.from_client_cache ? tr("Shown from this terminal's copy — no new request was made.")
                                                          : QString());
}

void CalendarPanel::set_message(const QString& html, const QString& tone) {
    message_->setText(html);
    message_->setVisible(!html.isEmpty());
    if (message_->property("tone").toString() != tone) {
        message_->setProperty("tone", tone);
        repolish(message_);
    }
}

void CalendarPanel::open_symbol_at(int row) {
    const int col = symbol_column();
    if (col < 0)
        return;
    const QTableWidgetItem* item = table_->item(row, col);
    const QString sym = item ? item->data(kSymbolRole).toString().trimmed() : QString();
    if (sym.isEmpty())
        return;
    // nav.open_symbol navigates first (constructing Equity Research if needed), then
    // hands the symbol over through IGroupLinked — the same path Markets / Watchlist use.
    EventBus::instance().publish(QStringLiteral("nav.open_symbol"),
                                 {{QStringLiteral("screen_id"), QStringLiteral("equity_research")},
                                  {QStringLiteral("symbol"), sym}});
}

// ── State + events ───────────────────────────────────────────────────────────

QVariantMap CalendarPanel::save_state() const {
    QVariantMap s;
    if (symbol_edit_ && !symbol_edit_->text().trimmed().isEmpty())
        s.insert(QStringLiteral("symbol"), symbol_edit_->text().trimmed());
    if (sort_combo_)
        s.insert(QStringLiteral("sort"), sort_combo_->currentData().toString());
    return s;
}

void CalendarPanel::restore_state(const QVariantMap& state) {
    // Dates are not restored: the calendar always opens on the coming week.
    if (symbol_edit_)
        symbol_edit_->setText(state.value(QStringLiteral("symbol")).toString());
    if (sort_combo_) {
        const int idx = sort_combo_->findData(state.value(QStringLiteral("sort")).toString());
        if (idx >= 0)
            sort_combo_->setCurrentIndex(idx);
    }
}

void CalendarPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (has_data_) {
        update_freshness();
        age_timer_->start();
    }
}

void CalendarPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    age_timer_->stop();
}

void CalendarPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

} // namespace fincept::screens
