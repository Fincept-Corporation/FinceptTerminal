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

static const QString kColor = "#3B82F6";
static const QString kScript = "government_us_data.py";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

GovDataTreasuryPanel::GovDataTreasuryPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataTreasuryPanel::on_result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    content_stack_ = new QStackedWidget(this);

    // Table style shared
    const QString table_style =
        QString("QTableWidget { background:%1; color:%2; border:none; gridline-color:%3;"
                "  font-family:%4; font-size:12px; }"
                "QTableWidget::item { padding:6px 8px; border-bottom:1px solid %3; }"
                "QTableWidget::item:selected { background:%5; }"
                "QTableWidget::item:alternate { background:%6; }"
                "QHeaderView::section { background:%7; color:%8; border:none; border-bottom:1px solid %3;"
                "  font-family:%4; font-size:10px; font-weight:700; padding:6px 8px; }")
            .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM, fonts::DATA_FAMILY, colors::BG_HOVER,
                 colors::ROW_ALT, colors::BG_RAISED, kColor);

    // Page 0: Prices table
    prices_table_ = new QTableWidget(this);
    prices_table_->setColumnCount(7);
    prices_table_->setHorizontalHeaderLabels({"CUSIP", "TYPE", "RATE", "MATURITY", "BID", "OFFER", "EOD PRICE"});
    prices_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    prices_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    prices_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    prices_table_->verticalHeader()->setVisible(false);
    prices_table_->setAlternatingRowColors(true);
    prices_table_->setStyleSheet(table_style);
    content_stack_->addWidget(prices_table_);

    // Page 1: Auctions table
    auctions_table_ = new QTableWidget(this);
    auctions_table_->setColumnCount(8);
    auctions_table_->setHorizontalHeaderLabels(
        {"CUSIP", "TYPE", "TERM", "AUCTION DATE", "HIGH RATE", "HIGH PRICE", "BID/COVER", "OFFERING"});
    auctions_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auctions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    auctions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auctions_table_->verticalHeader()->setVisible(false);
    auctions_table_->setAlternatingRowColors(true);
    auctions_table_->setStyleSheet(table_style);
    content_stack_->addWidget(auctions_table_);

    // Page 2: Summary
    summary_widget_ = new QWidget(this);
    auto* sum_layout = new QVBoxLayout(summary_widget_);
    sum_layout->setContentsMargins(16, 16, 16, 16);
    sum_layout->setSpacing(12);

    const QString card_style =
        QString("QLabel { color:%1; font-family:%2; font-size:12px; }").arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY);

    const QString header_style = QString("QLabel { color:%1; font-family:%2; font-size:10px; font-weight:700; }")
                                     .arg(kColor, fonts::DATA_FAMILY);

    const QString value_style = QString("QLabel { color:%1; font-family:%2; font-size:16px; font-weight:700; }")
                                    .arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY);

    // Stats grid
    auto* stats_grid = new QGridLayout();
    stats_grid->setSpacing(16);

    auto add_stat = [&](int row, int col, const QString& label, QLabel*& value_lbl) {
        auto* lbl = new QLabel(label, summary_widget_);
        lbl->setStyleSheet(header_style);
        value_lbl = new QLabel("-", summary_widget_);
        value_lbl->setStyleSheet(value_style);
        stats_grid->addWidget(lbl, row * 2, col);
        stats_grid->addWidget(value_lbl, row * 2 + 1, col);
    };

    add_stat(0, 0, "TOTAL SECURITIES", total_securities_label_);
    add_stat(0, 1, "MIN RATE", min_rate_label_);
    add_stat(0, 2, "MAX RATE", max_rate_label_);
    add_stat(0, 3, "AVG RATE", avg_rate_label_);
    add_stat(1, 0, "MIN PRICE", min_price_label_);
    add_stat(1, 1, "MAX PRICE", max_price_label_);
    add_stat(1, 2, "AVG PRICE", avg_price_label_);

    sum_layout->addLayout(stats_grid);

    // Type breakdown table
    auto* type_label = new QLabel("SECURITY TYPE BREAKDOWN", summary_widget_);
    type_label->setStyleSheet(header_style);
    sum_layout->addWidget(type_label);

    type_breakdown_table_ = new QTableWidget(summary_widget_);
    type_breakdown_table_->setColumnCount(2);
    type_breakdown_table_->setHorizontalHeaderLabels({"TYPE", "COUNT"});
    type_breakdown_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    type_breakdown_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    type_breakdown_table_->verticalHeader()->setVisible(false);
    type_breakdown_table_->setStyleSheet(table_style);
    sum_layout->addWidget(type_breakdown_table_);

    summary_widget_->setStyleSheet(QString("QWidget { background:%1; }").arg(colors::BG_BASE));
    content_stack_->addWidget(summary_widget_);

    // Page 3: Status label
    status_label_ = new QLabel(this);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("QLabel { color:%1; font-family:%2; font-size:12px; }")
                                     .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    content_stack_->addWidget(status_label_);

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataTreasuryPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(38);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED, colors::BORDER_MED));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(4);

    const QString btn_style = QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                      " font-family:%3; font-size:10px; font-weight:700; padding:4px 10px; }"
                                      "QPushButton:hover { background:%4; }"
                                      "QPushButton:checked { background:%5; color:%6; border-color:%5; }")
                                  .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM, fonts::DATA_FAMILY, colors::BG_HOVER,
                                       kColor, colors::TEXT_PRIMARY);

    prices_btn_ = new QPushButton("PRICES", bar);
    prices_btn_->setCheckable(true);
    prices_btn_->setChecked(true);
    prices_btn_->setStyleSheet(btn_style);
    connect(prices_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Prices); });

    auctions_btn_ = new QPushButton("AUCTIONS", bar);
    auctions_btn_->setCheckable(true);
    auctions_btn_->setStyleSheet(btn_style);
    connect(auctions_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Auctions); });

    summary_btn_ = new QPushButton("SUMMARY", bar);
    summary_btn_->setCheckable(true);
    summary_btn_->setStyleSheet(btn_style);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { on_endpoint_changed(Summary); });

    hl->addWidget(prices_btn_);
    hl->addWidget(auctions_btn_);
    hl->addWidget(summary_btn_);
    hl->addSpacing(12);

    // Date range
    const QString date_style = QString("QDateEdit { background:%1; color:%2; border:1px solid %3;"
                                       " font-family:%4; font-size:11px; padding:2px 6px; }"
                                       "QDateEdit::drop-down { border:none; }")
                                   .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM, fonts::DATA_FAMILY);

    auto* from_label = new QLabel("FROM:", bar);
    from_label->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px; font-weight:700;")
                                  .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));

    start_date_ = new QDateEdit(QDate::currentDate().addMonths(-1), bar);
    start_date_->setCalendarPopup(true);
    start_date_->setDisplayFormat("yyyy-MM-dd");
    start_date_->setStyleSheet(date_style);

    auto* to_label = new QLabel("TO:", bar);
    to_label->setStyleSheet(from_label->styleSheet());

    // Default to previous business day
    QDate end = QDate::currentDate().addDays(-1);
    if (end.dayOfWeek() == 6)
        end = end.addDays(-1);
    if (end.dayOfWeek() == 7)
        end = end.addDays(-2);
    end_date_ = new QDateEdit(end, bar);
    end_date_->setCalendarPopup(true);
    end_date_->setDisplayFormat("yyyy-MM-dd");
    end_date_->setStyleSheet(date_style);

    hl->addWidget(from_label);
    hl->addWidget(start_date_);
    hl->addWidget(to_label);
    hl->addWidget(end_date_);
    hl->addSpacing(8);

    // Security type filter
    auto* type_label = new QLabel("TYPE:", bar);
    type_label->setStyleSheet(from_label->styleSheet());

    security_type_ = new QComboBox(bar);
    security_type_->addItem("All", "all");
    security_type_->addItem("Bills", "bill");
    security_type_->addItem("Notes", "note");
    security_type_->addItem("Bonds", "bond");
    security_type_->addItem("TIPS", "tips");
    security_type_->addItem("FRN", "frn");
    security_type_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                " font-family:%4; font-size:11px; padding:2px 6px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; }")
            .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM, fonts::DATA_FAMILY));

    hl->addWidget(type_label);
    hl->addWidget(security_type_);

    hl->addStretch();

    // Fetch
    fetch_btn_ = new QPushButton("FETCH", bar);
    fetch_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      " font-family:%3; font-size:10px; font-weight:700; padding:4px 12px; }"
                                      "QPushButton:hover { opacity:0.8; }")
                                  .arg(kColor, colors::TEXT_PRIMARY, fonts::DATA_FAMILY));
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    // Export
    export_btn_ = new QPushButton("CSV", bar);
    export_btn_->setStyleSheet(btn_style);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataTreasuryPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::load_initial_data() {
    // Show empty state, user clicks FETCH
    show_loading("Select an endpoint and click FETCH to load US Treasury data");
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
                args << ""; // cusip placeholder
                args << sec_type;
            }
            break;
        case Auctions:
            command = "treasury_auctions";
            args << start_str << date_str;
            args << (sec_type != "all" ? sec_type : "");
            args << "50" << "1"; // page_size, page_num
            break;
        case Summary:
            command = "summary";
            args << date_str;
            break;
    }

    show_loading("Loading US Treasury data...");
    services::GovDataService::instance().execute(kScript, command, args, "gov_treasury_" + command);
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

// ─────────────────────────────────────────────────────────────────────────────
// Populate
// ─────────────────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::populate_prices(const QJsonObject& data) {
    const QJsonArray records = data["data"].toArray();
    prices_table_->setRowCount(0);
    prices_table_->setRowCount(records.size());

    for (int i = 0; i < records.size(); ++i) {
        const auto r = records[i].toObject();
        prices_table_->setItem(i, 0, new QTableWidgetItem(r["cusip"].toString()));

        auto* type_item = new QTableWidgetItem(r["security_type"].toString());
        type_item->setForeground(QColor(kColor));
        prices_table_->setItem(i, 1, type_item);

        auto fmt_num = [](const QJsonValue& v) -> QString {
            if (v.isNull() || v.isUndefined())
                return "-";
            return QString::number(v.toDouble(), 'f', 3);
        };

        prices_table_->setItem(i, 2, new QTableWidgetItem(fmt_num(r["rate"])));
        prices_table_->setItem(i, 3, new QTableWidgetItem(r["maturity_date"].toString("-")));
        prices_table_->setItem(i, 4, new QTableWidgetItem(fmt_num(r["bid"])));
        prices_table_->setItem(i, 5, new QTableWidgetItem(fmt_num(r["offer"])));
        prices_table_->setItem(i, 6, new QTableWidgetItem(fmt_num(r["eod_price"])));
    }

    LOG_INFO("GovTreasury", QString("Loaded %1 price records").arg(records.size()));
}

void GovDataTreasuryPanel::populate_auctions(const QJsonObject& data) {
    const QJsonArray records = data["data"].toArray();
    auctions_table_->setRowCount(0);
    auctions_table_->setRowCount(records.size());

    for (int i = 0; i < records.size(); ++i) {
        const auto r = records[i].toObject();
        auctions_table_->setItem(i, 0, new QTableWidgetItem(r["cusip"].toString()));

        auto* type_item = new QTableWidgetItem(r["securityType"].toString());
        type_item->setForeground(QColor(kColor));
        auctions_table_->setItem(i, 1, type_item);

        auctions_table_->setItem(i, 2, new QTableWidgetItem(r["securityTerm"].toString()));
        auctions_table_->setItem(i, 3, new QTableWidgetItem(r["auctionDate"].toString("-")));

        auto fmt_num = [](const QJsonValue& v) -> QString {
            if (v.isNull() || v.isUndefined())
                return "-";
            return QString::number(v.toDouble(), 'f', 3);
        };

        auctions_table_->setItem(i, 4, new QTableWidgetItem(fmt_num(r["highDiscountRate"])));
        auctions_table_->setItem(i, 5, new QTableWidgetItem(fmt_num(r["highPrice"])));
        auctions_table_->setItem(i, 6, new QTableWidgetItem(fmt_num(r["bidToCoverRatio"])));

        // Format offering amount
        double offering = r["offeringAmount"].toDouble(0);
        QString offering_str = offering > 0 ? QString("$%L1").arg(static_cast<qlonglong>(offering)) : "-";
        auctions_table_->setItem(i, 7, new QTableWidgetItem(offering_str));
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

    // Type breakdown
    auto types = data["security_types"].toObject();
    type_breakdown_table_->setRowCount(0);
    type_breakdown_table_->setRowCount(types.size());
    int row = 0;
    for (auto it = types.begin(); it != types.end(); ++it, ++row) {
        auto* name_item = new QTableWidgetItem(it.key());
        name_item->setForeground(QColor(kColor));
        type_breakdown_table_->setItem(row, 0, name_item);
        type_breakdown_table_->setItem(row, 1, new QTableWidgetItem(QString::number(it.value().toInt())));
    }

    LOG_INFO("GovTreasury", "Summary loaded");
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void GovDataTreasuryPanel::show_loading(const QString& message) {
    status_label_->setText(message);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:13px; }").arg(kColor, fonts::DATA_FAMILY));
    content_stack_->setCurrentIndex(3);
}

void GovDataTreasuryPanel::show_error(const QString& message) {
    status_label_->setText("Error: " + message);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:12px; }").arg(colors::RED, fonts::DATA_FAMILY));
    content_stack_->setCurrentIndex(3);
}

void GovDataTreasuryPanel::on_export_csv() {
    QTableWidget* table = nullptr;
    if (active_endpoint_ == Prices)
        table = prices_table_;
    else if (active_endpoint_ == Auctions)
        table = auctions_table_;
    else if (active_endpoint_ == Summary)
        table = type_breakdown_table_;
    if (!table || table->rowCount() == 0)
        return;

    QString path = QFileDialog::getSaveFileName(this, "Export CSV", "treasury_data.csv", "CSV Files (*.csv)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

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
