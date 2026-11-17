#include "services/news/NewsMonitorService.h"

#include "core/logging/Logger.h"
#include "storage/repositories/NewsMonitorRepository.h"
#include "ui/theme/ThemeTokens.h"

#include <QRegularExpression>
#include <QUuid>

namespace fincept::services {

// Colours handed to new monitors in turn. A monitor's colour is stored with it,
// so it must not depend on the theme in effect when the monitor was created:
// these are the dark preset's categorical chart colours (fixed values, never
// the active theme's), drawn through ui::colors::legible() / tint() like any
// other identity colour.
const QStringList& monitor_colors() {
    static const QStringList colors = [] {
        QStringList out;
        for (const char* c : ui::THEME_OBSIDIAN.chart_palette)
            out << QString::fromLatin1(c);
        return out;
    }();
    return colors;
}

NewsMonitorService& NewsMonitorService::instance() {
    static NewsMonitorService s;
    return s;
}

NewsMonitorService::NewsMonitorService() {
    // Table now created by MigrationRunner v001 — no ensure_table() needed
    load_from_db();
}

void NewsMonitorService::load_from_db() {
    monitors_.clear();
    auto result = fincept::NewsMonitorRepository::instance().list_all();
    if (result.is_err())
        return;
    for (const auto& row : result.value()) {
        NewsMonitor m;
        m.id = row.id;
        m.label = row.label;
        m.keywords = row.keywords;
        m.color = row.color;
        m.enabled = row.enabled;
        monitors_.append(m);
    }
}

void NewsMonitorService::save_to_db(const NewsMonitor& m) {
    fincept::NewsMonitorRow row{m.id, m.label, m.keywords, m.color, m.enabled};
    fincept::NewsMonitorRepository::instance().upsert(row);
}

void NewsMonitorService::remove_from_db(const QString& id) {
    fincept::NewsMonitorRepository::instance().remove(id);
}

QVector<NewsMonitor> NewsMonitorService::get_monitors() const {
    return monitors_;
}

void NewsMonitorService::add_monitor(const QString& label, const QStringList& keywords, const QString& color) {
    NewsMonitor m;
    m.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    m.label = label;
    m.keywords = keywords;
    m.color = color.isEmpty() ? monitor_colors()[monitors_.size() % monitor_colors().size()] : color;
    m.enabled = true;
    monitors_.append(m);
    save_to_db(m);
}

void NewsMonitorService::update_monitor(const NewsMonitor& monitor) {
    for (auto& m : monitors_) {
        if (m.id == monitor.id) {
            m = monitor;
            save_to_db(m);
            return;
        }
    }
}

void NewsMonitorService::delete_monitor(const QString& id) {
    monitors_.erase(
        std::remove_if(monitors_.begin(), monitors_.end(), [&](const NewsMonitor& m) { return m.id == id; }),
        monitors_.end());
    remove_from_db(id);
}

void NewsMonitorService::toggle_monitor(const QString& id) {
    for (auto& m : monitors_) {
        if (m.id == id) {
            m.enabled = !m.enabled;
            save_to_db(m);
            return;
        }
    }
}

QMap<QString, QVector<NewsArticle>> NewsMonitorService::scan_monitors(const QVector<NewsMonitor>& monitors,
                                                                      const QVector<NewsArticle>& articles) const {
    QMap<QString, QVector<NewsArticle>> result;

    for (const auto& mon : monitors) {
        if (!mon.enabled || mon.keywords.isEmpty())
            continue;

        // Build regex from keywords. Look-arounds instead of \b so a keyword that
        // ends in punctuation ("OPEC+", "U.S.") can match; and a blank keyword is
        // skipped — it used to compile to \b\b, which matches EVERY article.
        QStringList escaped;
        for (const auto& kw : mon.keywords) {
            const QString trimmed = kw.trimmed();
            if (trimmed.isEmpty())
                continue;
            escaped << ("(?<!\\w)" + QRegularExpression::escape(trimmed) + "(?!\\w)");
        }
        if (escaped.isEmpty())
            continue;
        QRegularExpression re(escaped.join("|"), QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid())
            continue;

        QVector<NewsArticle> matches;
        for (const auto& a : articles) {
            QString text = a.headline + " " + a.summary;
            if (re.match(text).hasMatch())
                matches.append(a);
        }
        if (!matches.isEmpty())
            result[mon.id] = matches;
    }

    return result;
}

} // namespace fincept::services
