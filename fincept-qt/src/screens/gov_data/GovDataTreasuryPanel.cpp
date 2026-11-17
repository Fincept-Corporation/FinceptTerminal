// src/screens/gov_data/GovDataTreasuryPanel.cpp
#include "screens/gov_data/GovDataTreasuryPanel.h"

#include "core/logging/Logger.h"
#include "screens/gov_data/GovDataProviderPanel.h"
#include "screens/gov_data/GovTableModel.h"
#include "services/gov_data/GovDataService.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/StatTile.h"

#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
namespace fmt = fincept::ui::formatting;

namespace {

// US Treasury identity colour as it reads on the dark theme; always drawn through colors::legible().
const QString kGovDataTreasuryColor = QStringLiteral("#3B82F6");
const QString kGovDataTreasuryScript = QStringLiteral("government_us_data.py");
const QString kGovTreasuryPrefix = QStringLiteral("gov.treasury");
constexpr int kGovTreasuryMaxAgeS = 6 * 3600; // end-of-day data
constexpr int kGovTreasuryFallbackDays = 3;   // business days to step back from a default date without prices

QString treasury_extra_qss() {
    const auto& t = ThemeManager::instance().tokens();
    return QString("#govFilterBar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
}

QString treasury_date_text(const QDate& d) {
    return QLocale().toString(d, QStringLiteral("d MMM yyyy"));
}

QString treasury_pct(const QJsonValue& v) {
    // Rates arrive as fractions (0.04125 == 4.125 %); 0 / missing = no coupon (bills) or not published.
    return v.isDouble() && v.toDouble() > 0 ? fmt::fraction_pct(v.toDouble(), 3, false) : fmt::placeholder();
}

QString treasury_num(const QJsonValue& v, int decimals = 3) {
    return v.isDouble() ? fmt::number(v.toDouble(), decimals) : fmt::placeholder();
}

QJsonValue treasury_double(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (v.isDouble())
            return v;
        if (v.isString()) { // a numeric string from an older script version
            bool ok = false;
            const double d = v.toString().trimmed().toDouble(&ok);
            if (ok)
                return QJsonValue(d);
        }
    }
    return QJsonValue(QJsonValue::Null);
}

} // namespace

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataTreasuryPanel::GovDataTreasuryPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(make_gov_panel_style(kGovDataTreasuryColor, treasury_extra_qss())); // once, before children
    default_end_ = previous_business_day(QDate::currentDate());
    default_start_ = QDate::currentDate().addMonths(-1);
    build_ui();
    retranslateUi();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataTreasuryPanel::on_result);
    // The prices / auctions tables are models inked from the live theme; the small breakdown table's
    // item brushes are not reached by stylesheets: recolour it once the new appearance has settled.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, &GovDataTreasuryPanel::recolor_items);

    // The security type and the tab come back with the panel. The dates start on the latest
    // business day every session (a remembered date would pin the tab to old prices).
    memory_ = new InputMemory(QStringLiteral("gov_data"), QStringLiteral("us_treasury"), this);
    memory_->track(QStringLiteral("type"), security_type_);
    memory_->track(QStringLiteral("tab_prices"), prices_btn_);
    memory_->track(QStringLiteral("tab_auctions"), auctions_btn_);
    memory_->track(QStringLiteral("tab_summary"), summary_btn_);
    memory_->restore();
    const Endpoint first = summary_btn_->isChecked()    ? Summary
                           : auctions_btn_->isChecked() ? Auctions
                                                        : Prices;
    go(first, false); // loads when the pane is first shown
}

void GovDataTreasuryPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

QDate GovDataTreasuryPanel::previous_business_day(const QDate& d) {
    QDate p = d.addDays(-1);
    while (p.dayOfWeek() > 5)
        p = p.addDays(-1);
    return p;
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());
    root->addWidget(build_filter_bar());
    build_pane(this);
    root->addWidget(pane_, 1);
}

void GovDataTreasuryPanel::build_pane(QWidget* parent) {
    pane_ = gov::make_pane(parent, QStringLiteral("gov_data/us_treasury"), as_of_label_);
    content_stack_ = new QStackedWidget(pane_->content_parent());

    using Ink = GovTableModel::Ink;
    const auto field = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return r.value(k).toString(); };
    };
    const auto field_or_dash = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) {
            const QString s = r.value(k).toString();
            return s.isEmpty() ? fmt::placeholder() : s;
        };
    };
    const auto pct = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return treasury_pct(r.value(k)); };
    };
    const auto num = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return treasury_num(r.value(k)); };
    };
    const Qt::Alignment right = Qt::AlignRight | Qt::AlignVCenter;

    // Prices table: CUSIP | TYPE | COUPON | MATURITY | BID | OFFER | EOD PRICE (headers set in retranslateUi)
    prices_model_ = gov::make_model(kGovDataTreasuryColor, this);
    prices_model_->set_columns({
        {.text = field("cusip")},
        {.text = field("type"), .ink = Ink::Accent},
        {.text = pct("rate"), .align = right},
        {.text = field_or_dash("maturity")},
        {.text = num("bid"), .align = right},
        {.text = num("offer"), .align = right},
        {.text = num("eod"), .align = right},
    });
    prices_table_ = gov::make_table_view(content_stack_, prices_model_);
    prices_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    prices_table_->viewport()->setCursor(Qt::ArrowCursor); // rows open nothing
    content_stack_->addWidget(prices_table_);

    // Auctions table: CUSIP | TYPE | TERM | AUCTION DATE | HIGH RATE | HIGH PRICE | BID/COVER | OFFERING
    auctions_model_ = gov::make_model(kGovDataTreasuryColor, this);
    auctions_model_->set_columns({
        {.text = field("cusip")},
        {.text = field("type"), .ink = Ink::Accent},
        {.text = field("term")},
        {.text = field_or_dash("date")},
        {.text = pct("rate"), .align = right},
        {.text = num("price"), .align = right},
        {.text = [](const QJsonObject& r) {
             const QJsonValue btc = r.value(QStringLiteral("btc"));
             return btc.isDouble() ? fmt::ratio(btc.toDouble()) : fmt::placeholder();
         },
         .align = right},
        // Offering amounts are US dollars (the currency of every Treasury security).
        {.text = [](const QJsonObject& r) {
             const QJsonValue offering = r.value(QStringLiteral("offering"));
             return offering.isDouble() && offering.toDouble() > 0
                        ? fmt::money(offering.toDouble(), QStringLiteral("USD"), fmt::Compact::FromSettings)
                        : fmt::placeholder();
         },
         .align = right},
    });
    auctions_table_ = gov::make_table_view(content_stack_, auctions_model_);
    auctions_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auctions_table_->viewport()->setCursor(Qt::ArrowCursor);
    content_stack_->addWidget(auctions_table_);

    // Summary: KPI tiles (they wrap at any width / font size) + the breakdown by security type
    summary_page_ = new QWidget(content_stack_);
    summary_page_->setObjectName("govSummaryPage");
    auto* svl = new QVBoxLayout(summary_page_);
    svl->setContentsMargins(16, 16, 16, 16);
    svl->setSpacing(12);
    summary_strip_ = new KpiStrip(summary_page_);
    tile_total_ = summary_strip_->add_tile(QString());
    tile_min_rate_ = summary_strip_->add_tile(QString());
    tile_max_rate_ = summary_strip_->add_tile(QString());
    tile_avg_rate_ = summary_strip_->add_tile(QString());
    tile_min_price_ = summary_strip_->add_tile(QString());
    tile_max_price_ = summary_strip_->add_tile(QString());
    tile_avg_price_ = summary_strip_->add_tile(QString());
    svl->addWidget(summary_strip_);
    breakdown_hdr_lbl_ = new QLabel(summary_page_);
    breakdown_hdr_lbl_->setObjectName("govSectionHdr");
    svl->addWidget(breakdown_hdr_lbl_);
    type_breakdown_table_ = new QTableWidget(summary_page_);
    type_breakdown_table_->setColumnCount(2);
    type_breakdown_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    configure_table(type_breakdown_table_);
    type_breakdown_table_->viewport()->setCursor(Qt::ArrowCursor);
    svl->addWidget(type_breakdown_table_, 1);
    content_stack_->addWidget(summary_page_);

    // An empty answer is a result: say so, with the way to a date that has data
    no_data_page_ = gov::make_no_data_page(content_stack_);
    connect(no_data_page_, &EmptyState::primary_clicked, this, [this]() {
        if (active_endpoint_ == Auctions) {
            start_date_->setDate(end_date_->date().addMonths(-3));
        } else if (no_data_retry_date_.isValid()) {
            end_date_->setDate(no_data_retry_date_);
        }
        go(active_endpoint_, true, true);
    });
    content_stack_->addWidget(no_data_page_);

    pane_->set_content(content_stack_);
    pane_->set_start([this](quint64 run) { start_run(run); });
    pane_->set_cancel([this](quint64 run) { gov::cancel_token(tokens_, run); });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p); });
}

QWidget* GovDataTreasuryPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setMinimumHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Endpoint tabs (text set in retranslateUi)
    prices_btn_ = new QPushButton(bar);
    prices_btn_->setObjectName("govTabBtn");
    prices_btn_->setCheckable(true);
    prices_btn_->setChecked(true);
    prices_btn_->setCursor(Qt::PointingHandCursor);
    connect(prices_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Prices); });

    auctions_btn_ = new QPushButton(bar);
    auctions_btn_->setObjectName("govTabBtn");
    auctions_btn_->setCheckable(true);
    auctions_btn_->setCursor(Qt::PointingHandCursor);
    connect(auctions_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Auctions); });

    summary_btn_ = new QPushButton(bar);
    summary_btn_->setObjectName("govTabBtn");
    summary_btn_->setCheckable(true);
    summary_btn_->setCursor(Qt::PointingHandCursor);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Summary); });

    hl->addWidget(prices_btn_);
    hl->addWidget(auctions_btn_);
    hl->addWidget(summary_btn_);
    hl->addStretch(1);

    as_of_label_ = gov::make_as_of_label(bar);
    hl->addWidget(as_of_label_);
    hl->addSpacing(8);

    fetch_btn_ = new QPushButton(bar);
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

QWidget* GovDataTreasuryPanel::build_filter_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govFilterBar");
    bar->setMinimumHeight(34);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);

    // Filter labels (text set in retranslateUi)
    filter_from_lbl_ = new QLabel(bar);
    filter_from_lbl_->setObjectName("govFilterLabel");
    hl->addWidget(filter_from_lbl_);

    start_date_ = new QDateEdit(default_start_, bar);
    start_date_->setCalendarPopup(true);
    start_date_->setDisplayFormat("yyyy-MM-dd");
    start_date_->setMinimumHeight(24);
    hl->addWidget(start_date_);

    filter_to_lbl_ = new QLabel(bar);
    filter_to_lbl_->setObjectName("govFilterLabel");
    hl->addWidget(filter_to_lbl_);

    end_date_ = new QDateEdit(default_end_, bar);
    end_date_->setCalendarPopup(true);
    end_date_->setDisplayFormat("yyyy-MM-dd");
    end_date_->setMaximumDate(QDate::currentDate());
    end_date_->setMinimumHeight(24);
    hl->addWidget(end_date_);

    hl->addSpacing(16);
    filter_type_lbl_ = new QLabel(bar);
    filter_type_lbl_->setObjectName("govFilterLabel");
    hl->addWidget(filter_type_lbl_);

    // ComboBox items: store the API key in userData; display text is set
    // (and re-set on language change) by retranslateUi.
    security_type_ = new QComboBox(bar);
    security_type_->addItem(QString(), "all");
    security_type_->addItem(QString(), "bill");
    security_type_->addItem(QString(), "note");
    security_type_->addItem(QString(), "bond");
    security_type_->addItem(QString(), "tips");
    security_type_->addItem(QString(), "frn");
    security_type_->setMinimumHeight(24);
    hl->addWidget(security_type_);

    hl->addStretch(1);
    info_label_ = new QLabel(bar);
    info_label_->setObjectName("govBreadcrumbText");
    hl->addWidget(info_label_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataTreasuryPanel::retranslateUi() {
    if (prices_btn_)
        prices_btn_->setText(tr("PRICES"));
    if (auctions_btn_)
        auctions_btn_->setText(tr("AUCTIONS"));
    if (summary_btn_)
        summary_btn_->setText(tr("SUMMARY"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));

    if (filter_from_lbl_)
        filter_from_lbl_->setText(tr("FROM"));
    if (filter_to_lbl_)
        filter_to_lbl_->setText(tr("TO"));
    if (filter_type_lbl_)
        filter_type_lbl_->setText(tr("TYPE"));

    if (security_type_) {
        // Preserve the user's current selection across language change by
        // operating on the user-data key, not the visible text.
        const QString cur = security_type_->currentData().toString();
        security_type_->setItemText(0, tr("All"));
        security_type_->setItemText(1, tr("Bills"));
        security_type_->setItemText(2, tr("Notes"));
        security_type_->setItemText(3, tr("Bonds"));
        security_type_->setItemText(4, tr("TIPS"));
        security_type_->setItemText(5, tr("FRN"));
        const int idx = security_type_->findData(cur);
        if (idx >= 0)
            security_type_->setCurrentIndex(idx);
    }

    if (prices_model_)
        prices_model_->set_headers(
            {tr("CUSIP"), tr("TYPE"), tr("COUPON"), tr("MATURITY"), tr("BID"), tr("OFFER"), tr("EOD PRICE")});
    if (auctions_model_)
        auctions_model_->set_headers({tr("CUSIP"), tr("TYPE"), tr("TERM"), tr("AUCTION DATE"), tr("HIGH RATE"),
                                      tr("HIGH PRICE"), tr("BID/COVER"), tr("OFFERING")});
    if (type_breakdown_table_)
        type_breakdown_table_->setHorizontalHeaderLabels({tr("TYPE"), tr("COUNT")});

    if (tile_total_) {
        tile_total_->set_label(tr("TOTAL SECURITIES"));
        tile_min_rate_->set_label(tr("MIN COUPON"));
        tile_max_rate_->set_label(tr("MAX COUPON"));
        tile_avg_rate_->set_label(tr("AVG COUPON"));
        tile_min_price_->set_label(tr("MIN PRICE"));
        tile_max_price_->set_label(tr("MAX PRICE"));
        tile_avg_price_->set_label(tr("AVG PRICE"));
        // FedInvest's RATE column is the coupon, not a yield; bills (no coupon) are excluded.
        const QString yield_sub = tr("coupon, excl. bills");
        const QString price_sub = tr("per 100 face value");
        tile_min_rate_->set_sub_label(yield_sub);
        tile_max_rate_->set_sub_label(yield_sub);
        tile_avg_rate_->set_sub_label(yield_sub);
        tile_min_price_->set_sub_label(price_sub);
        tile_max_price_->set_sub_label(price_sub);
        tile_avg_price_->set_sub_label(price_sub);
    }

    if (breakdown_hdr_lbl_)
        breakdown_hdr_lbl_->setText(tr("SECURITY TYPE BREAKDOWN"));
}

// ── Views and runs ───────────────────────────────────────────────────────────

QString GovDataTreasuryPanel::view_key(Endpoint ep) const {
    const QDate end = end_date_->date();
    const QDate start = start_date_->date();
    const QString end_tok = end == default_end_ ? QStringLiteral("latest") : end.toString(Qt::ISODate);
    const QString start_tok = start == default_start_ ? QStringLiteral("default") : start.toString(Qt::ISODate);
    const QString type = security_type_->currentData().toString();
    switch (ep) {
        case Prices:
            return QStringLiteral("prices|") + end_tok + QLatin1Char('|') + type;
        case Auctions:
            return QStringLiteral("auctions|") + start_tok + QLatin1Char('|') + end_tok + QLatin1Char('|') + type;
        case Summary:
            return QStringLiteral("summary|") + end_tok;
    }
    return {};
}

GovDataTreasuryPanel::Endpoint GovDataTreasuryPanel::endpoint_of(const QString& key) {
    if (key.startsWith(QLatin1String("auctions|")))
        return Auctions;
    if (key.startsWith(QLatin1String("summary|")))
        return Summary;
    return Prices;
}

QDate GovDataTreasuryPanel::resolve_end(const QString& token) const {
    if (token == QLatin1String("latest"))
        return default_end_;
    const QDate d = QDate::fromString(token, Qt::ISODate);
    return d.isValid() ? d : default_end_;
}

QDate GovDataTreasuryPanel::resolve_start(const QString& token) const {
    if (token == QLatin1String("default"))
        return default_start_;
    const QDate d = QDate::fromString(token, Qt::ISODate);
    return d.isValid() ? d : default_start_;
}

void GovDataTreasuryPanel::go(Endpoint ep, bool user, bool force) {
    active_endpoint_ = ep;
    pane_->set_loading_text(ep == Prices     ? tr("Loading end-of-day Treasury prices from FedInvest…")
                            : ep == Auctions ? tr("Loading Treasury auction results…")
                                             : tr("Summarising Treasury prices…"));
    const QString key = view_key(ep);
    // End-of-day data: a remembered view older than 6 h refreshes quietly on the next visit.
    pane_->set_view_max_age(key, kGovTreasuryMaxAgeS);
    pane_->set_view_skeleton(key, ep == Summary ? LoadingSkeleton::Shape::Cards : LoadingSkeleton::Shape::Table);
    pane_->set_view(key);
    update_toolbar_state();
    const bool running_here = pane_->is_running() && pane_->running_view() == key;
    if (force || (user && !pane_->has_result() && !running_here))
        pane_->run();
}

void GovDataTreasuryPanel::load_initial_data() {
    if (pane_->view().isEmpty())
        go(active_endpoint_, false);
}

void GovDataTreasuryPanel::on_endpoint_changed(int index) {
    go(static_cast<Endpoint>(index), true);
}

void GovDataTreasuryPanel::on_fetch() {
    go(active_endpoint_, true, true);
}

void GovDataTreasuryPanel::start_run(quint64 run) {
    fallback_left_ = kGovTreasuryFallbackDays;
    gov::start_token(tokens_, run);
    request(run, pane_->view());
}

void GovDataTreasuryPanel::request(quint64 run, const QString& key) {
    const QStringList parts = key.split(QLatin1Char('|'));
    QString command;
    QStringList args;
    switch (endpoint_of(key)) {
        case Prices: {
            command = QStringLiteral("treasury_prices");
            args << resolve_end(parts.value(1)).toString(Qt::ISODate);
            const QString type = parts.value(2);
            if (!type.isEmpty() && type != QLatin1String("all"))
                args << QString() << type; // empty CUSIP placeholder, then the type
            break;
        }
        case Auctions: {
            command = QStringLiteral("treasury_auctions");
            const QString type = parts.value(3);
            args << resolve_start(parts.value(1)).toString(Qt::ISODate) << resolve_end(parts.value(2)).toString(Qt::ISODate)
                 << (type == QLatin1String("all") ? QString() : type) << QStringLiteral("50") << QStringLiteral("1");
            break;
        }
        case Summary:
            command = QStringLiteral("summary");
            args << resolve_end(parts.value(1)).toString(Qt::ISODate);
            break;
    }
    services::GovDataService::instance().execute(kGovDataTreasuryScript, command, args,
                                                 gov::request_id(kGovTreasuryPrefix, run), tokens_.value(run));
}

void GovDataTreasuryPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    const quint64 run = gov::run_of(request_id, kGovTreasuryPrefix);
    if (run == 0)
        return;
    if (!pane_->is_current(run)) {
        tokens_.remove(run);
        return; // superseded, cancelled, or its view was left
    }
    const QString key = pane_->running_view();
    const QStringList parts = key.split(QLatin1Char('|'));
    const Endpoint ep = endpoint_of(key);

    if (!result.success) {
        if (result.error_code == QLatin1String("no_data") && ep != Auctions) {
            const QString end_tok = parts.value(1);
            if (end_tok == QLatin1String("latest") && fallback_left_ > 0) {
                // The default date has no prices yet (holiday, not yet published): the latest
                // business day before it is what "latest" means.
                --fallback_left_;
                default_end_ = previous_business_day(default_end_);
                {
                    const QSignalBlocker block(end_date_);
                    end_date_->setDate(default_end_);
                }
                LOG_DEBUG("GovTreasury", logf("No prices for the default date, trying the business day before",
                                              {{"date", default_end_.toString(Qt::ISODate)}}));
                request(run, key);
                return;
            }
            tokens_.remove(run);
            pane_->deliver(run, QJsonObject{{QStringLiteral("kind"), ep == Prices ? QStringLiteral("prices")
                                                                                  : QStringLiteral("summary")},
                                            {QStringLiteral("rows"), QJsonArray()},
                                            {QStringLiteral("no_data"), true},
                                            {QStringLiteral("date"), resolve_end(end_tok).toString(Qt::ISODate)}});
            return;
        }
        tokens_.remove(run);
        gov::fail_pane(pane_, run, result, tr("TreasuryDirect"));
        return;
    }
    tokens_.remove(run);

    // The answer (~460 securities on a business day) is shaped on the shaping thread; the dates of
    // the view are resolved here (they depend on the panel's state) and passed as text.
    const QString start_iso = ep == Auctions ? resolve_start(parts.value(1)).toString(Qt::ISODate) : QString();
    const QString end_iso = resolve_end(parts.value(ep == Auctions ? 2 : 1)).toString(Qt::ISODate);
    gov::shape_async(
        this, [ep, data = result.data, start_iso, end_iso]() { return shape(ep, data, start_iso, end_iso); },
        [this, run](const QJsonObject& payload) {
            if (pane_->is_current(run)) // superseded, cancelled or its view left meanwhile: dropped
                pane_->deliver(run, payload);
        });
}

QJsonObject GovDataTreasuryPanel::shape(Endpoint ep, const QJsonObject& data, const QString& start_iso,
                                        const QString& end_iso) {
    const QJsonArray raw = data.value("data").isArray() ? data.value("data").toArray() : data.value("json").toArray();
    QJsonObject payload;
    if (ep == Prices) {
        QJsonArray rows;
        for (const QJsonValue& v : raw) {
            const QJsonObject r = v.toObject();
            rows.append(QJsonObject{{"cusip", gov::text(r.value("cusip"))},
                                    {"type", gov::text(r.value("security_type"))},
                                    {"rate", treasury_double(r, {"rate"})},
                                    {"maturity", gov::text(r.value("maturity_date"))},
                                    {"bid", treasury_double(r, {"bid"})},
                                    {"offer", treasury_double(r, {"offer"})},
                                    {"eod", treasury_double(r, {"eod_price"})}});
        }
        payload = QJsonObject{{"kind", "prices"}, {"rows", rows}, {"date", data.value("date").toString(end_iso)}};
    } else if (ep == Auctions) {
        QJsonArray rows;
        for (const QJsonValue& v : raw) {
            const QJsonObject r = v.toObject();
            // TA_WS records carry no "auctionDate": the auction is the competitive close date
            // ("2026-11-09T00:00:00"); issueDate is the last resort.
            QString adate;
            for (const char* k : {"auctionDate", "competitiveAuctionCloseDate", "issueDate"}) {
                adate = gov::text(r.value(QLatin1String(k))).left(10);
                if (!adate.isEmpty())
                    break;
            }
            // Bills auction on a discount rate; notes/bonds on a yield. All arrive as fractions.
            rows.append(QJsonObject{
                {"cusip", gov::text(r.value("cusip"))},
                {"type", gov::text(r.value("securityType"))},
                {"term", gov::text(r.value("securityTerm"))},
                {"date", adate},
                {"rate", treasury_double(r, {"highDiscountRate", "highRate", "highYield", "highInvestmentRate"})},
                {"price", treasury_double(r, {"highPrice"})},
                {"btc", treasury_double(r, {"bidToCoverRatio"})},
                {"offering", treasury_double(r, {"offeringAmount"})}});
        }
        payload = QJsonObject{{"kind", "auctions"}, {"rows", rows}, {"start", start_iso}, {"end", end_iso}};
    } else {
        payload = data;
        payload.insert("kind", "summary");
        if (!payload.value("date").isString())
            payload.insert("date", end_iso);
    }
    return payload;
}

// ── Render ───────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::render(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QString kind = p.value("kind").toString();
    const QJsonArray rows = p.value("rows").toArray();
    const QDate date = QDate::fromString(p.value("date").toString(), Qt::ISODate);

    if (p.value("no_data").toBool(false) || (kind != QLatin1String("summary") && rows.isEmpty())) {
        show_no_data(p);
        return;
    }
    if (kind == QLatin1String("prices")) {
        populate_prices(rows);
        content_stack_->setCurrentWidget(prices_table_);
        info_label_->setText(tr("%1 securities · prices of %2")
                                 .arg(fmt::quantity(rows.size()), treasury_date_text(date)));
    } else if (kind == QLatin1String("auctions")) {
        populate_auctions(rows);
        content_stack_->setCurrentWidget(auctions_table_);
        info_label_->setText(tr("%1 auctions · %2 – %3")
                                 .arg(fmt::quantity(rows.size()),
                                      treasury_date_text(QDate::fromString(p.value("start").toString(), Qt::ISODate)),
                                      treasury_date_text(QDate::fromString(p.value("end").toString(), Qt::ISODate))));
    } else {
        populate_summary(p);
        content_stack_->setCurrentWidget(summary_page_);
        info_label_->setText(tr("Prices of %1").arg(treasury_date_text(date)));
    }
}

void GovDataTreasuryPanel::show_no_data(const QJsonObject& p) {
    info_label_->clear();
    EmptyState::Spec spec;
    if (p.value("kind").toString() == QLatin1String("auctions")) {
        no_data_retry_date_ = {};
        spec = EmptyState::no_data_spec(
            tr("No auctions in this date range"),
            tr("No Treasury auction closed between %1 and %2.")
                .arg(treasury_date_text(QDate::fromString(p.value("start").toString(), Qt::ISODate)),
                     treasury_date_text(QDate::fromString(p.value("end").toString(), Qt::ISODate))),
            tr("WIDEN TO 3 MONTHS"));
    } else {
        const QDate date = QDate::fromString(p.value("date").toString(), Qt::ISODate);
        no_data_retry_date_ = previous_business_day(date.isValid() ? date : QDate::currentDate());
        spec = EmptyState::no_data_spec(
            tr("No prices for %1").arg(treasury_date_text(date)),
            tr("FedInvest publishes end-of-day prices on business days only, after the market close."),
            tr("SHOW %1").arg(treasury_date_text(no_data_retry_date_).toUpper()));
    }
    spec.glyph = QStringLiteral("∅");
    gov::show_no_data(content_stack_, no_data_page_, spec);
}

void GovDataTreasuryPanel::populate_prices(const QJsonArray& rows) {
    gov::show_rows(prices_table_, prices_model_, rows); // a model reset: cells are formatted when shown
    LOG_DEBUG("GovTreasury", QString("Loaded %1 price records").arg(rows.size()));
}

void GovDataTreasuryPanel::populate_auctions(const QJsonArray& rows) {
    gov::show_rows(auctions_table_, auctions_model_, rows);
    LOG_DEBUG("GovTreasury", QString("Loaded %1 auction records").arg(rows.size()));
}

void GovDataTreasuryPanel::populate_summary(const QJsonObject& json) {
    tile_total_->set_value(fmt::quantity(gov::integer(json.value("total_securities"), 0)));

    // Coupon rates of coupon-bearing securities (bills carry none and are left out by the script).
    const QJsonObject yield = json.value("yield_summary").toObject();
    tile_min_rate_->set_value(treasury_pct(yield.value("min_rate")));
    tile_max_rate_->set_value(treasury_pct(yield.value("max_rate")));
    tile_avg_rate_->set_value(treasury_pct(yield.value("avg_rate")));

    // EOD price, else the offer / bid quote; absent when FedInvest published no quotes.
    const QJsonObject price = json.value("price_summary").toObject();
    tile_min_price_->set_value(treasury_num(price.value("min_price"), 2));
    tile_max_price_->set_value(treasury_num(price.value("max_price"), 2));
    tile_avg_price_->set_value(treasury_num(price.value("avg_price"), 2));

    const QColor accent = colors::legible_color(QColor(kGovDataTreasuryColor));
    const QJsonObject types = json.value("security_types").toObject();
    {
        gov::TableFill fill(type_breakdown_table_, int(types.size()));
        int row = 0;
        for (auto it = types.begin(); it != types.end(); ++it, ++row) {
            auto* name_item = new QTableWidgetItem(it.key());
            name_item->setForeground(accent);
            type_breakdown_table_->setItem(row, 0, name_item);
            auto* count_item = new QTableWidgetItem(fmt::quantity(gov::integer(it.value(), 0)));
            count_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            type_breakdown_table_->setItem(row, 1, count_item);
        }
    }
    LOG_DEBUG("GovTreasury", "Summary loaded");
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::update_toolbar_state() {
    prices_btn_->setChecked(active_endpoint_ == Prices);
    auctions_btn_->setChecked(active_endpoint_ == Auctions);
    summary_btn_->setChecked(active_endpoint_ == Summary);
    // Only what the tab uses is editable: FROM is the auctions window, TYPE filters lists.
    start_date_->setEnabled(active_endpoint_ == Auctions);
    filter_from_lbl_->setEnabled(active_endpoint_ == Auctions);
    security_type_->setEnabled(active_endpoint_ != Summary);
    filter_type_lbl_->setEnabled(active_endpoint_ != Summary);
}

void GovDataTreasuryPanel::recolor_items() {
    // The prices / auctions models read their ink at paint time; only the breakdown has item brushes.
    recolor_column(type_breakdown_table_, 0, colors::legible_color(QColor(kGovDataTreasuryColor)));
}

void GovDataTreasuryPanel::on_export_csv() {
    QTableView* table = nullptr;
    if (content_stack_->currentWidget() == prices_table_)
        table = prices_table_;
    else if (content_stack_->currentWidget() == auctions_table_)
        table = auctions_table_;
    else if (content_stack_->currentWidget() == summary_page_)
        table = type_breakdown_table_;
    export_table_to_csv(table, "treasury_data.csv", this);
}

} // namespace fincept::screens
