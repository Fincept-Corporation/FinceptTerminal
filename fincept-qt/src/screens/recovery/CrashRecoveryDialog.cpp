#include "screens/recovery/CrashRecoveryDialog.h"

#include "core/layout/LayoutTypes.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr const char* kRecoveryTag = "CrashRecovery";
constexpr int kMaxListed = 5;
} // namespace

namespace fincept::screens {

CrashRecoveryDialog::CrashRecoveryDialog(fincept::CrashRecovery* recovery,
                                         fincept::WorkspaceSnapshotRing* ring,
                                         QWidget* parent)
    : QDialog(parent), recovery_(recovery), ring_(ring) {
    setWindowTitle(QStringLiteral("Fincept Terminal — Recover Previous Session"));
    setObjectName(QStringLiteral("CrashRecoveryDialog"));
    setModal(true);
    resize(560, 420);

    build_ui();
    populate_snapshots();
}

void CrashRecoveryDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(12);

    heading_ = new QLabel(QStringLiteral("The previous session ended unexpectedly."), this);
    heading_->setObjectName(QStringLiteral("recoveryHeading"));
    QFont f = heading_->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    heading_->setFont(f);

    explainer_ = new QLabel(this);
    explainer_->setObjectName(QStringLiteral("recoveryExplainer"));
    explainer_->setWordWrap(true);
    explainer_->setText(QStringLiteral(
        "Fincept Terminal saves periodic snapshots of your workspace so you "
        "can recover from a crash, power loss, or process kill. Pick a "
        "snapshot to restore your windows and panels, or skip to start "
        "fresh. Skipping does not delete the snapshots."));

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("recoverySnapshotList"));
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setUniformItemSizes(true);
    list_->setAlternatingRowColors(true);
    connect(list_, &QListWidget::itemSelectionChanged, this,
            &CrashRecoveryDialog::on_selection_changed);
    connect(list_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { on_restore_clicked(); });

    auto* button_row = new QHBoxLayout();
    button_row->setContentsMargins(0, 0, 0, 0);
    button_row->addStretch();

    skip_button_ = new QPushButton(QStringLiteral("Skip"), this);
    skip_button_->setObjectName(QStringLiteral("recoverySkipButton"));
    skip_button_->setAutoDefault(false);
    connect(skip_button_, &QPushButton::clicked, this, &CrashRecoveryDialog::on_skip_clicked);

    restore_button_ = new QPushButton(QStringLiteral("Restore Selected"), this);
    restore_button_->setObjectName(QStringLiteral("recoveryRestoreButton"));
    restore_button_->setDefault(true);
    restore_button_->setEnabled(false);
    connect(restore_button_, &QPushButton::clicked, this, &CrashRecoveryDialog::on_restore_clicked);

    button_row->addWidget(skip_button_);
    button_row->addSpacing(8);
    button_row->addWidget(restore_button_);

    root->addWidget(heading_);
    root->addWidget(explainer_);
    root->addWidget(list_, 1);
    root->addLayout(button_row);
}

void CrashRecoveryDialog::populate_snapshots() {
    list_->clear();
    entries_.clear();

    if (!recovery_) {
        LOG_WARN(kRecoveryTag, "CrashRecoveryDialog has no recovery pointer — empty list");
        return;
    }

    auto r = recovery_->latest_snapshots(kMaxListed);
    if (r.is_err()) {
        LOG_ERROR(kRecoveryTag,
                  QString("latest_snapshots failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }

    entries_ = r.value();
    for (const auto& e : entries_) {
        const QString ts = format_timestamp(e.created_at_ms);
        const QString kind = format_kind(e.kind);
        QString label = e.name.isEmpty()
                            ? QString("%1   —   %2").arg(ts, kind)
                            : QString("%1   —   %2   —   %3").arg(ts, kind, e.name);
        auto* item = new QListWidgetItem(label, list_);
        item->setData(Qt::UserRole, e.snapshot_id);
    }

    if (!entries_.isEmpty()) {
        list_->setCurrentRow(0); // pre-select newest
    }
}

QString CrashRecoveryDialog::format_timestamp(qint64 unix_ms) const {
    if (unix_ms <= 0) return QStringLiteral("(unknown time)");
    const auto dt = QDateTime::fromMSecsSinceEpoch(unix_ms);
    return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd  hh:mm:ss"));
}

QString CrashRecoveryDialog::format_kind(const QString& kind) const {
    if (kind == QLatin1String("auto")) return QStringLiteral("autosave");
    if (kind == QLatin1String("crash_recovery")) return QStringLiteral("crash snapshot");
    if (kind == QLatin1String("named_save")) return QStringLiteral("named");
    return kind;
}

void CrashRecoveryDialog::on_selection_changed() {
    restore_button_->setEnabled(list_->currentItem() != nullptr);
}

void CrashRecoveryDialog::on_restore_clicked() {
    auto* item = list_->currentItem();
    if (!item || !ring_) {
        reject();
        return;
    }

    const qint64 id = item->data(Qt::UserRole).toLongLong();
    auto payload_r = ring_->load_payload(id);
    if (payload_r.is_err()) {
        LOG_ERROR(kRecoveryTag,
                  QString("load_payload(%1) failed: %2")
                      .arg(id)
                      .arg(QString::fromStdString(payload_r.error())));
        // Stay open so the user can pick a different snapshot.
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        return;
    }

    const QByteArray payload = payload_r.value();
    if (payload.isEmpty()) {
        LOG_WARN(kRecoveryTag, QString("snapshot %1 has empty payload — skipping").arg(id));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        return;
    }

    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_ERROR(kRecoveryTag,
                  QString("snapshot %1 JSON parse error: %2").arg(id).arg(err.errorString()));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        return;
    }

    layout::Workspace ws = layout::Workspace::from_json(doc.object());
    if (ws.frames.isEmpty()) {
        LOG_WARN(kRecoveryTag,
                 QString("snapshot %1 decoded with zero frames — nothing to restore").arg(id));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        return;
    }

    LOG_INFO(kRecoveryTag,
             QString("Restoring snapshot %1 ('%2', %3 frames)")
                 .arg(id)
                 .arg(ws.name.isEmpty() ? QStringLiteral("(unnamed)") : ws.name)
                 .arg(ws.frames.size()));

    const int applied = layout::WorkspaceShell::apply(ws);
    LOG_INFO(kRecoveryTag,
             QString("WorkspaceShell::apply returned %1/%2 frames")
                 .arg(applied)
                 .arg(ws.frames.size()));

    restored_ = (applied > 0);
    accept();
}

void CrashRecoveryDialog::on_skip_clicked() {
    LOG_INFO(kRecoveryTag, "User skipped crash recovery dialog");
    restored_ = false;
    reject();
}

} // namespace fincept::screens
