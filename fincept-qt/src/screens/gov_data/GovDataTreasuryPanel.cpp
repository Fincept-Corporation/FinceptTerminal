// src/screens/gov_data/GovDataTreasuryPanel.cpp
#include "screens/gov_data/GovDataTreasuryPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const QString kGovDataTreasuryColor = "#3B82F6";
static const QString kGovDataTreasuryScript = "government_us_data.py";

// ── Dynamic stylesheet ──────────────────────────────────────────────────────

static QString build_treasury_style() {
    const auto& t = ThemeManager::instance().tokens();
    const auto cc = QColor(kGovDataTreasuryColor);
    const QString cr = QString::number(cc.red());
    const QString cg = QString::number(cc.green());
    const QString cb = QString::number(cc.blue());
    const QString c = kGovDataTreasuryColor;

    QString s;
    s += QString("#govTreasuryToolbar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_raised, t.border_dim);

    s += QString("#govTreasTab { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }")
             .arg(t.text_secondary, t.border_dim);
    s += QString("#govTreasTab:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);
    s += QString("#govTreasTab:checked { background:rgba(%1,%2,%3,0.12); color:%4;"
                 "  border:1px solid %4; }")
             .arg(cr, cg, cb, c);

    s += QString("#govFetchBtn { background:%1; color:%2; border:none;"
                 "  font-size:10px; font-weight:700; padding:4px 14px; }")
             .arg(c, t.bg_base);
    s += QString("#govFetchBtn:hover { background:%1; }").arg(cc.lighter(120).name());
    s += QString("#govFetchBtn:disabled { background:%1; color:%2; }").arg(t.border_dim, t.text_dim);
    s += QString("#govCsvBtn { background:transparent; color:%1; border:1px solid %2;"
                 "  font-size:10px; font-weight:700; padding:4px 10px; }")
             .arg(t.text_secondary, t.border_dim);
    s += QString("#govCsvBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    // Filter bar
    s += QString("#govTreasuryFilterBar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#govTreasuryFilterLabel { color:%1; font-size:9px; font-weight:700;"
                 "  letter-spacing:0.5px; background:transparent; }")
             .arg(t.text_secondary);

    s += QString("QDateEdit { background:%1; color:%2; border:1px solid %3;"
                 "  font-size:11px; padding:2px 6px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim);
    s += "QDateEdit::drop-down { border:none; }";
    s += QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                 "  font-size:11px; padding:2px 6px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim);
    s += "QComboBox::drop-down { border:none; }";
    s += QString("QComboBox QAbstractItemView { background:%1; color:%2;"
                 "  border:1px solid %3; }")
             .arg(t.bg_raised, t.text_primary, t.border_dim);

    s += QString("QTableWidget { background:%1; color:%2; border:none;"
                 "  gridline-color:%3; font-size:11px; alternate-background-color:%4; }")
             .arg(t.bg_base, t.text_primary, t.border_dim, t.bg_surface);
    s += QString("QTableWidget::item { padding:5px 8px; border-bottom:1px solid %1; }").arg(t.border_dim);
    s += QString("QTableWidget::item:selected { background:rgba(%1,%2,%3,0.1); color:%4; }").arg(cr, cg, cb, c);
    s += QString("QHeaderView::section { background:%1; color:%2; border:none;"
                 "  border-bottom:2px solid %3; border-right:1px solid %3;"
                 "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }")
             .arg(t.bg_raised, t.text_secondary, t.border_dim);

    // Summary cards
    s += QString("#govSummaryPage { background:%1; }").arg(t.bg_base);
    s += QString("#govSumCard { background:%1; border:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#govSumCard:hover { border-color:%1; }").arg(t.border_bright);
    s += QString("#govSumLabel { color:%1; font-size:8px; font-weight:700;"
                 "  letter-spacing:1px; background:transparent; }")
             .arg(t.text_secondary);
    s += QString("#govSumValue { color:%1; font-size:16px; font-weight:700; background:transparent; }").arg(c);
    s += QString("#govSumSub   { color:%1; font-size:9px; background:transparent; }").arg(t.text_secondary);

    // Status page
    s += QString("#govStatusPage { background:%1; }").arg(t.bg_base);
    s += QString("#govStatusMsg { color:%1; font-size:13px; background:transparent; }").arg(t.text_secondary);
    s += QString("#govStatusErr { color:%1; font-size:12px; background:transparent; }").arg(t.negative);

    s += QString("QScrollBar:vertical { background:%1; width:5px; }").arg(t.bg_base);
    s += QString("QScrollBar::handle:vertical { background:%1; min-height:20px; }").arg(t.border_dim);
    s += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";
    return s;
}

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataTreasuryPanel::GovDataTreasuryPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(build_treasury_style());
    build_ui();
    retranslateUi();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataTreasuryPanel::on_result);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this]() { setStyleSheet(build_treasury_style()); });
}

void GovDataTreasuryPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());
    root->addWidget(build_filter_bar());

    content_stack_ = new QStackedWidget;

    // ── Page 0: Prices table (headers set in retranslateUi) ──
    prices_table_ = new QTableWidget;
    prices_table_->setColumnCount(7);
    prices_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    configure_table(prices_table_);
    content_stack_->addWidget(prices_table_);

    // ── Page 1: Auctions table (headers set in retranslateUi) ──
    auctions_table_ = new QTableWidget;
    auctions_table_->setColumnCount(8);
    auctions_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    configure_table(auctions_table_);
    content_stack_->addWidget(auctions_table_);

    // ── Page 2: Summary ──
    auto* summary_page = new QWidget(this);
    summary_page->setObjectName("govSummaryPage");
    auto* svl = new QVBoxLayout(summary_page);
    svl->setContentsMargins(16, 16, 16, 16);
    svl->setSpacing(12);

    // Stat cards row
    auto* cards_row = new QWidget(this);
    auto* crhl = new QHBoxLayout(cards_row);
    crhl->setContentsMargins(0, 0, 0, 0);
    crhl->setSpacing(8);

    // Card factory captures title-label and sub-label pointers so retranslateUi
    // can re-set their text on language change. The value label receives data
    // from populate_summary and isn't translated.
    auto make_card = [&](QLabel*& title_out, QLabel*& val_out, QLabel** sub_out = nullptr) {
        auto* card = new QWidget(this);
        card->setObjectName("govSumCard");
        card->setMinimumWidth(110);
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(12, 8, 12, 8);
        cvl->setSpacing(2);
        title_out = new QLabel;
        title_out->setObjectName("govSumLabel");
        val_out = new QLabel("—");
        val_out->setObjectName("govSumValue");
        cvl->addWidget(title_out);
        cvl->addWidget(val_out);
        if (sub_out) {
            *sub_out = new QLabel;
            (*sub_out)->setObjectName("govSumSub");
            cvl->addWidget(*sub_out);
        }
        crhl->addWidget(card);
    };

    make_card(card_total_lbl_,     total_securities_label_);
    make_card(card_min_rate_lbl_,  min_rate_label_,  &card_min_rate_sub_);
    make_card(card_max_rate_lbl_,  max_rate_label_,  &card_max_rate_sub_);
    make_card(card_avg_rate_lbl_,  avg_rate_label_,  &card_avg_rate_sub_);
    make_card(card_min_price_lbl_, min_price_label_, &card_min_price_sub_);
    make_card(card_max_price_lbl_, max_price_label_, &card_max_price_sub_);
    make_card(card_avg_price_lbl_, avg_price_label_, &card_avg_price_sub_);
    crhl->addStretch(1);
    svl->addWidget(cards_row);

    // Breakdown table
    breakdown_hdr_lbl_ = new QLabel;
    breakdown_hdr_lbl_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;"
                                              " background:transparent; margin-top:8px;")
                                          .arg(kGovDataTreasuryColor));
    svl->addWidget(breakdown_hdr_lbl_);

    type_breakdown_table_ = new QTableWidget;
    type_breakdown_table_->setColumnCount(2);
    type_breakdown_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    type_breakdown_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    type_breakdown_table_->verticalHeader()->setVisible(false);
    type_breakdown_table_->setAlternatingRowColors(true);
    type_breakdown_table_->setMaximumHeight(200);
    svl->addWidget(type_breakdown_table_);
    svl->addStretch(1);
    content_stack_->addWidget(summary_page);

    // ── Page 3: Status ──
    auto* status_page = new QWidget(this);
    status_page->setObjectName("govStatusPage");
    auto* spvl = new QVBoxLayout(status_page);
    spvl->setAlignment(Qt::AlignCenter);
    status_label_ = new QLabel;
    status_label_->setObjectName("govStatusMsg");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    spvl->addWidget(status_label_);
    content_stack_->addWidget(status_page);

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataTreasuryPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govTreasuryToolbar");
    bar->setFixedHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Endpoint tabs (text set in retranslateUi)
    prices_btn_ = new QPushButton;
    prices_btn_->setObjectName("govTreasTab");
    prices_btn_->setCheckable(true);
    prices_btn_->setChecked(true);
    prices_btn_->setCursor(Qt::PointingHandCursor);
    connect(prices_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Prices); });

    auctions_btn_ = new QPushButton;
    auctions_btn_->setObjectName("govTreasTab");
    auctions_btn_->setCheckable(true);
    auctions_btn_->setCursor(Qt::PointingHandCursor);
    connect(auctions_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Auctions); });

    summary_btn_ = new QPushButton;
    summary_btn_->setObjectName("govTreasTab");
    summary_btn_->setCheckable(true);
    summary_btn_->setCursor(Qt::PointingHandCursor);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Summary); });

    hl->addWidget(prices_btn_);
    hl->addWidget(auctions_btn_);
    hl->addWidget(summary_btn_);
    hl->addStretch(1);

    fetch_btn_ = new QPushButton;
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    export_btn_ = new QPushButton;
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

QWidget* GovDataTreasuryPanel::build_filter_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govTreasuryFilterBar");
    bar->setFixedHeight(34);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);

    // Filter labels (text set in retranslateUi)
    filter_from_lbl_ = new QLabel;
    filter_from_lbl_->setObjectName("govTreasuryFilterLabel");
    hl->addWidget(filter_from_lbl_);

    start_date_ = new QDateEdit(QDate::currentDate().addMonths(-1));
    start_date_->setCalendarPopup(true);
    start_date_->setDisplayFormat("yyyy-MM-dd");
    start_date_->setFixedHeight(24);
    hl->addWidget(start_date_);

    filter_to_lbl_ = new QLabel;
    filter_to_lbl_->setObjectName("govTreasuryFilterLabel");
    hl->addWidget(filter_to_lbl_);

    QDate end = QDate::currentDate().addDays(-1);
    if (end.dayOfWeek() == 6)
        end = end.addDays(-1);
    if (end.dayOfWeek() == 7)
        end = end.addDays(-2);
    end_date_ = new QDateEdit(end);
    end_date_->setCalendarPopup(true);
    end_date_->setDisplayFormat("yyyy-MM-dd");
    end_date_->setFixedHeight(24);
    hl->addWidget(end_date_);

    hl->addSpacing(16);
    filter_type_lbl_ = new QLabel;
    filter_type_lbl_->setObjectName("govTreasuryFilterLabel");
    hl->addWidget(filter_type_lbl_);

    // ComboBox items: store the API key in userData; display text is set
    // (and re-set on language change) by retranslateUi.
    security_type_ = new QComboBox;
    security_type_->addItem(QString(), "all");
    security_type_->addItem(QString(), "bill");
    security_type_->addItem(QString(), "note");
    security_type_->addItem(QString(), "bond");
    security_type_->addItem(QString(), "tips");
    security_type_->addItem(QString(), "frn");
    security_type_->setFixedHeight(24);
    hl->addWidget(security_type_);

    hl->addStretch(1);

    return bar;
}

// ── Actions ──────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::load_initial_data() {
    show_loading(tr("Select PRICES, AUCTIONS or SUMMARY then click FETCH"));
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataTreasuryPanel::retranslateUi() {
    if (prices_btn_)   prices_btn_->setText(tr("PRICES"));
    if (auctions_btn_) auctions_btn_->setText(tr("AUCTIONS"));
    if (summary_btn_)  summary_btn_->setText(tr("SUMMARY"));
    if (fetch_btn_)    fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)   export_btn_->setText(tr("CSV"));

    if (filter_from_lbl_) filter_from_lbl_->setText(tr("FROM"));
    if (filter_to_lbl_)   filter_to_lbl_->setText(tr("TO"));
    if (filter_type_lbl_) filter_type_lbl_->setText(tr("TYPE"));

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
        if (idx >= 0) security_type_->setCurrentIndex(idx);
    }

    if (prices_table_)
        prices_table_->setHorizontalHeaderLabels(
            {tr("CUSIP"), tr("TYPE"), tr("RATE %"), tr("MATURITY"),
             tr("BID"), tr("OFFER"), tr("EOD PRICE")});
    if (auctions_table_)
        auctions_table_->setHorizontalHeaderLabels(
            {tr("CUSIP"), tr("TYPE"), tr("TERM"), tr("AUCTION DATE"),
             tr("HIGH RATE"), tr("HIGH PRICE"), tr("BID/COVER"), tr("OFFERING")});
    if (type_breakdown_table_)
        type_breakdown_table_->setHorizontalHeaderLabels({tr("TYPE"), tr("COUNT")});

    if (card_total_lbl_)     card_total_lbl_->setText(tr("TOTAL SECURITIES"));
    if (card_min_rate_lbl_)  card_min_rate_lbl_->setText(tr("MIN RATE"));
    if (card_max_rate_lbl_)  card_max_rate_lbl_->setText(tr("MAX RATE"));
    if (card_avg_rate_lbl_)  card_avg_rate_lbl_->setText(tr("AVG RATE"));
    if (card_min_price_lbl_) card_min_price_lbl_->setText(tr("MIN PRICE"));
    if (card_max_price_lbl_) card_max_price_lbl_->setText(tr("MAX PRICE"));
    if (card_avg_price_lbl_) card_avg_price_lbl_->setText(tr("AVG PRICE"));

    const QString yield_sub = tr("yield %");
    const QString price_sub = tr("per $100");
    if (card_min_rate_sub_)  card_min_rate_sub_->setText(yield_sub);
    if (card_max_rate_sub_)  card_max_rate_sub_->setText(yield_sub);
    if (card_avg_rate_sub_)  card_avg_rate_sub_->setText(yield_sub);
    if (card_min_price_sub_) card_min_price_sub_->setText(price_sub);
    if (card_max_price_sub_) card_max_price_sub_->setText(price_sub);
    if (card_avg_price_sub_) card_avg_price_sub_->setText(price_sub);

    if (breakdown_hdr_lbl_) breakdown_hdr_lbl_->setText(tr("SECURITY TYPE BREAKDOWN"));
}

void GovDataTreasuryPanel::on_endpoint_changed(int index) {
    active_endpoint_ = static_cast<Endpoint>(index);
    prices_btn_->setChecked(index == Prices);
    auctions_btn_->setChecked(index == Auctions);
    summary_btn_->setChecked(index == Summary);
}

void GovDataTreasuryPanel::on_fetch() {
    QString date_str = end_date_->date().toString("yyyy-MM-dd");
    QString start_str = start_date_->date().toString("yyyy-MM-dd");
    QString sec_type = security_type_->currentData().toString();

    QStringList args;
    QString command;

    switch (active_endpoint_) {
        case Prices:
            command = "treasury_prices";
            args << date_str;
            if (sec_type != "all") {
                args << "";
                args << sec_type;
            }
            break;
        case Auctions:
            command = "treasury_auctions";
            args << start_str << date_str << (sec_type != "all" ? sec_type : "") << "50" << "1";
            break;
        case Summary:
            command = "summary";
            args << date_str;
            break;
    }

    show_loading(tr("Loading US Treasury data…"));
    services::GovDataService::instance().execute(kGovDataTreasuryScript, command, args, "gov_treasury_" + command);
}

void GovDataTreasuryPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    if (!request_id.startsWith("gov_treasury_"))
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    if (request_id.contains("treasury_prices")) {
        populate_prices(result.data);
        content_stack_->setCurrentIndex(Prices);
    } else if (request_id.contains("treasury_auctions")) {
        populate_auctions(result.data);
        content_stack_->setCurrentIndex(Auctions);
    } else if (request_id.contains("summary")) {
        populate_summary(result.data);
        content_stack_->setCurrentIndex(Summary);
    }
}

// ── Populate ─────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::populate_prices(const QJsonObject& json) {
    const QJsonArray records = json["json"].toArray();
    prices_table_->setRowCount(0);
    prices_table_->setRowCount(records.size());

    auto fmt = [](const QJsonValue& v) -> QString {
        if (v.isNull() || v.isUndefined())
            return "—";
        return QString::number(v.toDouble(), 'f', 3);
    };

    for (int i = 0; i < records.size(); ++i) {
        const auto r = records[i].toObject();
        prices_table_->setItem(i, 0, new QTableWidgetItem(r["cusip"].toString()));

        auto* type_item = new QTableWidgetItem(r["security_type"].toString());
        type_item->setForeground(QColor(kGovDataTreasuryColor));
        prices_table_->setItem(i, 1, type_item);

        prices_table_->setItem(i, 2, new QTableWidgetItem(fmt(r["rate"])));
        QString mat = r["maturity_date"].toString();
        prices_table_->setItem(i, 3, new QTableWidgetItem(mat.isEmpty() ? "—" : mat));

        for (int c : {4, 5, 6}) {
            auto* it = new QTableWidgetItem(fmt(r[c == 4 ? "bid" : c == 5 ? "offer" : "eod_price"]));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            prices_table_->setItem(i, c, it);
        }
    }
    LOG_INFO("GovTreasury", QString("Loaded %1 price records").arg(records.size()));
}

void GovDataTreasuryPanel::populate_auctions(const QJsonObject& json) {
    const QJsonArray records = json["json"].toArray();
    auctions_table_->setRowCount(0);
    auctions_table_->setRowCount(records.size());

    auto fmt = [](const QJsonValue& v) -> QString {
        if (v.isNull() || v.isUndefined())
            return "—";
        return QString::number(v.toDouble(), 'f', 3);
    };

    for (int i = 0; i < records.size(); ++i) {
        const auto r = records[i].toObject();
        auctions_table_->setItem(i, 0, new QTableWidgetItem(r["cusip"].toString()));

        auto* type_item = new QTableWidgetItem(r["securityType"].toString());
        type_item->setForeground(QColor(kGovDataTreasuryColor));
        auctions_table_->setItem(i, 1, type_item);

        auctions_table_->setItem(i, 2, new QTableWidgetItem(r["securityTerm"].toString()));
        QString adate = r["auctionDate"].toString();
        auctions_table_->setItem(i, 3, new QTableWidgetItem(adate.isEmpty() ? "—" : adate));

        for (int c : {4, 5, 6}) {
            auto key = c == 4 ? "highDiscountRate" : c == 5 ? "highPrice" : "bidToCoverRatio";
            auto* it = new QTableWidgetItem(fmt(r[key]));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auctions_table_->setItem(i, c, it);
        }

        double offering = r["offeringAmount"].toDouble(0);
        QString os = offering > 0 ? "$" + QString::number(static_cast<qlonglong>(offering / 1e9), 'f', 1) + "B" : "—";
        auctions_table_->setItem(i, 7, new QTableWidgetItem(os));
    }
    LOG_INFO("GovTreasury", QString("Loaded %1 auction records").arg(records.size()));
}

void GovDataTreasuryPanel::populate_summary(const QJsonObject& json) {
    total_securities_label_->setText(QString::number(json["total_securities"].toInt()));

    auto yield = json["yield_summary"].toObject();
    min_rate_label_->setText(QString::number(yield["min_rate"].toDouble(), 'f', 3) + "%");
    max_rate_label_->setText(QString::number(yield["max_rate"].toDouble(), 'f', 3) + "%");
    avg_rate_label_->setText(QString::number(yield["avg_rate"].toDouble(), 'f', 3) + "%");

    auto price = json["price_summary"].toObject();
    min_price_label_->setText(QString::number(price["min_price"].toDouble(), 'f', 2));
    max_price_label_->setText(QString::number(price["max_price"].toDouble(), 'f', 2));
    avg_price_label_->setText(QString::number(price["avg_price"].toDouble(), 'f', 2));

    auto types = json["security_types"].toObject();
    type_breakdown_table_->setRowCount(0);
    type_breakdown_table_->setRowCount(types.size());
    int row = 0;
    for (auto it = types.begin(); it != types.end(); ++it, ++row) {
        auto* name_item = new QTableWidgetItem(it.key());
        name_item->setForeground(QColor(kGovDataTreasuryColor));
        type_breakdown_table_->setItem(row, 0, name_item);
        type_breakdown_table_->setItem(row, 1, new QTableWidgetItem(QString::number(it.value().toInt())));
    }
    LOG_INFO("GovTreasury", "Summary loaded");
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataTreasuryColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

void GovDataTreasuryPanel::show_error(const QString& message) {
    status_label_->setStyleSheet(QString("color:%1; font-size:12px; background:transparent;").arg(colors::NEGATIVE()));
    status_label_->setText(tr("Error: %1").arg(message));
    content_stack_->setCurrentIndex(3);
}

void GovDataTreasuryPanel::on_export_csv() {
    QTableWidget* table = nullptr;
    if (active_endpoint_ == Prices)
        table = prices_table_;
    else if (active_endpoint_ == Auctions)
        table = auctions_table_;
    else
        table = type_breakdown_table_;
    export_table_to_csv(table, "treasury_data.csv", this);
}

} // namespace fincept::screens
