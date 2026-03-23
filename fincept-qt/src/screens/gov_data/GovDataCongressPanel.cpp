// src/screens/gov_data/GovDataCongressPanel.cpp
#include "screens/gov_data/GovDataCongressPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const QString kColor = "#8B5CF6";
static const QString kScript = "congress_gov_data.py";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

GovDataCongressPanel::GovDataCongressPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect(&services::GovDataService::instance(),
            &services::GovDataService::result_ready,
            this, &GovDataCongressPanel::on_result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

void GovDataCongressPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    content_stack_ = new QStackedWidget(this);

    const QString table_style = QString(
        "QTableWidget { background:%1; color:%2; border:none; gridline-color:%3;"
        "  font-family:%4; font-size:12px; }"
        "QTableWidget::item { padding:6px 8px; border-bottom:1px solid %3; }"
        "QTableWidget::item:selected { background:%5; }"
        "QTableWidget::item:alternate { background:%6; }"
        "QHeaderView::section { background:%7; color:%8; border:none; border-bottom:1px solid %3;"
        "  font-family:%4; font-size:10px; font-weight:700; padding:6px 8px; }")
        .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM,
             fonts::DATA_FAMILY, colors::BG_HOVER, colors::ROW_ALT,
             colors::BG_RAISED, kColor);

    // Page 0: Bills table
    bills_table_ = new QTableWidget(this);
    bills_table_->setColumnCount(6);
    bills_table_->setHorizontalHeaderLabels(
        {"CONGRESS", "TYPE", "NUMBER", "TITLE", "LATEST ACTION", "DATE"});
    bills_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    bills_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    for (int c : {0, 1, 2, 5}) {
        bills_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    }
    bills_table_->setColumnWidth(0, 80);
    bills_table_->setColumnWidth(1, 60);
    bills_table_->setColumnWidth(2, 70);
    bills_table_->setColumnWidth(5, 100);
    bills_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    bills_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bills_table_->verticalHeader()->setVisible(false);
    bills_table_->setAlternatingRowColors(true);
    bills_table_->setStyleSheet(table_style);
    connect(bills_table_, &QTableWidget::cellDoubleClicked, this, &GovDataCongressPanel::on_bill_clicked);
    content_stack_->addWidget(bills_table_);

    // Page 1: Bill detail (rich text)
    detail_browser_ = new QTextBrowser(this);
    detail_browser_->setOpenExternalLinks(true);
    detail_browser_->setStyleSheet(
        QString("QTextBrowser { background:%1; color:%2; border:none;"
                " font-family:%3; font-size:13px; padding:16px; }")
            .arg(colors::BG_BASE, colors::TEXT_PRIMARY, fonts::DATA_FAMILY));
    content_stack_->addWidget(detail_browser_);

    // Page 2: Summary table
    summary_table_ = new QTableWidget(this);
    summary_table_->setColumnCount(2);
    summary_table_->setHorizontalHeaderLabels({"BILL TYPE", "COUNT"});
    summary_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    summary_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    summary_table_->verticalHeader()->setVisible(false);
    summary_table_->setAlternatingRowColors(true);
    summary_table_->setStyleSheet(table_style);
    content_stack_->addWidget(summary_table_);

    // Page 3: Status label
    status_label_ = new QLabel(this);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:12px; }")
            .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    content_stack_->addWidget(status_label_);

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataCongressPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(38);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;")
            .arg(colors::BG_RAISED, colors::BORDER_MED));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(4);

    const QString btn_style =
        QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                " font-family:%3; font-size:10px; font-weight:700; padding:4px 10px; }"
                "QPushButton:hover { background:%4; }"
                "QPushButton:checked { background:%5; color:%6; border-color:%5; }")
            .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM, fonts::DATA_FAMILY,
                 colors::BG_HOVER, kColor, colors::TEXT_PRIMARY);

    // Back button (shown in detail view)
    back_btn_ = new QPushButton("< BACK", bar);
    back_btn_->setStyleSheet(btn_style);
    back_btn_->setVisible(false);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_back_to_list);
    hl->addWidget(back_btn_);

    // View tabs
    bills_btn_ = new QPushButton("BILLS", bar);
    bills_btn_->setCheckable(true);
    bills_btn_->setChecked(true);
    bills_btn_->setStyleSheet(btn_style);
    connect(bills_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Bills); });

    summary_btn_ = new QPushButton("SUMMARY", bar);
    summary_btn_->setCheckable(true);
    summary_btn_->setStyleSheet(btn_style);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(SummaryView); });

    hl->addWidget(bills_btn_);
    hl->addWidget(summary_btn_);
    hl->addSpacing(12);

    // Congress number
    auto* cong_label = new QLabel("CONGRESS:", bar);
    cong_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px; font-weight:700;")
            .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));

    congress_num_ = new QSpinBox(bar);
    congress_num_->setRange(1, 200);
    congress_num_->setValue(118); // Current congress
    congress_num_->setStyleSheet(
        QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                " font-family:%4; font-size:11px; padding:2px 6px; }"
                "QSpinBox::up-button, QSpinBox::down-button { width:14px; }")
            .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM, fonts::DATA_FAMILY));

    hl->addWidget(cong_label);
    hl->addWidget(congress_num_);
    hl->addSpacing(8);

    // Bill type filter
    auto* type_label = new QLabel("TYPE:", bar);
    type_label->setStyleSheet(cong_label->styleSheet());

    bill_type_combo_ = new QComboBox(bar);
    bill_type_combo_->addItem("All Types", "");
    bill_type_combo_->addItem("House Bill", "hr");
    bill_type_combo_->addItem("Senate Bill", "s");
    bill_type_combo_->addItem("House Joint Res.", "hjres");
    bill_type_combo_->addItem("Senate Joint Res.", "sjres");
    bill_type_combo_->addItem("House Con. Res.", "hconres");
    bill_type_combo_->addItem("Senate Con. Res.", "sconres");
    bill_type_combo_->addItem("House Simple Res.", "hres");
    bill_type_combo_->addItem("Senate Simple Res.", "sres");
    bill_type_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                " font-family:%4; font-size:11px; padding:2px 6px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; }")
            .arg(colors::BG_BASE, colors::TEXT_PRIMARY, colors::BORDER_DIM, fonts::DATA_FAMILY));

    hl->addWidget(type_label);
    hl->addWidget(bill_type_combo_);

    hl->addStretch();

    // Fetch
    fetch_btn_ = new QPushButton("FETCH", bar);
    fetch_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none;"
                " font-family:%3; font-size:10px; font-weight:700; padding:4px 12px; }")
            .arg(kColor, colors::TEXT_PRIMARY, fonts::DATA_FAMILY));
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_fetch_bills);
    hl->addWidget(fetch_btn_);

    // Export
    export_btn_ = new QPushButton("CSV", bar);
    export_btn_->setStyleSheet(btn_style);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

void GovDataCongressPanel::load_initial_data() {
    show_loading("Select BILLS or SUMMARY and click FETCH");
}

void GovDataCongressPanel::on_view_changed(int index) {
    auto view = static_cast<View>(index);
    bills_btn_->setChecked(view == Bills);
    summary_btn_->setChecked(view == SummaryView);
    back_btn_->setVisible(false);
    current_view_ = view;
}

void GovDataCongressPanel::on_fetch_bills() {
    int congress = congress_num_->value();
    QString bill_type = bill_type_combo_->currentData().toString();

    if (current_view_ == SummaryView) {
        show_loading("Loading Congress summary...");
        services::GovDataService::instance().execute(
            kScript, "summary",
            {QString::number(congress)},
            "gov_congress_summary");
    } else {
        show_loading("Loading bills...");
        QStringList args;
        args << QString::number(congress);
        if (!bill_type.isEmpty()) args << bill_type;
        services::GovDataService::instance().execute(
            kScript, "bills", args, "gov_congress_bills");
    }
}

void GovDataCongressPanel::on_bill_clicked(int row) {
    auto* type_item = bills_table_->item(row, 1);
    auto* num_item = bills_table_->item(row, 2);
    auto* cong_item = bills_table_->item(row, 0);
    if (!type_item || !num_item || !cong_item) return;

    QString bill_type = type_item->data(Qt::UserRole).toString();
    QString bill_num = num_item->text();
    int congress = cong_item->text().toInt();

    show_loading("Loading bill detail...");
    back_btn_->setVisible(true);
    services::GovDataService::instance().execute(
        kScript, "bill_info",
        {QString::number(congress), bill_type.toLower(), bill_num},
        "gov_congress_detail");
}

void GovDataCongressPanel::on_back_to_list() {
    back_btn_->setVisible(false);
    content_stack_->setCurrentIndex(Bills);
    current_view_ = Bills;
}

void GovDataCongressPanel::on_result(const QString& request_id,
                                       const services::GovDataResult& result) {
    if (!request_id.startsWith("gov_congress_")) return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    if (request_id.contains("bills")) {
        populate_bills(result.data);
        content_stack_->setCurrentIndex(Bills);
    } else if (request_id.contains("detail")) {
        populate_bill_detail(result.data);
        content_stack_->setCurrentIndex(BillDetail);
    } else if (request_id.contains("summary")) {
        populate_summary(result.data);
        content_stack_->setCurrentIndex(SummaryView);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate
// ─────────────────────────────────────────────────────────────────────────────

void GovDataCongressPanel::populate_bills(const QJsonObject& data) {
    const QJsonArray bills = data["bills"].toArray();
    bills_table_->setRowCount(0);
    bills_table_->setRowCount(bills.size());

    for (int i = 0; i < bills.size(); ++i) {
        const auto b = bills[i].toObject();

        bills_table_->setItem(i, 0,
            new QTableWidgetItem(QString::number(b["congress"].toInt())));

        auto* type_item = new QTableWidgetItem(b["bill_type"].toString().toUpper());
        type_item->setForeground(QColor(kColor));
        type_item->setData(Qt::UserRole, b["bill_type"].toString());
        bills_table_->setItem(i, 1, type_item);

        bills_table_->setItem(i, 2,
            new QTableWidgetItem(QString::number(b["bill_number"].toInt())));

        // Title (truncate for display)
        QString title = b["title"].toString();
        if (title.length() > 120) title = title.left(120) + "...";
        bills_table_->setItem(i, 3, new QTableWidgetItem(title));

        bills_table_->setItem(i, 4,
            new QTableWidgetItem(b["latest_action"].toString("-")));

        QString date = b["latest_action_date"].toString(b["update_date"].toString());
        if (date.length() > 10) date = date.left(10);
        bills_table_->setItem(i, 5, new QTableWidgetItem(date));
    }

    LOG_INFO("GovCongress", QString("Loaded %1 bills").arg(bills.size()));
}

void GovDataCongressPanel::populate_bill_detail(const QJsonObject& data) {
    QString md = data["markdown_content"].toString();
    if (md.isEmpty()) {
        // Fallback: render bill_data as formatted text
        auto bill = data["bill_data"].toObject();
        md = "# " + bill["title"].toString() + "\n\n";
        md += "**Type:** " + bill["type"].toString() + "\n\n";
        md += "**Congress:** " + QString::number(bill["congress"].toInt()) + "\n\n";
        md += "**Status:** " + bill["latestAction"].toObject()["text"].toString() + "\n\n";
    }

    // Simple markdown-to-HTML conversion for key patterns
    QString html = md;
    html.replace("\n\n", "<br><br>");
    html.replace(QRegularExpression("^# (.+)$", QRegularExpression::MultilineOption),
                 "<h2 style='color:" + kColor + ";'>\\1</h2>");
    html.replace(QRegularExpression("^## (.+)$", QRegularExpression::MultilineOption),
                 "<h3 style='color:" + kColor + ";'>\\1</h3>");
    html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");

    detail_browser_->setHtml(html);
    back_btn_->setVisible(true);
    current_view_ = BillDetail;
}

void GovDataCongressPanel::populate_summary(const QJsonObject& data) {
    auto bill_types = data["bill_types"].toObject();
    summary_table_->setRowCount(0);
    summary_table_->setRowCount(bill_types.size());

    int row = 0;
    for (auto it = bill_types.begin(); it != bill_types.end(); ++it, ++row) {
        auto* name_item = new QTableWidgetItem(it.key().toUpper());
        name_item->setForeground(QColor(kColor));
        summary_table_->setItem(row, 0, name_item);

        int count = it.value().toObject()["count"].toInt();
        summary_table_->setItem(row, 1, new QTableWidgetItem(QString::number(count)));
    }

    LOG_INFO("GovCongress", "Summary loaded");
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void GovDataCongressPanel::show_loading(const QString& message) {
    status_label_->setText(message);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:13px; }")
            .arg(kColor, fonts::DATA_FAMILY));
    content_stack_->setCurrentIndex(3);
}

void GovDataCongressPanel::show_error(const QString& message) {
    status_label_->setText("Error: " + message);
    status_label_->setStyleSheet(
        QString("QLabel { color:%1; font-family:%2; font-size:12px; }")
            .arg(colors::RED, fonts::DATA_FAMILY));
    content_stack_->setCurrentIndex(3);
}

void GovDataCongressPanel::on_export_csv() {
    QTableWidget* table = (current_view_ == SummaryView) ? summary_table_ : bills_table_;
    if (!table || table->rowCount() == 0) return;

    QString path = QFileDialog::getSaveFileName(this, "Export CSV",
        "congress_data.csv", "CSV Files (*.csv)");
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
