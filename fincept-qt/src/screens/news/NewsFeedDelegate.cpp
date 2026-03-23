#include "screens/news/NewsFeedDelegate.h"

#include "screens/news/NewsFeedModel.h"
#include "services/news/NewsService.h"
#include "ui/theme/Theme.h"

#include <QPainter>
#include <QStyleOptionViewItem>

namespace fincept::screens {

NewsFeedDelegate::NewsFeedDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
    , data_font_(ui::fonts::DATA_FAMILY, ui::fonts::TINY)
    , bold_font_(ui::fonts::DATA_FAMILY, ui::fonts::TINY)
    , tiny_font_(ui::fonts::DATA_FAMILY, 10)
    , data_fm_(data_font_)
    , bold_fm_(bold_font_)
    , tiny_fm_(tiny_font_) {
    bold_font_.setBold(true);
    bold_fm_ = QFontMetrics(bold_font_);
}

QSize NewsFeedDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    auto mode = index.data(ViewModeRole).toString();
    if (mode == "CLUSTERS") {
        auto cluster = index.data(ClusterRole).value<services::NewsCluster>();
        int related_count = std::max(0, cluster.source_count - 1);
        return QSize(0, 72 + std::min(related_count, 3) * 16);
    }
    return QSize(0, 26);
}

void NewsFeedDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);

    bool selected = index.data(IsSelectedRole).toBool();
    bool hovered = option.state & QStyle::State_MouseOver;

    auto mode = index.data(ViewModeRole).toString();
    if (mode == "CLUSTERS")
        paint_cluster_card(painter, option.rect, index, selected, hovered);
    else
        paint_wire_row(painter, option.rect, index, selected, hovered);

    painter->restore();
}

void NewsFeedDelegate::paint_wire_row(QPainter* painter, const QRect& rect,
                                       const QModelIndex& index, bool selected, bool hovered) const {
    auto article = index.data(ArticleRole).value<services::NewsArticle>();
    auto monitor_color = index.data(MonitorColorRole).toString();
    bool is_new = index.data(IsNewRole).toBool();
    int tier = index.data(SourceTierRole).toInt();

    // Background
    if (selected)
        painter->fillRect(rect, QColor(ui::colors::BG_HOVER));
    else if (hovered)
        painter->fillRect(rect, QColor(ui::colors::BG_RAISED));
    else if (index.row() % 2 == 1)
        painter->fillRect(rect, QColor(ui::colors::ROW_ALT));
    else
        painter->fillRect(rect, QColor(ui::colors::BG_BASE));

    int x = rect.left() + 2;
    int cy = rect.top() + rect.height() / 2;
    int text_y = rect.top() + (rect.height() + data_fm_.ascent() - data_fm_.descent()) / 2;

    // Pulse animation for new items — amber glow that fades
    int pulse = index.data(PulsePhaseRole).toInt();
    if (is_new && pulse >= 0) {
        int alpha = (pulse == 0) ? 40 : (pulse == 1 ? 25 : (pulse == 2 ? 15 : 8));
        painter->fillRect(rect, QColor(217, 119, 6, alpha)); // amber glow overlay
    }

    // New indicator — 3px amber dot
    if (is_new) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(ui::colors::AMBER));
        painter->drawEllipse(QPoint(x + 1, cy), 2, 2);
    }
    x += 6;

    // Monitor color stripe — 3px wide
    if (!monitor_color.isEmpty()) {
        painter->fillRect(QRect(x, rect.top(), 3, rect.height()), QColor(monitor_color));
    }
    x += 5;

    // Time
    painter->setFont(tiny_font_);
    painter->setPen(QColor(ui::colors::TEXT_DIM));
    QString time_str = services::relative_time(article.sort_ts);
    painter->drawText(QRect(x, rect.top(), 36, rect.height()), Qt::AlignVCenter | Qt::AlignRight, time_str);
    x += 40;

    // Priority dot — 4px
    QString pcolor = services::priority_color(article.priority);
    if (article.priority != services::Priority::ROUTINE) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(pcolor));
        painter->drawEllipse(QPoint(x + 2, cy), 2, 2);
    }
    x += 8;

    // Source tier badge
    painter->setFont(tiny_font_);
    if (tier == 1) {
        painter->setPen(QColor(ui::colors::AMBER));
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xe2\x98\x85")); // ★
    } else if (tier == 2) {
        painter->setPen(QColor(ui::colors::TEXT_SECONDARY));
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xe2\x97\x8f")); // ●
    } else if (tier == 3) {
        painter->setPen(QColor(ui::colors::TEXT_TERTIARY));
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xc2\xb7")); // ·
    }
    x += 12;

    // Source name
    painter->setFont(bold_font_);
    painter->setPen(QColor(ui::colors::CYAN));
    QString source = article.source.left(10).toUpper();
    painter->drawText(QRect(x, rect.top(), 72, rect.height()), Qt::AlignVCenter | Qt::AlignLeft, source);
    x += 76;

    // Language badge (if not English)
    if (!article.lang.isEmpty() && article.lang != "en") {
        painter->setFont(tiny_font_);
        painter->setPen(QColor(ui::colors::TEXT_DIM));
        QString lang_badge = article.lang.toUpper();
        painter->drawText(QRect(x, rect.top(), 20, rect.height()), Qt::AlignVCenter | Qt::AlignCenter, lang_badge);
        x += 22;
    }

    // Threat level left border (for MEDIUM+ threats)
    if (article.threat.level != services::ThreatLevel::INFO &&
        article.threat.level != services::ThreatLevel::LOW) {
        QString tcolor = services::threat_level_color(article.threat.level);
        painter->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), QColor(tcolor));
    }

    // Headline — fills remaining space
    painter->setFont(data_font_);
    bool is_hot = article.priority == services::Priority::FLASH ||
                  article.priority == services::Priority::URGENT;
    if (is_hot) {
        painter->setFont(bold_font_);
        painter->setPen(QColor(ui::colors::TEXT_PRIMARY));
    } else {
        painter->setPen(QColor(ui::colors::TEXT_SECONDARY));
    }

    int right_reserve = 140; // space for credibility + threat + sentiment + tickers
    int headline_width = rect.right() - x - right_reserve;
    if (headline_width > 0) {
        QFontMetrics& fm = is_hot ? const_cast<QFontMetrics&>(bold_fm_) : const_cast<QFontMetrics&>(data_fm_);
        QString elided = fm.elidedText(article.headline, Qt::ElideRight, headline_width);
        painter->drawText(QRect(x, rect.top(), headline_width, rect.height()), Qt::AlignVCenter | Qt::AlignLeft, elided);
    }
    x = rect.right() - right_reserve;

    // Source credibility warning
    if (article.source_flag != services::SourceFlag::NONE) {
        painter->setFont(tiny_font_);
        QString flag_label = services::NewsService::source_flag_label(article.source_flag);
        if (article.source_flag == services::SourceFlag::STATE_MEDIA) {
            painter->setPen(QColor("#f97316")); // orange warning
            painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xe2\x9a\xa0")); // ⚠
            x += 12;
        } else {
            painter->setPen(QColor(ui::colors::WARNING));
            painter->drawText(QPoint(x, text_y), "!");
            x += 8;
        }
    }

    // Threat level badge (only for MEDIUM+)
    if (article.threat.level != services::ThreatLevel::INFO &&
        article.threat.level != services::ThreatLevel::LOW) {
        painter->setFont(tiny_font_);
        QString tcolor = services::threat_level_color(article.threat.level);
        painter->setPen(QColor(tcolor));
        QString tlabel = services::threat_level_string(article.threat.level).left(4);
        painter->drawText(QPoint(x, text_y), tlabel);
        x += tiny_fm_.horizontalAdvance(tlabel) + 4;
    }

    // Sentiment arrow
    painter->setFont(data_font_);
    if (article.sentiment == services::Sentiment::BULLISH) {
        painter->setPen(QColor(ui::colors::POSITIVE));
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xe2\x96\xb2")); // ▲
    } else if (article.sentiment == services::Sentiment::BEARISH) {
        painter->setPen(QColor(ui::colors::NEGATIVE));
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xe2\x96\xbc")); // ▼
    } else {
        painter->setPen(QColor(ui::colors::TEXT_DIM));
        painter->drawText(QPoint(x, text_y), "-");
    }
    x += 16;

    // Tickers (up to 2)
    painter->setFont(tiny_font_);
    painter->setPen(QColor(ui::colors::WARNING));
    for (int i = 0; i < std::min(2, static_cast<int>(article.tickers.size())); ++i) {
        QString ticker = "$" + article.tickers[i];
        painter->drawText(QPoint(x, text_y), ticker);
        x += tiny_fm_.horizontalAdvance(ticker) + 6;
    }

    // Bottom border
    painter->setPen(QColor(ui::colors::BORDER_DIM));
    painter->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
}

void NewsFeedDelegate::paint_cluster_card(QPainter* painter, const QRect& rect,
                                            const QModelIndex& index, bool selected, bool hovered) const {
    auto cluster = index.data(ClusterRole).value<services::NewsCluster>();
    auto monitor_color = index.data(MonitorColorRole).toString();
    bool is_new = index.data(IsNewRole).toBool();
    auto velocity_text = index.data(VelocityTextRole).toString();

    // Card background
    QColor bg = selected ? QColor(ui::colors::BG_HOVER) : (hovered ? QColor(ui::colors::BG_RAISED) : QColor(ui::colors::BG_SURFACE));
    painter->fillRect(rect, bg);

    // Card border
    painter->setPen(QColor(ui::colors::BORDER_DIM));
    painter->drawRect(rect.adjusted(0, 0, -1, -1));

    int x = rect.left() + 8;
    int y = rect.top() + 4;

    // Monitor stripe
    if (!monitor_color.isEmpty())
        painter->fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), QColor(monitor_color));

    // New indicator
    if (is_new) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(ui::colors::AMBER));
        painter->drawEllipse(QPoint(rect.left() + 5, rect.top() + 8), 2, 2);
    }

    // Row 1: [BREAKING badge] [tier] [headline] [velocity]
    if (cluster.is_breaking) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(ui::colors::NEGATIVE));
        QRect badge_rect(x, y, 62, 16);
        painter->drawRect(badge_rect);
        painter->setFont(tiny_font_);
        painter->setPen(QColor(ui::colors::TEXT_PRIMARY));
        painter->drawText(badge_rect, Qt::AlignCenter, "BREAKING");
        x += 66;
    }

    // Tier badge
    painter->setFont(tiny_font_);
    if (cluster.tier == 1) {
        painter->setPen(QColor(ui::colors::AMBER));
        painter->drawText(QPoint(x, y + 12), QString::fromUtf8("\xe2\x98\x85"));
    } else if (cluster.tier == 2) {
        painter->setPen(QColor(ui::colors::TEXT_SECONDARY));
        painter->drawText(QPoint(x, y + 12), QString::fromUtf8("\xe2\x97\x8f"));
    }
    x += 14;

    // Headline
    painter->setFont(bold_font_);
    painter->setPen(QColor(ui::colors::TEXT_PRIMARY));
    int headline_w = rect.right() - x - 80;
    QString elided = bold_fm_.elidedText(cluster.lead_article.headline, Qt::ElideRight, headline_w);
    painter->drawText(QRect(x, y, headline_w, 18), Qt::AlignVCenter | Qt::AlignLeft, elided);

    // Velocity badge
    if (!velocity_text.isEmpty()) {
        int vx = rect.right() - 70;
        painter->setFont(tiny_font_);
        painter->setPen(cluster.velocity == "rising" ? QColor(ui::colors::POSITIVE) : QColor(ui::colors::TEXT_TERTIARY));
        painter->drawText(QPoint(vx, y + 12), velocity_text);
    }

    // Row 2: source | time | N sources | sentiment
    y += 22;
    x = rect.left() + 8;
    painter->setFont(data_font_);

    // Source
    painter->setPen(QColor(ui::colors::CYAN));
    QString source = cluster.lead_article.source.left(12).toUpper();
    painter->drawText(QPoint(x, y + 12), source);
    x += data_fm_.horizontalAdvance(source) + 10;

    // Time
    painter->setPen(QColor(ui::colors::TEXT_DIM));
    QString time_str = services::relative_time(cluster.latest_sort_ts);
    painter->drawText(QPoint(x, y + 12), time_str);
    x += data_fm_.horizontalAdvance(time_str) + 10;

    // Source count
    if (cluster.source_count > 1) {
        painter->setPen(QColor(ui::colors::WARNING));
        QString src_text = QString("%1 sources").arg(cluster.source_count);
        painter->drawText(QPoint(x, y + 12), src_text);
        x += data_fm_.horizontalAdvance(src_text) + 10;
    }

    // Sentiment
    if (cluster.sentiment == services::Sentiment::BULLISH) {
        painter->setPen(QColor(ui::colors::POSITIVE));
        painter->drawText(QPoint(x, y + 12), QString::fromUtf8("\xe2\x96\xb2 BULL"));
    } else if (cluster.sentiment == services::Sentiment::BEARISH) {
        painter->setPen(QColor(ui::colors::NEGATIVE));
        painter->drawText(QPoint(x, y + 12), QString::fromUtf8("\xe2\x96\xbc BEAR"));
    }

    // Row 3: "Also: Source1, Source2, Source3"
    y += 18;
    x = rect.left() + 8;
    if (cluster.articles.size() > 1) {
        painter->setFont(tiny_font_);
        painter->setPen(QColor(ui::colors::TEXT_TERTIARY));

        QStringList also_sources;
        QSet<QString> seen;
        seen.insert(cluster.lead_article.source);
        for (const auto& a : cluster.articles) {
            if (!seen.contains(a.source) && also_sources.size() < 3) {
                also_sources.append(a.source);
                seen.insert(a.source);
            }
        }
        if (!also_sources.isEmpty()) {
            QString also_text = "Also: " + also_sources.join(", ");
            int also_w = rect.right() - x - 8;
            also_text = tiny_fm_.elidedText(also_text, Qt::ElideRight, also_w);
            painter->drawText(QPoint(x, y + 10), also_text);
        }
    }
}

} // namespace fincept::screens
