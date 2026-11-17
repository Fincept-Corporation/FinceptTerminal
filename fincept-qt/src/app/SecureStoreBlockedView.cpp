#include "app/SecureStoreBlockedView.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept {

SecureStoreBlockedView::SecureStoreBlockedView(Actions actions, QWidget* parent)
    : QWidget(parent), actions_(std::move(actions)) {
    setObjectName(QStringLiteral("secureStoreBlocked"));
    // Stylesheet before the children exist (one polish), from theme tokens only;
    // ThemeRestyle keeps it in step with a later theme / font change.
    restyle();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->addStretch(1);

    auto* card = new QWidget(this);
    card->setObjectName(QStringLiteral("secureStoreCard"));
    card->setMaximumWidth(640);
    auto* col = new QVBoxLayout(card);
    col->setContentsMargins(28, 24, 28, 24);
    col->setSpacing(12);

    title_ = new QLabel(tr("Secure storage unavailable"), card);
    title_->setObjectName(QStringLiteral("secureStoreTitle"));
    message_ = new QLabel(card);
    message_->setObjectName(QStringLiteral("secureStoreMessage"));
    message_->setWordWrap(true);
    message_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hint_ = new QLabel(card);
    hint_->setObjectName(QStringLiteral("secureStoreHint"));
    hint_->setWordWrap(true);
    col->addWidget(title_);
    col->addWidget(message_);
    col->addWidget(hint_);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    retry_ = new QPushButton(tr("Retry"), card);
    retry_->setObjectName(QStringLiteral("secureStorePrimary"));
    retry_->setDefault(true);
    sign_in_ = new QPushButton(tr("Sign in again"), card);
    reset_ = new QPushButton(tr("Reset secure storage…"), card);
    quit_ = new QPushButton(tr("Quit"), card);
    for (QPushButton* b : {retry_, sign_in_, reset_, quit_}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(30);
    }
    buttons->addWidget(retry_);
    buttons->addWidget(sign_in_);
    buttons->addWidget(reset_);
    buttons->addStretch(1);
    buttons->addWidget(quit_);
    col->addLayout(buttons);

    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(card, 4);
    row->addStretch(1);
    outer->addLayout(row);
    outer->addStretch(2);

    connect(retry_, &QPushButton::clicked, this, [this]() {
        if (actions_.retry)
            actions_.retry();
    });
    connect(sign_in_, &QPushButton::clicked, this, [this]() {
        if (actions_.sign_in_again)
            actions_.sign_in_again();
    });
    connect(reset_, &QPushButton::clicked, this, [this]() {
        if (actions_.reset_store)
            actions_.reset_store();
    });
    connect(quit_, &QPushButton::clicked, this, [this]() {
        if (actions_.quit)
            actions_.quit();
    });
}

void SecureStoreBlockedView::restyle() {
    using namespace ui;
    setStyleSheet(QStringLiteral("#secureStoreBlocked { background:%1; }"
                                 "#secureStoreCard { background:%2; border:1px solid %3; border-radius:6px; }"
                                 "#secureStoreTitle { color:%4; font-size:%5px; font-weight:600; }"
                                 "#secureStoreMessage { color:%6; font-size:%7px; }"
                                 "#secureStoreHint { color:%8; font-size:%9px; }")
                      .arg(colors::BG_BASE(), colors::BG_SURFACE(), colors::BORDER_MED(), colors::TEXT_PRIMARY(),
                           fonts::px(4), colors::TEXT_PRIMARY(), fonts::px(0), colors::TEXT_SECONDARY(),
                           fonts::px(-1)) +
                  QStringLiteral("#secureStoreCard QPushButton { background:%1; color:%2; border:1px solid %3;"
                                 " border-radius:3px; padding:4px 14px; font-size:%4px; }"
                                 "#secureStoreCard QPushButton:hover { background:%5; }"
                                 "#secureStorePrimary { background:%6; color:%7; border-color:%6; }")
                      .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_MED(), fonts::px(0),
                           colors::BG_HOVER(), colors::AMBER(), colors::TEXT_ON_ACCENT()));
}

QString SecureStoreBlockedView::headline(const SecretRead& failure) {
    return tr("Could not read secure storage: %1.").arg(failure.reason());
}

void SecureStoreBlockedView::show_failure(const SecretRead& failure, const QString& subject) {
    message_->setText(headline(failure));
    const bool corrupt = failure.error == SecretRead::Error::Corrupt;
    const bool key_gone = failure.error == SecretRead::Error::KeyUnavailable;
    if (failure.transient()) {
        hint_->setText(tr("Nothing was changed: %1 and your other saved credentials are left as they are. The terminal "
                          "stays locked and retries by itself; choose Retry to try again now.")
                           .arg(subject));
    } else if (corrupt) {
        hint_->setText(tr("The stored copy of %1 is damaged. Retry if another program was writing to the terminal "
                          "database. Otherwise sign in again with your Fincept account: the damaged PIN record is "
                          "cleared and you choose a new PIN. Other saved credentials are not erased.")
                           .arg(subject));
    } else if (key_gone) {
        hint_->setText(tr("Your saved credentials were protected on another computer or user account, or the "
                          "protection key was lost. They cannot be unlocked here. Resetting secure storage erases "
                          "every saved credential (sign-in, PIN, API keys, broker logins) so you can enter them again."));
    } else {
        hint_->setText(tr("Nothing was changed. Choose Retry to try again."));
    }
    sign_in_->setVisible(corrupt);
    reset_->setVisible(key_gone);
}

} // namespace fincept
