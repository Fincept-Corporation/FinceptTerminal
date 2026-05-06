#include "screens/recovery/CrashRecoveryDialog.h"

#include "core/layout/LayoutTypes.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr const char* kRecoveryTag = "CrashRecovery";
constexpr int kMaxListed = 5;

// Theme tokens — hardcoded for now because the dialog runs *before* any
// WindowFrame exists, so ui::ThemeManager may not be subscribable yet.
// Values match the project's "obsidian / amber" theme (see ui/theme/Theme.h).
constexpr const char* kBgBase    = "#0a0a0a";
constexpr const char* kBgSurface = "#111111";
constexpr const char* kBgRaised  = "#161616";
constexpr const char* kBorder    = "#1f1f1f";
constexpr const char* kText      = "#e5e5e5";
constexpr const char* kTextDim   = "#808080";
constexpr const char* kTextMuted = "#404040";
constexpr const char* kAmber     = "#d97706";
constexpr const char* kAmberDim  = "#a85a05";
constexpr const char* kRed       = "#dc2626";
} // namespace

namespace fincept::screens {

CrashRecoveryDialog::CrashRecoveryDialog(fincept::CrashRecovery* recovery,
                                         fincept::WorkspaceSnapshotRing* ring,
                                         QWidget* parent)
    : QDialog(parent), recovery_(recovery), ring_(ring) {
    setWindowTitle(QStringLiteral("Fincept Terminal — Recover Previous Session"));
    setObjectName(QStringLiteral("CrashRecoveryDialog"));
    setModal(true);
    resize(640, 480);

    build_ui();
    apply_styles();
    populate_snapshots();
}

void CrashRecoveryDialog::build_ui() {
    // Outer frame: zero margin so the title bar bleeds to the dialog edges.
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Title bar (amber accent stripe + title) ──────────────────────────
    auto* title_bar = new QWidget(this);
    title_bar->setObjectName(QStringLiteral("recoveryTitleBar"));
    title_bar->setFixedHeight(44);
    auto* tbl = new QHBoxLayout(title_bar);
    tbl->setContentsMargins(20, 0, 20, 0);
    tbl->setSpacing(12);

    heading_ = new QLabel(QStringLiteral("RECOVER PREVIOUS SESSION"), title_bar);
    heading_->setObjectName(QStringLiteral("recoveryHeading"));

    auto* badge = new QLabel(QStringLiteral("UNCLEAN SHUTDOWN DETECTED"), title_bar);
    badge->setObjectName(QStringLiteral("recoveryBadge"));

    tbl->addWidget(heading_);
    tbl->addStretch();
    tbl->addWidget(badge);
    root->addWidget(title_bar);

    // ── Body panel ───────────────────────────────────────────────────────
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("recoveryBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(20, 16, 20, 16);
    bl->setSpacing(12);

    explainer_ = new QLabel(body);
    explainer_->setObjectName(QStringLiteral("recoveryExplainer"));
    explainer_->setWordWrap(true);
    explainer_->setText(QStringLiteral(
        "Pick a snapshot to restore your workspace, or skip to start fresh. "
        "Skipping does not delete the snapshots."));
    bl->addWidget(explainer_);

    // Section header above the list — column labels, terminal-style.
    auto* col_header = new QWidget(body);
    col_header->setObjectName(QStringLiteral("recoveryColHeader"));
    col_header->setFixedHeight(24);
    auto* chl = new QHBoxLayout(col_header);
    chl->setContentsMargins(10, 0, 10, 0);
    auto* col_name = new QLabel(QStringLiteral("SESSION"), col_header);
    auto* col_meta = new QLabel(QStringLiteral("WHEN"), col_header);
    col_name->setObjectName(QStringLiteral("recoveryColLabel"));
    col_meta->setObjectName(QStringLiteral("recoveryColLabel"));
    chl->addWidget(col_name);
    chl->addStretch();
    chl->addWidget(col_meta);
    bl->addWidget(col_header);

    list_ = new QListWidget(body);
    list_->setObjectName(QStringLiteral("recoverySnapshotList"));
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setAlternatingRowColors(false);
    list_->setSpacing(0);
    list_->setFrameShape(QFrame::Box);
    connect(list_, &QListWidget::itemSelectionChanged, this,
            &CrashRecoveryDialog::on_selection_changed);
    connect(list_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { on_restore_clicked(); });

    empty_label_ = new QLabel(
        QStringLiteral("NO SNAPSHOTS AVAILABLE — START FRESH"), body);
    empty_label_->setObjectName(QStringLiteral("recoveryEmptyLabel"));
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->hide();

    bl->addWidget(list_, 1);
    bl->addWidget(empty_label_, 1);

    root->addWidget(body, 1);

    // ── Action bar at bottom (separator + buttons) ───────────────────────
    auto* action_bar = new QWidget(this);
    action_bar->setObjectName(QStringLiteral("recoveryActionBar"));
    action_bar->setFixedHeight(56);
    auto* abl = new QHBoxLayout(action_bar);
    abl->setContentsMargins(20, 12, 20, 12);
    abl->setSpacing(8);

    rename_button_ = new QPushButton(QStringLiteral("RENAME"), action_bar);
    rename_button_->setObjectName(QStringLiteral("recoverySecondaryButton"));
    rename_button_->setAutoDefault(false);
    rename_button_->setEnabled(false);
    connect(rename_button_, &QPushButton::clicked, this, &CrashRecoveryDialog::on_rename_clicked);

    delete_button_ = new QPushButton(QStringLiteral("DELETE"), action_bar);
    delete_button_->setObjectName(QStringLiteral("recoveryDangerButton"));
    delete_button_->setAutoDefault(false);
    delete_button_->setEnabled(false);
    connect(delete_button_, &QPushButton::clicked, this, &CrashRecoveryDialog::on_delete_clicked);

    skip_button_ = new QPushButton(QStringLiteral("SKIP"), action_bar);
    skip_button_->setObjectName(QStringLiteral("recoverySecondaryButton"));
    skip_button_->setAutoDefault(false);
    connect(skip_button_, &QPushButton::clicked, this, &CrashRecoveryDialog::on_skip_clicked);

    restore_button_ = new QPushButton(QStringLiteral("RESTORE"), action_bar);
    restore_button_->setObjectName(QStringLiteral("recoveryPrimaryButton"));
    restore_button_->setDefault(true);
    restore_button_->setEnabled(false);
    connect(restore_button_, &QPushButton::clicked, this, &CrashRecoveryDialog::on_restore_clicked);

    abl->addWidget(rename_button_);
    abl->addWidget(delete_button_);
    abl->addStretch();
    abl->addWidget(skip_button_);
    abl->addWidget(restore_button_);

    root->addWidget(action_bar);
}

void CrashRecoveryDialog::apply_styles() {
    // Fincept-terminal aesthetic: zero radii, sharp 1px borders, monospaced
    // tracking on labels. Single dialog stylesheet keyed by objectName so
    // children inherit without per-widget styling.
    setStyleSheet(QString(R"(
        QDialog#CrashRecoveryDialog {
            background: %1;
            color: %2;
        }

        /* Title bar — amber underline like a Fincept header */
        QWidget#recoveryTitleBar {
            background: %5;
            border-bottom: 2px solid %8;
        }
        QLabel#recoveryHeading {
            color: %2;
            font-size: 13px;
            font-weight: 700;
            letter-spacing: 1.5px;
        }
        QLabel#recoveryBadge {
            color: %1;
            background: %8;
            font-size: 9px;
            font-weight: 700;
            letter-spacing: 1px;
            padding: 3px 8px;
        }

        /* Body */
        QWidget#recoveryBody { background: %1; }
        QLabel#recoveryExplainer {
            color: %3;
            font-size: 11px;
            padding: 0 2px;
        }

        /* Column header strip above the list */
        QWidget#recoveryColHeader {
            background: %5;
            border: 1px solid %6;
            border-bottom: none;
        }
        QLabel#recoveryColLabel {
            color: %3;
            font-size: 9px;
            font-weight: 700;
            letter-spacing: 1px;
            background: transparent;
        }

        /* Empty state */
        QLabel#recoveryEmptyLabel {
            color: %4;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 1px;
            background: %5;
            border: 1px solid %6;
            padding: 32px;
        }

        /* Snapshot list — boxed, no rounding, sharp row dividers */
        QListWidget#recoverySnapshotList {
            background: %5;
            color: %2;
            border: 1px solid %6;
            border-radius: 0;
            padding: 0;
            outline: none;
        }
        QListWidget#recoverySnapshotList::item {
            color: %2;
            background: transparent;
            padding: 0;
            border-bottom: 1px solid %6;
            border-radius: 0;
        }
        QListWidget#recoverySnapshotList::item:hover {
            background: %7;
        }
        QListWidget#recoverySnapshotList::item:selected {
            background: %7;
            color: %8;
            border-left: 3px solid %8;
        }
        QListWidget#recoverySnapshotList::item:disabled {
            color: %4;
        }

        /* Action bar */
        QWidget#recoveryActionBar {
            background: %5;
            border-top: 1px solid %6;
        }

        /* Buttons — flat rectangles, zero radius */
        QPushButton {
            min-height: 30px;
            min-width: 90px;
            padding: 0 16px;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
            border-radius: 0;
        }
        QPushButton#recoveryPrimaryButton {
            background: %8;
            color: %1;
            border: 1px solid %8;
        }
        QPushButton#recoveryPrimaryButton:hover {
            background: %9;
            border-color: %9;
        }
        QPushButton#recoveryPrimaryButton:disabled {
            background: %6;
            color: %4;
            border-color: %6;
        }
        QPushButton#recoverySecondaryButton {
            background: %7;
            color: %2;
            border: 1px solid %6;
        }
        QPushButton#recoverySecondaryButton:hover {
            border-color: %3;
            color: %2;
        }
        QPushButton#recoverySecondaryButton:disabled {
            color: %4;
            border-color: %6;
        }
        QPushButton#recoveryDangerButton {
            background: transparent;
            color: %10;
            border: 1px solid %10;
        }
        QPushButton#recoveryDangerButton:hover {
            background: rgba(220, 38, 38, 0.12);
        }
        QPushButton#recoveryDangerButton:disabled {
            color: %4;
            border-color: %6;
        }

        QScrollBar:vertical {
            background: %5;
            width: 8px;
            border: none;
            border-radius: 0;
        }
        QScrollBar::handle:vertical {
            background: %6;
            border-radius: 0;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: %3;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )").arg(kBgBase, kText, kTextDim, kTextMuted, kBgSurface,
            kBorder, kBgRaised, kAmber, kAmberDim, kRed));
}

void CrashRecoveryDialog::populate_snapshots() {
    list_->clear();
    entries_.clear();

    if (!recovery_) {
        LOG_WARN(kRecoveryTag, "CrashRecoveryDialog has no recovery pointer — empty list");
        list_->hide();
        empty_label_->show();
        return;
    }

    auto r = recovery_->latest_snapshots(kMaxListed);
    if (r.is_err()) {
        LOG_ERROR(kRecoveryTag,
                  QString("latest_snapshots failed: %1").arg(QString::fromStdString(r.error())));
        list_->hide();
        empty_label_->show();
        return;
    }

    entries_ = r.value();
    if (entries_.isEmpty()) {
        list_->hide();
        empty_label_->show();
        return;
    }
    list_->show();
    empty_label_->hide();

    // Custom row widget: name left, timestamp right, kind badge. Two-line
    // layout for the left side keeps long names readable. QListWidgetItem
    // text alone doesn't render \n as a line break.
    for (const auto& e : entries_) {
        const QString name     = display_name(e);
        const QString relative = format_relative(e.created_at_ms);
        const QString absolute = format_timestamp(e.created_at_ms);
        const QString kind     = format_kind(e.kind);

        auto* row = new QWidget(list_);
        row->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->setStyleSheet("background: transparent;");

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(12, 8, 12, 8);
        hl->setSpacing(12);

        // Left column: name + kind badge.
        auto* left = new QVBoxLayout;
        left->setContentsMargins(0, 0, 0, 0);
        left->setSpacing(3);

        auto* name_lbl = new QLabel(name, row);
        name_lbl->setStyleSheet(
            QString("color:%1;font-size:12px;font-weight:600;background:transparent;")
                .arg(kText));

        auto* kind_lbl = new QLabel(kind.toUpper().remove('[').remove(']'), row);
        kind_lbl->setStyleSheet(
            QString("color:%1;background:%2;padding:1px 6px;font-size:8px;"
                    "font-weight:700;letter-spacing:1px;")
                .arg(kTextDim, kBgRaised));
        kind_lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        kind_lbl->setMaximumWidth(80);

        left->addWidget(name_lbl);
        left->addWidget(kind_lbl, 0, Qt::AlignLeft);
        hl->addLayout(left, 1);

        // Right column: relative + absolute timestamp.
        auto* right = new QVBoxLayout;
        right->setContentsMargins(0, 0, 0, 0);
        right->setSpacing(3);

        auto* rel_lbl = new QLabel(relative.toUpper(), row);
        rel_lbl->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:700;letter-spacing:0.5px;"
                    "background:transparent;")
                .arg(kAmber));
        rel_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* abs_lbl = new QLabel(absolute, row);
        abs_lbl->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;")
                .arg(kTextMuted));
        abs_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        right->addWidget(rel_lbl);
        right->addWidget(abs_lbl);
        hl->addLayout(right, 0);

        auto* item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, e.snapshot_id);
        item->setSizeHint(QSize(0, row->sizeHint().height() + 2));
        list_->setItemWidget(item, row);
    }

    list_->setCurrentRow(0); // pre-select newest
}

QString CrashRecoveryDialog::format_timestamp(qint64 unix_ms) const {
    if (unix_ms <= 0) return QStringLiteral("(unknown time)");
    const auto dt = QDateTime::fromMSecsSinceEpoch(unix_ms);
    return dt.toLocalTime().toString(QStringLiteral("MMM d, yyyy  h:mm AP"));
}

QString CrashRecoveryDialog::format_relative(qint64 unix_ms) const {
    if (unix_ms <= 0) return QStringLiteral("(unknown)");
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    qint64 diff = now_ms - unix_ms;
    if (diff < 0) diff = 0;
    const qint64 sec = diff / 1000;
    if (sec < 60) return QStringLiteral("just now");
    const qint64 min = sec / 60;
    if (min < 60) return min == 1 ? QStringLiteral("1 minute ago")
                                  : QString("%1 minutes ago").arg(min);
    const qint64 hr = min / 60;
    if (hr < 24) return hr == 1 ? QStringLiteral("1 hour ago")
                                : QString("%1 hours ago").arg(hr);
    const qint64 day = hr / 24;
    if (day == 1) return QStringLiteral("yesterday");
    if (day < 7)  return QString("%1 days ago").arg(day);
    // Older than a week — let the absolute timestamp carry the weight.
    return format_timestamp(unix_ms);
}

QString CrashRecoveryDialog::format_kind(const QString& kind) const {
    if (kind == QLatin1String("auto"))           return QStringLiteral("[autosave]");
    if (kind == QLatin1String("crash_recovery")) return QStringLiteral("[crash]");
    if (kind == QLatin1String("named_save"))     return QStringLiteral("[saved]");
    return QString("[%1]").arg(kind);
}

QString CrashRecoveryDialog::display_name(
    const fincept::WorkspaceSnapshotRing::Entry& e) const {
    if (!e.name.isEmpty())
        return e.name;
    // Auto-generate a unique, human-readable name from the timestamp so each
    // snapshot has a recognisable identifier. Users can override via Rename.
    if (e.created_at_ms <= 0)
        return QString("Session #%1").arg(e.snapshot_id);
    const auto dt = QDateTime::fromMSecsSinceEpoch(e.created_at_ms);
    return QString("Session — %1").arg(
        dt.toLocalTime().toString(QStringLiteral("MMM d, h:mm AP")));
}

void CrashRecoveryDialog::on_selection_changed() {
    const bool has = (list_->currentItem() != nullptr);
    restore_button_->setEnabled(has);
    rename_button_->setEnabled(has);
    delete_button_->setEnabled(has);
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

void CrashRecoveryDialog::on_rename_clicked() {
    auto* item = list_->currentItem();
    if (!item || !ring_)
        return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();

    // Find the current entry to seed the dialog with its current display name.
    QString current_name;
    for (const auto& e : entries_) {
        if (e.snapshot_id == id) {
            current_name = display_name(e);
            break;
        }
    }

    bool ok = false;
    const QString new_name = QInputDialog::getText(
        this, QStringLiteral("Rename Session"),
        QStringLiteral("Session name:"),
        QLineEdit::Normal, current_name, &ok);
    if (!ok)
        return;

    auto r = ring_->rename(id, new_name);
    if (r.is_err()) {
        LOG_ERROR(kRecoveryTag,
                  QString("rename(%1) failed: %2")
                      .arg(id).arg(QString::fromStdString(r.error())));
        QMessageBox::warning(this, QStringLiteral("Rename failed"),
                             QString::fromStdString(r.error()));
        return;
    }

    LOG_INFO(kRecoveryTag,
             QString("Renamed snapshot %1 → '%2'").arg(id).arg(new_name));
    populate_snapshots();
    // Re-select the renamed row so the user sees the new label immediately.
    for (int i = 0; i < list_->count(); ++i) {
        if (list_->item(i)->data(Qt::UserRole).toLongLong() == id) {
            list_->setCurrentRow(i);
            break;
        }
    }
}

void CrashRecoveryDialog::on_delete_clicked() {
    auto* item = list_->currentItem();
    if (!item || !ring_)
        return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();

    QString name;
    for (const auto& e : entries_) {
        if (e.snapshot_id == id) {
            name = display_name(e);
            break;
        }
    }

    const auto reply = QMessageBox::question(
        this, QStringLiteral("Delete snapshot"),
        QString("Delete \"%1\"?\n\nThis cannot be undone.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    auto r = ring_->erase(id);
    if (r.is_err()) {
        LOG_ERROR(kRecoveryTag,
                  QString("erase(%1) failed: %2")
                      .arg(id).arg(QString::fromStdString(r.error())));
        QMessageBox::warning(this, QStringLiteral("Delete failed"),
                             QString::fromStdString(r.error()));
        return;
    }

    LOG_INFO(kRecoveryTag, QString("Deleted snapshot %1 ('%2')").arg(id).arg(name));
    populate_snapshots();
}

} // namespace fincept::screens
