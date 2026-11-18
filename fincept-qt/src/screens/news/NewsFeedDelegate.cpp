#include "screens/news/NewsFeedDelegate.h"

#include "screens/news/NewsFeedModel.h"
#include "services/news/NewsService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QPainter>
#include <QStyleOptionViewItem>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

// Cluster card line heights: 18px lines at the default font size, growing with it.
int cluster_headline_height(const QFontMetrics& bold_fm) {
    return std::max(18, bold_fm.height() + 2);
}
int cluster_meta_height(const QFontMetrics& data_fm) {
    return std::max(18, data_fm.height() + 2);
}

/// Width of the tone bar (centre line, fill toward the sign, length by magnitude).
constexpr int kNfdToneBarW = 28;
/// Width of the impact "signal bars" glyph.
constexpr int kNfdImpactW = 12;

/// Three rising bars, `level` (1..3) of them lit — market impact LOW / MEDIUM / HIGH.
void nfd_paint_impact(QPainter* p, const QRect& r, int level, const QColor& lit, const QColor& dim) {
    const int bar_w = 2;
    const int gap = 2;
    const int base = r.center().y() + 4;
    for (int i = 0; i < 3; ++i) {
        const int h = 3 + i * 2;
        const QRect bar(r.left() + i * (bar_w + gap), base - h, bar_w, h);
        p->fillRect(bar, i < level ? lit : dim);
    }
}

/// Signed tone bar: grey track, centre tick, fill from the centre toward the
/// sign in the direction colours. Not scored: a dim "n/s".
void nfd_paint_tone(QPainter* p, const QRect& r, const services::NewsArticle& a, const QFont& tiny,
                    const QColor& faint) {
    if (!a.sentiment_scored) {
        p->setFont(tiny);
        p->setPen(faint);
        p->drawText(r, Qt::AlignCenter, QCoreApplication::translate("NewsFeedDelegate", "n/s"));
        return;
    }
    const int cy = r.center().y();
    const QRect track(r.left(), cy - 2, r.width(), 4);
    p->fillRect(track, QColor(ui::colors::BORDER_DIM()));
    const int mid = track.left() + track.width() / 2;
    const double mag = std::clamp(std::abs(a.sentiment_score), 0.0, 1.0);
    const int len = static_cast<int>(std::lround(mag * (track.width() / 2)));
    if (len > 0) {
        const bool up = a.sentiment_score > 0;
        const QRect fill = up ? QRect(mid, track.top(), len, track.height())
                              : QRect(mid - len, track.top(), len, track.height());
        p->fillRect(fill, QColor(up ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    }
    p->fillRect(QRect(mid, cy - 3, 1, 6), QColor(ui::colors::TEXT_TERTIARY()));
}

} // namespace

NewsFeedDelegate::NewsFeedDelegate(QObject* parent)
    : QStyledItemDelegate(parent),
      data_font_(ui::fonts::font(-2)),
      bold_font_(ui::fonts::font(-2, QFont::Bold)),
      tiny_font_(ui::fonts::font(-3)),
      data_fm_(data_font_),
      bold_fm_(bold_font_),
      tiny_fm_(tiny_font_) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_fonts(); });
}

void NewsFeedDelegate::refresh_fonts() {
    data_font_ = ui::fonts::font(-2);
    bold_font_ = ui::fonts::font(-2, QFont::Bold);
    tiny_font_ = ui::fonts::font(-3);
    data_fm_ = QFontMetrics(data_font_);
    bold_fm_ = QFontMetrics(bold_font_);
    tiny_fm_ = QFontMetrics(tiny_font_);
    // Row heights follow the font; the view re-lays out its items on this signal.
    emit sizeHintChanged(QModelIndex());
}

QSize NewsFeedDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    auto mode = index.data(ViewModeRole).toString();
    if (mode == "CLUSTERS") {
        // Variable height: the "Also:" line only when other sources carried it.
        const int also_lines = index.data(CardHeightRole).toInt();
        const int body = 4 + cluster_headline_height(bold_fm_) + 4 + cluster_meta_height(data_fm_) +
                         also_lines * std::max(16, tiny_fm_.height() + 4) + 6;
        return QSize(0, std::max(also_lines > 0 ? 62 : 46, body));
    }
    return QSize(0, std::max(26, data_fm_.height() + 10));
}

void NewsFeedDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
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

void NewsFeedDelegate::paint_wire_row(QPainter* painter, const QRect& rect, const QModelIndex& index, bool selected,
                                      bool hovered) const {
    // GCC 12 false-positive: -Warray-bounds on QVariant::value<NewsArticle>()
    // inlined move/copy — the analyzer wrongly treats the temporary as
    // QVariant[1]. Suppress locally; struct layout is fixed and QVariant
    // conversion is validated by QMetaType.
#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    auto article = index.data(ArticleRole).value<services::NewsArticle>();
#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic pop
#endif
    auto monitor_color = index.data(MonitorColorRole).toString();
    bool is_new = index.data(IsNewRole).toBool();
    int tier = index.data(SourceTierRole).toInt();

    // Background
    if (selected)
        painter->fillRect(rect, QColor(ui::colors::BG_HOVER()));
    else if (hovered)
        painter->fillRect(rect, QColor(ui::colors::BG_RAISED()));
    else if (index.row() % 2 == 1)
        painter->fillRect(rect, QColor(ui::colors::ROW_ALT()));
    else
        painter->fillRect(rect, QColor(ui::colors::BG_BASE()));

    // Faint text tokens only read on the base / surface backgrounds; a hovered or
    // selected row steps them up to the secondary text colour.
    const bool lit = selected || hovered;
    const QColor faint_text = lit ? QColor(ui::colors::TEXT_SECONDARY()) : QColor(ui::colors::TEXT_DIM());
    const QColor muted_text = lit ? QColor(ui::colors::TEXT_SECONDARY()) : QColor(ui::colors::TEXT_TERTIARY());

    int x = rect.left() + 2;
    int cy = rect.top() + rect.height() / 2;
    int text_y = rect.top() + (rect.height() + data_fm_.ascent() - data_fm_.descent()) / 2;

    // Pulse animation for new items — accent glow that fades
    int pulse = index.data(PulsePhaseRole).toInt();
    if (is_new && pulse >= 0) {
        int alpha = (pulse == 0) ? 40 : (pulse == 1 ? 25 : (pulse == 2 ? 15 : 8));
        QColor glow(ui::colors::AMBER());
        glow.setAlpha(alpha);
        painter->fillRect(rect, glow); // accent glow overlay
    }

    // New indicator — 3px amber dot
    if (is_new) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(ui::colors::AMBER()));
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
    painter->setPen(faint_text);
    // An undated item has no age to show (its time is only when we first saw it).
    QString time_str = article.undated ? QString(QChar(0x2014)) : services::relative_time(article.sort_ts);
    const int time_w = std::max(36, tiny_fm_.horizontalAdvance(QStringLiteral("000m")));
    painter->drawText(QRect(x, rect.top(), time_w, rect.height()), Qt::AlignVCenter | Qt::AlignRight, time_str);
    x += time_w + 4;

    // Priority dot — 4px
    QString pcolor = services::priority_color(article.priority);
    if (article.priority != services::Priority::ROUTINE) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(ui::colors::legible_color(QColor(pcolor)));
        painter->drawEllipse(QPoint(x + 2, cy), 2, 2);
    }
    x += 8;

    // Source tier badge
    painter->setFont(tiny_font_);
    if (tier == 1) {
        painter->setPen(QColor(ui::colors::AMBER()));
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xe2\x98\x85")); // ★
    } else if (tier == 2) {
        painter->setPen(QColor(ui::colors::TEXT_SECONDARY()));
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xe2\x97\x8f")); // ●
    } else if (tier == 3) {
        painter->setPen(muted_text);
        painter->drawText(QPoint(x, text_y), QString::fromUtf8("\xc2\xb7")); // ·
    }
    x += std::max(12, tiny_fm_.horizontalAdvance(QString::fromUtf8("\xe2\x98\x85")) + 4);

    // Pre-formatted strings from model — zero allocation in paint path
    const QString source = index.data(FormattedSourceRole).toString();
    const QString lang_badge = index.data(FormattedLangRole).toString();
    const QString threat_label = index.data(FormattedThreatRole).toString();
    const QString tickers_str = index.data(FormattedTickersRole).toString();
    const QString threat_color = index.data(ThreatColorRole).toString();
    const QString event_label = index.data(EventLabelRole).toString();
    const QString earnings_badge = index.data(EarningsBadgeRole).toString();
    const int duplicates = index.data(DuplicateCountRole).toInt();
    const bool has_geo = index.data(HasGeoRole).toBool();

    // Source name, elided to its column (the full name is in the tooltip).
    painter->setFont(bold_font_);
    painter->setPen(QColor(ui::colors::CYAN()));
    const int source_w = std::max(72, bold_fm_.horizontalAdvance(QString(11, QLatin1Char('M'))));
    painter->drawText(QRect(x, rect.top(), source_w, rect.height()), Qt::AlignVCenter | Qt::AlignLeft,
                      bold_fm_.elidedText(source, Qt::ElideRight, source_w));
    x += source_w + 4;

    // Language badge (if not English)
    if (!lang_badge.isEmpty()) {
        painter->setFont(tiny_font_);
        painter->setPen(faint_text);
        const int lang_w = std::max(20, tiny_fm_.horizontalAdvance(QStringLiteral("WW")) + 2);
        painter->drawText(QRect(x, rect.top(), lang_w, rect.height()), Qt::AlignVCenter | Qt::AlignCenter,
                          lang_badge);
        x += lang_w + 2;
    }

    // Geolocated story (a place the gazetteer recognised) — small ring.
    if (has_geo) {
        painter->setPen(QPen(QColor(ui::colors::INFO()), 1.2));
        painter->setBrush(Qt::NoBrush);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawEllipse(QPoint(x + 3, cy), 3, 3);
        painter->setRenderHint(QPainter::Antialiasing, false);
        x += 10;
    }

    // Threat level left border (for MEDIUM+ threats)
    const QColor threat = threat_color.isEmpty() ? QColor() : ui::colors::legible_color(QColor(threat_color));
    if (threat.isValid())
        painter->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), threat);

    // ── Right-hand group, laid out from the right edge. Items are dropped from
    // the least important end when the headline would get too narrow, instead of
    // a fixed reserve that let long tags overflow into the next column.
    const int min_headline_w = std::max(140, (rect.right() - x) * 45 / 100);
    int right = rect.right() - 6;
    const int right_limit = x + min_headline_w;
    auto fits = [&](int w) { return right - w - 6 >= right_limit; };

    // 1. Tone bar (always) and impact bars.
    {
        const QRect tone_r(right - kNfdToneBarW, rect.top(), kNfdToneBarW, rect.height());
        nfd_paint_tone(painter, tone_r, article, tiny_font_, faint_text);
        right = tone_r.left() - 6;
        const int level = article.impact == services::Impact::HIGH ? 3 : (article.impact == services::Impact::MEDIUM ? 2 : 1);
        const QColor lit_c = article.impact == services::Impact::HIGH ? QColor(ui::colors::AMBER())
                                                                       : QColor(ui::colors::TEXT_SECONDARY());
        nfd_paint_impact(painter, QRect(right - kNfdImpactW, rect.top(), kNfdImpactW, rect.height()), level, lit_c,
                         QColor(ui::colors::BORDER_MED()));
        right -= kNfdImpactW + 8;
    }
    painter->setFont(tiny_font_);
    auto draw_right_text = [&](const QString& text, const QColor& color) -> bool {
        if (text.isEmpty())
            return true;
        const int w = tiny_fm_.horizontalAdvance(text);
        if (!fits(w))
            return false;
        painter->setPen(color);
        painter->drawText(QRect(right - w, rect.top(), w, rect.height()), Qt::AlignVCenter | Qt::AlignRight, text);
        right -= w + 8;
        return true;
    };
    // 2. Tickers, duplicates, earnings, event, threat, source flag — in that order.
    bool room = draw_right_text(tickers_str, QColor(ui::colors::WARNING()));
    if (room && duplicates > 0)
        room = draw_right_text(QStringLiteral("+%1").arg(duplicates), muted_text);
    if (room)
        room = draw_right_text(earnings_badge, QColor(ui::colors::INFO()));
    if (room)
        room = draw_right_text(event_label, QColor(ui::colors::CYAN()));
    if (room)
        room = draw_right_text(threat_label, threat);
    if (room && article.source_flag != services::SourceFlag::NONE)
        draw_right_text(article.source_flag == services::SourceFlag::STATE_MEDIA ? QString::fromUtf8("\xe2\x9a\xa0")
                                                                                 : QStringLiteral("!"),
                        QColor(ui::colors::WARNING()));

    // Headline — fills what is left between the left columns and the right group.
    bool is_hot = article.priority == services::Priority::FLASH || article.priority == services::Priority::URGENT;
    painter->setFont(is_hot ? bold_font_ : data_font_);
    painter->setPen(is_hot ? QColor(ui::colors::TEXT_PRIMARY()) : QColor(ui::colors::TEXT_SECONDARY()));
    const int headline_width = right - x;
    if (headline_width > 0) {
        QFontMetrics& fm = is_hot ? bold_fm_ : data_fm_;
        const QString elided = fm.elidedText(article.headline, Qt::ElideRight, headline_width);
        painter->drawText(QRect(x, rect.top(), headline_width, rect.height()), Qt::AlignVCenter | Qt::AlignLeft,
                          elided);
    }

    // Bottom border
    painter->setPen(QColor(ui::colors::BORDER_DIM()));
    painter->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
}

void NewsFeedDelegate::paint_cluster_card(QPainter* painter, const QRect& rect, const QModelIndex& index, bool selected,
                                          bool hovered) const {
    // Same GCC 12 false positive as paint_wire_row() (-Warray-bounds on the inlined move out
    // of the QVariant temporary).
#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    auto cluster = index.data(ClusterRole).value<services::NewsCluster>();
#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic pop
#endif
    auto monitor_color = index.data(MonitorColorRole).toString();
    bool is_new = index.data(IsNewRole).toBool();
    auto velocity_text = index.data(VelocityTextRole).toString();
    const QString earnings_badge = index.data(EarningsBadgeRole).toString();

    // Card background
    QColor bg = selected ? QColor(ui::colors::BG_HOVER())
                         : (hovered ? QColor(ui::colors::BG_RAISED()) : QColor(ui::colors::BG_SURFACE()));
    painter->fillRect(rect, bg);

    // Faint text tokens only read on the surface background; a hovered or
    // selected card steps them up to the secondary text colour.
    const bool lit = selected || hovered;
    const QColor faint_text = lit ? QColor(ui::colors::TEXT_SECONDARY()) : QColor(ui::colors::TEXT_DIM());
    const QColor muted_text = lit ? QColor(ui::colors::TEXT_SECONDARY()) : QColor(ui::colors::TEXT_TERTIARY());

    // Card border
    painter->setPen(QColor(ui::colors::BORDER_DIM()));
    painter->drawRect(rect.adjusted(0, 0, -1, -1));

    int x = rect.left() + 8;
    int y = rect.top() + 4;
    const int line1_h = cluster_headline_height(bold_fm_);
    const int line1_base = y + (line1_h + tiny_fm_.ascent() - tiny_fm_.descent()) / 2;

    // Monitor stripe (user-chosen monitor colour)
    if (!monitor_color.isEmpty())
        painter->fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), QColor(monitor_color));

    // New indicator
    if (is_new) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(ui::colors::AMBER()));
        painter->drawEllipse(QPoint(rect.left() + 5, rect.top() + 8), 2, 2);
    }

    // Row 1: [BREAKING badge] [tier] [headline] [velocity]
    if (cluster.is_breaking) {
        const QString badge_text = tr("BREAKING");
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(ui::colors::DANGER()));
        QRect badge_rect(x, y + (line1_h - std::max(16, tiny_fm_.height() + 2)) / 2,
                         std::max(62, tiny_fm_.horizontalAdvance(badge_text) + 8),
                         std::max(16, tiny_fm_.height() + 2));
        painter->drawRect(badge_rect);
        painter->setFont(tiny_font_);
        painter->setPen(QColor(ui::colors::TEXT_ON_ACCENT()));
        painter->drawText(badge_rect, Qt::AlignCenter, badge_text);
        x += badge_rect.width() + 4;
    }

    // Tier badge
    painter->setFont(tiny_font_);
    if (cluster.tier == 1) {
        painter->setPen(QColor(ui::colors::AMBER()));
        painter->drawText(QPoint(x, line1_base), QString::fromUtf8("\xe2\x98\x85"));
    } else if (cluster.tier == 2) {
        painter->setPen(QColor(ui::colors::TEXT_SECONDARY()));
        painter->drawText(QPoint(x, line1_base), QString::fromUtf8("\xe2\x97\x8f"));
    }
    x += std::max(14, tiny_fm_.horizontalAdvance(QString::fromUtf8("\xe2\x98\x85")) + 6);

    // Headline
    const int velocity_w =
        velocity_text.isEmpty() ? 0 : std::max(70, tiny_fm_.horizontalAdvance(velocity_text) + 8);
    painter->setFont(bold_font_);
    painter->setPen(QColor(ui::colors::TEXT_PRIMARY()));
    int headline_w = rect.right() - x - (velocity_w + 10);
    QString elided = bold_fm_.elidedText(cluster.lead_article.headline, Qt::ElideRight, headline_w);
    painter->drawText(QRect(x, y, headline_w, line1_h), Qt::AlignVCenter | Qt::AlignLeft, elided);

    // Velocity badge: rising = more articles per hour than the 6 h before.
    if (!velocity_text.isEmpty()) {
        int vx = rect.right() - velocity_w;
        painter->setFont(tiny_font_);
        painter->setPen(cluster.velocity == "rising" ? QColor(ui::colors::AMBER()) : muted_text);
        painter->drawText(QPoint(vx, line1_base), velocity_text);
    }

    // Row 2: source | time | N sources | sentiment | earnings
    y += line1_h + 4;
    x = rect.left() + 8;
    painter->setFont(data_font_);
    const int line2_h = cluster_meta_height(data_fm_);
    const int line2_base = y + (line2_h + data_fm_.ascent() - data_fm_.descent()) / 2;

    // Source
    painter->setPen(QColor(ui::colors::CYAN()));
    QString source = data_fm_.elidedText(cluster.lead_article.source.toUpper(), Qt::ElideRight,
                                         data_fm_.horizontalAdvance(QString(16, QLatin1Char('M'))));
    painter->drawText(QPoint(x, line2_base), source);
    x += data_fm_.horizontalAdvance(source) + 10;

    // Time
    painter->setPen(faint_text);
    QString time_str = services::relative_time(cluster.latest_sort_ts);
    painter->drawText(QPoint(x, line2_base), time_str);
    x += data_fm_.horizontalAdvance(time_str) + 10;

    // Source count
    if (cluster.source_count > 1) {
        painter->setPen(QColor(ui::colors::WARNING()));
        QString src_text = tr("%1 sources").arg(cluster.source_count);
        painter->drawText(QPoint(x, line2_base), src_text);
        x += data_fm_.horizontalAdvance(src_text) + 10;
    }

    // Sentiment
    if (cluster.sentiment == services::Sentiment::BULLISH) {
        painter->setPen(QColor(ui::colors::POSITIVE()));
        const QString t = tr("\xe2\x96\xb2 BULL");
        painter->drawText(QPoint(x, line2_base), t);
        x += data_fm_.horizontalAdvance(t) + 10;
    } else if (cluster.sentiment == services::Sentiment::BEARISH) {
        painter->setPen(QColor(ui::colors::NEGATIVE()));
        const QString t = tr("\xe2\x96\xbc BEAR");
        painter->drawText(QPoint(x, line2_base), t);
        x += data_fm_.horizontalAdvance(t) + 10;
    }
    if (!earnings_badge.isEmpty()) {
        painter->setPen(QColor(ui::colors::INFO()));
        painter->drawText(QPoint(x, line2_base), earnings_badge);
    }

    // Row 3: "Also: Source1, Source2, Source3"
    y += line2_h;
    x = rect.left() + 8;
    if (cluster.articles.size() > 1) {
        painter->setFont(tiny_font_);
        painter->setPen(muted_text);

        QStringList also_sources;
        QSet<QString> seen;
        seen.insert(cluster.lead_article.source);
        int more = 0;
        for (const auto& a : cluster.articles) {
            if (seen.contains(a.source))
                continue;
            seen.insert(a.source);
            if (also_sources.size() < 4)
                also_sources.append(a.source);
            else
                ++more;
        }
        if (!also_sources.isEmpty()) {
            QString also_text = tr("Also: ") + also_sources.join(", ");
            if (more > 0)
                also_text += tr(" +%1 more").arg(more);
            int also_w = rect.right() - x - 8;
            also_text = tiny_fm_.elidedText(also_text, Qt::ElideRight, also_w);
            painter->drawText(QPoint(x, y + std::max(10, tiny_fm_.ascent())), also_text);
        }
    }
}

} // namespace fincept::screens
