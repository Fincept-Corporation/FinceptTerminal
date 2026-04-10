// src/screens/settings/KeybindingsSection.cpp
#include "screens/settings/KeybindingsSection.h"

#include "core/keys/KeyConfigManager.h"
#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

// ── KeyCaptureDialog ──────────────────────────────────────────────────────────

KeyCaptureDialog::KeyCaptureDialog(KeyAction action, const QKeySequence& current, QWidget* parent)
    : QDialog(parent), action_(action) {
    setWindowTitle("Rebind: " + KeyConfigManager::instance().display_name(action));
    setFixedSize(360, 200);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* current_lbl = new QLabel("Current: " + current.toString(QKeySequence::NativeText));
    current_lbl->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_SECONDARY()));

    hint_label_ = new QLabel("Press new key combination...");
    hint_label_->setStyleSheet(QString("color:%1;font-weight:bold;").arg(ui::colors::TEXT_PRIMARY()));

    captured_label_ = new QLabel;
    captured_label_->setStyleSheet(
        QString("color:%1;font-size:16px;font-weight:700;").arg(ui::colors::AMBER()));
    captured_label_->setAlignment(Qt::AlignCenter);

    conflict_label_ = new QLabel;
    conflict_label_->setStyleSheet(QString("color:%1;").arg(ui::colors::NEGATIVE()));
    conflict_label_->hide();

    auto* btn_box = new QDialogButtonBox(Qt::Horizontal);
    apply_btn_ = btn_box->addButton("Apply", QDialogButtonBox::AcceptRole);
    btn_box->addButton("Cancel", QDialogButtonBox::RejectRole);
    apply_btn_->setEnabled(false);

    connect(btn_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(current_lbl);
    layout->addWidget(hint_label_);
    layout->addWidget(captured_label_);
    layout->addWidget(conflict_label_);
    layout->addStretch();
    layout->addWidget(btn_box);

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

void KeyCaptureDialog::keyPressEvent(QKeyEvent* event) {
    // Ignore lone modifier keys
    const int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift ||
        key == Qt::Key_Alt    || key == Qt::Key_Meta)
        return;

    captured_ = QKeySequence(event->keyCombination());
    captured_label_->setText(captured_.toString(QKeySequence::NativeText));
    apply_btn_->setEnabled(true);

    // Conflict check
    auto conflict = KeyConfigManager::instance().find_conflict(captured_, action_);
    if (conflict.has_value()) {
        const QString name = KeyConfigManager::instance().display_name(conflict.value());
        conflict_label_->setText("Warning: already used by \"" + name + "\"");
        conflict_label_->show();
    } else {
        conflict_label_->hide();
    }
}

// ── KeybindingsSection ────────────────────────────────────────────────────────

KeybindingsSection::KeybindingsSection(QWidget* parent) : QWidget(parent) {
    build_ui();

    // Rebuild rows whenever any key changes (live update)
    connect(&KeyConfigManager::instance(), &KeyConfigManager::key_changed,
            this, [this](KeyAction, QKeySequence) { rebuild_rows(); });
}

void KeybindingsSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // Search bar
    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Search actions...");
    search_input_->setStyleSheet(
        QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                "QLineEdit:focus{border:1px solid %4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BORDER_MED(), ui::colors::AMBER()));
    connect(search_input_, &QLineEdit::textChanged, this, [this](const QString&) { rebuild_rows(); });

    // Scrollable groups area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget;
    groups_layout_ = new QVBoxLayout(container);
    groups_layout_->setContentsMargins(0, 0, 0, 0);
    groups_layout_->setSpacing(16);
    scroll->setWidget(container);

    rebuild_rows();

    // Reset All button
    auto* reset_all_btn = new QPushButton("Reset All to Defaults");
    reset_all_btn->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 12px;height:32px;}"
                "QPushButton:hover{background:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BORDER_MED(), ui::colors::BG_HOVER()));
    connect(reset_all_btn, &QPushButton::clicked, this, []() {
        KeyConfigManager::instance().reset_all();
    });

    root->addWidget(search_input_);
    root->addWidget(scroll, 1);
    root->addWidget(reset_all_btn);
}

void KeybindingsSection::rebuild_rows() {
    // Clear existing group widgets
    while (QLayoutItem* item = groups_layout_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }

    const QString filter = search_input_ ? search_input_->text().toLower() : QString{};

    // Collect actions by group, preserving order
    QMap<QString, QList<KeyAction>> groups;
    for (KeyAction a : KeyConfigManager::instance().all_actions()) {
        const QString name = KeyConfigManager::instance().display_name(a);
        if (!filter.isEmpty() && !name.toLower().contains(filter))
            continue;
        groups[KeyConfigManager::instance().group_name(a)].append(a);
    }

    const QStringList group_order = {"Global", "Navigation", "News", "Code Editor"};
    for (const QString& group : group_order) {
        if (!groups.contains(group))
            continue;
        groups_layout_->addWidget(build_group(group, groups[group]));
    }
    groups_layout_->addStretch();
}

QWidget* KeybindingsSection::build_group(const QString& group_name, const QList<KeyAction>& actions) {
    auto* group_widget = new QWidget;
    auto* vbox = new QVBoxLayout(group_widget);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(2);

    auto* title = new QLabel(group_name.toUpper());
    title->setStyleSheet(
        QString("color:%1;font-weight:bold;letter-spacing:0.5px;padding:4px 0;")
            .arg(ui::colors::AMBER()));
    vbox->addWidget(title);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_DIM()));
    vbox->addWidget(sep);

    auto& km = KeyConfigManager::instance();
    for (KeyAction a : actions) {
        auto* row = new QWidget;
        row->setProperty("key_action_set", true);
        row->setProperty("key_action", static_cast<int>(a));
        row->setCursor(Qt::PointingHandCursor);
        row->installEventFilter(this);

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(4, 4, 4, 4);

        auto* name_lbl = new QLabel(km.display_name(a));
        name_lbl->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_PRIMARY()));

        auto* key_lbl = new QLabel(km.key(a).toString(QKeySequence::NativeText));
        key_lbl->setStyleSheet(
            QString("color:%1;font-family:monospace;min-width:120px;").arg(ui::colors::TEXT_SECONDARY()));
        key_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* reset_btn = new QPushButton("Reset");
        reset_btn->setFixedSize(56, 24);
        reset_btn->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:1px solid %3;}"
                    "QPushButton:hover{background:%4;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
                     ui::colors::BORDER_DIM(), ui::colors::BG_HOVER()));
        // Use a captured copy of a for the lambda
        connect(reset_btn, &QPushButton::clicked, this, [a]() {
            KeyConfigManager::instance().reset_key(a);
        });

        hl->addWidget(name_lbl, 1);
        hl->addWidget(key_lbl);
        hl->addWidget(reset_btn);

        vbox->addWidget(row);
    }

    return group_widget;
}

bool KeybindingsSection::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && w->property("key_action_set").toBool()) {
            const KeyAction a = static_cast<KeyAction>(w->property("key_action").toInt());
            auto& km = KeyConfigManager::instance();
            KeyCaptureDialog dlg(a, km.key(a), this);
            if (dlg.exec() == QDialog::Accepted && !dlg.captured().isEmpty())
                km.set_key(a, dlg.captured());
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace fincept::screens
