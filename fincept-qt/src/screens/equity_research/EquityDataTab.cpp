// src/screens/equity_research/EquityDataTab.cpp
#include "screens/equity_research/EquityDataTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "services/billing/PlanAccess.h"
#include "services/fincept_data/FinceptDataService.h"
#include "storage/cache/CacheManager.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fd = services::fincept_data;

namespace equity_data_tab_detail {

/// The age line is re-rendered this often while the tab is visible (no request).
constexpr int kAgeTickMs = 30 * 1000;

void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// Table row height: 24px at the default font size, growing with the user's font.
int data_row_height() {
    return std::max(24, QFontMetrics(ui::fonts::font(-3)).height() + 10);
}

/// Placeholder for the link colour in the message line's HTML, filled in from the
/// active theme whenever the message is (re)shown.
const QLatin1String kLinkColor("@LINK@");

} // namespace equity_data_tab_detail

using namespace equity_data_tab_detail;

EquityDataTab::EquityDataTab(const QString& object_name, QWidget* parent) : QWidget(parent) {
    setObjectName(object_name);
    build_frame();
    age_timer_ = new QTimer(this);
    age_timer_->setInterval(kAgeTickMs);
    connect(age_timer_, &QTimer::timeout, this, &EquityDataTab::update_freshness);
    // Table cells carry their colours as item brushes, the rows are sized for the font and
    // the message line is rich text with an inline link colour: re-render all of it once the
    // appearance change has settled (row sizing inside theme_changed lays out mid-restyle).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        const int row_h = data_row_height();
        for (auto* t : findChildren<QTableWidget*>())
            t->verticalHeader()->setDefaultSectionSize(row_h);
        const QString html = message_->property("raw_html").toString();
        if (!html.isEmpty())
            message_->setText(QString(html).replace(kLinkColor, QString::fromLatin1(ui::colors::AMBER())));
        retranslate();
    });
}

EquityDataTab::~EquityDataTab() {
    // Withdraw callbacks still waiting for this tab. Only when the service was used:
    // instance() would otherwise construct it during teardown.
    if (service_used_)
        fd::FinceptDataService::instance().cancel(this);
}

void EquityDataTab::build_frame() {
    // One stylesheet for the tab; children are styled by objectName (same look as Peers / Filings).
    setStyleSheet(
        QString("#%1 { background:%2; }"
                "QFrame#fdCtrl { background:%3; border:1px solid %4; }"
                "QLabel { background:transparent; border:0; }"
                "QLabel#fdCaption { color:%5; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:1px; }"
                "QLabel#fdSection { color:%6; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; letter-spacing:1px; padding-top:8px; }"
                "QLabel#fdFresh { color:%7; font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                "QLabel#fdFresh[tone=\"stale\"] { color:%8; }"
                "QLabel#fdNote { color:%7; font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                "QLabel#fdMessage { color:%5; font-size:" + fincept::ui::fonts::px(-2) + "px; padding:10px 6px; }"
                "QLabel#fdMessage[tone=\"warn\"] { color:%8; }"
                "QLabel#fdMessage[tone=\"error\"] { color:%9; }"
                "QLabel#fdStatCaption { color:%7; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:1px; }"
                "QLabel#fdStatValue { color:%10; font-size:" + fincept::ui::fonts::px(0) + "px; font-weight:700; }"
                "QPushButton#fdButton { background:transparent; color:%6; border:1px solid %11; padding:3px 12px;"
                " font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton#fdButton:hover { background:%6; color:%13; }"
                "QPushButton#fdButton:disabled { color:%7; border-color:%4; }"
                "QComboBox { background:%12; color:%10; border:1px solid %4; padding:2px 8px; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "QScrollArea { background:transparent; border:0; }"
                "QTableWidget { background:%3; alternate-background-color:%2; gridline-color:%4; color:%10; border:0;"
                " font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "QHeaderView::section { background:%12; color:%5; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; padding:5px 4px;"
                " border:0; border-bottom:1px solid %4; letter-spacing:1px; }"
                "QTableWidget::item { padding:2px 6px; }")
            .arg(objectName(), ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(),
                 ui::colors::TEXT_SECONDARY(), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(), ui::colors::WARNING(),
                 ui::colors::NEGATIVE())
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::AMBER_DIM(), ui::colors::BG_RAISED(),
                 ui::colors::TEXT_ON_ACCENT()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    // ── Header row: tab controls | freshness | refresh ───────────────────────
    auto* ctrl = new QFrame(this);
    header_ = ctrl;
    ctrl->setObjectName(QStringLiteral("fdCtrl"));
    ctrl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(ctrl);
    hl->setContentsMargins(12, 6, 12, 6);
    hl->setSpacing(10);
    controls_ = new QHBoxLayout;
    controls_->setContentsMargins(0, 0, 0, 0);
    controls_->setSpacing(8);
    hl->addLayout(controls_);
    hl->addStretch(1);
    fresh_label_ = new QLabel(ctrl);
    fresh_label_->setObjectName(QStringLiteral("fdFresh"));
    hl->addWidget(fresh_label_);
    refresh_btn_ = make_button(QString(), ctrl);
    refresh_btn_->setProperty("fincept_agent_risk", "spend"); // billed /data call
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        // LOAD (nothing loaded yet) asks normally; REFRESH asks for newer data.
        if (!symbol_.isEmpty())
            request(/*refresh=*/has_data_);
    });
    hl->addWidget(refresh_btn_);
    vl->addWidget(ctrl);

    // ── Message line (failure reason / notice) ───────────────────────────────
    message_ = new QLabel(this);
    message_->setObjectName(QStringLiteral("fdMessage"));
    message_->setWordWrap(true);
    message_->setTextFormat(Qt::RichText);
    message_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    message_->setOpenExternalLinks(false);
    message_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    message_->hide();
    connect(message_, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link.startsWith(QLatin1String("sym:")))
            emit research_requested(link.mid(4));
    });
    vl->addWidget(message_);

    // ── Body ─────────────────────────────────────────────────────────────────
    body_ = new QWidget(this);
    body_layout_ = new QVBoxLayout(body_);
    body_layout_->setContentsMargins(0, 0, 0, 0);
    body_layout_->setSpacing(6);
    vl->addWidget(body_, 1);
    // Takes the space while the body is hidden (notice / failure without data), so the
    // header and the message stay at the top instead of being spread over the tab.
    vl->addStretch(0);

    overlay_ = new ui::LoadingOverlay(this);
    retranslate_frame();
}

void EquityDataTab::retranslate_frame() {
    if (offering_) {
        refresh_btn_->setText(tr("LOAD"));
        refresh_btn_->setToolTip(tr("Load this tab's data. Each data call is billed like a /market call."));
        return;
    }
    refresh_btn_->setText(tr("⟳ REFRESH"));
    refresh_btn_->setToolTip(tr("Ask for newer data. Each data call is billed like a /market call; while the source "
                                "has nothing newer the answer comes from this terminal's copy, free."));
}

// ── Symbol + requests ────────────────────────────────────────────────────────

void EquityDataTab::set_symbol(const QString& symbol) {
    if (symbol.isEmpty() || symbol == symbol_)
        return;
    symbol_ = symbol;
    has_data_ = false;
    showing_remembered_ = false;
    unavailable_shown_ = false;
    last_ok_ = {};
    age_timer_->stop();
    fresh_label_->clear();
    set_message(QString(), QString());
    header_->show();
    body_->show();
    refresh_btn_->show();
    clear_view();
    show_remembered_or_offer(); // billed: never a request by itself (see the header)
}

// ── Remembered answers / LOAD offer ──────────────────────────────────────────

void EquityDataTab::show_remembered_or_offer() {
    // A plan that includes market data at no credit cost loads by itself, as a free tab would.
    const auto& access = billing::PlanAccess::instance();
    if (access.has_table() && access.included(QStringLiteral("market_data"))) {
        request(/*refresh=*/false);
        return;
    }
    fd::DataResult kept;
    if (recall(&kept)) {
        // Shown exactly as an answer, with its "as of"; REFRESH asks for newer data.
        replaying_ = true;
        showing_remembered_ = true;
        finish(kept);
        replaying_ = false;
        if (apply_remembered(kept))
            return;
        showing_remembered_ = false;
        has_data_ = false;
        clear_view();
    }
    show_load_offer();
}

void EquityDataTab::show_load_offer() {
    offering_ = true;
    has_data_ = false;
    age_timer_->stop();
    fresh_label_->clear();
    body_->hide();
    retranslate_frame();
    set_message(tr("Nothing is loaded for %1 yet. This tab's data is billed per load, like a /market call — press "
                   "LOAD to fetch it.")
                    .arg(symbol_.toHtmlEscaped()),
                QString());
}

QString EquityDataTab::remembered_key() const {
    const QString variant = remember_variant();
    return QStringLiteral("equity_research/data/") + objectName() + QLatin1Char('/') + symbol_ +
           (variant.isEmpty() ? QString() : QLatin1Char('/') + variant);
}

void EquityDataTab::remember(const fd::DataResult& r) const {
    const fd::DataMeta& m = r.meta;
    const QJsonObject page{{QStringLiteral("present"), m.pagination.present},
                           {QStringLiteral("page"), m.pagination.page},
                           {QStringLiteral("page_size"), m.pagination.page_size},
                           {QStringLiteral("total"), m.pagination.total},
                           {QStringLiteral("total_pages"), m.pagination.total_pages},
                           {QStringLiteral("has_more"), m.pagination.has_more}};
    const QJsonObject meta{{QStringLiteral("dataset"), m.dataset},
                           {QStringLiteral("source"), m.source},
                           {QStringLiteral("source_name"), m.source_name},
                           {QStringLiteral("as_of"), m.as_of.isValid() ? m.as_of.toString(Qt::ISODateWithMs) : QString()},
                           {QStringLiteral("cached"), m.cached},
                           {QStringLiteral("age"), m.age_seconds},
                           {QStringLiteral("max_age"), m.max_age_seconds},
                           {QStringLiteral("stale"), m.stale},
                           {QStringLiteral("stale_reason"), m.stale_reason},
                           {QStringLiteral("delayed"), m.delayed_minutes},
                           {QStringLiteral("source_time"), m.source_time},
                           {QStringLiteral("source_timezone"), m.source_timezone},
                           {QStringLiteral("pagination"), page},
                           {QStringLiteral("received_ms"), static_cast<double>(r.received_ms)}};
    const QByteArray text = QJsonDocument(QJsonObject{{QStringLiteral("data"), r.data}, {QStringLiteral("meta"), meta}})
                                .toJson(QJsonDocument::Compact);
    constexpr int kEdtRememberMaxBytes = 1024 * 1024; // a chain page is tens of KB; never keep a runaway one
    if (text.size() > kEdtRememberMaxBytes)
        return;
    constexpr int kEdtRememberTtlSec = 14 * 24 * 3600;
    CacheManager::instance().put(remembered_key(), QString::fromUtf8(text), kEdtRememberTtlSec,
                                 QStringLiteral("equity_research"));
}

bool EquityDataTab::recall(fd::DataResult* out) const {
    if (symbol_.isEmpty())
        return false;
    const std::optional<QString> text = CacheManager::instance().try_get(remembered_key());
    if (!text || text->isEmpty())
        return false;
    const QJsonObject o = QJsonDocument::fromJson(text->toUtf8()).object();
    if (!o.value(QStringLiteral("data")).isObject())
        return false;
    const QJsonObject m = o.value(QStringLiteral("meta")).toObject();
    const QJsonObject page = m.value(QStringLiteral("pagination")).toObject();
    fd::DataResult r;
    r.data = o.value(QStringLiteral("data")).toObject();
    r.meta.dataset = m.value(QStringLiteral("dataset")).toString();
    r.meta.source = m.value(QStringLiteral("source")).toString();
    r.meta.source_name = m.value(QStringLiteral("source_name")).toString();
    r.meta.as_of = QDateTime::fromString(m.value(QStringLiteral("as_of")).toString(), Qt::ISODateWithMs);
    if (r.meta.as_of.isValid())
        r.meta.as_of = r.meta.as_of.toUTC();
    r.meta.cached = m.value(QStringLiteral("cached")).toBool();
    r.meta.age_seconds = m.value(QStringLiteral("age")).toInt();
    r.meta.max_age_seconds = m.value(QStringLiteral("max_age")).toInt();
    r.meta.stale = m.value(QStringLiteral("stale")).toBool();
    r.meta.stale_reason = m.value(QStringLiteral("stale_reason")).toString();
    r.meta.delayed_minutes = m.value(QStringLiteral("delayed")).toInt();
    r.meta.source_time = m.value(QStringLiteral("source_time")).toString();
    r.meta.source_timezone = m.value(QStringLiteral("source_timezone")).toString();
    r.meta.pagination.present = page.value(QStringLiteral("present")).toBool();
    r.meta.pagination.page = page.value(QStringLiteral("page")).toInt();
    r.meta.pagination.page_size = page.value(QStringLiteral("page_size")).toInt();
    r.meta.pagination.total = page.value(QStringLiteral("total")).toInt();
    r.meta.pagination.total_pages = page.value(QStringLiteral("total_pages")).toInt();
    r.meta.pagination.has_more = page.value(QStringLiteral("has_more")).toBool();
    r.received_ms = static_cast<qint64>(m.value(QStringLiteral("received_ms")).toDouble());
    r.from_client_cache = true; // nothing was asked for it
    *out = r;
    return true;
}

quint64 EquityDataTab::start_request() {
    if (!service_used_) {
        service_used_ = true;
        connect(&fd::FinceptDataService::instance(), &fd::FinceptDataService::availability_changed, this,
                &EquityDataTab::on_availability_changed);
    }
    overlay_->show_loading(loading_text());
    refresh_btn_->setEnabled(false);
    return ++token_;
}

void EquityDataTab::finish(const fd::DataResult& r) {
    overlay_->hide_loading();
    refresh_btn_->setEnabled(true);

    if (r.ok()) {
        if (!replaying_) {
            // A fresh answer: what the tab shows next time instead of a billed auto-load.
            showing_remembered_ = false;
            if (rememberable(r))
                remember(r);
        }
        if (offering_) {
            offering_ = false;
            retranslate_frame(); // LOAD → ⟳ REFRESH
        }
        has_data_ = true;
        unavailable_shown_ = false;
        last_ok_ = r;
        header_->show();
        body_->show();
        refresh_btn_->show();
        set_message(QString(), QString());
        update_freshness();
        if (isVisible())
            age_timer_->start();
        return;
    }

    const fd::DataError& e = r.error;
    using fd::ErrorKind;
    if (e.kind == ErrorKind::ServerLacksApi || e.kind == ErrorKind::DatasetUnavailable) {
        // One calm sentence, nothing else: no controls, no table frame, no refresh button.
        unavailable_shown_ = true;
        has_data_ = false;
        age_timer_->stop();
        clear_view();
        header_->hide();
        body_->hide();
        refresh_btn_->hide();
        fresh_label_->clear();
        set_message(fd::FinceptDataService::error_text(e).toHtmlEscaped(), QString());
        return;
    }

    QString text = e.kind == ErrorKind::NotFound ? not_found_text(e) : fd::FinceptDataService::error_text(e);
    QString html;
    if (e.kind == ErrorKind::NotFound && !e.suggestions.isEmpty()) {
        // The server's "… Did you mean: X?" becomes clickable suggestions.
        const qsizetype at = text.indexOf(QLatin1String("Did you mean"), 0, Qt::CaseInsensitive);
        if (at > 0)
            text = text.left(at).trimmed();
        QStringList links;
        for (const QString& s : e.suggestions)
            links << QString(QStringLiteral("<a href=\"sym:%1\" style=\"color:") + kLinkColor + QStringLiteral(";\">%1</a>"))
                         .arg(s.toHtmlEscaped());
        html = text.toHtmlEscaped() + QStringLiteral(" ") + tr("Did you mean: %1?").arg(links.join(QStringLiteral(", ")));
    } else {
        html = text.toHtmlEscaped();
    }
    if (has_data_)
        html = tr("Could not refresh — %1").arg(html); // the last good data stays on screen

    QString tone = QStringLiteral("error");
    switch (e.kind) {
    case ErrorKind::NotFound:
        tone = QString(); // informational (no options listed, unknown ticker)
        break;
    case ErrorKind::Throttled:
    case ErrorKind::RateLimited:
    case ErrorKind::NoCredits:
    case ErrorKind::NotSignedIn:
    case ErrorKind::NotConfigured:
        tone = QStringLiteral("warn");
        break;
    default:
        break;
    }
    if (!has_data_)
        body_->hide();
    set_message(html, tone);
}

QString EquityDataTab::not_found_text(const fd::DataError& e) const {
    return fd::FinceptDataService::error_text(e);
}

void EquityDataTab::set_message(const QString& html, const QString& tone) {
    message_->setProperty("raw_html", html);
    message_->setText(QString(html).replace(kLinkColor, QString::fromLatin1(ui::colors::AMBER())));
    message_->setVisible(!html.isEmpty());
    if (message_->property("tone").toString() != tone) {
        message_->setProperty("tone", tone);
        repolish(message_);
    }
}

void EquityDataTab::update_freshness() {
    if (!has_data_) {
        fresh_label_->clear();
        fresh_label_->setToolTip(QString());
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QString fresh = fd::FinceptDataService::freshness_text(last_ok_, now);
    if (showing_remembered_)
        fresh += QStringLiteral(" · ") + tr("saved from your last load — REFRESH asks for newer data (billed)");
    fresh_label_->setText(fresh);
    const QString tone = last_ok_.meta.stale ? QStringLiteral("stale") : QString();
    if (fresh_label_->property("tone").toString() != tone) {
        fresh_label_->setProperty("tone", tone);
        repolish(fresh_label_);
    }
    QStringList tip;
    if (last_ok_.meta.as_of.isValid())
        tip << tr("Fetched from the source at %1 (your time).")
                   .arg(last_ok_.meta.as_of.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (last_ok_.meta.max_age_seconds > 0)
        tip << tr("The server re-reads this data every %1.")
                   .arg(fd::FinceptDataService::age_text(last_ok_.meta.max_age_seconds));
    if (last_ok_.from_client_cache)
        tip << tr("Shown from this terminal's copy — no new request was made.");
    if (last_ok_.meta.stale)
        tip << tr("The source is failing, so the server returned its last good copy.");
    fresh_label_->setToolTip(tip.join(QLatin1Char('\n')));
}

void EquityDataTab::on_availability_changed() {
    // The signed-in account changed (the server may have been upgraded meanwhile):
    // a tab showing "needs a newer server" asks again.
    if (!unavailable_shown_)
        return;
    if (fd::FinceptDataService::instance().availability() == fd::FinceptDataService::Availability::Unavailable)
        return;
    if (!isVisible()) {
        symbol_.clear(); // reloads when it is next shown
        return;
    }
    unavailable_shown_ = false;
    set_message(QString(), QString());
    header_->show();
    body_->show();
    refresh_btn_->show();
    clear_view();
    show_remembered_or_offer(); // billed: never a request by itself
}

// ── Events ───────────────────────────────────────────────────────────────────

void EquityDataTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (has_data_) {
        update_freshness();
        age_timer_->start();
    }
}

void EquityDataTab::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    age_timer_->stop();
}

void EquityDataTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslate_frame();
        retranslate();
        update_freshness();
        if (offering_ && message_->isVisible() && !symbol_.isEmpty() && !has_data_)
            show_load_offer(); // re-words the offer line
    }
    QWidget::changeEvent(event);
}

// ── Shared widgets ───────────────────────────────────────────────────────────

QLabel* EquityDataTab::make_caption(const QString& text, QWidget* parent) const {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("fdCaption"));
    return l;
}

QLabel* EquityDataTab::make_section(const QString& text, QWidget* parent) const {
    auto* l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("fdSection"));
    return l;
}

QLabel* EquityDataTab::make_note(QWidget* parent) const {
    auto* l = new QLabel(parent);
    l->setObjectName(QStringLiteral("fdNote"));
    l->setWordWrap(true);
    return l;
}

QPushButton* EquityDataTab::make_button(const QString& text, QWidget* parent) const {
    auto* b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("fdButton"));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QTableWidget* EquityDataTab::make_table(QWidget* parent) {
    auto* t = new QTableWidget(parent);
    t->setAlternatingRowColors(true);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setWordWrap(false);
    t->verticalHeader()->setDefaultSectionSize(data_row_height());
    t->verticalHeader()->hide();
    t->horizontalHeader()->setStretchLastSection(false);
    t->horizontalHeader()->setHighlightSections(false);
    return t;
}

void EquityDataTab::reset_table(QTableWidget* t, const QStringList& headers, int rows) {
    t->setSortingEnabled(false);
    t->clearContents();
    t->setColumnCount(static_cast<int>(headers.size()));
    t->setHorizontalHeaderLabels(headers);
    t->setRowCount(rows);
}

void EquityDataTab::fit_table_height(QTableWidget* t) {
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    const int rows = t->rowCount();
    const int row_h = t->verticalHeader()->defaultSectionSize();
    const int header_h = t->horizontalHeader()->sizeHint().height();
    // Room for a horizontal scroll bar: wide tables scroll sideways at narrow widths.
    const int bar_h = t->horizontalScrollBar()->sizeHint().height();
    t->setFixedHeight(header_h + rows * row_h + 2 * t->frameWidth() + bar_h + 2);
}

QTableWidgetItem* EquityDataTab::text_item(const QString& text, const QColor& fg) {
    auto* item = new QTableWidgetItem(text.isEmpty() ? equity_fmt::dash() : text);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (fg.isValid())
        item->setForeground(fg);
    if (!text.isEmpty())
        item->setToolTip(text);
    return item;
}

QTableWidgetItem* EquityDataTab::number_item(double value, const QString& text, const QColor& fg) {
    // NaN would compare false both ways and scramble sorting: missing values sort first.
    const double key = fd::has(value) ? value : -std::numeric_limits<double>::infinity();
    auto* item = new ui::NumericTableWidgetItem(text, key);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (fg.isValid())
        item->setForeground(fg);
    return item;
}

QTableWidgetItem* EquityDataTab::date_item(const QDate& date) {
    auto* item = number_item(date.isValid() ? static_cast<double>(date.toJulianDay()) : fd::kNaN,
                             date.isValid() ? date.toString(Qt::ISODate) : equity_fmt::dash());
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return item;
}

QString EquityDataTab::num(double v, int decimals) {
    return fd::has(v) ? equity_fmt::grouped(v, decimals) : equity_fmt::dash();
}

QString EquityDataTab::count(double v) {
    return fd::has(v) ? equity_fmt::grouped(v, 0) : equity_fmt::dash();
}

QString EquityDataTab::big(double v) {
    return fd::has(v) ? equity_fmt::large(v) : equity_fmt::dash();
}

QString EquityDataTab::percent(double pct, int decimals, bool signed_) {
    if (!fd::has(pct))
        return equity_fmt::dash();
    const QString body = QString::number(pct, 'f', decimals) + QLatin1Char('%');
    return (signed_ && pct > 0.0) ? QLatin1Char('+') + body : body;
}

QColor EquityDataTab::sign_color(double v) {
    if (!fd::has(v) || v == 0.0)
        return QColor(ui::colors::TEXT_TERTIARY());
    return QColor(v > 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
}

} // namespace fincept::screens
