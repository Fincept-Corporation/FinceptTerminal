// UiObserve.cpp — the observing core tools: ui_overview, ui_snapshot, ui_read,
// ui_wait, ui_screenshot. Nothing here changes terminal state. See UiEngine.h.

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "core/layout/WorkspaceShell.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolRef.h"
#include "mcp/ResultStore.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"

#include <QAbstractBarSeries>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QBarCategoryAxis>
#include <QBarSet>
#include <QBuffer>
#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QDialog>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPieSeries>
#include <QPieSlice>
#include <QPixmap>
#include <QTabBar>
#include <QTreeView>
#include <QUrl>
#include <QXYSeries>

#include <DockManager.h>
#include <DockWidget.h>
#include <algorithm>
#include <memory>

namespace fincept::mcp::core::ui {

namespace ui_observe_detail {

// Page sizes. Small on purpose: the first page answers most questions, and
// every answer says how to get the next one — never a wall of raw data.
constexpr int kTextPageChars = 3000; // default characters per text page (limit overrides, ≤ 20000)
constexpr int kMaxTextPageChars = 20000;
constexpr int kDefaultRows = 20; // rows / points / items per page (limit overrides, ≤ 500)
constexpr int kMaxRows = 500;
constexpr int kReadCols = 10;   // columns per table page (col_offset / columns for others)
constexpr int kSeriesShown = 6; // chart series listed before "query=<name>" is needed

Detail parse_detail(const QJsonObject& args) {
    const QString d = args.value(QStringLiteral("detail")).toString().toLower();
    if (d == QLatin1String("brief"))
        return Detail::Brief;
    if (d == QLatin1String("full"))
        return Detail::Full;
    return Detail::Normal;
}

QString panel_line(const PanelInfo& p) {
    QString line =
        QStringLiteral("%1 %2 (%3)")
            .arg(Refs::instance().ref_for(p.dock.data(), RefKind::Panel), common::quoted(p.title), p.screen_id);
    QStringList states;
    if (p.focused)
        states << QStringLiteral("focused");
    if (!p.visible)
        states << QStringLiteral("background tab");
    if (p.dock) {
        if (auto* linked = dynamic_cast<IGroupLinked*>(screen_widget_of(p.dock))) {
            const SymbolRef ref = linked->current_symbol();
            if (!ref.symbol.isEmpty())
                states << QStringLiteral("symbol %1").arg(ref.symbol);
        }
    }
    if (!states.isEmpty())
        line += QStringLiteral(" [") + states.join(QStringLiteral(", ")) + QLatin1Char(']');
    return line;
}

QString dialog_line(QWidget* d) {
    QString line = QStringLiteral("dialog %1 %2")
                       .arg(Refs::instance().ref_for(d, RefKind::Dialog), common::quoted(window_title_of(d)));
    if (d == QApplication::activeModalWidget())
        line += QStringLiteral(" [modal — answer it before anything else]");
    if (auto* mb = qobject_cast<QMessageBox*>(d)) {
        const QString text = plain_text(mb->text()).simplified();
        if (!text.isEmpty())
            line += QStringLiteral(": ") + common::clip(text, 220);
    }
    return line;
}

/// Window chrome + panels + dialogs of a frame as snapshot lines.
void snapshot_frame(WindowFrame* f, const WalkOptions& base, QStringList& lines, bool* truncated) {
    int budget = base.max_nodes;
    WalkOptions chrome = base;
    if (f->dock_manager())
        chrome.exclude.insert(f->dock_manager());
    chrome.max_nodes = std::max(10, std::min(60, budget / 4));
    bool t = false;
    const QList<Node> chrome_nodes = collect_nodes(f, chrome, &t);
    lines << QStringLiteral("window %1 %2")
                 .arg(Refs::instance().ref_for(f, RefKind::Window), common::quoted(window_title_of(f)));
    for (const Node& n : chrome_nodes) {
        Node copy = n;
        copy.depth += 1;
        lines += format_node(copy, base.detail);
    }
    budget -= static_cast<int>(chrome_nodes.size());
    *truncated = *truncated || t;

    for (const PanelInfo& p : panels_of(f)) {
        lines << panel_line(p);
        if (!p.visible || !p.dock)
            continue; // a background tab: ui_workspace focus or a snapshot of its ref shows it
        if (budget <= 0) {
            *truncated = true;
            continue;
        }
        WalkOptions po = base;
        po.max_nodes = budget;
        po.base_depth = 1;
        bool pt = false;
        const QList<Node> nodes = collect_nodes(p.dock, po, &pt);
        for (const Node& n : nodes)
            lines += format_node(n, base.detail);
        budget -= static_cast<int>(nodes.size());
        *truncated = *truncated || pt;
    }
}

void snapshot_top(QWidget* top, const QString& header, const WalkOptions& opt, QStringList& lines, bool* truncated,
                  int offset = 0) {
    lines << header;
    WalkOptions o = opt;
    o.base_depth = 1;
    o.max_nodes = opt.max_nodes + std::max(0, offset);
    bool t = false;
    const QList<Node> nodes = collect_nodes(top, o, &t);
    if (offset > 0)
        lines << QStringLiteral("  (elements from #%1)").arg(offset);
    for (int i = std::max(0, offset); i < nodes.size(); ++i)
        lines += format_node(nodes[i], opt.detail);
    if (auto* mb = qobject_cast<QMessageBox*>(top)) {
        const QString info = plain_text(mb->informativeText()).simplified();
        if (!info.isEmpty())
            lines.insert(1, QStringLiteral("  text ") + common::quoted(common::clip(info, 400)));
    }
    *truncated = *truncated || t;
}

/// "showing 0–19 of 250 · next: ui_read(ref="e12", offset=20)" — every paged
/// answer ends with where it stands and how to go on.
QString page_footer(const QString& what, int first, int count, int total, bool total_is_floor,
                    const QString& next_call) {
    if (count <= 0)
        return {};
    QString line = QStringLiteral("showing %1 %2–%3 of %4%5")
                       .arg(what)
                       .arg(first)
                       .arg(first + count - 1)
                       .arg(total)
                       .arg(total_is_floor ? QStringLiteral("+") : QString());
    if (!next_call.isEmpty())
        line += QStringLiteral(" · next: ") + next_call;
    return line;
}

QString json_outline(const QJsonValue& v, int depth = 0) {
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        if (depth >= 2)
            return QStringLiteral("{%1 keys}").arg(o.size());
        QStringList parts;
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (parts.size() >= 12) {
                parts << QStringLiteral("+%1 more").arg(o.size() - 12);
                break;
            }
            const QJsonValue c = it.value();
            parts << ((c.isObject() || c.isArray()) ? it.key() + QStringLiteral(": ") + json_outline(c, depth + 1)
                                                    : it.key());
        }
        return QLatin1Char('{') + parts.join(QStringLiteral(", ")) + QLatin1Char('}');
    }
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        if (a.isEmpty())
            return QStringLiteral("array[0]");
        const QJsonValue f = a.at(0);
        return QStringLiteral("array[%1] of %2")
            .arg(a.size())
            .arg(f.isObject() || f.isArray() ? json_outline(f, depth + 1)
                 : f.isString()              ? QStringLiteral("strings")
                 : f.isDouble()              ? QStringLiteral("numbers")
                                             : QStringLiteral("values"));
    }
    if (v.isString())
        return QStringLiteral("text (%1 chars)").arg(v.toString().size());
    return QStringLiteral("value");
}

QString json_compact(const QJsonValue& v) {
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isString())
        return common::quoted(v.toString());
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    return QStringLiteral("null");
}

QString fmt_number(double v) {
    return QString::number(v, 'g', 10);
}

QString fmt_x(double x, bool datetime_axis) {
    if (datetime_axis)
        return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(x)).toString(Qt::ISODate);
    return fmt_number(x);
}

bool chart_has_datetime_x(QChart* chart, QAbstractSeries* s) {
    for (QAbstractAxis* a : s->attachedAxes()) {
        if (a->orientation() == Qt::Horizontal && qobject_cast<QDateTimeAxis*>(a))
            return true;
    }
    Q_UNUSED(chart);
    return false;
}

QString read_chart(QChartView* view, const QString& ref, int offset, int limit, const QStringList& series_words) {
    QChart* chart = view->chart();
    if (!chart)
        return QStringLiteral("%1 chart has no data.").arg(ref);
    QStringList lines;
    const auto series = chart->series();
    lines << QStringLiteral("%1 chart %2 · %3 series")
                 .arg(ref, common::quoted(plain_text(chart->title()).simplified()))
                 .arg(series.size());
    int listed = 0;
    QStringList skipped;
    auto more = [&](int end, int total) {
        if (end < total)
            lines << QStringLiteral("  … %1 more — ui_read(ref=\"%2\", offset=%3%4)")
                         .arg(total - end)
                         .arg(ref)
                         .arg(end)
                         .arg(series_words.isEmpty()
                                  ? QString()
                                  : QStringLiteral(", query=\"%1\"").arg(series_words.join(QLatin1Char(' '))));
    };
    for (QAbstractSeries* s : series) {
        const QString name = s->name().trimmed().isEmpty() ? QStringLiteral("(unnamed)") : s->name().trimmed();
        if (!series_words.isEmpty() && !words_match(name.toLower(), series_words))
            continue;
        if (series_words.isEmpty() && listed >= kSeriesShown) {
            skipped << common::quoted(name);
            continue;
        }
        ++listed;
        if (auto* xy = qobject_cast<QXYSeries*>(s)) {
            const bool dt = chart_has_datetime_x(chart, s);
            const auto pts = xy->points();
            QString head = QStringLiteral("series %1 · %2 points").arg(common::quoted(name)).arg(pts.size());
            if (!pts.isEmpty()) {
                double lo = pts[0].y(), hi = pts[0].y();
                for (const QPointF& pt : pts) {
                    lo = std::min(lo, pt.y());
                    hi = std::max(hi, pt.y());
                }
                head += QStringLiteral(" · x %1 → %2 · y min %3, max %4, last %5")
                            .arg(fmt_x(pts.first().x(), dt), fmt_x(pts.last().x(), dt), fmt_number(lo), fmt_number(hi),
                                 fmt_number(pts.last().y()));
            }
            lines << head << QStringLiteral("  x | y");
            const int end = std::min(static_cast<int>(pts.size()), offset + limit);
            for (int i = std::max(0, offset); i < end; ++i)
                lines << QStringLiteral("  %1 | %2").arg(fmt_x(pts[i].x(), dt), fmt_number(pts[i].y()));
            more(end, static_cast<int>(pts.size()));
        } else if (auto* cs = qobject_cast<QCandlestickSeries*>(s)) {
            const auto sets = cs->sets();
            QString head = QStringLiteral("series %1 · %2 candles").arg(common::quoted(name)).arg(sets.size());
            if (!sets.isEmpty()) {
                QCandlestickSet* last = sets.last();
                head += QStringLiteral(" · %1 → %2 · last O %3 H %4 L %5 C %6")
                            .arg(fmt_x(sets.first()->timestamp(), true), fmt_x(last->timestamp(), true),
                                 fmt_number(last->open()), fmt_number(last->high()), fmt_number(last->low()),
                                 fmt_number(last->close()));
            }
            lines << head << QStringLiteral("  time | open | high | low | close");
            const int end = std::min(static_cast<int>(sets.size()), offset + limit);
            for (int i = std::max(0, offset); i < end; ++i) {
                QCandlestickSet* c = sets[i];
                lines << QStringLiteral("  %1 | %2 | %3 | %4 | %5")
                             .arg(fmt_x(c->timestamp(), true), fmt_number(c->open()), fmt_number(c->high()),
                                  fmt_number(c->low()), fmt_number(c->close()));
            }
            more(end, static_cast<int>(sets.size()));
        } else if (auto* bs = qobject_cast<QAbstractBarSeries*>(s)) {
            QStringList categories;
            for (QAbstractAxis* a : s->attachedAxes()) {
                if (auto* cat = qobject_cast<QBarCategoryAxis*>(a))
                    categories = cat->categories();
            }
            lines << QStringLiteral("series %1 · bars · %2 sets").arg(common::quoted(name)).arg(bs->count());
            int longest = 0;
            for (QBarSet* set : bs->barSets()) {
                QStringList vals;
                const int end = std::min(static_cast<int>(set->count()), offset + limit);
                longest = std::max(longest, static_cast<int>(set->count()));
                for (int i = std::max(0, offset); i < end; ++i)
                    vals << (categories.value(i).isEmpty()
                                 ? fmt_number(set->at(i))
                                 : categories.value(i) + QLatin1Char('=') + fmt_number(set->at(i)));
                lines << QStringLiteral("  %1: %2").arg(set->label(), vals.join(QStringLiteral(", ")));
            }
            more(std::min(longest, offset + limit), longest);
        } else if (auto* ps = qobject_cast<QPieSeries*>(s)) {
            const auto slices = ps->slices();
            lines << QStringLiteral("series %1 · pie · %2 slices").arg(common::quoted(name)).arg(slices.size());
            const int end = std::min(static_cast<int>(slices.size()), offset + limit);
            for (int i = std::max(0, offset); i < end; ++i)
                lines << QStringLiteral("  %1 | %2 | %3%")
                             .arg(slices[i]->label(), fmt_number(slices[i]->value()))
                             .arg(QString::number(slices[i]->percentage() * 100.0, 'f', 1));
            more(end, static_cast<int>(slices.size()));
        } else {
            lines << QStringLiteral("series %1 (type %2) — not readable as data; ui_screenshot(target=\"%3\") shows it")
                         .arg(common::quoted(name))
                         .arg(static_cast<int>(s->type()))
                         .arg(ref);
        }
    }
    if (!skipped.isEmpty())
        lines << QStringLiteral("… %1 more series: %2 — ui_read(ref=\"%3\", query=\"<series name>\") reads one")
                     .arg(skipped.size())
                     .arg(common::clip(skipped.join(QStringLiteral(", ")), 400))
                     .arg(ref);
    if (series.isEmpty())
        lines << QStringLiteral("(no series)");
    else if (listed == 0 && skipped.isEmpty())
        lines << QStringLiteral("(no series named \"%1\")").arg(series_words.join(QLatin1Char(' ')));
    return lines.join(QLatin1Char('\n'));
}

QString read_view(QAbstractItemView* view, const QString& ref, const QJsonObject& args) {
    const int offset = clamp_int(args.value(QStringLiteral("offset")), 0, 0, 100000000);
    const int limit = clamp_int(args.value(QStringLiteral("limit")), kDefaultRows, 1, kMaxRows);
    const QStringList words = query_words(args.value(QStringLiteral("query")).toString());
    const QList<int> all_cols = visible_columns(view);
    const QStringList all_headers = column_headers(view, all_cols);
    const QString base_ref = ref.section(QLatin1Char(':'), 0, 0);

    // Columns: by header (exact, then contained) or by number; else a window.
    QList<int> cols;
    QStringList headers;
    QStringList unknown;
    const QJsonArray wanted = args.value(QStringLiteral("columns")).toArray();
    for (const QJsonValue& w : wanted) {
        int at = -1;
        if (w.isDouble()) {
            at = w.toInt(-1);
        } else {
            const QString want = w.toString().trimmed().toLower();
            for (int i = 0; i < all_headers.size() && at < 0; ++i) {
                if (all_headers[i].toLower() == want)
                    at = i;
            }
            for (int i = 0; i < all_headers.size() && at < 0 && !want.isEmpty(); ++i) {
                if (all_headers[i].toLower().contains(want))
                    at = i;
            }
        }
        if (at >= 0 && at < all_cols.size()) {
            if (!cols.contains(all_cols[at])) {
                cols << all_cols[at];
                headers << all_headers[at];
            }
        } else {
            unknown << (w.isDouble() ? QString::number(w.toInt()) : w.toString());
        }
    }
    int col_offset = 0;
    if (cols.isEmpty()) {
        col_offset = clamp_int(args.value(QStringLiteral("col_offset")), 0, 0,
                               std::max(0, static_cast<int>(all_cols.size()) - 1));
        cols = all_cols.mid(col_offset, kReadCols);
        headers = all_headers.mid(col_offset, kReadCols);
    }

    bool capped = false;
    const int total = view_row_count(view, &capped);
    QStringList lines;
    QString name = view->accessibleName().trimmed();
    if (auto node = describe_widget(view, Detail::Brief); node && !node->name.isEmpty())
        name = node->name;
    QString head = QStringLiteral("%1 %2%3 · %4%5 rows")
                       .arg(base_ref,
                            view->inherits("QTreeView")    ? QStringLiteral("tree")
                            : view->inherits("QTableView") ? QStringLiteral("table")
                                                           : QStringLiteral("list"),
                            name.isEmpty() ? QString() : QLatin1Char(' ') + common::quoted(name))
                       .arg(total)
                       .arg(capped ? QStringLiteral("+") : QString());
    if (const int hidden = view_hidden_rows(view); hidden > 0)
        head += QStringLiteral(" (+%1 hidden by a filter)").arg(hidden);
    if (const QString sort = view_sort_state(view); !sort.isEmpty())
        head += QStringLiteral(" · ") + sort;
    if (QItemSelectionModel* sel = view->selectionModel()) {
        const QModelIndex cur = sel->currentIndex();
        if (cur.isValid() && sel->isSelected(cur)) {
            const int row = row_of_index(view, cur);
            if (row >= 0)
                head += QStringLiteral(" · selected %1:r%2").arg(base_ref).arg(row);
        }
    }
    lines << head;
    if (cols.size() > 1 || !headers.value(0).isEmpty()) {
        QString cl = QStringLiteral("row | ") + headers.join(QStringLiteral(" | "));
        if (wanted.isEmpty() && all_cols.size() > cols.size())
            cl += QStringLiteral("   (columns %1–%2 of %3; others: col_offset=%4, or columns=[\"<header>\", …])")
                      .arg(col_offset + 1)
                      .arg(col_offset + cols.size())
                      .arg(all_cols.size())
                      .arg(col_offset + cols.size() < all_cols.size() ? col_offset + cols.size() : 0);
        lines << cl;
    }
    if (!unknown.isEmpty())
        lines << QStringLiteral("(no column %1 — columns: %2)")
                     .arg(unknown.join(QStringLiteral(", ")),
                          common::clip(all_headers.join(QStringLiteral(" | ")), 600));

    int emitted = 0;
    int matched = 0;
    int scanned = 0;
    bool scan_capped = false;
    for_each_view_row(view, 0, [&](int r, const QModelIndex& first) {
        if (!words.isEmpty()) {
            if (++scanned > kMaxRowScan) {
                scan_capped = true;
                return false;
            }
            // A query matches against every column, not only the ones shown.
            if (!words_match(index_row_text(view, first, all_cols, 80).toLower(), words))
                return true;
        }
        const int ordinal = matched++;
        if (ordinal < offset)
            return true;
        if (emitted >= limit)
            return !words.isEmpty(); // keep counting matches; plain rows: the total is known
        lines << QStringLiteral("%1:r%2 | %3")
                     .arg(base_ref)
                     .arg(r)
                     .arg(common::clip(index_row_text(view, first, cols, 60, true), 700));
        Refs::instance().anchor_row(view, base_ref, r, first);
        ++emitted;
        return true;
    });
    const int found = words.isEmpty() ? total : matched;
    if (emitted == 0) {
        lines << (words.isEmpty() ? QStringLiteral("(no rows at offset %1 — the list has %2)").arg(offset).arg(total)
                                  : QStringLiteral("(no rows hold \"%1\"%2)")
                                        .arg(words.join(QLatin1Char(' ')),
                                             offset > 0 ? QStringLiteral(" past offset %1").arg(offset) : QString()));
    } else {
        QString next;
        if (offset + emitted < found)
            next = QStringLiteral("ui_read(ref=\"%1\", offset=%2%3%4)")
                       .arg(base_ref)
                       .arg(offset + emitted)
                       .arg(words.isEmpty() ? QString()
                                            : QStringLiteral(", query=\"%1\"").arg(words.join(QLatin1Char(' '))))
                       .arg(col_offset > 0 ? QStringLiteral(", col_offset=%1").arg(col_offset) : QString());
        lines << page_footer(words.isEmpty() ? QStringLiteral("rows") : QStringLiteral("matching rows"), offset,
                             emitted, found, capped || scan_capped, next);
    }
    if (scan_capped)
        lines << QStringLiteral("(searched the first %1 rows only)").arg(kMaxRowScan);
    lines << QStringLiteral(
                 "Act on a row: ui_act(action=\"click\", ref=\"%1:r<N>\"[, column=\"<header>\"]); controls in "
                 "rows show as [e… button \"…\"]. ui_read(ref=\"%1:r<N>\") gives one row in full.")
                 .arg(base_ref);
    return lines.join(QLatin1Char('\n'));
}

/// `more_args` is put into the "more" call after the ref (e.g. `, path="text"`).
QString page_text(const QString& label, const QString& text, int offset, const QString& ref,
                  int page_chars = kTextPageChars, const QString& more_args = {}) {
    const int start = std::clamp(offset, 0, static_cast<int>(text.size()));
    const QString page = text.mid(start, page_chars);
    QString out = QStringLiteral("%1 · %2 characters · showing %3–%4\n%5")
                      .arg(label)
                      .arg(text.size())
                      .arg(start)
                      .arg(start + page.size())
                      .arg(page);
    if (start + page.size() < text.size())
        out +=
            QStringLiteral("\n… more: ui_read(ref=\"%1\"%2, offset=%3)").arg(ref, more_args).arg(start + page.size());
    return out;
}

QString read_result(const QString& id, const QJsonObject& args) {
    const auto raw = ResultStore::instance().raw(id);
    if (!raw)
        return {};
    const int offset = clamp_int(args.value(QStringLiteral("offset")), 0, 0, 100000000);
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray("[") + *raw + QByteArray("]"));
    if (!doc.isArray() || doc.array().size() != 1)
        return page_text(QStringLiteral("stored result %1").arg(id), QString::fromUtf8(*raw), offset, id,
                         clamp_int(args.value(QStringLiteral("limit")), kTextPageChars, 200, kMaxTextPageChars));
    QJsonValue v = doc.array().at(0);

    // path: "data", "data.rows", "data.rows.3", "items[2].name"
    const QString path = args.value(QStringLiteral("path")).toString().trimmed();
    QString walked;
    if (!path.isEmpty()) {
        QString norm = path;
        norm.replace(QLatin1Char('['), QLatin1Char('.')).remove(QLatin1Char(']'));
        for (const QString& seg : norm.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
            bool is_num = false;
            const int at = seg.toInt(&is_num);
            if (v.isArray() && is_num && at >= 0 && at < v.toArray().size()) {
                v = v.toArray().at(at);
            } else if (v.isObject() && v.toObject().contains(seg)) {
                v = v.toObject().value(seg);
            } else {
                const QString here = walked.isEmpty() ? QStringLiteral("the top") : walked;
                return QStringLiteral("stored result %1: no \"%2\" at %3, which is %4.")
                    .arg(id, seg, here, json_outline(v));
            }
            walked += (walked.isEmpty() ? QString() : QStringLiteral(".")) + seg;
        }
    }
    const QString where = walked.isEmpty() ? QString() : QStringLiteral(" · path %1").arg(walked);
    const QString path_arg = walked.isEmpty() ? QString() : QStringLiteral(", path=\"%1\"").arg(walked);

    if (v.isString())
        return page_text(QStringLiteral("stored result %1%2").arg(id, where), v.toString(), offset, id,
                         clamp_int(args.value(QStringLiteral("limit")), kTextPageChars, 200, kMaxTextPageChars));
    const int limit = clamp_int(args.value(QStringLiteral("limit")), kDefaultRows, 1, kMaxRows);
    QStringList lines;
    lines << QStringLiteral("stored result %1%2 · %3").arg(id, where, json_outline(v));
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        const int end = std::min(static_cast<int>(a.size()), offset + limit);
        for (int i = std::max(0, offset); i < end; ++i)
            lines << QStringLiteral("[%1] %2").arg(i).arg(common::clip(json_compact(a.at(i)), 400));
        const QString next =
            end < a.size() ? QStringLiteral("ui_read(ref=\"%1\"%2, offset=%3)").arg(id, path_arg).arg(end) : QString();
        if (end > offset)
            lines << page_footer(QStringLiteral("items"), offset, end - offset, static_cast<int>(a.size()), false,
                                 next);
        lines << QStringLiteral("An item in full: path=\"%1%2<N>\".")
                     .arg(walked, walked.isEmpty() ? QString() : QStringLiteral("."));
    } else if (v.isObject()) {
        const QJsonObject o = v.toObject();
        const QStringList keys = o.keys();
        const int end = std::min(static_cast<int>(keys.size()), offset + limit);
        for (int i = std::max(0, offset); i < end; ++i) {
            const QJsonValue c = o.value(keys[i]);
            lines << QStringLiteral("%1: %2").arg(
                keys[i], (c.isObject() || c.isArray()) ? json_outline(c) : common::clip(json_compact(c), 200));
        }
        const QString next = end < keys.size()
                                 ? QStringLiteral("ui_read(ref=\"%1\"%2, offset=%3)").arg(id, path_arg).arg(end)
                                 : QString();
        if (end > offset)
            lines << page_footer(QStringLiteral("keys"), offset, end - offset, static_cast<int>(keys.size()), false,
                                 next);
        lines << QStringLiteral("Go deeper: path=\"%1%2<key>\".")
                     .arg(walked, walked.isEmpty() ? QString() : QStringLiteral("."));
    } else {
        lines << json_compact(v);
    }
    return lines.join(QLatin1Char('\n'));
}

/// One line saying what an element is and what state it is in — the "details"
/// a person sees: role, name, value, states, where it is, its tooltip.
QString element_header(const QString& ref, QWidget* w) {
    QString line = ref;
    if (auto n = describe_widget(w, Detail::Full)) {
        line += QLatin1Char(' ') + n->role;
        if (!n->name.isEmpty())
            line += QLatin1Char(' ') + common::quoted(common::clip(n->name, 200));
        if (!n->value.isEmpty())
            line += QStringLiteral(" = ") + common::quoted(common::clip(n->value, 200));
        QStringList states = n->states;
        if (w->hasFocus())
            states << QStringLiteral("focused");
        if (!w->isVisible())
            states << QStringLiteral("not visible");
        if (!states.isEmpty())
            line += QStringLiteral(" [") + states.join(QStringLiteral(", ")) + QLatin1Char(']');
    } else {
        line += QStringLiteral(" element");
        if (!w->isEnabled())
            line += QStringLiteral(" [disabled]");
    }
    line += QStringLiteral(" · in ") + location_label(w);
    const QString tip = plain_text(w->toolTip()).simplified();
    if (!tip.isEmpty())
        line += QStringLiteral(" · tooltip ") + common::quoted(common::clip(tip, 160));
    return line;
}

QString panel_state(ads::CDockWidget* dock) {
    QStringList lines;
    WindowFrame* f = frame_of(dock);
    PanelInfo info;
    for (const PanelInfo& p : panels_of(f)) {
        if (p.dock == dock)
            info = p;
    }
    if (!info.dock) {
        info.dock = dock;
        info.screen_id = dock->objectName();
        info.title = dock->windowTitle();
    }
    lines << panel_line(info);
    if (auto* linked = dynamic_cast<IGroupLinked*>(screen_widget_of(dock))) {
        const SymbolRef ref = linked->current_symbol();
        lines << QStringLiteral("symbol: %1%2%3")
                     .arg(ref.symbol.isEmpty() ? QStringLiteral("(none)") : ref.symbol,
                          ref.asset_class.isEmpty() ? QString() : QStringLiteral(" · ") + ref.asset_class,
                          ref.exchange.isEmpty() ? QString() : QStringLiteral(" · ") + ref.exchange);
    }
    WalkOptions opt;
    opt.detail = Detail::Brief;
    opt.max_nodes = 150;
    opt.base_depth = 1;
    bool truncated = false;
    for (const Node& n : collect_nodes(dock, opt, &truncated))
        lines += format_node(n, Detail::Brief);
    if (truncated)
        lines << QStringLiteral("  … more controls — ui_snapshot(target=\"%1\", query=…)")
                     .arg(Refs::instance().ref_for(dock, RefKind::Panel));
    lines << QStringLiteral("Terminal commands (panels, layouts, windows, lock …): ui_invoke(action=\"list\").");
    return lines.join(QLatin1Char('\n'));
}

// ── Waits ────────────────────────────────────────────────────────────────────

QString scope_text(QWidget* scope) {
    QStringList parts;
    QList<QWidget*> roots;
    if (scope)
        roots << scope;
    for (QWidget* d : open_dialogs()) {
        if (!roots.contains(d))
            roots << d;
    }
    if (QWidget* p = open_popup())
        roots << p;
    for (QWidget* root : roots) {
        parts << root->windowTitle();
        QList<QWidget*> all = root->findChildren<QWidget*>();
        all.prepend(root);
        for (QWidget* w : all) {
            if (!w->isVisible() || is_hidden_from_agents(w))
                continue;
            if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
                const QList<int> cols = visible_columns(view);
                const int n = std::min(view_row_count(view), 400);
                for (int r = 0; r < n; ++r)
                    parts << view_row_text(view, r, cols, 200);
                continue;
            }
            if (auto* bar = qobject_cast<QTabBar*>(w)) {
                for (int i = 0; i < bar->count(); ++i)
                    parts << bar->tabText(i);
                continue;
            }
            if (auto* menu = qobject_cast<QMenu*>(w)) {
                for (QAction* a : menu->actions())
                    parts << a->text();
                continue;
            }
            const QString t = widget_text(w);
            if (!t.isEmpty())
                parts << t;
        }
    }
    return parts.join(QLatin1Char('\n')).toLower();
}

std::optional<QString> state_holds(const QString& ref, const QString& state, const QString& value) {
    // Returns nullopt when the condition holds, else a description of what is there now.
    auto r = Refs::instance().resolve(ref);
    QWidget* w = r ? (r->widget ? r->widget.data() : nullptr) : nullptr;
    if (state == QLatin1String("hidden")) {
        if (!w || !w->isVisible())
            return std::nullopt;
        return QStringLiteral("still visible");
    }
    if (!r || (!w && !r->action))
        return QStringLiteral("%1 is gone").arg(ref);
    if (state == QLatin1String("visible"))
        return w && w->isVisible() ? std::nullopt : std::optional<QString>(QStringLiteral("not visible"));
    if (r->action) {
        QAction* a = r->action;
        if (state == QLatin1String("enabled"))
            return a->isEnabled() ? std::nullopt : std::optional<QString>(QStringLiteral("disabled"));
        if (state == QLatin1String("disabled"))
            return !a->isEnabled() ? std::nullopt : std::optional<QString>(QStringLiteral("enabled"));
        if (state == QLatin1String("checked"))
            return a->isChecked() ? std::nullopt : std::optional<QString>(QStringLiteral("unchecked"));
        if (state == QLatin1String("unchecked"))
            return !a->isChecked() ? std::nullopt : std::optional<QString>(QStringLiteral("checked"));
    }
    if (state == QLatin1String("enabled"))
        return w->isEnabled() ? std::nullopt : std::optional<QString>(QStringLiteral("disabled"));
    if (state == QLatin1String("disabled"))
        return !w->isEnabled() ? std::nullopt : std::optional<QString>(QStringLiteral("enabled"));
    if (state == QLatin1String("checked") || state == QLatin1String("unchecked")) {
        const bool want = state == QLatin1String("checked");
        bool checked = false;
        if (auto* b = qobject_cast<QAbstractButton*>(w))
            checked = b->isChecked();
        else if (auto* bar = qobject_cast<QTabBar*>(w))
            checked = bar->currentIndex() == r->sub;
        return checked == want
                   ? std::nullopt
                   : std::optional<QString>(checked ? QStringLiteral("checked") : QStringLiteral("unchecked"));
    }
    if (state == QLatin1String("value")) {
        const QString now = widget_text(w).trimmed();
        if (now.compare(value.trimmed(), Qt::CaseInsensitive) == 0 ||
            (!value.trimmed().isEmpty() && now.contains(value.trimmed(), Qt::CaseInsensitive)))
            return std::nullopt;
        return QStringLiteral("value is %1").arg(common::quoted(common::clip(now, 80)));
    }
    return QStringLiteral("unknown state \"%1\"").arg(state);
}

// ── Web pages ────────────────────────────────────────────────────────────────

constexpr int kWebAnswerMs = 5000;      // a page that has not answered by then is loading or hung
constexpr int kWebOverviewChars = 1500; // page text shown before path="text" is needed
constexpr int kWebMatchLinesShown = 10; // query: matching text lines shown in the overview

/// "e12:w3 button "MACD" = "…" [checked]" — one control of a page.
QString web_control_line(const QString& base, const QJsonObject& c) {
    QString line = QStringLiteral("%1:w%2 %3")
                       .arg(base)
                       .arg(c.value(QStringLiteral("i")).toInt())
                       .arg(c.value(QStringLiteral("r")).toString());
    const QString name = c.value(QStringLiteral("n")).toString();
    if (!name.isEmpty())
        line += QLatin1Char(' ') + common::quoted(name);
    const QString value = c.value(QStringLiteral("v")).toString();
    if (!value.isEmpty())
        line += QStringLiteral(" = ") + common::quoted(value);
    QStringList states;
    for (const QJsonValue& s : c.value(QStringLiteral("s")).toArray())
        states << s.toString();
    if (!states.isEmpty())
        line += QStringLiteral(" [") + states.join(QStringLiteral(", ")) + QLatin1Char(']');
    return line;
}

bool web_control_hidden(const QJsonObject& c) {
    return c.value(QStringLiteral("s")).toArray().contains(QJsonValue(QStringLiteral("hidden")));
}

/// Where the page comes from, without its query (it can carry tokens).
QString web_url_label(const QString& raw) {
    const QUrl u(raw);
    if (!u.isValid() || raw.isEmpty())
        return QStringLiteral("(no address)");
    if (u.isLocalFile())
        return u.fileName();
    if (u.scheme() == QLatin1String("data") || u.scheme() == QLatin1String("blob"))
        return u.scheme() + QStringLiteral(": page");
    return u.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo).toString();
}

QString web_page_line(const QJsonObject& page) {
    QString line = QStringLiteral("web page %1 · %2 · %3×%4")
                       .arg(common::quoted(common::clip(page.value(QStringLiteral("title")).toString(), 120)),
                            web_url_label(page.value(QStringLiteral("url")).toString()))
                       .arg(page.value(QStringLiteral("vw")).toInt())
                       .arg(page.value(QStringLiteral("vh")).toInt());
    const int sh = page.value(QStringLiteral("sh")).toInt();
    const int vh = page.value(QStringLiteral("vh")).toInt();
    if (sh > vh + 4)
        line += QStringLiteral(" · scrolled to %1 of %2 px").arg(page.value(QStringLiteral("sy")).toInt()).arg(sh - vh);
    if (page.value(QStringLiteral("loading")).toBool())
        line += QStringLiteral(" · still loading");
    return line;
}

/// The page's controls: on-screen ones (overview) or all of them, paged.
/// Every control listed gets its "<base>:w<N>" ref anchored.
QStringList web_controls(const QString& base, const QJsonArray& controls, const QStringList& words, bool overview,
                         int offset, int limit) {
    QList<QJsonObject> picked;
    int hidden = 0;
    for (const QJsonValue& v : controls) {
        const QJsonObject c = v.toObject();
        if (overview && web_control_hidden(c)) {
            ++hidden;
            continue;
        }
        if (!words.isEmpty() && !words_match((c.value(QStringLiteral("r")).toString() + QLatin1Char(' ') +
                                              c.value(QStringLiteral("n")).toString() + QLatin1Char(' ') +
                                              c.value(QStringLiteral("v")).toString())
                                                 .toLower(),
                                             words))
            continue;
        picked << c;
    }
    QStringList lines;
    if (picked.isEmpty()) {
        lines << (words.isEmpty() ? QStringLiteral("controls: none%1")
                                        .arg(hidden ? QStringLiteral(" on screen (%1 hidden — path=\"controls\" lists "
                                                                     "them)")
                                                          .arg(hidden)
                                                    : QString())
                                  : QStringLiteral("controls: none match \"%1\"").arg(words.join(QLatin1Char(' '))));
        return lines;
    }
    const int start = std::clamp(offset, 0, static_cast<int>(picked.size()));
    const int end = std::min(static_cast<int>(picked.size()), start + limit);
    QString head =
        QStringLiteral("controls · %1%2").arg(picked.size()).arg(overview ? QStringLiteral(" on screen") : QString());
    if (hidden)
        head += QStringLiteral(" (+%1 hidden, e.g. in closed menus — path=\"controls\" lists them)").arg(hidden);
    lines << head + QLatin1Char(':');
    for (int i = start; i < end; ++i) {
        const QJsonObject& c = picked[i];
        Refs::instance().anchor_web(base, c.value(QStringLiteral("i")).toInt(), web_control_key(c));
        lines << QStringLiteral("  ") + web_control_line(base, c);
    }
    if (end < picked.size() || start > 0) {
        const QString query =
            words.isEmpty() ? QString() : QStringLiteral(", query=\"%1\"").arg(words.join(QLatin1Char(' ')));
        lines << QStringLiteral("  ") +
                     page_footer(QStringLiteral("controls"), start, end - start, static_cast<int>(picked.size()), false,
                                 end < picked.size() ? QStringLiteral("ui_read(ref=\"%1\", path=\"controls\"%2, "
                                                                      "offset=%3)")
                                                           .arg(base, query)
                                                           .arg(end)
                                                     : QString());
    }
    return lines;
}

/// Lines of the page text holding every query word, paged.
QStringList web_text_matches(const QString& base, const QString& text, const QStringList& words, int offset,
                             int limit) {
    QStringList hits;
    int line_no = 0;
    for (const QString& raw : text.split(QLatin1Char('\n'))) {
        ++line_no;
        const QString l = raw.simplified();
        if (!l.isEmpty() && words_match(l.toLower(), words))
            hits << QStringLiteral("  line %1: %2").arg(line_no).arg(common::clip(l, 300));
    }
    QStringList lines;
    if (hits.isEmpty()) {
        lines << QStringLiteral("text: no line holds \"%1\"").arg(words.join(QLatin1Char(' ')));
        return lines;
    }
    const int start = std::clamp(offset, 0, static_cast<int>(hits.size()));
    const int end = std::min(static_cast<int>(hits.size()), start + limit);
    lines << QStringLiteral("text lines holding \"%1\":").arg(words.join(QLatin1Char(' ')));
    lines += hits.mid(start, end - start);
    lines << QStringLiteral("  ") + page_footer(QStringLiteral("lines"), start, end - start,
                                                static_cast<int>(hits.size()), false,
                                                end < hits.size() ? QStringLiteral("ui_read(ref=\"%1\", path=\"text\", "
                                                                                   "query=\"%2\", offset=%3)")
                                                                        .arg(base, words.join(QLatin1Char(' ')))
                                                                        .arg(end)
                                                                  : QString());
    return lines;
}

QString web_time(qint64 ms, bool with_time) {
    return QDateTime::fromMSecsSinceEpoch(ms).toString(with_time ? QStringLiteral("yyyy-MM-dd HH:mm")
                                                                 : QStringLiteral("yyyy-MM-dd"));
}

/// Intraday candles show their time; daily and longer ones only the date.
bool web_candles_have_time(const QJsonObject& chart) {
    const int n = chart.value(QStringLiteral("count")).toInt();
    if (n < 2)
        return true;
    const double first = chart.value(QStringLiteral("first")).toDouble();
    const double last = chart.value(QStringLiteral("last")).toArray().at(0).toDouble();
    return (last - first) / (n - 1) < 20.0 * 3600.0 * 1000.0;
}

QString web_candle_text(const QJsonArray& k, bool with_time) {
    return QStringLiteral("%1 | %2 | %3 | %4 | %5 | %6")
        .arg(web_time(static_cast<qint64>(k.at(0).toDouble()), with_time), fmt_number(k.at(1).toDouble()),
             fmt_number(k.at(2).toDouble()), fmt_number(k.at(3).toDouble()), fmt_number(k.at(4).toDouble()),
             k.at(5).isDouble() ? fmt_number(k.at(5).toDouble()) : QStringLiteral("—"));
}

/// A KLineChart page's data in a few lines: range, last candle, indicators.
QStringList web_chart_summary(const QString& base, const QJsonObject& c) {
    QStringList lines;
    const int n = c.value(QStringLiteral("count")).toInt();
    if (n <= 0) {
        lines << QStringLiteral("chart: no candles loaded yet");
        return lines;
    }
    const bool with_time = web_candles_have_time(c);
    const QJsonArray last = c.value(QStringLiteral("last")).toArray();
    QString head = QStringLiteral("chart · %1 candles · %2 → %3 · low %4 · high %5")
                       .arg(n)
                       .arg(web_time(static_cast<qint64>(c.value(QStringLiteral("first")).toDouble()), with_time),
                            web_time(static_cast<qint64>(last.at(0).toDouble()), with_time),
                            fmt_number(c.value(QStringLiteral("low")).toDouble()),
                            fmt_number(c.value(QStringLiteral("high")).toDouble()));
    if (c.contains(QStringLiteral("from")) && c.contains(QStringLiteral("to"))) {
        const int from = std::max(0, c.value(QStringLiteral("from")).toInt());
        const int to = std::min(n, c.value(QStringLiteral("to")).toInt());
        if (to > from)
            head += QStringLiteral(" · on screen: candles %1–%2").arg(from).arg(to - 1);
    }
    if (const QString type = c.value(QStringLiteral("type")).toString(); !type.isEmpty())
        head += QStringLiteral(" · style %1").arg(type);
    lines << head;
    lines << QStringLiteral("  last: %1").arg(web_candle_text(last, with_time))
          << QStringLiteral("  (columns: time | open | high | low | close | volume)");
    for (const QJsonValue& v : c.value(QStringLiteral("indicators")).toArray()) {
        const QJsonObject ind = v.toObject();
        QStringList params;
        for (const QJsonValue& p : ind.value(QStringLiteral("params")).toArray())
            params << (p.isDouble() ? fmt_number(p.toDouble()) : p.toString());
        QStringList values;
        for (const QJsonValue& pair : ind.value(QStringLiteral("last")).toArray()) {
            const QJsonArray kv = pair.toArray();
            values << QStringLiteral("%1 %2").arg(kv.at(0).toString(), fmt_number(kv.at(1).toDouble()));
        }
        const QString pane = ind.value(QStringLiteral("pane")).toString();
        lines << QStringLiteral("  indicator %1%2 %3%4")
                     .arg(ind.value(QStringLiteral("name")).toString(),
                          params.isEmpty() ? QString()
                                           : QLatin1Char('(') + params.join(QLatin1Char(',')) + QLatin1Char(')'),
                          pane == QLatin1String("candle_pane") ? QStringLiteral("on the price pane")
                                                               : QStringLiteral("in its own pane"),
                          values.isEmpty() ? QString()
                                           : QStringLiteral(" — last: ") + values.join(QStringLiteral(", ")));
    }
    lines << QStringLiteral("  every candle: ui_read(ref=\"%1\", path=\"candles\") — the newest %2; offset=N for "
                            "earlier ones")
                 .arg(base)
                 .arg(kDefaultRows);
    return lines;
}

QString web_candles_page(const QString& base, const QJsonObject& c, int limit) {
    const int n = c.value(QStringLiteral("count")).toInt();
    const QJsonArray rows = c.value(QStringLiteral("rows")).toArray();
    if (n <= 0 || rows.isEmpty())
        return QStringLiteral("%1 chart: no candles loaded yet").arg(base);
    const int start = c.value(QStringLiteral("start")).toInt();
    const bool with_time = web_candles_have_time(c);
    QStringList lines;
    lines
        << QStringLiteral("%1 chart candles · %2 in all · time | open | high | low | close | volume").arg(base).arg(n);
    for (const QJsonValue& v : rows)
        lines << QStringLiteral("  c%1: %2")
                     .arg(start + static_cast<int>(lines.size()) - 1)
                     .arg(web_candle_text(v.toArray(), with_time));
    QString footer = QStringLiteral("showing candles %1–%2 of %3").arg(start).arg(start + rows.size() - 1).arg(n);
    if (start > 0)
        footer += QStringLiteral(" · earlier: ui_read(ref=\"%1\", path=\"candles\", offset=%2)")
                      .arg(base)
                      .arg(std::max(0, start - limit));
    if (start + rows.size() < n)
        footer += QStringLiteral(" · later: ui_read(ref=\"%1\", path=\"candles\", offset=%2)")
                      .arg(base)
                      .arg(start + rows.size());
    lines << footer;
    return lines.join(QLatin1Char('\n'));
}

QString web_failure(const QString& base, const QJsonObject& answer, const QString& error) {
    QString why = error;
    if (why.isEmpty())
        why = answer.value(QStringLiteral("error")).toString();
    if (why.isEmpty())
        why = QStringLiteral("it gave no answer");
    return QStringLiteral("%1: the web page could not be read — %2. ui_screenshot(target=\"%1\") shows it.")
        .arg(base, why);
}

/// ui_read on a web page (or a chart / host showing one). Asynchronous: the
/// page answers from its own process.
void read_web(QWidget* view, QWidget* host, const QString& ref, int web_control, const QString& web_key,
              const QJsonObject& args, Reply reply) {
    const QString base = ref.section(QLatin1Char(':'), 0, 0);
    const QString path = args.value(QStringLiteral("path")).toString().trimmed().toLower();
    const bool has_offset = args.contains(QStringLiteral("offset"));
    const int offset = clamp_int(args.value(QStringLiteral("offset")), 0, 0, 100000000);
    const int limit = clamp_int(args.value(QStringLiteral("limit")), kDefaultRows, 1, kMaxRows);
    const int text_chars = clamp_int(args.value(QStringLiteral("limit")), kTextPageChars, 200, kMaxTextPageChars);
    const QStringList words = query_words(args.value(QStringLiteral("query")).toString());
    const QString header = element_header(base, host);
    QPointer<QWidget> vg(view);

    // One control in full: role, name, value, states, a select's options.
    if (web_control >= 0) {
        const QJsonObject spec{{QStringLiteral("mode"), QStringLiteral("control")},
                               {QStringLiteral("i"), web_control},
                               {QStringLiteral("k"), web_key}};
        run_web_script(
            view, web_target_script(spec), false, kWebAnswerMs,
            [reply, ref, base, web_key](const QJsonObject& c, const QString& error) {
                if (!error.isEmpty() || c.isEmpty()) {
                    reply(ToolResult::fail(web_failure(base, c, error)));
                    return;
                }
                const QString err = c.value(QStringLiteral("error")).toString();
                if (err == QLatin1String("gone") || err == QLatin1String("several")) {
                    reply(ToolResult::fail(QStringLiteral("%1 (%2) is no longer on the page as listed — read it again: "
                                                          "ui_read(ref=\"%3\").")
                                               .arg(ref, common::quoted(web_key.section(QLatin1Char('|'), 1)), base)));
                    return;
                }
                if (!err.isEmpty()) {
                    reply(ToolResult::fail(web_failure(base, c, QString())));
                    return;
                }
                QStringList lines;
                lines << web_control_line(base, c);
                if (c.value(QStringLiteral("covered")).toBool())
                    lines << QStringLiteral("  covered by %1 — a click would land on that")
                                 .arg(common::quoted(c.value(QStringLiteral("cover")).toString()));
                const QJsonArray options = c.value(QStringLiteral("options")).toArray();
                if (!options.isEmpty()) {
                    lines << QStringLiteral("  options (%1):").arg(options.size());
                    for (const QJsonValue& o : options)
                        lines << QStringLiteral("    ") + o.toString();
                }
                lines << QStringLiteral("Act on it: ui_act(action=\"click\" | \"type\" | \"select\" …, "
                                        "ref=\"%1\").")
                             .arg(ref);
                reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
            });
        return;
    }

    if (path == QLatin1String("candles") || path == QLatin1String("chart")) {
        run_web_script(
            view, web_chart_script(has_offset ? offset : -1, limit), true, kWebAnswerMs,
            [reply, base, limit](const QJsonObject& c, const QString& error) {
                if (!error.isEmpty() || c.contains(QStringLiteral("error"))) {
                    reply(ToolResult::fail(web_failure(base, c, error)));
                    return;
                }
                if (c.isEmpty()) {
                    reply(ToolResult::fail(
                        QStringLiteral("%1 shows no chart data (path=\"candles\" is for chart pages).").arg(base)));
                    return;
                }
                reply(ToolResult::ok(web_candles_page(base, c, limit)));
            });
        return;
    }
    if (!path.isEmpty() && path != QLatin1String("text") && path != QLatin1String("controls")) {
        reply(ToolResult::fail(QStringLiteral("%1 is a web page: path is \"text\", \"controls\" or \"candles\" (a "
                                              "chart's data).")
                                   .arg(base)));
        return;
    }
    run_web_script(view, web_read_script(), false, kWebAnswerMs,
                   [reply, vg, base, header, path, words, offset, limit, text_chars](const QJsonObject& page,
                                                                                     const QString& error) {
                       if (!error.isEmpty() || page.isEmpty() || page.contains(QStringLiteral("error"))) {
                           reply(ToolResult::fail(web_failure(base, page, error)));
                           return;
                       }
                       const QString text = page.value(QStringLiteral("text")).toString();
                       const QJsonArray controls = page.value(QStringLiteral("controls")).toArray();
                       QStringList head{header, web_page_line(page)};
                       QStringList body;
                       if (path == QLatin1String("text")) {
                           if (!words.isEmpty())
                               body += web_text_matches(base, text, words, offset, limit);
                           else
                               body << page_text(QStringLiteral("text"), text, offset, base, text_chars,
                                                 QStringLiteral(", path=\"text\""));
                           reply(ToolResult::ok((head + body).join(QLatin1Char('\n'))));
                           return;
                       }
                       if (path == QLatin1String("controls")) {
                           body += web_controls(base, controls, words, false, offset, limit);
                           body << QStringLiteral(
                                       "Act on one: ui_act(action=\"click\", ref=\"%1:w<N>\") — a hidden one is "
                                       "clicked through the page.")
                                       .arg(base);
                           reply(ToolResult::ok((head + body).join(QLatin1Char('\n'))));
                           return;
                       }
                       // Overview: the controls on screen, the start of the text, the chart's data.
                       body += web_controls(base, controls, words, true, 0, limit);
                       if (!words.isEmpty())
                           body += web_text_matches(base, text, words, 0, kWebMatchLinesShown);
                       else
                           body << page_text(QStringLiteral("text"), text, 0, base, kWebOverviewChars,
                                             QStringLiteral(", path=\"text\""));
                       body << QStringLiteral("Act: ui_act(action=\"click\", ref=\"%1:w<N>\"); type into a field: "
                                              "ui_act(action=\"type\", ref=\"%1:w<N>\", text=…); anywhere else: "
                                              "ui_act(action=\"click\", ref=\"%1\", x=…, y=…).")
                                   .arg(base);
                       if (!vg) {
                           reply(ToolResult::ok((head + body).join(QLatin1Char('\n'))));
                           return;
                       }
                       run_web_script(vg, web_chart_script(-1, 0), true, kWebAnswerMs,
                                      [reply, base, head, body](const QJsonObject& c, const QString& error) {
                                          QStringList lines = head;
                                          if (error.isEmpty() && !c.isEmpty() && !c.contains(QStringLiteral("error")))
                                              lines += web_chart_summary(base, c);
                                          lines += body;
                                          reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
                                      });
                   });
}

} // namespace ui_observe_detail

using namespace ui_observe_detail;

// ═════════════════════════════════════════════════════════════════════════════
// ui_overview
// ═════════════════════════════════════════════════════════════════════════════

void ui_overview(const QJsonObject& /*args*/, Reply reply) {
    QStringList lines;
    auto& auth = auth::AuthManager::instance();
    const bool locked = auth::InactivityGuard::instance().is_terminal_locked();
    QString header = QStringLiteral("Fincept Terminal %1").arg(QCoreApplication::applicationVersion());
    header += auth.is_authenticated() ? QStringLiteral(" · signed in (%1)").arg(auth.session().plan_label())
                                      : QStringLiteral(" · not signed in (the sign-in screen is showing)");
    const QString layout_name = layout::WorkspaceShell::current_name();
    if (!layout_name.isEmpty())
        header += QStringLiteral(" · layout %1").arg(common::quoted(layout_name));
    if (locked)
        header += QStringLiteral(" · LOCKED");
    lines << header;
    if (locked)
        lines << QStringLiteral(
            "The terminal is locked (PIN screen). Nothing can be operated until the user unlocks it.");

    WindowFrame* active = active_frame();
    const QList<WindowFrame*> all = frames();
    for (WindowFrame* f : all) {
        QList<PanelInfo> panels = panels_of(f);
        std::stable_sort(panels.begin(), panels.end(), [](const PanelInfo& a, const PanelInfo& b) {
            if (a.focused != b.focused)
                return a.focused;
            return a.visible && !b.visible;
        });
        QStringList states;
        if (f == active)
            states << QStringLiteral("active");
        if (f->isMinimized())
            states << QStringLiteral("minimized");
        if (f->is_locked())
            states << QStringLiteral("locked");
        lines << QStringLiteral("window %1 %2 (id %3)%4 · %5 panel%6")
                     .arg(Refs::instance().ref_for(f, RefKind::Window), common::quoted(window_title_of(f)))
                     .arg(f->window_id())
                     .arg(states.isEmpty()
                              ? QString()
                              : QStringLiteral(" [") + states.join(QStringLiteral(", ")) + QLatin1Char(']'))
                     .arg(panels.size())
                     .arg(panels.size() == 1 ? QString() : QStringLiteral("s"));
        for (const PanelInfo& p : panels)
            lines << QStringLiteral("  ") + panel_line(p);
        if (panels.isEmpty() || (f->dock_manager() && !f->dock_manager()->isVisible()))
            lines << QStringLiteral("  (the panel area is not showing — the window shows a sign-in, lock or chat-mode "
                                    "screen; ui_snapshot(target=\"%1\") shows it)")
                         .arg(Refs::instance().ref_for(f, RefKind::Window));
    }
    if (all.isEmpty())
        lines << QStringLiteral("No terminal window is open.");

    for (QWidget* d : open_dialogs())
        lines << dialog_line(d);
    if (QWidget* popup = open_popup())
        lines << QStringLiteral("menu %1 open — ui_snapshot(target=\"menu\") lists its items")
                     .arg(Refs::instance().ref_for(popup, RefKind::Menu));

    const BusyState busy = busy_state(nullptr);
    if (busy.busy())
        lines << QStringLiteral("busy: ") + busy.describe() + QStringLiteral(" — ui_wait(until=\"idle\") waits for it");
    lines << QStringLiteral(
        "Next: ui_snapshot(target=<panel ref>) lists a panel's controls; ui_open() lists every screen.");
    reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
}

// ═════════════════════════════════════════════════════════════════════════════
// ui_snapshot
// ═════════════════════════════════════════════════════════════════════════════

void ui_snapshot(const QJsonObject& args, Reply reply) {
    QString label, error;
    QWidget* scope = resolve_scope(args.value(QStringLiteral("target")).toString(), &label, &error);
    if (!scope) {
        reply(ToolResult::fail(error));
        return;
    }
    WalkOptions opt;
    opt.detail = parse_detail(args);
    opt.max_nodes = clamp_int(args.value(QStringLiteral("max_nodes")), 150, 20, 800);
    opt.query = query_words(args.value(QStringLiteral("query")).toString());
    opt.cell_widgets = !opt.query.isEmpty(); // a query also finds the buttons in table rows
    const int offset = clamp_int(args.value(QStringLiteral("offset")), 0, 0, 100000);

    QStringList lines;
    bool truncated = false;
    if (auto* f = qobject_cast<WindowFrame*>(scope)) {
        snapshot_frame(f, opt, lines, &truncated);
        for (QWidget* d : open_dialogs())
            snapshot_top(d, dialog_line(d), opt, lines, &truncated);
        if (QWidget* popup = open_popup())
            snapshot_top(popup, QStringLiteral("menu %1").arg(Refs::instance().ref_for(popup, RefKind::Menu)), opt,
                         lines, &truncated);
    } else if (auto* dock = qobject_cast<ads::CDockWidget*>(scope)) {
        PanelInfo info;
        for (const PanelInfo& p : panels_of(frame_of(dock))) {
            if (p.dock == dock)
                info = p;
        }
        if (!info.dock) {
            info.dock = dock;
            info.screen_id = dock->objectName();
            info.title = dock->windowTitle();
        }
        if (!dock->isVisible()) {
            reply(ToolResult::fail(QStringLiteral("%1 is a background tab — bring it forward with "
                                                  "ui_workspace(action=\"focus\", panel=\"%2\") first.")
                                       .arg(label, Refs::instance().ref_for(dock, RefKind::Panel))));
            return;
        }
        snapshot_top(dock, panel_line(info), opt, lines, &truncated, offset);
    } else if (qobject_cast<QMenu*>(scope)) {
        snapshot_top(scope, QStringLiteral("menu %1").arg(Refs::instance().ref_for(scope, RefKind::Menu)), opt, lines,
                     &truncated, offset);
    } else if (scope->isWindow()) {
        snapshot_top(scope, dialog_line(scope), opt, lines, &truncated, offset);
    } else {
        snapshot_top(scope, QStringLiteral("element %1 (in %2)").arg(label, location_label(scope)), opt, lines,
                     &truncated, offset);
    }

    if (!opt.query.isEmpty() && lines.size() <= 1)
        lines << QStringLiteral("(nothing holds \"%1\")").arg(opt.query.join(QLatin1Char(' ')));
    if (truncated) {
        if (qobject_cast<WindowFrame*>(scope))
            lines << QStringLiteral("… more elements not listed — snapshot one panel (target=\"p…\") or narrow with "
                                    "query=\"words\".");
        else
            lines << QStringLiteral("… more elements not listed — continue with ui_snapshot(target=\"%1\", offset=%2), "
                                    "or narrow with query=\"words\".")
                         .arg(args.value(QStringLiteral("target")).toString().trimmed(),
                              QString::number(offset + opt.max_nodes));
    }
    const BusyState busy = busy_state(scope);
    if (busy.busy())
        lines << QStringLiteral("busy: ") + busy.describe();
    reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
}

// ═════════════════════════════════════════════════════════════════════════════
// ui_read
// ═════════════════════════════════════════════════════════════════════════════

void ui_read(const QJsonObject& args, Reply reply) {
    const QString ref = args.value(QStringLiteral("ref")).toString().trimmed();
    const int offset = clamp_int(args.value(QStringLiteral("offset")), 0, 0, 100000000);
    const int limit = clamp_int(args.value(QStringLiteral("limit")), kDefaultRows, 1, kMaxRows);
    const int text_chars = clamp_int(args.value(QStringLiteral("limit")), kTextPageChars, 200, kMaxTextPageChars);
    if (ref.isEmpty()) {
        reply(ToolResult::fail(QStringLiteral("`ref` is required — an element ref, a panel ref / screen, or a stored "
                                              "result id (res_…).")));
        return;
    }
    if (ref.startsWith(QLatin1String("res_"))) {
        const QString text = read_result(ref, args);
        reply(text.isEmpty()
                  ? ToolResult::fail(QStringLiteral("Unknown or expired result %1 (kept 15 minutes).").arg(ref))
                  : ToolResult::ok(text));
        return;
    }

    // A PIN-locked terminal shows nothing, so nothing on it is readable — refs
    // taken before the lock included (stored res_… results are already out).
    if (auth::InactivityGuard::instance().is_terminal_locked()) {
        reply(ToolResult::fail(QStringLiteral("The terminal is locked — the user must unlock it first.")));
        return;
    }

    QString why;
    std::optional<Resolved> r = Refs::instance().resolve(ref, &why);
    if (!r) {
        // A screen id / title reads that panel's state.
        if (ads::CDockWidget* dock = find_open_panel(ref)) {
            reply(ToolResult::ok(panel_state(dock)));
            return;
        }
        reply(ToolResult::fail(
            why.isEmpty() ? QStringLiteral("%1 is unknown or gone — take a fresh ui_snapshot.").arg(ref) : why));
        return;
    }
    if (r->dock) {
        reply(ToolResult::ok(panel_state(r->dock)));
        return;
    }
    if (r->action) {
        QAction* a = r->action;
        QString text =
            QStringLiteral("%1 menuitem %2").arg(ref, common::quoted(plain_text(a->text()).remove(QLatin1Char('&'))));
        if (!a->toolTip().isEmpty() && a->toolTip() != a->text())
            text += QStringLiteral(" — ") + a->toolTip();
        if (QMenu* sub = a->menu()) {
            text += QStringLiteral("\nsubmenu:");
            for (QAction* s : sub->actions()) {
                if (!s->isSeparator() && s->isVisible())
                    text += QStringLiteral("\n  ") + plain_text(s->text()).remove(QLatin1Char('&'));
            }
        }
        reply(ToolResult::ok(text));
        return;
    }
    QWidget* w = r->widget;
    if (!w) {
        reply(ToolResult::fail(QStringLiteral("%1 is gone — take a fresh ui_snapshot.").arg(ref)));
        return;
    }
    if (is_hidden_from_agents(w)) {
        reply(ToolResult::fail(QStringLiteral("%1 cannot be read.").arg(ref)));
        return;
    }
    if (is_secret(w)) {
        reply(ToolResult::fail(QStringLiteral("%1 is a secret field — its contents are never read.").arg(ref)));
        return;
    }
    // A web page (a web view, or a chart drawn by one): read through the page.
    if (QWidget* web = web_view_of(w)) {
        read_web(web, w, ref, r->web, r->web_key, args, std::move(reply));
        return;
    }
    if (w->inherits("fincept::ui::KLineChartWidget")) {
        reply(ToolResult::ok(element_header(ref, w) +
                             QStringLiteral("\nIts chart page is not running (it starts once the chart is shown) — "
                                            "ui_screenshot(target=\"%1\") shows what is there.")
                                 .arg(ref)));
        return;
    }
    if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        if (r->row >= 0) {
            // One row in full: every column (paged past 60), the controls in it
            // with their refs, and its selection / check state.
            const QList<int> cols = visible_columns(view);
            const QStringList headers = column_headers(view, cols);
            const QModelIndex first = view_index(view, r->row, cols.value(0, 0));
            if (!first.isValid()) {
                reply(ToolResult::fail(QStringLiteral("%1: row %2 does not exist (now %3 rows).")
                                           .arg(ref)
                                           .arg(r->row)
                                           .arg(view_row_count(view))));
                return;
            }
            const QString base = ref.section(QLatin1Char(':'), 0, 0);
            QStringList lines;
            QString head = QStringLiteral("%1:r%2").arg(base).arg(r->row);
            if (r->listed_row >= 0)
                head += QStringLiteral(" (you listed it as r%1 — the list has changed since)").arg(r->listed_row);
            QStringList states;
            if (QItemSelectionModel* sel = view->selectionModel();
                sel && sel->isRowSelected(first.row(), first.parent()))
                states << QStringLiteral("selected");
            if (const QVariant check = first.data(Qt::CheckStateRole); check.isValid())
                states << (check.toInt() == Qt::Checked ? QStringLiteral("checked") : QStringLiteral("unchecked"));
            if (auto* tree = qobject_cast<QTreeView*>(view);
                tree && view->model()->hasChildren(first.siblingAtColumn(0)))
                states << (tree->isExpanded(first.siblingAtColumn(0)) ? QStringLiteral("expanded")
                                                                      : QStringLiteral("collapsed"));
            if (!states.isEmpty())
                head += QStringLiteral(" [") + states.join(QStringLiteral(", ")) + QLatin1Char(']');
            lines << head;
            const int col_offset = clamp_int(args.value(QStringLiteral("col_offset")), 0, 0,
                                             std::max(0, static_cast<int>(cols.size()) - 1));
            const int end = std::min(static_cast<int>(cols.size()), col_offset + 60);
            for (int i = col_offset; i < end; ++i) {
                const QModelIndex idx = first.siblingAtColumn(cols[i]);
                QString value = idx.data(Qt::DisplayRole).toString();
                if (value.isEmpty()) {
                    if (QWidget* cell = view->indexWidget(idx))
                        value = cell_widget_text(cell, true);
                    else if (const QVariant check = idx.data(Qt::CheckStateRole); check.isValid())
                        value = check.toInt() == Qt::Checked ? QStringLiteral("[x]") : QStringLiteral("[ ]");
                    else
                        value = plain_text(idx.data(Qt::ToolTipRole).toString()).simplified();
                }
                lines << QStringLiteral("  %1: %2")
                             .arg(headers.value(i).isEmpty() ? QStringLiteral("value") : headers.value(i),
                                  common::clip(value, 1000));
            }
            if (end < cols.size())
                lines << QStringLiteral("  … %1 more columns — ui_read(ref=\"%2\", col_offset=%3)")
                             .arg(cols.size() - end)
                             .arg(ref)
                             .arg(end);
            lines << QStringLiteral("Click one cell: ui_act(action=\"click\", ref=\"%1:r%2\", column=\"<header>\").")
                         .arg(base)
                         .arg(r->row);
            reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
            return;
        }
        reply(ToolResult::ok(read_view(view, ref, args)));
        return;
    }
    if (auto* cb = qobject_cast<QComboBox*>(w)) {
        QStringList lines;
        lines << QStringLiteral("%1 combobox · %2 options · current %3")
                     .arg(ref)
                     .arg(cb->count())
                     .arg(common::quoted(cb->currentText()));
        const int end = std::min(cb->count(), offset + limit);
        for (int i = std::max(0, offset); i < end; ++i)
            lines << QStringLiteral("  %1%2").arg(cb->itemText(i),
                                                  i == cb->currentIndex() ? QStringLiteral("  ← current") : QString());
        if (end < cb->count())
            lines << QStringLiteral("  … %1 more: ui_read(ref=\"%2\", offset=%3)")
                         .arg(cb->count() - end)
                         .arg(ref)
                         .arg(end);
        reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
        return;
    }
    if (auto* chart = qobject_cast<QChartView*>(w)) {
        reply(ToolResult::ok(
            read_chart(chart, ref, offset, limit, query_words(args.value(QStringLiteral("query")).toString()))));
        return;
    }
    if (auto* bar = qobject_cast<QTabBar*>(w)) {
        QStringList lines;
        lines << QStringLiteral("%1 tabs:").arg(ref.section(QLatin1Char(':'), 0, 0));
        for (int i = 0; i < bar->count(); ++i)
            lines << QStringLiteral("  %1 %2%3")
                         .arg(Refs::instance().ref_for(bar, RefKind::Element, i), common::quoted(bar->tabText(i)),
                              i == bar->currentIndex() ? QStringLiteral(" [selected]") : QString());
        reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
        return;
    }
    if (w->isWindow() && !qobject_cast<QMenu*>(w)) {
        // A dialog: everything it says, then its controls.
        // A dialog: everything it says, then its controls (paged by offset).
        QStringList lines;
        bool truncated = false;
        WalkOptions opt;
        opt.detail = Detail::Full;
        opt.max_nodes = 120;
        snapshot_top(w, dialog_line(w), opt, lines, &truncated, offset);
        if (auto* mb = qobject_cast<QMessageBox*>(w); mb && !mb->detailedText().isEmpty())
            lines << page_text(QStringLiteral("details"), mb->detailedText(), 0, ref, 3000);
        if (truncated)
            lines << QStringLiteral("… more — ui_read(ref=\"%1\", offset=%2)").arg(ref).arg(offset + opt.max_nodes);
        reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
        return;
    }
    const QString text = widget_full_text(w);
    if (!text.isEmpty() || qobject_cast<QLabel*>(w)) {
        reply(ToolResult::ok(element_header(ref, w) + QLatin1Char('\n') +
                             page_text(QStringLiteral("text"), text, offset, ref, text_chars)));
        return;
    }
    // Anything else: what it is, then its contents (paged by offset).
    QStringList lines;
    bool truncated = false;
    WalkOptions opt;
    opt.detail = Detail::Full;
    opt.max_nodes = 120;
    snapshot_top(w, element_header(ref, w), opt, lines, &truncated, offset);
    if (lines.size() <= 1)
        lines << QStringLiteral("(no readable content — ui_screenshot(target=\"%1\") shows it; a click at a point: "
                                "ui_act(action=\"click\", ref=\"%1\", x=…, y=…))")
                     .arg(ref);
    if (truncated)
        lines << QStringLiteral("… more — ui_read(ref=\"%1\", offset=%2)").arg(ref).arg(offset + opt.max_nodes);
    reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
}

// ═════════════════════════════════════════════════════════════════════════════
// ui_wait
// ═════════════════════════════════════════════════════════════════════════════

void ui_wait(const QJsonObject& args, CancelCheck cancelled, Reply reply) {
    const QString until = args.value(QStringLiteral("until")).toString().trimmed().toLower();
    const int timeout_ms = clamp_int(args.value(QStringLiteral("timeout_ms")), 10000, 0, 60000);
    const QString text = args.value(QStringLiteral("text")).toString().trimmed();
    const QString ref = args.value(QStringLiteral("ref")).toString().trimmed();
    const QString state = args.value(QStringLiteral("state")).toString().trimmed().toLower();
    const QString value = args.value(QStringLiteral("value")).toString();
    const QString target = args.value(QStringLiteral("target")).toString();

    if (until == QLatin1String("time")) {
        // Polled rather than one timer, so a caller that gives up ends the pause.
        common::poll_until(
            250, timeout_ms, [cancelled]() { return cancelled && cancelled(); },
            [reply, timeout_ms](bool was_cancelled) {
                reply(was_cancelled ? ToolResult::fail(QStringLiteral("cancelled"))
                                    : ToolResult::ok(QStringLiteral("Waited %1 ms.").arg(timeout_ms)));
            });
        return;
    }

    QPointer<QWidget> scope;
    if (until == QLatin1String("text") || until == QLatin1String("gone") || until == QLatin1String("idle")) {
        QString label, error;
        scope = resolve_scope(target, &label, &error);
        if (!scope && !target.trimmed().isEmpty()) {
            reply(ToolResult::fail(error));
            return;
        }
    }
    if ((until == QLatin1String("text") || until == QLatin1String("gone")) && text.isEmpty()) {
        reply(ToolResult::fail(QStringLiteral("until=%1 needs `text`.").arg(until)));
        return;
    }
    if (until == QLatin1String("state") && (ref.isEmpty() || state.isEmpty())) {
        reply(ToolResult::fail(QStringLiteral("until=state needs `ref` and `state`.")));
        return;
    }
    if (until == QLatin1String("state") && !Refs::instance().was_issued(ref)) {
        reply(ToolResult::fail(QStringLiteral("%1 was never listed — take a ui_snapshot for refs.").arg(ref)));
        return;
    }
    if (until == QLatin1String("state") && ref.contains(QStringLiteral(":w"))) {
        reply(ToolResult::fail(
            QStringLiteral("A web page's controls are waited on by their text: ui_wait(until=\"text\", "
                           "text=…, target=\"%1\").")
                .arg(ref.section(QLatin1Char(':'), 0, 0))));
        return;
    }
    static const QStringList kModes = {QStringLiteral("text"),   QStringLiteral("gone"),      QStringLiteral("state"),
                                       QStringLiteral("dialog"), QStringLiteral("no_dialog"), QStringLiteral("idle")};
    if (!kModes.contains(until)) {
        reply(
            ToolResult::fail(QStringLiteral("Unknown until=\"%1\" — use text, gone, state, dialog, no_dialog, idle or "
                                            "time.")
                                 .arg(until)));
        return;
    }

    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    auto idle_streak = std::make_shared<int>(0);
    auto last_note = std::make_shared<QString>();
    const QString needle = text.toLower();

    // Web pages in the scope: their text lives in the page's process, so each
    // poll asks for it in the background and the last answer counts. "gone"
    // holds only once every page has answered at least once.
    struct WebWatch {
        QList<QPointer<QWidget>> views;
        QHash<QWidget*, QString> text; // keys are never dereferenced
        QSet<QWidget*> pending;
    };
    auto web = std::make_shared<WebWatch>();
    if (until == QLatin1String("text") || until == QLatin1String("gone")) {
        QList<QWidget*> roots{scope ? scope.data() : static_cast<QWidget*>(active_frame())};
        roots += open_dialogs();
        for (QWidget* root : roots) {
            if (!root)
                continue;
            QList<QWidget*> all = root->findChildren<QWidget*>();
            all.prepend(root);
            for (QWidget* w : all) {
                if (w->inherits("QWebEngineView") && w->isVisible() && !web->views.contains(w))
                    web->views << w;
            }
        }
    }

    auto check = [=]() -> bool {
        if (cancelled && cancelled())
            return true;
        if (until == QLatin1String("text") || until == QLatin1String("gone")) {
            QString haystack = scope_text(scope ? scope.data() : active_frame());
            bool every_page_answered = true;
            for (const QPointer<QWidget>& view : web->views) {
                if (!view)
                    continue;
                QWidget* key = view.data();
                if (!web->pending.contains(key)) {
                    web->pending.insert(key);
                    run_web_script(view, web_text_script(), false, 3000,
                                   [web, key](const QJsonObject& answer, const QString& error) {
                                       web->pending.remove(key);
                                       if (error.isEmpty())
                                           web->text.insert(key,
                                                            answer.value(QStringLiteral("text")).toString().toLower());
                                   });
                }
                if (web->text.contains(key))
                    haystack += QLatin1Char('\n') + web->text.value(key);
                else
                    every_page_answered = false;
            }
            const bool present = haystack.contains(needle);
            return until == QLatin1String("text") ? present : (!present && every_page_answered);
        }
        if (until == QLatin1String("state")) {
            const auto now = state_holds(ref, state, value);
            *last_note = now.value_or(QString());
            return !now.has_value();
        }
        if (until == QLatin1String("dialog"))
            return !open_dialogs().isEmpty() || open_popup() != nullptr;
        if (until == QLatin1String("no_dialog"))
            return open_dialogs().isEmpty() && open_popup() == nullptr;
        // idle: quiet across two consecutive polls
        const BusyState b = busy_state(scope ? scope.data() : nullptr);
        *last_note = b.describe();
        *idle_streak = b.busy() ? 0 : *idle_streak + 1;
        return *idle_streak >= 2;
    };
    const int interval = (until == QLatin1String("text") || until == QLatin1String("gone")) ? 200 : 150;
    common::poll_until(interval, timeout_ms, check, [=](bool ok) {
        if (cancelled && cancelled()) {
            reply(ToolResult::fail(QStringLiteral("cancelled")));
            return;
        }
        const double secs = clock->elapsed() / 1000.0;
        QString what = until;
        if (until == QLatin1String("text") || until == QLatin1String("gone"))
            what += QStringLiteral(" ") + common::quoted(text);
        else if (until == QLatin1String("state"))
            what = QStringLiteral("%1 %2%3").arg(
                ref, state, value.isEmpty() ? QString() : QLatin1Char(' ') + common::quoted(value));
        if (ok) {
            QString msg = QStringLiteral("OK after %1 s: %2.").arg(QString::number(secs, 'f', 1), what);
            if (until == QLatin1String("dialog")) {
                for (QWidget* d : open_dialogs())
                    msg += QLatin1Char('\n') + dialog_line(d);
                if (QWidget* p = open_popup())
                    msg += QStringLiteral("\nmenu %1 open").arg(Refs::instance().ref_for(p, RefKind::Menu));
            }
            reply(ToolResult::ok(msg));
            return;
        }
        QString msg = QStringLiteral("Timed out after %1 s waiting for %2").arg(QString::number(secs, 'f', 1), what);
        if (!last_note->isEmpty())
            msg += QStringLiteral(" (now: %1)").arg(*last_note);
        reply(ToolResult::fail(msg + QLatin1Char('.')));
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// ui_screenshot
// ═════════════════════════════════════════════════════════════════════════════

void ui_screenshot(const QJsonObject& args, Reply reply) {
    QString label, error;
    QWidget* scope = resolve_scope(args.value(QStringLiteral("target")).toString(), &label, &error);
    if (!scope) {
        reply(ToolResult::fail(error));
        return;
    }
    if (!scope->isVisible()) {
        reply(ToolResult::fail(QStringLiteral("%1 is not visible (a background tab or hidden).").arg(label)));
        return;
    }
    bool png = args.value(QStringLiteral("format")).toString().toLower() == QLatin1String("png");
    const int max_width = clamp_int(args.value(QStringLiteral("max_width")), 1280, 320, 2560);
    const bool marks = args.value(QStringLiteral("marks")).toBool(false);

    if (is_secret(scope)) {
        reply(ToolResult::fail(
            QStringLiteral("%1 holds a secret (password / API key) and is never captured.").arg(label)));
        return;
    }
    QPixmap pm = scope->grab();
    if (pm.isNull()) {
        reply(ToolResult::fail(QStringLiteral("Could not capture %1.").arg(label)));
        return;
    }
    // Pixels must not leak what text never does: black out every visible secret
    // field (a revealed key, the MCP token with SHOW on) before anything else.
    int masked = 0;
    {
        QPainter p(&pm);
        for (QWidget* w : scope->findChildren<QWidget*>()) {
            if (!w->isVisible() || !is_secret(w))
                continue;
            if (QWidget* parent = w->parentWidget(); parent && parent != scope && is_secret(parent))
                continue; // the secret ancestor's box covers it
            const QRect rect(w->mapTo(scope, QPoint(0, 0)), w->size());
            if (!rect.intersects(scope->rect()))
                continue;
            p.fillRect(rect, QColor(20, 20, 20));
            p.setPen(QColor(160, 160, 160));
            p.drawText(rect, Qt::AlignCenter, QStringLiteral("[secret]"));
            ++masked;
        }
    }
    int marked = 0;
    if (marks) {
        WalkOptions opt;
        opt.detail = Detail::Brief;
        opt.max_nodes = 300;
        bool truncated = false;
        const QList<Node> nodes = collect_nodes(scope, opt, &truncated);
        QPainter p(&pm);
        QFont f = p.font();
        f.setPixelSize(11);
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        for (const Node& n : nodes) {
            if (n.ref.isEmpty() || !n.widget || !n.widget->isVisible())
                continue;
            QWidget* w = n.widget;
            if (w != scope && !scope->isAncestorOf(w))
                continue;
            QRect rect(w->mapTo(scope, QPoint(0, 0)), w->size());
            if (auto* bar = qobject_cast<QTabBar*>(w); bar && n.sub >= 0)
                rect = QRect(bar->mapTo(scope, bar->tabRect(n.sub).topLeft()), bar->tabRect(n.sub).size());
            if (!rect.intersects(scope->rect()))
                continue;
            p.setPen(QPen(QColor(255, 196, 0), 1));
            p.drawRect(rect.adjusted(0, 0, -1, -1));
            const QRect tag(rect.topLeft(), QSize(fm.horizontalAdvance(n.ref) + 6, fm.height() + 2));
            p.fillRect(tag, QColor(255, 196, 0));
            p.setPen(Qt::black);
            p.drawText(tag, Qt::AlignCenter, n.ref);
            ++marked;
        }
    }
    if (pm.width() > max_width)
        pm = pm.scaledToWidth(max_width, Qt::SmoothTransformation);

    QByteArray bytes;
    {
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        if (!png && !pm.save(&buf, "JPG", 80)) {
            bytes.clear(); // no JPEG image plugin in this build — fall back to PNG
            buf.seek(0);
        }
        if (bytes.isEmpty()) {
            png = true;
            pm.save(&buf, "PNG");
        }
    }
    if (bytes.isEmpty()) {
        reply(ToolResult::fail(QStringLiteral("Could not encode the screenshot of %1.").arg(label)));
        return;
    }
    const QString mime = png ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");

    ToolResult tr = ToolResult::ok(
        QStringLiteral("Screenshot of %1 — %2×%3 %4, %5 KB%6.")
            .arg(label)
            .arg(pm.width())
            .arg(pm.height())
            .arg(png ? QStringLiteral("PNG") : QStringLiteral("JPEG"))
            .arg(bytes.size() / 1024)
            .arg((marks ? QStringLiteral("; %1 controls labelled with their refs").arg(marked) : QString()) +
                 (masked > 0 ? QStringLiteral("; %1 secret field(s) blacked out").arg(masked) : QString())));
    tr.content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("image")},
                                  {QStringLiteral("data"), QString::fromLatin1(bytes.toBase64())},
                                  {QStringLiteral("mimeType"), mime}});
    reply(tr);
}

} // namespace fincept::mcp::core::ui
