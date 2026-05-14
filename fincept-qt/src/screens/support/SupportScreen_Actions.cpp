// src/screens/support/SupportScreen_Actions.cpp
//
// Ticket data flow: load_tickets, on_create_ticket, on_send_message,
// on_close_ticket, on_reopen_ticket.
//
// Part of the partial-class split of SupportScreen.cpp.

#include "screens/support/SupportScreen.h"
#include "screens/support/SupportScreen_internal.h"

#include "auth/UserApi.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::screens::support_internal;

void SupportScreen::load_tickets() {
    set_busy(true);

    auth::UserApi::instance().get_tickets([this](auth::ApiResponse r) {
        set_busy(false);

        auto* lay = qobject_cast<QVBoxLayout*>(ticket_container_->layout());
        if (!lay)
            return;
        while (lay->count() > 1)
            delete lay->takeAt(0)->widget();
        active_row_btn_ = nullptr;

        // Robust response unwrap:
        // API may return: {"tickets":[...]}  or  {"data":{"tickets":[...]}}
        // or a bare array at top level, or {"success":true,"data":[...]}
        QJsonArray tickets;
        if (r.success) {
            auto payload = r.data;
            // Unwrap {"data": ...}
            if (payload.contains("data") && payload["data"].isObject())
                payload = payload["data"].toObject();
            // Try known array keys
            if (payload.contains("tickets") && payload["tickets"].isArray())
                tickets = payload["tickets"].toArray();
            else if (payload.contains("items") && payload["items"].isArray())
                tickets = payload["items"].toArray();
            else if (payload.contains("results") && payload["results"].isArray())
                tickets = payload["results"].toArray();
            // If the whole response data is itself an array key at root
            if (tickets.isEmpty()) {
                for (auto it = payload.begin(); it != payload.end(); ++it) {
                    if (it.value().isArray()) {
                        tickets = it.value().toArray();
                        break;
                    }
                }
            }
        }

        // Demo ticket
        QJsonObject demo;
        demo["id"] = "DEMO-001";
        demo["subject"] = "Welcome to Fincept Support";
        demo["status"] = "resolved";
        demo["priority"] = "low";
        demo["category"] = "general";
        demo["created_at"] = "2026-01-01T00:00:00Z";

        QJsonArray all;
        all.append(demo);
        for (const auto& v : tickets)
            all.append(v);

        // Stats
        int open_c = 0, done_c = 0;
        for (const auto& v : all) {
            QString st = v.toObject()["status"].toString().toLower();
            if (st == "open" || st == "in_progress" || st == "pending")
                ++open_c;
            else if (st == "resolved" || st == "closed")
                ++done_c;
        }
        stat_total_->setText(QString::number(all.size()));
        stat_open_->setText(QString::number(open_c));
        stat_resolved_->setText(QString::number(done_c));

        if (all.isEmpty()) {
            auto* empty = lbl("No tickets yet", ui::colors::TEXT_TERTIARY(), 12);
            empty->setAlignment(Qt::AlignCenter);
            empty->setContentsMargins(0, 40, 0, 0);
            lay->insertWidget(0, empty);
            return;
        }

        // Build sidebar rows
        for (const auto& v : all) {
            auto obj = v.toObject();
            QString id = obj["id"].toVariant().toString();
            QString subject = obj["subject"].toString();
            QString status = obj["status"].toString();
            QString priority = obj["priority"].toString();
            QString category = obj["category"].toString();
            QString created = obj["created_at"].toString();
            bool is_demo = (id == "DEMO-001");

            QString date_str;
            if (!created.isEmpty()) {
                QDateTime dt = QDateTime::fromString(created, Qt::ISODate);
                if (dt.isValid())
                    date_str = dt.toLocalTime().toString("d MMM yyyy");
            }

            // Row widget (inner content)
            auto* row_inner = new QWidget(this);
            row_inner->setStyleSheet("background:transparent;");
            auto* rl = new QVBoxLayout(row_inner);
            rl->setContentsMargins(14, 10, 14, 10);
            rl->setSpacing(5);

            // Top: subject + status dot
            auto* top = new QHBoxLayout;
            auto* sdot = lbl("●", status_color(status), 9);
            sdot->setFixedWidth(14);
            top->addWidget(sdot);
            auto* subj = lbl(subject, ui::colors::TEXT_PRIMARY(), 12, true);
            subj->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            // Elide long subjects
            subj->setMaximumWidth(220);
            top->addWidget(subj, 1);

            if (is_demo) {
                auto* demo_tag = status_badge("DEMO", ui::colors::INFO(), ui::colors::BG_BASE());
                top->addWidget(demo_tag);
            }
            rl->addLayout(top);

            // Bottom: meta row
            auto* meta = new QHBoxLayout;
            meta->setSpacing(8);
            meta->addWidget(lbl(category.toLower(), ui::colors::TEXT_TERTIARY(), 10));
            meta->addWidget(lbl("·", ui::colors::BORDER_MED(), 10));
            auto* pr = lbl(priority.toLower(), priority_color(priority), 10);
            meta->addWidget(pr);
            meta->addStretch();
            if (!date_str.isEmpty())
                meta->addWidget(lbl(date_str, ui::colors::TEXT_TERTIARY(), 9));
            rl->addLayout(meta);

            // Wrap in QPushButton for click
            auto* btn = new QPushButton;
            btn->setFixedHeight(66);
            btn->setStyleSheet(QString("QPushButton { background:transparent; border:none;"
                                       " border-bottom:1px solid %1; padding:0; text-align:left; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::BORDER_DIM(), ui::colors::BG_RAISED()));
            auto* btn_lay = new QHBoxLayout(btn);
            btn_lay->setContentsMargins(0, 0, 0, 0);
            row_inner->setParent(btn);
            btn_lay->addWidget(row_inner);

            const int id_int = is_demo ? -1 : id.toInt();
            const QString sub_c = subject;
            const QString st_c = status;
            const QString pr_c = priority;
            const QString cat_c = category;
            const QString cr_c = date_str;
            const QString body_c = obj["description"].toString();
            const bool demo_c = is_demo;

            connect(btn, &QPushButton::clicked, this, [=, this]() {
                select_ticket_row(btn);

                selected_ticket_id_ = id_int;
                selected_is_demo_ = demo_c;
                selected_is_closed_ = (st_c.toLower() == "closed" || st_c.toLower() == "resolved");

                // Populate header
                detail_id_lbl_->setText(QString("Ticket #%1").arg(demo_c ? "DEMO" : QString::number(id_int)));
                detail_subject_lbl_->setText(sub_c);

                detail_status_lbl_->setText(" " + status_label(st_c) + " ");
                detail_status_lbl_->setStyleSheet(QString("background:%1;color:%2;font-size:10px;font-weight:700;"
                                                          "padding:0 10px;letter-spacing:0.5px;%3")
                                                      .arg(status_color(st_c), ui::colors::BG_BASE(), MF));

                detail_meta_lbl_->setText(
                    QString("%1  ·  %2  ·  Opened %3").arg(cat_c.toLower(), pr_c.toLower(), cr_c));

                detail_body_lbl_->setText(body_c.isEmpty() ? "No description provided." : body_c);

                // Show/hide bottom widgets
                demo_box_->setVisible(demo_c);
                closed_box_->setVisible(!demo_c && selected_is_closed_);
                reply_box_->setVisible(!demo_c && !selected_is_closed_);
                close_btn_->setVisible(!demo_c && !selected_is_closed_);
                reopen_btn_->setVisible(!demo_c && selected_is_closed_);

                // Clear messages
                auto* mcl2 = qobject_cast<QVBoxLayout*>(messages_container_->layout());
                while (mcl2->count() > 1)
                    delete mcl2->takeAt(0)->widget();

                if (demo_c) {
                    // Canned demo message
                    auto* m = new QWidget(this);
                    m->setStyleSheet(QString("background:%1;border:1px solid %2;border-left:3px solid #9333ea;")
                                         .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
                    auto* ml = new QVBoxLayout(m);
                    ml->setContentsMargins(14, 10, 14, 12);
                    ml->setSpacing(6);
                    auto* mh = new QHBoxLayout;
                    mh->addWidget(lbl("Support Team", "#9333ea", 11, true));
                    mh->addStretch();
                    mh->addWidget(lbl("1 Jan 2026", ui::colors::TEXT_TERTIARY(), 10));
                    ml->addLayout(mh);
                    auto* mb = lbl("Welcome to Fincept! This demo ticket shows how the support system works.\n"
                                   "Create a real ticket and our team will respond within 24 hours.",
                                   ui::colors::TEXT_PRIMARY(), 12, false, true);
                    ml->addWidget(mb);
                    mcl2->insertWidget(mcl2->count() - 1, m);
                } else {
                    set_busy(true);
                    auth::UserApi::instance().get_ticket_details(id_int, [this](auth::ApiResponse dr) {
                        set_busy(false);
                        if (!dr.success)
                            return;

                        // Robust message unwrap — try all known shapes
                        auto dr_data = dr.data;
                        if (dr_data.contains("data") && dr_data["data"].isObject())
                            dr_data = dr_data["data"].toObject();
                        QJsonArray msgs;
                        for (const auto& key : {"messages", "replies", "comments", "items"}) {
                            if (dr_data.contains(key) && dr_data[key].isArray()) {
                                msgs = dr_data[key].toArray();
                                break;
                            }
                        }

                        auto* mcl3 = qobject_cast<QVBoxLayout*>(messages_container_->layout());
                        while (mcl3->count() > 1)
                            delete mcl3->takeAt(0)->widget();

                        for (const auto& mv : msgs) {
                            auto mo = mv.toObject();
                            // Handle various field name conventions the API may use
                            QString sender_type = mo["sender_type"].toString();
                            if (sender_type.isEmpty())
                                sender_type = mo["role"].toString();
                            if (sender_type.isEmpty())
                                sender_type = mo["type"].toString();
                            if (sender_type.isEmpty())
                                sender_type = mo["sender"].toString();
                            bool is_user = (sender_type == "user" || sender_type == "customer");
                            QString sender = is_user ? "You" : "Support Team";
                            QString bubble_color = is_user ? ui::colors::CYAN() : "#9333ea";

                            QString ts;
                            QDateTime mdt = QDateTime::fromString(mo["created_at"].toString(), Qt::ISODate);
                            if (mdt.isValid())
                                ts = mdt.toLocalTime().toString("d MMM yyyy  hh:mm");

                            auto* m = new QWidget(this);
                            m->setStyleSheet(
                                QString("background:%1;border:1px solid %2;"
                                        "border-left:3px solid %3;")
                                    .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), bubble_color));
                            auto* ml = new QVBoxLayout(m);
                            ml->setContentsMargins(14, 10, 14, 12);
                            ml->setSpacing(6);
                            auto* mh = new QHBoxLayout;
                            mh->addWidget(lbl(sender, bubble_color, 11, true));
                            mh->addStretch();
                            mh->addWidget(lbl(ts, ui::colors::TEXT_TERTIARY(), 10));
                            ml->addLayout(mh);
                            auto* mb = lbl(mo["message"].toString(), ui::colors::TEXT_PRIMARY(), 12, false, true);
                            ml->addWidget(mb);
                            mcl3->insertWidget(mcl3->count() - 1, m);
                        }

                        if (msgs.isEmpty()) {
                            mcl3->insertWidget(
                                0, lbl("No messages yet — be the first to reply.", ui::colors::TEXT_TERTIARY(), 11));
                        }

                        // Scroll to bottom
                        QTimer::singleShot(50, this, [this]() {
                            detail_scroll_->verticalScrollBar()->setValue(
                                detail_scroll_->verticalScrollBar()->maximum());
                        });
                    });
                }

                content_stack_->setCurrentIndex(2);
            });

            lay->insertWidget(lay->count() - 1, btn);
        }
    });
}

// ── on_create_ticket ──────────────────────────────────────────────────────────

void SupportScreen::on_create_ticket() {
    QString subject = subject_input_->text().trimmed();
    QString desc = desc_input_->toPlainText().trimmed();
    if (subject.isEmpty() || desc.isEmpty())
        return;

    set_busy(true);
    create_btn_->setText("Submitting…");

    auth::UserApi::instance().create_ticket(subject, desc, category_combo_->currentText().toLower().replace(' ', '_'),
                                            priority_combo_->currentText().toLower(), [this](auth::ApiResponse r) {
                                                set_busy(false);
                                                create_btn_->setText("Submit Ticket →");
                                                if (r.success) {
                                                    subject_input_->clear();
                                                    desc_input_->clear();
                                                    content_stack_->setCurrentIndex(0);
                                                    load_tickets();
                                                }
                                            });
}

// ── on_send_message ───────────────────────────────────────────────────────────

void SupportScreen::on_send_message() {
    if (selected_ticket_id_ < 0 || selected_is_demo_)
        return;
    QString msg = msg_input_->toPlainText().trimmed();
    if (msg.isEmpty())
        return;

    set_busy(true);
    send_btn_->setText("Sending…");

    auth::UserApi::instance().add_ticket_message(selected_ticket_id_, msg, [this, msg](auth::ApiResponse r) {
        set_busy(false);
        send_btn_->setText("Send Reply →");
        if (!r.success)
            return;

        msg_input_->clear();
        auto* mcl = qobject_cast<QVBoxLayout*>(messages_container_->layout());
        if (!mcl)
            return;

        auto* m = new QWidget(this);
        m->setStyleSheet(QString("background:%1;border:1px solid %2;border-left:3px solid %3;")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::CYAN()));
        auto* ml = new QVBoxLayout(m);
        ml->setContentsMargins(14, 10, 14, 12);
        ml->setSpacing(6);
        auto* mh = new QHBoxLayout;
        mh->addWidget(lbl("You", ui::colors::CYAN(), 11, true));
        mh->addStretch();
        mh->addWidget(lbl(QDateTime::currentDateTime().toString("d MMM yyyy  hh:mm"), ui::colors::TEXT_TERTIARY(), 10));
        ml->addLayout(mh);
        auto* mb = lbl(msg, ui::colors::TEXT_PRIMARY(), 12, false, true);
        ml->addWidget(mb);
        mcl->insertWidget(mcl->count() - 1, m);

        // Scroll to bottom
        QTimer::singleShot(50, this, [this]() {
            detail_scroll_->verticalScrollBar()->setValue(detail_scroll_->verticalScrollBar()->maximum());
        });
    });
}

// ── on_close_ticket / on_reopen_ticket ────────────────────────────────────────

void SupportScreen::on_close_ticket() {
    if (selected_ticket_id_ < 0)
        return;
    set_busy(true);
    auth::UserApi::instance().update_ticket_status(selected_ticket_id_, "closed", [this](auth::ApiResponse r) {
        set_busy(false);
        if (!r.success)
            return;
        selected_is_closed_ = true;
        close_btn_->hide();
        reopen_btn_->show();
        reply_box_->hide();
        closed_box_->show();
        load_tickets();
    });
}

void SupportScreen::on_reopen_ticket() {
    if (selected_ticket_id_ < 0)
        return;
    set_busy(true);
    auth::UserApi::instance().update_ticket_status(selected_ticket_id_, "open", [this](auth::ApiResponse r) {
        set_busy(false);
        if (!r.success)
            return;
        selected_is_closed_ = false;
        reopen_btn_->hide();
        close_btn_->show();
        reply_box_->show();
        closed_box_->hide();
        load_tickets();
    });
}

} // namespace fincept::screens
