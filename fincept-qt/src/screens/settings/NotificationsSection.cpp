// NotificationsSection.cpp — per-provider accordion + alert triggers panel.

#include "screens/settings/NotificationsSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "services/notifications/NotificationService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QVBoxLayout>
#include <QVector>

namespace fincept::screens {

namespace {

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
        {"telegram", "Telegram", "✈",
         {{"bot_token", "Bot Token", "Enter bot token from @BotFather", true},
          {"chat_id",   "Chat ID",   "e.g. 123456789"}}},
        {"discord", "Discord", "",
         {{"webhook_url", "Webhook URL", "https://discord.com/api/webhooks/..."}}},
        {"slack", "Slack", "",
         {{"webhook_url", "Webhook URL", "https://hooks.slack.com/services/..."},
          {"channel",     "Channel",     "#alerts (optional)"}}},
        {"email", "Email", "",
         {{"smtp_host", "SMTP Host", "smtp.gmail.com"},
          {"smtp_port", "Port",      "587"},
          {"smtp_user", "Username",  "you@gmail.com"},
          {"smtp_pass", "Password",  "App password", true},
          {"to_addr",   "To Address","recipient@example.com"},
          {"from_addr", "From Address","you@gmail.com (optional)"}}},
        {"whatsapp", "WhatsApp", "",
         {{"account_sid", "Account SID", "Twilio Account SID"},
          {"auth_token",  "Auth Token",  "Twilio Auth Token", true},
          {"from_number", "From Number", "+14155238886"},
          {"to_number",   "To Number",   "+1XXXXXXXXXX"}}},
        {"pushover", "Pushover", "",
         {{"api_token", "API Token", "Your Pushover app token", true},
          {"user_key",  "User Key",  "Your Pushover user key", true}}},
        {"ntfy", "ntfy", "",
         {{"server_url", "Server URL", "https://ntfy.sh (leave empty for default)"},
          {"topic",      "Topic",      "your-topic-name"},
          {"token",      "Auth Token", "Optional auth token", true}}},
        {"pushbullet", "Pushbullet", "",
         {{"api_key",     "API Key",     "Your Pushbullet API key", true},
          {"channel_tag", "Channel Tag", "Optional channel tag"}}},
        {"gotify", "Gotify", "",
         {{"server_url", "Server URL", "https://your-gotify-server.com"},
          {"app_token",  "App Token",  "Application token", true}}},
        {"mattermost", "Mattermost", "",
         {{"webhook_url", "Webhook URL", "https://your-mattermost.com/hooks/..."},
          {"channel",     "Channel",     "#town-square (optional)"},
          {"username",    "Username",    "Fincept (optional)"}}},
        {"teams", "MS Teams", "",
         {{"webhook_url", "Webhook URL", "https://outlook.office.com/webhook/..."}}},
        {"webhook", "Webhook", "",
         {{"url",    "URL",    "https://your-endpoint.com/notify"},
          {"method", "Method", "POST"}}},
        {"pagerduty", "PagerDuty", "",
         {{"routing_key", "Routing Key", "32-character integration key", true}}},
        {"opsgenie", "Opsgenie", "",
         {{"api_key", "API Key", "Your Opsgenie API key", true}}},
        {"sms", "SMS (Twilio)", "",
         {{"account_sid", "Account SID", "Twilio Account SID"},
          {"auth_token",  "Auth Token",  "Twilio Auth Token", true},
          {"from_number", "From Number", "+15005550006"},
          {"to_number",   "To Number",   "+1XXXXXXXXXX"}}},
    };
    return defs;
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
        arrow_lbl->setStyleSheet(
            QString("color: %1; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        hl->addWidget(arrow_lbl);

        auto* icon_lbl = new QLabel(def.icon.isEmpty() ? def.name.toUpper()
                                                       : def.icon + "  " + def.name.toUpper());
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
            if (fd.is_password)
                field->setEchoMode(QLineEdit::Password);
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
        pw.test_btn->setFixedWidth(100);
        pw.test_btn->setStyleSheet(btn_secondary_ss());

        pw.status_lbl = new QLabel;
        pw.status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));

        thl->addWidget(pw.test_btn);
        thl->addWidget(pw.status_lbl, 1);
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
            save_provider_fields(pid, pw);
            NotificationService::instance().reload_all_configs();

            pw.status_lbl->setText(tr("Sending..."));
            pw.status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));

            NotificationRequest req;
            req.title   = tr("Fincept Test");
            req.message = tr("This is a test notification from Fincept Terminal.");
            req.trigger = NotifTrigger::Manual;

            QPointer<QLabel> status_ptr = pw.status_lbl;
            NotificationService::instance().send_to(pid, req, [status_ptr](bool ok, const QString& err) {
                if (!status_ptr) return;
                if (ok) {
                    status_ptr->setText(tr("✓ Sent successfully"));
                    status_ptr->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                } else {
                    status_ptr->setText("✗ " + err.left(60));
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

    trigger_inapp_  = new QCheckBox; trigger_inapp_->setStyleSheet(check_ss());
    trigger_price_  = new QCheckBox; trigger_price_->setStyleSheet(check_ss());
    trigger_news_   = new QCheckBox; trigger_news_->setStyleSheet(check_ss());
    trigger_orders_ = new QCheckBox; trigger_orders_->setStyleSheet(check_ss());

    auto* row_inapp = make_row(tr("In-App Alerts (toast + bell)"), trigger_inapp_,
                               tr("Show slide-in toasts and update bell badge."));
    capture_row_labels(row_inapp, &row_inapp_lbl_, &row_inapp_desc_);
    vl->addWidget(row_inapp);
    auto* row_price = make_row(tr("Price Alerts"), trigger_price_, tr("Notify when price alert thresholds are crossed."));
    capture_row_labels(row_price, &row_price_lbl_, &row_price_desc_);
    vl->addWidget(row_price);
    auto* row_news = make_row(tr("News Alerts"), trigger_news_, tr("Enable news notifications (configure which types below)."));
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

    news_breaking_   = new QCheckBox; news_breaking_->setStyleSheet(check_ss());
    news_monitors_   = new QCheckBox; news_monitors_->setStyleSheet(check_ss());
    news_deviations_ = new QCheckBox; news_deviations_->setStyleSheet(check_ss());
    news_flash_      = new QCheckBox; news_flash_->setStyleSheet(check_ss());

    auto* row_breaking = make_row(tr("Breaking News"), news_breaking_,
                                  tr("Notify on FLASH/BREAKING/URGENT priority clusters."));
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
    auto* row_flash = make_row(tr("FLASH + High-Impact Articles"), news_flash_,
                               tr("Notify on individual articles that are both FLASH priority and high market impact."));
    capture_row_labels(row_flash, &row_flash_lbl_, &row_flash_desc_);
    sub_vl->addWidget(row_flash);

    vl->addWidget(news_subopts_frame_);

    connect(trigger_news_, &QCheckBox::toggled, this,
            [this](bool on) { news_subopts_frame_->setVisible(on); });

    auto* row_orders = make_row(tr("Order Fill Alerts"), trigger_orders_,
                                tr("Notify when orders are filled or rejected."));
    capture_row_labels(row_orders, &row_orders_lbl_, &row_orders_desc_);
    vl->addWidget(row_orders);

    vl->addSpacing(16);

    // Save
    save_btn_ = new QPushButton(tr("Save All Providers"));
    save_btn_->setFixedWidth(200);
    save_btn_->setStyleSheet(btn_primary_ss());
    connect(save_btn_, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();
        auto b = [](bool v) { return v ? "1" : "0"; };
        repo.set("notifications.inapp",           b(trigger_inapp_->isChecked()),  "notifications");
        repo.set("notifications.price_alerts",    b(trigger_price_->isChecked()),  "notifications");
        repo.set("notifications.news_alerts",     b(trigger_news_->isChecked()),   "notifications");
        repo.set("notifications.order_fills",     b(trigger_orders_->isChecked()), "notifications");
        repo.set("notifications.news_breaking",   b(news_breaking_->isChecked()),  "notifications");
        repo.set("notifications.news_monitors",   b(news_monitors_->isChecked()),  "notifications");
        repo.set("notifications.news_deviations", b(news_deviations_->isChecked()),"notifications");
        repo.set("notifications.news_flash",      b(news_flash_->isChecked()),     "notifications");

        for (const auto& def : provider_defs())
            save_provider_fields(def.id, provider_widgets_.value(def.id));

        NotificationService::instance().reload_all_configs();
        LOG_INFO("Settings", "All notification providers saved");
    });
    vl->addWidget(save_btn_);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll);
}

void NotificationsSection::reload() {
    if (provider_widgets_.isEmpty()) return;

    auto& repo = SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };

    if (trigger_inapp_)  trigger_inapp_->setChecked(get_bool("notifications.inapp", true));
    if (trigger_price_)  trigger_price_->setChecked(get_bool("notifications.price_alerts", true));
    if (trigger_orders_) trigger_orders_->setChecked(get_bool("notifications.order_fills", true));

    const bool news_on = get_bool("notifications.news_alerts", false);
    if (trigger_news_) trigger_news_->setChecked(news_on);

    if (news_breaking_)   news_breaking_->setChecked(get_bool("notifications.news_breaking", true));
    if (news_monitors_)   news_monitors_->setChecked(get_bool("notifications.news_monitors", true));
    if (news_deviations_) news_deviations_->setChecked(get_bool("notifications.news_deviations", true));
    if (news_flash_)      news_flash_->setChecked(get_bool("notifications.news_flash", true));
    if (news_subopts_frame_) news_subopts_frame_->setVisible(news_on);

    for (const auto& def : provider_defs()) {
        if (!provider_widgets_.contains(def.id)) continue;
        const auto& pw = provider_widgets_[def.id];
        const QString cat = QString("notif_%1").arg(def.id);

        if (pw.enabled) {
            auto r = repo.get(cat + ".enabled");
            pw.enabled->setChecked(r.is_ok() && r.value() == "1");
        }

        for (const auto& fd : def.fields) {
            if (!pw.fields.contains(fd.key)) continue;
            auto r = repo.get(cat + "." + fd.key);
            if (r.is_ok() && pw.fields[fd.key])
                pw.fields[fd.key]->setText(r.value());
        }
    }
}

void NotificationsSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NotificationsSection::retranslateUi() {
    if (providers_hdr_) providers_hdr_->setText(tr("NOTIFICATION PROVIDERS"));
    if (triggers_hdr_)  triggers_hdr_->setText(tr("ALERT TRIGGERS"));
    if (save_btn_)      save_btn_->setText(tr("Save All Providers"));

    // Trigger rows.
    if (row_inapp_lbl_)  row_inapp_lbl_->setText(tr("In-App Alerts (toast + bell)"));
    if (row_inapp_desc_) row_inapp_desc_->setText(tr("Show slide-in toasts and update bell badge."));
    if (row_price_lbl_)  row_price_lbl_->setText(tr("Price Alerts"));
    if (row_price_desc_) row_price_desc_->setText(tr("Notify when price alert thresholds are crossed."));
    if (row_news_lbl_)   row_news_lbl_->setText(tr("News Alerts"));
    if (row_news_desc_)  row_news_desc_->setText(tr("Enable news notifications (configure which types below)."));
    if (row_orders_lbl_)  row_orders_lbl_->setText(tr("Order Fill Alerts"));
    if (row_orders_desc_) row_orders_desc_->setText(tr("Notify when orders are filled or rejected."));

    // News sub-option rows.
    if (row_breaking_lbl_)  row_breaking_lbl_->setText(tr("Breaking News"));
    if (row_breaking_desc_) row_breaking_desc_->setText(tr("Notify on FLASH/BREAKING/URGENT priority clusters."));
    if (row_monitors_lbl_)  row_monitors_lbl_->setText(tr("Monitor Keyword Matches"));
    if (row_monitors_desc_) row_monitors_desc_->setText(tr("Notify when a news monitor watch list gets new matches."));
    if (row_deviations_lbl_)  row_deviations_lbl_->setText(tr("Category Volume Spikes"));
    if (row_deviations_desc_) row_deviations_desc_->setText(tr("Notify when a category has abnormally high article volume (z-score ≥ 3)."));
    if (row_flash_lbl_)  row_flash_lbl_->setText(tr("FLASH + High-Impact Articles"));
    if (row_flash_desc_) row_flash_desc_->setText(tr("Notify on individual articles that are both FLASH priority and high market impact."));

    // Per-provider: Test Send button + field row labels (provider names + icons
    // are brand data, left as-is; placeholders are config examples, left as-is).
    for (const auto& def : provider_defs()) {
        if (!provider_widgets_.contains(def.id)) continue;
        const auto& pw = provider_widgets_[def.id];
        if (pw.test_btn) pw.test_btn->setText(tr("Test Send"));
        for (const auto& fd : def.fields) {
            if (auto* lbl = pw.field_labels.value(fd.key, nullptr))
                lbl->setText(tr(fd.label.toUtf8().constData()));
        }
    }
}

void NotificationsSection::save_provider_fields(const QString& provider_id, const ProviderWidgets& pw) {
    auto& repo = SettingsRepository::instance();
    const QString cat = QString("notif_%1").arg(provider_id);

    if (pw.enabled)
        repo.set(cat + ".enabled", pw.enabled->isChecked() ? "1" : "0", cat);

    for (auto it = pw.fields.constBegin(); it != pw.fields.constEnd(); ++it) {
        if (it.value())
            repo.set(cat + "." + it.key(), it.value()->text().trimmed(), cat);
    }
}

} // namespace fincept::screens
