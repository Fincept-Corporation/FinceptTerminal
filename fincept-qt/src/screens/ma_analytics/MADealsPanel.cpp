// MADealsPanel.cpp — deal-database query sub-tab builder.
// Method definition split from MAModulePanel.cpp.

#include "screens/ma_analytics/MAModulePanel.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

using namespace fincept::services::ma;

namespace fincept::screens {

using namespace fincept::services::ma;

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 3: DEAL DATABASE
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_deals_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Controls row
    auto* ctl = new QWidget(w);
    auto* ctl_hl = new QHBoxLayout(ctl);
    ctl_hl->setContentsMargins(0, 0, 0, 0);
    ctl_hl->setSpacing(8);

    auto* scan_days = make_int_spin(1, 365, 30, ctl);
    int_inputs_["scan_days"] = scan_days;
    ctl_hl->addWidget(new QLabel(tr("Scan Days:"), ctl));
    ctl_hl->addWidget(scan_days);

    auto* scan_btn = make_run_button(tr("SCAN SEC FILINGS"), ctl);
    connect(scan_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText(tr("Scanning SEC EDGAR..."));
        MAAnalyticsService::instance().scan_filings(int_inputs_["scan_days"]->value());
    });
    ctl_hl->addWidget(scan_btn);

    auto* load_btn = make_run_button(tr("LOAD ALL DEALS"), ctl);
    connect(load_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText(tr("Loading deals..."));
        MAAnalyticsService::instance().get_all_deals();
    });
    ctl_hl->addWidget(load_btn);
    ctl_hl->addStretch();
    vl->addWidget(ctl);

    // Search
    auto* search_box = new QComboBox(w);
    search_box->setEditable(true);
    search_box->setPlaceholderText(tr("Search deals by target, acquirer, or industry..."));
    search_box->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:6px 10px; }")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::SMALL));
    combo_inputs_["deal_search"] = search_box;
    // Search on enter
    connect(search_box->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
        auto q = combo_inputs_["deal_search"]->currentText();
        if (!q.isEmpty()) {
            status_label_->setText(tr("Searching deals..."));
            MAAnalyticsService::instance().search_deals(q);
        }
    });
    vl->addWidget(search_box);

    // Parse one SEC filing into a structured deal (MAAnalyticsService::parse_filing). The parser
    // works on filings the scan has stored locally, so SCAN SEC FILINGS fills the picker below.
    auto* parse_row = new QWidget(w);
    auto* parse_hl = new QHBoxLayout(parse_row);
    parse_hl->setContentsMargins(0, 0, 0, 0);
    parse_hl->setSpacing(8);
    parse_hl->addWidget(new QLabel(tr("Filing:"), parse_row));

    auto* filing_box = new QComboBox(parse_row);
    filing_box->setEditable(true);
    filing_box->setInsertPolicy(QComboBox::NoInsert);
    filing_box->setMinimumWidth(320);
    filing_box->setPlaceholderText(tr("Accession no. (e.g. 0001193125-24-012345) - scan first"));
    filing_box->setToolTip(tr("Pick a filing found by SCAN SEC FILINGS, or paste its accession number / EDGAR URL"));
    combo_inputs_["parse_accession"] = filing_box;
    parse_hl->addWidget(filing_box, 1);

    auto* form_box = new QComboBox(parse_row);
    form_box->setEditable(true);
    form_box->addItems({"8-K", "DEFM14A", "PREM14A", "S-4", "SC TO-T", "SC 14D9", "425", "10-K"});
    form_box->setToolTip(tr("Filing form type"));
    combo_inputs_["parse_form_type"] = form_box;
    parse_hl->addWidget(form_box);

    // Picking a scanned filing also selects its form type.
    connect(filing_box, &QComboBox::activated, this, [filing_box, form_box](int index) {
        const QString form = filing_box->itemData(index, Qt::UserRole + 1).toString();
        if (form.isEmpty())
            return;
        const int fi = form_box->findText(form);
        if (fi >= 0)
            form_box->setCurrentIndex(fi);
        else
            form_box->setEditText(form);
    });

    auto* parse_btn = make_run_button(tr("PARSE FILING"), parse_row);
    connect(parse_btn, &QPushButton::clicked, this, [this, filing_box, form_box]() {
        // A picked item carries the bare accession number; typed text may be a full EDGAR URL.
        QString accession;
        const int idx = filing_box->currentIndex();
        if (idx >= 0 && filing_box->itemText(idx) == filing_box->currentText())
            accession = filing_box->itemData(idx).toString();
        if (accession.isEmpty()) {
            const QString typed = filing_box->currentText().trimmed();
            static const QRegularExpression kAccession(QStringLiteral("\\d{10}-\\d{2}-\\d{6}"));
            const QRegularExpressionMatch m = kAccession.match(typed);
            accession = m.hasMatch() ? m.captured(0) : typed;
        }
        const QString form = form_box->currentText().trimmed();
        if (accession.isEmpty()) {
            status_label_->setText(tr("Enter an accession number, or pick a filing after SCAN SEC FILINGS"));
            return;
        }
        status_label_->setText(tr("Parsing filing %1...").arg(accession));
        MAAnalyticsService::instance().parse_filing(accession, form.isEmpty() ? QStringLiteral("8-K") : form);
    });
    parse_hl->addWidget(parse_btn);
    vl->addWidget(parse_row);

    // Results area
    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 4: STARTUP VALUATION

} // namespace fincept::screens
