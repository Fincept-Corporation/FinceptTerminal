// src/screens/forum/ForumPostListPanel.cpp
#include "screens/forum/ForumPostListPanel.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>

#include <cmath>

namespace fincept::screens {

static QString mono(int sz = 12) {
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
        return QString("%1s").arg(sec);
    if (sec < 3600)
        return QString("%1m").arg(sec / 60);
    if (sec < 86400)
        return QString("%1h").arg(sec / 3600);
    return QString("%1d").arg(sec / 86400);
}

// Compact number formatter
static QString fmt_num(int n) {
    if (n >= 1000)
        return QString("%1k").arg(n / 1000);
    return QString::number(n);
}

// Deterministic color from string
static QString str_color(const QString& s) {
    static const QString palette[] = {"#d97706", "#06b6d4", "#10b981", "#8b5cf6", "#ef4444",
                                      "#f97316", "#3b82f6", "#84cc16", "#ec4899", "#14b8a6"};
    uint h = qHash(s);
    return palette[h % 10];
}

ForumPostListPanel::ForumPostListPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ForumPostListPanel::build_ui() {
    setStyleSheet("background:#080808;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Channel header ────────────────────────────────────────────────────────
    auto* hdr = new QWidget;
    hdr->setFixedHeight(38);
    hdr->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #111111;");
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(12, 0, 12, 0);
    hdr_hl->setSpacing(8);

    auto* hash = new QLabel("#");
    hash->setStyleSheet(
        QString("color:#333333;font-size:18px;font-weight:700;background:transparent;%1").arg(mono(18)));
    hash->setFixedWidth(16);

    channel_label_ = new QLabel("all-posts");
    channel_label_->setStyleSheet(
        QString("color:#888888;font-size:13px;font-weight:700;background:transparent;%1").arg(mono(13)));

    count_label_ = new QLabel;
    count_label_->setStyleSheet(QString("color:#2a2a2a;font-size:11px;background:transparent;%1").arg(mono(11)));

    hdr_hl->addWidget(hash);
    hdr_hl->addWidget(channel_label_, 1);
    hdr_hl->addWidget(count_label_);
    root->addWidget(hdr);

    // ── Scrollable card feed ───────────────────────────────────────────────────
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setStyleSheet("QScrollArea{border:none;background:#080808;}"
                           "QScrollBar:vertical{width:3px;background:transparent;margin:0;}"
                           "QScrollBar::handle:vertical{background:#1a1a1a;min-height:20px;}"
                           "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    feed_container_ = new QWidget;
    feed_container_->setStyleSheet("background:#080808;");
    feed_layout_ = new QVBoxLayout(feed_container_);
    feed_layout_->setContentsMargins(0, 0, 0, 0);
    feed_layout_->setSpacing(0);
    feed_layout_->addStretch();

    scroll_->setWidget(feed_container_);
    root->addWidget(scroll_, 1);

    // Skeleton timer
    skeleton_timer_ = new QTimer(this);
    skeleton_timer_->setInterval(400);
    connect(skeleton_timer_, &QTimer::timeout, this, &ForumPostListPanel::pulse_skeleton);
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
    sk_vl->setContentsMargins(0, 0, 0, 0);
    sk_vl->setSpacing(1);

    for (int i = 0; i < 8; ++i) {
        auto* card = new QWidget;
        card->setFixedHeight(72);
        card->setStyleSheet("background:#0a0a0a;border-bottom:1px solid #0d0d0d;");
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(12, 10, 12, 10);
        cvl->setSpacing(6);

        // Title shimmer bar
        auto* bar1 = new QWidget;
        bar1->setFixedHeight(10);
        bar1->setStyleSheet(QString("background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                                    "stop:0 #111111,stop:%1 #1a1a1a,stop:1 #111111);")
                                .arg(0.3 + (i % 4) * 0.15));
        bar1->setMaximumWidth(200 + i * 20);

        // Meta shimmer bar
        auto* bar2 = new QWidget;
        bar2->setFixedHeight(8);
        bar2->setStyleSheet("background:#0d0d0d;");
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
                         "stop:0 #090909,stop:%1 #1c1c1c,stop:%2 #090909);")
                     .arg(s, 0, 'f', 2)
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
            if (ci && ci->widget() && ci->widget()->height() >= 8 && ci->widget()->height() <= 12)
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

void ForumPostListPanel::set_posts(const services::ForumPostsPage& page, const QString& category_color) {
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
    count_label_->setText(QString("%1 posts").arg(current_page_.total));

    if (current_page_.posts.isEmpty()) {
        auto* empty = new QWidget;
        empty->setFixedHeight(80);
        empty->setStyleSheet("background:transparent;");
        auto* el = new QVBoxLayout(empty);
        auto* lbl = new QLabel("NO POSTS YET");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:#222222;font-size:13px;font-weight:700;letter-spacing:1px;"
                                   "background:transparent;%1")
                               .arg(mono(13)));
        auto* sub = new QLabel("Be the first to start a discussion");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet(QString("color:#1a1a1a;font-size:11px;background:transparent;%1").arg(mono(11)));
        el->addStretch();
        el->addWidget(lbl);
        el->addWidget(sub);
        el->addStretch();
        feed_layout_->addWidget(empty);
        feed_layout_->addStretch();
        return;
    }

    feed_container_->setUpdatesEnabled(false);

    for (int i = 0; i < current_page_.posts.size(); ++i) {
        const auto& post = current_page_.posts[i];
        const bool active = (post.post_uuid == active_uuid_);

        // ── Post card ─────────────────────────────────────────────────────────
        auto* card = new QWidget;
        card->setCursor(Qt::PointingHandCursor);

        QString cat_color = post.category_color.isEmpty() ? str_color(post.category_name) : post.category_color;

        card->setStyleSheet(active ? QString("background:#0d0d0d;border-bottom:1px solid #111111;"
                                             "border-left:3px solid %1;")
                                         .arg(cat_color)
                                   : "background:#080808;border-bottom:1px solid #0d0d0d;"
                                     "border-left:3px solid transparent;");

        auto* card_vl = new QVBoxLayout(card);
        card_vl->setContentsMargins(12, 8, 12, 6);
        card_vl->setSpacing(4);

        // ── Top row: avatar + name + category chip + time ─────────────────────
        auto* top_row = new QWidget;
        top_row->setStyleSheet("background:transparent;");
        auto* top_hl = new QHBoxLayout(top_row);
        top_hl->setContentsMargins(0, 0, 0, 0);
        top_hl->setSpacing(6);

        // Avatar square with initials
        auto* avatar = new QLabel;
        avatar->setFixedSize(22, 22);
        avatar->setAlignment(Qt::AlignCenter);
        QString av_color = str_color(post.author_display_name);
        QString initials = post.author_display_name.isEmpty() ? "?" : post.author_display_name.left(2).toUpper();
        avatar->setText(initials);
        avatar->setStyleSheet(
            QString("color:#080808;font-size:9px;font-weight:700;background:%1;%2").arg(av_color, mono(9)));

        // Author name
        auto* author = new QLabel(post.author_display_name.left(16));
        author->setStyleSheet(
            QString("color:#888888;font-size:11px;font-weight:600;background:transparent;%1").arg(mono(11)));

        // Category chip
        auto* cat_chip = new QLabel(post.category_name.toUpper().left(12));
        cat_chip->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;background:rgba(0,0,0,0);"
                                        "border:1px solid %1;padding:0 4px;letter-spacing:0.5px;%2")
                                    .arg(cat_color, mono(9)));

        // Time
        auto* time_lbl = new QLabel(rel_time(post.created_at));
        time_lbl->setStyleSheet(QString("color:#2a2a2a;font-size:10px;background:transparent;%1").arg(mono(10)));

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
            active ? QString("color:#e5e5e5;font-size:13px;font-weight:700;background:transparent;%1").arg(mono(13))
                   : QString("color:#cccccc;font-size:13px;background:transparent;%1").arg(mono(13)));
        card_vl->addWidget(title_lbl);

        // ── Engagement bar ────────────────────────────────────────────────────
        auto* eng_row = new QWidget;
        eng_row->setStyleSheet("background:transparent;");
        auto* eng_hl = new QHBoxLayout(eng_row);
        eng_hl->setContentsMargins(0, 0, 0, 0);
        eng_hl->setSpacing(12);

        auto make_eng = [&](const QString& icon, const QString& val, const QString& color) {
            auto* w = new QWidget;
            w->setStyleSheet("background:transparent;");
            auto* hl = new QHBoxLayout(w);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(3);
            auto* ic = new QLabel(icon);
            ic->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;%2").arg(color, mono(10)));
            auto* vl = new QLabel(val);
            vl->setStyleSheet(QString("color:#444444;font-size:10px;background:transparent;%1").arg(mono(10)));
            hl->addWidget(ic);
            hl->addWidget(vl);
            return w;
        };

        QString like_color = post.likes > 0 ? "#d97706" : "#2a2a2a";
        QString rep_color = post.reply_count > 0 ? "#06b6d4" : "#2a2a2a";

        eng_hl->addWidget(make_eng("▲", fmt_num(post.likes), like_color));
        eng_hl->addWidget(make_eng("◆", fmt_num(post.reply_count), rep_color));
        eng_hl->addWidget(make_eng("◉", fmt_num(post.views), "#2a2a2a"));
        eng_hl->addStretch();

        // Hot indicator if replies > 5
        if (post.reply_count > 5) {
            auto* hot = new QLabel("HOT");
            hot->setStyleSheet(QString("color:#ef4444;font-size:9px;font-weight:700;letter-spacing:0.8px;"
                                       "background:transparent;%1")
                                   .arg(mono(9)));
            eng_hl->addWidget(hot);
        }

        card_vl->addWidget(eng_row);

        // ── Click handler ────────────────────────────────────────────────────
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
        auto* more_btn = new QPushButton(QString("↓  LOAD %1 MORE POSTS").arg(remaining));
        more_btn->setFixedHeight(32);
        more_btn->setCursor(Qt::PointingHandCursor);
        more_btn->setStyleSheet(QString("QPushButton{background:#0a0a0a;color:#333333;border:none;"
                                        "border-top:1px solid #111111;font-size:11px;font-weight:700;"
                                        "letter-spacing:0.5px;%1}"
                                        "QPushButton:hover{background:#0d0d0d;color:%2;}")
                                    .arg(mono(11), ui::colors::AMBER));
        int next = current_page_.page + 1;
        connect(more_btn, &QPushButton::clicked, this, [this, next]() { emit load_more_requested(next); });
        feed_layout_->addWidget(more_btn);
    }

    feed_layout_->addStretch();
    feed_container_->setUpdatesEnabled(true);
}

} // namespace fincept::screens
