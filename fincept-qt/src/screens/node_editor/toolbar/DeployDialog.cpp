#include "screens/node_editor/toolbar/DeployDialog.h"

#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::workflow {

DeployDialog::DeployDialog(const QString& current_name, const QString& current_description, bool currently_live,
                           const QStringList& live_rows, QWidget* parent)
    : QDialog(parent), live_rows_(live_rows) {
    setObjectName("deployDialog");
    setWindowTitle(tr("Save & Run Workflow"));
    setMinimumSize(520, 380);
    // One stylesheet for the whole dialog (objectName selectors) instead of one per widget.
    setStyleSheet(
        QString("QDialog#deployDialog { background: %1; }"
                "QLabel { color: %2; font-family: Consolas; font-size: 11px; }"
                "QLabel#deployTitle { color: %3; font-size: 14px; font-weight: bold; letter-spacing: 0.5px; }"
                "QLabel#deploySection { color: %2; font-weight: bold; letter-spacing: 0.5px; }"
                "QLabel#deployLiveWarning { color: %4; font-weight: bold; }"
                "QFrame#deploySep { background: %5; border: none; }"
                "QLineEdit, QPlainTextEdit { background: %6; color: %7; border: 1px solid %5;"
                "  font-family: Consolas; font-size: 12px; padding: 6px 8px; }"
                "QLineEdit:focus, QPlainTextEdit:focus { border: 1px solid %3; }"
                "QPlainTextEdit#deployLiveList { color: %4; font-size: 11px; }"
                "QWidget#deployLivePanel { border: 1px solid %4; }"
                "QRadioButton { color: %7; font-family: Consolas; font-size: 12px; }"
                "QPushButton { background: %6; color: %2; border: 1px solid %5; font-family: Consolas;"
                "  font-size: 11px; font-weight: bold; padding: 0 16px; min-height: 26px; }"
                "QPushButton:hover { color: %7; }"
                "QPushButton:disabled { color: %8; }"
                "QPushButton#deployRunBtn { background: %9; color: %3; border: 1px solid %3; }"
                "QPushButton#deployRunBtn:hover { background: %3; color: %10; }"
                "QPushButton#deployRunBtn:disabled { background: %6; color: %8; border: 1px solid %5; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER(), ui::colors::NEGATIVE(),
                 ui::colors::BORDER_MED(), ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_DIM(),
                 ui::colors::ACCENT_BG())
            .arg(ui::colors::BG_BASE()));
    build_ui(current_name, current_description, currently_live);
}

QString DeployDialog::workflow_name() const {
    return name_edit_->text();
}
QString DeployDialog::workflow_description() const {
    return desc_edit_->toPlainText();
}

bool DeployDialog::live_mode() const {
    return live_radio_ && live_radio_->isChecked() && confirm_edit_ && confirm_edit_->text().trimmed() == "LIVE";
}

void DeployDialog::build_ui(const QString& current_name, const QString& current_description, bool currently_live) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    title_label_ = new QLabel;
    title_label_->setObjectName("deployTitle");
    root->addWidget(title_label_);

    auto* sep = new QFrame;
    sep->setObjectName("deploySep");
    sep->setFixedHeight(1);
    root->addWidget(sep);

    name_label_ = new QLabel;
    name_label_->setObjectName("deploySection");
    root->addWidget(name_label_);
    name_edit_ = new QLineEdit(current_name);
    root->addWidget(name_edit_);

    desc_label_ = new QLabel;
    desc_label_->setObjectName("deploySection");
    root->addWidget(desc_label_);
    desc_edit_ = new QPlainTextEdit;
    desc_edit_->setPlainText(current_description);
    desc_edit_->setMaximumHeight(64);
    root->addWidget(desc_edit_);

    // ── Execution mode ─────────────────────────────────────────────
    mode_label_ = new QLabel;
    mode_label_->setObjectName("deploySection");
    root->addWidget(mode_label_);

    paper_radio_ = new QRadioButton;
    live_radio_ = new QRadioButton;
    auto* group = new QButtonGroup(this);
    group->addButton(paper_radio_);
    group->addButton(live_radio_);
    // The dialog always opens on the workflow's saved mode, but LIVE must be
    // re-confirmed by typing every time it is kept or chosen.
    (currently_live ? live_radio_ : paper_radio_)->setChecked(true);
    root->addWidget(paper_radio_);
    root->addWidget(live_radio_);

    live_panel_ = new QWidget;
    live_panel_->setObjectName("deployLivePanel");
    auto* lp = new QVBoxLayout(live_panel_);
    lp->setContentsMargins(10, 8, 10, 8);
    lp->setSpacing(6);
    live_warning_ = new QLabel;
    live_warning_->setObjectName("deployLiveWarning");
    live_warning_->setWordWrap(true);
    lp->addWidget(live_warning_);
    live_list_ = new QPlainTextEdit;
    live_list_->setObjectName("deployLiveList");
    live_list_->setReadOnly(true);
    live_list_->setMaximumHeight(110);
    lp->addWidget(live_list_);
    confirm_label_ = new QLabel;
    confirm_label_->setWordWrap(true);
    lp->addWidget(confirm_label_);
    confirm_edit_ = new QLineEdit;
    confirm_edit_->setPlaceholderText(QStringLiteral("LIVE"));
    lp->addWidget(confirm_edit_);
    root->addWidget(live_panel_);

    root->addStretch();

    // ── Buttons ────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(8);

    cancel_btn_ = new QPushButton;
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_row->addWidget(cancel_btn_);
    btn_row->addStretch();

    draft_btn_ = new QPushButton;
    connect(draft_btn_, &QPushButton::clicked, this, [this]() {
        deploy_ = false;
        accept();
    });
    btn_row->addWidget(draft_btn_);

    deploy_btn_ = new QPushButton;
    deploy_btn_->setObjectName("deployRunBtn");
    connect(deploy_btn_, &QPushButton::clicked, this, [this]() {
        deploy_ = true;
        accept();
    });
    btn_row->addWidget(deploy_btn_);
    root->addLayout(btn_row);

    // Enter in the confirmation field must not submit a half-typed confirmation.
    cancel_btn_->setAutoDefault(false);
    draft_btn_->setAutoDefault(false);
    deploy_btn_->setAutoDefault(false);

    connect(paper_radio_, &QRadioButton::toggled, this, [this]() { update_mode_ui(); });
    connect(confirm_edit_, &QLineEdit::textChanged, this, [this]() { update_mode_ui(); });

    retranslateUi();
}

void DeployDialog::update_mode_ui() {
    const bool live = live_radio_->isChecked();
    live_panel_->setVisible(live);
    const bool confirmed = !live || confirm_edit_->text().trimmed() == QLatin1String("LIVE");
    draft_btn_->setEnabled(confirmed);
    deploy_btn_->setEnabled(confirmed);
    deploy_btn_->setText(live ? tr("SAVE & RUN LIVE") : tr("SAVE & RUN (PAPER)"));
}

void DeployDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void DeployDialog::retranslateUi() {
    setWindowTitle(tr("Save & Run Workflow"));
    title_label_->setText(tr("SAVE & RUN WORKFLOW"));
    name_label_->setText(tr("WORKFLOW NAME"));
    desc_label_->setText(tr("DESCRIPTION"));
    desc_edit_->setPlaceholderText(tr("Describe what this workflow does..."));
    mode_label_->setText(tr("EXECUTION MODE"));
    paper_radio_->setText(tr("PAPER — every order node is simulated (default)"));
    live_radio_->setText(tr("LIVE — order nodes set to \"live\" send REAL orders to your broker"));
    if (live_rows_.isEmpty()) {
        live_warning_->setText(tr("No order node in this workflow is set to live, so LIVE changes nothing yet. Set a "
                                  "node's Mode to \"live\" to let it trade."));
        live_list_->setPlainText(QString());
        live_list_->setVisible(false);
    } else {
        live_warning_->setText(tr("In LIVE mode these %n node(s) act for real, with real money:", "", live_rows_.size()));
        live_list_->setPlainText(live_rows_.join(QLatin1Char('\n')));
        live_list_->setVisible(true);
    }
    confirm_label_->setText(
        tr("Type LIVE to confirm. Later runs started with EXECUTE will ask you to type it again."));
    cancel_btn_->setText(tr("CANCEL"));
    draft_btn_->setText(tr("SAVE DRAFT"));
    update_mode_ui();
}

} // namespace fincept::workflow
