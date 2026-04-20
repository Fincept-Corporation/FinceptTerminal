// src/screens/gov_data/GovDataCongressPanel.cpp
#include "screens/gov_data/GovDataCongressPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const QString kGovDataCongressColor = "#8B5CF6";
static const QString kGovDataCongressScript = "congress_gov_data.py";

// ── Congress-specific extra styles (QSpinBox, QComboBox, QTextBrowser) ──────

static QString congress_extra_qss() {
    const auto& t = ThemeManager::instance().tokens();
    QString s;
    s += QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                 "  font-size:11px; padding:2px 6px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim);
    s += "QSpinBox::up-button, QSpinBox::down-button { width:14px; }";
    s += QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                 "  font-size:11px; padding:2px 6px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim);
    s += "QComboBox::drop-down { border:none; }";
    s += QString("QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; }")
             .arg(t.bg_raised, t.text_primary, t.border_dim);
    s += QString("QTextBrowser { background:%1; color:%2; border:none; padding:16px; font-size:12px; }")
             .arg(t.bg_base, t.text_primary);
    return s;
}

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataCongressPanel::GovDataCongressPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(make_gov_panel_style(kGovDataCongressColor, congress_extra_qss()));
    build_ui();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataCongressPanel::on_result);
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this]() { setStyleSheet(make_gov_panel_style(kGovDataCongressColor, congress_extra_qss())); });
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataCongressPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    content_stack_ = new QStackedWidget;

    // ── Page 0: Bills table ──
    bills_table_ = new QTableWidget;
    bills_table_->setColumnCount(6);
    bills_table_->setHorizontalHeaderLabels({"CONGRESS", "TYPE", "NUMBER", "TITLE", "LATEST ACTION", "DATE"});
    bills_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    bills_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    for (int c : {0, 1, 2, 5})
        bills_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    bills_table_->setColumnWidth(0, 80);
    bills_table_->setColumnWidth(1, 60);
    bills_table_->setColumnWidth(2, 70);
    bills_table_->setColumnWidth(5, 100);
    configure_table(bills_table_);
    connect(bills_table_, &QTableWidget::cellDoubleClicked, this, &GovDataCongressPanel::on_bill_clicked);
    content_stack_->addWidget(bills_table_);

    // ── Page 1: Bill detail ──
    detail_browser_ = new QTextBrowser;
    detail_browser_->setOpenExternalLinks(true);
    content_stack_->addWidget(detail_browser_);

    // ── Page 2: Summary table ──
    summary_table_ = new QTableWidget;
    summary_table_->setColumnCount(2);
    summary_table_->setHorizontalHeaderLabels({"BILL TYPE", "COUNT"});
    summary_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    summary_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    summary_table_->verticalHeader()->setVisible(false);
    summary_table_->setAlternatingRowColors(true);
    summary_table_->setShowGrid(true);
    content_stack_->addWidget(summary_table_);

    // ── Page 3: Status ──
    auto* status_page = new QWidget(this);
    status_page->setObjectName("govStatusPage");
    auto* svl = new QVBoxLayout(status_page);
    svl->setAlignment(Qt::AlignCenter);
    status_label_ = new QLabel;
    status_label_->setObjectName("govStatusMsg");
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    svl->addWidget(status_label_);
    content_stack_->addWidget(status_page);

    root->addWidget(content_stack_, 1);
}

QWidget* GovDataCongressPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setFixedHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button
    back_btn_ = new QPushButton("← BACK");
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_back_to_list);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // View tabs
    bills_btn_ = new QPushButton("BILLS");
    bills_btn_->setObjectName("govTabBtn");
    bills_btn_->setCheckable(true);
    bills_btn_->setChecked(true);
    bills_btn_->setCursor(Qt::PointingHandCursor);
    connect(bills_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Bills); });

    summary_btn_ = new QPushButton("SUMMARY");
    summary_btn_->setObjectName("govTabBtn");
    summary_btn_->setCheckable(true);
    summary_btn_->setCursor(Qt::PointingHandCursor);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(SummaryView); });

    hl->addWidget(bills_btn_);
    hl->addWidget(summary_btn_);
    hl->addSpacing(14);

    // Congress number
    auto lbl_style =
        QString("color:%1; font-size:9px; font-weight:700; background:transparent;").arg(colors::TEXT_TERTIARY());
    auto* cong_lbl = new QLabel("CONGRESS");
    cong_lbl->setStyleSheet(lbl_style);
    hl->addWidget(cong_lbl);

    congress_num_ = new QSpinBox;
    congress_num_->setRange(1, 200);
    congress_num_->setValue(118);
    congress_num_->setFixedHeight(26);
    hl->addWidget(congress_num_);

    hl->addSpacing(10);

    // Bill type
    auto* type_lbl = new QLabel("TYPE");
    type_lbl->setStyleSheet(lbl_style);
    hl->addWidget(type_lbl);

    bill_type_combo_ = new QComboBox;
    bill_type_combo_->addItem("All Types", "");
    bill_type_combo_->addItem("House Bill", "hr");
    bill_type_combo_->addItem("Senate Bill", "s");
    bill_type_combo_->addItem("H. Joint Res.", "hjres");
    bill_type_combo_->addItem("S. Joint Res.", "sjres");
    bill_type_combo_->addItem("H. Con. Res.", "hconres");
    bill_type_combo_->addItem("S. Con. Res.", "sconres");
    bill_type_combo_->addItem("H. Simple Res.", "hres");
    bill_type_combo_->addItem("S. Simple Res.", "sres");
    bill_type_combo_->setFixedHeight(26);
    hl->addWidget(bill_type_combo_);

    hl->addStretch(1);

    fetch_btn_ = new QPushButton("FETCH");
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_fetch_bills);
    hl->addWidget(fetch_btn_);

    export_btn_ = new QPushButton("CSV");
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Actions ──────────────────────────────────────────────────────────────────

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
        show_loading("Loading Congress summary…");
        services::GovDataService::instance().execute(kGovDataCongressScript, "summary", {QString::number(congress)},
                                                     "gov_congress_summary");
    } else {
        show_loading("Loading bills…");
        QStringList args;
        args << QString::number(congress);
        if (!bill_type.isEmpty())
            args << bill_type;
        services::GovDataService::instance().execute(kGovDataCongressScript, "congress_bills", args,
                                                     "gov_congress_bills");
    }
}

void GovDataCongressPanel::on_bill_clicked(int row) {
    auto* type_item = bills_table_->item(row, 1);
    auto* num_item = bills_table_->item(row, 2);
    auto* cong_item = bills_table_->item(row, 0);
    if (!type_item || !num_item || !cong_item)
        return;

    QString bill_type = type_item->data(Qt::UserRole).toString();
    QString bill_num = num_item->text();
    int congress = cong_item->text().toInt();

    show_loading("Loading bill detail…");
    back_btn_->setVisible(true);
    services::GovDataService::instance().execute(kGovDataCongressScript, "bill_info",
                                                 {QString::number(congress), bill_type.toLower(), bill_num},
                                                 "gov_congress_detail");
}

void GovDataCongressPanel::on_back_to_list() {
    back_btn_->setVisible(false);
    content_stack_->setCurrentIndex(Bills);
    current_view_ = Bills;
}

void GovDataCongressPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    if (!request_id.startsWith("gov_congress_"))
        return;
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

// ── Populate ─────────────────────────────────────────────────────────────────

void GovDataCongressPanel::populate_bills(const QJsonObject& json) {
    const QJsonArray bills = json["bills"].toArray();
    bills_table_->setRowCount(0);
    bills_table_->setRowCount(bills.size());

    for (int i = 0; i < bills.size(); ++i) {
        const auto b = bills[i].toObject();

        bills_table_->setItem(i, 0, new QTableWidgetItem(QString::number(b["congress"].toInt())));

        auto* type_item = new QTableWidgetItem(b["bill_type"].toString().toUpper());
        type_item->setForeground(QColor(kGovDataCongressColor));
        type_item->setData(Qt::UserRole, b["bill_type"].toString());
        bills_table_->setItem(i, 1, type_item);

        bills_table_->setItem(i, 2, new QTableWidgetItem(QString::number(b["bill_number"].toInt())));

        QString title = b["title"].toString();
        if (title.length() > 120)
            title = title.left(120) + "…";
        bills_table_->setItem(i, 3, new QTableWidgetItem(title));

        QString action = b["latest_action"].toString();
        bills_table_->setItem(i, 4, new QTableWidgetItem(action.isEmpty() ? "—" : action));

        QString date = b["latest_action_date"].toString();
        if (date.isEmpty())
            date = b["update_date"].toString();
        if (date.length() > 10)
            date = date.left(10);
        bills_table_->setItem(i, 5, new QTableWidgetItem(date));
    }
    LOG_INFO("GovCongress", QString("Loaded %1 bills").arg(bills.size()));
}

void GovDataCongressPanel::populate_bill_detail(const QJsonObject& json) {
    QString md = json["markdown_content"].toString();
    if (md.isEmpty()) {
        auto bill = json["bill_data"].toObject();
        md = "# " + bill["title"].toString() + "\n\n";
        md += "**Type:** " + bill["type"].toString() + "\n\n";
        md += "**Congress:** " + QString::number(bill["congress"].toInt()) + "\n\n";
        md += "**Status:** " + bill["latestAction"].toObject()["text"].toString() + "\n\n";
    }

    QString html = md;
    html.replace("\n\n", "<br><br>");
    html.replace(QRegularExpression("^# (.+)$", QRegularExpression::MultilineOption),
                 "<h2 style='color:" + kGovDataCongressColor + ";font-weight:700;'>\\1</h2>");
    html.replace(QRegularExpression("^## (.+)$", QRegularExpression::MultilineOption),
                 "<h3 style='color:" + kGovDataCongressColor + ";'>\\1</h3>");
    html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");

    const auto& tk = ThemeManager::instance().tokens();
    detail_browser_->setHtml(QString("<html><body style='background:%1; color:%2; font-family:%3,monospace;"
                                     " font-size:12px; padding:16px;'>")
                                 .arg(tk.bg_base, tk.text_primary, tk.font_family) +
                             html + "</body></html>");
    back_btn_->setVisible(true);
    current_view_ = BillDetail;
}

void GovDataCongressPanel::populate_summary(const QJsonObject& json) {
    auto bill_types = json["bill_types"].toObject();
    summary_table_->setRowCount(0);
    summary_table_->setRowCount(bill_types.size());

    int row = 0;
    for (auto it = bill_types.begin(); it != bill_types.end(); ++it, ++row) {
        auto* name_item = new QTableWidgetItem(it.key().toUpper());
        name_item->setForeground(QColor(kGovDataCongressColor));
        summary_table_->setItem(row, 0, name_item);
        summary_table_->setItem(row, 1, new QTableWidgetItem(QString::number(it.value().toObject()["count"].toInt())));
    }
    LOG_INFO("GovCongress", "Summary loaded");
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void GovDataCongressPanel::show_loading(const QString& message) {
    status_label_->setStyleSheet(
        QString("color:%1; font-size:13px; background:transparent;").arg(kGovDataCongressColor));
    status_label_->setText(message);
    content_stack_->setCurrentIndex(3);
}

void GovDataCongressPanel::show_error(const QString& message) {
    status_label_->setStyleSheet(QString("color:%1; font-size:12px; background:transparent;").arg(colors::NEGATIVE()));
    status_label_->setText("Error: " + message);
    content_stack_->setCurrentIndex(3);
}

void GovDataCongressPanel::on_export_csv() {
    QTableWidget* table = (current_view_ == SummaryView) ? summary_table_ : bills_table_;
    export_table_to_csv(table, "congress_data.csv", this);
}

} // namespace fincept::screens
