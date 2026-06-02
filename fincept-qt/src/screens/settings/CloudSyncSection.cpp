#include "screens/settings/CloudSyncSection.h"

#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "services/cloud/CloudSyncEngine.h"
#include "storage/sync/CloudSyncSettings.h"

#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QVector>

namespace fincept::screens {

using fincept::services::cloud::CloudSyncEngine;
// NOTE: settings_styles / settings_helpers using-directives are kept FUNCTION-LOCAL
// (inside build_ui) — a namespace-scope `using namespace` would leak into the shared
// unity-build translation unit and clash with other sections' file-local style helpers.

namespace {
struct DomainRow {
    const char* entity;
    const char* label;
    const char* desc;
};

// The single source of truth for which domains appear in the UI. Add a synced
// domain here (must match the adapter's entity() tag) and it gets a toggle + a
// status line automatically.
const QVector<DomainRow>& cloud_domains() {
    static const QVector<DomainRow> v = {
        {"watchlist", "Watchlists", "Sync watchlists and their symbols."},
        {"note", "Notes", "Sync financial notes (title, content, tags, favourites)."},
        {"portfolio", "Portfolios", "Sync portfolios, holdings, transactions and snapshots."},
        {"agent_config", "Agent configs", "Sync AI agent configurations."},
        {"report", "Reports", "Sync report-builder documents."},
        {"workflow", "Workflows", "Sync node-editor workflows (nodes + edges)."},
        {"dashboard", "Dashboard", "Sync dashboard layouts (grid + widgets)."},
        {"setting", "Preferences", "Sync UI preferences (appearance, general, notifications, keybindings)."},
        {"news_monitor", "News monitors", "Sync news keyword monitors."},
        {"news_feed", "News feeds", "Sync your custom RSS feeds."},
        {"notebook", "Notebooks", "Sync notebooks (cells + metadata)."},
    };
    return v;
}

// make_row() builds the label (and optional description) QLabel internally and
// returns only the row widget. Grab them back from the row's direct child
// QLabels so retranslateUi() can re-apply text. Order is deterministic: title
// label first, description label (when present) second.
void capture_row_labels(QWidget* row, QLabel** label_out, QLabel** desc_out = nullptr) {
    const auto labels = row->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    if (!labels.isEmpty() && label_out)
        *label_out = labels.at(0);
    if (labels.size() > 1 && desc_out)
        *desc_out = labels.at(1);
}

QString cloud_status_text(CloudSyncEngine::Status s, int pending, const QString& error) {
    switch (s) {
    case CloudSyncEngine::Status::Disabled:
        return QStringLiteral("off");
    case CloudSyncEngine::Status::Idle:
        return pending > 0 ? QStringLiteral("%1 pending").arg(pending) : QStringLiteral("synced");
    case CloudSyncEngine::Status::Syncing:
        return QStringLiteral("syncing…");
    case CloudSyncEngine::Status::Paused:
        return QStringLiteral("paused — out of credits");
    case CloudSyncEngine::Status::Error:
        return QStringLiteral("error: %1").arg(error);
    }
    return {};
}
} // namespace

CloudSyncSection::CloudSyncSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void CloudSyncSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void CloudSyncSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(8);

    title_ = new QLabel(tr("Fincept Cloud Sync"));
    title_->setStyleSheet(section_title_ss());
    vl->addWidget(title_);

    blurb_ = new QLabel(tr("Mirror your data to your Fincept account. Your local copy stays the working "
                           "copy — sync runs in the background, on this device and across your devices."));
    blurb_->setWordWrap(true);
    blurb_->setStyleSheet(label_ss());
    vl->addWidget(blurb_);

    credits_banner_ = new QLabel(tr("Out of credits — top up to resume cloud sync."));
    credits_banner_->setStyleSheet(
        QString("color:%1;background:transparent;font-weight:700;").arg(ui::colors::NEGATIVE()));
    credits_banner_->setVisible(false);
    vl->addWidget(credits_banner_);

    master_toggle_ = new QCheckBox(tr("Enable cloud sync"));
    master_toggle_->setStyleSheet(check_ss());
    auto* cloud_sync_row = make_row(tr("Cloud Sync"), master_toggle_,
                                    tr("When on, changes mirror to your account and pull on this device."));
    capture_row_labels(cloud_sync_row, &cloud_sync_label_, &cloud_sync_desc_);
    vl->addWidget(cloud_sync_row);

    sign_in_hint_ = new QLabel(tr("Sign in to enable cloud sync."));
    sign_in_hint_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
    sign_in_hint_->setVisible(false);
    vl->addWidget(sign_in_hint_);

    vl->addWidget(make_sep());

    adv_title_ = new QLabel(tr("ADVANCED — DOMAINS"));
    adv_title_->setStyleSheet(sub_title_ss());
    vl->addWidget(adv_title_);

    for (const DomainRow& d : cloud_domains()) {
        const QString entity = QString::fromLatin1(d.entity);
        auto* cb = new QCheckBox(tr("Sync this domain"));
        cb->setStyleSheet(check_ss());
        auto* domain_row = make_row(tr(d.label), cb, tr(d.desc));
        QLabel* row_label = nullptr;
        QLabel* row_desc = nullptr;
        capture_row_labels(domain_row, &row_label, &row_desc);
        if (row_label) domain_row_label_.insert(entity, row_label);
        if (row_desc) domain_row_desc_.insert(entity, row_desc);
        vl->addWidget(domain_row);
        domain_toggles_.insert(entity, cb);

        auto* st = new QLabel(QStringLiteral("—"));
        st->setStyleSheet(QString("color:%1;background:transparent;padding-left:8px;").arg(ui::colors::TEXT_DIM()));
        vl->addWidget(st);
        domain_status_.insert(entity, st);

        connect(cb, &QCheckBox::toggled, this,
                [entity](bool on) { CloudSyncEngine::instance().set_domain_excluded(entity, !on); });
    }

    refresh_btn_ = new QPushButton(tr("Refresh now"));
    refresh_btn_->setFixedWidth(140);
    refresh_btn_->setStyleSheet(btn_secondary_ss());
    vl->addWidget(refresh_btn_);

    vl->addStretch();

    auto& engine = CloudSyncEngine::instance();

    connect(master_toggle_, &QCheckBox::toggled, this, [this](bool on) {
        CloudSyncEngine::instance().set_master_enabled(on);
        update_enabled_state();
    });
    connect(refresh_btn_, &QPushButton::clicked, this, []() { CloudSyncEngine::instance().refresh_all(); });

    connect(&engine, &CloudSyncEngine::status_changed, this,
            [this](QString entity, CloudSyncEngine::Status status, int pending, QString error) {
                if (auto* lbl = domain_status_.value(entity, nullptr))
                    lbl->setText(cloud_status_text(status, pending, error));
            });
    connect(&engine, &CloudSyncEngine::credits_exhausted, this, [this]() {
        if (credits_banner_)
            credits_banner_->setVisible(true);
    });
    connect(&engine, &CloudSyncEngine::first_enable_conflict, this, [this](QString entity) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Cloud Sync"));
        box.setText(tr("You have local data and existing cloud data for \"%1\".").arg(entity));
        box.setInformativeText(tr("Upload & merge keeps your local items (recommended). "
                                  "Use cloud replaces this device's copy with your cloud account "
                                  "— a local backup is saved first."));
        auto* upload = box.addButton(tr("Upload && merge"), QMessageBox::AcceptRole);
        box.addButton(tr("Use cloud"), QMessageBox::DestructiveRole);
        box.exec();
        CloudSyncEngine::instance().resolve_first_enable(entity, box.clickedButton() == upload);
    });
}

void CloudSyncSection::update_enabled_state() {
    const bool can = CloudSyncEngine::instance().can_enable();
    if (master_toggle_)
        master_toggle_->setEnabled(can);
    if (sign_in_hint_)
        sign_in_hint_->setVisible(!can);
}

void CloudSyncSection::reload() {
    if (!master_toggle_)
        return;
    auto& engine = CloudSyncEngine::instance();
    {
        const QSignalBlocker b(master_toggle_);
        master_toggle_->setChecked(engine.master_enabled());
    }
    for (auto it = domain_toggles_.constBegin(); it != domain_toggles_.constEnd(); ++it) {
        const QSignalBlocker b(it.value());
        it.value()->setChecked(!fincept::CloudSyncSettings::is_domain_excluded(it.key()));
    }
    for (auto it = domain_status_.constBegin(); it != domain_status_.constEnd(); ++it)
        it.value()->setText(cloud_status_text(engine.status(it.key()), engine.pending_count(it.key()), {}));
    update_enabled_state();
}

void CloudSyncSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CloudSyncSection::retranslateUi() {
    if (title_) title_->setText(tr("Fincept Cloud Sync"));
    if (blurb_)
        blurb_->setText(tr("Mirror your data to your Fincept account. Your local copy stays the working "
                           "copy — sync runs in the background, on this device and across your devices."));
    if (credits_banner_) credits_banner_->setText(tr("Out of credits — top up to resume cloud sync."));
    if (master_toggle_)  master_toggle_->setText(tr("Enable cloud sync"));
    if (cloud_sync_label_) cloud_sync_label_->setText(tr("Cloud Sync"));
    if (cloud_sync_desc_)
        cloud_sync_desc_->setText(tr("When on, changes mirror to your account and pull on this device."));
    if (sign_in_hint_)   sign_in_hint_->setText(tr("Sign in to enable cloud sync."));
    if (adv_title_)      adv_title_->setText(tr("ADVANCED — DOMAINS"));

    // Per-domain rows: re-apply checkbox text, row label, and description.
    for (const DomainRow& d : cloud_domains()) {
        const QString entity = QString::fromLatin1(d.entity);
        if (auto* cb = domain_toggles_.value(entity, nullptr))
            cb->setText(tr("Sync this domain"));
        if (auto* lbl = domain_row_label_.value(entity, nullptr))
            lbl->setText(tr(d.label));
        if (auto* desc = domain_row_desc_.value(entity, nullptr))
            desc->setText(tr(d.desc));
    }

    if (refresh_btn_) refresh_btn_->setText(tr("Refresh now"));
}

} // namespace fincept::screens
