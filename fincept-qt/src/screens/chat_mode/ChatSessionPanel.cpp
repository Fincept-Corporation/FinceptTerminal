// ChatSessionPanel.cpp — the CHATS section of Chat Mode's left rail: the cloud
// conversations (2026-11-17 revamp: rail look, painted two-line rows, keyboard
// path; the panel's own EXIT button went to the command bar's WORKSPACE).
//
//   [+ NEW] [RENAME] [DELETE] [EXPORT]
//   [search conversations…]
//   title …………………………………… 14:02
//   12 messages · matches the search
//   [Load older conversations]
//   48 conversations · 1,204 messages

#include "screens/chat_mode/ChatSessionPanel.h"

#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QListWidgetItem>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSet>
#include <QStyledItemDelegate>
#include <QThreadPool>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::chat_mode {

namespace rv = fincept::chat_mode::run_view;

namespace chat_session_panel_detail {

constexpr int kCspUuidRole = Qt::UserRole;      ///< the session uuid (kept from before)
constexpr int kCspTitleRole = Qt::UserRole + 1; ///< the stored title (kept from before)
constexpr int kCspWhenRole = Qt::UserRole + 2;  ///< compact time of the last message
constexpr int kCspCountRole = Qt::UserRole + 3; ///< message count
constexpr int kCspHitRole = Qt::UserRole + 4;   ///< a message matches the server search

/// A message box shown with open(): window-modal but asynchronous — no nested
/// event loop (no exec()), deleted when closed.
QMessageBox* csp_message_box(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text,
                             QMessageBox::StandardButtons buttons = QMessageBox::Ok) {
    auto* box = new QMessageBox(icon, title, text, buttons, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    return box;
}

void csp_notify(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text) {
    csp_message_box(parent, icon, title, text)->open();
}

/// Asks (asynchronously) whether to delete `uuid` and emits delete_session_requested on Yes.
void csp_ask_delete(ChatSessionPanel* panel, const QString& uuid, const QString& stored_title) {
    if (uuid.isEmpty())
        return;
    const QString name = stored_title.isEmpty() ? ChatSessionPanel::tr("(Untitled)")
                                                 : chat_display_session_title(stored_title);
    QMessageBox* box = csp_message_box(panel, QMessageBox::Question, ChatSessionPanel::tr("Delete Conversation"),
                                       ChatSessionPanel::tr("Delete \"%1\"?").arg(name),
                                       QMessageBox::Yes | QMessageBox::No);
    box->setDefaultButton(QMessageBox::No);
    QPointer<ChatSessionPanel> self(panel);
    QObject::connect(box, &QMessageBox::buttonClicked, panel, [self, box, uuid](QAbstractButton* button) {
        if (self && box->standardButton(button) == QMessageBox::Yes)
            emit self->delete_session_requested(uuid);
    });
    box->open();
}

/// A server timestamp (ISO 8601, UTC) as a compact local time: "14:02" today,
/// "Mon" this week, "12 Nov" before; empty when unreadable.
QString csp_when(const QString& iso) {
    const QString s = iso.trimmed();
    if (s.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid()) {
        static const QRegularExpression kMicros(QStringLiteral("(\\.\\d{3})\\d+"));
        QString trimmed = s;
        trimmed.replace(kMicros, QStringLiteral("\\1"));
        dt = QDateTime::fromString(trimmed, Qt::ISODateWithMs);
    }
    if (!dt.isValid())
        return {};
    if (dt.timeSpec() == Qt::LocalTime) // no offset in the text: the server writes UTC
        dt.setTimeZone(QTimeZone::utc());
    const QDateTime local = dt.toLocalTime();
    const QDate today = QDate::currentDate();
    if (local.date() == today)
        return QLocale().toString(local.time(), QStringLiteral("HH:mm"));
    if (local.date() >= today.addDays(-6))
        return QLocale().toString(local, QStringLiteral("ddd"));
    return QLocale().toString(local.date(), QStringLiteral("dd MMM"));
}

QString csp_style() {
    return QStringLiteral(
               "#chatSessionPanel{background:%1;}"
               "#chatSessionPanel QLabel{background:transparent;border:none;color:%3;font-size:%9px;}"
               "#chatSessionPanel QLabel#cspStats{color:%10;font-size:%13px;}"
               "#chatSessionPanel QLineEdit#cspSearch{background:%4;color:%2;border:1px solid %5;padding:4px 8px;"
               "font-size:%11px;}"
               "#chatSessionPanel QLineEdit#cspSearch:focus{border:1px solid %6;}"
               "#chatSessionPanel QPushButton{background:%4;color:%3;border:1px solid %5;padding:3px 10px;"
               "font-size:%9px;font-weight:700;}"
               "#chatSessionPanel QPushButton:hover{color:%2;border-color:%6;}"
               "#chatSessionPanel QPushButton:focus{border-color:%7;}"
               "#chatSessionPanel QPushButton:disabled{color:%10;border-color:%5;}"
               "#chatSessionPanel QPushButton#cspNew{background:%7;color:%8;border-color:%7;}"
               "#chatSessionPanel QPushButton#cspNew:hover{background:%12;border-color:%12;}"
               "#chatSessionPanel QPushButton#cspMore{background:transparent;font-weight:400;}"
               "#chatSessionPanel QListWidget#cspList{background:%1;border:none;border-top:1px solid %5;outline:none;}"
               "#chatSessionPanel QScrollBar:vertical{background:transparent;width:6px;}"
               "#chatSessionPanel QScrollBar::handle:vertical{background:%5;min-height:20px;}"
               "#chatSessionPanel QScrollBar::add-line:vertical,"
               "#chatSessionPanel QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT(), ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(),
             ui::fonts::px(-3), ui::colors::TEXT_DIM(), ui::fonts::px(-2), ui::colors::AMBER_DIM(), ui::fonts::px(-4));
}

/// Two painted lines per conversation: title ……… time / N messages (· search hit).
/// One height for every row, from cached font metrics.
class CspDelegate : public QStyledItemDelegate {
  public:
    explicit CspDelegate(QObject* parent) : QStyledItemDelegate(parent) { refresh_fonts(); }

    void refresh_fonts() {
        title_font_ = ui::fonts::font(-2, QFont::DemiBold);
        meta_font_ = ui::fonts::font(-3);
        const QFontMetrics tfm(title_font_);
        const QFontMetrics mfm(meta_font_);
        pad_v_ = std::max(5, mfm.height() / 3);
        line1_h_ = std::max(tfm.height(), mfm.height());
        line2_h_ = mfm.height();
        height_ = pad_v_ + line1_h_ + 3 + line2_h_ + pad_v_ + 1;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        return QSize(option.rect.width(), height_);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const QRect rect = option.rect;
        p->save();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool enabled = index.flags() & Qt::ItemIsEnabled;
        QColor bg(ui::colors::BG_SURFACE());
        if (selected)
            bg = QColor(ui::colors::BG_HOVER());
        else if (hovered && enabled)
            bg = QColor(ui::colors::BG_RAISED());
        p->fillRect(rect, bg);
        if (selected)
            p->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), QColor(ui::colors::AMBER()));

        const bool hit = index.data(kCspHitRole).toBool();
        const int left = rect.left() + 10;
        const int right = rect.right() - 8;
        const QFontMetrics tfm(title_font_);
        const QFontMetrics mfm(meta_font_);
        const int top1 = rect.top() + pad_v_;
        const QString when = index.data(kCspWhenRole).toString();
        const int when_w = when.isEmpty() ? 0 : mfm.horizontalAdvance(when);
        p->setFont(meta_font_);
        p->setPen(QColor(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY()));
        if (!when.isEmpty())
            p->drawText(QRect(right - when_w, top1, when_w, line1_h_), Qt::AlignRight | Qt::AlignVCenter, when);
        const int tw = std::max(10, right - when_w - (when_w > 0 ? 8 : 0) - left);
        p->setFont(title_font_);
        p->setPen(hit ? QColor(ui::colors::AMBER()) : QColor(ui::colors::TEXT_PRIMARY()));
        p->drawText(QRect(left, top1, tw, line1_h_), Qt::AlignLeft | Qt::AlignVCenter,
                    tfm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, tw));

        const int top2 = top1 + line1_h_ + 3;
        QString meta;
        const QVariant count = index.data(kCspCountRole);
        if (count.isValid())
            meta = ChatSessionPanel::tr("%n message(s)", nullptr, count.toInt());
        if (hit)
            meta += (meta.isEmpty() ? QString() : QStringLiteral(" · ")) + ChatSessionPanel::tr("matches the search");
        p->setFont(meta_font_);
        p->setPen(QColor(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY()));
        p->drawText(QRect(left, top2, right - left, line2_h_), Qt::AlignLeft | Qt::AlignVCenter,
                    mfm.elidedText(meta, Qt::ElideRight, std::max(10, right - left)));

        if (option.state & QStyle::State_HasFocus) {
            p->setPen(QColor(ui::colors::BORDER_BRIGHT()));
            p->setBrush(Qt::NoBrush);
            p->drawRect(rect.adjusted(0, 0, -1, -1));
        }
        p->setPen(QColor(ui::colors::BORDER_DIM()));
        p->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
        p->restore();
    }

  private:
    QFont title_font_;
    QFont meta_font_;
    int pad_v_ = 6;
    int line1_h_ = 16;
    int line2_h_ = 14;
    int height_ = 42;
};

} // namespace chat_session_panel_detail

using namespace chat_session_panel_detail;

ChatSessionPanel::ChatSessionPanel(QWidget* parent) : QWidget(parent) {
    session_limit_ = ChatModeService::kSessionPageSize;
    build_ui();
    retranslateUi();
    // No refresh_sessions() here (§P3). WindowFrame builds ChatModeScreen
    // eagerly at startup, so this GET fired before first paint and before
    // login — an empty X-API-Key that could only 401. ChatModeScreen::showEvent
    // already calls session_panel_->refresh_sessions() on every entry into chat
    // mode, which is the only moment the list is actually looked at.
}

void ChatSessionPanel::build_ui() {
    // One selector stylesheet, before any child exists (the central re-style pass
    // keeps it in step with theme and font); rows are painted from the live theme.
    setObjectName(QStringLiteral("chatSessionPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(csp_style());

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 8, 8, 6);
    vl->setSpacing(6);

    // Actions: they wrap at large font sizes instead of clipping.
    auto* actions_host = new QWidget(this);
    auto* actions = new rv::RvFlowLayout(actions_host, 4, 4);
    const auto mk_btn = [actions_host, actions](const char* object_name) {
        auto* btn = new QPushButton(actions_host);
        if (object_name)
            btn->setObjectName(QLatin1String(object_name));
        btn->setCursor(Qt::PointingHandCursor);
        actions->addWidget(btn);
        return btn;
    };
    new_btn_ = mk_btn("cspNew");
    rename_btn_ = mk_btn(nullptr);
    rename_btn_->setEnabled(false);
    delete_btn_ = mk_btn(nullptr);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the selected conversations
    delete_btn_->setEnabled(false);
    export_btn_ = mk_btn(nullptr);
    vl->addWidget(actions_host);

    // Search
    search_edit_ = new QLineEdit(this);
    search_edit_->setObjectName(QStringLiteral("cspSearch"));
    search_edit_->setClearButtonEnabled(true);
    connect(search_edit_, &QLineEdit::textChanged, this, &ChatSessionPanel::on_search_changed);
    vl->addWidget(search_edit_);

    // Debounce timer for server-side search
    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(400);
    connect(search_timer_, &QTimer::timeout, this, &ChatSessionPanel::on_search_server);

    // Session list: painted two-line rows (CspDelegate).
    session_list_ = new QListWidget(this);
    session_list_->setObjectName(QStringLiteral("cspList"));
    session_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    session_list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    session_list_->setUniformItemSizes(true);
    session_list_->setMouseTracking(true);
    auto* delegate = new CspDelegate(session_list_);
    session_list_->setItemDelegate(delegate);
    connect(session_list_, &QListWidget::itemClicked, this, &ChatSessionPanel::on_item_clicked);
    // Enter (or a double click) opens the highlighted conversation; a second
    // activation of the one already open does not reload it.
    connect(session_list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (item && item->data(kCspUuidRole).toString() != active_uuid_)
            on_item_clicked(item);
    });
    vl->addWidget(session_list_, 1);

    // Paging: shown while the server may hold older conversations than the list.
    more_btn_ = new QPushButton(this);
    more_btn_->setObjectName(QStringLiteral("cspMore"));
    more_btn_->setCursor(Qt::PointingHandCursor);
    more_btn_->setVisible(false);
    connect(more_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_load_more);
    vl->addWidget(more_btn_);

    // Stats footer
    stats_lbl_ = new QLabel(this);
    stats_lbl_->setObjectName(QStringLiteral("cspStats"));
    stats_lbl_->setAlignment(Qt::AlignCenter);
    vl->addWidget(stats_lbl_);

    connect(new_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_new_clicked);
    connect(delete_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_delete_clicked);
    connect(rename_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_rename_clicked);
    connect(export_btn_, &QPushButton::clicked, this, &ChatSessionPanel::on_export_clicked);

    // Keyboard: Delete deletes the highlighted conversation (asks first), Esc clears
    // the search, Down moves from the search into the list.
    new rv::RvKeyHook(session_list_, [this](const QKeyEvent* ke, bool apply) {
        if (ke->key() != Qt::Key_Delete || ke->modifiers() != Qt::NoModifier)
            return false;
        QListWidgetItem* item = session_list_->currentItem();
        const QString uuid = item ? item->data(kCspUuidRole).toString() : QString();
        if (uuid.isEmpty())
            return false;
        if (apply)
            csp_ask_delete(this, uuid, item->data(kCspTitleRole).toString());
        return true;
    });
    new rv::RvKeyHook(search_edit_, [this](const QKeyEvent* ke, bool apply) {
        if (ke->key() == Qt::Key_Escape && !search_edit_->text().isEmpty()) {
            if (apply)
                search_edit_->clear();
            return true;
        }
        if (ke->key() == Qt::Key_Down && ke->type() == QEvent::KeyPress) {
            if (apply)
                focus_list();
            return apply;
        }
        return false;
    });

    // The painted rows re-measure once a new theme / font has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this, delegate]() {
        delegate->refresh_fonts();
        if (session_list_->count() > 0)
            emit delegate->sizeHintChanged(session_list_->model()->index(0, 0));
        session_list_->viewport()->update();
    });
}

void ChatSessionPanel::refresh_sessions() {
    // P8: the callback is owned by the ChatModeService singleton, which outlives
    // this panel. Guard so a window closed mid-request doesn't touch a dead
    // widget (refresh_sessions fires on every stream finish + session mutation).
    QPointer<ChatSessionPanel> self = this;
    // Refreshes overlap (every stream finish, every session mutation, "load more"); an
    // older, shorter answer arriving last would drop the conversations just loaded.
    const int seq = ++list_seq_;
    ChatModeService::instance().list_sessions_page(
        std::min(session_limit_, kMaxWindow), 0,
        [this, self, seq](bool ok, ChatSessionsPage page, QString err) {
        if (!self || seq != list_seq_)
            return;
        loading_more_ = false;
        if (!ok) {
            LOG_DEBUG("ChatSessionPanel", "Failed to load sessions: " + err); // ChatModeService logs the failure
            update_more_button();
            return;
        }
        server_pages_ = page.has_more_known;
        has_more_ = page.has_more;
        QVector<ChatSession> fresh = std::move(page.sessions);
        // A window larger than the server's cap comes back cut: keep the older
        // conversations loaded earlier instead of dropping them from the list.
        if (server_pages_ && page.has_more && sessions_.size() > fresh.size()) {
            QSet<QString> listed;
            for (const auto& s : std::as_const(fresh))
                listed.insert(s.uuid);
            for (qsizetype i = fresh.size(); i < sessions_.size(); ++i) {
                if (!listed.contains(sessions_[i].uuid))
                    fresh.append(sessions_[i]);
            }
        }
        sessions_ = std::move(fresh);
        // Through the filter, not around it: every stream finish refreshes the list, and
        // populating from the full set dropped whatever the user had typed in the search box.
        apply_filter(search_edit_->text());
        update_more_button();
    });
}

void ChatSessionPanel::focus_list() {
    if (session_list_->count() == 0) {
        new_btn_->setFocus(Qt::OtherFocusReason); // nothing listed: start one
        return;
    }
    if (!session_list_->currentItem())
        session_list_->setCurrentRow(0, QItemSelectionModel::NoUpdate);
    session_list_->setFocus(Qt::OtherFocusReason);
}

void ChatSessionPanel::export_conversations() {
    on_export_clicked();
}

void ChatSessionPanel::update_more_button() {
    if (!more_btn_)
        return;
    // Paging servers say whether older conversations exist (`has_more`). Older ones
    // do not: a full window may mean more, a short one means we have them all.
    // (/chat/stats is not used to decide: it is fetched once on entry and goes stale
    // as conversations are created.)
    more_btn_->setVisible(server_pages_ ? has_more_ : sessions_.size() >= session_limit_);
    more_btn_->setEnabled(!loading_more_);
    more_btn_->setText(loading_more_ ? tr("Loading…") : tr("Load older conversations"));
}

void ChatSessionPanel::on_load_more() {
    if (loading_more_)
        return;
    loading_more_ = true;
    session_limit_ += ChatModeService::kSessionPageSize;
    update_more_button();
    if (!server_pages_) {
        refresh_sessions(); // older server: re-read the grown window
        return;
    }
    // Only the next page. A refresh started meanwhile (stream finish) supersedes this
    // request, and re-reads the grown window itself.
    QPointer<ChatSessionPanel> self = this;
    const int seq = ++list_seq_;
    ChatModeService::instance().list_sessions_page(
        ChatModeService::kSessionPageSize, static_cast<int>(sessions_.size()),
        [this, self, seq](bool ok, ChatSessionsPage page, QString err) {
        if (!self || seq != list_seq_)
            return;
        loading_more_ = false;
        if (!ok) {
            LOG_WARN("ChatSessionPanel", "Failed to load older sessions: " + err);
            update_more_button();
            return;
        }
        // A conversation that moved to the top since the last read shifts the pages by
        // one: skip what is listed already.
        QSet<QString> listed;
        for (const auto& s : std::as_const(sessions_))
            listed.insert(s.uuid);
        for (const auto& s : std::as_const(page.sessions)) {
            if (!listed.contains(s.uuid))
                sessions_.append(s);
        }
        server_pages_ = page.has_more_known;
        has_more_ = page.has_more_known ? page.has_more
                                        : page.sessions.size() >= ChatModeService::kSessionPageSize;
        apply_filter(search_edit_->text());
        update_more_button();
    });
}

void ChatSessionPanel::populate_list(const QVector<ChatSession>& sessions) {
    // Keep the reading position: a refresh after every reply re-fills the list.
    const int scroll = session_list_->verticalScrollBar() ? session_list_->verticalScrollBar()->value() : 0;
    session_list_->clear();
    for (const auto& s : sessions) {
        auto* item = new QListWidgetItem(session_list_);
        const QString display = s.title.isEmpty() ? tr("(Untitled)") : chat_display_session_title(s.title);
        const QString when = csp_when(!s.last_message_at.isEmpty() ? s.last_message_at
                                      : !s.updated_at.isEmpty()   ? s.updated_at
                                                                  : s.created_at);
        item->setText(display);
        item->setData(kCspUuidRole, s.uuid);
        item->setData(kCspTitleRole, s.title);
        item->setData(kCspWhenRole, when);
        item->setData(kCspCountRole, s.message_count);
        item->setData(kCspHitRole, search_hits_.contains(s.uuid));
        item->setData(Qt::AccessibleTextRole, tr("%1, %n message(s), %2", nullptr, s.message_count).arg(display, when));
        item->setToolTip(display);
        if (s.uuid == active_uuid_) {
            item->setSelected(true);
            session_list_->setCurrentItem(item);
        }
    }
    if (session_list_->verticalScrollBar())
        session_list_->verticalScrollBar()->setValue(scroll);
    const bool has_sel = !active_uuid_.isEmpty();
    delete_btn_->setEnabled(has_sel);
    rename_btn_->setEnabled(has_sel);
}

void ChatSessionPanel::apply_filter(const QString& text) {
    if (text.trimmed().isEmpty()) {
        populate_list(sessions_);
        return;
    }
    QVector<ChatSession> filtered;
    for (const auto& s : sessions_) {
        if (s.title.contains(text, Qt::CaseInsensitive) || search_hits_.contains(s.uuid))
            filtered.append(s);
    }
    populate_list(filtered);
}

void ChatSessionPanel::set_active_session(const QString& uuid) {
    active_uuid_ = uuid;
    for (int i = 0; i < session_list_->count(); ++i) {
        auto* item = session_list_->item(i);
        const bool match = item->data(kCspUuidRole).toString() == uuid;
        item->setSelected(match);
        if (match)
            session_list_->setCurrentItem(item);
    }
    if (uuid.isEmpty())
        session_list_->clearSelection();
    delete_btn_->setEnabled(!uuid.isEmpty());
    rename_btn_->setEnabled(!uuid.isEmpty());
}

void ChatSessionPanel::update_stats(const ChatStats& stats) {
    last_stats_ = stats;
    stats_lbl_->setText(tr("%1 conversations · %2 messages")
                            .arg(QLocale().toString(stats.total_sessions), QLocale().toString(stats.total_messages)));
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ChatSessionPanel::on_item_clicked(QListWidgetItem* item) {
    if (!item)
        return;
    const QString uuid = item->data(kCspUuidRole).toString();
    if (uuid.isEmpty())
        return;
    active_uuid_ = uuid;
    delete_btn_->setEnabled(true);
    rename_btn_->setEnabled(true);
    emit session_selected(uuid);
}

void ChatSessionPanel::on_new_clicked() {
    emit new_session_requested();
}

void ChatSessionPanel::on_delete_clicked() {
    if (active_uuid_.isEmpty())
        return;
    QString title;
    for (const auto& s : sessions_) {
        if (s.uuid == active_uuid_) {
            title = s.title;
            break;
        }
    }
    // Asynchronous confirmation (open(), not exec()): the uuid is captured now, so
    // a selection change while the box is up cannot redirect the delete.
    csp_ask_delete(this, active_uuid_, title);
}

void ChatSessionPanel::on_rename_clicked() {
    if (active_uuid_.isEmpty())
        return;

    QString current_title;
    for (const auto& s : sessions_) {
        if (s.uuid == active_uuid_) {
            current_title = chat_display_session_title(s.title);
            break;
        }
    }

    // Asynchronous (open(), not exec()); the uuid is captured now.
    const QString uuid = active_uuid_;
    auto* dlg = new QInputDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Rename Conversation"));
    dlg->setLabelText(tr("New title:"));
    dlg->setInputMode(QInputDialog::TextInput);
    dlg->setTextValue(current_title);
    connect(dlg, &QInputDialog::textValueSelected, this, [this, uuid](const QString& new_title) {
        if (!new_title.trimmed().isEmpty())
            emit rename_session_requested(uuid, new_title.trimmed());
    });
    dlg->open();
}

void ChatSessionPanel::on_export_clicked() {
    // Export ALL conversations. sessions_ is only the loaded window (the first 50
    // until "Load older conversations"), so its uuids exported a subset while the
    // message said "Exported N conversations". An empty uuid list makes
    // POST /chat/export return every conversation of the user.
    if (sessions_.isEmpty()) {
        csp_notify(this, QMessageBox::Information, tr("Export"), tr("No conversations to export."));
        return;
    }
    if (!export_btn_->isEnabled())
        return; // an export is already on its way

    // Default file name is a stable artifact name, not translated; the dialog
    // caption and filter are user-facing. Asynchronous: open(), not exec().
    auto* dlg = new QFileDialog(this, tr("Export Conversations"), QStringLiteral("fincept_chat_export.json"),
                                tr("JSON (*.json)"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAcceptMode(QFileDialog::AcceptSave);
    dlg->setDefaultSuffix(QStringLiteral("json"));
    connect(dlg, &QFileDialog::fileSelected, this, [this](const QString& path) {
        if (path.isEmpty())
            return;
        export_btn_->setEnabled(false);
        export_btn_->setText(tr("EXPORTING…"));

        const QStringList uuids;
        QPointer<ChatSessionPanel> self = this;
        ChatModeService::instance().export_sessions(uuids, [this, self, path](bool ok, QJsonArray payload, QString err) {
            if (!self)
                return;
            export_btn_->setEnabled(true);
            export_btn_->setText(tr("EXPORT"));
            if (!ok) {
                csp_notify(this, QMessageBox::Warning, tr("Export Failed"), err);
                return;
            }
            // Serialised and written on a worker (an export can be large), atomically:
            // a failed write never leaves a half file behind and is never reported as done.
            export_btn_->setEnabled(false);
            QThreadPool::globalInstance()->start([self, path, payload]() {
                QSaveFile file(path);
                QString error;
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    error = file.errorString();
                } else {
                    const QByteArray bytes = QJsonDocument(payload).toJson(QJsonDocument::Indented);
                    if (file.write(bytes) != bytes.size() || !file.commit())
                        error = file.errorString();
                }
                if (!error.isEmpty())
                    LOG_WARN("ChatSessionPanel", QString("Conversation export failed path=%1 error=\"%2\"")
                                                     .arg(privacy::redact_home(path), error));
                const int count = static_cast<int>(payload.size());
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [self, error, count]() {
                        if (!self)
                            return;
                        self->export_btn_->setEnabled(true);
                        if (!error.isEmpty())
                            csp_notify(self, QMessageBox::Warning, tr("Export Failed"),
                                       tr("Could not write the file: %1").arg(error));
                        else
                            csp_notify(self, QMessageBox::Information, tr("Export"),
                                       tr("Exported %1 conversations.").arg(count));
                    },
                    Qt::QueuedConnection);
            });
        });
    });
    dlg->open();
}

void ChatSessionPanel::on_search_changed(const QString& text) {
    // Hits belong to the previous text; the debounced server search below refills them.
    search_hits_.clear();
    // Local filter immediately
    apply_filter(text);
    // Debounce server-side search for better results
    if (text.trimmed().length() >= 2)
        search_timer_->start();
    else
        search_timer_->stop();
}

void ChatSessionPanel::on_search_server() {
    const QString query = search_edit_->text().trimmed();
    if (query.length() < 2)
        return;

    QPointer<ChatSessionPanel> self = this;
    ChatModeService::instance().search_messages(query, [this, self, query](bool ok, QVector<ChatMessage> results, QString) {
        if (!self || !ok || results.isEmpty())
            return;
        // The box moved on while the request was out (a newer query has its own search
        // in flight) — don't mark the list for text that is no longer typed.
        if (search_edit_->text().trimmed() != query)
            return;
        // Sessions that have a matching MESSAGE (the title filter alone would hide them).
        QSet<QString> hit_sessions;
        for (const auto& r : results) {
            if (!r.session_uuid.isEmpty())
                hit_sessions.insert(r.session_uuid);
        }
        if (hit_sessions.isEmpty())
            return;
        // Re-filter so sessions matched only by message content join the list and are
        // painted as search hits.
        search_hits_ = hit_sessions;
        apply_filter(search_edit_->text());
    });
}

void ChatSessionPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ChatSessionPanel::retranslateUi() {
    if (search_edit_) {
        search_edit_->setPlaceholderText(tr("Search conversations…"));
        search_edit_->setAccessibleName(tr("Search conversations by title or message"));
    }
    if (session_list_) {
        session_list_->setAccessibleName(tr("Conversations"));
        session_list_->setAccessibleDescription(tr("Enter opens the conversation, Delete deletes it (asks first)."));
    }
    if (new_btn_) {
        new_btn_->setText(tr("+ NEW"));
        new_btn_->setToolTip(tr("New conversation"));
        new_btn_->setAccessibleName(tr("New conversation"));
    }
    if (rename_btn_) {
        rename_btn_->setText(tr("RENAME"));
        rename_btn_->setToolTip(tr("Rename the open conversation"));
        rename_btn_->setAccessibleName(tr("Rename the open conversation"));
    }
    if (delete_btn_) {
        delete_btn_->setText(tr("DELETE"));
        delete_btn_->setToolTip(tr("Delete the open conversation (asks first)"));
        delete_btn_->setAccessibleName(tr("Delete the open conversation"));
    }
    if (export_btn_) {
        if (export_btn_->isEnabled())
            export_btn_->setText(tr("EXPORT"));
        export_btn_->setToolTip(tr("Export every conversation as a JSON file"));
        export_btn_->setAccessibleName(tr("Export conversations"));
    }
    if (stats_lbl_)
        stats_lbl_->setText(tr("%1 conversations · %2 messages")
                                .arg(QLocale().toString(last_stats_.total_sessions),
                                     QLocale().toString(last_stats_.total_messages)));
    update_more_button();
    // Re-render the session list so "(Untitled)" / the counts pick up the language.
    apply_filter(search_edit_ ? search_edit_->text() : QString());
}

} // namespace fincept::chat_mode
