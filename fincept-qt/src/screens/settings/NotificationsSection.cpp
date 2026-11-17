// NotificationsSection.cpp — per-provider accordion + alert triggers panel.

#include "screens/settings/NotificationsSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "services/notifications/NotificationService.h"
#include "services/notifications/providers/BaseProvider.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>

namespace fincept::screens {

namespace {

/// Dynamic-property guards set by reload() when a settings *read* failed.
/// `kNotifProviderReadFailedProp` lives on each provider's body_frame (the only
/// handle save_provider_fields() has); `kNotifTriggersReadFailedProp` lives on
/// the section itself and covers the eight alert-trigger booleans. While set,
/// the corresponding save path refuses to write — the widgets hold defaults,
/// not the user's values, and set() is an INSERT OR REPLACE.
constexpr const char* kNotifProviderReadFailedProp = "fincept_notif_provider_read_failed";
constexpr const char* kNotifTriggersReadFailedProp = "fincept_notif_triggers_read_failed";
/// QStringList on body_frame: fields whose stored secret is damaged (SecretRead
/// Corrupt — it can never be read). Such a provider saves only once each of them
/// was entered again; an empty box never erases the stored row.
constexpr const char* kNotifDamagedSecretsProp = "fincept_notif_damaged_secrets";

struct FieldDef {
    QString key;
    QString label;
    QString placeholder;
    bool is_password{false};
};

struct ProviderDef {
    QString id;
    QString name;
    QString icon;
    QVector<FieldDef> fields;
};

// make_row() builds the label QLabel internally; grab it back from the row's
// direct child QLabels so retranslateUi() can re-apply text. Order is
// deterministic: title label first, description label (when present) second.
void capture_row_labels(QWidget* row, QLabel** label_out, QLabel** desc_out = nullptr) {
    const auto labels = row->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    if (!labels.isEmpty() && label_out)
        *label_out = labels.at(0);
    if (labels.size() > 1 && desc_out)
        *desc_out = labels.at(1);
}

const QVector<ProviderDef>& provider_defs() {
    static const QVector<ProviderDef> defs = {
        {"telegram",
         "Telegram",
         "✈",
         {{"bot_token", "Bot Token", "Enter bot token from @BotFather", true},
          {"chat_id", "Chat ID", "e.g. 123456789"}}},
        {"discord", "Discord", "", {{"webhook_url", "Webhook URL", "https://discord.com/api/webhooks/..."}}},
        {"slack",
         "Slack",
         "",
         {{"webhook_url", "Webhook URL", "https://hooks.slack.com/services/..."},
          {"channel", "Channel", "#alerts (optional)"}}},
        {"email",
         "Email",
         "",
         {{"smtp_host", "SMTP Host", "smtp.gmail.com"},
          {"smtp_port", "Port", "587 (STARTTLS) or 465 (SSL)"},
          {"smtp_user", "Username", "you@gmail.com"},
          {"smtp_pass", "Password", "App password", true},
          {"to_addr", "To Address", "recipient@example.com"},
          {"from_addr", "From Address", "you@gmail.com (optional)"}}},
        {"whatsapp",
         "WhatsApp",
         "",
         {{"account_sid", "Account SID", "Twilio Account SID"},
          {"auth_token", "Auth Token", "Twilio Auth Token", true},
          {"from_number", "From Number", "+14155238886"},
          {"to_number", "To Number", "+1XXXXXXXXXX"}}},
        {"pushover",
         "Pushover",
         "",
         {{"api_token", "API Token", "Your Pushover app token", true},
          {"user_key", "User Key", "Your Pushover user key", true}}},
        {"ntfy",
         "ntfy",
         "",
         {{"server_url", "Server URL", "https://ntfy.sh (leave empty for default)"},
          {"topic", "Topic", "your-topic-name"},
          {"token", "Auth Token", "Optional auth token", true}}},
        {"pushbullet",
         "Pushbullet",
         "",
         {{"api_key", "API Key", "Your Pushbullet API key", true},
          {"channel_tag", "Channel Tag", "Optional channel tag"}}},
        {"gotify",
         "Gotify",
         "",
         {{"server_url", "Server URL", "https://your-gotify-server.com"},
          {"app_token", "App Token", "Application token", true}}},
        {"mattermost",
         "Mattermost",
         "",
         {{"webhook_url", "Webhook URL", "https://your-mattermost.com/hooks/..."},
          {"channel", "Channel", "#town-square (optional)"},
          {"username", "Username", "Fincept (optional)"}}},
        {"teams", "MS Teams", "", {{"webhook_url", "Webhook URL", "https://outlook.office.com/webhook/..."}}},
        {"webhook",
         "Webhook",
         "",
         {{"url", "URL", "https://your-endpoint.com/notify"}, {"method", "Method", "POST or PUT (default POST)"}}},
        {"pagerduty", "PagerDuty", "", {{"routing_key", "Routing Key", "32-character integration key", true}}},
        {"opsgenie", "Opsgenie", "", {{"api_key", "API Key", "Your Opsgenie API key", true}}},
        {"sms",
         "SMS (Twilio)",
         "",
         {{"account_sid", "Account SID", "Twilio Account SID"},
          {"auth_token", "Auth Token", "Twilio Auth Token", true},
          {"from_number", "From Number", "+15005550006"},
          {"to_number", "To Number", "+1XXXXXXXXXX"}}},
    };
    return defs;
}

/// True for the credentials NotificationService's providers keep in SecureStorage
/// (BaseProvider::get_secret / set_secret) rather than in the plaintext `settings`
/// table: every password field plus the webhook endpoints, which are bearer URLs.
/// Keep in step with the providers' `set_secret(...)` calls.
bool notif_field_is_secret(const QString& provider_id, const FieldDef& fd) {
    return fd.is_password || fd.key == QLatin1String("webhook_url") ||
           (provider_id == QLatin1String("webhook") && fd.key == QLatin1String("url"));
}

/// SecureStorage handle for a secret setting key ("notif_<provider>.<field>").
/// Must stay identical to BaseProvider::secret_handle().
QString notif_secret_handle(const QString& setting_key) {
    return QStringLiteral("notif:secret:") + setting_key;
}

/// Persist one secret field the way BaseProvider::set_secret does: the encrypted
/// copy is authoritative and the plaintext column is blanked — but only once the
/// encrypted store succeeded, so an unavailable keychain never loses the value.
void notif_store_secret(SettingsRepository& repo, const QString& setting_key, const QString& value,
                        const QString& category) {
    const QString handle = notif_secret_handle(setting_key);
    if (value.isEmpty()) {
        const auto rm = SecureStorage::instance().remove(handle);
        if (rm.is_err())
            LOG_WARN("Settings", QString("Could not clear stored secret for '%1': %2")
                                     .arg(setting_key, QString::fromStdString(rm.error())));
        repo.set(setting_key, QString{}, category);
        return;
    }
    // Blank the plaintext row only once the encrypted copy was written AND read back equal.
    const auto st = SecureStorage::instance().store_verified(handle, value);
    if (st.is_ok()) {
        repo.set(setting_key, QString{}, category);
    } else {
        LOG_WARN("Settings", logf("Notification credential kept in settings, secure storage write not confirmed",
                                  {{"key", setting_key}, {"error", QString::fromStdString(st.error())}}));
        repo.set(setting_key, value, category);
        // Reads prefer a stored copy: the value the user just replaced must not shadow it.
        SecureStorage::instance().remove(handle);
    }
}

const ProviderDef* notif_find_def(const QString& provider_id) {
    for (const auto& d : provider_defs())
        if (d.id == provider_id)
            return &d;
    return nullptr;
}

/// The provider's status line in `color`; the stylesheet is set only when it changes.
void notif_set_status(QLabel* label, const QString& text, const QString& color) {
    if (!label)
        return;
    const QString ss = QStringLiteral("color:%1;background:transparent;").arg(color);
    if (label->styleSheet() != ss)
        label->setStyleSheet(ss);
    label->setText(text);
}

/// Returns an error message when `value` is unusable for `field_key`; empty when OK
/// (an empty value is always accepted — the provider simply stays unconfigured).
QString notif_validate_field(const QString& field_key, const QString& value) {
    if (value.isEmpty())
        return {};
    if (field_key == QLatin1String("smtp_port")) {
        bool ok = false;
        const int port = value.toInt(&ok);
        if (!ok || port < 1 || port > 65535)
            return QCoreApplication::translate("NotificationsSection", "must be a number between 1 and 65535");
    }
    // Webhook method: the provider sends POST or PUT only — anything else (GET,
    // PATCH, a typo) used to go out as POST without a word.
    if (field_key == QLatin1String("method")) {
        const QString m = value.toUpper();
        if (m != QLatin1String("POST") && m != QLatin1String("PUT"))
            return QCoreApplication::translate("NotificationsSection", "must be POST or PUT");
    }
    if (field_key == QLatin1String("webhook_url") || field_key == QLatin1String("url") ||
        field_key == QLatin1String("server_url")) {
        const QUrl u(value, QUrl::StrictMode);
        const QString scheme = u.scheme().toLower();
        if (!u.isValid() || u.host().isEmpty() ||
            (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
            return QCoreApplication::translate("NotificationsSection", "must be a full http:// or https:// URL");
    }
    return {};
}

} // namespace

NotificationsSection::NotificationsSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void NotificationsSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void NotificationsSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;
    using namespace fincept::notifications;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; border-radius: 3px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(6);

    providers_hdr_ = new QLabel(tr("NOTIFICATION PROVIDERS"));
    providers_hdr_->setStyleSheet(section_title_ss());
    vl->addWidget(providers_hdr_);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Per-provider accordion cards ──────────────────────────────────────────
    for (const auto& def : provider_defs()) {
        ProviderWidgets pw;

        auto* card = new QFrame;
        card->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 3px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(0);

        // Header row
        auto* head = new QWidget(this);
        head->setFixedHeight(36);
        head->setStyleSheet("background: transparent;");
        head->setCursor(Qt::PointingHandCursor);
        auto* hl = new QHBoxLayout(head);
        hl->setContentsMargins(10, 0, 10, 0);
        hl->setSpacing(8);

        auto* arrow_lbl = new QLabel("▶");
        arrow_lbl->setStyleSheet(QString("color: %1; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        hl->addWidget(arrow_lbl);

        auto* icon_lbl = new QLabel(def.icon.isEmpty() ? def.name.toUpper() : def.icon + "  " + def.name.toUpper());
        icon_lbl->setStyleSheet(
            QString("color: %1; font-weight: bold; background: transparent;").arg(ui::colors::TEXT_PRIMARY()));
        hl->addWidget(icon_lbl, 1);

        pw.enabled = new QCheckBox;
        pw.enabled->setStyleSheet(check_ss());
        hl->addWidget(pw.enabled);

        cvl->addWidget(head);

        // Body
        pw.body_frame = new QFrame;
        pw.body_frame->setStyleSheet(QString("QFrame { background: %1; border-top: 1px solid %2; }")
                                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* bvl = new QVBoxLayout(pw.body_frame);
        bvl->setContentsMargins(12, 10, 12, 10);
        bvl->setSpacing(6);

        const QString pid = def.id;

        for (const auto& fd : def.fields) {
            auto* field = new QLineEdit;
            // Placeholders are concrete config examples (URLs, host names, phone
            // numbers) — left untranslated. Field labels are UI text.
            field->setPlaceholderText(fd.placeholder);
            field->setStyleSheet(input_ss());
            field->setAccessibleName(QString("%1 — %2").arg(def.name, fd.label));
            if (fd.is_password) {
                field->setEchoMode(QLineEdit::Password);
                // Bot tokens / SMTP passwords / Twilio auth tokens must not be
                // harvested by predictive text or an IME candidate window.
                field->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
                field->setDragEnabled(false);
                field->setAcceptDrops(false);
            } else if (notif_field_is_secret(def.id, fd)) {
                // Webhook endpoints are bearer URLs — anyone holding one can post
                // as the user. Visible while being edited, masked otherwise.
                field->setEchoMode(QLineEdit::PasswordEchoOnEdit);
                field->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText);
                field->setDragEnabled(false);
                field->setAcceptDrops(false);
            }
            pw.fields[fd.key] = field;
            auto* field_row = make_row(tr(fd.label.toUtf8().constData()), field);
            QLabel* row_lbl = nullptr;
            capture_row_labels(field_row, &row_lbl);
            if (row_lbl)
                pw.field_labels[fd.key] = row_lbl;
            bvl->addWidget(field_row);
        }

        // Test button + status
        auto* test_row = new QWidget(this);
        test_row->setStyleSheet("background: transparent;");
        auto* thl = new QHBoxLayout(test_row);
        thl->setContentsMargins(0, 4, 0, 0);
        thl->setSpacing(8);

        pw.test_btn = new QPushButton(tr("Test Send"));
        pw.test_btn->setProperty("fincept_agent_risk", "spend"); // agents: sends a real message through the provider
        pw.test_btn->setMinimumWidth(100);
        pw.test_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        pw.test_btn->setStyleSheet(btn_secondary_ss());

        pw.status_lbl = new QLabel;
        pw.status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        pw.status_lbl->setWordWrap(true); // a read problem is a sentence; never widen the page

        // RETRY: read this provider's stored settings again (only while they could not be read).
        pw.retry_btn = new QPushButton(tr("Retry"), test_row);
        pw.retry_btn->setProperty("fincept_agent_risk", "spend"); // agents: re-sends a real message through the provider
        pw.retry_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        pw.retry_btn->setStyleSheet(btn_secondary_ss());
        pw.retry_btn->setVisible(false);
        connect(pw.retry_btn, &QPushButton::clicked, this, [this, pid]() { reload_provider(pid); });

        thl->addWidget(pw.test_btn);
        thl->addWidget(pw.status_lbl, 1);
        thl->addWidget(pw.retry_btn);
        thl->addStretch();
        bvl->addWidget(test_row);

        cvl->addWidget(pw.body_frame);
        pw.body_frame->hide(); // collapsed by default

        // Expand/collapse on header click — overlay invisible button (QWidget has no clicked signal)
        auto* head_btn = new QPushButton(head);
        head_btn->setGeometry(0, 0, 9999, 36);
        head_btn->setFlat(true);
        head_btn->setStyleSheet("QPushButton { background: transparent; border: none; }");
        head_btn->raise();

        connect(head_btn, &QPushButton::clicked, this, [card, pw, arrow_lbl]() mutable {
            const bool expanded = pw.body_frame->isVisible();
            pw.body_frame->setVisible(!expanded);
            arrow_lbl->setText(expanded ? "▶" : "▼");
            Q_UNUSED(card);
        });

        // Test Send wiring
        connect(pw.test_btn, &QPushButton::clicked, this, [this, pid, pw]() mutable {
            // Invalid input (bad port / URL) must not be saved or sent — say why
            // in the provider's own status line.
            QString save_err;
            if (!save_provider_fields(pid, pw, &save_err)) {
                if (!save_err.isEmpty())
                    pw.status_lbl->setText(save_err);
                return;
            }
            NotificationService::instance().reload_all_configs();

            pw.status_lbl->setText(tr("Sending..."));
            pw.status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));

            NotificationRequest req;
            req.title = tr("Fincept Test");
            req.message = tr("This is a test notification from Fincept Terminal.");
            req.trigger = NotifTrigger::Manual;

            // Transport errors can echo the request URL, which for Telegram embeds
            // the bot token and for webhooks IS the secret. Scrub every masked
            // field's value before the error reaches the screen.
            QStringList secrets;
            for (auto it = pw.fields.cbegin(); it != pw.fields.cend(); ++it) {
                if (it.value() && it.value()->echoMode() != QLineEdit::Normal && !it.value()->text().trimmed().isEmpty())
                    secrets << it.value()->text().trimmed();
            }

            QPointer<QLabel> status_ptr = pw.status_lbl;
            NotificationService::instance().send_to(pid, req, [status_ptr, secrets](bool ok, const QString& err) {
                if (!status_ptr)
                    return;
                if (ok) {
                    status_ptr->setText(tr("✓ Sent successfully"));
                    status_ptr->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                } else {
                    QString shown = err;
                    for (const QString& secret : secrets)
                        shown.replace(secret, QStringLiteral("••••"));
                    status_ptr->setText("✗ " + shown.left(60));
                    status_ptr->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
                }
            });
        });

        provider_widgets_[def.id] = pw;
        vl->addWidget(card);
    }

    vl->addSpacing(16);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Alert triggers ────────────────────────────────────────────────────────
    triggers_hdr_ = new QLabel(tr("ALERT TRIGGERS"));
    triggers_hdr_->setStyleSheet(sub_title_ss());
    vl->addWidget(triggers_hdr_);
    vl->addSpacing(4);

    trigger_inapp_ = new QCheckBox;
    trigger_inapp_->setStyleSheet(check_ss());
    trigger_price_ = new QCheckBox;
    trigger_price_->setStyleSheet(check_ss());
    trigger_news_ = new QCheckBox;
    trigger_news_->setStyleSheet(check_ss());
    trigger_orders_ = new QCheckBox;
    trigger_orders_->setStyleSheet(check_ss());

    auto* row_inapp =
        make_row(tr("In-App Alerts (toast + bell)"), trigger_inapp_, tr("Show slide-in toasts and update bell badge."));
    capture_row_labels(row_inapp, &row_inapp_lbl_, &row_inapp_desc_);
    vl->addWidget(row_inapp);
    auto* row_price =
        make_row(tr("Price Alerts"), trigger_price_, tr("Notify when price alert thresholds are crossed."));
    capture_row_labels(row_price, &row_price_lbl_, &row_price_desc_);
    vl->addWidget(row_price);
    auto* row_news =
        make_row(tr("News Alerts"), trigger_news_, tr("Enable news notifications (configure which types below)."));
    capture_row_labels(row_news, &row_news_lbl_, &row_news_desc_);
    vl->addWidget(row_news);

    // News alert sub-options
    news_subopts_frame_ = new QFrame;
    news_subopts_frame_->setObjectName("newsSuboptsFrame");
    news_subopts_frame_->setStyleSheet(
        QString("#newsSuboptsFrame { border-left: 2px solid %1; margin-left: 24px; padding-left: 8px; }")
            .arg(ui::colors::BORDER_BRIGHT()));
    auto* sub_vl = new QVBoxLayout(news_subopts_frame_);
    sub_vl->setContentsMargins(0, 4, 0, 4);
    sub_vl->setSpacing(4);

    news_breaking_ = new QCheckBox;
    news_breaking_->setStyleSheet(check_ss());
    news_monitors_ = new QCheckBox;
    news_monitors_->setStyleSheet(check_ss());
    news_deviations_ = new QCheckBox;
    news_deviations_->setStyleSheet(check_ss());
    news_flash_ = new QCheckBox;
    news_flash_->setStyleSheet(check_ss());

    auto* row_breaking =
        make_row(tr("Breaking News"), news_breaking_, tr("Notify on FLASH/BREAKING/URGENT priority clusters."));
    capture_row_labels(row_breaking, &row_breaking_lbl_, &row_breaking_desc_);
    sub_vl->addWidget(row_breaking);
    auto* row_monitors = make_row(tr("Monitor Keyword Matches"), news_monitors_,
                                  tr("Notify when a news monitor watch list gets new matches."));
    capture_row_labels(row_monitors, &row_monitors_lbl_, &row_monitors_desc_);
    sub_vl->addWidget(row_monitors);
    auto* row_deviations = make_row(tr("Category Volume Spikes"), news_deviations_,
                                    tr("Notify when a category has abnormally high article volume (z-score ≥ 3)."));
    capture_row_labels(row_deviations, &row_deviations_lbl_, &row_deviations_desc_);
    sub_vl->addWidget(row_deviations);
    auto* row_flash =
        make_row(tr("FLASH + High-Impact Articles"), news_flash_,
                 tr("Notify on individual articles that are both FLASH priority and high market impact."));
    capture_row_labels(row_flash, &row_flash_lbl_, &row_flash_desc_);
    sub_vl->addWidget(row_flash);

    vl->addWidget(news_subopts_frame_);

    connect(trigger_news_, &QCheckBox::toggled, this, [this](bool on) { news_subopts_frame_->setVisible(on); });

    auto* row_orders =
        make_row(tr("Order Fill Alerts"), trigger_orders_, tr("Notify when orders are filled or rejected."));
    capture_row_labels(row_orders, &row_orders_lbl_, &row_orders_desc_);
    vl->addWidget(row_orders);

    vl->addSpacing(16);

    // Save
    save_btn_ = new QPushButton(tr("Save All Providers"));
    save_btn_->setProperty("fincept_agent_risk", "account"); // agents: stores notification-provider credentials
    save_btn_->setMinimumWidth(200);
    save_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    save_btn_->setStyleSheet(btn_primary_ss());
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();
        auto b = [](bool v) { return v ? "1" : "0"; };
        if (property(kNotifTriggersReadFailedProp).toBool()) {
            LOG_WARN("Settings", logf("Alert triggers not saved: the stored triggers could not be read, so the "
                                      "checkboxes show defaults",
                                      {{"section", "notifications.triggers"},
                                       {"error", "read failed at load (logged then)"}}));
        } else {
            repo.set("notifications.inapp", b(trigger_inapp_->isChecked()), "notifications");
            repo.set("notifications.price_alerts", b(trigger_price_->isChecked()), "notifications");
            repo.set("notifications.news_alerts", b(trigger_news_->isChecked()), "notifications");
            repo.set("notifications.order_fills", b(trigger_orders_->isChecked()), "notifications");
            repo.set("notifications.news_breaking", b(news_breaking_->isChecked()), "notifications");
            repo.set("notifications.news_monitors", b(news_monitors_->isChecked()), "notifications");
            repo.set("notifications.news_deviations", b(news_deviations_->isChecked()), "notifications");
            repo.set("notifications.news_flash", b(news_flash_->isChecked()), "notifications");
        }

        QStringList errors;
        for (const auto& def : provider_defs()) {
            QString err;
            if (!save_provider_fields(def.id, provider_widgets_.value(def.id), &err) && !err.isEmpty())
                errors << err;
        }

        NotificationService::instance().reload_all_configs();

        if (!errors.isEmpty()) {
            LOG_WARN("Settings", QString("Notification providers not saved (invalid input): %1").arg(errors.size()));
            QMessageBox::warning(this, tr("Notification Settings"),
                                 tr("These providers were not saved:\n\n%1").arg(errors.join(QLatin1Char('\n'))));
            return;
        }

        // Saving previously gave no on-screen acknowledgement at all.
        save_btn_->setText(tr("Saved ✓"));
        QTimer::singleShot(2000, save_btn_, [this]() {
            if (save_btn_)
                save_btn_->setText(tr("Save All Providers"));
        });
        LOG_INFO("Settings", "All notification providers saved");
    });
    vl->addWidget(save_btn_);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll);
}

void NotificationsSection::reload() {
    if (provider_widgets_.isEmpty())
        return;

    auto& repo = SettingsRepository::instance();
    // A read error is not "unset". Every value loaded here is written straight
    // back by the Save handler (and, for provider fields, by Test Send), so any
    // failed read must disarm the corresponding save path instead of letting a
    // default masquerade as the stored value.
    bool triggers_read_failed = false;
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        if (r.is_err()) {
            LOG_ERROR("Settings", QString("settings read failed for '%1' — showing the default, saving of the alert "
                                          "triggers is disabled: %2")
                                      .arg(key, QString::fromStdString(r.error())));
            triggers_read_failed = true;
            return def;
        }
        return !r.value().isEmpty() ? (r.value() == "1") : def;
    };

    if (trigger_inapp_)
        trigger_inapp_->setChecked(get_bool("notifications.inapp", true));
    if (trigger_price_)
        trigger_price_->setChecked(get_bool("notifications.price_alerts", true));
    if (trigger_orders_)
        trigger_orders_->setChecked(get_bool("notifications.order_fills", true));

    const bool news_on = get_bool("notifications.news_alerts", false);
    if (trigger_news_)
        trigger_news_->setChecked(news_on);

    if (news_breaking_)
        news_breaking_->setChecked(get_bool("notifications.news_breaking", true));
    if (news_monitors_)
        news_monitors_->setChecked(get_bool("notifications.news_monitors", true));
    if (news_deviations_)
        news_deviations_->setChecked(get_bool("notifications.news_deviations", true));
    if (news_flash_)
        news_flash_->setChecked(get_bool("notifications.news_flash", true));
    if (news_subopts_frame_)
        news_subopts_frame_->setVisible(news_on);

    setProperty(kNotifTriggersReadFailedProp, triggers_read_failed);

    for (const auto& def : provider_defs())
        reload_provider(def.id);
}

void NotificationsSection::reload_provider(const QString& provider_id) {
    const ProviderDef* def = notif_find_def(provider_id);
    if (!def || !provider_widgets_.contains(provider_id))
        return;
    auto& repo = SettingsRepository::instance();
    const ProviderWidgets pw = provider_widgets_.value(provider_id);
    const QString cat = QString("notif_%1").arg(provider_id);
    bool settings_read_failed = false; ///< a plain settings row could not be read
    QString secret_reason;             ///< SecureStorage could not answer (busy, key unavailable …)
    QString damaged_reason;            ///< …or a stored secret is damaged (never readable again)
    QStringList damaged;               ///< fields with a damaged stored secret
    QStringList damaged_labels;

    if (pw.enabled) {
        auto r = repo.get(cat + ".enabled");
        if (r.is_err()) {
            LOG_ERROR("Settings", QString("settings read failed for '%1' — leaving the stored value untouched: %2")
                                      .arg(cat + ".enabled", QString::fromStdString(r.error())));
            settings_read_failed = true;
        } else {
            pw.enabled->setChecked(r.value() == "1");
        }
    }

    for (const auto& fd : def->fields) {
        QLineEdit* edit = pw.fields.value(fd.key, nullptr);
        if (!edit)
            continue;
        // Secrets live in SecureStorage (see notif_field_is_secret); the
        // plaintext settings row is only a legacy fallback that the next
        // save blanks. Without this the fields render empty once migrated.
        if (notif_field_is_secret(provider_id, fd)) {
            // Tri-state (SecretRead.h): found / nothing stored / could not read.
            const SecretRead sr = SecureStorage::instance().read(notif_secret_handle(cat + "." + fd.key));
            if (sr.found() && !sr.value.isEmpty()) {
                edit->setText(sr.value);
                continue;
            }
            if (sr.failed()) {
                // The secret may exist: never "not set". The box stays empty (it is
                // not the stored value) and the provider is not saved over it.
                edit->clear();
                if (sr.error == SecretRead::Error::Corrupt) {
                    damaged << fd.key;
                    damaged_labels << tr(fd.label.toUtf8().constData());
                    damaged_reason = sr.reason();
                } else if (secret_reason.isEmpty()) {
                    secret_reason = sr.reason();
                }
                continue;
            }
            // Nothing stored: the legacy plaintext row (if any) below.
        }
        auto r = repo.get(cat + "." + fd.key);
        if (r.is_err()) {
            // These fields are credentials — webhook URLs, bot tokens, SMTP
            // passwords. Leave the box empty rather than pretending it is
            // the stored value, and disarm the save path below so neither
            // Save nor Test Send can write the blank over the real secret.
            LOG_ERROR("Settings", QString("settings read failed for '%1' — leaving the stored credential "
                                          "untouched: %2")
                                      .arg(cat + "." + fd.key, QString::fromStdString(r.error())));
            edit->clear();
            settings_read_failed = true;
            continue;
        }
        edit->setText(r.value());
    }

    // The guards live on the provider's own widget so save_provider_fields()
    // — which only receives the ProviderWidgets struct — can see them.
    const bool blocked = settings_read_failed || !secret_reason.isEmpty();
    if (pw.body_frame) {
        pw.body_frame->setProperty(kNotifProviderReadFailedProp, blocked);
        pw.body_frame->setProperty(kNotifDamagedSecretsProp, damaged);
    }
    if (pw.test_btn)
        pw.test_btn->setEnabled(!blocked);
    if (pw.retry_btn)
        pw.retry_btn->setVisible(blocked);
    if (settings_read_failed)
        notif_set_status(pw.status_lbl,
                         tr("Could not read this provider's saved settings — saving is disabled so the blank fields "
                            "cannot overwrite them."),
                         ui::colors::WARNING());
    else if (!secret_reason.isEmpty())
        notif_set_status(pw.status_lbl,
                         tr("Could not read secure storage: %1. Saving this provider is disabled until it can be "
                            "read.")
                             .arg(secret_reason),
                         ui::colors::WARNING());
    else if (!damaged.isEmpty())
        notif_set_status(pw.status_lbl,
                         tr("Could not read secure storage: %1 (%2). Enter it again to replace it; until then the "
                            "stored value is kept.")
                             .arg(damaged_reason, damaged_labels.join(QStringLiteral(", "))),
                         ui::colors::WARNING());
    else
        notif_set_status(pw.status_lbl, QString(), ui::colors::TEXT_SECONDARY());

    // The provider itself (NotificationService) may have failed to read the same
    // secret earlier (BaseProvider::has_unreadable_secret): it reads again now
    // that it can, instead of staying "not configured".
    if (!blocked && damaged.isEmpty()) {
        auto* provider =
            dynamic_cast<notifications::BaseProvider*>(notifications::NotificationService::instance().provider(provider_id));
        if (provider && provider->has_unreadable_secret())
            provider->load_config();
    }
}

void NotificationsSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NotificationsSection::retranslateUi() {
    if (providers_hdr_)
        providers_hdr_->setText(tr("NOTIFICATION PROVIDERS"));
    if (triggers_hdr_)
        triggers_hdr_->setText(tr("ALERT TRIGGERS"));
    if (save_btn_)
        save_btn_->setText(tr("Save All Providers"));

    // Trigger rows.
    if (row_inapp_lbl_)
        row_inapp_lbl_->setText(tr("In-App Alerts (toast + bell)"));
    if (row_inapp_desc_)
        row_inapp_desc_->setText(tr("Show slide-in toasts and update bell badge."));
    if (row_price_lbl_)
        row_price_lbl_->setText(tr("Price Alerts"));
    if (row_price_desc_)
        row_price_desc_->setText(tr("Notify when price alert thresholds are crossed."));
    if (row_news_lbl_)
        row_news_lbl_->setText(tr("News Alerts"));
    if (row_news_desc_)
        row_news_desc_->setText(tr("Enable news notifications (configure which types below)."));
    if (row_orders_lbl_)
        row_orders_lbl_->setText(tr("Order Fill Alerts"));
    if (row_orders_desc_)
        row_orders_desc_->setText(tr("Notify when orders are filled or rejected."));

    // News sub-option rows.
    if (row_breaking_lbl_)
        row_breaking_lbl_->setText(tr("Breaking News"));
    if (row_breaking_desc_)
        row_breaking_desc_->setText(tr("Notify on FLASH/BREAKING/URGENT priority clusters."));
    if (row_monitors_lbl_)
        row_monitors_lbl_->setText(tr("Monitor Keyword Matches"));
    if (row_monitors_desc_)
        row_monitors_desc_->setText(tr("Notify when a news monitor watch list gets new matches."));
    if (row_deviations_lbl_)
        row_deviations_lbl_->setText(tr("Category Volume Spikes"));
    if (row_deviations_desc_)
        row_deviations_desc_->setText(tr("Notify when a category has abnormally high article volume (z-score ≥ 3)."));
    if (row_flash_lbl_)
        row_flash_lbl_->setText(tr("FLASH + High-Impact Articles"));
    if (row_flash_desc_)
        row_flash_desc_->setText(
            tr("Notify on individual articles that are both FLASH priority and high market impact."));

    // Per-provider: Test Send button + field row labels (provider names + icons
    // are brand data, left as-is; placeholders are config examples, left as-is).
    for (const auto& def : provider_defs()) {
        if (!provider_widgets_.contains(def.id))
            continue;
        const auto& pw = provider_widgets_[def.id];
        if (pw.test_btn)
            pw.test_btn->setText(tr("Test Send"));
        if (pw.retry_btn)
            pw.retry_btn->setText(tr("Retry"));
        for (const auto& fd : def.fields) {
            if (auto* lbl = pw.field_labels.value(fd.key, nullptr))
                lbl->setText(tr(fd.label.toUtf8().constData()));
        }
    }
}

bool NotificationsSection::save_provider_fields(const QString& provider_id, const ProviderWidgets& pw,
                                                QString* error_out) {
    if (pw.body_frame && pw.body_frame->property(kNotifProviderReadFailedProp).toBool()) {
        LOG_WARN("Settings", QString("Skipping save for notification provider '%1' — its stored settings could not "
                                     "be read, so the fields on screen are not its real values")
                                 .arg(provider_id));
        return false;
    }

    const ProviderDef* def = notif_find_def(provider_id);
    if (!def)
        return false;

    // A damaged stored secret is replaced only by a value the user entered again;
    // an empty box never erases it (nor is anything else of the provider written).
    const QStringList damaged =
        pw.body_frame ? pw.body_frame->property(kNotifDamagedSecretsProp).toStringList() : QStringList();
    for (const QString& key : damaged) {
        const QLineEdit* edit = pw.fields.value(key, nullptr);
        if (!edit || edit->text().trimmed().isEmpty()) {
            LOG_WARN("Settings", logf("Notification provider not saved: a damaged stored credential was not entered "
                                      "again",
                                      {{"provider", provider_id}, {"field", key}}));
            return false;
        }
    }

    // Validate everything before writing anything, so a bad port or URL never
    // leaves the provider half-saved.
    for (const auto& fd : def->fields) {
        QLineEdit* edit = pw.fields.value(fd.key, nullptr);
        if (!edit)
            continue;
        const QString problem = notif_validate_field(fd.key, edit->text().trimmed());
        if (!problem.isEmpty()) {
            if (error_out)
                *error_out = QString("%1 — %2: %3").arg(def->name, tr(fd.label.toUtf8().constData()), problem);
            return false;
        }
    }

    auto& repo = SettingsRepository::instance();
    const QString cat = QString("notif_%1").arg(provider_id);

    if (pw.enabled)
        repo.set(cat + ".enabled", pw.enabled->isChecked() ? "1" : "0", cat);

    for (const auto& fd : def->fields) {
        QLineEdit* edit = pw.fields.value(fd.key, nullptr);
        if (!edit)
            continue;
        const QString value = edit->text().trimmed();
        const QString setting_key = cat + "." + fd.key;
        if (notif_field_is_secret(provider_id, fd))
            notif_store_secret(repo, setting_key, value, cat);
        else
            repo.set(setting_key, value, cat);
    }
    if (!damaged.isEmpty())
        reload_provider(provider_id); // the damaged rows were replaced: read them back
    return true;
}

} // namespace fincept::screens
