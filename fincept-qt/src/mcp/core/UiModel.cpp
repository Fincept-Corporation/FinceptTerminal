// UiModel.cpp — how the core UI tools see the terminal: refs, element
// classification, reading-order walks, windows / panels / dialogs, element
// search by words and the busy state. See UiEngine.h.

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "core/window/WindowRegistry.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"
#include "mcp/server/ToolApprovalDialog.h"
#include "python/PythonRunner.h"

#include <QAbstractBarSeries>
#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractSeries>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QCandlestickSeries>
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPieSeries>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableView>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QXYSeries>

#ifdef HAS_QT_WEBENGINE
#    include <QWebEnginePage>
#    include <QWebEngineScript>
#    include <QWebEngineView>
#endif

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <algorithm>
#include <memory>

namespace fincept::mcp::core::ui {

namespace ui_model_detail {

constexpr int kSweepEvery = 2000;
constexpr int kMaxQueryWalk = 4000; // nodes considered when filtering by words
constexpr int kQueryRowsShown = 10; // matching rows listed per table in a query snapshot
constexpr int kMaxAnchorsPerView = 2000;

QString strip_mnemonic(QString s) {
    static const QRegularExpression amp(QStringLiteral("&(?!&)"));
    s.remove(amp);
    s.replace(QStringLiteral("&&"), QStringLiteral("&"));
    return s;
}

QString short_text(const QString& s, int max_chars) {
    return common::clip(plain_text(s), max_chars);
}

bool is_heading_label(const QLabel* l) {
    const QString on = l->objectName().toLower();
    if (on.contains(QLatin1String("title")) || on.contains(QLatin1String("heading")) ||
        on.contains(QLatin1String("header")) || on.contains(QLatin1String("section")))
        return true;
    const QFont f = l->font();
    const QFont app = QApplication::font();
    return f.bold() && f.pointSizeF() > 0 && app.pointSizeF() > 0 && f.pointSizeF() >= app.pointSizeF() * 1.15;
}

/// The layout that directly holds `w`, searching `root` recursively.
QLayout* containing_layout(QLayout* root, QWidget* w) {
    if (!root)
        return nullptr;
    for (int i = 0; i < root->count(); ++i) {
        QLayoutItem* item = root->itemAt(i);
        if (!item)
            continue;
        if (item->widget() == w)
            return root;
        if (QLayout* sub = item->layout()) {
            if (QLayout* found = containing_layout(sub, w))
                return found;
        }
    }
    return nullptr;
}

QLabel* short_label(QWidget* w) {
    auto* l = qobject_cast<QLabel*>(w);
    if (!l || !l->isVisible())
        return nullptr;
    const QString t = plain_text(l->text()).trimmed();
    if (t.isEmpty() || t.size() > 48)
        return nullptr;
    return l;
}

/// The label a person reads as this field's name: a buddy label, the form row
/// label, or the label right before it in its row/column.
QLabel* label_for(QWidget* w) {
    QWidget* parent = w->parentWidget();
    if (!parent)
        return nullptr;
    for (QObject* o : parent->children()) {
        if (auto* l = qobject_cast<QLabel*>(o); l && l->buddy() == w && l->isVisible())
            return l;
    }
    QLayout* layout = containing_layout(parent->layout(), w);
    if (!layout)
        return nullptr;
    if (auto* form = qobject_cast<QFormLayout*>(layout)) {
        if (auto* l = qobject_cast<QLabel*>(form->labelForField(w)))
            return l->isVisible() ? l : nullptr;
        return nullptr;
    }
    if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
        const int idx = grid->indexOf(w);
        int r = 0, c = 0, rs = 0, cs = 0;
        grid->getItemPosition(idx, &r, &c, &rs, &cs);
        if (c > 0) {
            if (QLayoutItem* left = grid->itemAtPosition(r, c - 1); left && left->widget())
                if (QLabel* l = short_label(left->widget()))
                    return l;
        }
        if (r > 0) {
            if (QLayoutItem* up = grid->itemAtPosition(r - 1, c); up && up->widget())
                if (QLabel* l = short_label(up->widget()))
                    return l;
        }
        return nullptr;
    }
    const int idx = layout->indexOf(w);
    for (int i = idx - 1; i >= 0 && i >= idx - 2; --i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item || !item->widget())
            continue;
        return short_label(item->widget());
    }
    return nullptr;
}

QString name_from_object(const QWidget* w) {
    if (!w->accessibleName().trimmed().isEmpty())
        return w->accessibleName().trimmed();
    if (!w->toolTip().trimmed().isEmpty())
        return short_text(w->toolTip(), 60);
    return humanize_object_name(w->objectName());
}

/// Letters and digits only, lower-cased — "[COLS]" and "cols" compare equal.
QString word_key(const QString& s) {
    QString out;
    for (const QChar c : s) {
        if (c.isLetterOrNumber())
            out += c.toLower();
    }
    return out;
}

/// A metric's caption: short words, no digits, not a sentence ("NET MARGIN", "P/E Ratio:").
bool caption_like(const QString& s) {
    if (s.isEmpty() || s.size() > 40 || s.endsWith(QLatin1Char('.')))
        return false;
    bool letter = false;
    for (const QChar c : s) {
        if (c.isDigit())
            return false;
        letter = letter || c.isLetter();
    }
    return letter;
}

/// A metric's value: short, and a number or an explicit blank ("$245.1B", "12.4%", "—").
bool value_like(const QString& s) {
    if (s.isEmpty() || s.size() > 40)
        return false;
    static const QStringList blanks = {QStringLiteral("—"),  QStringLiteral("–"),   QStringLiteral("-"),
                                       QStringLiteral("--"), QStringLiteral("N/A"), QStringLiteral("n/a")};
    if (blanks.contains(s))
        return true;
    return std::any_of(s.begin(), s.end(), [](QChar c) { return c.isDigit(); });
}

/// Labels showing text directly inside `container` (stops counting at 3).
int shown_text_labels(const QWidget* container) {
    int n = 0;
    for (const QLabel* l : container->findChildren<QLabel*>(Qt::FindDirectChildrenOnly)) {
        if (l->isVisible() && !plain_text(l->text()).trimmed().isEmpty() && ++n > 2)
            break;
    }
    return n;
}

/// Labels showing text as direct items of `layout` (stops counting at 3).
int shown_text_labels(QLayout* layout) {
    int n = 0;
    for (int i = 0; layout && i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        auto* l = item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
        if (l && l->isVisible() && !plain_text(l->text()).trimmed().isEmpty() && ++n > 2)
            break;
    }
    return n;
}

/// Two labels that form one metric tile: alone together in their container, or
/// alone together in one row / column of its layout.
bool lone_label_pair(QWidget* a, QWidget* b) {
    QWidget* parent = a->parentWidget();
    if (!parent || b->parentWidget() != parent)
        return false;
    if (shown_text_labels(parent) == 2)
        return true;
    QLayout* la = containing_layout(parent->layout(), a);
    return la && la == containing_layout(parent->layout(), b) && shown_text_labels(la) == 2;
}

/// The first short visible label in `w` (or `w` itself) that holds a letter —
/// a section header's title ("GLOBAL INDICES"), not a count or an icon.
QLabel* first_title_in(QWidget* w) {
    if (!w || !w->isVisible() || qobject_cast<QAbstractItemView*>(w) || qobject_cast<QAbstractButton*>(w))
        return nullptr;
    auto usable = [](QLabel* l) -> bool {
        if (!short_label(l))
            return false;
        const QString t = plain_text(l->text());
        return std::any_of(t.begin(), t.end(), [](QChar c) { return c.isLetter(); });
    };
    if (auto* l = qobject_cast<QLabel*>(w))
        return usable(l) ? l : nullptr;
    for (QLabel* l : w->findChildren<QLabel*>()) {
        if (usable(l))
            return l;
    }
    return nullptr;
}

QLabel* first_title_in(QLayoutItem* item, int depth = 0) {
    if (!item)
        return nullptr;
    if (item->widget())
        return first_title_in(item->widget());
    if (QLayout* sub = item->layout(); sub && depth < 2) {
        for (int i = 0; i < sub->count(); ++i) {
            if (QLabel* l = first_title_in(sub->itemAt(i), depth + 1))
                return l;
        }
    }
    return nullptr;
}

/// What a table, list or tree with no name of its own is about, as a person
/// reads it: the title of its group box, the name of the small container holding
/// it, or the heading just above it — whichever is nearest, at most three levels
/// up. (Nine identical "table 7 rows" lines on a Markets screen are useless to
/// an agent; "table \"STOCK INDICES\"" is not.) Not used for charts: the label
/// above a chart is usually its controls' ("PERIOD"), not its subject.
/// `*heading` gets the label the name was read from, if it came from one.
QString section_title_for(QWidget* w, QLabel** heading) {
    *heading = nullptr;
    QWidget* child = w;
    for (int level = 0; level < 3; ++level) {
        QWidget* parent = child->parentWidget();
        if (!parent || child->isWindow() || qobject_cast<ads::CDockWidget*>(parent))
            break;
        if (auto* gb = qobject_cast<QGroupBox*>(parent); gb && !gb->title().trimmed().isEmpty())
            return strip_mnemonic(gb->title()).simplified();
        if (level < 2 && !parent->accessibleName().trimmed().isEmpty())
            return parent->accessibleName().trimmed();
        if (QLayout* layout = containing_layout(parent->layout(), child)) {
            const int idx = layout->indexOf(child);
            for (int i = idx - 1; i >= 0 && i >= idx - 2; --i) {
                if (QLabel* l = first_title_in(layout->itemAt(i))) {
                    *heading = l;
                    return plain_text(l->text()).simplified();
                }
            }
        }
        child = parent;
    }
    return {};
}

void append_layout(QLayout* layout, QList<QWidget*>& out, QSet<QWidget*>& seen) {
    if (!layout)
        return;
    if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
        struct Cell {
            int r, c, i;
        };
        QList<Cell> cells;
        for (int i = 0; i < grid->count(); ++i) {
            int r = 0, c = 0, rs = 0, cs = 0;
            grid->getItemPosition(i, &r, &c, &rs, &cs);
            cells.append({r, c, i});
        }
        std::stable_sort(cells.begin(), cells.end(),
                         [](const Cell& a, const Cell& b) { return a.r != b.r ? a.r < b.r : a.c < b.c; });
        for (const Cell& cell : cells) {
            QLayoutItem* item = grid->itemAt(cell.i);
            if (!item)
                continue;
            if (QWidget* w = item->widget()) {
                if (!seen.contains(w)) {
                    out << w;
                    seen << w;
                }
            } else if (QLayout* sub = item->layout()) {
                append_layout(sub, out, seen);
            }
        }
        return;
    }
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item)
            continue;
        if (QWidget* w = item->widget()) {
            if (!seen.contains(w)) {
                out << w;
                seen << w;
            }
        } else if (QLayout* sub = item->layout()) {
            append_layout(sub, out, seen);
        }
    }
}

/// Children in the order a person reads them: layout order where there is a
/// layout, splitter order for splitters, then anything else top-to-bottom.
QList<QWidget*> ordered_children(QWidget* parent) {
    QList<QWidget*> out;
    QSet<QWidget*> seen;
    if (auto* sp = qobject_cast<QSplitter*>(parent)) {
        for (int i = 0; i < sp->count(); ++i) {
            if (QWidget* w = sp->widget(i)) {
                out << w;
                seen << w;
            }
        }
    } else if (parent->layout()) {
        append_layout(parent->layout(), out, seen);
    }
    QList<QWidget*> rest;
    for (QObject* o : parent->children()) {
        auto* w = qobject_cast<QWidget*>(o);
        if (w && !w->isWindow() && !seen.contains(w))
            rest << w;
    }
    std::stable_sort(rest.begin(), rest.end(), [](QWidget* a, QWidget* b) {
        const int ay = a->geometry().y() / 8;
        const int by = b->geometry().y() / 8;
        return ay != by ? ay < by : a->geometry().x() < b->geometry().x();
    });
    out += rest;
    return out;
}

bool is_leaf_control(QWidget* w) {
    return qobject_cast<QAbstractButton*>(w) || qobject_cast<QLineEdit*>(w) || qobject_cast<QAbstractSpinBox*>(w) ||
           qobject_cast<QComboBox*>(w) || qobject_cast<QAbstractItemView*>(w) || qobject_cast<QTextEdit*>(w) ||
           qobject_cast<QPlainTextEdit*>(w) || qobject_cast<QAbstractSlider*>(w) || qobject_cast<QProgressBar*>(w) ||
           qobject_cast<QLabel*>(w) || qobject_cast<QGraphicsView*>(w) || qobject_cast<QTabBar*>(w) ||
           qobject_cast<QMenuBar*>(w) || w->inherits("QWebEngineView") || w->inherits("fincept::ui::KLineChartWidget");
}

bool skipped_class(const QWidget* w) {
    return qobject_cast<const QScrollBar*>(w) || w->inherits("QSplitterHandle") || w->inherits("QSizeGrip") ||
           w->inherits("QRubberBand") || w->inherits("QFocusFrame") || w->inherits("QToolBarExtension") ||
           w->inherits("ads::CDockAreaTitleBar") || w->inherits("ads::CDockWidgetTab") ||
           w->inherits("ads::CDockAreaTabBar") || w->inherits("ads::CTitleBarButton") ||
           w->inherits("ads::CAutoHideSideBar") || w->inherits("ads::CAutoHideTab") ||
           w->inherits("fincept::ui::LoadingOverlay") || w->inherits("fincept::screens::widgets::LoadingOverlay");
}

bool needs_ref(const QString& role) {
    static const QSet<QString> k = {
        QStringLiteral("button"),     QStringLiteral("checkbox"),  QStringLiteral("radio"),
        QStringLiteral("textbox"),    QStringLiteral("textarea"),  QStringLiteral("combobox"),
        QStringLiteral("spinbutton"), QStringLiteral("slider"),    QStringLiteral("date"),
        QStringLiteral("time"),       QStringLiteral("datetime"),  QStringLiteral("tab"),
        QStringLiteral("menuitem"),   QStringLiteral("table"),     QStringLiteral("list"),
        QStringLiteral("tree"),       QStringLiteral("chart"),     QStringLiteral("webview"),
        QStringLiteral("link"),       QStringLiteral("clickable"), QStringLiteral("document"),
    };
    return k.contains(role);
}

QString chart_summary(QChartView* view) {
    QChart* chart = view->chart();
    if (!chart)
        return {};
    QStringList parts;
    const auto series = chart->series();
    for (QAbstractSeries* s : series) {
        QString kind;
        int points = -1;
        switch (s->type()) {
            case QAbstractSeries::SeriesTypeLine:
            case QAbstractSeries::SeriesTypeSpline:
            case QAbstractSeries::SeriesTypeScatter:
            case QAbstractSeries::SeriesTypeArea:
                kind = s->type() == QAbstractSeries::SeriesTypeScatter ? QStringLiteral("scatter")
                       : s->type() == QAbstractSeries::SeriesTypeArea  ? QStringLiteral("area")
                                                                       : QStringLiteral("line");
                if (auto* xy = qobject_cast<QXYSeries*>(s))
                    points = static_cast<int>(xy->count());
                break;
            case QAbstractSeries::SeriesTypeCandlestick:
                kind = QStringLiteral("candles");
                if (auto* cs = qobject_cast<QCandlestickSeries*>(s))
                    points = static_cast<int>(cs->count());
                break;
            case QAbstractSeries::SeriesTypePie:
                kind = QStringLiteral("pie");
                if (auto* ps = qobject_cast<QPieSeries*>(s))
                    points = static_cast<int>(ps->count());
                break;
            case QAbstractSeries::SeriesTypeBoxPlot:
                kind = QStringLiteral("boxplot");
                break;
            default:
                kind = QStringLiteral("bars");
                if (auto* bs = qobject_cast<QAbstractBarSeries*>(s))
                    points = static_cast<int>(bs->count());
                break;
        }
        QString p =
            s->name().trimmed().isEmpty() ? kind : QStringLiteral("%1 (%2)").arg(short_text(s->name(), 30), kind);
        if (points >= 0)
            p += QStringLiteral(" %1 pts").arg(points);
        parts << p;
        if (parts.size() >= 6)
            break;
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace ui_model_detail

using namespace ui_model_detail;

QString cell_widget_text(QWidget* cell, bool with_refs) {
    if (!cell || is_secret(cell) || is_hidden_from_agents(cell))
        return {};
    QList<QWidget*> all{cell};
    all += cell->findChildren<QWidget*>();
    QStringList parts;
    for (QWidget* c : all) {
        // Visible as part of the cell — a cell scrolled out of view is hidden
        // itself, so its own visibility says nothing.
        if (c != cell && !c->isVisibleTo(cell))
            continue;
        if (is_secret(c))
            continue;
        const bool control = qobject_cast<QAbstractButton*>(c) || qobject_cast<QComboBox*>(c) ||
                             qobject_cast<QAbstractSpinBox*>(c) ||
                             (qobject_cast<QLineEdit*>(c) && !qobject_cast<QComboBox*>(c->parentWidget()) &&
                              !qobject_cast<QAbstractSpinBox*>(c->parentWidget()));
        if (!control && !qobject_cast<QLabel*>(c))
            continue;
        QString t = widget_text(c).simplified();
        if (t.isEmpty() && control)
            t = name_from_object(c);
        if (t.isEmpty())
            continue;
        if (control && with_refs) {
            const auto node = describe_widget(c, Detail::Brief);
            parts << QStringLiteral("[%1 %2 %3]")
                         .arg(Refs::instance().ref_for(c, RefKind::Element),
                              node ? node->role : QStringLiteral("control"), common::quoted(common::clip(t, 30)));
        } else {
            parts << common::clip(t, 40);
        }
        if (parts.size() >= 4)
            break;
    }
    return parts.join(QStringLiteral(" · "));
}

// ═════════════════════════════════════════════════════════════════════════════
// Utilities
// ═════════════════════════════════════════════════════════════════════════════

QString plain_text(const QString& maybe_rich) {
    if (maybe_rich.contains(QLatin1Char('<')) && Qt::mightBeRichText(maybe_rich))
        return QTextDocumentFragment::fromHtml(maybe_rich).toPlainText();
    return maybe_rich;
}

QStringList query_words(const QString& query) {
    QStringList out;
    for (const QString& w : query.toLower().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
        if (!out.contains(w))
            out << w;
    }
    return out;
}

bool words_match(const QString& haystack_lower, const QStringList& words) {
    for (const QString& w : words) {
        if (!haystack_lower.contains(w))
            return false;
    }
    return true;
}

QString humanize_object_name(const QString& object_name) {
    if (object_name.isEmpty())
        return {};
    QString out;
    for (int i = 0; i < object_name.size(); ++i) {
        const QChar c = object_name[i];
        if (c == QLatin1Char('_') || c == QLatin1Char('-')) {
            out += QLatin1Char(' ');
            continue;
        }
        if (c.isUpper() && i > 0 && object_name[i - 1].isLower())
            out += QLatin1Char(' ');
        out += c.toLower();
    }
    return out.simplified();
}

int clamp_int(const QJsonValue& v, int def, int lo, int hi) {
    int n = def;
    if (v.isDouble())
        n = v.toInt(def);
    else if (v.isString()) {
        bool ok = false;
        const int parsed = v.toString().trimmed().toInt(&ok);
        if (ok)
            n = parsed;
    }
    return std::clamp(n, lo, hi);
}

namespace {

/// A line edit whose own words say it holds a credential. Catches a field that
/// a "show" toggle switched to plain text before the engine ever saw it masked.
/// Deliberately narrow ("token" alone is a crypto asset, "key" a column name).
bool names_a_credential(const QLineEdit* le) {
    static const QRegularExpression rx(
        QStringLiteral(R"(\b(password|passwd|passphrase|api[\s_-]?key|apikey|api[\s_-]?secret|secret[\s_-]?key|)"
                       R"(client[\s_-]?secret|private[\s_-]?key|access[\s_-]?token|auth[\s_-]?token|api[\s_-]?token|)"
                       R"(bearer|mnemonic|seed[\s_-]?phrase)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    const QString words = le->objectName() + QLatin1Char(' ') + le->accessibleName() + QLatin1Char(' ') +
                          le->placeholderText() + QLatin1Char(' ') + le->toolTip();
    return rx.match(words).hasMatch();
}

} // namespace

bool is_secret(const QWidget* w) {
    for (const QWidget* p = w; p; p = p->parentWidget()) {
        if (p->property("fincept_secret").toBool())
            return true;
        if (p == w) {
            if (auto* le = qobject_cast<const QLineEdit*>(p)) {
                if (le->echoMode() != QLineEdit::Normal) {
                    // Sticky: a "show" toggle later switches the echo mode to
                    // Normal, and the value must not become readable then.
                    const_cast<QLineEdit*>(le)->setProperty("fincept_secret", true);
                    return true;
                }
                if (names_a_credential(le))
                    return true;
            }
        }
        if (p->isWindow())
            break;
    }
    return false;
}

bool is_hidden_from_agents(const QWidget* w) {
    for (const QWidget* p = w; p; p = p->parentWidget()) {
        if (qobject_cast<const ToolApprovalDialog*>(p) || p->property("fincept_agent_hidden").toBool())
            return true;
    }
    return false;
}

QString widget_text(QWidget* w) {
    if (!w || is_secret(w))
        return {};
    if (auto* l = qobject_cast<QLabel*>(w))
        return plain_text(l->text());
    if (auto* b = qobject_cast<QAbstractButton*>(w))
        return strip_mnemonic(b->text());
    if (auto* le = qobject_cast<QLineEdit*>(w))
        return le->text();
    if (auto* cb = qobject_cast<QComboBox*>(w))
        return cb->currentText();
    if (auto* sb = qobject_cast<QAbstractSpinBox*>(w))
        return sb->text();
    if (auto* te = qobject_cast<QTextEdit*>(w))
        return te->toPlainText().left(20000);
    if (auto* pe = qobject_cast<QPlainTextEdit*>(w))
        return pe->toPlainText().left(20000);
    if (auto* gb = qobject_cast<QGroupBox*>(w))
        return strip_mnemonic(gb->title());
    if (auto* pb = qobject_cast<QProgressBar*>(w))
        return pb->text();
    return {};
}

QString widget_full_text(QWidget* w) {
    if (!w || is_secret(w))
        return {};
    if (auto* te = qobject_cast<QTextEdit*>(w))
        return te->toPlainText();
    if (auto* pe = qobject_cast<QPlainTextEdit*>(w))
        return pe->toPlainText();
    return widget_text(w);
}

// ═════════════════════════════════════════════════════════════════════════════
// Refs
// ═════════════════════════════════════════════════════════════════════════════

Refs& Refs::instance() {
    static Refs r;
    return r;
}

QString Refs::ref_for(QObject* obj, RefKind kind, int sub) {
    if (!obj)
        return {};
    const char k = static_cast<char>(kind);
    const QString key = QString(QLatin1Char(k)) + QString::number(reinterpret_cast<quintptr>(obj), 16) +
                        QLatin1Char(':') + QString::number(sub);
    const auto it = by_key_.constFind(key);
    if (it != by_key_.constEnd()) {
        const auto e = by_ref_.constFind(it.value());
        if (e != by_ref_.constEnd() && e->obj == obj)
            return it.value();
        by_ref_.remove(it.value()); // the old object died and its address was reused
        by_key_.remove(key);
    }
    const QString ref = QString(QLatin1Char(k)) + QString::number(++next_[k]);
    by_ref_.insert(ref, Entry{obj, kind, sub});
    by_key_.insert(key, ref);
    if (++allocations_since_sweep_ >= kSweepEvery)
        sweep();
    return ref;
}

void Refs::sweep() {
    allocations_since_sweep_ = 0;
    for (auto it = by_key_.begin(); it != by_key_.end();) {
        const auto e = by_ref_.constFind(it.value());
        if (e == by_ref_.constEnd() || !e->obj) {
            anchors_.remove(it.value());
            web_anchors_.remove(it.value());
            by_ref_.remove(it.value());
            it = by_key_.erase(it);
        } else {
            ++it;
        }
    }
}

bool Refs::was_issued(const QString& raw) const {
    return by_ref_.contains(raw.trimmed().section(QLatin1Char(':'), 0, 0));
}

void Refs::anchor_row(QAbstractItemView* view, const QString& base_ref, int row, const QModelIndex& first) {
    if (!view || base_ref.isEmpty() || row < 0 || !first.isValid())
        return;
    auto& rows = anchors_[base_ref];
    if (rows.size() >= kMaxAnchorsPerView && !rows.contains(row))
        rows.clear(); // a memo of what was last listed, not a store
    rows.insert(row, RowAnchor{QPersistentModelIndex(first), row_key(view, first)});
}

void Refs::anchor_web(const QString& base_ref, int index, const QString& key) {
    if (base_ref.isEmpty() || index < 0 || key.isEmpty())
        return;
    auto& controls = web_anchors_[base_ref];
    if (controls.size() >= kMaxAnchorsPerView && !controls.contains(index))
        controls.clear();
    controls.insert(index, key);
}

std::optional<Resolved> Refs::resolve(const QString& raw, QString* why) {
    static const QRegularExpression rx(QStringLiteral("^\\s*([wpdme])(\\d+)(?::([rw])(\\d+))?\\s*$"));
    const auto m = rx.match(raw);
    auto fail = [why](const QString& reason) -> std::optional<Resolved> {
        if (why)
            *why = reason;
        return std::nullopt;
    };
    if (!m.hasMatch())
        return fail(QStringLiteral("\"%1\" is not a ref (refs look like p3, e12 or e12:r5).").arg(raw.trimmed()));
    const QString base = m.captured(1) + m.captured(2);
    const auto it = by_ref_.constFind(base);
    if (it == by_ref_.constEnd())
        return fail(QStringLiteral("%1 was never listed — take a ui_snapshot for refs.").arg(raw.trimmed()));
    if (!it->obj)
        return fail(QStringLiteral("%1 is gone (closed or rebuilt) — take a fresh ui_snapshot.").arg(raw.trimmed()));
    Resolved r;
    r.ref = raw.trimmed();
    r.kind = it->kind;
    r.sub = it->sub;
    const bool web_suffix = m.captured(3) == QLatin1String("w");
    r.row = m.captured(4).isEmpty() || web_suffix ? -1 : m.captured(4).toInt();
    QObject* obj = it->obj.data();
    if (auto* dock = qobject_cast<ads::CDockWidget*>(obj)) {
        if (dock->isClosed())
            return std::nullopt;
        r.dock = dock;
        r.widget = dock->widget();
    } else if (auto* action = qobject_cast<QAction*>(obj)) {
        r.action = action;
    } else if (auto* w = qobject_cast<QWidget*>(obj)) {
        r.widget = w;
    }
    if (!r.widget && !r.action)
        return fail(QStringLiteral("%1 is gone (closed or rebuilt) — take a fresh ui_snapshot.").arg(r.ref));

    // A web control ref names the control that was LISTED under that number;
    // the page script finds it again by its role and name when it is used.
    if (web_suffix) {
        if (!web_view_of(r.widget))
            return fail(QStringLiteral("%1 shows no web page now.").arg(base));
        r.web = m.captured(4).toInt();
        const auto controls = web_anchors_.constFind(base);
        if (controls == web_anchors_.constEnd() || !controls->contains(r.web))
            return fail(QStringLiteral("%1 was never listed — ui_read(ref=\"%2\") lists the page's controls.")
                            .arg(r.ref, base));
        r.web_key = controls->value(r.web);
        return r;
    }

    // A row ref means the row that was LISTED under that number. Follow it if
    // the view re-sorted or inserted rows; recognise it by its text if the model
    // was rebuilt; refuse if it is gone — never act on whatever row now has the
    // number (a live blotter re-sorts every tick).
    if (r.row >= 0) {
        auto* view = qobject_cast<QAbstractItemView*>(r.widget.data());
        const auto rows = anchors_.constFind(base);
        if (view && rows != anchors_.constEnd()) {
            const auto a = rows->constFind(r.row);
            if (a != rows->constEnd()) {
                int now = -1;
                if (a->idx.isValid() && a->idx.model() == view->model())
                    now = row_of_index(view, a->idx);
                if (now >= 0 && !a->key.isEmpty() &&
                    row_key(view, view_index(view, now, visible_columns(view).value(0, 0))) != a->key)
                    now = -1; // same slot, different contents: the model was refilled in place
                if (now < 0 && !a->key.isEmpty()) {
                    int found = -1;
                    int hits = 0;
                    int scanned = 0;
                    for_each_view_row(view, 0, [&](int row, const QModelIndex& first) {
                        if (row_key(view, first) == a->key) {
                            found = row;
                            ++hits;
                        }
                        return hits < 2 && ++scanned < kMaxRowScan;
                    });
                    if (hits == 1)
                        now = found;
                }
                if (now < 0)
                    return fail(
                        QStringLiteral("%1 (%2) is no longer in that list — it was removed or changed. Read the "
                                       "list again: ui_read(ref=\"%3\").")
                            .arg(r.ref, common::quoted(common::clip(a->key, 60)), base));
                if (now != r.row) {
                    r.listed_row = r.row;
                    r.row = now;
                }
            }
        }
    }
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// Item views
// ═════════════════════════════════════════════════════════════════════════════

QList<int> visible_columns(QAbstractItemView* view) {
    QList<int> cols;
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return cols;
    if (auto* lv = qobject_cast<QListView*>(view)) {
        cols << lv->modelColumn();
        return cols;
    }
    const int n = model->columnCount(view->rootIndex());
    auto* tv = qobject_cast<QTableView*>(view);
    auto* tr = qobject_cast<QTreeView*>(view);
    for (int c = 0; c < n; ++c) {
        if (tv && tv->isColumnHidden(c))
            continue;
        if (tr && tr->isColumnHidden(c))
            continue;
        cols << c;
    }
    return cols;
}

QStringList column_headers(QAbstractItemView* view, const QList<int>& cols) {
    QStringList out;
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return out;
    for (int c : cols) {
        QString h = model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString().simplified();
        if (h.isEmpty())
            h = cols.size() == 1 ? QString() : QStringLiteral("col%1").arg(c);
        out << h;
    }
    return out;
}

int view_row_count(QAbstractItemView* view, bool* capped) {
    if (capped)
        *capped = false;
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return 0;
    if (auto* tree = qobject_cast<QTreeView*>(view)) {
        int n = 0;
        for (QModelIndex idx = model->index(0, 0, tree->rootIndex()); idx.isValid(); idx = tree->indexBelow(idx)) {
            if (++n >= kMaxTreeRows) {
                if (capped)
                    *capped = true;
                break;
            }
        }
        return n;
    }
    return model->rowCount(view->rootIndex()) - view_hidden_rows(view);
}

int view_hidden_rows(QAbstractItemView* view) {
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return 0;
    auto* tv = qobject_cast<QTableView*>(view);
    auto* lv = qobject_cast<QListView*>(view);
    if (!tv && !lv)
        return 0;
    const int n = model->rowCount(view->rootIndex());
    int hidden = 0;
    for (int r = 0; r < n; ++r) {
        if ((tv && tv->isRowHidden(r)) || (lv && lv->isRowHidden(r)))
            ++hidden;
    }
    return hidden;
}

int for_each_view_row(QAbstractItemView* view, int from,
                      const std::function<bool(int row, const QModelIndex& first)>& fn) {
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return 0;
    const int col = visible_columns(view).value(0, 0);
    int visited = 0;
    if (auto* tree = qobject_cast<QTreeView*>(view)) {
        int pos = 0;
        for (QModelIndex idx = model->index(0, 0, tree->rootIndex()); idx.isValid();
             idx = tree->indexBelow(idx), ++pos) {
            if (pos < from)
                continue;
            ++visited;
            if (!fn(pos, col == 0 ? idx : idx.siblingAtColumn(col)))
                break;
        }
        return visited;
    }
    auto* tv = qobject_cast<QTableView*>(view);
    auto* lv = qobject_cast<QListView*>(view);
    const QModelIndex root = view->rootIndex();
    const int n = model->rowCount(root);
    for (int r = std::max(0, from); r < n; ++r) {
        if ((tv && tv->isRowHidden(r)) || (lv && lv->isRowHidden(r)))
            continue;
        ++visited;
        if (!fn(r, model->index(r, col, root)))
            break;
    }
    return visited;
}

int row_of_index(QAbstractItemView* view, const QModelIndex& idx) {
    if (!view || !idx.isValid() || idx.model() != view->model())
        return -1;
    if (qobject_cast<QTreeView*>(view)) {
        const QModelIndex target = idx.siblingAtColumn(0);
        int found = -1;
        for_each_view_row(view, 0, [&](int row, const QModelIndex& first) {
            if (first.siblingAtColumn(0) == target) {
                found = row;
                return false;
            }
            return true;
        });
        return found;
    }
    if (idx.parent() != view->rootIndex())
        return -1;
    if (auto* tv = qobject_cast<QTableView*>(view); tv && tv->isRowHidden(idx.row()))
        return -1;
    if (auto* lv = qobject_cast<QListView*>(view); lv && lv->isRowHidden(idx.row()))
        return -1;
    return idx.row();
}

QString row_key(QAbstractItemView* view, const QModelIndex& first) {
    if (!view || !first.isValid())
        return {};
    // Cells a person names the row by: mostly letters (a symbol, a name, a side),
    // not prices or times, which tick. At most two; else the first cell.
    QStringList parts;
    QString fallback;
    int seen = 0;
    for (int c : visible_columns(view)) {
        if (++seen > 8 || parts.size() >= 2)
            break;
        const QString t = first.siblingAtColumn(c).data(Qt::DisplayRole).toString().simplified();
        if (fallback.isEmpty())
            fallback = t;
        int letters = 0;
        int digits = 0;
        for (const QChar ch : t) {
            if (ch.isLetter())
                ++letters;
            else if (ch.isDigit())
                ++digits;
        }
        if (letters >= 2 && letters >= digits)
            parts << t;
    }
    return parts.isEmpty() ? fallback : parts.join(QStringLiteral(" | "));
}

QString view_sort_state(QAbstractItemView* view) {
    QHeaderView* header = nullptr;
    if (auto* tv = qobject_cast<QTableView*>(view); tv && tv->isSortingEnabled())
        header = tv->horizontalHeader();
    else if (auto* tr = qobject_cast<QTreeView*>(view); tr && tr->isSortingEnabled())
        header = tr->header();
    if (!header || !header->isSortIndicatorShown() || !view->model())
        return {};
    const int c = header->sortIndicatorSection();
    if (c < 0)
        return {};
    const QString name = view->model()->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString().simplified();
    return QStringLiteral("sorted by %1 %2")
        .arg(name.isEmpty() ? QStringLiteral("col%1").arg(c) : name,
             header->sortIndicatorOrder() == Qt::AscendingOrder ? QStringLiteral("↑") : QStringLiteral("↓"));
}

QModelIndex view_index(QAbstractItemView* view, int row, int col) {
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model || row < 0)
        return {};
    if (auto* tree = qobject_cast<QTreeView*>(view)) {
        QModelIndex idx = model->index(0, 0, tree->rootIndex());
        for (int i = 0; i < row && idx.isValid(); ++i)
            idx = tree->indexBelow(idx);
        if (!idx.isValid())
            return {};
        return col == 0 ? idx : idx.siblingAtColumn(col);
    }
    if (row >= model->rowCount(view->rootIndex()))
        return {};
    return model->index(row, col, view->rootIndex());
}

QString view_row_text(QAbstractItemView* view, int row, const QList<int>& cols, int cell_chars) {
    return index_row_text(view, view_index(view, row, cols.value(0, 0)), cols, cell_chars, false);
}

QString index_row_text(QAbstractItemView* view, const QModelIndex& first, const QList<int>& cols, int cell_chars,
                       bool cell_refs) {
    QStringList cells;
    if (!first.isValid())
        return {};
    int depth = 0;
    if (qobject_cast<QTreeView*>(view)) {
        for (QModelIndex p = first.parent(); p.isValid(); p = p.parent())
            ++depth;
    }
    for (int c : cols) {
        const QModelIndex idx = first.siblingAtColumn(c);
        QString t = idx.data(Qt::DisplayRole).toString();
        if (t.isEmpty()) {
            const QVariant check = idx.data(Qt::CheckStateRole);
            if (check.isValid())
                t = check.toInt() == Qt::Checked ? QStringLiteral("[x]") : QStringLiteral("[ ]");
            else if (QWidget* cell = view->indexWidget(idx))
                t = cell_widget_text(cell, cell_refs); // buttons placed in the row: "Modify · Cancel"
            else if (const QString acc = idx.data(Qt::AccessibleTextRole).toString(); !acc.isEmpty())
                t = acc; // an icon-only cell that says what it shows
            else if (const QString tip = idx.data(Qt::ToolTipRole).toString(); !tip.isEmpty())
                t = common::clip(plain_text(tip), 30);
        }
        // Control refs must survive the clip: they are what the agent acts on.
        cells << (cell_refs && t.contains(QLatin1Char('[')) ? t : common::clip(t, cell_chars));
    }
    QString line = cells.join(QStringLiteral(" | "));
    if (depth > 0)
        line = QString(qsizetype{depth} * 2, QLatin1Char(' ')) + line;
    return line;
}

int view_find_row(QAbstractItemView* view, const QString& text, int from, bool* scan_capped) {
    if (scan_capped)
        *scan_capped = false;
    const QStringList words = query_words(text);
    if (words.isEmpty() || !view || !view->model())
        return -1;
    const QList<int> cols = visible_columns(view);
    // One pass: an exact cell match wins outright; otherwise the first row
    // holding every word.
    const QString exact = text.trimmed().toLower();
    int exact_row = -1;
    int words_row = -1;
    int scanned = 0;
    for_each_view_row(view, from, [&](int r, const QModelIndex& first) {
        if (++scanned > kMaxRowScan) {
            if (scan_capped)
                *scan_capped = true;
            return false;
        }
        for (int c : cols) {
            if (first.siblingAtColumn(c).data(Qt::DisplayRole).toString().trimmed().toLower() == exact) {
                exact_row = r;
                return false;
            }
        }
        if (words_row < 0 && words_match(index_row_text(view, first, cols, 200).toLower(), words))
            words_row = r;
        return true;
    });
    return exact_row >= 0 ? exact_row : words_row;
}

QAbstractItemView* cell_widget_owner(QWidget* w, QModelIndex* idx) {
    for (QWidget* child = w; child; child = child->parentWidget()) {
        QWidget* parent = child->parentWidget();
        auto* view = parent ? qobject_cast<QAbstractItemView*>(parent->parentWidget()) : nullptr;
        if (!view || view->viewport() != parent || !view->model())
            continue;
        // `child` sits on the viewport: find the cell it is the index widget of.
        const QList<int> cols = visible_columns(view);
        QModelIndex found;
        int cells = 0;
        for_each_view_row(view, 0, [&](int, const QModelIndex& first) {
            for (int c : cols) {
                const QModelIndex i = first.siblingAtColumn(c);
                if (view->indexWidget(i) == child) {
                    found = i;
                    return false;
                }
            }
            return ++cells < 20000;
        });
        if (!found.isValid())
            return nullptr;
        if (idx)
            *idx = found;
        return view;
    }
    return nullptr;
}

// ═════════════════════════════════════════════════════════════════════════════
// Element classification
// ═════════════════════════════════════════════════════════════════════════════

std::optional<Node> describe_widget(QWidget* w, Detail detail) {
    Node n;
    n.widget = w;
    const int value_chars = detail == Detail::Full ? 300 : 80;

    if (auto* b = qobject_cast<QAbstractButton*>(w)) {
        n.interactive = true;
        n.name = strip_mnemonic(b->text()).simplified();
        if (qobject_cast<QCheckBox*>(b)) {
            n.role = QStringLiteral("checkbox");
            n.states << (b->isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked"));
        } else if (qobject_cast<QRadioButton*>(b)) {
            n.role = QStringLiteral("radio");
            n.states << (b->isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked"));
        } else {
            n.role = QStringLiteral("button");
            if (b->isCheckable())
                n.states << (b->isChecked() ? QStringLiteral("on") : QStringLiteral("off"));
            bool has_menu = false;
            if (auto* pb = qobject_cast<QPushButton*>(b))
                has_menu = pb->menu() != nullptr;
            else if (auto* tb = qobject_cast<QToolButton*>(b))
                has_menu = tb->menu() != nullptr || (tb->defaultAction() && tb->defaultAction()->menu());
            if (has_menu)
                n.states << QStringLiteral("menu");
            if (auto* pb = qobject_cast<QPushButton*>(b); pb && pb->isDefault())
                n.states << QStringLiteral("default");
        }
        if (n.name.isEmpty())
            n.name = name_from_object(w);
        else if (const QString an = w->accessibleName().simplified(); !an.isEmpty() && word_key(an) != word_key(n.name))
            n.name += QStringLiteral(" · ") + an; // "[DEL] · Remove panel": what the bare text only hints at
        if (!b->isEnabled())
            n.states << QStringLiteral("disabled");
        return n;
    }
    if (auto* le = qobject_cast<QLineEdit*>(w)) {
        n.role = QStringLiteral("textbox");
        n.interactive = true;
        QLabel* label = label_for(w);
        n.label_widget = label;
        n.name = !w->accessibleName().trimmed().isEmpty() ? w->accessibleName().trimmed()
                 : label                                  ? plain_text(label->text()).simplified()
                 : !le->placeholderText().isEmpty()       ? le->placeholderText().simplified()
                                                          : name_from_object(w);
        if (is_secret(w)) {
            n.states << QStringLiteral("secret");
            n.value = le->text().isEmpty() ? QString() : QStringLiteral("•••");
        } else {
            n.value = common::clip(le->text(), value_chars);
        }
        if (le->text().isEmpty() && !le->placeholderText().isEmpty() && n.name != le->placeholderText().simplified())
            n.states << QStringLiteral("hint: ") + common::clip(le->placeholderText(), 40);
        if (le->isReadOnly())
            n.states << QStringLiteral("readonly");
        if (!le->isEnabled())
            n.states << QStringLiteral("disabled");
        return n;
    }
    if (qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w)) {
        auto* te = qobject_cast<QTextEdit*>(w);
        auto* pe = qobject_cast<QPlainTextEdit*>(w);
        const bool ro = te ? te->isReadOnly() : pe->isReadOnly();
        const QString text = te ? te->toPlainText() : pe->toPlainText();
        n.role = ro ? QStringLiteral("document") : QStringLiteral("textarea");
        n.interactive = !ro;
        QLabel* label = label_for(w);
        n.label_widget = label;
        n.name = !w->accessibleName().trimmed().isEmpty() ? w->accessibleName().trimmed()
                 : label                                  ? plain_text(label->text()).simplified()
                                                          : name_from_object(w);
        if (is_secret(w))
            n.states << QStringLiteral("secret");
        else
            n.value = common::clip(text, detail == Detail::Full ? 600 : 160);
        if (text.size() > 160)
            n.states << QStringLiteral("%1 chars").arg(text.size());
        if (!w->isEnabled())
            n.states << QStringLiteral("disabled");
        return n;
    }
    if (auto* cb = qobject_cast<QComboBox*>(w)) {
        n.role = QStringLiteral("combobox");
        n.interactive = true;
        QLabel* label = label_for(w);
        n.label_widget = label;
        n.name = !w->accessibleName().trimmed().isEmpty() ? w->accessibleName().trimmed()
                 : label                                  ? plain_text(label->text()).simplified()
                                                          : name_from_object(w);
        n.value = common::clip(cb->currentText(), value_chars);
        n.states << QStringLiteral("%1 options").arg(cb->count());
        if (cb->isEditable())
            n.states << QStringLiteral("editable");
        if (!cb->isEnabled())
            n.states << QStringLiteral("disabled");
        return n;
    }
    if (auto* sb = qobject_cast<QAbstractSpinBox*>(w)) {
        n.interactive = true;
        if (auto* dt = qobject_cast<QDateTimeEdit*>(w)) {
            const bool has_date = (dt->displayedSections() & QDateTimeEdit::DateSections_Mask).toInt() != 0;
            const bool has_time = (dt->displayedSections() & QDateTimeEdit::TimeSections_Mask).toInt() != 0;
            n.role = has_date && has_time ? QStringLiteral("datetime")
                     : has_date           ? QStringLiteral("date")
                                          : QStringLiteral("time");
            n.value = has_date && has_time ? dt->dateTime().toString(Qt::ISODate)
                      : has_date           ? dt->date().toString(Qt::ISODate)
                                           : dt->time().toString(QStringLiteral("HH:mm:ss"));
        } else {
            n.role = QStringLiteral("spinbutton");
            n.value = sb->text();
            if (auto* is = qobject_cast<QSpinBox*>(w))
                n.states << QStringLiteral("%1..%2").arg(is->minimum()).arg(is->maximum());
            else if (auto* ds = qobject_cast<QDoubleSpinBox*>(w))
                n.states << QStringLiteral("%1..%2").arg(ds->minimum()).arg(ds->maximum());
        }
        QLabel* label = label_for(w);
        n.label_widget = label;
        n.name = !w->accessibleName().trimmed().isEmpty() ? w->accessibleName().trimmed()
                 : label                                  ? plain_text(label->text()).simplified()
                                                          : name_from_object(w);
        if (!w->isEnabled())
            n.states << QStringLiteral("disabled");
        return n;
    }
    if (auto* sl = qobject_cast<QAbstractSlider*>(w)) {
        if (qobject_cast<QScrollBar*>(w))
            return std::nullopt;
        n.role = QStringLiteral("slider");
        n.interactive = true;
        QLabel* label = label_for(w);
        n.label_widget = label;
        n.name = label ? plain_text(label->text()).simplified() : name_from_object(w);
        n.value = QString::number(sl->value());
        n.states << QStringLiteral("%1..%2").arg(sl->minimum()).arg(sl->maximum());
        return n;
    }
    if (auto* pb = qobject_cast<QProgressBar*>(w)) {
        n.role = QStringLiteral("progressbar");
        n.name = name_from_object(w);
        n.value = pb->minimum() == pb->maximum()
                      ? QStringLiteral("busy")
                      : QStringLiteral("%1%").arg(
                            pb->maximum() > pb->minimum()
                                ? qRound(100.0 * (pb->value() - pb->minimum()) / (pb->maximum() - pb->minimum()))
                                : 0);
        return n;
    }
    if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        if (qobject_cast<QTreeView*>(w))
            n.role = QStringLiteral("tree");
        else if (qobject_cast<QTableView*>(w))
            n.role = QStringLiteral("table");
        else
            n.role = QStringLiteral("list");
        n.interactive = true;
        n.name = w->accessibleName().trimmed();
        if (n.name.isEmpty()) {
            QLabel* heading = nullptr;
            n.name = section_title_for(w, &heading);
            n.label_widget = heading; // the heading is the table's name now — not repeated as text
        }
        if (n.name.isEmpty())
            n.name = name_from_object(w);
        const QList<int> cols = visible_columns(view);
        n.rows = view_row_count(view, &n.rows_capped);
        n.cols = static_cast<int>(cols.size());
        // A preview shows the first columns only, and says how many it left out.
        const QList<int> shown_cols = cols.mid(0, kPreviewCols);
        if (n.role == QLatin1String("table") || cols.size() > 1) {
            n.headers = column_headers(view, shown_cols);
            if (cols.size() > shown_cols.size())
                n.headers << QStringLiteral("+%1 cols").arg(cols.size() - shown_cols.size());
        }
        const int preview = detail == Detail::Brief ? 0 : detail == Detail::Normal ? 3 : 10;
        const int cell_chars = detail == Detail::Full ? 60 : 24;
        if (preview > 0) {
            for_each_view_row(view, 0, [&](int r, const QModelIndex& first) {
                n.row_lines << QStringLiteral("r%1: %2").arg(r).arg(
                    common::clip(index_row_text(view, first, shown_cols, cell_chars), 220));
                n.row_anchors.append({r, QPersistentModelIndex(first)});
                return n.row_lines.size() < preview;
            });
        }
        if (const int hidden = view_hidden_rows(view); hidden > 0)
            n.states << QStringLiteral("%1 hidden by a filter").arg(hidden);
        if (const QString sort = view_sort_state(view); !sort.isEmpty())
            n.states << sort;
        if (QItemSelectionModel* sel = view->selectionModel()) {
            const QModelIndex cur = sel->currentIndex();
            if (cur.isValid() && sel->isSelected(cur)) {
                const int row = row_of_index(view, cur);
                if (row >= 0)
                    n.states << QStringLiteral("selected r%1").arg(row);
            }
            if (const int count = static_cast<int>(sel->selectedRows().size()); count > 1)
                n.states << QStringLiteral("%1 rows selected").arg(count);
        }
        if (!w->isEnabled())
            n.states << QStringLiteral("disabled");
        return n;
    }
    if (auto* cv = qobject_cast<QChartView*>(w)) {
        n.role = QStringLiteral("chart");
        n.name = cv->chart() && !cv->chart()->title().trimmed().isEmpty()
                     ? plain_text(cv->chart()->title()).simplified()
                     : name_from_object(w);
        n.value = chart_summary(cv);
        return n;
    }
    if (w->inherits("fincept::ui::KLineChartWidget") || w->inherits("QWebEngineView")) {
        n.role = w->inherits("QWebEngineView") ? QStringLiteral("webview") : QStringLiteral("chart");
        n.name = name_from_object(w);
        n.states << (web_view_of(w) ? QStringLiteral("web page — ui_read gives its text, controls and data")
                                    : QStringLiteral("not loaded — ui_screenshot shows it"));
        return n;
    }
    if (auto* l = qobject_cast<QLabel*>(w)) {
        const QString raw = l->text();
        const QString text = plain_text(raw).simplified();
        if (text.isEmpty()) {
            if (!l->pixmap().isNull() && (!w->accessibleName().isEmpty() || !w->toolTip().isEmpty())) {
                n.role = QStringLiteral("image");
                n.name = name_from_object(w);
                return n;
            }
            return std::nullopt;
        }
        if (raw.contains(QLatin1String("<a "), Qt::CaseInsensitive)) {
            n.role = QStringLiteral("link");
            n.interactive = true;
        } else if (is_heading_label(l)) {
            n.role = QStringLiteral("heading");
        } else {
            n.role = QStringLiteral("text");
        }
        if (is_secret(w)) {
            n.name = QStringLiteral("(secret)");
            return n;
        }
        n.name = text;
        if (text.size() > 200)
            n.role = n.role == QLatin1String("text") ? QStringLiteral("text") : n.role;
        return n;
    }
    if (auto* gb = qobject_cast<QGroupBox*>(w)) {
        const QString title = strip_mnemonic(gb->title()).simplified();
        if (title.isEmpty() && !gb->isCheckable())
            return std::nullopt;
        n.role = QStringLiteral("group");
        n.name = title;
        if (gb->isCheckable()) {
            n.interactive = true;
            n.states << (gb->isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked"));
        }
        return n;
    }
    // A custom widget made clickable (the app marks those with a pointing-hand cursor).
    if (w->testAttribute(Qt::WA_SetCursor) && w->cursor().shape() == Qt::PointingHandCursor) {
        n.role = QStringLiteral("clickable");
        n.interactive = true;
        n.name = w->accessibleName().trimmed();
        if (n.name.isEmpty()) {
            QStringList texts;
            for (QLabel* l : w->findChildren<QLabel*>()) {
                if (!l->isVisible())
                    continue;
                const QString t = plain_text(l->text()).simplified();
                if (!t.isEmpty())
                    texts << common::clip(t, 40);
                if (texts.size() >= 3)
                    break;
            }
            n.name = texts.isEmpty() ? name_from_object(w) : texts.join(QStringLiteral(" · "));
        }
        if (!w->isEnabled())
            n.states << QStringLiteral("disabled");
        return n;
    }
    return std::nullopt;
}

// ═════════════════════════════════════════════════════════════════════════════
// Walk
// ═════════════════════════════════════════════════════════════════════════════

namespace ui_model_detail {

struct WalkCtx {
    const WalkOptions* opt = nullptr;
    QList<Node> nodes;
    QSet<QWidget*> consumed_labels;
    int cap = 0;
    bool truncated = false;
};

void push_node(WalkCtx& ctx, Node n) {
    if (static_cast<int>(ctx.nodes.size()) >= ctx.cap) {
        ctx.truncated = true;
        return;
    }
    if (ctx.opt->detail == Detail::Brief && !n.interactive && n.role != QLatin1String("table") &&
        n.role != QLatin1String("list") && n.role != QLatin1String("tree") && n.role != QLatin1String("chart"))
        return;
    // Text longer than a snapshot line shows gets a ref, so the rest is readable.
    if (needs_ref(n.role) || (n.role == QLatin1String("text") && n.name.size() > 150)) {
        QObject* obj = n.action ? static_cast<QObject*>(n.action.data()) : static_cast<QObject*>(n.widget.data());
        n.ref = Refs::instance().ref_for(obj, RefKind::Element, n.sub);
    }
    if (auto* view = qobject_cast<QAbstractItemView*>(n.widget.data()); view && !n.ref.isEmpty()) {
        for (const auto& a : n.row_anchors)
            Refs::instance().anchor_row(view, n.ref, a.first, a.second);
    }
    if (n.label_widget)
        ctx.consumed_labels.insert(n.label_widget.data());
    ctx.nodes.append(std::move(n));
}

/// Buttons, checkboxes, combos placed in the rows currently on screen, each
/// named "<its name> · r<row>" so a person's "the Cancel on the RELIANCE row"
/// can be found by words. (Every row's controls are also listed, with refs, by
/// ui_read of the table.)
void push_cell_controls(QAbstractItemView* view, int depth, WalkCtx& ctx) {
    if (!view->model())
        return;
    const QList<int> cols = visible_columns(view);
    const QRect vp = view->viewport()->rect();
    const int start = std::max(0, row_of_index(view, view->indexAt(QPoint(1, 1))));
    for_each_view_row(view, start, [&](int row, const QModelIndex& first) {
        const QRect rr = view->visualRect(first);
        if (rr.top() > vp.bottom() || ctx.truncated)
            return false;
        for (int c : cols) {
            QWidget* cell = view->indexWidget(first.siblingAtColumn(c));
            if (!cell || !cell->isVisible() || is_hidden_from_agents(cell))
                continue;
            QList<QWidget*> controls{cell};
            controls += cell->findChildren<QWidget*>();
            for (QWidget* ctl : controls) {
                if (!ctl->isVisible() || !(qobject_cast<QAbstractButton*>(ctl) || qobject_cast<QComboBox*>(ctl) ||
                                           qobject_cast<QAbstractSpinBox*>(ctl)))
                    continue;
                if (auto node = describe_widget(ctl, Detail::Brief)) {
                    node->name = (node->name.isEmpty() ? node->role : node->name) + QStringLiteral(" · r%1").arg(row);
                    node->depth = depth;
                    push_node(ctx, std::move(*node));
                }
            }
        }
        return true;
    });
}

void walk(QWidget* w, int depth, WalkCtx& ctx) {
    if (ctx.truncated || !w || !w->isVisible() || is_hidden_from_agents(w) || skipped_class(w) ||
        ctx.opt->exclude.contains(w))
        return;

    if (auto* bar = qobject_cast<QTabBar*>(w)) {
        for (int i = 0; i < bar->count(); ++i) {
            if (!bar->isTabVisible(i))
                continue;
            Node n;
            n.widget = bar;
            n.sub = i;
            n.role = QStringLiteral("tab");
            n.interactive = true;
            n.name = strip_mnemonic(bar->tabText(i)).simplified();
            if (n.name.isEmpty())
                n.name = short_text(bar->tabToolTip(i), 40);
            if (i == bar->currentIndex())
                n.states << QStringLiteral("selected");
            if (!bar->isTabEnabled(i))
                n.states << QStringLiteral("disabled");
            n.depth = depth;
            push_node(ctx, std::move(n));
        }
        return;
    }
    if (auto* mb = qobject_cast<QMenuBar*>(w)) {
        for (QAction* a : mb->actions()) {
            if (!a->isVisible() || a->isSeparator())
                continue;
            Node n;
            n.widget = mb;
            n.action = a;
            n.role = QStringLiteral("menuitem");
            n.interactive = true;
            n.name = strip_mnemonic(a->text()).simplified();
            if (a->menu())
                n.states << QStringLiteral("menu");
            if (!a->isEnabled())
                n.states << QStringLiteral("disabled");
            n.depth = depth;
            push_node(ctx, std::move(n));
        }
        return;
    }
    if (auto* menu = qobject_cast<QMenu*>(w)) {
        for (QAction* a : menu->actions()) {
            if (!a->isVisible() || a->isSeparator())
                continue;
            Node n;
            n.widget = menu;
            n.action = a;
            n.role = QStringLiteral("menuitem");
            n.interactive = true;
            n.name = strip_mnemonic(a->text()).simplified();
            if (a->menu())
                n.states << QStringLiteral("submenu");
            if (a->isCheckable())
                n.states << (a->isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked"));
            if (!a->isEnabled())
                n.states << QStringLiteral("disabled");
            if (!a->shortcut().isEmpty())
                n.states << a->shortcut().toString(QKeySequence::NativeText);
            n.depth = depth;
            push_node(ctx, std::move(n));
        }
        return;
    }

    int child_depth = depth;
    if (auto node = describe_widget(w, ctx.opt->detail)) {
        node->depth = depth;
        const bool group = node->role == QLatin1String("group");
        push_node(ctx, std::move(*node));
        if (group)
            child_depth = depth + 1;
    }
    if (auto* view = qobject_cast<QAbstractItemView*>(w); view && ctx.opt->cell_widgets)
        push_cell_controls(view, depth + 1, ctx);
    if (is_leaf_control(w))
        return;
    if (auto* stack = qobject_cast<QStackedWidget*>(w)) {
        walk(stack->currentWidget(), child_depth, ctx);
        return;
    }
    for (QWidget* child : ordered_children(w))
        walk(child, child_depth, ctx);
}

int name_score(const QString& name_lower, const QString& q) {
    if (name_lower == q)
        return 100;
    if (name_lower.startsWith(q))
        return 70;
    static const QRegularExpression word_sep(QStringLiteral("[^a-z0-9]"));
    const QStringList words = name_lower.split(word_sep, Qt::SkipEmptyParts);
    if (words.contains(q))
        return 55;
    if (name_lower.contains(q))
        return 35;
    const QStringList qw = query_words(q);
    if (qw.size() > 1 && words_match(name_lower, qw))
        return 25;
    return 0;
}

bool role_matches(const QString& wanted, const QString& role) {
    if (wanted.isEmpty())
        return true;
    const QString w = wanted.toLower();
    if (w == role)
        return true;
    static const QHash<QString, QStringList> aliases = {
        {QStringLiteral("textbox"),
         {QStringLiteral("textarea"), QStringLiteral("spinbutton"), QStringLiteral("combobox"), QStringLiteral("date"),
          QStringLiteral("time"), QStringLiteral("datetime")}},
        {QStringLiteral("input"),
         {QStringLiteral("textbox"), QStringLiteral("textarea"), QStringLiteral("spinbutton"),
          QStringLiteral("combobox")}},
        {QStringLiteral("field"),
         {QStringLiteral("textbox"), QStringLiteral("textarea"), QStringLiteral("spinbutton"),
          QStringLiteral("combobox")}},
        {QStringLiteral("button"), {QStringLiteral("clickable"), QStringLiteral("link")}},
        {QStringLiteral("link"), {QStringLiteral("clickable"), QStringLiteral("button")}},
        {QStringLiteral("dropdown"), {QStringLiteral("combobox")}},
        {QStringLiteral("select"), {QStringLiteral("combobox")}},
        {QStringLiteral("menu"), {QStringLiteral("menuitem")}},
        {QStringLiteral("option"), {QStringLiteral("menuitem"), QStringLiteral("radio")}},
        {QStringLiteral("toggle"), {QStringLiteral("checkbox"), QStringLiteral("button")}},
        {QStringLiteral("switch"), {QStringLiteral("checkbox"), QStringLiteral("button")}},
        {QStringLiteral("grid"), {QStringLiteral("table")}},
    };
    return aliases.value(w).contains(role);
}

} // namespace ui_model_detail

QList<Node> collect_nodes(QWidget* root, const WalkOptions& opt, bool* truncated) {
    WalkCtx ctx;
    ctx.opt = &opt;
    ctx.cap = opt.query.isEmpty() ? std::max(1, opt.max_nodes) : kMaxQueryWalk;
    if (root) {
        const int d = std::max(0, opt.base_depth);
        if (qobject_cast<QMenu*>(root) || is_leaf_control(root)) {
            walk(root, d, ctx); // the root is itself an element (a menu, a table used as a scope)
        } else {
            for (QWidget* child : ordered_children(root))
                walk(child, d, ctx);
        }
    }

    QList<Node> out;
    out.reserve(ctx.nodes.size());
    for (Node& n : ctx.nodes) {
        // A label already used as a field's name is not repeated as text.
        if ((n.role == QLatin1String("text") || n.role == QLatin1String("heading")) && n.widget &&
            ctx.consumed_labels.contains(n.widget.data()))
            continue;
        // A metric tile — a container showing exactly two labels, a caption and a
        // value in either order ("TOTAL REVENUE" / "$245.1B") — reads as one line,
        // as a person reads it: half the lines on the dashboards and research tabs
        // this terminal is full of. Only a lone pair: in a busier container the
        // label above a value may be a heading, not its caption.
        if (!out.isEmpty() && n.role == QLatin1String("text") && n.ref.isEmpty() && n.widget) {
            Node& prev = out.last();
            if (prev.role == QLatin1String("text") && prev.ref.isEmpty() && prev.widget && prev.depth == n.depth &&
                !prev.name.contains(QLatin1String(": ")) && lone_label_pair(prev.widget, n.widget)) {
                auto join = [](const QString& caption, const QString& value) {
                    return caption + (caption.endsWith(QLatin1Char(':')) ? QStringLiteral(" ") : QStringLiteral(": ")) +
                           value;
                };
                if (caption_like(prev.name) && value_like(n.name)) {
                    prev.name = join(prev.name, n.name);
                    continue;
                }
                if (value_like(prev.name) && caption_like(n.name)) {
                    prev.name = join(n.name, prev.name);
                    continue;
                }
            }
        }
        out.append(std::move(n));
    }

    if (!opt.query.isEmpty()) {
        const QString q = opt.query.join(QLatin1Char(' '));
        QList<QPair<int, Node>> scored;
        for (Node& n : out) {
            const QString hay = (n.role + QLatin1Char(' ') + n.name + QLatin1Char(' ') + n.value + QLatin1Char(' ') +
                                 n.states.join(QLatin1Char(' ')) + QLatin1Char(' ') + n.headers.join(QLatin1Char(' ')))
                                    .toLower();
            int score = 0;
            if (words_match(hay, opt.query))
                score = 10 + name_score(n.name.toLower(), q);
            // Table / list rows holding the words: the first few listed (with the
            // refs of controls in them), every match counted, the scan bounded.
            if (auto* view = qobject_cast<QAbstractItemView*>(n.widget.data())) {
                const QList<int> cols = visible_columns(view);
                const QList<int> shown_cols = cols.mid(0, kPreviewCols);
                int scanned = 0;
                for_each_view_row(view, 0, [&](int r, const QModelIndex& first) {
                    if (++scanned > kMaxRowScan) {
                        n.scan_capped = true;
                        return false;
                    }
                    const QString text = index_row_text(view, first, cols, 40);
                    if (!words_match(text.toLower(), opt.query))
                        return true;
                    if (++n.matched_total <= kQueryRowsShown) {
                        n.matched_rows << QStringLiteral("%1:r%2: %3")
                                              .arg(n.ref)
                                              .arg(r)
                                              .arg(
                                                  common::clip(index_row_text(view, first, shown_cols, 40, true), 260));
                        Refs::instance().anchor_row(view, n.ref, r, first);
                    }
                    return true;
                });
                if (n.matched_total > 0)
                    score = std::max(score, 20 + std::min(n.matched_total, 50));
            }
            if (score > 0)
                scored.append({score, std::move(n)});
        }
        std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        QList<Node> filtered;
        for (auto& s : scored) {
            if (static_cast<int>(filtered.size()) >= opt.max_nodes) {
                ctx.truncated = true;
                break;
            }
            s.second.depth = 0;
            filtered.append(std::move(s.second));
        }
        out = std::move(filtered);
    }
    if (truncated)
        *truncated = ctx.truncated;
    return out;
}

QStringList format_node(const Node& n, Detail detail) {
    QStringList lines;
    const QString indent(qsizetype{std::min(n.depth, 6)} * 2, QLatin1Char(' '));
    QString line = indent;
    if (!n.ref.isEmpty())
        line += n.ref + QLatin1Char(' ');
    line += n.role;
    const int name_chars =
        n.role == QLatin1String("text") ? (detail == Detail::Full ? 600 : 160) : (detail == Detail::Full ? 200 : 90);
    if (!n.name.isEmpty())
        line += QLatin1Char(' ') + common::quoted(common::clip(n.name, name_chars));
    if (!n.value.isEmpty() || n.role == QLatin1String("textbox"))
        line += QStringLiteral(" = ") + common::quoted(n.value);
    if (n.rows >= 0) {
        line += n.rows_capped ? QStringLiteral(" %1+ rows").arg(n.rows) : QStringLiteral(" %1 rows").arg(n.rows);
        if (n.cols > 1)
            line += QStringLiteral(" × %1 cols").arg(n.cols);
        if (!n.headers.isEmpty())
            line += QStringLiteral(": ") + common::clip(n.headers.join(QStringLiteral(" | ")), 200);
    }
    if (!n.states.isEmpty())
        line += QStringLiteral(" [") + n.states.join(QStringLiteral(", ")) + QLatin1Char(']');
    lines << line;
    const QString row_indent = indent + QStringLiteral("   ");
    if (!n.matched_rows.isEmpty()) {
        for (const QString& r : n.matched_rows)
            lines << row_indent + r;
        if (n.matched_total > n.matched_rows.size())
            lines << row_indent + QStringLiteral("… %1 more matching rows — ui_read(ref=\"%2\", query=…) pages them")
                                      .arg(n.matched_total - n.matched_rows.size())
                                      .arg(n.ref);
        if (n.scan_capped)
            lines << row_indent + QStringLiteral("(searched the first %1 rows only)").arg(kMaxRowScan);
    } else {
        for (const QString& r : n.row_lines)
            lines << row_indent + r;
        if (n.rows > n.row_lines.size() && !n.row_lines.isEmpty())
            lines << row_indent +
                         QStringLiteral("… %1 more — ui_read(ref=\"%2\")").arg(n.rows - n.row_lines.size()).arg(n.ref);
    }
    if (n.role == QLatin1String("text") && n.name.size() > name_chars && !n.ref.isEmpty())
        lines.last() += QStringLiteral(" (full text: ui_read(ref=\"%1\"))").arg(n.ref);
    return lines;
}

// ═════════════════════════════════════════════════════════════════════════════
// Windows, panels, dialogs
// ═════════════════════════════════════════════════════════════════════════════

QList<WindowFrame*> frames() {
    QList<WindowFrame*> out;
    for (WindowFrame* f : WindowRegistry::instance().frames()) {
        if (f)
            out << f;
    }
    return out;
}

WindowFrame* frame_of(QWidget* w) {
    for (QWidget* p = w; p; p = p->parentWidget()) {
        if (auto* f = qobject_cast<WindowFrame*>(p))
            return f;
    }
    return nullptr;
}

WindowFrame* active_frame() {
    if (QWidget* aw = QApplication::activeWindow()) {
        if (WindowFrame* f = frame_of(aw))
            return f;
    }
    const QList<WindowFrame*> all = frames();
    for (WindowFrame* f : all) {
        if (f->isVisible() && !f->isMinimized())
            return f;
    }
    return all.value(0, nullptr);
}

ads::CDockWidget* dock_of(QWidget* w) {
    for (QWidget* p = w; p; p = p->parentWidget()) {
        if (auto* d = qobject_cast<ads::CDockWidget*>(p))
            return d;
        if (p->isWindow())
            break;
    }
    return nullptr;
}

ads::CDockWidget* focused_dock(WindowFrame* frame) {
    if (!frame || !frame->dock_manager())
        return nullptr;
    if (QWidget* fw = QApplication::focusWidget()) {
        if (frame_of(fw) == frame) {
            if (ads::CDockWidget* d = dock_of(fw); d && !d->isClosed())
                return d;
        }
    }
    if (DockScreenRouter* router = frame->dock_router()) {
        if (ads::CDockWidget* d = router->find_dock_widget(router->current_screen_id()); d && !d->isClosed())
            return d;
    }
    for (ads::CDockWidget* d : frame->dock_manager()->openedDockWidgets()) {
        if (d->isVisible())
            return d;
    }
    return nullptr;
}

QWidget* screen_widget_of(ads::CDockWidget* dock) {
    if (!dock)
        return nullptr;
    if (WindowFrame* f = frame_of(dock)) {
        if (DockScreenRouter* router = f->dock_router()) {
            if (QWidget* screen = router->screen_widget(dock->objectName()))
                return screen;
        }
    }
    return dock->widget();
}

QList<PanelInfo> panels_of(WindowFrame* frame) {
    QList<PanelInfo> out;
    if (!frame || !frame->dock_manager())
        return out;
    ads::CDockWidget* focused = focused_dock(frame);
    for (ads::CDockWidget* d : frame->dock_manager()->openedDockWidgets()) {
        PanelInfo p;
        p.dock = d;
        p.screen_id = d->objectName();
        p.title = d->windowTitle().trimmed();
        if (p.title.isEmpty())
            p.title = DockScreenRouter::title_for_id(p.screen_id.section(QLatin1Char('#'), 0, 0));
        p.visible = d->isVisible();
        p.focused = d == focused;
        out << p;
    }
    return out;
}

QList<QWidget*> open_dialogs() {
    QList<QWidget*> out;
    QWidget* modal = QApplication::activeModalWidget();
    if (modal && modal->isVisible() && !is_hidden_from_agents(modal))
        out << modal;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (!w->isVisible() || out.contains(w) || is_hidden_from_agents(w) || qobject_cast<QMenu*>(w))
            continue;
        if (qobject_cast<QDialog*>(w) || w->windowType() == Qt::Dialog)
            out << w;
    }
    return out;
}

QWidget* blocking_dialog() {
    QWidget* modal = QApplication::activeModalWidget();
    if (modal && modal->isVisible() && !is_hidden_from_agents(modal))
        return modal;
    return nullptr;
}

QWidget* open_popup() {
    QWidget* p = QApplication::activePopupWidget();
    if (p && p->isVisible() && !is_hidden_from_agents(p))
        return p;
    return nullptr;
}

QString window_title_of(QWidget* top) {
    if (!top)
        return {};
    if (auto* mb = qobject_cast<QMessageBox*>(top)) {
        const QString t = mb->windowTitle().trimmed();
        return t.isEmpty() ? common::clip(plain_text(mb->text()), 60) : t;
    }
    QString t = top->windowTitle().trimmed();
    if (t.isEmpty())
        t = humanize_object_name(top->objectName());
    return t.isEmpty() ? QStringLiteral("(untitled)") : t;
}

ads::CDockWidget* find_open_panel(const QString& screen) {
    const QString s = screen.trimmed().toLower();
    if (s.isEmpty())
        return nullptr;
    QList<WindowFrame*> order = frames();
    if (WindowFrame* af = active_frame()) {
        order.removeAll(af);
        order.prepend(af);
    }
    ads::CDockWidget* partial = nullptr;
    for (WindowFrame* f : order) {
        for (const PanelInfo& p : panels_of(f)) {
            const QString id = p.screen_id.toLower();
            const QString base = id.section(QLatin1Char('#'), 0, 0);
            const QString title = p.title.toLower();
            if (id == s || base == s || title == s || DockScreenRouter::title_for_id(base).toLower() == s)
                return p.dock;
            if (!partial && (title.contains(s) || base.contains(s)))
                partial = p.dock;
        }
    }
    return partial;
}

QString location_label(QWidget* w) {
    if (!w)
        return {};
    QWidget* top = w->window();
    if (top && (qobject_cast<QDialog*>(top) || top->windowType() == Qt::Dialog))
        return Refs::instance().ref_for(top, RefKind::Dialog) + QLatin1Char(' ') + common::quoted(window_title_of(top));
    if (auto* menu = qobject_cast<QMenu*>(top))
        return Refs::instance().ref_for(menu, RefKind::Menu) + QStringLiteral(" menu");
    if (ads::CDockWidget* d = dock_of(w)) {
        QString title = d->windowTitle().trimmed();
        if (title.isEmpty())
            title = DockScreenRouter::title_for_id(d->objectName().section(QLatin1Char('#'), 0, 0));
        return Refs::instance().ref_for(d, RefKind::Panel) + QLatin1Char(' ') + common::quoted(title);
    }
    if (WindowFrame* f = frame_of(w))
        return Refs::instance().ref_for(f, RefKind::Window) + QLatin1Char(' ') + common::quoted(window_title_of(f));
    return common::quoted(window_title_of(top));
}

QWidget* resolve_scope(const QString& target_raw, QString* label, QString* error) {
    const QString target = target_raw.trimmed();
    auto fail = [error](const QString& why) -> QWidget* {
        if (error)
            *error = why;
        return nullptr;
    };
    if (target.isEmpty() || target.compare(QLatin1String("window"), Qt::CaseInsensitive) == 0) {
        WindowFrame* f = active_frame();
        if (!f)
            return fail(QStringLiteral("No Fincept Terminal window is open."));
        if (label)
            *label = location_label(f);
        return f;
    }
    if (target.compare(QLatin1String("dialog"), Qt::CaseInsensitive) == 0 ||
        target.compare(QLatin1String("menu"), Qt::CaseInsensitive) == 0) {
        QWidget* d = open_popup();
        if (!d)
            d = blocking_dialog();
        if (!d)
            d = open_dialogs().value(0, nullptr);
        if (!d)
            return fail(QStringLiteral("No dialog or menu is open."));
        if (label)
            *label = qobject_cast<QMenu*>(d) ? Refs::instance().ref_for(d, RefKind::Menu) + QStringLiteral(" menu")
                                             : location_label(d);
        return d;
    }
    if (auto r = Refs::instance().resolve(target)) {
        if (r->dock) {
            if (label)
                *label = location_label(r->dock);
            return r->dock;
        }
        if (r->widget) {
            if (label)
                *label = r->kind == RefKind::Element ? r->ref : location_label(r->widget);
            return r->widget;
        }
        return fail(QStringLiteral("%1 is a menu item, not something with contents.").arg(target));
    }
    static const QRegularExpression ref_rx(QStringLiteral("^[wpdme]\\d+(:r\\d+)?$"));
    if (ref_rx.match(target).hasMatch())
        return fail(QStringLiteral("%1 is gone (closed or rebuilt) — take a fresh ui_snapshot.").arg(target));
    if (ads::CDockWidget* d = find_open_panel(target)) {
        if (label)
            *label = location_label(d);
        return d;
    }
    return fail(QStringLiteral("\"%1\" is not open. ui_overview lists what is open; ui_open(screen=\"%1\") opens it.")
                    .arg(target));
}

// ═════════════════════════════════════════════════════════════════════════════
// Search by words
// ═════════════════════════════════════════════════════════════════════════════

QList<NameMatch> find_by_name(QWidget* scope, const QString& name, const QString& role) {
    QList<NameMatch> out;
    const QString q = name.trimmed().toLower();
    if (q.isEmpty())
        return out;
    QList<QWidget*> roots;
    if (QWidget* popup = open_popup())
        roots << popup; // an open menu is what a person would be looking at
    if (scope)
        roots << scope;
    for (QWidget* root : roots) {
        WalkOptions opt;
        opt.detail = Detail::Normal;
        opt.max_nodes = kMaxQueryWalk;
        opt.cell_widgets = true; // "Cancel" on a blotter row is a button too
        bool truncated = false;
        const QList<Node> nodes = collect_nodes(root, opt, &truncated);
        for (const Node& n : nodes) {
            if (!n.interactive && !needs_ref(n.role))
                continue;
            if (!role_matches(role, n.role))
                continue;
            int score = name_score(n.name.toLower(), q);
            if (score == 0 && !n.value.isEmpty() && n.value.toLower() == q)
                score = 20;
            if (score == 0)
                continue;
            if (n.states.contains(QStringLiteral("disabled")))
                score -= 15;
            NameMatch m;
            m.widget = n.widget;
            m.action = n.action;
            m.sub = n.sub;
            m.score = score;
            m.ref = n.ref;
            m.role = n.role;
            m.name = n.name;
            out << m;
        }
        if (!out.isEmpty())
            break; // matches in the popup win over the window behind it
    }
    std::stable_sort(out.begin(), out.end(), [](const NameMatch& a, const NameMatch& b) { return a.score > b.score; });
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// Busy state
// ═════════════════════════════════════════════════════════════════════════════

QString BusyState::describe() const {
    QStringList parts;
    if (python > 0)
        parts << QStringLiteral("%1 Python script%2 running")
                     .arg(python)
                     .arg(python == 1 ? QString() : QStringLiteral("s"));
    if (indicators > 0)
        parts << QStringLiteral("%1 loading indicator%2 showing")
                     .arg(indicators)
                     .arg(indicators == 1 ? QString() : QStringLiteral("s"));
    return parts.join(QStringLiteral(", "));
}

BusyState busy_state(QWidget* scope) {
    BusyState s;
    s.python = python::PythonRunner::instance().pending_count();
    QList<QWidget*> roots;
    if (scope)
        roots << scope;
    else if (WindowFrame* f = active_frame())
        roots << f;
    for (QWidget* d : open_dialogs())
        roots << d;
    for (QWidget* root : roots) {
        for (QWidget* w : root->findChildren<QWidget*>()) {
            if (!w->isVisible())
                continue;
            const QByteArray cls = w->metaObject()->className();
            if (cls.endsWith("LoadingOverlay")) {
                ++s.indicators;
                continue;
            }
            if (auto* pb = qobject_cast<QProgressBar*>(w); pb && pb->minimum() == pb->maximum()) {
                ++s.indicators;
                continue;
            }
            if (auto* l = qobject_cast<QLabel*>(w)) {
                const QString t = l->text().trimmed().toLower();
                if (t.size() <= 60 &&
                    (t.startsWith(QLatin1String("loading")) || t.startsWith(QLatin1String("fetching")) ||
                     t.startsWith(QLatin1String("please wait")) || t.startsWith(QLatin1String("connecting"))))
                    ++s.indicators;
            }
        }
    }
    return s;
}

// ═════════════════════════════════════════════════════════════════════════════
// Web pages
// ═════════════════════════════════════════════════════════════════════════════

namespace ui_web_detail {

// One script for every page question; the caller appends "(<spec JSON>)".
// It lists the page's controls the same way each time, so control N of a read
// is control N when it is acted on (and is recognised by role + name if the
// page changed). Password fields are never read: no value, only "secret".
const char* const kWebPageJs = R"JS((function (spec) {
  try {
    var SEL = 'a[href],button,input:not([type=hidden]),select,textarea,summary,[role=button],[role=link],' +
      '[role=tab],[role=menuitem],[role=menuitemcheckbox],[role=menuitemradio],[role=option],[role=checkbox],' +
      '[role=radio],[role=switch],[role=combobox],[role=textbox],[role=searchbox],[role=slider],' +
      '[role=spinbutton],[contenteditable=""],[contenteditable=true],[onclick],[tabindex]:not([tabindex="-1"])';
    var OWNER = 'a[href],button,summary,select,textarea,[role=button],[role=link],[role=tab],[role=menuitem],' +
      '[role=option],[role=checkbox],[role=radio],[role=switch]';
    function clip(s, n) {
      s = String(s == null ? '' : s).replace(/\s+/g, ' ').trim();
      return s.length > n ? s.slice(0, n - 1) + '…' : s;
    }
    function textOf(el) { return el.innerText != null ? el.innerText : el.textContent; }
    function secret(el) { return el.tagName === 'INPUT' && String(el.type).toLowerCase() === 'password'; }
    function roleOf(el) {
      var r = el.getAttribute('role');
      if (r) return r;
      var t = el.tagName;
      if (t === 'A') return 'link';
      if (t === 'BUTTON' || t === 'SUMMARY') return 'button';
      if (t === 'SELECT') return 'combobox';
      if (t === 'TEXTAREA') return 'textarea';
      if (t === 'INPUT') {
        var ty = String(el.type || 'text').toLowerCase();
        if (ty === 'checkbox' || ty === 'radio') return ty;
        if (ty === 'range') return 'slider';
        if (ty === 'number') return 'spinbutton';
        if (/^(button|submit|reset|image)$/.test(ty)) return 'button';
        if (/^(date|datetime-local|month|week|time)$/.test(ty)) return 'date';
        return 'textbox';
      }
      if (el.isContentEditable) return 'textbox';
      return 'clickable';
    }
    function nameOf(el) {
      var n = el.getAttribute('aria-label') || '';
      var lb = el.getAttribute('aria-labelledby');
      if (!n && lb) n = lb.split(/\s+/).map(function (id) {
        var x = document.getElementById(id);
        return x ? textOf(x) : '';
      }).join(' ');
      if (!n && el.labels && el.labels.length) n = textOf(el.labels[0]);
      var t = el.tagName;
      if (!n && t !== 'INPUT' && t !== 'SELECT' && t !== 'TEXTAREA') n = textOf(el);
      if (!n && t === 'INPUT' && /^(button|submit|reset)$/i.test(el.type)) n = el.value;
      if (!n) n = el.getAttribute('title') || el.getAttribute('placeholder') || el.getAttribute('alt') ||
        el.getAttribute('name') || '';
      if (!n && el.querySelector) {
        var img = el.querySelector('img[alt],svg title');
        if (img) n = img.getAttribute('alt') || img.textContent;
      }
      return clip(n, 120);
    }
    function valueOf(el) {
      if (secret(el)) return '';
      var t = el.tagName;
      if (t === 'SELECT') { var o = el.options[el.selectedIndex]; return o ? clip(o.text, 120) : ''; }
      if (t === 'TEXTAREA') return clip(el.value, 200);
      if (t === 'INPUT' && !/^(checkbox|radio|button|submit|reset|image|file)$/i.test(el.type)) return clip(el.value, 200);
      if (el.isContentEditable) return clip(textOf(el), 200);
      return '';
    }
    function box(el) {
      var r = el.getBoundingClientRect();
      if (r.width < 1 || r.height < 1) return null;
      var cs = getComputedStyle(el);
      if (cs.visibility === 'hidden' || cs.visibility === 'collapse' || Number(cs.opacity) === 0) return null;
      return r;
    }
    function statesOf(el, r) {
      var s = [];
      if (!r) s.push('hidden');
      else if (r.bottom <= 0 || r.top >= innerHeight || r.right <= 0 || r.left >= innerWidth) s.push('off-screen');
      if (el.disabled || el.getAttribute('aria-disabled') === 'true') s.push('disabled');
      var ty = String(el.type || '').toLowerCase(), ro = el.getAttribute('role') || '';
      if (el.checked === true || el.getAttribute('aria-checked') === 'true' || el.getAttribute('aria-pressed') === 'true')
        s.push('checked');
      else if (ty === 'checkbox' || ty === 'radio' || /^(checkbox|radio|switch|menuitemcheckbox|menuitemradio)$/.test(ro))
        s.push('unchecked');
      if (el.getAttribute('aria-selected') === 'true') s.push('selected');
      var ex = el.getAttribute('aria-expanded');
      if (ex === 'true') s.push('expanded'); else if (ex === 'false') s.push('collapsed');
      if (el.readOnly) s.push('read-only');
      if (el.required) s.push('required');
      if (typeof el.className === 'string' && /(^|\s)(active|selected)(\s|$)/.test(el.className)) s.push('active');
      if (secret(el)) s.push('secret');
      if (el === document.activeElement) s.push('focused');
      return s;
    }
    function list() {
      var all = document.querySelectorAll(SEL), out = [];
      for (var i = 0; i < all.length && out.length < 3000; ++i) {
        var p = all[i].parentElement;
        if (p && p.closest(OWNER)) continue; // a part of the control around it
        out.push(all[i]);
      }
      return out;
    }
    function info(el, at) {
      var r = box(el), o = {i: at, r: roleOf(el), n: nameOf(el), v: valueOf(el), s: statesOf(el, r)};
      if (r) {
        o.x = Math.round(r.left + r.width / 2); o.y = Math.round(r.top + r.height / 2);
        o.w = Math.round(r.width); o.h = Math.round(r.height);
      }
      return o;
    }
    function key(o) { return o.r + '|' + o.n; }
    function wordsOf(el) {
      var w = [nameOf(el), clip(textOf(el), 200), el.getAttribute('title'), el.getAttribute('aria-label'), el.id,
        typeof el.className === 'string' ? el.className : '', el.getAttribute('href'), el.getAttribute('onclick'),
        el.getAttribute('name')];
      if (el.tagName === 'INPUT' && /^(button|submit|reset)$/i.test(el.type)) w.push(el.value);
      return clip(w.filter(Boolean).join(' '), 600);
    }
    function formWords(el) {
      var f = el.form;
      if (!f) return '';
      var w = [f.id, f.getAttribute('name'), f.getAttribute('action')];
      var b = f.querySelectorAll('button,input[type=submit]');
      for (var j = 0; j < b.length && j < 4; ++j) w.push(wordsOf(b[j]));
      return clip(w.filter(Boolean).join(' '), 600);
    }
)JS"
                               R"JS(
    if (spec.mode === 'text') {
      var tb = document.body;
      return JSON.stringify({text: tb ? String(textOf(tb) || '').slice(0, 2000000) : ''});
    }
    if (spec.mode === 'read') {
      var els = list(), cs = [];
      for (var i = 0; i < els.length; ++i) cs.push(info(els[i], i));
      var b = document.body, d = document.documentElement;
      return JSON.stringify({title: document.title, url: location.href, vw: innerWidth, vh: innerHeight,
        sx: Math.round(scrollX), sy: Math.round(scrollY), sw: d ? d.scrollWidth : 0, sh: d ? d.scrollHeight : 0,
        loading: document.readyState !== 'complete', text: b ? String(textOf(b) || '').slice(0, 2000000) : '',
        controls: cs});
    }
    var el = null, at = -1;
    if (spec.mode === 'control') {
      var all2 = list(), c = all2[spec.i];
      if (c && key(info(c, spec.i)) === spec.k) { el = c; at = spec.i; }
      else {
        var hits = [];
        for (var j = 0; j < all2.length; ++j) if (key(info(all2[j], j)) === spec.k) hits.push(j);
        if (hits.length !== 1) return JSON.stringify({error: hits.length ? 'several' : 'gone', count: hits.length});
        at = hits[0]; el = all2[at];
      }
    } else if (spec.mode === 'point') {
      var h = document.elementFromPoint(spec.x, spec.y);
      if (!h) return JSON.stringify({error: 'nothing'});
      el = h.closest(OWNER) || h.closest(SEL) || h;
    } else if (spec.mode === 'focus') {
      el = document.activeElement;
      if (!el || el === document.body || el === document.documentElement) return JSON.stringify({none: true});
    } else {
      return JSON.stringify({error: 'mode'});
    }
    if (spec.reveal && el.scrollIntoView) {
      var rb = box(el);
      if (rb && (rb.top < 0 || rb.bottom > innerHeight || rb.left < 0 || rb.right > innerWidth))
        el.scrollIntoView({block: 'center', inline: 'center'});
    }
    var out = info(el, at);
    out.tag = el.tagName.toLowerCase();
    out.secret = secret(el);
    out.words = wordsOf(el);
    out.form = formWords(el);
    out.editable = !!(el.isContentEditable || el.tagName === 'TEXTAREA' ||
      (el.tagName === 'INPUT' && !/^(checkbox|radio|button|submit|reset|image|file|range|color)$/i.test(el.type)));
    if (out.x !== undefined) {
      var top = document.elementFromPoint(out.x, out.y);
      if (top && top !== el && !el.contains(top) && !top.contains(el)) {
        out.covered = true;
        out.cover = clip(nameOf(top.closest(SEL) || top) || textOf(top), 80);
      }
    }
    if (el.tagName === 'SELECT') {
      out.options = [];
      for (var k = 0; k < el.options.length && k < 200; ++k) out.options.push(clip(el.options[k].text, 80));
    }
    var op = spec.op || '';
    if (op === 'click') { el.click(); out.done = true; }
    else if (op === 'focus') { el.focus(); out.done = true; }
    else if (op === 'select') {
      var pick = -1, want = String(spec.text || '').trim().toLowerCase();
      if (el.tagName === 'SELECT') {
        for (var m = 0; m < el.options.length && pick < 0; ++m)
          if (el.options[m].text.trim().toLowerCase() === want || el.options[m].value.toLowerCase() === want) pick = m;
        for (var m2 = 0; m2 < el.options.length && pick < 0; ++m2)
          if (want && el.options[m2].text.toLowerCase().indexOf(want) >= 0) pick = m2;
      }
      if (pick < 0) out.error = el.tagName === 'SELECT' ? 'no-option' : 'not-select';
      else {
        out.picked = clip(el.options[pick].text, 80);
        if (!spec.dry) {
          el.selectedIndex = pick;
          el.dispatchEvent(new Event('input', {bubbles: true}));
          el.dispatchEvent(new Event('change', {bubbles: true}));
          out.done = true;
        }
      }
    } else if (op === 'scroll') {
      var sc = el;
      while (sc && sc !== document.body && !(sc.scrollHeight > sc.clientHeight + 4 &&
             /(auto|scroll)/.test(getComputedStyle(sc).overflowY))) sc = sc.parentElement;
      var dy = spec.dy || Math.round(innerHeight * 0.9);
      if (sc && sc !== document.body) sc.scrollBy(0, dy); else window.scrollBy(0, dy);
      out.done = true;
    }
    return JSON.stringify(out);
  } catch (e) {
    return JSON.stringify({error: 'script: ' + (e && e.message ? e.message : String(e))});
  }
}))JS";

// A KLineChart page's data, read through its own API (the page world).
const char* const kWebChartJs = R"JS((function (off, lim) {
  try {
    var c = window.chart;
    if (!c || typeof c.getDataList !== 'function') return '';
    var d = c.getDataList() || [], n = d.length, res = {count: n};
    if (n) {
      var lo = Infinity, hi = -Infinity;
      for (var i = 0; i < n; ++i) { if (d[i].low < lo) lo = d[i].low; if (d[i].high > hi) hi = d[i].high; }
      res.low = lo; res.high = hi; res.first = d[0].timestamp;
      var l = d[n - 1];
      res.last = [l.timestamp, l.open, l.high, l.low, l.close, l.volume];
    }
    try {
      var vr = c.getVisibleRange();
      res.from = vr.realFrom != null ? vr.realFrom : vr.from;
      res.to = vr.realTo != null ? vr.realTo : vr.to;
    } catch (e1) {}
    try { res.type = c.getStyles().candle.type; } catch (e2) {}
    var inds = [];
    try {
      var all = c.getIndicatorByPaneId();
      if (all && all.forEach) all.forEach(function (m, pane) {
        if (m && m.forEach) m.forEach(function (ind, name) {
          var r = ind.result || [], last = r.length ? r[r.length - 1] : null, vals = [];
          if (last) (ind.figures || []).forEach(function (f) {
            var v = last[f.key];
            if (typeof v === 'number' && isFinite(v)) vals.push([String(f.title || f.key).replace(/[:\s]+$/, ''), v]);
          });
          inds.push({pane: pane, name: name, params: ind.calcParams || [], last: vals});
        });
      });
    } catch (e3) {}
    res.indicators = inds;
    if (lim > 0 && n) {
      var start = off < 0 ? Math.max(0, n - lim) : Math.min(off, n);
      res.start = start;
      res.rows = d.slice(start, start + lim).map(function (k) {
        return [k.timestamp, k.open, k.high, k.low, k.close, k.volume];
      });
    }
    return JSON.stringify(res);
  } catch (e) {
    return JSON.stringify({error: String(e && e.message ? e.message : e)});
  }
}))JS";

} // namespace ui_web_detail

QWidget* web_view_of(QWidget* w) {
#ifdef HAS_QT_WEBENGINE
    if (!w)
        return nullptr;
    // The view itself, or its render child (what a click at a point lands on).
    for (QWidget* p = w; p; p = p->parentWidget()) {
        if (qobject_cast<QWebEngineView*>(p))
            return p;
        if (p->isWindow())
            break;
    }
    // A host that only wraps a view (KLineChartWidget) — never a panel that
    // merely contains one somewhere deeper.
    auto* view = w->findChild<QWebEngineView*>(QString(), Qt::FindDirectChildrenOnly);
    if (!view)
        return nullptr;
    if (w->inherits("fincept::ui::KLineChartWidget"))
        return view;
    for (QWidget* c : w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (c != view && c->isVisible())
            return nullptr;
    }
    return view;
#else
    Q_UNUSED(w);
    return nullptr;
#endif
}

QWidget* web_input_widget(QWidget* view) {
    if (!view)
        return nullptr;
    if (QWidget* proxy = view->focusProxy())
        return proxy;
    for (QWidget* c : view->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (c->isVisible())
            return c;
    }
    return view;
}

double web_zoom(QWidget* view) {
#ifdef HAS_QT_WEBENGINE
    if (auto* v = qobject_cast<QWebEngineView*>(view)) {
        const double z = v->zoomFactor();
        return z > 0.0 ? z : 1.0;
    }
#else
    Q_UNUSED(view);
#endif
    return 1.0;
}

void run_web_script(QWidget* view, const QString& script, bool page_world, int timeout_ms,
                    std::function<void(const QJsonObject& result, const QString& error)> done) {
#ifdef HAS_QT_WEBENGINE
    auto* v = qobject_cast<QWebEngineView*>(view);
    if (!v || !v->page()) {
        done({}, QStringLiteral("there is no web page"));
        return;
    }
    // Answered once: by the page, or by the timer when the page never answers
    // (still loading, its renderer died, or the view went away).
    auto answered = std::make_shared<bool>(false);
    auto finish = std::make_shared<std::function<void(const QJsonObject&, const QString&)>>(std::move(done));
    QTimer::singleShot(timeout_ms, QCoreApplication::instance(), [answered, finish, timeout_ms]() {
        if (*answered)
            return;
        *answered = true;
        (*finish)({}, QStringLiteral("the page did not answer within %1 s (still loading, or busy)")
                          .arg(timeout_ms / 1000.0, 0, 'g', 2));
    });
    v->page()->runJavaScript(script, page_world ? QWebEngineScript::MainWorld : QWebEngineScript::ApplicationWorld,
                             [answered, finish](const QVariant& result) {
                                 if (*answered)
                                     return;
                                 *answered = true;
                                 const QString text = result.toString();
                                 if (text.isEmpty()) {
                                     (*finish)({}, QString()); // nothing to report (e.g. no chart on the page)
                                     return;
                                 }
                                 const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
                                 if (!doc.isObject()) {
                                     (*finish)({}, QStringLiteral("the page gave an unreadable answer"));
                                     return;
                                 }
                                 (*finish)(doc.object(), QString());
                             });
#else
    Q_UNUSED(view);
    Q_UNUSED(script);
    Q_UNUSED(page_world);
    Q_UNUSED(timeout_ms);
    done({}, QStringLiteral("this build has no web engine"));
#endif
}

QString web_read_script() {
    return QString::fromUtf8(ui_web_detail::kWebPageJs) + QStringLiteral("({\"mode\":\"read\"})");
}

QString web_text_script() {
    return QString::fromUtf8(ui_web_detail::kWebPageJs) + QStringLiteral("({\"mode\":\"text\"})");
}

QString web_target_script(const QJsonObject& spec) {
    return QString::fromUtf8(ui_web_detail::kWebPageJs) + QLatin1Char('(') +
           QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact)) + QLatin1Char(')');
}

QString web_chart_script(int offset, int limit) {
    return QString::fromUtf8(ui_web_detail::kWebChartJs) + QStringLiteral("(%1, %2)").arg(offset).arg(limit);
}

QString web_control_key(const QJsonObject& control) {
    return control.value(QStringLiteral("r")).toString() + QLatin1Char('|') +
           control.value(QStringLiteral("n")).toString();
}

} // namespace fincept::mcp::core::ui
