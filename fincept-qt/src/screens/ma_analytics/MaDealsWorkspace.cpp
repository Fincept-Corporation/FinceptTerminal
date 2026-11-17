// src/screens/ma_analytics/MaDealsWorkspace.cpp
#include "screens/ma_analytics/MaDealsWorkspace.h"

#include "core/logging/Logger.h"
#include "screens/ma_analytics/MaDealsModel.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/AmountInput.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <optional>

namespace fincept::screens {

using fincept::services::ma::MAAnalyticsService;

// ── Deals filter: free text over every column + verified / unverified ───────
class MaDealsProxy : public QSortFilterProxyModel {
  public:
    enum Show { All = 0, Unverified = 1, Verified = 2 };
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void set_show(int show) {
        show_ = show;
        invalidateFilter();
    }
    void set_text(const QString& text) {
        text_ = text.trimmed();
        invalidateFilter();
    }

  protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        const QAbstractItemModel* m = sourceModel();
        if (!m)
            return true;
        if (show_ != All) {
            // Column 0 is the verified badge; its sort key is 1 / 0.
            const bool verified = m->index(row, 0, parent).data(MaDealsModel::kSortRole).toInt() == 1;
            if ((show_ == Unverified && verified) || (show_ == Verified && !verified))
                return false;
        }
        if (text_.isEmpty())
            return true;
        for (int c = 0; c < m->columnCount(parent); ++c)
            if (m->index(row, c, parent).data(Qt::DisplayRole).toString().contains(text_, Qt::CaseInsensitive))
                return true;
        return false;
    }

  private:
    int show_ = All;
    QString text_;
};

namespace {

// Form types offered by the scan (key = EDGAR form, label = what it is).
struct MaFormChoice {
    const char* form;
    const char* label;
    bool on;
};
const MaFormChoice kMaScanForms[] = {
    {"8-K", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "8-K (items 1.01 / 2.01)"), true},
    {"S-4", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "S-4 (stock deals)"), true},
    {"DEFM14A", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "DEFM14A (merger proxy)"), true},
    {"PREM14A", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "PREM14A (preliminary proxy)"), true},
    {"425", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "425 (deal communications)"), true},
    {"SC TO-T", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "SC TO-T (tender offer)"), true},
    {"SC 14D9", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "SC 14D9 (target response)"), true},
    {"SC 13E-3", QT_TRANSLATE_NOOP("fincept::screens::MaDealsWorkspace", "SC 13E-3 (going private)"), true},
};

/// The deals database stores every amount in US dollars.
const QString kMaWsDealCcy = QStringLiteral("USD");

/// A stored amount as the edit dialog shows it ("$68.73B" in the user's own number and currency
/// format), so that ui::formatting::read_amount_input reads it back; an untouched field keeps the
/// stored value exactly (the save compares the text with this one).
QString ma_ws_amount_text(const QJsonValue& v) {
    if (!v.isDouble())
        return v.isString() ? v.toString() : QString();
    return fincept::ui::formatting::money(v.toDouble(), kMaWsDealCcy, fincept::ui::formatting::Compact::FromSettings);
}

/// A stored multiple / percentage as the dialog shows it ("8.7" / "8,7").
QString ma_ws_number_text(const QJsonValue& v) {
    if (!v.isDouble())
        return v.isString() ? v.toString() : QString();
    return fincept::ui::formatting::amount_input_text(v.toDouble(), 4);
}

QString ma_ws_plain(const QJsonValue& v) {
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 12);
    return v.isString() ? v.toString() : QString();
}

} // namespace

// ── Construction / styling ───────────────────────────────────────────────────

MaDealsWorkspace::MaDealsWorkspace(QColor accent, std::function<void(const QString&)> set_status, QWidget* host)
    : QObject(host), accent_(std::move(accent)), set_status_(std::move(set_status)), host_(host) {
    auto& svc = MAAnalyticsService::instance();
    connect(&svc, &MAAnalyticsService::deals_loaded, this, &MaDealsWorkspace::on_deals_loaded);
    connect(&svc, &MAAnalyticsService::filings_loaded, this, &MaDealsWorkspace::on_filings_loaded);
    connect(&svc, &MAAnalyticsService::deal_scan_progress, this, &MaDealsWorkspace::on_scan_progress);
    connect(&svc, &MAAnalyticsService::deal_scan_finished, this, &MaDealsWorkspace::on_scan_finished);
    connect(&svc, &MAAnalyticsService::deal_scan_failed, this, &MaDealsWorkspace::on_scan_failed);
    connect(&svc, &MAAnalyticsService::deal_saved, this, &MaDealsWorkspace::on_deal_saved);
    connect(&svc, &MAAnalyticsService::deal_removed, this, &MaDealsWorkspace::on_deal_removed);
    connect(&svc, &MAAnalyticsService::deal_extracted, this, &MaDealsWorkspace::on_deal_extracted);
    connect(&svc, &MAAnalyticsService::deal_op_failed, this, &MaDealsWorkspace::on_op_failed);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { restyle(); });
}

QString MaDealsWorkspace::page_stylesheet(const QString& object_name) const {
    const QString s = QStringLiteral("#") + object_name;
    const QString accent = ui::colors::legible(accent_);
    QString css;
    css += QString("%1 QLabel { color:%2; font-family:%3; font-size:%4px; background:transparent; }")
               .arg(s, ui::colors::TEXT_SECONDARY(), ui::fonts::DATA_FAMILY())
               .arg(ui::fonts::px(-1));
    css += QString("%1 QLabel#maWarn { color:%2; } %1 QLabel#maError { color:%3; } %1 QLabel#maOk { color:%4; }")
               .arg(s, ui::colors::WARNING(), ui::colors::DANGER(), ui::colors::SUCCESS());
    css += QString("%1 QLabel#maEmpty { color:%2; padding:18px; border:1px dashed %3; }")
               .arg(s, ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED());
    css += QString("%1 QLineEdit, %1 QComboBox, %1 QSpinBox, %1 QPlainTextEdit { background:%2; color:%3;"
                   " border:1px solid %4; padding:4px 6px; font-family:%5; font-size:%6px; }"
                   "%1 QLineEdit:focus, %1 QComboBox:focus, %1 QSpinBox:focus, %1 QPlainTextEdit:focus"
                   " { border-color:%7; }")
               .arg(s, ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                    ui::fonts::DATA_FAMILY())
               .arg(ui::fonts::px(-1))
               .arg(accent);
    css += QString("%1 QCheckBox { color:%2; font-family:%3; font-size:%4px; spacing:6px; background:transparent; }")
               .arg(s, ui::colors::TEXT_PRIMARY(), ui::fonts::DATA_FAMILY())
               .arg(ui::fonts::px(-1));
    css += QString("%1 QPushButton { background:%2; color:%3; border:1px solid %4; padding:6px 12px;"
                   " font-family:%5; font-size:%6px; font-weight:600; }"
                   "%1 QPushButton:hover { background:%7; }"
                   "%1 QPushButton:disabled { color:%8; border-color:%9; }")
               .arg(s, ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                    ui::fonts::DATA_FAMILY())
               .arg(ui::fonts::px(-2))
               .arg(ui::colors::BG_HOVER(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM());
    css += QString("%1 QPushButton#maPrimary { background:%2; color:%3; border:none; font-weight:700; }"
                   "%1 QPushButton#maPrimary:hover { background:%4; }"
                   "%1 QPushButton#maPrimary:disabled { background:%5; color:%6; }")
               .arg(s, accent, ui::colors::TEXT_ON_ACCENT(), ui::colors::legible(accent_.lighter(115)),
                    ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY());
    css += QString("%1 QTableView { background:%2; alternate-background-color:%3; color:%4; border:1px solid %5;"
                   " gridline-color:%5; selection-background-color:%6; selection-color:%4;"
                   " font-family:%7; font-size:%8px; }"
                   "%1 QHeaderView::section { background:%9; color:%10; border:none;"
                   " border-right:1px solid %5; border-bottom:1px solid %5; padding:4px 6px; font-weight:700; }")
               .arg(s, ui::colors::BG_BASE(), ui::colors::ROW_ALT(), ui::colors::TEXT_PRIMARY(),
                    ui::colors::BORDER_DIM(), ui::colors::BG_HOVER(), ui::fonts::DATA_FAMILY())
               .arg(ui::fonts::px(-1))
               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY());
    css += QString("%1 QProgressBar { background:%2; border:1px solid %3; color:%4; text-align:center;"
                   " font-family:%5; font-size:%6px; }"
                   "%1 QProgressBar::chunk { background:%7; }")
               .arg(s, ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
                    ui::fonts::DATA_FAMILY())
               .arg(ui::fonts::px(-2))
               .arg(ui::colors::tint(accent_, 0.45));
    return css;
}

void MaDealsWorkspace::restyle() {
    if (deals_page_)
        deals_page_->setStyleSheet(page_stylesheet(deals_page_->objectName()));
    if (filings_page_)
        filings_page_->setStyleSheet(page_stylesheet(filings_page_->objectName()));
    if (deals_model_)
        deals_model_->refresh_colors();
    if (filings_model_)
        filings_model_->refresh_colors();
}

void MaDealsWorkspace::set_note(QLabel* label, const QString& text, const char* tone) {
    if (!label)
        return;
    label->setObjectName(QString::fromLatin1(tone));
    label->setText(text);
    label->setVisible(!text.isEmpty());
    // The object name selects the tone rule of the page stylesheet: re-polish to apply it.
    label->style()->unpolish(label);
    label->style()->polish(label);
}

namespace {

QTableView* ma_ws_make_table(QWidget* parent, QAbstractItemModel* model) {
    auto* view = new QTableView(parent);
    view->setModel(model);
    view->setSortingEnabled(true);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setAlternatingRowColors(true);
    view->setWordWrap(false);
    view->verticalHeader()->setVisible(false);
    view->horizontalHeader()->setStretchLastSection(true);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->horizontalHeader()->setDefaultSectionSize(ui::metrics::width(110));
    ui::metrics::follow_row_height(view, 26);
    view->setMinimumHeight(ui::metrics::height(360));
    return view;
}

QPushButton* ma_ws_button(const QString& text, QWidget* parent, bool primary = false) {
    auto* b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    if (primary)
        b->setObjectName(QStringLiteral("maPrimary"));
    return b;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// DEALS PAGE
// ═════════════════════════════════════════════════════════════════════════════

void MaDealsWorkspace::build_deals_page(QWidget* page) {
    deals_page_ = page;
    page->setObjectName(QStringLiteral("maDealsPage"));
    page->setStyleSheet(page_stylesheet(page->objectName())); // before any child exists

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    deals_info_ = new QLabel(page);
    deals_info_->setWordWrap(true);
    deals_info_->setText(tr("Loading deals..."));
    vl->addWidget(deals_info_);

    // Filter + actions
    auto* bar = new QWidget(page);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    filter_edit_ = new QLineEdit(bar);
    filter_edit_->setPlaceholderText(tr("Filter - acquirer, target, industry, status..."));
    filter_edit_->setClearButtonEnabled(true);
    hl->addWidget(filter_edit_, 1);
    show_combo_ = new QComboBox(bar);
    show_combo_->addItem(tr("All deals"), MaDealsProxy::All);
    show_combo_->addItem(tr("Unverified only"), MaDealsProxy::Unverified);
    show_combo_->addItem(tr("Verified only"), MaDealsProxy::Verified);
    hl->addWidget(show_combo_);
    auto* add_btn = ma_ws_button(tr("ADD DEAL"), bar, true);
    edit_btn_ = ma_ws_button(tr("EDIT"), bar);
    verify_btn_ = ma_ws_button(tr("MARK VERIFIED"), bar);
    delete_btn_ = ma_ws_button(tr("DELETE"), bar);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the deal
    export_btn_ = ma_ws_button(tr("EXPORT CSV"), bar);
    auto* reload_btn = ma_ws_button(tr("RELOAD"), bar);
    for (QPushButton* b : {add_btn, edit_btn_, verify_btn_, delete_btn_, export_btn_, reload_btn})
        hl->addWidget(b);
    vl->addWidget(bar);

    deals_note_ = new QLabel(page);
    deals_note_->setWordWrap(true);
    deals_note_->hide();
    vl->addWidget(deals_note_);

    const QVector<MaColumn> cols = {
        {QStringLiteral("verified"), tr("Status"), MaColumn::Kind::Verified},
        {QStringLiteral("announcement_date"), tr("Announced"), MaColumn::Kind::Date},
        {QStringLiteral("acquirer_name"), tr("Acquirer"), MaColumn::Kind::Text},
        {QStringLiteral("target_name"), tr("Target"), MaColumn::Kind::Text},
        {QStringLiteral("deal_value"), tr("Value"), MaColumn::Kind::Money},
        {QStringLiteral("ev_revenue"), tr("EV/Rev"), MaColumn::Kind::Multiple},
        {QStringLiteral("ev_ebitda"), tr("EV/EBITDA"), MaColumn::Kind::Multiple},
        {QStringLiteral("premium_1day"), tr("Prem. 1D"), MaColumn::Kind::Percent},
        {QStringLiteral("cash_percentage"), tr("Cash %"), MaColumn::Kind::Percent},
        {QStringLiteral("industry"), tr("Industry"), MaColumn::Kind::Text},
        {QStringLiteral("deal_status"), tr("Deal status"), MaColumn::Kind::Text},
        {QStringLiteral("source"), tr("Source"), MaColumn::Kind::Text},
    };
    deals_model_ = new MaDealsModel(cols, QStringLiteral("deal_id"), this);
    fincept::ui::formatting::refresh_model_on_format_change(deals_model_); // amounts follow the number format
    deals_proxy_ = new MaDealsProxy(this);
    deals_proxy_->setSourceModel(deals_model_);
    deals_proxy_->setSortRole(MaDealsModel::kSortRole);
    deals_view_ = ma_ws_make_table(page, deals_proxy_);
    deals_view_->sortByColumn(1, Qt::DescendingOrder);
    // Company names are what a deal is recognised by: room for them before the numeric columns.
    deals_view_->setColumnWidth(2, ui::metrics::width(200));
    deals_view_->setColumnWidth(3, ui::metrics::width(200));
    vl->addWidget(deals_view_, 1);

    // While the database loads: the shape of the table, not an empty grid.
    deals_loading_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, page);
    deals_loading_->set_message(tr("Loading the deal database…"));
    deals_loading_->set_row_count(8);
    deals_view_->hide();
    vl->addWidget(deals_loading_, 1);

    // A database with no deal yet: say so, with the two ways to fill it. Same stretch as the table it
    // stands in for: with the table hidden nothing else may take the spare height (the info line and the
    // toolbar would drift to the middle of the page).
    deals_empty_ = new ui::EmptyState(page);
    deals_empty_->hide();
    vl->addWidget(deals_empty_, 1);
    set_deals_empty(false, QString());
    connect(deals_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (deals_failed_) {
            reload_deals();
            return;
        }
        open_deal_dialog(QJsonObject(), tr("Enter a deal you know. Saved deals count as verified and are used by "
                                           "Precedent Txns and Regression."));
    });
    connect(deals_empty_, &ui::EmptyState::secondary_clicked, this, [this]() { show_filings_tab(); });

    // Search as you type, after a 300 ms pause.
    filter_timer_ = new QTimer(this);
    filter_timer_->setSingleShot(true);
    filter_timer_->setInterval(300);
    connect(filter_timer_, &QTimer::timeout, this, [this]() {
        deals_proxy_->set_text(filter_edit_->text());
        update_deals_info();
    });
    connect(filter_edit_, &QLineEdit::textChanged, filter_timer_, qOverload<>(&QTimer::start));
    connect(show_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        deals_proxy_->set_show(show_combo_->currentData().toInt());
        update_deals_info();
    });
    connect(deals_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { update_deal_buttons(); });
    connect(deals_view_, &QTableView::doubleClicked, this, [this]() {
        const QJsonObject d = selected_deal();
        if (!d.isEmpty())
            open_deal_dialog(d, QString());
    });
    connect(add_btn, &QPushButton::clicked, this, [this]() {
        open_deal_dialog(QJsonObject(), tr("Enter a deal you know. Saved deals count as verified and are used by "
                                           "Precedent Txns and Regression."));
    });
    connect(edit_btn_, &QPushButton::clicked, this, [this]() {
        const QJsonObject d = selected_deal();
        if (!d.isEmpty())
            open_deal_dialog(d, QString());
    });
    connect(verify_btn_, &QPushButton::clicked, this, [this]() { toggle_verified(); });
    connect(delete_btn_, &QPushButton::clicked, this, [this]() { delete_selected(); });
    connect(export_btn_, &QPushButton::clicked, this, [this]() { export_csv(); });
    connect(reload_btn, &QPushButton::clicked, this, [this]() { reload_deals(); });

    update_deal_buttons();
    reload_deals();
}

void MaDealsWorkspace::reload_deals() {
    if (!deals_model_)
        return;
    deals_req_ = MAAnalyticsService::instance().load_deals();
    if (deals_failed_)
        set_deals_empty(false, QString());
    update_deals_info(); // the loading skeleton while the table is still empty
    if (set_status_)
        set_status_(tr("Loading deals..."));
}

void MaDealsWorkspace::set_deals_empty(bool failed, const QString& message) {
    deals_failed_ = failed;
    if (!deals_empty_)
        return;
    if (failed) {
        deals_empty_->set_content(QStringLiteral("!"), tr("Could not load the deal database"), message,
                                  tr("RELOAD"));
        deals_empty_->set_glyph(QStringLiteral("!"), ui::EmptyState::Tone::Danger);
        deals_empty_->set_secondary_action(QString());
        return;
    }
    deals_empty_->set_content(QStringLiteral("◇"), tr("No deals in your database yet"),
                              tr("Deals you enter, or extract from SEC filings, are kept here on this computer. "
                                 "Verified deals feed Precedent Txns and Regression."),
                              tr("ADD DEAL"), tr("Extracted deals start UNVERIFIED until you check them"),
                              ui::EmptyState::Tone::Info);
    deals_empty_->set_glyph(QStringLiteral("◇"), ui::EmptyState::Tone::Accent);
    deals_empty_->set_secondary_action(tr("SCAN SEC FILINGS"));
}

void MaDealsWorkspace::show_filings_tab() {
    // The sub-tab widget that holds the deals page; the Filings page is its other tab.
    for (QWidget* w = deals_page_ ? deals_page_->parentWidget() : nullptr; w; w = w->parentWidget()) {
        if (auto* tabs = qobject_cast<QTabWidget*>(w)) {
            for (int i = 0; i < tabs->count(); ++i)
                if (tabs->widget(i) != deals_page_) {
                    tabs->setCurrentIndex(i);
                    return;
                }
            return;
        }
    }
}

void MaDealsWorkspace::on_deals_loaded(quint64 id, const QJsonArray& deals) {
    if (id != deals_req_ || !deals_model_)
        return; // a newer load is on its way
    deals_req_ = 0;
    deals_model_->set_rows(deals);
    update_deals_info();
    update_deal_buttons();
    if (set_status_)
        set_status_(tr("%n deal(s)", nullptr, int(deals.size())));
}

void MaDealsWorkspace::update_deals_info() {
    if (!deals_model_ || !deals_info_)
        return;
    const int total = deals_model_->rowCount();
    const int verified = deals_model_->verified_count();
    const int shown = deals_proxy_ ? deals_proxy_->rowCount() : total;
    QString text = tr("%1 deal(s), %2 verified by you, %3 unverified.").arg(total).arg(verified).arg(total - verified);
    if (shown != total)
        text += QLatin1Char(' ') + tr("Showing %1.").arg(shown);
    if (total - verified > 0)
        text += QLatin1Char(' ') + tr("UNVERIFIED deals were extracted automatically from SEC filings and can be "
                                      "wrong: open the filing, then EDIT and save (or MARK VERIFIED) once the "
                                      "figures are right. Only verified deals feed Precedent Txns and Regression.");
    const bool loading = deals_req_ != 0 && total == 0;
    deals_info_->setText(loading ? tr("Loading deals...") : text);
    // One of: the table, the loading skeleton, the empty (or failed) state.
    if (deals_loading_)
        deals_loading_->setVisible(loading);
    if (deals_empty_)
        deals_empty_->setVisible(!loading && total == 0);
    deals_view_->setVisible(!loading && total > 0);
}

QJsonObject MaDealsWorkspace::selected_deal() const {
    if (!deals_view_ || !deals_view_->selectionModel())
        return {};
    const QModelIndexList rows = deals_view_->selectionModel()->selectedRows();
    if (rows.size() != 1)
        return {};
    return deals_model_->row_at(deals_proxy_->mapToSource(rows.first()).row());
}

void MaDealsWorkspace::update_deal_buttons() {
    const QJsonObject d = selected_deal();
    const bool one = !d.isEmpty();
    if (edit_btn_)
        edit_btn_->setEnabled(one);
    if (delete_btn_)
        delete_btn_->setEnabled(one);
    if (verify_btn_) {
        verify_btn_->setEnabled(one);
        verify_btn_->setText(one && d.value(QStringLiteral("verified")).toInt() == 1 ? tr("MARK UNVERIFIED")
                                                                                     : tr("MARK VERIFIED"));
    }
    if (export_btn_)
        export_btn_->setEnabled(deals_model_ && deals_model_->rowCount() > 0);
}

void MaDealsWorkspace::toggle_verified() {
    const QJsonObject d = selected_deal();
    const QString id = d.value(QStringLiteral("deal_id")).toString();
    if (id.isEmpty())
        return;
    const bool verify = d.value(QStringLiteral("verified")).toInt() != 1;
    op_reqs_.insert(MAAnalyticsService::instance().set_deal_verified(id, verify));
    if (set_status_)
        set_status_(verify ? tr("Marking verified...") : tr("Marking unverified..."));
}

void MaDealsWorkspace::delete_selected() {
    const QJsonObject d = selected_deal();
    const QString id = d.value(QStringLiteral("deal_id")).toString();
    if (id.isEmpty() || !host_)
        return;
    auto* box = new QMessageBox(QMessageBox::Question, tr("Delete deal"),
                                tr("Delete %1 / %2 from your deal database? The SEC filing stays in the filing list.")
                                    .arg(d.value(QStringLiteral("acquirer_name")).toString(),
                                         d.value(QStringLiteral("target_name")).toString()),
                                QMessageBox::Yes | QMessageBox::Cancel, host_->window());
    box->setAttribute(Qt::WA_DeleteOnClose);
    connect(box, &QMessageBox::finished, this, [this, id](int result) {
        if (result != QMessageBox::Yes)
            return;
        op_reqs_.insert(MAAnalyticsService::instance().delete_deal(id));
        if (set_status_)
            set_status_(tr("Deleting deal..."));
    });
    box->open();
}

void MaDealsWorkspace::export_csv() {
    if (!deals_model_ || deals_model_->rowCount() == 0 || !host_)
        return;
    const QString suggested = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                                  .filePath(QStringLiteral("ma_deals.csv"));
    QString path = QFileDialog::getSaveFileName(host_->window(), tr("Export deals"), suggested, tr("CSV file (*.csv)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");

    // Raw values (not the table's display formatting), in the table's current order and filter.
    static const QList<QPair<const char*, const char*>> kCols = {
        {"verified", QT_TR_NOOP("Verified")},
        {"announcement_date", QT_TR_NOOP("Announced")},
        {"acquirer_name", QT_TR_NOOP("Acquirer")},
        {"acquirer_ticker", QT_TR_NOOP("Acquirer ticker")},
        {"target_name", QT_TR_NOOP("Target")},
        {"target_ticker", QT_TR_NOOP("Target ticker")},
        {"deal_value", QT_TR_NOOP("Deal value (USD)")},
        {"ev_revenue", QT_TR_NOOP("EV/Revenue")},
        {"ev_ebitda", QT_TR_NOOP("EV/EBITDA")},
        {"target_revenue", QT_TR_NOOP("Target revenue (USD)")},
        {"target_ebitda", QT_TR_NOOP("Target EBITDA (USD)")},
        {"premium_1day", QT_TR_NOOP("Premium 1-day %")},
        {"cash_percentage", QT_TR_NOOP("Cash %")},
        {"stock_percentage", QT_TR_NOOP("Stock %")},
        {"payment_method", QT_TR_NOOP("Payment")},
        {"industry", QT_TR_NOOP("Industry")},
        {"deal_type", QT_TR_NOOP("Deal type")},
        {"deal_status", QT_TR_NOOP("Deal status")},
        {"source", QT_TR_NOOP("Source")},
        {"verified_at", QT_TR_NOOP("Verified at")},
        {"filing_url", QT_TR_NOOP("Filing")},
        {"notes", QT_TR_NOOP("Notes")},
        {"deal_id", QT_TR_NOOP("Deal ID")},
    };
    QStringList headers;
    for (const auto& c : kCols)
        headers << tr(c.second);
    QVector<QStringList> rows;
    rows.reserve(deals_proxy_->rowCount());
    for (int r = 0; r < deals_proxy_->rowCount(); ++r) {
        const QJsonObject d = deals_model_->row_at(deals_proxy_->mapToSource(deals_proxy_->index(r, 0)).row());
        QStringList row;
        for (const auto& c : kCols) {
            const QString key = QString::fromLatin1(c.first);
            if (key == QLatin1String("verified"))
                row << (d.value(key).toInt() == 1 ? QStringLiteral("yes") : QStringLiteral("no"));
            else
                row << ma_ws_plain(d.value(key));
        }
        rows << row;
    }

    // The write runs off the GUI thread; the result is reported when it lands.
    auto* watcher = new QFutureWatcher<bool>(this);
    const int count = static_cast<int>(rows.size());
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, path, count]() {
        const bool ok = watcher->result();
        watcher->deleteLater();
        set_note(deals_note_,
                 ok ? tr("Exported %n deal(s) to %1.", nullptr, count).arg(QDir::toNativeSeparators(path))
                    : tr("Could not write %1.").arg(QDir::toNativeSeparators(path)),
                 ok ? "maOk" : "maError");
    });
    watcher->setFuture(QtConcurrent::run(
        [path, headers, rows]() { return ui::ChartExport::write_csv(path, headers, rows); }));
}

// ── Add / edit dialog ────────────────────────────────────────────────────────

void MaDealsWorkspace::open_deal_dialog(const QJsonObject& deal, const QString& intro) {
    if (!host_)
        return;
    const QString deal_id = deal.value(QStringLiteral("deal_id")).toString();
    auto* dlg = new QDialog(host_->window());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setObjectName(QStringLiteral("maDealDialog"));
    dlg->setWindowTitle(deal_id.isEmpty() ? tr("Add deal") : tr("Edit deal"));
    // Styled before any child exists; the dialog's own background through its object name.
    dlg->setStyleSheet(page_stylesheet(dlg->objectName()) +
                       QString("QDialog#maDealDialog { background:%1; }").arg(ui::colors::BG_SURFACE()));
    dlg->setMinimumWidth(ui::metrics::width(560));

    auto* vl = new QVBoxLayout(dlg);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(10);

    QString lead = intro;
    if (lead.isEmpty() && deal.value(QStringLiteral("verified")).toInt() != 1 && !deal_id.isEmpty())
        lead = tr("This deal was extracted automatically and is UNVERIFIED. Check each field against the filing; "
                  "saving marks it verified by you.");
    if (!lead.isEmpty()) {
        auto* intro_lbl = new QLabel(lead, dlg);
        intro_lbl->setObjectName(deal.value(QStringLiteral("verified")).toInt() == 1 || deal_id.isEmpty()
                                     ? QStringLiteral("maInfo")
                                     : QStringLiteral("maWarn"));
        intro_lbl->setWordWrap(true);
        vl->addWidget(intro_lbl);
    }
    const QString filing_url = deal.value(QStringLiteral("filing_url")).toString();
    if (filing_url.startsWith(QLatin1String("https://www.sec.gov/"))) {
        auto* link = new QLabel(dlg);
        link->setTextFormat(Qt::RichText);
        link->setOpenExternalLinks(true);
        link->setText(QStringLiteral("<a href=\"%1\" style=\"color:%2;\">%3</a>")
                          .arg(filing_url.toHtmlEscaped(), ui::colors::legible(accent_),
                               tr("Open the SEC filing")));
        vl->addWidget(link);
    }

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);
    vl->addLayout(form);

    enum class FieldKind { Text, Amount, Number, Date, Choice };
    struct FieldSpec {
        const char* key;
        QString label;
        FieldKind kind;
        QStringList choices;
        QString hint;
    };
    const QVector<FieldSpec> specs = {
        {"acquirer_name", tr("Acquirer *"), FieldKind::Text, {}, {}},
        {"acquirer_ticker", tr("Acquirer ticker"), FieldKind::Text, {}, {}},
        {"target_name", tr("Target *"), FieldKind::Text, {}, {}},
        {"target_ticker", tr("Target ticker"), FieldKind::Text, {}, {}},
        {"announcement_date", tr("Announced"), FieldKind::Date, {}, tr("YYYY-MM-DD")},
        // The deals database is USD: the unit follows the user's symbol style ("($)" / "(USD)").
        {"deal_value", fincept::ui::formatting::header(tr("Deal value"), QStringLiteral("USD")), FieldKind::Amount,
         {}, tr("e.g. 68.7B or 350M")},
        {"ev_revenue", tr("EV / Revenue (x)"), FieldKind::Number, {}, tr("e.g. 8.7")},
        {"ev_ebitda", tr("EV / EBITDA (x)"), FieldKind::Number, {}, tr("e.g. 23.1")},
        {"target_revenue", fincept::ui::formatting::header(tr("Target revenue"), QStringLiteral("USD")),
         FieldKind::Amount, {}, tr("LTM, optional")},
        {"target_ebitda", fincept::ui::formatting::header(tr("Target EBITDA"), QStringLiteral("USD")),
         FieldKind::Amount, {}, tr("LTM, optional")},
        {"premium_1day", tr("Premium, 1 day (%)"), FieldKind::Number, {}, tr("e.g. 45.3")},
        {"cash_percentage", tr("Cash (%)"), FieldKind::Number, {}, tr("0 - 100")},
        {"stock_percentage", tr("Stock (%)"), FieldKind::Number, {}, tr("0 - 100")},
        {"industry", tr("Industry"), FieldKind::Choice,
         {QString(), "Technology", "Healthcare", "Financial Services", "Industrials", "Consumer Discretionary",
          "Consumer Staples", "Energy", "Materials", "Real Estate", "Utilities", "Telecommunications", "Media"},
         {}},
        {"deal_type", tr("Deal type"), FieldKind::Choice,
         {"Acquisition", "Merger", "Takeover", "Asset Purchase", "Joint Venture", "Reverse Merger"}, {}},
        {"deal_status", tr("Deal status"), FieldKind::Choice,
         {"Announced", "Pending", "Completed", "Terminated", "Withdrawn"}, {}},
    };

    auto edits = std::make_shared<QHash<QString, QLineEdit*>>();
    auto shown = std::make_shared<QHash<QString, QString>>(); // the text each field opened with
    auto combos = std::make_shared<QHash<QString, QComboBox*>>();
    for (const FieldSpec& f : specs) {
        const QString key = QString::fromLatin1(f.key);
        const QJsonValue v = deal.value(key);
        if (f.kind == FieldKind::Choice) {
            auto* cb = new QComboBox(dlg);
            cb->setEditable(key == QLatin1String("industry"));
            cb->addItems(f.choices);
            const QString cur = v.toString();
            if (!cur.isEmpty()) {
                const int i = cb->findText(cur);
                if (i >= 0)
                    cb->setCurrentIndex(i);
                else if (cb->isEditable())
                    cb->setEditText(cur);
                else {
                    cb->addItem(cur);
                    cb->setCurrentIndex(cb->count() - 1);
                }
            }
            combos->insert(key, cb);
            form->addRow(f.label, cb);
            continue;
        }
        auto* le = new QLineEdit(dlg);
        le->setPlaceholderText(f.hint);
        le->setText(f.kind == FieldKind::Amount   ? ma_ws_amount_text(v)
                    : f.kind == FieldKind::Number ? ma_ws_number_text(v)
                                                  : ma_ws_plain(v));
        shown->insert(key, le->text());
        edits->insert(key, le);
        form->addRow(f.label, le);
    }
    auto* notes = new QPlainTextEdit(dlg);
    notes->setPlainText(deal.value(QStringLiteral("notes")).toString());
    notes->setMaximumHeight(ui::metrics::height(70));
    form->addRow(tr("Notes"), notes);

    auto* error = new QLabel(dlg);
    error->setObjectName(QStringLiteral("maError"));
    error->setWordWrap(true);
    error->hide();
    vl->addWidget(error);

    auto* hint = new QLabel(tr("Saving marks this deal as verified by you."), dlg);
    hint->setWordWrap(true);
    vl->addWidget(hint);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancel = ma_ws_button(tr("CANCEL"), dlg);
    auto* save = ma_ws_button(tr("SAVE"), dlg, true);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    vl->addLayout(buttons);
    // Enter in a field saves. Without this the dialog's default button is the first one created
    // (CANCEL), and Enter silently threw the typed deal away.
    cancel->setAutoDefault(false);
    save->setAutoDefault(true);
    save->setDefault(true);

    connect(cancel, &QPushButton::clicked, dlg, &QDialog::reject);
    QHash<QString, FieldKind> kinds;
    QHash<QString, QString> labels;
    for (const FieldSpec& f : specs) {
        kinds.insert(QString::fromLatin1(f.key), f.kind);
        labels.insert(QString::fromLatin1(f.key), f.label);
    }
    const QJsonObject original = deal;
    connect(save, &QPushButton::clicked, this, [this, dlg, edits, shown, combos, notes, error, kinds, labels, original,
                                                 deal_id]() {
        auto fail = [error](QWidget* field, const QString& message) {
            error->setText(message);
            error->show();
            if (field)
                field->setFocus();
        };
        QJsonObject fields;
        if (!deal_id.isEmpty())
            fields.insert(QStringLiteral("deal_id"), deal_id);
        for (const char* required : {"acquirer_name", "target_name"}) {
            QLineEdit* le = edits->value(QString::fromLatin1(required));
            if (le && le->text().trimmed().isEmpty()) {
                fail(le, qstrcmp(required, "acquirer_name") == 0 ? tr("Enter the acquirer's name.")
                                                                  : tr("Enter the target's name."));
                return;
            }
        }
        for (auto it = edits->constBegin(); it != edits->constEnd(); ++it) {
            const QString key = it.key();
            const QString text = it.value()->text().trimmed();
            const FieldKind kind = kinds.value(key);
            if (text.isEmpty()) {
                // Cleared on an existing deal: store "unknown" rather than keep the old value.
                if (!deal_id.isEmpty() && original.contains(key) && !original.value(key).isNull())
                    fields.insert(key, QJsonValue(QJsonValue::Null));
                continue;
            }
            if (kind == FieldKind::Amount || kind == FieldKind::Number) {
                // Untouched: keep the stored figure exactly (the field shows it rounded).
                if (text == shown->value(key).trimmed() && original.value(key).isDouble()) {
                    fields.insert(key, original.value(key).toDouble());
                    continue;
                }
                // The shared reader: the user's separators, K / M / B, lakh / crore, symbols; a deal
                // amount in another currency is refused with both currencies named, never converted.
                const fincept::ui::formatting::AmountInput a =
                    kind == FieldKind::Amount ? fincept::ui::formatting::read_amount_input(text, kMaWsDealCcy)
                                              : fincept::ui::formatting::read_number_input(text);
                if (!a.ok()) {
                    fail(it.value(), tr("%1: %2").arg(labels.value(key), a.error));
                    return;
                }
                fields.insert(key, *a.value);
            } else if (kind == FieldKind::Date) {
                const QDate d = QDate::fromString(text, Qt::ISODate);
                if (!d.isValid()) {
                    fail(it.value(), tr("Enter the announcement date as YYYY-MM-DD."));
                    return;
                }
                fields.insert(key, d.toString(Qt::ISODate));
            } else {
                fields.insert(key, text);
            }
        }
        for (auto it = combos->constBegin(); it != combos->constEnd(); ++it) {
            const QString text = it.value()->currentText().trimmed();
            if (!text.isEmpty())
                fields.insert(it.key(), text);
            else if (!deal_id.isEmpty() && original.contains(it.key()))
                fields.insert(it.key(), QJsonValue(QJsonValue::Null));
        }
        const QString note_text = notes->toPlainText().trimmed();
        if (!note_text.isEmpty() || (!deal_id.isEmpty() && original.contains(QStringLiteral("notes"))))
            fields.insert(QStringLiteral("notes"), note_text.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                                       : QJsonValue(note_text));
        if (deal_id.isEmpty()) {
            const QString url = original.value(QStringLiteral("filing_url")).toString();
            if (!url.isEmpty()) {
                fields.insert(QStringLiteral("filing_url"), url);
                fields.insert(QStringLiteral("source"), QStringLiteral("SEC filing (completed by you)"));
            }
        }
        fields.insert(QStringLiteral("verified"), true);
        op_reqs_.insert(MAAnalyticsService::instance().save_deal(fields));
        if (set_status_)
            set_status_(tr("Saving deal..."));
        dlg->accept();
    });
    dlg->open();
}

// ── Replies for single-deal operations ───────────────────────────────────────

void MaDealsWorkspace::on_deal_saved(quint64 id, const QJsonObject& deal) {
    if (!op_reqs_.remove(id))
        return;
    const QString deal_id = deal.value(QStringLiteral("deal_id")).toString();
    if (deals_model_ && deal.contains(QStringLiteral("acquirer_name"))) {
        deals_model_->upsert_row(deal); // a full row (save)
    } else if (deals_model_) {
        // A verify toggle answers with the id and the flag only.
        const int r = deals_model_->row_of(deal_id);
        if (r >= 0) {
            QJsonObject row = deals_model_->row_at(r);
            row.insert(QStringLiteral("verified"), deal.value(QStringLiteral("verified")).toInt());
            if (row.value(QStringLiteral("verified")).toInt() == 1)
                row.insert(QStringLiteral("quality_issues"), QJsonArray());
            deals_model_->upsert_row(row);
        }
    }
    update_deals_info();
    update_deal_buttons();
    // Saved from a filing's draft: the filing is now tied to this deal — show that on the Filings tab and
    // drop its "complete the deal yourself" line.
    if (!deal.value(QStringLiteral("filing_url")).toString().isEmpty() && filings_model_) {
        if (scan_req_ == 0) // a running scan keeps its progress line
            set_note(scan_note_, QString(), "maInfo");
        reload_filings();
    }
    const bool verified = deal.value(QStringLiteral("verified")).toInt() == 1;
    set_note(deals_note_, verified ? tr("Saved - marked verified by you.") : tr("Marked unverified."),
             verified ? "maOk" : "maWarn");
    if (set_status_)
        set_status_(tr("Saved"));
}

void MaDealsWorkspace::on_deal_removed(quint64 id, const QString& deal_id) {
    if (!op_reqs_.remove(id))
        return;
    if (deals_model_)
        deals_model_->remove_id(deal_id);
    update_deals_info();
    update_deal_buttons();
    set_note(deals_note_, tr("Deal deleted."), "maOk");
    if (set_status_)
        set_status_(tr("Deleted"));
}

void MaDealsWorkspace::on_op_failed(quint64 id, const QString& message, const QStringList& missing_inputs) {
    Q_UNUSED(missing_inputs); // the dialog checks the required fields before it saves
    if (id == deals_req_) {
        deals_req_ = 0;
        set_note(deals_note_, tr("Could not load the deal database: %1").arg(message), "maError");
        if (deals_model_ && deals_model_->rowCount() == 0)
            set_deals_empty(true, message);
        update_deals_info();
    } else if (id == filings_req_) {
        filings_req_ = 0;
        set_note(scan_note_, tr("Could not load the stored filings: %1").arg(message), "maError");
        update_filings_view();
    } else if (id == extract_req_) {
        extract_req_ = 0;
        update_filing_buttons();
        set_note(scan_note_, message, "maError");
    } else if (op_reqs_.remove(id)) {
        set_note(deals_note_, message, "maError");
    } else {
        return;
    }
    if (set_status_)
        set_status_(tr("Error"));
}

// ═════════════════════════════════════════════════════════════════════════════
// FILINGS PAGE
// ═════════════════════════════════════════════════════════════════════════════

void MaDealsWorkspace::build_filings_page(QWidget* page) {
    filings_page_ = page;
    page->setObjectName(QStringLiteral("maFilingsPage"));
    page->setStyleSheet(page_stylesheet(page->objectName())); // before any child exists

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* about = new QLabel(tr("Finds M&A filings on SEC EDGAR (free full-text search; at most 8 requests a second, "
                                "as SEC's fair-access rules ask). Deals extracted from them are stored as UNVERIFIED."),
                             page);
    about->setWordWrap(true);
    vl->addWidget(about);

    // Form types
    auto* forms_box = new QWidget(page);
    // Four to a row: all eight in one row are wider than the page, which then scrolls sideways and
    // clips the text above and the RELOAD button.
    auto* fg = new QGridLayout(forms_box);
    fg->setContentsMargins(0, 0, 0, 0);
    fg->setHorizontalSpacing(14);
    fg->setVerticalSpacing(4);
    fg->addWidget(new QLabel(tr("Forms:"), forms_box), 0, 0);
    constexpr int kFormsPerRow = 4;
    int form_index = 0;
    for (const MaFormChoice& f : kMaScanForms) {
        auto* cb = new QCheckBox(tr(f.label), forms_box);
        cb->setChecked(f.on);
        form_checks_.append({QString::fromLatin1(f.form), cb});
        fg->addWidget(cb, form_index / kFormsPerRow, 1 + form_index % kFormsPerRow);
        ++form_index;
    }
    fg->setColumnStretch(1 + kFormsPerRow, 1);
    vl->addWidget(forms_box);

    // Window, limit, scan / cancel
    auto* ctl = new QWidget(page);
    auto* hl = new QHBoxLayout(ctl);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(new QLabel(tr("Last"), ctl));
    days_spin_ = new QSpinBox(ctl);
    days_spin_->setRange(1, 90);
    days_spin_->setValue(30);
    days_spin_->setSuffix(tr(" days"));
    hl->addWidget(days_spin_);
    hl->addWidget(new QLabel(tr("Read up to"), ctl));
    max_spin_ = new QSpinBox(ctl);
    max_spin_->setRange(0, 200);
    max_spin_->setValue(40);
    max_spin_->setSuffix(tr(" filings"));
    max_spin_->setToolTip(tr("How many high-confidence filings to read for deal terms (each one is a download from "
                             "SEC). 0 = only list the filings."));
    hl->addWidget(max_spin_);
    scan_btn_ = ma_ws_button(tr("SCAN"), ctl, true);
    cancel_btn_ = ma_ws_button(tr("CANCEL"), ctl);
    cancel_btn_->setEnabled(false);
    hl->addWidget(scan_btn_);
    hl->addWidget(cancel_btn_);
    hl->addStretch();
    vl->addWidget(ctl);

    progress_ = new QProgressBar(page);
    progress_->setTextVisible(true);
    progress_->setFixedHeight(ui::metrics::height(16));
    progress_->hide();
    vl->addWidget(progress_);

    scan_note_ = new QLabel(page);
    scan_note_->setWordWrap(true);
    scan_note_->hide();
    vl->addWidget(scan_note_);

    // Stored filings
    auto* actions = new QWidget(page);
    auto* al = new QHBoxLayout(actions);
    al->setContentsMargins(0, 0, 0, 0);
    al->setSpacing(6);
    open_btn_ = ma_ws_button(tr("OPEN FILING"), actions);
    open_btn_->setToolTip(tr("Open the filing's index page on sec.gov in your browser"));
    extract_btn_ = ma_ws_button(tr("EXTRACT DEAL"), actions);
    extract_btn_->setToolTip(tr("Read this filing and store its deal as UNVERIFIED (or complete it yourself when "
                                "the filing does not name both parties clearly)"));
    auto* reload_btn = ma_ws_button(tr("RELOAD"), actions);
    al->addWidget(open_btn_);
    al->addWidget(extract_btn_);
    al->addStretch();
    al->addWidget(reload_btn);
    vl->addWidget(actions);

    const QVector<MaColumn> cols = {
        {QStringLiteral("filing_date"), tr("Filed"), MaColumn::Kind::Date},
        {QStringLiteral("filing_type"), tr("Form"), MaColumn::Kind::Text},
        {QStringLiteral("company_name"), tr("Company"), MaColumn::Kind::Text},
        {QStringLiteral("ticker"), tr("Ticker"), MaColumn::Kind::Text},
        {QStringLiteral("items"), tr("8-K items"), MaColumn::Kind::Text},
        {QStringLiteral("confidence_score"), tr("Confidence"), MaColumn::Kind::Score},
        {QStringLiteral("industry"), tr("Industry"), MaColumn::Kind::Text},
        {QStringLiteral("deal_id"), tr("Deal"), MaColumn::Kind::Parsed},
        {QStringLiteral("deal_indicators"), tr("Why it matched"), MaColumn::Kind::Text},
    };
    filings_model_ = new MaDealsModel(cols, QStringLiteral("accession_number"), this);
    auto* proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(filings_model_);
    proxy->setSortRole(MaDealsModel::kSortRole);
    filings_view_ = ma_ws_make_table(page, proxy);
    filings_view_->sortByColumn(0, Qt::DescendingOrder);
    filings_view_->setColumnWidth(2, ui::metrics::width(240)); // company
    filings_view_->setColumnWidth(7, ui::metrics::width(150)); // "Deal extracted"
    vl->addWidget(filings_view_, 1);

    filings_loading_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, page);
    filings_loading_->set_message(tr("Loading the stored filings…"));
    filings_loading_->set_row_count(8);
    vl->addWidget(filings_loading_, 1); // takes the table's place, and its stretch
    // No filing stored yet. The scan reads SEC EDGAR (rate-limited): it runs only on SCAN.
    filings_empty_ = new ui::EmptyState(page);
    filings_empty_->set_content(QStringLiteral("⌁"), tr("No SEC filings stored yet"),
                                tr("SCAN reads SEC EDGAR for recent M&A filings of the form types above, stores them "
                                   "on this computer and extracts their deals (UNVERIFIED until you check them)."),
                                tr("SCAN SEC FILINGS"), tr("SEC EDGAR · free, rate-limited · never started automatically"),
                                ui::EmptyState::Tone::Info);
    filings_empty_->set_glyph(QStringLiteral("⌁"), ui::EmptyState::Tone::Accent);
    filings_empty_->hide();
    vl->addWidget(filings_empty_, 1);
    connect(filings_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (scan_btn_ && scan_btn_->isEnabled())
            start_scan();
    });
    filings_view_->hide();

    connect(scan_btn_, &QPushButton::clicked, this, [this]() { start_scan(); });
    connect(cancel_btn_, &QPushButton::clicked, this, [this]() {
        MAAnalyticsService::instance().cancel_deal_scan();
        cancel_btn_->setEnabled(false);
        set_note(scan_note_, tr("Stopping the scan..."), "maInfo");
    });
    connect(open_btn_, &QPushButton::clicked, this, [this]() { open_selected_filing(); });
    connect(extract_btn_, &QPushButton::clicked, this, [this]() { extract_selected(); });
    connect(reload_btn, &QPushButton::clicked, this, [this]() { reload_filings(); });
    connect(filings_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { update_filing_buttons(); });
    connect(filings_view_, &QTableView::doubleClicked, this, [this]() { open_selected_filing(); });

    // A scan started earlier (this panel was rebuilt, or the user came back) keeps reporting here.
    auto& svc = MAAnalyticsService::instance();
    if (svc.deal_scan_running()) {
        scan_req_ = svc.deal_scan_request();
        set_scanning(true);
        set_note(scan_note_, tr("A scan is running..."), "maInfo");
    }
    update_filing_buttons();
    reload_filings();
}

void MaDealsWorkspace::reload_filings() {
    if (!filings_model_)
        return;
    filings_req_ = MAAnalyticsService::instance().load_filings();
    update_filings_view();
}

void MaDealsWorkspace::update_filings_view() {
    if (!filings_model_ || !filings_view_)
        return;
    const bool any = filings_model_->rowCount() > 0;
    const bool loading = filings_req_ != 0 && !any;
    if (filings_loading_)
        filings_loading_->setVisible(loading);
    if (filings_empty_)
        filings_empty_->setVisible(!loading && !any);
    filings_view_->setVisible(any);
}

void MaDealsWorkspace::on_filings_loaded(quint64 id, const QJsonArray& filings) {
    if (id != filings_req_ || !filings_model_)
        return;
    filings_req_ = 0;
    filings_model_->set_rows(filings);
    update_filings_view();
    update_filing_buttons();
}

void MaDealsWorkspace::start_scan() {
    QStringList forms;
    for (const auto& f : form_checks_)
        if (f.second->isChecked())
            forms << f.first;
    if (forms.isEmpty()) {
        set_note(scan_note_, tr("Choose at least one form type."), "maError");
        return;
    }
    if (extract_req_ != 0) {
        set_note(scan_note_, tr("Wait for the filing being read to finish, then scan."), "maWarn");
        return;
    }
    scan_req_ = MAAnalyticsService::instance().start_deal_scan(days_spin_->value(), max_spin_->value(), forms);
    set_scanning(true);
    progress_->setRange(0, 0); // busy until the first progress line
    set_note(scan_note_, tr("Searching SEC full-text index..."), "maInfo");
    if (set_status_)
        set_status_(tr("Scanning SEC EDGAR..."));
}

void MaDealsWorkspace::set_scanning(bool on) {
    if (scan_btn_)
        scan_btn_->setEnabled(!on);
    if (cancel_btn_)
        cancel_btn_->setEnabled(on);
    if (days_spin_)
        days_spin_->setEnabled(!on);
    if (max_spin_)
        max_spin_->setEnabled(!on);
    for (const auto& f : form_checks_)
        f.second->setEnabled(!on);
    if (progress_)
        progress_->setVisible(on);
    update_filing_buttons();
}

void MaDealsWorkspace::on_scan_progress(quint64 id, const QString& message, int done, int total, int filings,
                                        int deals_created) {
    if (id != scan_req_ || !progress_)
        return;
    if (total > 0) {
        progress_->setRange(0, total);
        progress_->setValue(std::min(done, total));
    }
    QString text = message;
    if (filings > 0)
        text += QStringLiteral("  ·  ") + tr("%1 filings, %2 new deals").arg(filings).arg(deals_created);
    set_note(scan_note_, text, "maInfo");
}

void MaDealsWorkspace::on_scan_finished(quint64 id, const QJsonObject& r) {
    if (id != scan_req_)
        return;
    scan_req_ = 0;
    set_scanning(false);
    const int found = r.value(QStringLiteral("filings_found")).toInt();
    const int created = r.value(QStringLiteral("deals_created")).toInt();
    const int updated = r.value(QStringLiteral("deals_updated")).toInt();
    const int rejected = r.value(QStringLiteral("deals_rejected")).toInt();
    const int failed = r.value(QStringLiteral("parsing_failed")).toInt();
    const int over = r.value(QStringLiteral("parse_skipped_over_limit")).toInt();
    QStringList parts;
    parts << tr("Scan finished in %1 s: %2 filings found, %3 new deals (UNVERIFIED).")
                 .arg(r.value(QStringLiteral("elapsed_s")).toDouble(), 0, 'f', 0)
                 .arg(found)
                 .arg(created);
    if (updated > 0)
        parts << tr("%1 unverified deals refreshed.").arg(updated);
    if (rejected > 0)
        parts << tr("%1 filings did not name both parties clearly and were not stored as deals - use EXTRACT DEAL "
                    "to complete one yourself.")
                     .arg(rejected);
    if (failed > 0)
        parts << tr("%1 filings could not be read.").arg(failed);
    if (over > 0)
        parts << tr("%1 more high-confidence filings were not read (limit) - raise \"Read up to\" or scan again.")
                     .arg(over);
    const QString warning = r.value(QStringLiteral("warning")).toString();
    if (!warning.isEmpty())
        parts << warning;
    set_note(scan_note_, parts.join(QLatin1Char(' ')), warning.isEmpty() ? "maOk" : "maWarn");
    if (set_status_)
        set_status_(tr("Scan finished"));
    reload_filings();
    if (deals_model_)
        reload_deals();
}

void MaDealsWorkspace::on_scan_failed(quint64 id, const QString& message, bool cancelled, int kept_filings) {
    if (id != scan_req_)
        return;
    scan_req_ = 0;
    set_scanning(false);
    if (cancelled)
        set_note(scan_note_, tr("Scan stopped. %n filing(s) kept.", nullptr, kept_filings), "maWarn");
    else
        set_note(scan_note_, message, "maError");
    if (set_status_)
        set_status_(cancelled ? tr("Scan stopped") : tr("Scan failed"));
    // Whatever was stored before it ended is kept: show it.
    reload_filings();
    if (deals_model_)
        reload_deals();
}

QJsonObject MaDealsWorkspace::selected_filing() const {
    if (!filings_view_ || !filings_view_->selectionModel())
        return {};
    const QModelIndexList rows = filings_view_->selectionModel()->selectedRows();
    if (rows.size() != 1)
        return {};
    auto* proxy = qobject_cast<QSortFilterProxyModel*>(filings_view_->model());
    return proxy ? filings_model_->row_at(proxy->mapToSource(rows.first()).row()) : QJsonObject();
}

void MaDealsWorkspace::update_filing_buttons() {
    const QJsonObject f = selected_filing();
    if (open_btn_)
        open_btn_->setEnabled(f.value(QStringLiteral("filing_url")).toString().startsWith(QLatin1String("https://")));
    // One SEC reader at a time keeps the terminal under SEC's 10-requests-a-second limit (each reader paces
    // itself to 8 a second).
    if (extract_btn_)
        extract_btn_->setEnabled(!f.isEmpty() && extract_req_ == 0 && scan_req_ == 0);
}

void MaDealsWorkspace::open_selected_filing() {
    const QString url = selected_filing().value(QStringLiteral("filing_url")).toString();
    // Only SEC's own pages: the URL comes from the local database.
    if (url.startsWith(QLatin1String("https://www.sec.gov/")))
        QDesktopServices::openUrl(QUrl(url));
}

void MaDealsWorkspace::extract_selected() {
    const QJsonObject f = selected_filing();
    const QString acc = f.value(QStringLiteral("accession_number")).toString();
    if (acc.isEmpty())
        return;
    extract_req_ = MAAnalyticsService::instance().extract_deal(acc, f.value(QStringLiteral("filing_type")).toString());
    update_filing_buttons();
    set_note(scan_note_, tr("Reading %1 (%2)...").arg(f.value(QStringLiteral("company_name")).toString(),
                                                     f.value(QStringLiteral("filing_type")).toString()),
             "maInfo");
}

void MaDealsWorkspace::on_deal_extracted(quint64 id, const QJsonObject& reply) {
    if (id != extract_req_)
        return;
    extract_req_ = 0;
    update_filing_buttons();
    const QString status = reply.value(QStringLiteral("status")).toString();
    const QJsonObject deal = reply.value(QStringLiteral("data")).toObject();
    if (status == QLatin1String("rejected")) {
        set_note(scan_note_, tr("This filing does not name both parties clearly - complete the deal yourself."),
                 "maWarn");
        QJsonObject draft = deal;
        draft.remove(QStringLiteral("deal_id"));
        open_deal_dialog(draft, tr("The filing could not be read reliably (%1). Fill in what is missing and check "
                                   "every pre-filled figure against the filing before you save - the deal is "
                                   "stored as verified by you.")
                                    .arg(reply.value(QStringLiteral("reason")).toString()));
        return;
    }
    if (status == QLatin1String("kept_verified")) {
        set_note(scan_note_, tr("You already verified this filing's deal - it was kept as you saved it."), "maOk");
        return;
    }
    set_note(scan_note_,
             tr("Deal stored as UNVERIFIED: %1 / %2. Check it on the Deals tab.")
                 .arg(deal.value(QStringLiteral("acquirer_name")).toString(),
                      deal.value(QStringLiteral("target_name")).toString()),
             "maOk");
    reload_filings();
    if (deals_model_)
        reload_deals();
}

} // namespace fincept::screens
