// src/screens/equity_research/EquityFilingsTab.cpp
#include "screens/equity_research/EquityFilingsTab.h"

#include "core/events/EventBus.h"
#include "screens/equity_research/EquityResearchFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace filings_tab_detail {

namespace fmt = equity_fmt;
using services::equity::SecSection;

constexpr int kEventsSlot = 4; ///< sections_[4]: the 8-K panel, fed by the Filings call
constexpr int kUrlRole = Qt::UserRole + 1;
constexpr int kIndexUrlRole = Qt::UserRole + 2;
constexpr int kTickerRole = Qt::UserRole + 3;

enum FormGroup { kGroupAll, kGroupPeriodic, kGroupCurrent, kGroupProxy, kGroupRegistration, kGroupOwnership, kGroupOther };

int form_group(const QString& form) {
    const QString f = form.trimmed().toUpper();
    if (f.startsWith(QLatin1String("10-K")) || f.startsWith(QLatin1String("10-Q")) || f.startsWith(QLatin1String("20-F")) ||
        f.startsWith(QLatin1String("40-F")) || f.startsWith(QLatin1String("6-K")) || f.startsWith(QLatin1String("NT 10")) ||
        f.startsWith(QLatin1String("11-K")) || f.startsWith(QLatin1String("N-CSR")))
        return kGroupPeriodic;
    if (f.startsWith(QLatin1String("8-K")))
        return kGroupCurrent;
    if (f.contains(QLatin1String("14A")) || f.contains(QLatin1String("14C")) || f.startsWith(QLatin1String("PX14")))
        return kGroupProxy;
    if (f.startsWith(QLatin1String("S-")) || f.startsWith(QLatin1String("F-")) || f.startsWith(QLatin1String("424B")) ||
        f.startsWith(QLatin1String("FWP")) || f.startsWith(QLatin1String("POS AM")) || f.startsWith(QLatin1String("8-A")))
        return kGroupRegistration;
    if (f.startsWith(QLatin1String("SC 13")) || f.startsWith(QLatin1String("SCHEDULE 13")) ||
        f.startsWith(QLatin1String("SC TO")) || f.startsWith(QLatin1String("13F")))
        return kGroupOwnership;
    return kGroupOther;
}

double num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : std::nan("");
}

/// Value in its XBRL unit: "USD" → $416.16B, "USD/shares" → $7.46, "TWD" → NT$2.89T, "shares" → 14.59B sh.
QString unit_value(double v, const QString& unit) {
    if (fmt::missing(v))
        return fmt::dash();
    if (unit.compare(QLatin1String("shares"), Qt::CaseInsensitive) == 0)
        return fmt::large(v) + QStringLiteral(" sh");
    const bool per_share = unit.contains(QLatin1Char('/'));
    const QString code = per_share ? unit.section(QLatin1Char('/'), 0, 0) : unit;
    const QString body = per_share ? fmt::grouped(std::fabs(v), 2) : fmt::large(std::fabs(v));
    return (v < 0 ? QStringLiteral("-") : QString()) + fmt::currency_symbol(code) + body;
}

QString when(qint64 ms) {
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QString();
}

QTableWidgetItem* text_item(const QString& text, const QString& tooltip = {}) {
    auto* it = new QTableWidgetItem(text);
    it->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (!tooltip.isEmpty())
        it->setToolTip(tooltip);
    return it;
}

QTableWidgetItem* num_item(const QString& text, double sort_value) {
    auto* it = new ui::NumericTableWidgetItem(text, std::isfinite(sort_value) ? sort_value : -1e300);
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

QTableWidget* make_table(QWidget* parent) {
    auto* t = new QTableWidget(parent);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setAlternatingRowColors(true);
    t->setShowGrid(false);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(22);
    t->horizontalHeader()->setHighlightSections(false);
    t->setWordWrap(false);
    t->setContextMenuPolicy(Qt::CustomContextMenu);
    return t;
}

void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// Short Form 4 transaction labels for the table; the tooltip carries the full one.
QString txn_short(const QString& code) {
    static const QHash<QString, const char*> kShort = {
        {"P", QT_TRANSLATE_NOOP("EquityFilingsTab", "Buy")},       {"S", QT_TRANSLATE_NOOP("EquityFilingsTab", "Sell")},
        {"A", QT_TRANSLATE_NOOP("EquityFilingsTab", "Grant")},     {"M", QT_TRANSLATE_NOOP("EquityFilingsTab", "Exercise")},
        {"X", QT_TRANSLATE_NOOP("EquityFilingsTab", "Exercise")},  {"F", QT_TRANSLATE_NOOP("EquityFilingsTab", "Tax w/h")},
        {"G", QT_TRANSLATE_NOOP("EquityFilingsTab", "Gift")},      {"D", QT_TRANSLATE_NOOP("EquityFilingsTab", "To issuer")},
        {"C", QT_TRANSLATE_NOOP("EquityFilingsTab", "Conversion")},
    };
    const auto it = kShort.constFind(code);
    return it != kShort.constEnd() ? QCoreApplication::translate("EquityFilingsTab", it.value()) : code;
}

const char* metric_label_key(const QString& key) {
    static const QHash<QString, const char*> kLabels = {
        {"revenue", QT_TRANSLATE_NOOP("EquityFilingsTab", "Revenue")},
        {"operating_income", QT_TRANSLATE_NOOP("EquityFilingsTab", "Operating income")},
        {"net_income", QT_TRANSLATE_NOOP("EquityFilingsTab", "Net income")},
        {"eps_diluted", QT_TRANSLATE_NOOP("EquityFilingsTab", "EPS (diluted)")},
        {"operating_cash_flow", QT_TRANSLATE_NOOP("EquityFilingsTab", "Operating cash flow")},
        {"capex", QT_TRANSLATE_NOOP("EquityFilingsTab", "Capital expenditure")},
        {"free_cash_flow", QT_TRANSLATE_NOOP("EquityFilingsTab", "Free cash flow (OCF − capex)")},
        {"total_assets", QT_TRANSLATE_NOOP("EquityFilingsTab", "Total assets")},
        {"total_liabilities", QT_TRANSLATE_NOOP("EquityFilingsTab", "Total liabilities")},
        {"equity", QT_TRANSLATE_NOOP("EquityFilingsTab", "Shareholders' equity")},
        {"cash", QT_TRANSLATE_NOOP("EquityFilingsTab", "Cash & equivalents")},
        {"long_term_debt", QT_TRANSLATE_NOOP("EquityFilingsTab", "Long-term debt (non-current)")},
    };
    return kLabels.value(key, nullptr);
}

} // namespace filings_tab_detail

using namespace filings_tab_detail;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

EquityFilingsTab::EquityFilingsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::SecFilingsService::instance();
    connect(&svc, &services::equity::SecFilingsService::section_loaded, this, &EquityFilingsTab::on_section_loaded);
    connect(&svc, &services::equity::SecFilingsService::section_failed, this, &EquityFilingsTab::on_section_failed);
}

void EquityFilingsTab::build_ui() {
    setObjectName(QStringLiteral("equityFilingsTab"));
    // One stylesheet for the tab; every widget below is styled by objectName / "tone".
    setStyleSheet(
        QStringLiteral(
            "#equityFilingsTab, QWidget#filingsContent { background:%1; }"
            "QFrame#filingsHeader { background:%2; border:0; border-bottom:2px solid %3; }"
            "QLabel { background:transparent; border:0; }"
            "QLabel#filingsTitle { color:%3; font-size:11px; font-weight:700; letter-spacing:2px; }"
            "QLabel#filingsCompany { color:%4; font-size:11px; }"
            "QLabel#filingsLatest { color:%5; font-size:10px; }"
            "QPushButton#filingsBtn { background:transparent; color:%5; border:1px solid %6; padding:3px 10px;"
            " font-size:10px; font-weight:700; }"
            "QPushButton#filingsBtn:hover { color:%3; border-color:%3; }"
            "QFrame#filingsPanel { background:%2; border:1px solid %6; }"
            "QLabel#filingsPanelTitle { color:%3; font-size:11px; font-weight:700; letter-spacing:1px; }"
            "QLabel#filingsMeta { color:%7; font-size:10px; }"
            "QLabel#filingsMeta[tone=\"stale\"] { color:%8; }"
            "QLabel#filingsStatus { color:%5; font-size:11px; padding:8px 0; }"
            "QLabel#filingsStatus[tone=\"error\"] { color:%9; }"
            "QLabel#filingsSummary { color:%4; font-size:11px; }"
            "QLabel#filingsBuy { color:%10; font-size:11px; font-weight:600; }"
            "QLabel#filingsSell { color:%9; font-size:11px; font-weight:600; }"
            "QLabel#filingsNote { color:%7; font-size:10px; }"
            "QLabel#filingsNotice { color:%5; font-size:12px; padding:32px; }"
            "QComboBox#filingsFilter { background:%11; color:%4; border:1px solid %6; padding:2px 6px; font-size:11px; }"
            "QTableWidget { background:%2; alternate-background-color:%1; color:%4; border:0; font-size:11px; }"
            "QTableWidget::item { padding:0 6px; }"
            "QTableWidget::item:selected { background:%11; color:%4; }"
            "QHeaderView::section { background:%11; color:%5; font-size:10px; font-weight:700; padding:4px 6px; border:0;"
            " border-bottom:1px solid %6; }"
            "QSplitter::handle { background:%6; }"
            "QScrollArea#filingsScroll { background:transparent; border:0; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::AMBER(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::WARNING(), ui::colors::NEGATIVE())
            .arg(ui::colors::POSITIVE(), ui::colors::BG_RAISED()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(build_header());

    notice_lbl_ = new QLabel(tr("Open a US-listed symbol to see its SEC filings."), this);
    notice_lbl_->setObjectName(QStringLiteral("filingsNotice"));
    notice_lbl_->setAlignment(Qt::AlignCenter);
    notice_lbl_->setWordWrap(true);
    root->addWidget(notice_lbl_, 1);

    body_splitter_ = new QSplitter(Qt::Horizontal, this);
    body_splitter_->setChildrenCollapsible(false);
    body_splitter_->setHandleWidth(1);

    // Left: recent filings list.
    auto* left = new QWidget(body_splitter_);
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(8, 8, 4, 8);
    const int filings_slot = static_cast<int>(SecSection::Filings);
    make_section(filings_slot, SecSection::Filings, QT_TR_NOOP("RECENT FILINGS"), build_filings_body());
    lvl->addWidget(sections_[filings_slot].frame);

    // Right: stacked panels.
    auto* scroll = new QScrollArea(body_splitter_);
    scroll->setObjectName(QStringLiteral("filingsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* right = new QWidget;
    right->setObjectName(QStringLiteral("filingsContent"));
    auto* rvl = new QVBoxLayout(right);
    rvl->setContentsMargins(4, 8, 8, 8);
    rvl->setSpacing(8);
    make_section(static_cast<int>(SecSection::Financials), SecSection::Financials,
                 QT_TR_NOOP("KEY FINANCIALS — 10-K / 10-Q (XBRL)"), build_financials_body());
    make_section(kEventsSlot, SecSection::Filings, QT_TR_NOOP("8-K CURRENT EVENTS"), build_events_body());
    make_section(static_cast<int>(SecSection::Insiders), SecSection::Insiders, QT_TR_NOOP("INSIDER ACTIVITY — FORM 4"),
                 build_insiders_body());
    make_section(static_cast<int>(SecSection::Holders), SecSection::Holders, QT_TR_NOOP("INSTITUTIONAL HOLDERS — 13F"),
                 build_holders_body());
    for (int slot : {static_cast<int>(SecSection::Financials), kEventsSlot, static_cast<int>(SecSection::Insiders),
                     static_cast<int>(SecSection::Holders)})
        rvl->addWidget(sections_[slot].frame);
    rvl->addStretch();
    scroll->setWidget(right);

    body_splitter_->addWidget(left);
    body_splitter_->addWidget(scroll);
    body_splitter_->setStretchFactor(0, 5);
    body_splitter_->setStretchFactor(1, 6);
    root->addWidget(body_splitter_, 1);
    body_splitter_->setVisible(false);
    set_table_headers();
}

QWidget* EquityFilingsTab::build_header() {
    auto* hdr = new QFrame(this);
    hdr->setObjectName(QStringLiteral("filingsHeader"));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(14, 7, 14, 7);
    hl->setSpacing(12);

    title_lbl_ = new QLabel(tr("SEC FILINGS"), hdr);
    title_lbl_->setObjectName(QStringLiteral("filingsTitle"));
    hl->addWidget(title_lbl_);

    company_lbl_ = new QLabel(hdr);
    company_lbl_->setObjectName(QStringLiteral("filingsCompany"));
    company_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hl->addWidget(company_lbl_, 1);

    latest_lbl_ = new QLabel(hdr);
    latest_lbl_->setObjectName(QStringLiteral("filingsLatest"));
    latest_lbl_->setTextFormat(Qt::RichText);
    connect(latest_lbl_, &QLabel::linkActivated, this, &EquityFilingsTab::open_url);
    hl->addWidget(latest_lbl_);

    edgar_btn_ = new QPushButton(tr("EDGAR ↗"), hdr);
    edgar_btn_->setObjectName(QStringLiteral("filingsBtn"));
    edgar_btn_->setCursor(Qt::PointingHandCursor);
    edgar_btn_->setToolTip(tr("Open this company's filing index on sec.gov"));
    edgar_btn_->setEnabled(false);
    connect(edgar_btn_, &QPushButton::clicked, this, [this]() { open_url(edgar_url_); });
    hl->addWidget(edgar_btn_);

    refresh_btn_ = new QPushButton(tr("REFRESH"), hdr);
    refresh_btn_->setObjectName(QStringLiteral("filingsBtn"));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setToolTip(tr("Re-fetch every panel from SEC EDGAR, bypassing the cache"));
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() { request_all(/*force=*/true); });
    hl->addWidget(refresh_btn_);
    return hdr;
}

void EquityFilingsTab::make_section(int slot, SecSection target, const char* title_key, QWidget* body) {
    Section& sec = sections_[slot];
    sec.title_key = title_key;
    sec.frame = new QFrame(this);
    sec.frame->setObjectName(QStringLiteral("filingsPanel"));
    auto* vl = new QVBoxLayout(sec.frame);
    vl->setContentsMargins(10, 8, 10, 10);
    vl->setSpacing(6);

    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    sec.title = new QLabel(tr(title_key), sec.frame);
    sec.title->setObjectName(QStringLiteral("filingsPanelTitle"));
    head->addWidget(sec.title);
    head->addStretch();
    sec.meta = new QLabel(sec.frame);
    sec.meta->setObjectName(QStringLiteral("filingsMeta"));
    head->addWidget(sec.meta);
    vl->addLayout(head);

    auto* status_row = new QHBoxLayout;
    sec.status = new QLabel(sec.frame);
    sec.status->setObjectName(QStringLiteral("filingsStatus"));
    sec.status->setWordWrap(true);
    status_row->addWidget(sec.status, 1);
    sec.retry = new QPushButton(tr("RETRY"), sec.frame);
    sec.retry->setObjectName(QStringLiteral("filingsBtn"));
    sec.retry->setCursor(Qt::PointingHandCursor);
    sec.retry->setVisible(false);
    status_row->addWidget(sec.retry, 0, Qt::AlignTop);
    vl->addLayout(status_row);

    sec.body = body;
    body->setParent(sec.frame);
    body->setVisible(false);
    vl->addWidget(body, 1);

    // The 8-K panel's RETRY re-runs the filings call that feeds it.
    connect(sec.retry, &QPushButton::clicked, this, [this, target]() { request(target, /*force=*/true); });
}

QWidget* EquityFilingsTab::build_filings_body() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    auto* row = new QHBoxLayout;
    filter_combo_ = new QComboBox(w);
    filter_combo_->setObjectName(QStringLiteral("filingsFilter"));
    filter_combo_->addItems({tr("All forms (excl. insider Forms 3/4/5/144)"), tr("Annual & quarterly reports"),
                             tr("Current reports (8-K)"), tr("Proxy statements"), tr("Registrations & prospectuses"),
                             tr("Ownership (13D/13G/13F)"), tr("Other")});
    connect(filter_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { apply_filings_filter(); });
    row->addWidget(filter_combo_);
    row->addStretch();
    filings_count_lbl_ = new QLabel(w);
    filings_count_lbl_->setObjectName(QStringLiteral("filingsNote"));
    row->addWidget(filings_count_lbl_);
    vl->addLayout(row);

    filings_table_ = make_table(w);
    filings_table_->setColumnCount(4);
    filings_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    filings_table_->setSortingEnabled(true);
    filings_table_->setToolTip(tr("Double-click to open the filing on sec.gov · right-click for more"));
    connect(filings_table_, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        if (auto* it = filings_table_->item(r, 0))
            open_url(it->data(kUrlRole).toString());
    });
    connect(filings_table_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { show_link_menu(filings_table_, pos); });
    vl->addWidget(filings_table_, 1);
    return w;
}

QWidget* EquityFilingsTab::build_financials_body() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    fin_table_ = make_table(w);
    fin_table_->setColumnCount(5);
    fin_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    fin_table_->setMinimumHeight(13 * 22 + 30);
    fin_table_->setContextMenuPolicy(Qt::NoContextMenu);
    vl->addWidget(fin_table_);
    fin_note_ = new QLabel(w);
    fin_note_->setObjectName(QStringLiteral("filingsNote"));
    fin_note_->setWordWrap(true);
    fin_note_->setTextFormat(Qt::RichText);
    connect(fin_note_, &QLabel::linkActivated, this, &EquityFilingsTab::open_url);
    vl->addWidget(fin_note_);
    return w;
}

QWidget* EquityFilingsTab::build_events_body() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    events_table_ = make_table(w);
    events_table_->setColumnCount(2);
    events_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    events_table_->setMinimumHeight(8 * 22 + 30);
    events_table_->setToolTip(tr("Double-click to open the 8-K on sec.gov"));
    connect(events_table_, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        if (auto* it = events_table_->item(r, 0))
            open_url(it->data(kUrlRole).toString());
    });
    connect(events_table_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { show_link_menu(events_table_, pos); });
    vl->addWidget(events_table_);
    return w;
}

QWidget* EquityFilingsTab::build_insiders_body() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    ins_buys_lbl_ = new QLabel(w);
    ins_buys_lbl_->setObjectName(QStringLiteral("filingsBuy"));
    ins_sells_lbl_ = new QLabel(w);
    ins_sells_lbl_->setObjectName(QStringLiteral("filingsSell"));
    ins_note_ = new QLabel(w);
    ins_note_->setObjectName(QStringLiteral("filingsNote"));
    ins_note_->setWordWrap(true);
    vl->addWidget(ins_buys_lbl_);
    vl->addWidget(ins_sells_lbl_);
    vl->addWidget(ins_note_);
    ins_table_ = make_table(w);
    ins_table_->setColumnCount(8);
    ins_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ins_table_->setSortingEnabled(true);
    ins_table_->setMinimumHeight(10 * 22 + 30);
    ins_table_->setToolTip(tr("Double-click to open the Form 4 on sec.gov"));
    connect(ins_table_, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        if (auto* it = ins_table_->item(r, 0))
            open_url(it->data(kUrlRole).toString());
    });
    connect(ins_table_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { show_link_menu(ins_table_, pos); });
    vl->addWidget(ins_table_);
    return w;
}

QWidget* EquityFilingsTab::build_holders_body() {
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);
    hold_summary_lbl_ = new QLabel(w);
    hold_summary_lbl_->setObjectName(QStringLiteral("filingsSummary"));
    hold_summary_lbl_->setWordWrap(true);
    vl->addWidget(hold_summary_lbl_);
    hold_table_ = make_table(w);
    hold_table_->setColumnCount(6);
    hold_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    hold_table_->setSortingEnabled(true);
    hold_table_->setMinimumHeight(10 * 22 + 30);
    hold_table_->setContextMenuPolicy(Qt::NoContextMenu);
    vl->addWidget(hold_table_);

    own13f_lbl_ = new QLabel(w);
    own13f_lbl_->setObjectName(QStringLiteral("filingsSummary"));
    own13f_lbl_->setWordWrap(true);
    own13f_lbl_->setVisible(false);
    vl->addWidget(own13f_lbl_);
    own13f_table_ = make_table(w);
    own13f_table_->setColumnCount(4);
    own13f_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    own13f_table_->setSortingEnabled(true);
    own13f_table_->setMinimumHeight(8 * 22 + 30);
    own13f_table_->setVisible(false);
    own13f_table_->setToolTip(tr("Double-click a position to research it"));
    connect(own13f_table_, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        const QString ticker = own13f_table_->item(r, 0) ? own13f_table_->item(r, 0)->data(kTickerRole).toString()
                                                         : QString();
        if (!ticker.isEmpty())
            EventBus::instance().publish(QStringLiteral("nav.open_symbol"),
                                         {{"screen_id", QStringLiteral("equity_research")}, {"symbol", ticker}});
    });
    vl->addWidget(own13f_table_);

    hold_note_ = new QLabel(w);
    hold_note_->setObjectName(QStringLiteral("filingsNote"));
    hold_note_->setWordWrap(true);
    vl->addWidget(hold_note_);
    return w;
}

EquityFilingsTab::Section& EquityFilingsTab::section(SecSection which) {
    return sections_[static_cast<int>(which)];
}

// ─────────────────────────────────────────────────────────────────────────────
// Symbol / requests
// ─────────────────────────────────────────────────────────────────────────────

void EquityFilingsTab::set_symbol(const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty() || sym == current_symbol_)
        return;
    current_symbol_ = sym;
    filings_data_ = financials_data_ = insiders_data_ = holders_data_ = QJsonObject();
    edgar_url_.clear();
    edgar_btn_->setEnabled(false);
    latest_lbl_->clear();
    for (auto& sec : sections_) {
        sec.loaded = sec.loading = sec.stale = false;
        sec.fetched_ms = 0;
    }

    QString reason;
    sec_eligible_ = services::equity::SecFilingsService::is_us_listing(sym, &reason);
    company_lbl_->setText(sym);
    refresh_btn_->setEnabled(sec_eligible_);
    if (!sec_eligible_) {
        body_splitter_->setVisible(false);
        notice_lbl_->setText(reason);
        notice_lbl_->setVisible(true);
        return;
    }
    notice_lbl_->setVisible(false);
    body_splitter_->setVisible(true);
    request_all(false);
}

void EquityFilingsTab::request_all(bool force) {
    if (current_symbol_.isEmpty() || !sec_eligible_)
        return;
    for (SecSection s : {SecSection::Filings, SecSection::Financials, SecSection::Insiders, SecSection::Holders})
        request(s, force);
}

void EquityFilingsTab::request(SecSection which, bool force) {
    Section& sec = section(which);
    sec.loading = true;
    if (which == SecSection::Filings)
        sections_[kEventsSlot].loading = true;
    if (!sec.loaded) {
        show_loading(sec);
        if (which == SecSection::Filings)
            show_loading(sections_[kEventsSlot]);
    } else {
        update_meta(sec);
        if (which == SecSection::Filings)
            update_meta(sections_[kEventsSlot]);
    }
    services::equity::SecFilingsService::instance().fetch(current_symbol_, which, force);
}

void EquityFilingsTab::on_section_loaded(const services::equity::SecSectionData& d) {
    if (d.symbol.compare(current_symbol_, Qt::CaseInsensitive) != 0)
        return; // a late answer for the symbol the user already left
    Section& sec = section(d.section);
    sec.loaded = true;
    sec.stale = d.stale;
    sec.loading = d.stale; // a stale replay means the refresh is still running
    sec.fetched_ms = d.fetched_at_ms;
    switch (d.section) {
        case SecSection::Filings: {
            Section& ev = sections_[kEventsSlot];
            ev.loaded = true;
            ev.stale = sec.stale;
            ev.loading = sec.loading;
            ev.fetched_ms = sec.fetched_ms;
            render_filings(d.data);
            show_body(ev);
            if (events_table_->rowCount() == 0) {
                ev.body->setVisible(false);
                ev.status->setText(tr("No 8-K filings in the recent filing history."));
                ev.status->setVisible(true);
            }
            break;
        }
        case SecSection::Financials:
            render_financials(d.data);
            break;
        case SecSection::Insiders:
            render_insiders(d.data);
            break;
        case SecSection::Holders:
            render_holders(d.data);
            break;
    }
    show_body(sec);
}

void EquityFilingsTab::on_section_failed(const QString& symbol, SecSection which, const QString& error) {
    if (symbol.compare(current_symbol_, Qt::CaseInsensitive) != 0)
        return;
    QVector<Section*> targets{&section(which)};
    if (which == SecSection::Filings)
        targets.append(&sections_[kEventsSlot]);
    for (Section* sec : targets) {
        sec->loading = false;
        if (sec->loaded) {
            // Keep the copy on screen and say the refresh failed.
            update_meta(*sec);
            sec->meta->setText(tr("refresh failed — showing copy from %1").arg(when(sec->fetched_ms)));
            sec->meta->setToolTip(error);
            sec->meta->setProperty("tone", QStringLiteral("stale"));
            repolish(sec->meta);
        } else {
            show_error(*sec, error);
        }
    }
}

void EquityFilingsTab::show_loading(Section& sec) {
    sec.body->setVisible(false);
    sec.retry->setVisible(false);
    sec.status->setProperty("tone", QString());
    repolish(sec.status);
    sec.status->setText(tr("Loading from SEC EDGAR…"));
    sec.status->setVisible(true);
    sec.meta->clear();
}

void EquityFilingsTab::show_error(Section& sec, const QString& message) {
    sec.body->setVisible(false);
    sec.status->setProperty("tone", QStringLiteral("error"));
    repolish(sec.status);
    sec.status->setText(message.isEmpty() ? tr("Could not load this panel.") : message);
    sec.status->setVisible(true);
    sec.retry->setVisible(true);
    sec.meta->clear();
}

void EquityFilingsTab::show_body(Section& sec) {
    sec.status->setVisible(false);
    sec.retry->setVisible(false);
    sec.body->setVisible(true);
    update_meta(sec);
}

void EquityFilingsTab::update_meta(Section& sec) {
    sec.meta->setToolTip(QString());
    if (sec.stale || (sec.loading && sec.loaded)) {
        sec.meta->setText(sec.stale ? tr("cached copy from %1 — refreshing…").arg(when(sec.fetched_ms))
                                    : tr("refreshing…"));
        sec.meta->setProperty("tone", QStringLiteral("stale"));
    } else {
        sec.meta->setText(sec.fetched_ms > 0 ? tr("as of %1").arg(when(sec.fetched_ms)) : QString());
        sec.meta->setProperty("tone", QString());
    }
    repolish(sec.meta);
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering
// ─────────────────────────────────────────────────────────────────────────────

void EquityFilingsTab::set_table_headers() {
    filings_table_->setHorizontalHeaderLabels({tr("FILED"), tr("FORM"), tr("PERIOD"), tr("DESCRIPTION / 8-K ITEMS")});
    events_table_->setHorizontalHeaderLabels({tr("FILED"), tr("EVENT (8-K ITEMS)")});
    ins_table_->setHorizontalHeaderLabels({tr("TRADE DATE"), tr("INSIDER"), tr("ROLE"), tr("TXN"), tr("SHARES"),
                                           tr("PRICE"), tr("VALUE"), tr("OWNED AFTER")});
    hold_table_->setHorizontalHeaderLabels(
        {tr("HOLDER"), tr("SHARES"), tr("% OUT"), tr("VALUE"), tr("Δ SHARES QoQ"), tr("REPORTED")});
    own13f_table_->setHorizontalHeaderLabels({tr("POSITION"), tr("TICKER"), tr("SHARES"), tr("VALUE (USD)")});
}

void EquityFilingsTab::render_filings(const QJsonObject& d) {
    filings_data_ = d;
    // Header: issuer identity + quick links to the latest periodic reports.
    QStringList parts;
    const QString name = d.value(QStringLiteral("name")).toString();
    if (!name.isEmpty())
        parts << name;
    const qint64 cik = static_cast<qint64>(d.value(QStringLiteral("cik")).toDouble());
    if (cik > 0)
        parts << tr("CIK %1").arg(cik, 10, 10, QLatin1Char('0'));
    const QString sic = d.value(QStringLiteral("sic")).toString();
    const QString industry = d.value(QStringLiteral("industry")).toString();
    if (!sic.isEmpty())
        parts << (industry.isEmpty() ? tr("SIC %1").arg(sic) : tr("SIC %1 %2").arg(sic, industry));
    const QString fye = d.value(QStringLiteral("fiscal_year_end")).toString();
    if (fye.size() == 4)
        parts << tr("FY ends %1-%2").arg(fye.left(2), fye.mid(2));
    QStringList exchanges;
    for (const auto& e : d.value(QStringLiteral("exchanges")).toArray())
        exchanges << e.toString();
    if (!exchanges.isEmpty())
        parts << exchanges.join(QLatin1Char('/'));
    company_lbl_->setText(parts.isEmpty() ? current_symbol_
                                          : current_symbol_ + QStringLiteral("  ·  ") + parts.join(QStringLiteral(" · ")));
    edgar_url_ = d.value(QStringLiteral("edgar_url")).toString();
    edgar_btn_->setEnabled(!edgar_url_.isEmpty());

    const QJsonObject latest = d.value(QStringLiteral("latest")).toObject();
    QStringList links;
    for (const char* form : {"10-K", "20-F", "40-F", "10-Q", "DEF 14A"}) {
        const QJsonObject f = latest.value(QLatin1String(form)).toObject();
        const QString url = f.value(QStringLiteral("url")).toString();
        if (url.isEmpty())
            continue;
        links << QStringLiteral("<a href=\"%1\" style=\"color:%2; text-decoration:none;\">%3 %4</a>")
                     .arg(url.toHtmlEscaped(), QString(ui::colors::AMBER()), QString::fromLatin1(form).toHtmlEscaped(),
                          f.value(QStringLiteral("filed")).toString().toHtmlEscaped());
    }
    latest_lbl_->setText(links.isEmpty() ? QString() : tr("LATEST: %1").arg(links.join(QStringLiteral("  "))));
    latest_lbl_->setToolTip(tr("Open the latest annual/quarterly report or proxy on sec.gov"));

    apply_filings_filter();

    // 8-K events.
    const QJsonArray events = d.value(QStringLiteral("events")).toArray();
    events_table_->setSortingEnabled(false);
    events_table_->setRowCount(static_cast<int>(events.size()));
    for (int r = 0; r < events.size(); ++r) {
        const QJsonObject e = events.at(r).toObject();
        QStringList labels, codes;
        for (const auto& it : e.value(QStringLiteral("item_labels")).toArray()) {
            const QJsonObject o = it.toObject();
            codes << o.value(QStringLiteral("code")).toString();
            labels << o.value(QStringLiteral("label")).toString();
        }
        auto* date_item = text_item(e.value(QStringLiteral("filed")).toString());
        date_item->setData(kUrlRole, e.value(QStringLiteral("url")).toString());
        date_item->setData(kIndexUrlRole, e.value(QStringLiteral("index_url")).toString());
        events_table_->setItem(r, 0, date_item);
        const QString form = e.value(QStringLiteral("form")).toString();
        const QString text = (form.endsWith(QLatin1String("/A")) ? tr("[Amended] ") : QString()) +
                             (labels.isEmpty() ? tr("(no items listed)") : labels.join(QStringLiteral("; ")));
        events_table_->setItem(r, 1, text_item(text, tr("Items %1").arg(codes.join(QStringLiteral(", ")))));
    }
    events_table_->resizeColumnToContents(0);
}

void EquityFilingsTab::apply_filings_filter() {
    const QJsonArray rows = filings_data_.value(QStringLiteral("filings")).toArray();
    const int group = filter_combo_ ? filter_combo_->currentIndex() : kGroupAll;
    filings_table_->setSortingEnabled(false);
    filings_table_->setRowCount(0);
    int shown = 0;
    for (const auto& v : rows) {
        const QJsonObject f = v.toObject();
        const QString form = f.value(QStringLiteral("form")).toString();
        if (group != kGroupAll && form_group(form) != group)
            continue;
        filings_table_->insertRow(shown);
        auto* filed = text_item(f.value(QStringLiteral("filed")).toString());
        filed->setData(kUrlRole, f.value(QStringLiteral("url")).toString());
        filed->setData(kIndexUrlRole, f.value(QStringLiteral("index_url")).toString());
        filings_table_->setItem(shown, 0, filed);
        auto* form_item = text_item(form);
        form_item->setForeground(QColor(ui::colors::AMBER()));
        filings_table_->setItem(shown, 1, form_item);
        filings_table_->setItem(shown, 2, text_item(f.value(QStringLiteral("period")).toString()));
        QString desc = f.value(QStringLiteral("description")).toString();
        if (desc.compare(form, Qt::CaseInsensitive) == 0)
            desc.clear(); // the submissions feed often repeats the form type as its "description"
        const QString items = f.value(QStringLiteral("items")).toString();
        if (!items.isEmpty())
            desc = desc.isEmpty() ? tr("Items %1").arg(items) : tr("%1 · items %2").arg(desc, items);
        filings_table_->setItem(shown, 3, text_item(desc, f.value(QStringLiteral("accession")).toString()));
        ++shown;
    }
    filings_table_->setSortingEnabled(true);
    filings_table_->resizeColumnsToContents();
    filings_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    const int insider = filings_data_.value(QStringLiteral("insider_filings")).toInt();
    filings_count_lbl_->setText(tr("%1 shown · %2 insider forms in the Form 4 panel").arg(shown).arg(insider));
}

void EquityFilingsTab::render_financials(const QJsonObject& d) {
    financials_data_ = d;
    const QJsonObject annual = d.value(QStringLiteral("annual")).toObject();
    const QJsonObject quarterly = d.value(QStringLiteral("quarterly")).toObject();

    QStringList keys;
    QHash<QString, QJsonObject> a_map, q_map;
    QHash<QString, QString> script_labels;
    auto collect = [&](const QJsonObject& group, QHash<QString, QJsonObject>& map) {
        for (const auto& v : group.value(QStringLiteral("metrics")).toArray()) {
            const QJsonObject m = v.toObject();
            const QString key = m.value(QStringLiteral("key")).toString();
            map.insert(key, m);
            if (!script_labels.contains(key)) {
                script_labels.insert(key, m.value(QStringLiteral("label")).toString());
                keys << key;
            }
        }
    };
    collect(annual, a_map);
    collect(quarterly, q_map);

    auto group_header = [](const QJsonObject& g, const QString& fallback) {
        if (g.isEmpty())
            return fallback;
        const QString fp = g.value(QStringLiteral("fp")).toString();
        const QString fy = QString::number(g.value(QStringLiteral("fy")).toInt());
        const QString form = g.value(QStringLiteral("form")).toString();
        return fp == QLatin1String("FY") ? tr("%1 FY%2").arg(form, fy) : tr("%1 %2 FY%3").arg(form, fp, fy);
    };
    fin_table_->setHorizontalHeaderLabels({tr("METRIC"), group_header(annual, tr("ANNUAL")), tr("YoY"),
                                           group_header(quarterly, tr("QUARTER")), tr("YoY")});
    if (auto* h = fin_table_->horizontalHeaderItem(1))
        h->setToolTip(annual.isEmpty() ? QString()
                                       : tr("12 months to %1 · filed %2")
                                             .arg(annual.value(QStringLiteral("period_end")).toString(),
                                                  annual.value(QStringLiteral("filed")).toString()));
    if (auto* h = fin_table_->horizontalHeaderItem(3))
        h->setToolTip(quarterly.isEmpty() ? QString()
                                          : tr("Quarter to %1 · filed %2")
                                                .arg(quarterly.value(QStringLiteral("period_end")).toString(),
                                                     quarterly.value(QStringLiteral("filed")).toString()));

    auto yoy_item = [](const QJsonObject& m) -> QTableWidgetItem* {
        const double yoy = num(m.value(QStringLiteral("yoy")));
        const double value = num(m.value(QStringLiteral("value")));
        const double prior = num(m.value(QStringLiteral("prior")));
        QString text = fmt::dash();
        if (!fmt::missing(yoy))
            text = fmt::pct(yoy, 1, true);
        else if (!fmt::missing(value) && !fmt::missing(prior))
            text = tr("n/m"); // growth off a zero or negative base is not meaningful
        auto* it = num_item(text, yoy);
        if (!fmt::missing(yoy))
            it->setForeground(QColor(yoy >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        if (!fmt::missing(prior))
            it->setToolTip(tr("Prior period: %1").arg(unit_value(prior, m.value(QStringLiteral("unit")).toString())));
        return it;
    };
    auto value_item = [](const QJsonObject& m, bool quarter_col) -> QTableWidgetItem* {
        const double v = num(m.value(QStringLiteral("value")));
        QString text = unit_value(v, m.value(QStringLiteral("unit")).toString());
        const int months = m.value(QStringLiteral("months")).toInt();
        if (quarter_col && months > 3 && !fmt::missing(v))
            text += tr(" (%1M YTD)").arg(months); // 10-Q cash-flow statements are year-to-date
        auto* it = num_item(text, v);
        const QString xbrl_concept = m.value(QStringLiteral("concept")).toString();
        if (!xbrl_concept.isEmpty())
            it->setToolTip(months > 0 ? tr("%1 · %2 months").arg(xbrl_concept).arg(months)
                                      : tr("%1 · balance at period end").arg(xbrl_concept));
        return it;
    };

    fin_table_->setRowCount(static_cast<int>(keys.size()));
    for (int r = 0; r < keys.size(); ++r) {
        const QString& key = keys.at(r);
        const char* tr_key = metric_label_key(key);
        fin_table_->setItem(r, 0, text_item(tr_key ? QCoreApplication::translate("EquityFilingsTab", tr_key)
                                                   : script_labels.value(key)));
        const QJsonObject am = a_map.value(key);
        const QJsonObject qm = q_map.value(key);
        fin_table_->setItem(r, 1, am.isEmpty() ? num_item(fmt::dash(), std::nan("")) : value_item(am, false));
        fin_table_->setItem(r, 2, am.isEmpty() ? num_item(fmt::dash(), std::nan("")) : yoy_item(am));
        fin_table_->setItem(r, 3, qm.isEmpty() ? num_item(fmt::dash(), std::nan("")) : value_item(qm, true));
        fin_table_->setItem(r, 4, qm.isEmpty() ? num_item(fmt::dash(), std::nan("")) : yoy_item(qm));
    }
    fin_table_->resizeColumnsToContents();
    fin_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    QStringList notes;
    notes << tr("Source: SEC XBRL company facts (%1), values as filed. YoY compares the same period a year earlier; "
                "\"n/m\" = prior period zero or negative.")
                 .arg(d.value(QStringLiteral("taxonomy")).toString());
    const QJsonObject shares = d.value(QStringLiteral("shares_outstanding")).toObject();
    if (!fmt::missing(num(shares.value(QStringLiteral("value")))))
        notes << tr("Shares outstanding %1 as of %2 (cover page).")
                     .arg(fmt::large(num(shares.value(QStringLiteral("value")))),
                          shares.value(QStringLiteral("as_of")).toString());
    if (quarterly.isEmpty())
        notes << tr("No 10-Q on file — foreign private issuers report interim results on Form 6-K without XBRL.");
    QStringList links;
    for (const QJsonObject& g : {annual, quarterly}) {
        const QString url = g.value(QStringLiteral("index_url")).toString();
        if (!url.isEmpty())
            links << QStringLiteral("<a href=\"%1\" style=\"color:%2;\">%3</a>")
                         .arg(url.toHtmlEscaped(), QString(ui::colors::AMBER()),
                              tr("open %1").arg(g.value(QStringLiteral("form")).toString()).toHtmlEscaped());
    }
    fin_note_->setText(notes.join(QLatin1Char(' ')).toHtmlEscaped() +
                       (links.isEmpty() ? QString() : QStringLiteral("  ") + links.join(QStringLiteral(" · "))));
}

void EquityFilingsTab::render_insiders(const QJsonObject& d) {
    insiders_data_ = d;
    const QJsonObject s = d.value(QStringLiteral("summary")).toObject();
    const QJsonObject buys = s.value(QStringLiteral("buys")).toObject();
    const QJsonObject sells = s.value(QStringLiteral("sells")).toObject();
    auto side = [](const QJsonObject& o) {
        const int n = o.value(QStringLiteral("count")).toInt();
        return n == 0 ? tr("none")
                      : tr("%n trade(s) · %1 shares · %2", "", n)
                            .arg(fmt::large(num(o.value(QStringLiteral("shares")))),
                                 QStringLiteral("$") + fmt::large(num(o.value(QStringLiteral("value")))));
    };
    ins_buys_lbl_->setText(tr("OPEN-MARKET BUYS (P): %1").arg(side(buys)));
    ins_sells_lbl_->setText(tr("OPEN-MARKET SELLS (S): %1").arg(side(sells)));

    const QJsonObject win = d.value(QStringLiteral("window")).toObject();
    QStringList notes;
    notes << tr("Last %1 Form 4 filings (%2 → %3), %4 insider(s).")
                 .arg(win.value(QStringLiteral("filings_parsed")).toInt())
                 .arg(win.value(QStringLiteral("from")).toString(), win.value(QStringLiteral("to")).toString())
                 .arg(s.value(QStringLiteral("insiders")).toInt());
    const int plan_sells = s.value(QStringLiteral("sells_under_10b5_1")).toInt();
    if (sells.value(QStringLiteral("count")).toInt() > 0)
        notes << tr("%1 of %2 sales under Rule 10b5-1 plans (pre-scheduled).")
                     .arg(plan_sells)
                     .arg(sells.value(QStringLiteral("count")).toInt());
    QStringList other;
    const QJsonObject oc = s.value(QStringLiteral("other_codes")).toObject();
    for (auto it = oc.constBegin(); it != oc.constEnd(); ++it)
        other << QStringLiteral("%1 %2 (%3)")
                     .arg(it.value().toObject().value(QStringLiteral("count")).toInt())
                     .arg(it.value().toObject().value(QStringLiteral("label")).toString(), it.key());
    if (!other.isEmpty())
        notes << tr("Not trading decisions: %1.").arg(other.join(QStringLiteral(", ")));
    if (win.value(QStringLiteral("filings_failed")).toInt() > 0)
        notes << tr("%1 filing(s) could not be parsed.").arg(win.value(QStringLiteral("filings_failed")).toInt());
    ins_note_->setText(notes.join(QLatin1Char(' ')));

    const QJsonArray rows = d.value(QStringLiteral("transactions")).toArray();
    ins_table_->setSortingEnabled(false);
    ins_table_->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < rows.size(); ++r) {
        const QJsonObject t = rows.at(r).toObject();
        auto* date = text_item(t.value(QStringLiteral("trade_date")).toString(),
                               tr("Filed %1").arg(t.value(QStringLiteral("filed")).toString()));
        date->setData(kUrlRole, t.value(QStringLiteral("url")).toString());
        ins_table_->setItem(r, 0, date);
        ins_table_->setItem(r, 1, text_item(t.value(QStringLiteral("insider")).toString()));
        ins_table_->setItem(r, 2, text_item(t.value(QStringLiteral("role")).toString()));
        const QString code = t.value(QStringLiteral("code")).toString();
        QString tip = t.value(QStringLiteral("code_label")).toString() + QStringLiteral(" (") + code + QLatin1Char(')');
        if (t.value(QStringLiteral("plan_10b5_1")).toBool())
            tip += tr(" · under a Rule 10b5-1 plan");
        auto* code_item = text_item(txn_short(code), tip);
        code_item->setForeground(QColor(code == QLatin1String("P")   ? ui::colors::POSITIVE()
                                        : code == QLatin1String("S") ? ui::colors::NEGATIVE()
                                                                     : ui::colors::TEXT_TERTIARY()));
        ins_table_->setItem(r, 3, code_item);
        const double shares = num(t.value(QStringLiteral("shares")));
        const double price = num(t.value(QStringLiteral("price")));
        const double value = num(t.value(QStringLiteral("value")));
        const double after = num(t.value(QStringLiteral("owned_after")));
        ins_table_->setItem(r, 4, num_item(fmt::grouped(shares, 0), shares));
        ins_table_->setItem(r, 5, num_item(fmt::missing(price) ? fmt::dash() : fmt::price(price, QStringLiteral("USD")),
                                           price));
        ins_table_->setItem(r, 6, num_item(fmt::missing(value) ? fmt::dash() : QStringLiteral("$") + fmt::large(value),
                                           value));
        ins_table_->setItem(r, 7, num_item(fmt::grouped(after, 0), after));
    }
    ins_table_->setSortingEnabled(true);
    ins_table_->resizeColumnsToContents();
    ins_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    if (rows.isEmpty())
        ins_note_->setText(ins_note_->text() + QLatin1Char(' ') + tr("No non-derivative transactions in this window."));
}

void EquityFilingsTab::render_holders(const QJsonObject& d) {
    holders_data_ = d;
    const QJsonObject s = d.value(QStringLiteral("summary")).toObject();
    if (!s.isEmpty()) {
        hold_summary_lbl_->setText(
            tr("Institutions hold %1 of shares outstanding (%2 of float) across %3 holders · insiders %4.")
                .arg(fmt::pct(num(s.value(QStringLiteral("institutions_pct"))), 1),
                     fmt::pct(num(s.value(QStringLiteral("institutions_float_pct"))), 1),
                     fmt::grouped(num(s.value(QStringLiteral("institutions_count"))), 0),
                     fmt::pct(num(s.value(QStringLiteral("insiders_pct"))), 2)));
    } else {
        hold_summary_lbl_->setText(tr("Ownership breakdown not reported."));
    }

    const QJsonArray rows = d.value(QStringLiteral("institutional")).toArray();
    hold_table_->setSortingEnabled(false);
    hold_table_->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < rows.size(); ++r) {
        const QJsonObject h = rows.at(r).toObject();
        hold_table_->setItem(r, 0, text_item(h.value(QStringLiteral("holder")).toString()));
        const double shares = num(h.value(QStringLiteral("shares")));
        const double pct_out = num(h.value(QStringLiteral("pct_out")));
        const double value = num(h.value(QStringLiteral("value")));
        const double chg = num(h.value(QStringLiteral("pct_change")));
        hold_table_->setItem(r, 1, num_item(fmt::large(shares), shares));
        hold_table_->setItem(r, 2, num_item(fmt::pct(pct_out, 2), pct_out));
        hold_table_->setItem(r, 3, num_item(fmt::missing(value) ? fmt::dash() : QStringLiteral("$") + fmt::large(value),
                                            value));
        auto* chg_item = num_item(fmt::pct(chg, 2, true), chg);
        if (!fmt::missing(chg) && chg != 0.0)
            chg_item->setForeground(QColor(chg > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        hold_table_->setItem(r, 4, chg_item);
        hold_table_->setItem(r, 5, text_item(h.value(QStringLiteral("reported")).toString()));
    }
    hold_table_->setSortingEnabled(true);
    hold_table_->resizeColumnsToContents();
    hold_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    hold_table_->setVisible(!rows.isEmpty());

    const QJsonObject own = d.value(QStringLiteral("own_13f")).toObject();
    const QJsonArray positions = own.value(QStringLiteral("positions")).toArray();
    own13f_lbl_->setVisible(!own.isEmpty());
    own13f_table_->setVisible(!positions.isEmpty());
    if (!own.isEmpty()) {
        own13f_lbl_->setText(tr("THIS COMPANY FILES 13F-HR AS AN INSTITUTIONAL MANAGER — top positions of its %1 report "
                                "(%2 across %3 positions, filed %4):")
                                 .arg(own.value(QStringLiteral("report_period")).toString(),
                                      QStringLiteral("$") + fmt::large(num(own.value(QStringLiteral("total_value")))),
                                      fmt::grouped(num(own.value(QStringLiteral("total_holdings"))), 0),
                                      own.value(QStringLiteral("filed")).toString()));
        own13f_table_->setSortingEnabled(false);
        own13f_table_->setRowCount(static_cast<int>(positions.size()));
        for (int r = 0; r < positions.size(); ++r) {
            const QJsonObject p = positions.at(r).toObject();
            QString issuer = p.value(QStringLiteral("issuer")).toString();
            const QString put_call = p.value(QStringLiteral("put_call")).toString();
            if (!put_call.isEmpty())
                issuer += QStringLiteral(" (") + put_call + QLatin1Char(')');
            auto* issuer_item = text_item(issuer, p.value(QStringLiteral("class")).toString());
            issuer_item->setData(kTickerRole, p.value(QStringLiteral("ticker")).toString());
            own13f_table_->setItem(r, 0, issuer_item);
            own13f_table_->setItem(r, 1, text_item(p.value(QStringLiteral("ticker")).toString()));
            const double shares = num(p.value(QStringLiteral("shares")));
            const double value = num(p.value(QStringLiteral("value")));
            own13f_table_->setItem(r, 2, num_item(fmt::large(shares), shares));
            own13f_table_->setItem(r, 3, num_item(QStringLiteral("$") + fmt::large(value), value));
        }
        own13f_table_->setSortingEnabled(true);
        own13f_table_->resizeColumnsToContents();
        own13f_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    }
    QString note = d.value(QStringLiteral("source")).toString();
    if (!note.isEmpty())
        note = tr("Source: %1. Δ shares = change in the holder's position vs the prior quarter.").arg(note);
    if (rows.isEmpty())
        note = tr("No institutional holder list is published for %1.").arg(current_symbol_) + QLatin1Char(' ') + note;
    hold_note_->setText(note);
}

// ─────────────────────────────────────────────────────────────────────────────
// Links
// ─────────────────────────────────────────────────────────────────────────────

void EquityFilingsTab::open_url(const QString& url) {
    const QUrl u(url);
    // Only web links from our own payloads — never hand a file:/custom scheme to the OS.
    if (!u.isValid() || (u.scheme() != QLatin1String("https") && u.scheme() != QLatin1String("http")))
        return;
    QDesktopServices::openUrl(u);
}

void EquityFilingsTab::show_link_menu(QTableWidget* table, const QPoint& pos) {
    auto* item = table->itemAt(pos);
    if (!item)
        return;
    auto* first = table->item(item->row(), 0);
    if (!first)
        return;
    const QString url = first->data(kUrlRole).toString();
    const QString index_url = first->data(kIndexUrlRole).toString();
    if (url.isEmpty() && index_url.isEmpty())
        return;
    QMenu menu(this);
    if (!url.isEmpty())
        connect(menu.addAction(tr("Open document on sec.gov")), &QAction::triggered, this, [this, url]() { open_url(url); });
    if (!index_url.isEmpty())
        connect(menu.addAction(tr("Open filing index (all exhibits)")), &QAction::triggered, this,
                [this, index_url]() { open_url(index_url); });
    menu.addSeparator();
    const QString copy = url.isEmpty() ? index_url : url;
    connect(menu.addAction(tr("Copy link")), &QAction::triggered, this,
            [copy]() { QApplication::clipboard()->setText(copy); });
    menu.exec(table->viewport()->mapToGlobal(pos));
}

// ─────────────────────────────────────────────────────────────────────────────
// i18n
// ─────────────────────────────────────────────────────────────────────────────

void EquityFilingsTab::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void EquityFilingsTab::retranslateUi() {
    title_lbl_->setText(tr("SEC FILINGS"));
    edgar_btn_->setText(tr("EDGAR ↗"));
    refresh_btn_->setText(tr("REFRESH"));
    for (auto& sec : sections_) {
        if (sec.title && sec.title_key)
            sec.title->setText(tr(sec.title_key));
        if (sec.retry)
            sec.retry->setText(tr("RETRY"));
    }
    const QStringList filters = {tr("All forms (excl. insider Forms 3/4/5/144)"), tr("Annual & quarterly reports"),
                                 tr("Current reports (8-K)"), tr("Proxy statements"), tr("Registrations & prospectuses"),
                                 tr("Ownership (13D/13G/13F)"), tr("Other")};
    for (int i = 0; i < filters.size() && i < filter_combo_->count(); ++i)
        filter_combo_->setItemText(i, filters.at(i));
    set_table_headers();
    if (!filings_data_.isEmpty())
        render_filings(filings_data_);
    if (!financials_data_.isEmpty())
        render_financials(financials_data_);
    if (!insiders_data_.isEmpty())
        render_insiders(insiders_data_);
    if (!holders_data_.isEmpty())
        render_holders(holders_data_);
}

} // namespace fincept::screens
