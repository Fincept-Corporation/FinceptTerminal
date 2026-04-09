// src/screens/forum/ForumPostListPanel.cpp
#include "screens/forum/ForumPostListPanel.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

#include <cmath>

namespace fincept::screens {

static QString M(int sz = 12) {
    return QString("font-family:'Consolas','Courier New',monospace;font-size:%1px;").arg(sz);
}

static QString rel_time(const QString& iso) {
    if (iso.isEmpty())
        return {};
    auto dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso.left(19), "yyyy-MM-ddTHH:mm:ss");
    if (!dt.isValid())
        return iso.left(10);
    qint64 sec = dt.secsTo(QDateTime::currentDateTimeUtc());
    if (sec < 0)
        sec = 0;
    if (sec < 60)
        return QString("%1s ago").arg(sec);
    if (sec < 3600)
        return QString("%1m ago").arg(sec / 60);
    if (sec < 86400)
        return QString("%1h ago").arg(sec / 3600);
    return QString("%1d ago").arg(sec / 86400);
}

static QString fmt_num(int n) {
    if (n >= 10000)
        return QString("%1K").arg(n / 1000);
    if (n >= 1000)
        return QString("%1.%2K").arg(n / 1000).arg((n % 1000) / 100);
    return QString::number(n);
}

static QString det_color(const QString& s) {
    static const char* pal[] = {"#d97706", "#06b6d4", "#10b981", "#8b5cf6", "#f97316",
                                "#3b82f6", "#ec4899", "#14b8a6", "#84cc16", "#ef4444"};
    return pal[qHash(s) % 10];
}

ForumPostListPanel::ForumPostListPanel(QWidget* parent) : QWidget(parent) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) { setStyleSheet(QString("background:%1;color:%2;").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY())); });
    build_ui();
}

void ForumPostListPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Channel header ────────────────────────────────────────────────────────
    auto* hdr = new QWidget;
    hdr->setFixedHeight(44);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(16, 0, 16, 0);
    hdr_hl->setSpacing(10);

    auto* hash = new QLabel("#");
    hash->setStyleSheet(
        QString("color:%1;font-size:20px;font-weight:700;background:transparent;%2")
            .arg(ui::colors::AMBER(), M(20)));
    hash->setFixedWidth(18);

    channel_label_ = new QLabel("all-posts");
    channel_label_->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:700;background:transparent;%2")
            .arg(ui::colors::TEXT_PRIMARY(), M(14)));

    count_label_ = new QLabel;
    count_label_->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;%2")
            .arg(ui::colors::TEXT_TERTIARY(), M(11)));

    hdr_hl->addWidget(hash);
    hdr_hl->addWidget(channel_label_, 1);
    hdr_hl->addWidget(count_label_);
    root->addWidget(hdr);

    // ── Scrollable card feed ──────────────────────────────────────────────────
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setStyleSheet(
        QString("QScrollArea{border:none;background:%1;}"
                "QScrollBar:vertical{width:6px;background:transparent;margin:0;}"
                "QScrollBar::handle:vertical{background:%2;min-height:40px;"
                "border-radius:3px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical"
                "{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    feed_container_ = new QWidget;
    feed_container_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    feed_layout_ = new QVBoxLayout(feed_container_);
    feed_layout_->setContentsMargins(0, 0, 0, 0);
    feed_layout_->setSpacing(0);
    feed_layout_->addStretch();

    scroll_->setWidget(feed_container_);
    root->addWidget(scroll_, 1);

    skeleton_timer_ = new QTimer(this);
    skeleton_timer_->setInterval(400);
    connect(skeleton_timer_, &QTimer::timeout, this,
            &ForumPostListPanel::pulse_skeleton);
}

void ForumPostListPanel::set_header(const QString& title, const QString& color) {
    Q_UNUSED(color)
    channel_label_->setText(title.toLower().replace(' ', '-'));
}

void ForumPostListPanel::set_loading(bool on) {
    if (on) {
        show_skeleton();
    } else {
        skeleton_timer_->stop();
        if (skeleton_widget_) {
            skeleton_widget_->deleteLater();
            skeleton_widget_ = nullptr;
        }
    }
}

void ForumPostListPanel::show_skeleton() {
    if (skeleton_widget_) {
        skeleton_widget_->deleteLater();
        skeleton_widget_ = nullptr;
    }

    skeleton_widget_ = new QWidget(feed_container_);
    auto* sk_vl = new QVBoxLayout(skeleton_widget_);
    sk_vl->setContentsMargins(12, 8, 12, 8);
    sk_vl->setSpacing(6);

    for (int i = 0; i < 8; ++i) {
        auto* card = new QWidget;
        card->setFixedHeight(68);
        card->setStyleSheet(
            QString("background:%1;border:1px solid %2;border-radius:4px;")
                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(14, 10, 14, 10);
        cvl->setSpacing(8);

        auto* bar1 = new QWidget;
        bar1->setFixedHeight(10);
        bar1->setStyleSheet(
            QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                    "stop:0 %1,stop:%2 %3,stop:1 %1);border-radius:5px;")
                .arg(ui::colors::BORDER_DIM())
                .arg(0.3 + (i % 4) * 0.15)
                .arg(ui::colors::BORDER_MED()));
        bar1->setMaximumWidth(200 + i * 20);

        auto* bar2 = new QWidget;
        bar2->setFixedHeight(7);
        bar2->setStyleSheet(
            QString("background:%1;border-radius:3px;")
                .arg(ui::colors::BORDER_DIM()));
        bar2->setMaximumWidth(120);

        cvl->addWidget(bar1);
        cvl->addWidget(bar2);
        cvl->addStretch();
        sk_vl->addWidget(card);
    }

    sk_vl->addStretch();
    feed_layout_->insertWidget(0, skeleton_widget_);
    skeleton_timer_->start();
}

void ForumPostListPanel::pulse_skeleton() {
    if (!skeleton_widget_)
        return;
    static int off = 0;
    off = (off + 15) % 100;
    double s = off / 100.0;
    double e = std::min(1.0, s + 0.3);
    QString bg = QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                         "stop:0 %1,stop:%2 %3,stop:%4 %1);border-radius:5px;")
                     .arg(ui::colors::BORDER_DIM())
                     .arg(s, 0, 'f', 2)
                     .arg(ui::colors::BORDER_MED())
                     .arg(e, 0, 'f', 2);

    auto* lay = qobject_cast<QVBoxLayout*>(skeleton_widget_->layout());
    if (!lay)
        return;
    for (int i = 0; i < lay->count(); ++i) {
        auto* item = lay->itemAt(i);
        if (!item || !item->widget())
            continue;
        auto* card = item->widget();
        auto* cvl = qobject_cast<QVBoxLayout*>(card->layout());
        if (!cvl)
            continue;
        for (int j = 0; j < cvl->count(); ++j) {
            auto* ci = cvl->itemAt(j);
            if (ci && ci->widget() && ci->widget()->height() >= 7 &&
                ci->widget()->height() <= 12)
                ci->widget()->setStyleSheet(bg);
        }
    }
}

void ForumPostListPanel::clear() {
    feed_container_->setUpdatesEnabled(false);
    while (feed_layout_->count() > 0) {
        auto* item = feed_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    feed_container_->setUpdatesEnabled(true);
    active_uuid_.clear();
}

void ForumPostListPanel::set_active_post(const QString& uuid) {
    active_uuid_ = uuid;
}

void ForumPostListPanel::set_posts(const services::ForumPostsPage& page,
                                    const QString& category_color) {
    current_page_ = page;
    category_color_ = category_color;
    skeleton_timer_->stop();
    if (skeleton_widget_) {
        skeleton_widget_->deleteLater();
        skeleton_widget_ = nullptr;
    }
    rebuild_feed();
}

void ForumPostListPanel::rebuild_feed() {
    clear();
    count_label_->setText(
        current_page_.total > 0 ? QString("%1 posts").arg(current_page_.total) : "");

    if (current_page_.posts.isEmpty()) {
        auto* empty = new QWidget;
        empty->setMinimumHeight(120);
        empty->setStyleSheet("background:transparent;");
        auto* el = new QVBoxLayout(empty);
        el->setAlignment(Qt::AlignCenter);
        el->setSpacing(6);

        auto* icon = new QLabel("◇");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(
            QString("color:%1;font-size:24px;background:transparent;")
                .arg(ui::colors::BORDER_DIM()));

        auto* lbl = new QLabel("NO POSTS YET");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;letter-spacing:1.5px;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_TERTIARY(), M(13)));

        auto* sub = new QLabel("Be the first to start a discussion");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(
            QString("color:%1;font-size:11px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM(), M(11)));

        el->addWidget(icon);
        el->addWidget(lbl);
        el->addWidget(sub);
        feed_layout_->addWidget(empty);
        feed_layout_->addStretch();
        return;
    }

    feed_container_->setUpdatesEnabled(false);

    for (int i = 0; i < current_page_.posts.size(); ++i) {
        const auto& post = current_page_.posts[i];
        const bool active = (post.post_uuid == active_uuid_);
        QString cat_color = post.category_color.isEmpty()
                                ? det_color(post.category_name)
                                : post.category_color;

        auto* card = new QWidget;
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(
            active
                ? QString("background:rgba(217,119,6,0.04);"
                          "border-bottom:1px solid %1;"
                          "border-left:3px solid %2;")
                      .arg(ui::colors::BORDER_DIM(), cat_color)
                : QString("background:%1;"
                          "border-bottom:1px solid %2;"
                          "border-left:3px solid transparent;")
                      .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

        auto* card_vl = new QVBoxLayout(card);
        card_vl->setContentsMargins(14, 10, 14, 8);
        card_vl->setSpacing(4);

        // ── Top row: avatar + name + category chip + time ─────────────────────
        auto* top_row = new QWidget;
        top_row->setStyleSheet("background:transparent;");
        auto* top_hl = new QHBoxLayout(top_row);
        top_hl->setContentsMargins(0, 0, 0, 0);
        top_hl->setSpacing(8);

        auto* avatar = new QLabel(
            post.author_display_name.isEmpty()
                ? "?"
                : post.author_display_name.left(2).toUpper());
        avatar->setFixedSize(24, 24);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:%2;"
                    "border-radius:12px;%3")
                .arg(ui::colors::BG_BASE(), det_color(post.author_display_name),
                     M(9)));

        auto* author = new QLabel(post.author_display_name.left(16));
        author->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:600;"
                    "background:transparent;%2")
                .arg(ui::colors::TEXT_PRIMARY(), M(11)));

        auto* cat_chip = new QLabel(post.category_name.toUpper().left(12));
        cat_chip->setStyleSheet(
            QString("color:%1;font-size:9px;font-weight:700;background:transparent;"
                    "border:1px solid %1;padding:1px 6px;letter-spacing:0.5px;"
                    "border-radius:8px;%2")
                .arg(cat_color, M(9)));

        auto* time_lbl = new QLabel(rel_time(post.created_at));
        time_lbl->setStyleSheet(
            QString("color:%1;font-size:10px;background:transparent;%2")
                .arg(ui::colors::TEXT_DIM(), M(10)));

        top_hl->addWidget(avatar);
        top_hl->addWidget(author);
        top_hl->addSpacing(4);
        top_hl->addWidget(cat_chip);
        top_hl->addStretch();
        top_hl->addWidget(time_lbl);
        card_vl->addWidget(top_row);

        // ── Title ─────────────────────────────────────────────────────────────
        auto* title_lbl = new QLabel(post.title);
        title_lbl->setWordWrap(false);
        title_lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        title_lbl->setStyleSheet(
            active
                ? QString("color:%1;font-size:13px;font-weight:700;"
                          "background:transparent;%2")
                      .arg(ui::colors::TEXT_PRIMARY(), M(13))
                : QString("color:%1;font-size:13px;font-weight:600;"
                          "background:transparent;%2")
                      .arg(ui::colors::TEXT_PRIMARY(), M(13)));
        card_vl->addWidget(title_lbl);

        // ── Engagement ────────────────────────────────────────────────────────
        auto* eng_row = new QWidget;
        eng_row->setStyleSheet("background:transparent;");
        auto* eng_hl = new QHBoxLayout(eng_row);
        eng_hl->setContentsMargins(0, 2, 0, 0);
        eng_hl->setSpacing(14);

        auto mk_eng = [&](const QString& icon, const QString& val,
                          const QString& color) {
            auto* lbl = new QLabel(icon + " " + val);
            lbl->setStyleSheet(
                QString("color:%1;font-size:10px;background:transparent;%2")
                    .arg(color, M(10)));
            return lbl;
        };

        QString like_color = post.likes > 0 ? ui::colors::AMBER() : QString(ui::colors::BORDER_DIM());
        QString rep_color = post.reply_count > 0 ? ui::colors::CYAN() : QString(ui::colors::BORDER_DIM());

        eng_hl->addWidget(mk_eng("▲", fmt_num(post.likes), like_color));
        eng_hl->addWidget(mk_eng("◆", fmt_num(post.reply_count), rep_color));
        eng_hl->addWidget(mk_eng("◉", fmt_num(post.views), QString(ui::colors::BORDER_DIM())));
        eng_hl->addStretch();

        if (post.reply_count > 5) {
            auto* hot = new QLabel("● HOT");
            hot->setStyleSheet(
                QString("color:%1;font-size:9px;font-weight:700;"
                        "background:rgba(220,38,38,0.08);padding:1px 6px;"
                        "border-radius:6px;%2")
                    .arg(ui::colors::NEGATIVE(), M(9)));
            eng_hl->addWidget(hot);
        }
        card_vl->addWidget(eng_row);

        // ── Click handler ─────────────────────────────────────────────────────
        auto* click_btn = new QPushButton(card);
        click_btn->setGeometry(0, 0, 3000, 200);
        click_btn->setFlat(true);
        click_btn->setStyleSheet("QPushButton{background:transparent;border:none;}");
        click_btn->raise();

        auto p = post;
        connect(click_btn, &QPushButton::clicked, this, [this, p]() {
            active_uuid_ = p.post_uuid;
            rebuild_feed();
            emit post_selected(p);
        });

        feed_layout_->addWidget(card);
    }

    // ── Load more ─────────────────────────────────────────────────────────────
    if (current_page_.page < current_page_.pages) {
        int remaining = current_page_.total - current_page_.posts.size();
        auto* more_btn = new QPushButton(
            QString("Load %1 more posts").arg(remaining));
        more_btn->setFixedHeight(34);
        more_btn->setCursor(Qt::PointingHandCursor);
        more_btn->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:none;"
                    "border-top:1px solid %3;font-size:12px;font-weight:600;%4}"
                    "QPushButton:hover{color:%5;"
                    "background:rgba(217,119,6,0.04);}")
                .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(),
                     ui::colors::BORDER_DIM(), M(12), ui::colors::AMBER()));
        int next = current_page_.page + 1;
        connect(more_btn, &QPushButton::clicked, this,
                [this, next]() { emit load_more_requested(next); });
        feed_layout_->addWidget(more_btn);
    }

    feed_layout_->addStretch();
    feed_container_->setUpdatesEnabled(true);
}

} // namespace fincept::screens
