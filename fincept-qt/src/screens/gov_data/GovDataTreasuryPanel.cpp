// src/screens/gov_data/GovDataTreasuryPanel.cpp
#include "screens/gov_data/GovDataTreasuryPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const QString kColor  = "#3B82F6";
static const QString kScript = "government_us_data.py";

// ── Stylesheet ───────────────────────────────────────────────────────────────

static const char* kStyle =
    "#govTreasuryToolbar { background:#111111; border-bottom:1px solid #1a1a1a; }"

    "#govTreasTab { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 12px; letter-spacing:0.5px; }"
    "#govTreasTab:hover { color:#e5e5e5; background:#161616; }"
    "#govTreasTab:checked { background:rgba(59,130,246,0.12); color:#3B82F6;"
    "  border:1px solid #3B82F6; }"

    "#govFetchBtn { background:#3B82F6; color:#080808; border:none;"
    "  font-size:10px; font-weight:700; padding:4px 14px; }"
    "#govFetchBtn:hover { background:#60a5fa; }"
    "#govFetchBtn:disabled { background:#1a1a1a; color:#404040; }"
    "#govCsvBtn { background:transparent; color:#808080; border:1px solid #1a1a1a;"
    "  font-size:10px; font-weight:700; padding:4px 10px; }"
    "#govCsvBtn:hover { color:#e5e5e5; background:#161616; }"

    "QDateEdit { background:#080808; color:#e5e5e5; border:1px solid #1a1a1a;"
    "  font-size:11px; padding:2px 6px; }"
    "QDateEdit::drop-down { border:none; }"
    "QComboBox { background:#080808; color:#e5e5e5; border:1px solid #1a1a1a;"
    "  font-size:11px; padding:2px 6px; }"
    "QComboBox::drop-down { border:none; }"
    "QComboBox QAbstractItemView { background:#111111; color:#e5e5e5;"
    "  border:1px solid #1a1a1a; }"

    "QTableWidget { background:#080808; color:#e5e5e5; border:none;"
    "  gridline-color:#1a1a1a; font-size:11px; alternate-background-color:#0a0a0a; }"
    "QTableWidget::item { padding:5px 8px; border-bottom:1px solid #1a1a1a; }"
    "QTableWidget::item:selected { background:rgba(59,130,246,0.1); color:#3B82F6; }"
    "QHeaderView::section { background:#111111; color:#808080; border:none;"
    "  border-bottom:2px solid #1a1a1a; border-right:1px solid #1a1a1a;"
    "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"

    // Summary cards
    "#govSummaryPage { background:#080808; }"
    "#govSumCard { background:#0a0a0a; border:1px solid #1a1a1a; }"
    "#govSumCard:hover { border-color:#333333; }"
    "#govSumLabel { color:#808080; font-size:8px; font-weight:700;"
    "  letter-spacing:1px; background:transparent; }"
    "#govSumValue { color:#3B82F6; font-size:16px; font-weight:700; background:transparent; }"
    "#govSumSub   { color:#808080; font-size:9px; background:transparent; }"

    // Status page
    "#govStatusPage { background:#080808; }"
    "#govStatusMsg { color:#808080; font-size:13px; background:transparent; }"
    "#govStatusErr { color:#dc2626; font-size:12px; background:transparent; }"

    "QScrollBar:vertical { background:#080808; width:5px; }"
    "QScrollBar::handle:vertical { background:#1a1a1a; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataTreasuryPanel::GovDataTreasuryPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(kStyle);
    build_ui();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready,
            this, &GovDataTreasuryPanel::on_result);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    content_stack_ = new QStackedWidget;

    // ── Page 0: Prices table ──
    prices_table_ = new QTableWidget;
    prices_table_->setColumnCount(7);
    prices_table_->setHorizontalHeaderLabels(
        {"CUSIP", "TYPE", "RATE %", "MATURITY", "BID", "OFFER", "EOD PRICE"});
    prices_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    prices_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    prices_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    prices_table_->verticalHeader()->setVisible(false);
    prices_table_->setAlternatingRowColors(true);
    prices_table_->setShowGrid(true);
    content_stack_->addWidget(prices_table_);

    // ── Page 1: Auctions table ──
    auctions_table_ = new QTableWidget;
    auctions_table_->setColumnCount(8);
    auctions_table_->setHorizontalHeaderLabels(
        {"CUSIP", "TYPE", "TERM", "AUCTION DATE", "HIGH RATE", "HIGH PRICE", "BID/COVER", "OFFERING"});
    auctions_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auctions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    auctions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auctions_table_->verticalHeader()->setVisible(false);
    auctions_table_->setAlternatingRowColors(true);
    auctions_table_->setShowGrid(true);
    content_stack_->addWidget(auctions_table_);

    // ── Page 2: Summary ──
    auto* summary_page = new QWidget;
    summary_page->setObjectName("govSummaryPage");
    auto* svl = new QVBoxLayout(summary_page);
    svl->setContentsMargins(16, 16, 16, 16);
    svl->setSpacing(12);

    // Stat cards row
    auto* cards_row = new QWidget;
    auto* crhl = new QHBoxLayout(cards_row);
    crhl->setContentsMargins(0, 0, 0, 0);
    crhl->setSpacing(8);

    auto make_card = [&](const QString& label, QLabel*& val_out, const QString& sub = "") {
        auto* card = new QWidget;
        card->setObjectName("govSumCard");
        card->setMinimumWidth(110);
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(12, 8, 12, 8);
        cvl->setSpacing(2);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("govSumLabel");
        val_out = new QLabel("—");
        val_out->setObjectName("govSumValue");
        cvl->addWidget(lbl);
        cvl->addWidget(val_out);
        if (!sub.isEmpty()) {
            auto* sl = new QLabel(sub);
            sl->setObjectName("govSumSub");
            cvl->addWidget(sl);
        }
        crhl->addWidget(card);
    };

    make_card("TOTAL SECURITIES",  total_securities_label_);
    make_card("MIN RATE",          min_rate_label_,   "yield %");
    make_card("MAX RATE",          max_rate_label_,   "yield %");
    make_card("AVG RATE",          avg_rate_label_,   "yield %");
    make_card("MIN PRICE",         min_price_label_,  "per $100");
    make_card("MAX PRICE",         max_price_label_,  "per $100");
    make_card("AVG PRICE",         avg_price_label_,  "per $100");
    crhl->addStretch(1);
    svl->addWidget(cards_row);

    // Breakdown table
    auto* breakdown_hdr = new QLabel("SECURITY TYPE BREAKDOWN");
    breakdown_hdr->setStyleSheet(
        "color:#3B82F6; font-size:9px; font-weight:700; letter-spacing:1px;"
        " background:transparent; margin-top:8px;");
    svl->addWidget(breakdown_hdr);

    type_breakdown_table_ = new QTableWidget;
    type_breakdown_table_->setColumnCount(2);
    type_breakdown_table_->setHorizontalHeaderLabels({"TYPE", "COUNT"});
    type_breakdown_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    type_breakdown_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    type_breakdown_table_->verticalHeader()->setVisible(false);
    type_breakdown_table_->setAlternatingRowColors(true);
    type_breakdown_table_->setMaximumHeight(200);
    svl->addWidget(type_breakdown_table_);
    svl->addStretch(1);
    content_stack_->addWidget(summary_page);

    // ── Page 3: Status ──
    auto* status_page = new QWidget;
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
    auto* bar = new QWidget;
    bar->setObjectName("govTreasuryToolbar");
    bar->setFixedHeight(40);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Endpoint tabs
    prices_btn_ = new QPushButton("PRICES");
    prices_btn_->setObjectName("govTreasTab");
    prices_btn_->setCheckable(true);
    prices_btn_->setChecked(true);
    prices_btn_->setCursor(Qt::PointingHandCursor);
    connect(prices_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Prices); });

    auctions_btn_ = new QPushButton("AUCTIONS");
    auctions_btn_->setObjectName("govTreasTab");
    auctions_btn_->setCheckable(true);
    auctions_btn_->setCursor(Qt::PointingHandCursor);
    connect(auctions_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Auctions); });

    summary_btn_ = new QPushButton("SUMMARY");
    summary_btn_->setObjectName("govTreasTab");
    summary_btn_->setCheckable(true);
    summary_btn_->setCursor(Qt::PointingHandCursor);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Summary); });

    hl->addWidget(prices_btn_);
    hl->addWidget(auctions_btn_);
    hl->addWidget(summary_btn_);
    hl->addSpacing(14);

    // Date labels
    auto lbl_style = "color:#525252; font-size:9px; font-weight:700; background:transparent;";
    auto* from_lbl = new QLabel("FROM");
    from_lbl->setStyleSheet(lbl_style);
    hl->addWidget(from_lbl);

    start_date_ = new QDateEdit(QDate::currentDate().addMonths(-1));
    start_date_->setCalendarPopup(true);
    start_date_->setDisplayFormat("yyyy-MM-dd");
    start_date_->setFixedHeight(26);
    hl->addWidget(start_date_);

    auto* to_lbl = new QLabel("TO");
    to_lbl->setStyleSheet(lbl_style);
    hl->addWidget(to_lbl);

    QDate end = QDate::currentDate().addDays(-1);
    if (end.dayOfWeek() == 6) end = end.addDays(-1);
    if (end.dayOfWeek() == 7) end = end.addDays(-2);
    end_date_ = new QDateEdit(end);
    end_date_->setCalendarPopup(true);
    end_date_->setDisplayFormat("yyyy-MM-dd");
    end_date_->setFixedHeight(26);
    hl->addWidget(end_date_);

    hl->addSpacing(10);

    // Security type
    auto* type_lbl = new QLabel("TYPE");
    type_lbl->setStyleSheet(lbl_style);
    hl->addWidget(type_lbl);

    security_type_ = new QComboBox;
    security_type_->addItem("All",   "all");
    security_type_->addItem("Bills", "bill");
    security_type_->addItem("Notes", "note");
    security_type_->addItem("Bonds", "bond");
    security_type_->addItem("TIPS",  "tips");
    security_type_->addItem("FRN",   "frn");
    security_type_->setFixedHeight(26);
    hl->addWidget(security_type_);

    hl->addStretch(1);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Actions ──────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::load_initial_data() {
    show_loading("Select PRICES, AUCTIONS or SUMMARY then click FETCH");
}

void GovDataTreasuryPanel::on_endpoint_changed(int index) {
    active_endpoint_ = static_cast<Endpoint>(index);
    prices_btn_->setChecked(index == Prices);
    auctions_btn_->setChecked(index == Auctions);
    summary_btn_->setChecked(index == Summary);
}

void GovDataTreasuryPanel::on_fetch() {
    QString date_str  = end_date_->date().toString("yyyy-MM-dd");
    QString start_str = start_date_->date().toString("yyyy-MM-dd");
    QString sec_type  = security_type_->currentData().toString();

    QStringList args;
    QString     command;

    switch (active_endpoint_) {
        case Prices:
            command = "treasury_prices";
            args << date_str;
            if (sec_type != "all") { args << ""; args << sec_type; }
            break;
        case Auctions:
            command = "treasury_auctions";
            args << start_str << date_str
                 << (sec_type != "all" ? sec_type : "")
                 << "50" << "1";
            break;
        case Summary:
            command = "summary";
            args << date_str;
            break;
    }

    show_loading("Loading US Treasury data…");
    services::GovDataService::instance().execute(
        kScript, command, args, "gov_treasury_" + command);
}

void GovDataTreasuryPanel::on_result(const QString& request_id,
                                     const services::GovDataResult& result) {
    if (!request_id.startsWith("gov_treasury_")) return;
    if (!result.success) { show_error(result.error); return; }

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

void GovDataTreasuryPanel::populate_prices(const QJsonObject& data) {
    const QJsonArray records = data["data"].toArray();
    prices_table_->setRowCount(0);
    prices_table_->setRowCount(records.size());

    auto fmt = [](const QJsonValue& v) -> QString {
        if (v.isNull() || v.isUndefined()) return "—";
        return QString::number(v.toDouble(), 'f', 3);
    };

    for (int i = 0; i < records.size(); ++i) {
        const auto r = records[i].toObject();
        prices_table_->setItem(i, 0, new QTableWidgetItem(r["cusip"].toString()));

        auto* type_item = new QTableWidgetItem(r["security_type"].toString());
        type_item->setForeground(QColor(kColor));
        prices_table_->setItem(i, 1, type_item);

        prices_table_->setItem(i, 2, new QTableWidgetItem(fmt(r["rate"])));
        prices_table_->setItem(i, 3, new QTableWidgetItem(r["maturity_date"].toString("—")));

        for (int c : {4, 5, 6}) {
            auto* it = new QTableWidgetItem(fmt(r[c == 4 ? "bid" : c == 5 ? "offer" : "eod_price"]));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            prices_table_->setItem(i, c, it);
        }
    }
    LOG_INFO("GovTreasury", QString("Loaded %1 price records").arg(records.size()));
}

void GovDataTreasuryPanel::populate_auctions(const QJsonObject& data) {
    const QJsonArray records = data["data"].toArray();
    auctions_table_->setRowCount(0);
    auctions_table_->setRowCount(records.size());

    auto fmt = [](const QJsonValue& v) -> QString {
        if (v.isNull() || v.isUndefined()) return "—";
        return QString::number(v.toDouble(), 'f', 3);
    };

    for (int i = 0; i < records.size(); ++i) {
        const auto r = records[i].toObject();
        auctions_table_->setItem(i, 0, new QTableWidgetItem(r["cusip"].toString()));

        auto* type_item = new QTableWidgetItem(r["securityType"].toString());
        type_item->setForeground(QColor(kColor));
        auctions_table_->setItem(i, 1, type_item);

        auctions_table_->setItem(i, 2, new QTableWidgetItem(r["securityTerm"].toString()));
        auctions_table_->setItem(i, 3, new QTableWidgetItem(r["auctionDate"].toString("—")));

        for (int c : {4, 5, 6}) {
            auto key = c == 4 ? "highDiscountRate" : c == 5 ? "highPrice" : "bidToCoverRatio";
            auto* it = new QTableWidgetItem(fmt(r[key]));
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auctions_table_->setItem(i, c, it);
        }

        double offering = r["offeringAmount"].toDouble(0);
        QString os = offering > 0
            ? "$" + QString::number(static_cast<qlonglong>(offering / 1e9), 'f', 1) + "B"
            : "—";
        auctions_table_->setItem(i, 7, new QTableWidgetItem(os));
    }
    LOG_INFO("GovTreasury", QString("Loaded %1 auction records").arg(records.size()));
}

void GovDataTreasuryPanel::populate_summary(const QJsonObject& data) {
    total_securities_label_->setText(QString::number(data["total_securities"].toInt()));

    auto yield = data["yield_summary"].toObject();
    min_rate_label_->setText(QString::number(yield["min_rate"].toDouble(), 'f', 3) + "%");
    max_rate_label_->setText(QString::number(yield["max_rate"].toDouble(), 'f', 3) + "%");
    avg_rate_label_->setText(QString::number(yield["avg_rate"].toDouble(), 'f', 3) + "%");

    auto price = data["price_summary"].toObject();
    min_price_label_->setText(QString::number(price["min_price"].toDouble(), 'f', 2));
    max_price_label_->setText(QString::number(price["max_price"].toDouble(), 'f', 2));
    avg_price_label_->setText(QString::number(price["avg_price"].toDouble(), 'f', 2));

    auto types = data["security_types"].toObject();
    type_breakdown_table_->setRowCount(0);
    type_breakdown_table_->setRowCount(types.size());
    int row = 0;
    for (auto it = types.begin(); it != types.end(); ++it, ++row) {
        auto* name_item = new QTableWidgetItem(it.key());
        name_item->setForeground(QColor(kColor));
        type_breakdown_table_->setItem(row, 0, name_item);
        type_breakdown_table_->setItem(row, 1, new QTableWidgetItem(
            QString::number(it.value().toInt())));
    }
    LOG_INFO("GovTreasury", "Summary loaded");
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(kColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

void GovDataTreasuryPanel::show_error(const QString& message) {
    status_label_->setStyleSheet("color:#dc2626; font-size:12px; background:transparent;");
    status_label_->setText("Error: " + message);
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
    if (!table || table->rowCount() == 0) return;

    QString path = QFileDialog::getSaveFileName(
        this, "Export CSV", "treasury_data.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    QStringList headers;
    for (int c = 0; c < table->columnCount(); ++c) {
        auto* h = table->horizontalHeaderItem(c);
        headers << (h ? h->text() : "");
    }
    out << headers.join(",") << "\n";

    for (int r = 0; r < table->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < table->columnCount(); ++c) {
            auto* item = table->item(r, c);
            QString val = item ? item->text() : "";
            if (val.contains(',') || val.contains('"'))
                val = "\"" + val.replace("\"", "\"\"") + "\"";
            row << val;
        }
        out << row.join(",") << "\n";
    }
}

} // namespace fincept::screens
