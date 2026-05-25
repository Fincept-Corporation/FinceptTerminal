#include "screens/auth/PhaseOneBootstrapScreen.h"

#include "auth/PhaseOneAuthFlowBridge.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens {

PhaseOneBootstrapScreen::PhaseOneBootstrapScreen(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Create the first administrator"), this);
    username_input_ = new QLineEdit(this);
    password_input_ = new QLineEdit(this);
    error_label_ = new QLabel(this);
    submit_button_ = new QPushButton(tr("Create admin"), this);

    username_input_->setPlaceholderText(tr("Username"));
    password_input_->setPlaceholderText(tr("Password"));
    password_input_->setEchoMode(QLineEdit::Password);
    error_label_->hide();
    error_label_->setObjectName(QStringLiteral("phase1_bootstrap_error"));

    layout->addWidget(title);
    layout->addWidget(username_input_);
    layout->addWidget(password_input_);
    layout->addWidget(error_label_);
    layout->addWidget(submit_button_);
    layout->addStretch(1);

    connect(submit_button_, &QPushButton::clicked, this, [this]() { on_submit(); });
}

void PhaseOneBootstrapScreen::on_submit() {
    error_label_->hide();
    auth::PhaseOneAuthFlowBridge::submit_bootstrap(username_input_->text().trimmed(), password_input_->text(),
                                                   [this](auth::ApiResponse response) {
                                                       if (!response.success) {
                                                           show_error(response.error.isEmpty() ? tr("Bootstrap failed.") : response.error);
                                                           return;
                                                       }

                                                       emit bootstrap_succeeded();
                                                       emit navigate_login();
                                                   });
}

void PhaseOneBootstrapScreen::show_error(const QString& message) {
    error_label_->setText(message);
    error_label_->show();
}

} // namespace fincept::screens
