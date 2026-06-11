#include "screens/alpha_arena/panels/LiveModeGateDialog.h"

#include "core/logging/Logger.h"
#include "services/alpha_arena/ArenaStore.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace fincept::screens::alpha_arena {

namespace {
const QString kRequiredPhrase = QStringLiteral("I UNDERSTAND THIS IS REAL MONEY");
const QString kPrivateKeyHexPattern = QStringLiteral("^0x[a-fA-F0-9]{64}$");
} // namespace

LiveModeGateDialog::LiveModeGateDialog(const QString& competition_id, QWidget* parent)
    : QDialog(parent), competition_id_(competition_id) {
    setWindowTitle(tr("Alpha Arena — Live Mode"));
    setModal(true);
    setMinimumWidth(560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    warn_label_ = new QLabel(tr(
        "<b>You are about to enable live trading.</b><br>"
        "Orders below will be sent to Hyperliquid using a real agent wallet "
        "and may incur real losses. There is <b>no undo</b>.<br><br>"
        "Read every line of the next four prompts carefully."));
    warn_label_->setWordWrap(true);
    warn_label_->setStyleSheet("color:#FF6B6B;");
    root->addWidget(warn_label_);

    // The phrase itself is a logic value (compared verbatim against user input);
    // only the surrounding instruction is translated.
    phrase_prompt_ = new QLabel(tr("Type the phrase <b>exactly</b>: %1").arg(kRequiredPhrase));
    root->addWidget(phrase_prompt_);
    phrase_input_ = new QLineEdit;
    phrase_input_->setPlaceholderText(kRequiredPhrase);
    root->addWidget(phrase_input_);

    age_check_ = new QCheckBox(tr("I am at least 18 years old."));
    root->addWidget(age_check_);

    jurisdiction_check_ = new QCheckBox(tr(
        "I am NOT a resident or located in the United States, North Korea, "
        "Iran, or any other sanctioned jurisdiction."));
    root->addWidget(jurisdiction_check_);

    key_prompt_ = new QLabel(tr(
        "Agent wallet private key (separate from any master wallet — "
        "0x-prefixed 32-byte hex):"));
    root->addWidget(key_prompt_);
    private_key_input_ = new QLineEdit;
    private_key_input_->setEchoMode(QLineEdit::Password);
    private_key_input_->setPlaceholderText(QStringLiteral("0x..."));
    root->addWidget(private_key_input_);

    status_label_ = new QLabel;
    status_label_->setStyleSheet("color:#888;");
    root->addWidget(status_label_);

    auto* btn_row = new QHBoxLayout;
    btn_row->addStretch(1);
    cancel_btn_ = new QPushButton(tr("Cancel"));
    confirm_btn_ = new QPushButton(tr("ENABLE LIVE MODE"));
    confirm_btn_->setEnabled(false);
    confirm_btn_->setStyleSheet("background:#C72020;color:#fff;font-weight:700;padding:6px 14px;");
    btn_row->addWidget(cancel_btn_);
    btn_row->addWidget(confirm_btn_);
    root->addLayout(btn_row);

    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirm_btn_, &QPushButton::clicked, this, &LiveModeGateDialog::on_confirm);
    connect(phrase_input_, &QLineEdit::textChanged, this, &LiveModeGateDialog::update_confirm_state);
    connect(private_key_input_, &QLineEdit::textChanged, this, &LiveModeGateDialog::update_confirm_state);
    connect(age_check_, &QCheckBox::toggled, this, &LiveModeGateDialog::update_confirm_state);
    connect(jurisdiction_check_, &QCheckBox::toggled, this, &LiveModeGateDialog::update_confirm_state);
}

void LiveModeGateDialog::update_confirm_state() {
    const bool phrase_ok = phrase_input_->text() == kRequiredPhrase;
    const bool key_ok = QRegularExpression(kPrivateKeyHexPattern).match(private_key_input_->text()).hasMatch();
    const bool checks_ok = age_check_->isChecked() && jurisdiction_check_->isChecked();
    confirm_btn_->setEnabled(phrase_ok && key_ok && checks_ok);

    QStringList missing;
    if (!phrase_ok) missing << tr("phrase");
    if (!key_ok) missing << tr("private key");
    if (!checks_ok) missing << tr("acknowledgements");
    status_label_->setText(missing.isEmpty()
                              ? tr("Ready.")
                              : tr("Waiting on: %1").arg(missing.join(", ")));
}

void LiveModeGateDialog::on_confirm() {
    const QString handle = QStringLiteral("alpha_arena/agent_key/%1").arg(competition_id_);
    auto store_r = SecureStorage::instance().store(handle, private_key_input_->text());
    if (store_r.is_err()) {
        status_label_->setText(tr("Failed to store key: %1")
                                  .arg(QString::fromStdString(store_r.error())));
        return;
    }

    // Immutable acceptance record (hostname doubles as the old ack's host field).
    // competition_id_ may be "pending" when run from the wizard — the engine
    // re-keys the wallet handle on start(); the event keeps the literal id.
    const QJsonObject payload{
        {"hostname", QHostInfo::localHostName()},
        {"timestamp_iso", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"competition_id", competition_id_}};
    fincept::arena::ArenaStore::instance().insert_event(
        competition_id_, QString(), QStringLiteral("live_mode_accepted"),
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));

    LOG_WARN("AlphaArena", QStringLiteral("Live mode accepted for competition %1").arg(competition_id_));

    agent_key_handle_ = handle;
    // Wipe sensitive UI state.
    private_key_input_->clear();
    phrase_input_->clear();

    accept();
}

// ── Re-translation ───────────────────────────────────────────────────────────

void LiveModeGateDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void LiveModeGateDialog::retranslateUi() {
    setWindowTitle(tr("Alpha Arena — Live Mode"));
    if (warn_label_)
        warn_label_->setText(tr(
            "<b>You are about to enable live trading.</b><br>"
            "Orders below will be sent to Hyperliquid using a real agent wallet "
            "and may incur real losses. There is <b>no undo</b>.<br><br>"
            "Read every line of the next four prompts carefully."));
    if (phrase_prompt_)
        phrase_prompt_->setText(tr("Type the phrase <b>exactly</b>: %1").arg(kRequiredPhrase));
    if (age_check_)
        age_check_->setText(tr("I am at least 18 years old."));
    if (jurisdiction_check_)
        jurisdiction_check_->setText(tr(
            "I am NOT a resident or located in the United States, North Korea, "
            "Iran, or any other sanctioned jurisdiction."));
    if (key_prompt_)
        key_prompt_->setText(tr(
            "Agent wallet private key (separate from any master wallet — "
            "0x-prefixed 32-byte hex):"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("Cancel"));
    if (confirm_btn_)
        confirm_btn_->setText(tr("ENABLE LIVE MODE"));
    // Re-derive the dynamic status line in the new language.
    update_confirm_state();
}

} // namespace fincept::screens::alpha_arena
