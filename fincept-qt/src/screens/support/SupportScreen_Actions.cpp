// src/screens/support/SupportScreen_Actions.cpp
//
// Ticket data flow: load_tickets, rebuild_ticket_rows, open_ticket /
// load_ticket_details, on_create_ticket, on_send_message, on_close_ticket,
// on_reopen_ticket.
//
// Part of the partial-class split of SupportScreen.cpp.

#include "auth/UserApi.h"
#include "screens/support/SupportScreen.h"
#include "screens/support/SupportScreen_internal.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::screens::support_internal;

namespace {

// Pull an array out of a {"data": {...}} / flat envelope under the first key present.
QJsonArray support_array(QJsonObject payload, std::initializer_list<const char*> keys) {
    if (payload.value("data").isObject())
        payload = payload.value("data").toObject();
    for (const char* key : keys) {
        if (payload.value(QLatin1String(key)).isArray())
            return payload.value(QLatin1String(key)).toArray();
    }
    return {};
}

// The API sends naive UTC timestamps (no offset); read them as UTC, show local.
QDateTime support_parse_utc(const QString& iso) {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return dt;
    const QString tail = iso.mid(10);
    if (!tail.contains('Z') && !tail.contains('+') && !tail.contains('-'))
        dt = QDateTime(dt.date(), dt.time(), QTimeZone::utc());
    return dt.toLocalTime();
}

// Conversation bubble. Message text is user / staff authored: plain text only.
QWidget* support_message_bubble(QWidget* parent, const QString& sender, const QString& color, const QString& when,
                                const QString& text) {
    auto* m = new QWidget(parent);
    m->setStyleSheet(QString("background:%1;border:1px solid %2;border-left:3px solid %3;")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), color));
    auto* ml = new QVBoxLayout(m);
    ml->setContentsMargins(14, 10, 14, 12);
    ml->setSpacing(6);
    auto* mh = new QHBoxLayout;
    mh->addWidget(lbl(sender, color, 11, true));
    mh->addStretch();
    mh->addWidget(lbl(when, ui::colors::TEXT_TERTIARY(), 10));
    ml->addLayout(mh);
    auto* mb = lbl(text, ui::colors::TEXT_PRIMARY(), 12, false, true);
    mb->setTextFormat(Qt::PlainText);
    mb->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ml->addWidget(mb);
    return m;
}

} // namespace

void SupportScreen::load_tickets() {
    // Never asked while signed out (the requirement page shows instead).
    if (account_key_.isEmpty())
        return;
    last_load_ms_ = QDateTime::currentMSecsSinceEpoch();
    list_in_flight_ = true;
    set_busy(true);
    if (all_tickets_.isEmpty()) {
        // Nothing on screen yet (first load, or RETRY after a failure): skeletons
        // in the list and the content pane instead of the old error text.
        load_failed_ = false;
        rebuild_ticket_rows();
        update_home_page();
    }

    QPointer<SupportScreen> self(this);
    const quint64 generation = account_generation_;
    auth::UserApi::instance().get_tickets(
        [self, generation](auth::ApiResponse r) {
            if (!self)
                return; // screen closed while the request was in flight
            if (self->account_generation_ != generation)
                return; // signed out / another account meanwhile: not this list
            self->list_in_flight_ = false;
            self->set_busy(false);

            if (!r.success) {
                // Keep the last list we had (P11) and say the refresh failed. The
                // old code replaced the list with a canned demo ticket, which also
                // hid the "could not load" state entirely.
                self->load_failed_ = true;
                self->last_error_ = r.error;
                self->open_after_load_id_ = -1;
                self->status_lbl_->setText(self->all_tickets_.isEmpty() ? tr("Offline")
                                                                         : tr("Offline — showing last loaded"));
                self->status_lbl_->setToolTip(r.error);
                support_set_tone(self->status_lbl_, "bad");
                support_set_tone(self->status_dot_, "bad");
                self->rebuild_ticket_rows();
                self->update_home_page();
                return;
            }
            self->load_failed_ = false;
            self->last_error_.clear();
            self->tickets_loaded_ = true;
            self->status_lbl_->setToolTip(QString());

            const QJsonArray tickets = support_array(r.data, {"tickets", "items", "results"});
            int open_c = 0, done_c = 0;
            for (const auto& v : tickets) {
                const QString st = v.toObject()["status"].toString().toLower();
                if (st == "open" || st == "in_progress")
                    ++open_c;
                else if (st == "resolved" || st == "closed")
                    ++done_c;
            }
            self->stat_total_->setText(QString::number(tickets.size()));
            self->stat_open_->setText(QString::number(open_c));
            self->stat_resolved_->setText(QString::number(done_c));

            self->all_tickets_ = tickets;

            // Never leave "pick a ticket" on the right when there is one to show:
            // the ticket just created, else (nothing open, not composing) the first
            // open / in-progress ticket, else the newest. One extra GET (its thread).
            bool opened = false;
            const int created_id = self->open_after_load_id_;
            self->open_after_load_id_ = -1;
            if (self->content_stack_ && self->content_stack_->currentIndex() != 1) {
                if (created_id > 0) {
                    for (const auto& v : tickets) {
                        if (v.toObject()["id"].toVariant().toInt() == created_id) {
                            self->open_ticket(v.toObject());
                            opened = true;
                            break;
                        }
                    }
                }
                if (!opened && self->content_stack_->currentIndex() == 0)
                    opened = self->open_default_ticket();
            }
            self->rebuild_ticket_rows(); // after open_ticket(): highlights its row

            // ↻ also refreshes the conversation that is open.
            if (!opened && self->content_stack_ && self->content_stack_->currentIndex() == 2 &&
                self->selected_ticket_id_ > 0)
                self->load_ticket_details(self->selected_ticket_id_);
            self->update_home_page();
        },
        this);
}

bool SupportScreen::open_default_ticket() {
    if (all_tickets_.isEmpty() || !content_stack_)
        return false;
    QJsonObject pick;
    for (const auto& v : all_tickets_) {
        const QJsonObject o = v.toObject();
        const QString st = o["status"].toString().toLower();
        if (st == QLatin1String("open") || st == QLatin1String("in_progress")) {
            pick = o;
            break;
        }
    }
    if (pick.isEmpty()) {
        QDateTime newest;
        for (const auto& v : all_tickets_) {
            const QJsonObject o = v.toObject();
            const QDateTime dt = support_parse_utc(o["created_at"].toString());
            if (pick.isEmpty() || (dt.isValid() && (!newest.isValid() || dt > newest))) {
                pick = o;
                newest = dt;
            }
        }
    }
    if (pick.isEmpty())
        return false;
    open_ticket(pick);
    return true;
}

void SupportScreen::on_list_state_action() {
    switch (list_action_) {
        case ListAction::Retry:
            load_tickets();
            break;
        case ListAction::ClearFilters:
            // Both setters re-render the rows through their change signals.
            if (search_input_)
                search_input_->clear();
            if (filter_combo_)
                filter_combo_->setCurrentIndex(0);
            break;
        case ListAction::None:
            break;
    }
}

// ── rebuild_ticket_rows ───────────────────────────────────────────────────────
// Renders all_tickets_ into the sidebar, applying the search box and the status
// filter combo.

void SupportScreen::rebuild_ticket_rows() {
    auto* lay = qobject_cast<QVBoxLayout*>(ticket_container_->layout());
    if (!lay)
        return;
    clear_layout_keep_stretch(lay);
    active_row_btn_ = nullptr;

    const QString needle = search_input_ ? search_input_->text().trimmed().toLower() : QString();
    // Combo order matches retranslateUi(): All / Open / In Progress / Resolved /
    // Closed. Index 0 means "no status filter".
    static const char* kSupportFilterStatus[] = {"", "open", "in_progress", "resolved", "closed"};
    const int fi = filter_combo_ ? filter_combo_->currentIndex() : 0;
    const int n_filters = static_cast<int>(sizeof(kSupportFilterStatus) / sizeof(kSupportFilterStatus[0]));
    const QString want_status = (fi > 0 && fi < n_filters) ? QString::fromLatin1(kSupportFilterStatus[fi]) : QString();

    QJsonArray shown;
    for (const auto& v : all_tickets_) {
        const auto obj = v.toObject();
        if (!want_status.isEmpty() && obj["status"].toString().toLower() != want_status)
            continue;
        if (!needle.isEmpty()) {
            const QString hay = (obj["subject"].toString() + ' ' + obj["category"].toString() + ' ' +
                                 obj["ticket_number"].toString() + ' ' + obj["id"].toVariant().toString())
                                    .toLower();
            if (!hay.contains(needle))
                continue;
        }
        shown.append(v);
    }

    // Nothing to list: the first list is on its way (skeleton), or a designed
    // state says why and offers the way on — never a blank strip.
    if (shown.isEmpty() && list_stack_) {
        if (!tickets_loaded_ && !load_failed_ && all_tickets_.isEmpty()) {
            if (!list_skeleton_) {
                list_skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, list_stack_);
                list_skeleton_->set_row_count(7);
                list_skeleton_->set_opaque(false);
                list_stack_->addWidget(list_skeleton_);
            }
            list_skeleton_->set_message(tr("Loading your tickets…"));
            list_stack_->setCurrentWidget(list_skeleton_);
            return;
        }
        if (!list_state_) {
            list_state_ = new ui::EmptyState(list_stack_);
            list_state_->set_compact(true);
            list_state_->set_alignment(Qt::AlignTop);
            connect(list_state_, &ui::EmptyState::primary_clicked, this, &SupportScreen::on_list_state_action);
            list_stack_->addWidget(list_state_);
        }
        ui::EmptyState::Spec spec;
        if (load_failed_ && all_tickets_.isEmpty()) {
            spec = ui::EmptyState::error_spec(tr("Check your connection, then retry."), tr("Could not load tickets"),
                                              tr("RETRY"), last_error_);
            list_action_ = ListAction::Retry;
        } else if (!needle.isEmpty() || !want_status.isEmpty()) {
            spec = ui::EmptyState::no_data_spec(tr("No matching tickets"),
                                                tr("Nothing matches this search or status filter."),
                                                tr("CLEAR FILTERS"));
            list_action_ = ListAction::ClearFilters;
        } else {
            // The "New Ticket" button sits right above: no second button here.
            spec = ui::EmptyState::no_data_spec(tr("No tickets yet"),
                                                tr("Use New Ticket above to contact the Fincept team."));
            list_action_ = ListAction::None;
        }
        list_state_->apply(spec);
        list_stack_->setCurrentWidget(list_state_);
        return;
    }
    if (list_stack_)
        list_stack_->setCurrentWidget(list_scroll_);

    for (const auto& v : shown) {
        const QJsonObject obj = v.toObject();
        const int id = obj["id"].toVariant().toInt();
        const QString status = obj["status"].toString();
        const QString priority = obj["priority"].toString();

        QString date_str;
        const QDateTime dt = support_parse_utc(obj["created_at"].toString());
        if (dt.isValid())
            date_str = dt.toString("d MMM yyyy");

        auto* row_inner = new QWidget(this);
        row_inner->setStyleSheet("background:transparent;");
        auto* rl = new QVBoxLayout(row_inner);
        rl->setContentsMargins(14, 10, 14, 10);
        rl->setSpacing(5);

        auto* top = new QHBoxLayout;
        auto* sdot = lbl("●", status_color(status), 9);
        sdot->setFixedWidth(qMax(14, ui::fonts::font_px(-5) + 5));
        top->addWidget(sdot);
        auto* subj = lbl(obj["subject"].toString(), ui::colors::TEXT_PRIMARY(), 12, true);
        subj->setTextFormat(Qt::PlainText); // user-authored
        // The row's whole width at any font size (a fixed 220 px held a dozen
        // characters at font 24); a longer subject is cut at the row's edge
        // instead of widening the sidebar, and is in full in the tooltip.
        subj->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        subj->setMinimumWidth(1);
        subj->setToolTip(obj["subject"].toString());
        top->addWidget(subj, 1);
        rl->addLayout(top);

        auto* meta = new QHBoxLayout;
        meta->setSpacing(8);
        meta->addWidget(lbl(obj["category"].toString().toLower().replace('_', ' '), ui::colors::TEXT_TERTIARY(), 10));
        meta->addWidget(lbl("·", ui::colors::BORDER_MED(), 10));
        meta->addWidget(lbl(priority.toLower(), priority_color(priority), 10));
        meta->addStretch();
        if (!date_str.isEmpty())
            meta->addWidget(lbl(date_str, ui::colors::TEXT_TERTIARY(), 9));
        rl->addLayout(meta);

        auto* btn = new QPushButton;
        // Two text lines plus margins: 66px at the default font size, more for a larger one.
        btn->setFixedHeight(qMax(66, QFontMetrics(ui::fonts::font(-2)).height() +
                                         QFontMetrics(ui::fonts::font(-4)).height() + 25));
        btn->setStyleSheet(QString("QPushButton { background:transparent; border:none;"
                                   " border-bottom:1px solid %1; padding:0; text-align:left; }"
                                   "QPushButton:hover { background:%2; }")
                               .arg(ui::colors::BORDER_DIM(), ui::colors::BG_RAISED()));
        btn->setToolTip(obj["ticket_number"].toString());
        auto* btn_lay = new QHBoxLayout(btn);
        btn_lay->setContentsMargins(0, 0, 0, 0);
        row_inner->setParent(btn);
        btn_lay->addWidget(row_inner);

        connect(btn, &QPushButton::clicked, this, [this, btn, obj]() {
            select_ticket_row(btn);
            open_ticket(obj);
        });

        lay->insertWidget(lay->count() - 1, btn);

        // A refresh (close/reopen, ↻, filter change) rebuilds every row; keep the
        // ticket that is open in the detail pane highlighted.
        if (content_stack_ && content_stack_->currentIndex() == 2 && id == selected_ticket_id_)
            select_ticket_row(btn);
    }
}

// ── Detail ────────────────────────────────────────────────────────────────────

void SupportScreen::apply_detail_status(const QString& status) {
    const QString st = status.toLower();
    // Only "closed" locks the conversation. A reply to a "resolved" ticket is
    // accepted by the API (it reopens the ticket), so the reply box stays.
    selected_is_closed_ = (st == QLatin1String("closed"));
    if (detail_status_lbl_) {
        detail_status_lbl_->setText(" " + status_label(st) + " ");
        detail_status_lbl_->setStyleSheet(QString("background:%1;color:%2;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;"
                                                  "padding:0 10px;letter-spacing:0.5px;%3")
                                              .arg(status_color(st), ui::colors::TEXT_ON_ACCENT(), MF()));
    }
    if (closed_box_)
        closed_box_->setVisible(selected_is_closed_);
    if (reply_box_)
        reply_box_->setVisible(!selected_is_closed_);
    if (close_btn_)
        close_btn_->setVisible(!selected_is_closed_);
    if (reopen_btn_)
        reopen_btn_->setVisible(selected_is_closed_);
}

void SupportScreen::open_ticket(const QJsonObject& ticket) {
    selected_ticket_id_ = ticket["id"].toVariant().toInt();
    const QString number = ticket["ticket_number"].toString();
    detail_id_lbl_->setText(number.isEmpty() ? tr("Ticket #%1").arg(selected_ticket_id_) : number);
    detail_subject_lbl_->setText(ticket["subject"].toString());
    const QDateTime opened = support_parse_utc(ticket["created_at"].toString());
    detail_meta_lbl_->setText(tr("%1  ·  %2  ·  Opened %3")
                                  .arg(ticket["category"].toString().toLower().replace('_', ' '),
                                       ticket["priority"].toString().toLower(),
                                       opened.isValid() ? opened.toString("d MMM yyyy  hh:mm") : QString()));
    // The list endpoint carries no description; it arrives with the details.
    // Until then the description line is hidden and the conversation area shows
    // a skeleton (the reply replaces it).
    detail_body_lbl_->clear();
    detail_body_lbl_->hide();
    apply_detail_status(ticket["status"].toString());

    auto* mcl = qobject_cast<QVBoxLayout*>(messages_container_->layout());
    clear_layout_keep_stretch(mcl);
    if (mcl) {
        auto* sk = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Text, messages_container_);
        sk->set_opaque(false);
        sk->set_message(tr("Loading the conversation…"));
        mcl->insertWidget(0, sk);
    }
    content_stack_->setCurrentIndex(2);
    load_ticket_details(selected_ticket_id_);
}

void SupportScreen::load_ticket_details(int ticket_id) {
    if (ticket_id <= 0)
        return;
    set_busy(true);
    QPointer<SupportScreen> self(this);
    auth::UserApi::instance().get_ticket_details(
        ticket_id,
        [self, ticket_id](auth::ApiResponse dr) {
            if (!self)
                return;
            self->set_busy(false);
            // The user may have clicked another ticket while this reply was in
            // flight; painting it now would show ticket A's conversation under
            // ticket B's header.
            if (self->selected_ticket_id_ != ticket_id || !self->messages_container_)
                return;
            auto* mcl = qobject_cast<QVBoxLayout*>(self->messages_container_->layout());
            if (!mcl)
                return;

            if (!dr.success) {
                clear_layout_keep_stretch(mcl);
                // A designed failure state (plain sentence, the raw text behind
                // DETAILS) with RETRY, in place of a red one-liner.
                auto* err = ui::EmptyState::create(
                    ui::EmptyState::error_spec(tr("The conversation could not be loaded. Check your connection, "
                                                  "then retry."),
                                               tr("Could not load this conversation"), tr("RETRY"), dr.error),
                    self->messages_container_);
                err->set_compact(true);
                err->set_alignment(Qt::AlignTop);
                QPointer<SupportScreen> guard(self);
                connect(err, &ui::EmptyState::primary_clicked, self, [guard, ticket_id]() {
                    if (guard)
                        guard->load_ticket_details(ticket_id);
                });
                mcl->insertWidget(0, err);
                if (self->detail_body_lbl_ && self->detail_body_lbl_->text().isEmpty()) {
                    self->detail_body_lbl_->setText(tr("Description unavailable."));
                    self->detail_body_lbl_->show();
                }
                return;
            }

            QJsonObject data = dr.data.value("data").isObject() ? dr.data.value("data").toObject() : dr.data;
            const QJsonObject ticket = data.value("ticket").toObject();
            if (!ticket.isEmpty()) {
                const QString desc = ticket["description"].toString();
                self->detail_body_lbl_->setText(desc.isEmpty() ? tr("No description provided.") : desc);
                // Fresh status — the list row may be minutes old.
                self->apply_detail_status(ticket["status"].toString());
            } else if (self->detail_body_lbl_->text().isEmpty()) {
                self->detail_body_lbl_->setText(tr("No description provided."));
            }
            self->detail_body_lbl_->show();

            const QJsonArray msgs = support_array(dr.data, {"messages", "replies", "comments", "items"});
            clear_layout_keep_stretch(mcl);
            for (const auto& mv : msgs) {
                const auto mo = mv.toObject();
                QString sender_type = mo["sender_type"].toString();
                if (sender_type.isEmpty())
                    sender_type = mo["role"].toString();
                const bool is_user = (sender_type == "user" || sender_type == "customer");
                const QDateTime mdt = support_parse_utc(mo["created_at"].toString());
                mcl->insertWidget(mcl->count() - 1,
                                  support_message_bubble(self, is_user ? tr("You") : tr("Support Team"),
                                                         // Staff replies in the palette's purple slot.
                                                         is_user ? QString(ui::colors::CYAN())
                                                                 : ui::colors::chart_color(2).name(),
                                                         mdt.isValid() ? mdt.toString("d MMM yyyy  hh:mm") : QString(),
                                                         mo["message"].toString()));
            }
            if (msgs.isEmpty()) {
                ui::EmptyState::Spec none = ui::EmptyState::no_data_spec(
                    tr("No replies yet"), tr("The support team answers here; you will also get an e-mail."));
                none.glyph = QStringLiteral("✉");
                auto* es = ui::EmptyState::create(none, self->messages_container_);
                es->set_compact(true);
                es->set_alignment(Qt::AlignTop);
                mcl->insertWidget(0, es);
            }

            QPointer<SupportScreen> scroll_guard(self);
            QTimer::singleShot(50, self.data(), [scroll_guard]() {
                if (scroll_guard && scroll_guard->detail_scroll_)
                    scroll_guard->detail_scroll_->verticalScrollBar()->setValue(
                        scroll_guard->detail_scroll_->verticalScrollBar()->maximum());
            });
        },
        this);
}

// ── on_create_ticket ──────────────────────────────────────────────────────────

void SupportScreen::on_create_ticket() {
    QString subject = subject_input_->text().trimmed();
    QString desc = desc_input_->toPlainText().trimmed();
    // Silently doing nothing when a required field is blank looks like a broken
    // button — tell the user which field is missing and focus it.
    if (subject.isEmpty()) {
        QMessageBox::information(this, tr("Subject required"), tr("Please enter a short subject for your ticket."));
        subject_input_->setFocus();
        return;
    }
    if (desc.isEmpty()) {
        QMessageBox::information(this, tr("Description required"), tr("Please describe the issue you are reporting."));
        desc_input_->setFocus();
        return;
    }
    if (desc.length() > 2000) {
        QMessageBox::information(this, tr("Description too long"),
                                 tr("Descriptions are limited to 2000 characters (currently %1).").arg(desc.length()));
        desc_input_->setFocus();
        return;
    }

    set_busy(true);
    create_btn_->setText(tr("Submitting…"));

    // currentData() carries the stable API key (e.g. "bug_report", "medium")
    // regardless of which language the user sees in the combo.
    const QString cat_key = category_combo_->currentData().toString();
    const QString pri_key = priority_combo_->currentData().toString();

    QPointer<SupportScreen> self(this);
    auth::UserApi::instance().create_ticket(
        subject, desc, cat_key, pri_key,
        [self](auth::ApiResponse r) {
            if (!self)
                return;
            self->set_busy(false);
            if (self->create_btn_)
                self->create_btn_->setText(tr("Submit Ticket →"));
            if (r.success) {
                const QJsonObject d = r.data.value("data").isObject() ? r.data.value("data").toObject() : r.data;
                if (self->subject_input_)
                    self->subject_input_->clear();
                if (self->desc_input_)
                    self->desc_input_->clear();
                // Open the new ticket as soon as the refreshed list has it (page 0
                // shows a skeleton until then); without an id, the default ticket.
                int new_id = d["id"].toVariant().toInt();
                if (new_id <= 0)
                    new_id = d["ticket_id"].toVariant().toInt();
                self->open_after_load_id_ = new_id > 0 ? new_id : -1;
                self->selected_ticket_id_ = -1;
                self->content_stack_->setCurrentIndex(0);
                const QString number = d["ticket_number"].toString();
                self->load_tickets();
                self->update_home_page();
                // set_busy() resets the status line; show the receipt after it.
                QPointer<SupportScreen> guard(self);
                QTimer::singleShot(0, self.data(), [guard, number]() {
                    if (guard && guard->status_lbl_ && !number.isEmpty())
                        guard->status_lbl_->setText(tr("Created %1").arg(number));
                });
            } else {
                QMessageBox::warning(self, tr("Could not create ticket"),
                                     r.error.isEmpty() ? tr("The ticket could not be submitted. Please check your "
                                                            "connection and try again — your text has been kept.")
                                                       : tr("The ticket could not be submitted:\n%1").arg(r.error));
            }
        },
        this);
}

// ── on_send_message ───────────────────────────────────────────────────────────

void SupportScreen::on_send_message() {
    if (selected_ticket_id_ <= 0 || selected_is_closed_)
        return;
    // Ctrl+Enter bypasses the button's disabled state, so a held/repeated key
    // would send the same reply several times while the first is in flight.
    if (send_btn_ && !send_btn_->isEnabled())
        return;
    const QString msg = msg_input_->toPlainText().trimmed();
    if (msg.isEmpty())
        return;

    set_busy(true);
    send_btn_->setText(tr("Sending…"));

    QPointer<SupportScreen> self(this);
    const int ticket_id = selected_ticket_id_;
    auth::UserApi::instance().add_ticket_message(
        ticket_id, msg,
        [self, msg, ticket_id](auth::ApiResponse r) {
            if (!self)
                return;
            self->set_busy(false);
            if (self->send_btn_)
                self->send_btn_->setText(tr("Send Reply →"));
            if (!r.success) {
                QMessageBox::warning(self, tr("Reply not sent"),
                                     r.error.isEmpty() ? tr("Your reply could not be sent. It has been kept in the "
                                                            "box so you can try again.")
                                                       : tr("Your reply could not be sent:\n%1").arg(r.error));
                return;
            }
            if (self->selected_ticket_id_ != ticket_id)
                return; // user moved on; the next open re-fetches the thread
            if (self->msg_input_)
                self->msg_input_->clear();
            // A reply to a resolved ticket reopens it server-side.
            const QJsonObject d = r.data.value("data").isObject() ? r.data.value("data").toObject() : r.data;
            if (d.contains("ticket_status"))
                self->apply_detail_status(d["ticket_status"].toString());

            auto* mcl = qobject_cast<QVBoxLayout*>(self->messages_container_->layout());
            if (!mcl)
                return;
            // The thread is no longer empty: drop the "No replies yet" state.
            for (auto* es : self->messages_container_->findChildren<ui::EmptyState*>(QString(),
                                                                                     Qt::FindDirectChildrenOnly)) {
                mcl->removeWidget(es);
                es->hide();
                es->deleteLater();
            }
            mcl->insertWidget(mcl->count() - 1,
                              support_message_bubble(self, tr("You"), ui::colors::CYAN(),
                                                     QDateTime::currentDateTime().toString("d MMM yyyy  hh:mm"), msg));
            QPointer<SupportScreen> guard(self);
            QTimer::singleShot(50, self.data(), [guard]() {
                if (guard && guard->detail_scroll_)
                    guard->detail_scroll_->verticalScrollBar()->setValue(
                        guard->detail_scroll_->verticalScrollBar()->maximum());
            });
        },
        this);
}

// ── on_close_ticket / on_reopen_ticket ────────────────────────────────────────

void SupportScreen::on_close_ticket() {
    if (selected_ticket_id_ <= 0)
        return;
    // Closing a ticket is destructive from the user's point of view (the reply
    // box disappears) — confirm first.
    if (QMessageBox::question(this, tr("Close ticket"), tr("Close this ticket? You can reopen it later."),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    set_busy(true);
    QPointer<SupportScreen> self(this);
    const int ticket_id = selected_ticket_id_;
    auth::UserApi::instance().update_ticket_status(
        ticket_id, "closed",
        [self, ticket_id](auth::ApiResponse r) {
            if (!self)
                return;
            self->set_busy(false);
            if (!r.success) {
                QMessageBox::warning(self, tr("Could not close ticket"),
                                     r.error.isEmpty() ? tr("The ticket status could not be updated.")
                                                       : tr("The ticket status could not be updated:\n%1").arg(r.error));
                return;
            }
            if (self->selected_ticket_id_ == ticket_id)
                self->apply_detail_status(QStringLiteral("closed"));
            self->load_tickets();
        },
        this);
}

void SupportScreen::on_reopen_ticket() {
    if (selected_ticket_id_ <= 0)
        return;
    set_busy(true);
    QPointer<SupportScreen> self(this);
    const int ticket_id = selected_ticket_id_;
    auth::UserApi::instance().update_ticket_status(
        ticket_id, "open",
        [self, ticket_id](auth::ApiResponse r) {
            if (!self)
                return;
            self->set_busy(false);
            if (!r.success) {
                QMessageBox::warning(self, tr("Could not reopen ticket"),
                                     r.error.isEmpty() ? tr("The ticket status could not be updated.")
                                                       : tr("The ticket status could not be updated:\n%1").arg(r.error));
                return;
            }
            if (self->selected_ticket_id_ == ticket_id)
                self->apply_detail_status(QStringLiteral("open"));
            self->load_tickets();
        },
        this);
}

} // namespace fincept::screens
