// src/screens/gov_data/GovDataCongressPanel.cpp
#include "screens/gov_data/GovDataCongressPanel.h"

#include "core/logging/Logger.h"
#include "screens/gov_data/GovDataProviderPanel.h"
#include "screens/gov_data/GovTableModel.h"
#include "services/gov_data/GovDataService.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
namespace fmt = fincept::ui::formatting;

namespace {

// Congress.gov identity colour as it reads on the dark theme; always drawn through colors::legible().
const QString kGovDataCongressColor = QStringLiteral("#8B5CF6");
const QString kGovDataCongressScript = QStringLiteral("congress_gov_data.py");
const QString kGovCongressPrefix = QStringLiteral("gov.congress");
constexpr int kGovCongressMaxAgeS = 6 * 3600;

QString congress_extra_qss() {
    const auto& t = ThemeManager::instance().tokens();
    QString s;
    s += "QSpinBox::up-button, QSpinBox::down-button { width:14px; }";
    s += QString("QTextBrowser { background:%1; color:%2; border:none; padding:16px; font-size:%3px; }")
             .arg(t.bg_base, t.text_primary)
             .arg(fonts::px(-2));
    return s;
}

/// The sitting Congress: the 1st met in 1789 and a new one starts every two years (2025-2026 -> 119th).
int congress_current() {
    return (QDate::currentDate().year() - 1789) / 2 + 1;
}

} // namespace

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataCongressPanel::GovDataCongressPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(make_gov_panel_style(kGovDataCongressColor, congress_extra_qss())); // once, before children
    build_ui();
    retranslateUi();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataCongressPanel::on_result);
    // Item brushes and the detail document carry colours: redo them once the appearance settled.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });

    // The Congress, bill type and tab come back with the panel.
    memory_ = new InputMemory(QStringLiteral("gov_data"), QStringLiteral("us_congress"), this);
    memory_->track(QStringLiteral("congress"), congress_num_);
    memory_->track(QStringLiteral("bill_type"), bill_type_combo_);
    memory_->track(QStringLiteral("tab_summary"), summary_btn_);
    memory_->restore();
    current_view_ = summary_btn_->isChecked() ? SummaryView : Bills;
    go(view_key(current_view_), false); // loads when the pane is first shown (key permitting)
}

void GovDataCongressPanel::refresh_theme() {
    // The bills model reads its ink at paint time; the small summary table has item brushes.
    recolor_column(summary_table_, 0, colors::legible_color(QColor(kGovDataCongressColor)));
    render_bill_detail();
}

void GovDataCongressPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataCongressPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());
    build_pane(this);
    root->addWidget(pane_, 1);
}

void GovDataCongressPanel::build_pane(QWidget* parent) {
    pane_ = gov::make_pane(parent, QStringLiteral("gov_data/us_congress"), as_of_label_);
    // Congress.gov needs a free key: without it the pane shows "Needs a Congress.gov API key" with
    // OPEN SETTINGS > CREDENTIALS; once it is saved, the default list loads on the next show.
    pane_->set_requirements({Requirement::python(),
                             Requirement::api_key(QStringLiteral("CONGRESS_GOV_API_KEY"), QStringLiteral("Congress.gov"))});
    content_stack_ = new QStackedWidget(pane_->content_parent());

    // Bills table: CONGRESS | TYPE | NUMBER | TITLE | LATEST ACTION | DATE (headers set in retranslateUi)
    const auto field = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return r.value(k).toString(); };
    };
    bills_model_ = gov::make_model(kGovDataCongressColor, this);
    bills_model_->set_columns({
        {.text = [](const QJsonObject& r) { return QString::number(r.value(QStringLiteral("congress")).toInt()); }},
        {.text = [](const QJsonObject& r) { return r.value(QStringLiteral("type")).toString().toUpper(); },
         .ink = GovTableModel::Ink::Accent},
        {.text = field("number")},
        {.text = field("title"), .tooltip = field("title")},
        {.text = [](const QJsonObject& r) {
             const QString action = r.value(QStringLiteral("action")).toString();
             return action.isEmpty() ? fmt::placeholder() : action;
         },
         .tooltip = field("action")},
        {.text = field("date")},
    });
    bills_table_ = gov::make_table_view(content_stack_, bills_model_);
    bills_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    bills_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    for (int c : {0, 1, 2, 5})
        bills_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    bills_table_->setColumnWidth(0, 80);
    bills_table_->setColumnWidth(1, 70);
    bills_table_->setColumnWidth(2, 80);
    bills_table_->setColumnWidth(5, 100);
    connect(bills_table_, &QTableView::doubleClicked, this, [this](const QModelIndex& i) { on_bill_clicked(i.row()); });
    content_stack_->addWidget(bills_table_);

    // Bill detail
    detail_browser_ = new QTextBrowser(content_stack_);
    detail_browser_->setOpenExternalLinks(true);
    content_stack_->addWidget(detail_browser_);

    // Summary table (headers set in retranslateUi)
    summary_table_ = new QTableWidget(content_stack_);
    summary_table_->setColumnCount(2);
    summary_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    configure_table(summary_table_);
    summary_table_->viewport()->setCursor(Qt::ArrowCursor);
    content_stack_->addWidget(summary_table_);

    no_data_page_ = gov::make_no_data_page(content_stack_);
    connect(no_data_page_, &EmptyState::primary_clicked, this, [this]() {
        // "ALL TYPES": the filter emptied the list; otherwise reload.
        if (current_view_ == Bills && !bill_type_combo_->currentData().toString().isEmpty()) {
            bill_type_combo_->setCurrentIndex(0);
            go(view_key(Bills), true);
            return;
        }
        pane_->run();
    });
    content_stack_->addWidget(no_data_page_);

    pane_->set_content(content_stack_);
    pane_->set_start([this](quint64 run) { start_run(run); });
    pane_->set_cancel([this](quint64 run) { gov::cancel_token(tokens_, run); });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p); });
}

QWidget* GovDataCongressPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setMinimumHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button (text set in retranslateUi)
    back_btn_ = new QPushButton(bar);
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_back_to_list);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // View tabs (text set in retranslateUi)
    bills_btn_ = new QPushButton(bar);
    bills_btn_->setObjectName("govTabBtn");
    bills_btn_->setCheckable(true);
    bills_btn_->setChecked(true);
    bills_btn_->setCursor(Qt::PointingHandCursor);
    connect(bills_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(Bills); });

    summary_btn_ = new QPushButton(bar);
    summary_btn_->setObjectName("govTabBtn");
    summary_btn_->setCheckable(true);
    summary_btn_->setCursor(Qt::PointingHandCursor);
    connect(summary_btn_, &QPushButton::clicked, this, [this]() { on_view_changed(SummaryView); });

    hl->addWidget(bills_btn_);
    hl->addWidget(summary_btn_);
    hl->addSpacing(14);

    // Filter labels (text set in retranslateUi) — styled by the panel sheet (#govFilterLabel)
    congress_lbl_ = new QLabel(bar);
    congress_lbl_->setObjectName("govFilterLabel");
    hl->addWidget(congress_lbl_);

    congress_num_ = new QSpinBox(bar);
    congress_num_->setRange(1, 200);
    congress_num_->setValue(congress_current());
    congress_num_->setMinimumHeight(26);
    hl->addWidget(congress_num_);

    hl->addSpacing(10);

    type_lbl_ = new QLabel(bar);
    type_lbl_->setObjectName("govFilterLabel");
    hl->addWidget(type_lbl_);

    // Bill type — display text set in retranslateUi, user data is the API key.
    bill_type_combo_ = new QComboBox(bar);
    for (const char* code : {"", "hr", "s", "hjres", "sjres", "hconres", "sconres", "hres", "sres"})
        bill_type_combo_->addItem(QString(), QString::fromLatin1(code));
    bill_type_combo_->setMinimumHeight(26);
    hl->addWidget(bill_type_combo_);

    hl->addStretch(1);
    info_label_ = new QLabel(bar);
    info_label_->setObjectName("govBreadcrumbText");
    hl->addWidget(info_label_);
    hl->addSpacing(8);
    as_of_label_ = gov::make_as_of_label(bar);
    hl->addWidget(as_of_label_);
    hl->addSpacing(8);

    fetch_btn_ = new QPushButton(bar);
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_fetch_bills);
    hl->addWidget(fetch_btn_);

    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataCongressPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataCongressPanel::retranslateUi() {
    if (back_btn_)
        back_btn_->setText(tr("← BACK"));
    if (bills_btn_)
        bills_btn_->setText(tr("BILLS"));
    if (summary_btn_)
        summary_btn_->setText(tr("SUMMARY"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));
    if (congress_lbl_)
        congress_lbl_->setText(tr("CONGRESS"));
    if (type_lbl_)
        type_lbl_->setText(tr("TYPE"));

    if (bill_type_combo_) {
        const QString cur = bill_type_combo_->currentData().toString();
        bill_type_combo_->setItemText(0, tr("All Types"));
        bill_type_combo_->setItemText(1, tr("House Bill"));
        bill_type_combo_->setItemText(2, tr("Senate Bill"));
        bill_type_combo_->setItemText(3, tr("H. Joint Res."));
        bill_type_combo_->setItemText(4, tr("S. Joint Res."));
        bill_type_combo_->setItemText(5, tr("H. Con. Res."));
        bill_type_combo_->setItemText(6, tr("S. Con. Res."));
        bill_type_combo_->setItemText(7, tr("H. Simple Res."));
        bill_type_combo_->setItemText(8, tr("S. Simple Res."));
        const int idx = bill_type_combo_->findData(cur);
        if (idx >= 0)
            bill_type_combo_->setCurrentIndex(idx);
    }

    if (bills_model_)
        bills_model_->set_headers({tr("CONGRESS"), tr("TYPE"), tr("NUMBER"), tr("TITLE"), tr("LATEST ACTION"), tr("DATE")});
    if (summary_table_)
        summary_table_->setHorizontalHeaderLabels({tr("BILL TYPE"), tr("COUNT")});
}

// ── Views and runs ───────────────────────────────────────────────────────────

QString GovDataCongressPanel::view_key(View v) const {
    const QString congress = QString::number(congress_num_->value());
    if (v == SummaryView)
        return QStringLiteral("summary|") + congress;
    return QStringLiteral("bills|") + congress + QLatin1Char('|') + bill_type_combo_->currentData().toString();
}

GovDataCongressPanel::View GovDataCongressPanel::view_of(const QString& key) {
    if (key.startsWith(QLatin1String("summary|")))
        return SummaryView;
    if (key.startsWith(QLatin1String("bill|")))
        return BillDetail;
    return Bills;
}

void GovDataCongressPanel::go(const QString& key, bool user, bool force) {
    current_view_ = view_of(key);
    if (current_view_ == Bills)
        last_list_view_ = key;
    // New bills and actions land on session days: a remembered view older than 6 h refreshes quietly.
    pane_->set_view_max_age(key, kGovCongressMaxAgeS);
    pane_->set_view_skeleton(key, current_view_ == BillDetail    ? LoadingSkeleton::Shape::Text
                                  : current_view_ == SummaryView ? LoadingSkeleton::Shape::List
                                                                 : LoadingSkeleton::Shape::Table);
    pane_->set_loading_text(current_view_ == Bills         ? tr("Loading bills from Congress.gov…")
                            : current_view_ == SummaryView ? tr("Counting bills by type…")
                                                           : tr("Loading the bill…"));
    pane_->set_view(key);
    update_toolbar_state();
    // Without the key the pane shows its needs-setup page: FETCH must not spawn a script that can
    // only answer "API key not configured".
    if (!check_requirements({Requirement::api_key(QStringLiteral("CONGRESS_GOV_API_KEY"), QStringLiteral("Congress.gov"))})
             .ok) {
        pane_->recheck_requirements();
        return;
    }
    const bool running_here = pane_->is_running() && pane_->running_view() == key;
    if (force || (user && !pane_->has_result() && !running_here))
        pane_->run();
}

void GovDataCongressPanel::load_initial_data() {
    if (pane_->view().isEmpty())
        go(view_key(current_view_), false);
}

void GovDataCongressPanel::on_view_changed(int index) {
    go(view_key(static_cast<View>(index)), true);
}

void GovDataCongressPanel::on_fetch_bills() {
    // FETCH reloads the tab on screen (the bill detail's list when a bill is open).
    go(view_key(current_view_ == SummaryView ? SummaryView : Bills), true, true);
}

void GovDataCongressPanel::on_bill_clicked(int row) {
    const QJsonObject bill = bills_model_->row(row);
    const QString type = bill.value(QStringLiteral("type")).toString();
    const QString number = bill.value(QStringLiteral("number")).toString();
    if (bill.isEmpty() || type.isEmpty() || number.isEmpty())
        return;
    const QString key = QStringLiteral("bill|") + QString::number(bill.value(QStringLiteral("congress")).toInt()) +
                        QLatin1Char('|') + type.toLower() + QLatin1Char('|') + number;
    go(key, true);
}

void GovDataCongressPanel::on_back_to_list() {
    go(last_list_view_.isEmpty() ? view_key(Bills) : last_list_view_, true);
}

void GovDataCongressPanel::start_run(quint64 run) {
    const QString key = pane_->view();
    const QStringList parts = key.split(QLatin1Char('|'));
    QString command;
    QStringList args;
    switch (view_of(key)) {
        case Bills:
            command = QStringLiteral("congress_bills");
            args << parts.value(1);
            if (!parts.value(2).isEmpty())
                args << parts.value(2);
            break;
        case SummaryView:
            command = QStringLiteral("summary");
            args << parts.value(1);
            break;
        case BillDetail:
            command = QStringLiteral("bill_info");
            args << parts.value(1) << parts.value(2) << parts.value(3);
            break;
    }
    const auto token = gov::start_token(tokens_, run);
    services::GovDataService::instance().execute(kGovDataCongressScript, command, args,
                                                 gov::request_id(kGovCongressPrefix, run), token);
}

void GovDataCongressPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    const quint64 run = gov::run_of(request_id, kGovCongressPrefix);
    if (run == 0)
        return;
    tokens_.remove(run);
    if (!pane_->is_current(run))
        return; // superseded, cancelled, or its view was left
    const QString key = pane_->running_view();
    if (!result.success) {
        if (result.error_code == QLatin1String("no_data") && view_of(key) != BillDetail) {
            pane_->deliver(run, gov::list_payload(view_of(key) == SummaryView ? QStringLiteral("summary")
                                                                              : QStringLiteral("bills"),
                                                  QJsonArray()));
            return;
        }
        // A rejected key (the script's "auth_failed", or its wording) -> invalid_key: the "rejected the
        // API key" page with OPEN SETTINGS > CREDENTIALS on CONGRESS_GOV_API_KEY and RETRY.
        gov::fail_pane(pane_, run, result, QStringLiteral("Congress.gov"));
        return;
    }
    // Shaped on the shaping thread (bill lists, the bill detail's HTML); dropped when superseded.
    const QStringList labels{tr("Type:"), tr("Congress:"), tr("Status:")};
    gov::shape_async(
        this, [view = view_of(key), data = result.data, labels]() { return shape(view, data, labels); },
        [this, run](const QJsonObject& payload) {
            if (pane_->is_current(run))
                pane_->deliver(run, payload);
        });
}

QJsonObject GovDataCongressPanel::shape(View view, const QJsonObject& data, const QStringList& labels) {
    switch (view) {
        case Bills: {
            QJsonArray rows;
            for (const QJsonValue& v : data.value("bills").toArray()) {
                const QJsonObject b = v.toObject();
                // Numbers may arrive as JSON numbers or as numeric strings ("3076"): read both.
                QString title = gov::text(b.value("title"));
                if (title.length() > 160)
                    title = title.left(160) + QStringLiteral("…");
                QString date = gov::text(b.value("latest_action_date"));
                if (date.isEmpty())
                    date = gov::text(b.value("update_date"));
                rows.append(QJsonObject{{"congress", gov::integer(b.value("congress"), 0)},
                                        {"type", gov::text(b.value("bill_type"))},
                                        {"number", gov::text(b.value("bill_number"))},
                                        {"title", title},
                                        {"action", gov::text(b.value("latest_action"))},
                                        {"date", date.left(10)}});
            }
            return gov::list_payload(QStringLiteral("bills"), rows);
        }
        case SummaryView: {
            QJsonArray rows;
            const QJsonObject types = data.value("bill_types").toObject();
            for (auto it = types.begin(); it != types.end(); ++it)
                rows.append(QJsonObject{{"type", it.key()},
                                        {"count", gov::integer(it.value().toObject().value("count"), 0)}});
            return gov::list_payload(QStringLiteral("summary"), rows);
        }
        case BillDetail: {
            QString md = data.value("markdown_content").toString();
            if (md.isEmpty()) {
                const QJsonObject bill = data.value("bill_data").toObject();
                md = "# " + bill.value("title").toString() + "\n\n";
                md += "**" + labels.value(0) + "** " + bill.value("type").toString() + "\n\n";
                md += "**" + labels.value(1) + "** " + QString::number(bill.value("congress").toInt()) + "\n\n";
                md += "**" + labels.value(2) + "** " + bill.value("latestAction").toObject().value("text").toString() +
                      "\n\n";
            }
            static const QRegularExpression kH1(QStringLiteral("^# (.+)$"), QRegularExpression::MultilineOption);
            static const QRegularExpression kH2(QStringLiteral("^## (.+)$"), QRegularExpression::MultilineOption);
            static const QRegularExpression kBold(QStringLiteral("\\*\\*(.+?)\\*\\*"));
            QString html = md.toHtmlEscaped();
            html.replace("\n\n", "<br><br>");
            html.replace(kH1, "<h2 style='color:@ACCENT@;font-weight:700;'>\\1</h2>");
            html.replace(kH2, "<h3 style='color:@ACCENT@;'>\\1</h3>");
            html.replace(kBold, "<b>\\1</b>");
            return QJsonObject{{"kind", "bill"}, {"html", html}};
        }
    }
    return {};
}

// ── Render ───────────────────────────────────────────────────────────────────

void GovDataCongressPanel::render(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QString kind = p.value("kind").toString();
    if (kind == QLatin1String("bill")) {
        detail_body_ = p.value("html").toString();
        render_bill_detail();
        content_stack_->setCurrentWidget(detail_browser_);
        info_label_->clear();
        return;
    }
    const QJsonArray rows = p.value("rows").toArray();
    if (rows.isEmpty()) {
        info_label_->clear();
        const bool filtered = kind == QLatin1String("bills") && !bill_type_combo_->currentData().toString().isEmpty();
        EmptyState::Spec spec = EmptyState::no_data_spec(
            tr("No bills for Congress %1").arg(congress_num_->value()),
            filtered ? tr("No bill of this type has been introduced yet.")
                     : tr("Congress.gov listed nothing for this Congress."),
            filtered ? tr("ALL TYPES") : tr("RELOAD"));
        spec.glyph = QStringLiteral("∅");
        gov::show_no_data(content_stack_, no_data_page_, spec);
        return;
    }
    if (kind == QLatin1String("bills")) {
        populate_bills(rows);
        content_stack_->setCurrentWidget(bills_table_);
        info_label_->setText(tr("%1 bills").arg(fmt::quantity(rows.size())));
        bills_table_->setToolTip(tr("Double-click a bill for its detail"));
    } else {
        populate_summary(p);
        content_stack_->setCurrentWidget(summary_table_);
        info_label_->clear();
    }
}

void GovDataCongressPanel::populate_bills(const QJsonArray& bills) {
    gov::show_rows(bills_table_, bills_model_, bills); // a model reset: cells are formatted when shown
    LOG_DEBUG("GovCongress", QString("Loaded %1 bills").arg(bills.size()));
}

void GovDataCongressPanel::render_bill_detail() {
    if (detail_body_.isEmpty())
        return;
    // The document inherits the application font; colours and size come from the active theme.
    const auto& tk = ThemeManager::instance().tokens();
    QString body = detail_body_;
    body.replace(QStringLiteral("@ACCENT@"), colors::legible(QColor(kGovDataCongressColor)));
    detail_browser_->setHtml(QString("<html><body style='background:%1; color:%2; padding:16px; font-size:%3px;'>")
                                 .arg(tk.bg_base, tk.text_primary)
                                 .arg(fonts::px(-2)) +
                             body + "</body></html>");
}

void GovDataCongressPanel::populate_summary(const QJsonObject& payload) {
    const QJsonArray rows = payload.value("rows").toArray();
    const QColor accent = colors::legible_color(QColor(kGovDataCongressColor));
    gov::TableFill fill(summary_table_, int(rows.size()));
    for (int row = 0; row < rows.size(); ++row) {
        const QJsonObject r = rows[row].toObject();
        auto* name_item = new QTableWidgetItem(r.value("type").toString().toUpper());
        name_item->setForeground(accent);
        summary_table_->setItem(row, 0, name_item);
        auto* count_item = new QTableWidgetItem(fmt::quantity(r.value("count").toInt()));
        count_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        summary_table_->setItem(row, 1, count_item);
    }
    LOG_DEBUG("GovCongress", "Summary loaded");
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void GovDataCongressPanel::update_toolbar_state() {
    bills_btn_->setChecked(current_view_ == Bills);
    summary_btn_->setChecked(current_view_ == SummaryView);
    back_btn_->setVisible(current_view_ == BillDetail);
    bill_type_combo_->setEnabled(current_view_ != SummaryView);
    type_lbl_->setEnabled(current_view_ != SummaryView);
}

void GovDataCongressPanel::on_export_csv() {
    QWidget* page = content_stack_->currentWidget();
    if (page == summary_table_)
        export_table_to_csv(summary_table_, "congress_summary.csv", this);
    else if (page == bills_table_)
        export_table_to_csv(bills_table_, "congress_bills.csv", this);
}

} // namespace fincept::screens
