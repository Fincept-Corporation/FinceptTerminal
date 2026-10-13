// src/screens/economics/panels/EconPanelBase.cpp
#include "screens/economics/panels/EconPanelBase.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QSpinBox>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

// ── Period / number helpers ───────────────────────────────────────────────────
// Economics providers spell dates every possible way ("2024-03", "2024-Q1", "2024M03", "02 Jan 1975",
// "04-Jan-2011", "2025年09月份", bare years as JSON numbers…). LATEST / CHANGE used to sort those
// strings lexically, which put "02 Jan 1975" after "01 Feb 2024" and made the "latest" value an
// arbitrary row. Everything that orders observations goes through parse_period() instead.
namespace econ_panel_base_detail {

enum class Gran { Unknown, Day, Week, Month, Quarter, Half, Year };

struct Period {
    QDate date;
    Gran gran = Gran::Unknown;
};

int month_from_name(const QString& name) {
    static const char* const kNames[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                         "jul", "aug", "sep", "oct", "nov", "dec"};
    const QString l = name.left(3).toLower();
    for (int i = 0; i < 12; ++i)
        if (l == QLatin1String(kNames[i]))
            return i + 1;
    return 0;
}

int full_year(int y) {
    return y < 100 ? (y < 50 ? 2000 + y : 1900 + y) : y;
}

Period parse_period(const QString& raw) {
    using RE = QRegularExpression;
    static const RE re_day(QStringLiteral("^(\\d{4})-(\\d{1,2})-(\\d{1,2})(?:[T ].*)?$"));
    static const RE re_month(QStringLiteral("^(\\d{4})[-/.](\\d{1,2})$"));
    static const RE re_year(QStringLiteral("^(\\d{4})$"));
    static const RE re_quarter(QStringLiteral("^(\\d{4})[-\\s]?Q([1-4])$"), RE::CaseInsensitiveOption);
    static const RE re_quarter_rev(QStringLiteral("^Q([1-4])[-\\s]?(\\d{4})$"), RE::CaseInsensitiveOption);
    static const RE re_half(QStringLiteral("^(\\d{4})[-\\s]?[SH]([12])$"), RE::CaseInsensitiveOption);
    static const RE re_sdmx_month(QStringLiteral("^(\\d{4})-?M(\\d{1,2})$"), RE::CaseInsensitiveOption);
    static const RE re_week(QStringLiteral("^(\\d{4})-?W(\\d{1,2})$"), RE::CaseInsensitiveOption);
    static const RE re_d_mon_y(QStringLiteral("^(\\d{1,2})[\\s\\-/]([A-Za-z]{3,9})[\\s\\-/,]+(\\d{2,4})$"));
    static const RE re_mon_y(QStringLiteral("^([A-Za-z]{3,9})[\\s\\-/,]+(\\d{4})$"));
    static const RE re_y_mon(QStringLiteral("^(\\d{4})[\\s\\-]([A-Za-z]{3,9})$"));
    static const RE re_slash(QStringLiteral("^(\\d{1,2})/(\\d{1,2})/(\\d{4})$"));
    // 2025年09月份 · 2025年第3季度 / 2025年第1-3季度 (cumulative → ends in Q3) · 2025年
    static const RE re_cn_month(QStringLiteral("^(\\d{4})年(\\d{1,2})月"));
    static const RE re_cn_quarter(QStringLiteral("^(\\d{4})年第(?:\\d-)?(\\d)季度"));
    static const RE re_cn_year(QStringLiteral("^(\\d{4})年$"));

    Period p;
    const QString s = raw.trimmed();
    if (s.size() < 4)
        return p;
    auto set = [&p](int y, int m, int d, Gran g) {
        p.date = QDate(y, m, d);
        p.gran = p.date.isValid() ? g : Gran::Unknown;
    };
    QRegularExpressionMatch m;
    if ((m = re_day.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(), Gran::Day);
    } else if ((m = re_month.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), 1, Gran::Month);
    } else if ((m = re_year.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), 1, 1, Gran::Year);
    } else if ((m = re_quarter.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), (m.captured(2).toInt() - 1) * 3 + 1, 1, Gran::Quarter);
    } else if ((m = re_quarter_rev.match(s)).hasMatch()) {
        set(m.captured(2).toInt(), (m.captured(1).toInt() - 1) * 3 + 1, 1, Gran::Quarter);
    } else if ((m = re_half.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), (m.captured(2).toInt() - 1) * 6 + 1, 1, Gran::Half);
    } else if ((m = re_sdmx_month.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), 1, Gran::Month);
    } else if ((m = re_week.match(s)).hasMatch()) {
        // ISO week: week 1 is the week holding 4 January; weeks start on Monday.
        const QDate jan4(m.captured(1).toInt(), 1, 4);
        const QDate monday = jan4.addDays(1 - jan4.dayOfWeek());
        p.date = monday.addDays((m.captured(2).toInt() - 1) * 7);
        p.gran = p.date.isValid() ? Gran::Week : Gran::Unknown;
    } else if ((m = re_d_mon_y.match(s)).hasMatch()) {
        const int mon = month_from_name(m.captured(2));
        if (mon > 0)
            set(full_year(m.captured(3).toInt()), mon, m.captured(1).toInt(), Gran::Day);
    } else if ((m = re_mon_y.match(s)).hasMatch()) {
        const int mon = month_from_name(m.captured(1));
        if (mon > 0)
            set(m.captured(2).toInt(), mon, 1, Gran::Month);
    } else if ((m = re_y_mon.match(s)).hasMatch()) {
        const int mon = month_from_name(m.captured(2));
        if (mon > 0)
            set(m.captured(1).toInt(), mon, 1, Gran::Month);
    } else if ((m = re_slash.match(s)).hasMatch()) {
        // Day-first unless that is impossible (US month-first "12/31/2024").
        const int a = m.captured(1).toInt();
        const int b = m.captured(2).toInt();
        if (b > 12)
            set(m.captured(3).toInt(), a, b, Gran::Day);
        else
            set(m.captured(3).toInt(), b, a, Gran::Day);
    } else if ((m = re_cn_month.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), 1, Gran::Month);
    } else if ((m = re_cn_quarter.match(s)).hasMatch()) {
        const int q = m.captured(2).toInt();
        if (q >= 1 && q <= 4)
            set(m.captured(1).toInt(), (q - 1) * 3 + 1, 1, Gran::Quarter);
    } else if ((m = re_cn_year.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), 1, 1, Gran::Year);
    }
    return p;
}

/// The period text of a date cell — JSON numbers (WTO "Year", IMF "year") become "2024", and bare
/// short years are zero-padded so "999" can never sort after "1999".
QString period_text(const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (d == std::floor(d) && std::abs(d) < 1e9)
            return QString::number(static_cast<qint64>(d)).rightJustified(4, QLatin1Char('0'));
        return QString::number(d, 'g', 10);
    }
    QString s = v.toString().trimmed();
    bool is_int = false;
    const int y = s.toInt(&is_int);
    if (is_int && s.size() < 4 && y >= 0)
        s = s.rightJustified(4, QLatin1Char('0'));
    return s;
}

bool to_number(const QJsonValue& v, double* out) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (!std::isfinite(d))
            return false;
        *out = d;
        return true;
    }
    if (!v.isString())
        return false;
    QString s = v.toString().trimmed();
    if (s.endsWith(QLatin1Char('%')))
        s.chop(1);
    static const QRegularExpression kGrouped(QStringLiteral("^-?\\d{1,3}(,\\d{3})+(\\.\\d+)?$"));
    if (kGrouped.match(s).hasMatch())
        s.remove(QLatin1Char(','));
    if (s.isEmpty())
        return false;
    bool ok = false;
    const double d = s.toDouble(&ok);
    if (!ok || !std::isfinite(d))
        return false;
    *out = d;
    return true;
}

const QStringList& date_keys() {
    static const QStringList kKeys = {
        QStringLiteral("date"),         QStringLiteral("Date"),           QStringLiteral("DATE"),
        QStringLiteral("period"),       QStringLiteral("Period"),         QStringLiteral("TIME_PERIOD"),
        QStringLiteral("time_period"),  QStringLiteral("obs_date"),       QStringLiteral("observation_date"),
        QStringLiteral("record_date"),  QStringLiteral("ref_date"),       QStringLiteral("report_date"),
        QStringLiteral("year"),         QStringLiteral("Year"),           QStringLiteral("YEAR"),
        QStringLiteral("month"),        QStringLiteral("week"),
        QStringLiteral("time"),         QStringLiteral("Time"),           QStringLiteral("event_datetime"),
        QStringLiteral("月份"), QStringLiteral("季度"),   QStringLiteral("日期"),
        QStringLiteral("年份"),
    };
    return kKeys;
}

bool is_date_key(const QString& key) {
    return date_keys().contains(key);
}

const QStringList& value_keys() {
    static const QStringList kKeys = {QStringLiteral("value"),     QStringLiteral("Value"),
                                      QStringLiteral("VALUE"),     QStringLiteral("val"),
                                      QStringLiteral("OBS_VALUE"), QStringLiteral("obs_value"),
                                      QStringLiteral("amount"),    QStringLiteral("rate")};
    return kKeys;
}

bool is_id_like(const QString& key) {
    const QString l = key.toLower();
    return l == QLatin1String("id") || l.endsWith(QLatin1String("_id")) || l.endsWith(QLatin1String("code")) ||
           l == QLatin1String("series") || l == QLatin1String("symbol");
}

Gran gran_from_text(const QString& text) {
    const QString l = text.trimmed().toLower();
    if (l.isEmpty())
        return Gran::Unknown;
    if (l == QLatin1String("d") || l == QLatin1String("b") || l.startsWith(QLatin1String("daily")) ||
        l.contains(QLatin1String("business day")))
        return Gran::Day;
    if (l == QLatin1String("w") || l.startsWith(QLatin1String("week")))
        return Gran::Week;
    if (l == QLatin1String("m") || l.startsWith(QLatin1String("month")))
        return Gran::Month;
    if (l == QLatin1String("q") || l.startsWith(QLatin1String("quarter")))
        return Gran::Quarter;
    if (l == QLatin1String("s") || l == QLatin1String("h") || l.startsWith(QLatin1String("semi")) ||
        l.startsWith(QLatin1String("half")))
        return Gran::Half;
    if (l == QLatin1String("a") || l == QLatin1String("y") || l.startsWith(QLatin1String("annual")) ||
        l.startsWith(QLatin1String("year")))
        return Gran::Year;
    return Gran::Unknown;
}

/// pts must be sorted ascending.
Gran infer_gran(const QVector<Period>& pts) {
    if (pts.isEmpty())
        return Gran::Unknown;
    // A uniform period spelling coarser than a day decides it ("2024-03", "2024-Q1", "2024").
    Gran common = pts.first().gran;
    for (const auto& p : pts) {
        if (p.gran != common) {
            common = Gran::Unknown;
            break;
        }
    }
    if (common != Gran::Unknown && common != Gran::Day)
        return common;
    // Full dates: classify the median spacing of the most recent observations.
    QVector<qint64> gaps;
    for (qsizetype i = std::max<qsizetype>(1, pts.size() - 25); i < pts.size(); ++i) {
        const qint64 g = pts[i - 1].date.daysTo(pts[i].date);
        if (g > 0)
            gaps.append(g);
    }
    if (gaps.isEmpty())
        return Gran::Unknown;
    std::sort(gaps.begin(), gaps.end());
    const qint64 med = gaps[gaps.size() / 2];
    if (med <= 4)
        return Gran::Day;
    if (med <= 10)
        return Gran::Week;
    if (med <= 45)
        return Gran::Month;
    if (med <= 120)
        return Gran::Quarter;
    if (med <= 240)
        return Gran::Half;
    return Gran::Year;
}

QString gran_name(Gran g) {
    switch (g) {
        case Gran::Day:
            return EconPanelBase::tr("Daily");
        case Gran::Week:
            return EconPanelBase::tr("Weekly");
        case Gran::Month:
            return EconPanelBase::tr("Monthly");
        case Gran::Quarter:
            return EconPanelBase::tr("Quarterly");
        case Gran::Half:
            return EconPanelBase::tr("Semiannual");
        case Gran::Year:
            return EconPanelBase::tr("Annual");
        case Gran::Unknown:
            break;
    }
    return {};
}

/// Period-over-period tag for the CHANGE card.
QString change_tag(Gran g) {
    switch (g) {
        case Gran::Day:
            return EconPanelBase::tr("DoD");
        case Gran::Week:
            return EconPanelBase::tr("WoW");
        case Gran::Month:
            return EconPanelBase::tr("MoM");
        case Gran::Quarter:
            return EconPanelBase::tr("QoQ");
        case Gran::Half:
            return EconPanelBase::tr("HoH");
        case Gran::Year:
            return EconPanelBase::tr("YoY");
        case Gran::Unknown:
            break;
    }
    return {};
}

/// Does the description say the values are percentages / rates? Rates are compared in percentage
/// points: a policy rate moving 5.25 → 5.50 is +0.25 pp, not "+4.76%".
bool text_says_percent(const QString& text) {
    const QString l = text.toLower();
    if (l.contains(QLatin1String("per 1,000")) || l.contains(QLatin1String("per 1000")) ||
        l.contains(QLatin1String("per 100,000")) || l.contains(QLatin1String("per 100 ")))
        return false; // demographic "rates" per 1,000 people are not percentages
    if (l.contains(QLatin1Char('%')) || l.contains(QLatin1String("percent")) || l.contains(QLatin1String("per cent")) ||
        l.contains(QLatin1String("_pct")) || l.contains(QLatin1String("yield")))
        return true;
    static const QRegularExpression kRateWord(QStringLiteral("\\brates?\\b"));
    return kRateWord.match(l).hasMatch() && !l.contains(QLatin1String("exchange")) && !l.contains(QLatin1String("fx")) &&
           !l.contains(QLatin1Char('/'));
}

bool text_says_bp(const QString& units) {
    const QString l = units.trimmed().toLower();
    return l == QLatin1String("bp") || l == QLatin1String("bps") || l.contains(QLatin1String("basis point"));
}

QString signed_fixed(double v, int decimals) {
    const double eps = 0.5 * std::pow(10.0, -decimals);
    if (std::abs(v) < eps)
        v = 0.0;
    return (v > 0 ? QStringLiteral("+") : QString()) + QString::number(v, 'f', decimals);
}

/// Sets a change card: percentage points for rates, basis points for bp series, relative % for
/// positive levels, and an absolute difference when either value is ≤ 0 (a relative change of a
/// balance that crosses zero is meaningless).
void set_change(QLabel* val, QLabel* sub, double now, double then, bool pct, bool bp, const QString& sub_text) {
    if (!val)
        return;
    double sign = now - then;
    QString text;
    if (pct) {
        text = signed_fixed(now - then, 2) + EconPanelBase::tr(" pp");
    } else if (bp) {
        text = signed_fixed(now - then, 1) + EconPanelBase::tr(" bp");
    } else if (then > 0.0 && now > 0.0) {
        sign = (now - then) / then * 100.0;
        text = signed_fixed(sign, 2) + QStringLiteral("%");
    } else {
        text = (sign > 0 ? QStringLiteral("+") : QString()) + EconPanelBase::format_number(sign);
    }
    val->setText(text);
    const char* name = sign > 0 ? "econStatPos" : (sign < 0 ? "econStatNeg" : "econStatVal");
    if (val->objectName() != QLatin1String(name)) {
        val->setObjectName(QLatin1String(name));
        val->style()->unpolish(val);
        val->style()->polish(val);
    }
    if (sub)
        sub->setText(sub_text);
}

QString cell_text(const QJsonValue& v) {
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isNull() || v.isUndefined())
        return {};
    return v.toString();
}

// ── Generic input persistence ─────────────────────────────────────────────────
constexpr const char* kInputsKey = "inputs";
constexpr const char* kInputsSigKey = "inputs_sig";

enum class InputKind { Combo, List, Toggle, Text, Other };

InputKind kind_of(QWidget* w) {
    if (qobject_cast<QComboBox*>(w))
        return InputKind::Combo;
    if (qobject_cast<QListWidget*>(w))
        return InputKind::List;
    if (qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QCheckBox*>(w))
        return InputKind::Toggle;
    if (qobject_cast<QLineEdit*>(w))
        return InputKind::Text;
    return InputKind::Other;
}

/// Item data worth matching on (an empty string marks "custom" entries, e.g. "-- Custom series --").
bool usable_item_data(const QVariant& d) {
    return d.isValid() && !d.isNull() && !(d.typeId() == QMetaType::QString && d.toString().isEmpty());
}

QVariant read_input(QWidget* w) {
    if (auto* cb = qobject_cast<QComboBox*>(w)) {
        if (cb->currentIndex() < 0 && !cb->isEditable())
            return {};
        // The raw item data (not its string form): findData() compares QVariants, and the int data
        // of e.g. a "10 Years" item would never equal the string "10".
        return QVariantMap{{QStringLiteral("d"), cb->currentData()}, {QStringLiteral("t"), cb->currentText()}};
    }
    if (auto* lw = qobject_cast<QListWidget*>(w)) {
        const QListWidgetItem* it = lw->currentItem();
        if (!it)
            return {};
        return QVariantMap{{QStringLiteral("d"), it->data(Qt::UserRole).toString()}, {QStringLiteral("t"), it->text()}};
    }
    if (auto* dt = qobject_cast<QDateTimeEdit*>(w))
        return dt->dateTime().toString(Qt::ISODate);
    if (auto* sb = qobject_cast<QSpinBox*>(w))
        return sb->value();
    if (auto* ds = qobject_cast<QDoubleSpinBox*>(w))
        return ds->value();
    if (auto* ck = qobject_cast<QCheckBox*>(w))
        return ck->isChecked();
    if (auto* le = qobject_cast<QLineEdit*>(w))
        return le->text();
    return {};
}

/// Applies a saved value. Returns false when the target is a combo/list that has no items yet (filled
/// asynchronously) — the caller keeps the value pending. A value that no longer exists in a populated
/// widget is dropped (returns true) rather than retried forever.
bool apply_input(QWidget* w, const QVariant& v) {
    if (auto* cb = qobject_cast<QComboBox*>(w)) {
        const QVariantMap m = v.toMap();
        const QVariant d = m.value(QStringLiteral("d"));
        const QString t = m.value(QStringLiteral("t")).toString();
        int idx = usable_item_data(d) ? cb->findData(d) : -1;
        if (idx < 0 && !t.isEmpty())
            idx = cb->findText(t);
        if (idx >= 0) {
            cb->setCurrentIndex(idx);
            return true;
        }
        if (cb->isEditable() && !t.isEmpty()) {
            cb->setEditText(t);
            return true;
        }
        return cb->count() > 0;
    }
    if (auto* lw = qobject_cast<QListWidget*>(w)) {
        const QVariantMap m = v.toMap();
        const QString d = m.value(QStringLiteral("d")).toString();
        const QString t = m.value(QStringLiteral("t")).toString();
        for (int i = 0; i < lw->count(); ++i) {
            QListWidgetItem* it = lw->item(i);
            const bool hit = d.isEmpty() ? it->text() == t : it->data(Qt::UserRole).toString() == d;
            if (hit) {
                lw->setCurrentItem(it);
                lw->scrollToItem(it);
                return true;
            }
        }
        return lw->count() > 0;
    }
    if (auto* dt = qobject_cast<QDateTimeEdit*>(w)) {
        const QDateTime when = QDateTime::fromString(v.toString(), Qt::ISODate);
        if (when.isValid())
            dt->setDateTime(when);
        return true;
    }
    if (auto* sb = qobject_cast<QSpinBox*>(w)) {
        sb->setValue(v.toInt());
        return true;
    }
    if (auto* ds = qobject_cast<QDoubleSpinBox*>(w)) {
        ds->setValue(v.toDouble());
        return true;
    }
    if (auto* ck = qobject_cast<QCheckBox*>(w)) {
        ck->setChecked(v.toBool());
        return true;
    }
    if (auto* le = qobject_cast<QLineEdit*>(w)) {
        le->setText(v.toString());
        return true;
    }
    return true;
}

} // namespace econ_panel_base_detail

namespace econ_pbd = econ_panel_base_detail;

// ── Stylesheet (token-based) ──────────────────────────────────────────────────

QString EconPanelBase::panel_style() const {
    using namespace ui::colors;
    int r = QColor(color_).red();
    int g = QColor(color_).green();
    int b = QColor(color_).blue();
    return QString(
               // Toolbar control labels ("DATASET", "COUNTRY", …): panels set
               // objectName "econCtrlLabel" and inherit the look from this container
               // stylesheet (parsed once) instead of an inline setStyleSheet per label.
               // %14 is TEXT_TERTIARY — NOT %12, which is a BACKGROUND colour here.
               "QLabel#econCtrlLabel { color:%14; font-size:9px; font-weight:700;"
               "  background:transparent; }"
               "QLabel#econCreditHint { color:%14; font-size:9px; background:transparent; }"
               "#econToolbar { background:%5; border-bottom:1px solid %6; }"
               "#econFetchBtn { background:%1; color:%7; border:none;"
               "  font-size:10px; font-weight:700; padding:4px 14px; }"
               "#econFetchBtn:hover { background:rgba(%2,%3,%4,0.75); }"
               "#econFetchBtn:disabled { background:%6; color:%8; }"
               "#econCsvBtn { background:transparent; color:%9; border:1px solid %6;"
               "  font-size:10px; font-weight:700; padding:4px 10px; }"
               "#econCsvBtn:hover { color:%10; background:%11; }"
               "#econRetryBtn { background:transparent; color:%1; border:1px solid %1;"
               "  font-size:10px; font-weight:700; padding:4px 16px; }"
               "#econRetryBtn:hover { background:rgba(%2,%3,%4,0.12); }"
               "#econActionBtn { background:%1; color:%7; border:none;"
               "  font-size:10px; font-weight:700; padding:4px 16px; }"
               "#econActionBtn:hover { background:rgba(%2,%3,%4,0.75); }"
               "QComboBox, QSpinBox, QDateEdit, QLineEdit {"
               "  background:%7; color:%10; border:1px solid %6;"
               "  font-size:11px; padding:2px 6px; }"
               "QComboBox::drop-down, QDateEdit::drop-down { border:none; }"
               "QComboBox QAbstractItemView { background:%5; color:%10;"
               "  border:1px solid %6; }"
               "QCheckBox { color:%9; font-size:10px; background:transparent; }"
               "QTableWidget { background:%7; color:%10; border:none;"
               "  gridline-color:%6; font-size:11px; alternate-background-color:%12; }"
               "QTableWidget::item { padding:5px 8px; border-bottom:1px solid %6; }"
               "QTableWidget::item:selected { background:rgba(%2,%3,%4,0.1); color:%1; }"
               "QHeaderView::section { background:%5; color:%9; border:none;"
               "  border-bottom:2px solid %6; border-right:1px solid %6;"
               "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"
               "#econCardsRow  { background:%7; border-bottom:1px solid %6; }"
               "#econTitleBar  { background:%12; border-bottom:1px solid %6; }"
               "#econStatCard { background:%12; border:1px solid %6; }"
               "#econStatCard:hover { border-color:%13; }"
               "#econStatLabel { color:%9; font-size:8px; font-weight:700;"
               "  letter-spacing:1px; background:transparent; }"
               "#econStatVal  { color:%1;  font-size:15px; font-weight:700; background:transparent; }"
               "#econStatPos  { color:%16; font-size:15px; font-weight:700; background:transparent; }"
               "#econStatNeg  { color:%15; font-size:15px; font-weight:700; background:transparent; }"
               "#econStatSub  { color:%14; font-size:9px;  background:transparent; }"
               "#econEmptyPage { background:%7; }"
               "#econEmptyMsg   { color:%9;  font-size:13px; background:transparent; }"
               "#econLoadingMsg { color:%1;  font-size:13px; background:transparent; }"
               "#econErrMsg     { color:%15; font-size:12px; background:transparent; }"
               "#econTitleLbl { color:%10; font-size:11px; font-weight:700;"
               "  background:transparent; }"
               "#econRowCount { color:%14; font-size:9px; background:transparent; }"
               "#econMetaLbl  { color:%14; font-size:9px; background:transparent; }"
               "#econBannerLbl { color:%17; font-size:10px; font-weight:700; background:transparent; }"
               "QScrollBar:vertical { background:%7; width:5px; }"
               "QScrollBar::handle:vertical { background:%6; min-height:20px; }"
               "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
               "QScrollBar:horizontal { background:%7; height:5px; }"
               "QScrollBar::handle:horizontal { background:%6; min-width:20px; }"
               "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }")
        .arg(color_) // %1: source accent
        .arg(r)
        .arg(g)
        .arg(b)                // %2,%3,%4: accent RGB
        .arg(BG_RAISED())      // %5
        .arg(BORDER_DIM())     // %6
        .arg(BG_BASE())        // %7
        .arg(TEXT_DIM())       // %8
        .arg(TEXT_SECONDARY()) // %9
        .arg(TEXT_PRIMARY())   // %10
        .arg(BG_HOVER())       // %11
        .arg(BG_SURFACE())     // %12
        .arg(BORDER_BRIGHT())  // %13
        .arg(TEXT_TERTIARY())  // %14
        .arg(NEGATIVE())       // %15
        .arg(POSITIVE())       // %16
        .arg(WARNING());       // %17
}

// ── Reusable token-based style helpers ────────────────────────────────────────

QString EconPanelBase::sidebar_style() {
    using namespace ui::colors;
    return QString("background:%1; border-right:1px solid %2;").arg(BG_SURFACE(), BORDER_DIM());
}

QString EconPanelBase::section_hdr_style() {
    using namespace ui::colors;
    return QString("background:%1; border-bottom:1px solid %2;").arg(BG_RAISED(), BORDER_DIM());
}

QString EconPanelBase::section_lbl_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:8px; font-weight:700;"
                   " letter-spacing:1px; background:transparent; padding:4px 10px;")
        .arg(TEXT_TERTIARY());
}

QString EconPanelBase::search_input_style() {
    using namespace ui::colors;
    return QString("background:%1; color:%2; border:none;"
                   " border-bottom:1px solid %3; padding:4px 10px; font-size:11px;")
        .arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM());
}

QString EconPanelBase::ctrl_label_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:9px; font-weight:700; background:transparent;").arg(TEXT_TERTIARY());
}

QString EconPanelBase::list_style() const {
    using namespace ui::colors;
    QColor c(color_);
    QString rgba = QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
    return QString("QListWidget { background:transparent; border:none; outline:none; }"
                   "QListWidget::item { color:%1; padding:5px 10px;"
                   "  border-bottom:1px solid %2; font-size:11px; }"
                   "QListWidget::item:hover { color:%3; background:%4; }"
                   "QListWidget::item:selected { color:%5; background:rgba(%6,0.1);"
                   "  border-left:2px solid %5; font-weight:700; }")
        .arg(TEXT_SECONDARY(), BG_RAISED(), TEXT_PRIMARY(), BG_HOVER())
        .arg(color_, rgba);
}

QString EconPanelBase::notice_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:9px; background:transparent;").arg(WARNING());
}

QString EconPanelBase::format_number(double v) {
    static const QLocale kLoc(QLocale::English);
    const double a = std::abs(v);
    if (a >= 1e12)
        return kLoc.toString(v / 1e12, 'f', 2) + QStringLiteral("T");
    if (a >= 1e9)
        return kLoc.toString(v / 1e9, 'f', 2) + QStringLiteral("B");
    if (a >= 1e6)
        return kLoc.toString(v / 1e6, 'f', 2) + QStringLiteral("M");
    if (a >= 1e4)
        return kLoc.toString(v, 'f', v == std::floor(v) ? 0 : 1);
    return QString::number(v, 'g', 6);
}

// ── Constructor ───────────────────────────────────────────────────────────────

EconPanelBase::EconPanelBase(const QString& source_id, const QString& color, QWidget* parent)
    : QWidget(parent), source_id_(source_id), color_(color) {
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, &EconPanelBase::refresh_panel_theme);
    // Connected before any subclass connects its on_result(), so the retrieval time of the result a
    // panel is about to display() is already recorded when it does.
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            [this](const QString& /*request_id*/, const services::EconomicsResult& r) {
                if (r.success && r.source_id == source_id_)
                    set_retrieval_info(r.fetched_at, r.from_cache);
            });
}

// ── Build ─────────────────────────────────────────────────────────────────────

void EconPanelBase::build_base_ui(QWidget* container) {
    container_ = container;
    container->setStyleSheet(panel_style());

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("econToolbar");
    toolbar->setFixedHeight(40);
    auto* thl = new QHBoxLayout(toolbar);
    thl->setContentsMargins(10, 0, 10, 0);
    thl->setSpacing(6);
    build_controls(thl);
    thl->addStretch(1);

    fetch_btn_ = new QPushButton(tr("FETCH"));
    fetch_btn_->setObjectName("econFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    fetch_btn_->setAccessibleName(tr("Fetch data"));
    fetch_btn_->setAccessibleDescription(tr("Run the query against the %1 data source").arg(source_id_));
    fetch_btn_->setToolTip(tr("Run the query (Enter)"));
    connect(fetch_btn_, &QPushButton::clicked, this, &EconPanelBase::trigger_fetch);

    export_btn_ = new QPushButton(tr("CSV"));
    export_btn_->setObjectName("econCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    export_btn_->setAccessibleName(tr("Export results as CSV"));
    export_btn_->setToolTip(tr("Export the full result set (all pages) as CSV"));
    connect(export_btn_, &QPushButton::clicked, this, &EconPanelBase::export_csv);

    thl->addWidget(fetch_btn_);
    thl->addWidget(export_btn_);
    root->addWidget(toolbar);

    // Explicit keyboard tab order across the query form, in visual order,
    // ending on FETCH → CSV. Without this Qt falls back to construction order,
    // which for the sidebar panels starts focus in the results table.
    // Also make Enter in any toolbar text field run the query.
    {
        QWidget* prev = nullptr;
        for (int i = 0; i < thl->count(); ++i) {
            QWidget* w = thl->itemAt(i)->widget();
            if (!w || !(w->focusPolicy() & Qt::TabFocus))
                continue;
            if (prev)
                QWidget::setTabOrder(prev, w);
            prev = w;
            // Enter must not bypass the disabled FETCH button: a second query started while one is in
            // flight lets a slow earlier response overwrite the newer selection.
            if (auto* le = qobject_cast<QLineEdit*>(w))
                connect(le, &QLineEdit::returnPressed, this, [this]() {
                    if (fetch_btn_ && fetch_btn_->isEnabled())
                        trigger_fetch();
                });
        }
    }

    // Stat cards row
    cards_row_ = new QWidget(this);
    cards_row_->setObjectName("econCardsRow");
    auto* crhl = new QHBoxLayout(cards_row_);
    crhl->setContentsMargins(10, 6, 10, 6);
    crhl->setSpacing(6);

    auto make_card = [&](const QString& label, QLabel*& out, QLabel*& title_out, QLabel** sub_out) {
        auto* card = new QWidget(this);
        card->setObjectName("econStatCard");
        card->setMinimumWidth(90);
        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(10, 6, 10, 6);
        vl->setSpacing(1);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("econStatLabel");
        title_out = lbl;
        out = new QLabel(QStringLiteral("—"));
        out->setObjectName("econStatVal");
        vl->addWidget(lbl);
        vl->addWidget(out);
        if (sub_out) {
            *sub_out = new QLabel;
            (*sub_out)->setObjectName("econStatSub");
            vl->addWidget(*sub_out);
        }
        crhl->addWidget(card);
        return card;
    };
    make_card(tr("LATEST"), stat_latest_, stat_latest_lbl_, &stat_latest_sub_);
    make_card(tr("CHG vs PREV"), stat_change_, stat_change_lbl_, &stat_change_sub_);
    yoy_card_ = make_card(tr("YoY"), stat_yoy_, stat_yoy_lbl_, &stat_yoy_sub_);
    yoy_card_->hide();
    make_card(tr("MIN"), stat_min_, stat_min_lbl_, nullptr);
    make_card(tr("MAX"), stat_max_, stat_max_lbl_, nullptr);
    make_card(tr("AVG"), stat_avg_, stat_avg_lbl_, nullptr);
    make_card(tr("POINTS"), stat_count_, stat_count_lbl_, nullptr);
    crhl->addStretch(1);
    cards_row_->hide(); // shown by display() for a single dated series
    root->addWidget(cards_row_);

    // Title bar: title + record count, then the provenance line, then an optional warning banner.
    title_bar_ = new QWidget(this);
    title_bar_->setObjectName("econTitleBar");
    auto* tbvl = new QVBoxLayout(title_bar_);
    tbvl->setContentsMargins(12, 4, 12, 4);
    tbvl->setSpacing(1);
    auto* tbhl = new QHBoxLayout;
    tbhl->setContentsMargins(0, 0, 0, 0);
    title_lbl_ = new QLabel;
    title_lbl_->setObjectName("econTitleLbl");
    row_count_ = new QLabel;
    row_count_->setObjectName("econRowCount");
    tbhl->addWidget(title_lbl_);
    tbhl->addStretch(1);
    tbhl->addWidget(row_count_);
    tbvl->addLayout(tbhl);
    meta_lbl_ = new QLabel;
    meta_lbl_->setObjectName("econMetaLbl");
    meta_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    meta_lbl_->hide();
    tbvl->addWidget(meta_lbl_);
    banner_lbl_ = new QLabel;
    banner_lbl_->setObjectName("econBannerLbl");
    banner_lbl_->setWordWrap(true);
    banner_lbl_->hide();
    tbvl->addWidget(banner_lbl_);
    root->addWidget(title_bar_);

    // Content stack
    stack_ = new QStackedWidget;

    auto* empty_pg = new QWidget(this);
    empty_pg->setObjectName("econEmptyPage");
    auto* evl = new QVBoxLayout(empty_pg);
    evl->setAlignment(Qt::AlignCenter);
    evl->setSpacing(12);
    empty_lbl_ = new QLabel(tr("Select parameters and click FETCH"));
    empty_lbl_->setObjectName("econEmptyMsg");
    empty_lbl_->setAlignment(Qt::AlignCenter);
    empty_lbl_->setWordWrap(true);
    empty_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status_kind_ = StatusKind::Empty;
    status_msg_ = tr("Select parameters and click FETCH");
    evl->addWidget(empty_lbl_);

    auto* btn_row = new QHBoxLayout;
    btn_row->setAlignment(Qt::AlignCenter);
    btn_row->setSpacing(8);
    retry_btn_ = new QPushButton(tr("RETRY"));
    retry_btn_->setObjectName("econRetryBtn");
    retry_btn_->setCursor(Qt::PointingHandCursor);
    retry_btn_->setToolTip(tr("Run the same query again"));
    retry_btn_->hide();
    connect(retry_btn_, &QPushButton::clicked, this, [this]() {
        if (fetch_btn_ && fetch_btn_->isEnabled())
            trigger_fetch();
    });
    action_btn_ = new QPushButton;
    action_btn_->setObjectName("econActionBtn");
    action_btn_->setCursor(Qt::PointingHandCursor);
    action_btn_->hide();
    connect(action_btn_, &QPushButton::clicked, this, [this]() {
        const auto cb = action_cb_; // the action may change the panel state (and action_cb_)
        if (cb)
            cb();
    });
    btn_row->addWidget(retry_btn_);
    btn_row->addWidget(action_btn_);
    evl->addLayout(btn_row);
    stack_->addWidget(empty_pg); // index 0

    auto* table_page = new QWidget(this);
    auto* tpl = new QVBoxLayout(table_page);
    tpl->setContentsMargins(0, 0, 0, 0);
    tpl->setSpacing(0);

    table_ = new QTableWidget;
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(true);
    table_->setAccessibleName(tr("Results table"));
    // Header click sorts the FULL result set (all pages), not just the visible page.
    table_->horizontalHeader()->setSectionsClickable(true);
    table_->horizontalHeader()->setToolTip(tr("Click a column header to sort all rows"));
    connect(table_->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int section) {
        if (section < 0 || section >= columns_.size())
            return;
        const Qt::SortOrder order = (section == sort_column_ && sort_order_ == Qt::DescendingOrder)
                                        ? Qt::AscendingOrder
                                        : Qt::DescendingOrder;
        sort_rows(section, order);
        pager_->set_total(static_cast<int>(all_rows_.size()));
        render_page();
    });
    copy_sel_act_ = new QAction(tr("Copy selected rows"), table_);
    copy_sel_act_->setShortcut(QKeySequence::Copy);
    copy_sel_act_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(copy_sel_act_, &QAction::triggered, this, [this]() { copy_rows(true); });
    copy_all_act_ = new QAction(tr("Copy all rows (tab-separated)"), table_);
    connect(copy_all_act_, &QAction::triggered, this, [this]() { copy_rows(false); });
    export_act_ = new QAction(tr("Export CSV…"), table_);
    connect(export_act_, &QAction::triggered, this, &EconPanelBase::export_csv);
    table_->addAction(copy_sel_act_);
    table_->addAction(copy_all_act_);
    table_->addAction(export_act_);
    table_->setContextMenuPolicy(Qt::ActionsContextMenu);
    tpl->addWidget(table_, 1);

    pager_ = new ui::PaginationBar(this);
    connect(pager_, &ui::PaginationBar::page_changed, this, [this]() { render_page(); });
    tpl->addWidget(pager_);

    stack_->addWidget(table_page); // index 1

    root->addWidget(stack_, 1);

    // Apply token-based inline styles for non-QSS-covered widgets
    refresh_panel_theme();
}

// ── Theme refresh ─────────────────────────────────────────────────────────────

void EconPanelBase::refresh_panel_theme() {
    if (container_)
        container_->setStyleSheet(panel_style());
}

// ── State ─────────────────────────────────────────────────────────────────────

bool EconPanelBase::keeps_state_on_activate() const {
    return (stack_ && stack_->currentIndex() != 0) || status_kind_ == StatusKind::Loading;
}

void EconPanelBase::trigger_fetch() {
    emit query_started();
    on_fetch();
}

void EconPanelBase::show_loading(const QString& msg) {
    if (!empty_lbl_)
        return;
    status_kind_ = StatusKind::Loading;
    status_msg_ = msg.isEmpty() ? tr("Fetching data…") : msg;
    empty_lbl_->setObjectName("econLoadingMsg");
    empty_lbl_->style()->unpolish(empty_lbl_);
    empty_lbl_->style()->polish(empty_lbl_);
    empty_lbl_->setText(status_msg_);
    retry_btn_->hide();
    action_btn_->hide();
    action_cb_ = nullptr;
    stack_->setCurrentIndex(0);
    if (fetch_btn_)
        fetch_btn_->setEnabled(false);
}

void EconPanelBase::show_error(const QString& msg) {
    if (!empty_lbl_)
        return;
    status_kind_ = StatusKind::Error;
    status_msg_ = msg;
    empty_lbl_->setObjectName("econErrMsg");
    empty_lbl_->style()->unpolish(empty_lbl_);
    empty_lbl_->style()->polish(empty_lbl_);
    empty_lbl_->setText(tr("Error: %1").arg(msg));
    retry_btn_->show();
    action_btn_->hide();
    action_cb_ = nullptr;
    stack_->setCurrentIndex(0);
    if (fetch_btn_)
        fetch_btn_->setEnabled(true);
}

void EconPanelBase::show_error_with_action(const QString& msg, const QString& action_label,
                                           std::function<void()> action) {
    show_error(msg);
    if (!action_btn_ || action_label.isEmpty() || !action)
        return;
    action_cb_ = std::move(action);
    action_btn_->setText(action_label);
    action_btn_->show();
}

void EconPanelBase::show_empty(const QString& msg) {
    if (!empty_lbl_)
        return;
    status_kind_ = StatusKind::Empty;
    status_msg_ = msg.isEmpty() ? tr("Select parameters and click FETCH") : msg;
    empty_lbl_->setObjectName("econEmptyMsg");
    empty_lbl_->style()->unpolish(empty_lbl_);
    empty_lbl_->style()->polish(empty_lbl_);
    empty_lbl_->setText(status_msg_);
    retry_btn_->hide();
    action_btn_->hide();
    action_cb_ = nullptr;
    stack_->setCurrentIndex(0);
    if (fetch_btn_)
        fetch_btn_->setEnabled(true);
}

void EconPanelBase::show_table() {
    stack_->setCurrentIndex(1);
    if (fetch_btn_)
        fetch_btn_->setEnabled(true);
}

int EconPanelBase::add_content_page(QWidget* page) {
    if (!stack_ || !page)
        return -1;
    return stack_->addWidget(page);
}

void EconPanelBase::show_content_page(int index) {
    if (!stack_ || index < 0 || index >= stack_->count())
        return;
    stack_->setCurrentIndex(index);
    if (fetch_btn_)
        fetch_btn_->setEnabled(true);
}

void EconPanelBase::set_stats_visible(bool visible) {
    stats_allowed_ = visible;
    apply_stats_visibility();
}

void EconPanelBase::apply_stats_visibility() {
    if (cards_row_)
        cards_row_->setVisible(stats_allowed_ && stats_applicable_);
}

void EconPanelBase::set_series_meta(const SeriesMeta& meta) {
    next_meta_ = meta;
}

void EconPanelBase::set_retrieval_info(const QDateTime& fetched_utc, bool from_cache) {
    retrieved_at_ = fetched_utc;
    retrieved_from_cache_ = from_cache;
}

void EconPanelBase::set_banner(const QString& text) {
    if (!banner_lbl_)
        return;
    banner_lbl_->setText(text);
    banner_lbl_->setVisible(!text.isEmpty());
}

// ── Display ───────────────────────────────────────────────────────────────────

void EconPanelBase::display(const QJsonArray& rows, const QString& title, const QStringList& column_order,
                            bool keep_row_order) {
    meta_ = next_meta_;
    next_meta_ = SeriesMeta{};
    set_banner({});

    if (rows.isEmpty()) {
        show_empty(tr("No data returned for this selection"));
        return;
    }

    all_rows_ = rows;
    title_ = title;

    columns_.clear();
    QSet<QString> seen;
    for (const auto& v : rows) {
        const QStringList keys = v.toObject().keys();
        for (const auto& k : keys) {
            if (!seen.contains(k)) {
                seen.insert(k);
                columns_ << k;
            }
        }
    }
    if (!column_order.isEmpty()) {
        QStringList ordered;
        for (const auto& c : column_order)
            if (seen.contains(c) && !ordered.contains(c))
                ordered << c;
        for (const auto& c : std::as_const(columns_))
            if (!ordered.contains(c))
                ordered << c;
        columns_ = ordered;
    } else {
        // Date first, value second. Stop at the first key found: a later date-like key ("year"
        // after "date") must not displace the real one.
        for (const auto& dk : econ_pbd::date_keys()) {
            const auto idx = columns_.indexOf(dk);
            if (idx < 0)
                continue;
            if (idx > 0)
                columns_.move(idx, 0);
            break;
        }
        for (const auto& vk : econ_pbd::value_keys()) {
            const auto idx = columns_.indexOf(vk);
            if (idx < 0)
                continue;
            if (idx > 1)
                columns_.move(idx, 1);
            break;
        }
    }

    // Newest first when the first column holds real dates (a calendar's "8:30am" does not parse).
    sort_column_ = -1;
    table_->horizontalHeader()->setSortIndicatorShown(false);
    if (!keep_row_order && !columns_.isEmpty() && econ_pbd::is_date_key(columns_.first())) {
        int parsed = 0;
        for (const auto& rv : std::as_const(all_rows_))
            if (econ_pbd::parse_period(econ_pbd::period_text(rv.toObject().value(columns_.first()))).date.isValid())
                ++parsed;
        if (parsed * 5 >= all_rows_.size() * 4)
            sort_rows(0, Qt::DescendingOrder);
    }

    pager_->set_total(static_cast<int>(all_rows_.size()));
    render_page();

    if (title_lbl_)
        title_lbl_->setText(title);

    update_stats();

    if (row_count_)
        row_count_->setText(tr("%n record(s)", "", static_cast<int>(all_rows_.size())));
    refresh_provenance();

    show_table();
}

void EconPanelBase::sort_rows(int column, Qt::SortOrder order) {
    if (column < 0 || column >= columns_.size() || all_rows_.isEmpty())
        return;
    const QString key = columns_[column];
    struct Item {
        QJsonValue row;
        bool has = false;
        bool num_ok = false;
        double num = 0.0;
        bool date_ok = false;
        qint64 jd = 0;
        QString str;
    };
    QVector<Item> items;
    items.reserve(all_rows_.size());
    for (const auto& rv : std::as_const(all_rows_)) {
        Item it;
        it.row = rv;
        const QJsonValue v = rv.toObject().value(key);
        it.has = !(v.isUndefined() || v.isNull() || (v.isString() && v.toString().trimmed().isEmpty()));
        if (it.has) {
            it.num_ok = econ_pbd::to_number(v, &it.num);
            if (!it.num_ok) {
                const econ_pbd::Period p = econ_pbd::parse_period(econ_pbd::period_text(v));
                it.date_ok = p.date.isValid();
                if (it.date_ok)
                    it.jd = p.date.toJulianDay();
            }
            it.str = econ_pbd::cell_text(v);
        }
        items.append(std::move(it));
    }
    const bool asc = order == Qt::AscendingOrder;
    std::stable_sort(items.begin(), items.end(), [asc](const Item& a, const Item& b) {
        if (a.has != b.has)
            return a.has; // blanks last in either direction
        if (!a.has)
            return false;
        int c = 0;
        if (a.date_ok && b.date_ok)
            c = a.jd < b.jd ? -1 : (a.jd > b.jd ? 1 : 0);
        else if (a.num_ok && b.num_ok)
            c = a.num < b.num ? -1 : (a.num > b.num ? 1 : 0);
        else
            c = QString::compare(a.str, b.str, Qt::CaseInsensitive);
        return asc ? c < 0 : c > 0;
    });
    QJsonArray sorted;
    for (const auto& it : std::as_const(items))
        sorted.append(it.row);
    all_rows_ = sorted;
    sort_column_ = column;
    sort_order_ = order;
    table_->horizontalHeader()->setSortIndicatorShown(true);
    table_->horizontalHeader()->setSortIndicator(column, order);
}

void EconPanelBase::render_page() {
    const int start = pager_->offset();
    const int count = std::min<int>(pager_->page_size(), static_cast<int>(all_rows_.size()) - start);

    // Preserve the user's vertical scroll offset across a re-render (page-size
    // change, language switch) so the row they were reading doesn't jump.
    const int scroll_pos = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;
    const int sel_row = table_->currentRow();

    table_->setUpdatesEnabled(false);
    table_->clearContents();
    table_->setColumnCount(static_cast<int>(columns_.size()));
    table_->setHorizontalHeaderLabels(columns_);
    table_->setRowCount(std::max(0, count));

    static const QLocale kLoc(QLocale::English);
    const QColor accent(color_);
    QVector<bool> groupable(columns_.size());
    for (int c = 0; c < columns_.size(); ++c)
        groupable[c] = !econ_pbd::is_date_key(columns_[c]) && !econ_pbd::is_id_like(columns_[c]);
    for (int r = 0; r < count; ++r) {
        const auto obj = all_rows_[start + r].toObject();
        for (int c = 0; c < columns_.size(); ++c) {
            const auto jv = obj[columns_[c]];
            QString text;
            bool numeric = false;
            if (jv.isDouble()) {
                numeric = true;
                const double v = jv.toDouble();
                // Exact integers in full — 'g' turned national-accounts figures (21433226000000)
                // into "2.1433226e+13" — with thousands separators from five digits up (years and
                // codes are never grouped).
                if (v == std::floor(v) && std::abs(v) < 1e15)
                    text = (groupable[c] && std::abs(v) >= 1e4) ? kLoc.toString(static_cast<qint64>(v))
                                                                : QString::number(static_cast<qint64>(v));
                else
                    text = QString::number(v, 'g', 10);
            } else if (jv.isNull() || jv.isUndefined()) {
                text = QStringLiteral("—"); // explicit gap, never a silent 0
            } else if (jv.isBool()) {
                text = jv.toBool() ? QStringLiteral("true") : QStringLiteral("false");
            } else {
                text = jv.toString();
                // Strings that are purely numeric (FRED/BLS return values as
                // strings) still belong in a right-aligned column.
                bool ok = false;
                text.toDouble(&ok);
                numeric = ok && !text.isEmpty();
            }
            auto* item = new QTableWidgetItem(text);
            if (c == 0)
                item->setForeground(accent);
            // DESIGN_SYSTEM §5.4 — numeric columns are right-aligned.
            item->setTextAlignment(numeric ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter));
            table_->setItem(r, c, item);
        }
    }

    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    if (columns_.size() > 1)
        table_->horizontalHeader()->setSectionResizeMode(static_cast<int>(columns_.size()) - 1, QHeaderView::Stretch);
    table_->setUpdatesEnabled(true);

    if (sel_row >= 0 && sel_row < count)
        table_->setCurrentCell(sel_row, 0);
    if (table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(scroll_pos);
}

void EconPanelBase::update_stats() {
    stats_applicable_ = false;
    latest_period_.clear();
    stats_column_.clear();
    excluded_projections_ = 0;
    freq_label_ = meta_.frequency.trimmed();

    for (QLabel* l : {stat_latest_, stat_change_, stat_yoy_, stat_min_, stat_max_, stat_avg_})
        if (l)
            l->setText(QStringLiteral("—"));
    for (QLabel* l : {stat_latest_sub_, stat_change_sub_, stat_yoy_sub_})
        if (l)
            l->clear();
    if (stat_change_lbl_)
        stat_change_lbl_->setText(tr("CHG vs PREV"));
    if (stat_count_)
        stat_count_->setText(QString::number(all_rows_.size()));
    if (yoy_card_)
        yoy_card_->hide();

    const QString date_col =
        (!columns_.isEmpty() && econ_pbd::is_date_key(columns_.first())) ? columns_.first() : QString();

    // Column the cards summarise: the panel's choice, a conventional value key, else the first
    // mostly-numeric column that is not a date or an identifier.
    QString value_col = columns_.contains(meta_.value_column) ? meta_.value_column : QString();
    if (value_col.isEmpty()) {
        for (const auto& vk : econ_pbd::value_keys()) {
            if (columns_.contains(vk)) {
                value_col = vk;
                break;
            }
        }
    }
    if (value_col.isEmpty()) {
        for (const auto& c : std::as_const(columns_)) {
            if (c == date_col || econ_pbd::is_date_key(c) || econ_pbd::is_id_like(c))
                continue;
            int numeric = 0;
            int present = 0;
            for (const auto& rv : std::as_const(all_rows_)) {
                const QJsonValue v = rv.toObject().value(c);
                if (v.isUndefined() || v.isNull())
                    continue;
                ++present;
                double x = 0.0;
                if (econ_pbd::to_number(v, &x))
                    ++numeric;
            }
            if (present > 0 && numeric * 2 >= present) {
                value_col = c;
                break;
            }
        }
    }
    stats_column_ = value_col;
    if (date_col.isEmpty() || value_col.isEmpty()) {
        apply_stats_visibility();
        return;
    }

    struct Pt {
        econ_pbd::Period p;
        QString label;
        double v = 0.0;
    };
    QVector<Pt> pts;
    int unparsed = 0;
    for (const auto& rv : std::as_const(all_rows_)) {
        const QJsonObject o = rv.toObject();
        double x = 0.0;
        if (!econ_pbd::to_number(o.value(value_col), &x))
            continue;
        const QString label = econ_pbd::period_text(o.value(date_col));
        const econ_pbd::Period p = econ_pbd::parse_period(label);
        if (!p.date.isValid()) {
            ++unparsed;
            continue;
        }
        pts.append({p, label, x});
    }
    if (pts.isEmpty() || unparsed * 4 > pts.size()) {
        apply_stats_visibility();
        return;
    }
    std::stable_sort(pts.begin(), pts.end(), [](const Pt& a, const Pt& b) { return a.p.date < b.p.date; });
    // A time series has one observation per period. Repeated dates mean several series (or a
    // cross-section) are interleaved, and LATEST / CHANGE / MIN / MAX over them would mix
    // unrelated numbers — hide the cards instead of showing something nobody can interpret.
    for (qsizetype i = 1; i < pts.size(); ++i) {
        if (pts[i].p.date == pts[i - 1].p.date) {
            apply_stats_visibility();
            return;
        }
    }
    // Projections (IMF WEO, EIA STEO, …) stay in the table but never become "LATEST".
    if (meta_.stats_cutoff.isValid()) {
        const qsizetype before = pts.size();
        const QDate cutoff = meta_.stats_cutoff;
        pts.erase(std::remove_if(pts.begin(), pts.end(), [&cutoff](const Pt& x) { return x.p.date > cutoff; }),
                  pts.end());
        excluded_projections_ = static_cast<int>(before - pts.size());
        if (pts.isEmpty()) {
            apply_stats_visibility();
            return;
        }
    }
    stats_applicable_ = true;

    QVector<econ_pbd::Period> periods;
    periods.reserve(pts.size());
    for (const auto& x : std::as_const(pts))
        periods.append(x.p);
    econ_pbd::Gran gran = econ_pbd::gran_from_text(meta_.frequency);
    if (gran == econ_pbd::Gran::Unknown)
        gran = econ_pbd::infer_gran(periods);
    if (freq_label_.isEmpty())
        freq_label_ = econ_pbd::gran_name(gran);

    const bool bp = econ_pbd::text_says_bp(meta_.units);
    bool pct = false;
    if (meta_.percent == 1)
        pct = true;
    else if (meta_.percent == -1 && !bp)
        pct = econ_pbd::text_says_percent(meta_.units.trimmed().isEmpty() ? title_ + QLatin1Char(' ') + value_col
                                                                      : meta_.units);
    const QString suffix = pct ? QStringLiteral("%") : (bp ? tr(" bp") : QString());
    auto fmt = [&suffix](double v) { return EconPanelBase::format_number(v) + suffix; };

    const Pt& last = pts.last();
    latest_period_ = last.label;
    if (stat_latest_)
        stat_latest_->setText(fmt(last.v));
    if (stat_latest_sub_)
        stat_latest_sub_->setText(last.label);

    const QString tag = econ_pbd::change_tag(gran);
    if (stat_change_lbl_)
        stat_change_lbl_->setText(tag.isEmpty() ? tr("CHG vs PREV") : tr("CHG %1").arg(tag));
    if (pts.size() >= 2) {
        const Pt& prev = pts[pts.size() - 2];
        econ_pbd::set_change(stat_change_, stat_change_sub_, last.v, prev.v, pct, bp, tr("vs %1").arg(prev.label));
    }

    // Year-over-year for sub-annual series: the same period one year earlier (±7 days for daily /
    // weekly data, whose calendar dates drift).
    if (gran != econ_pbd::Gran::Unknown && gran != econ_pbd::Gran::Year && stat_yoy_) {
        const QDate target = last.p.date.addYears(-1);
        const Pt* match = nullptr;
        for (qsizetype i = pts.size() - 1; i >= 0 && !match; --i) {
            const QDate d = pts[i].p.date;
            if (gran == econ_pbd::Gran::Day || gran == econ_pbd::Gran::Week) {
                if (d <= target && d >= target.addDays(-7))
                    match = &pts[i];
                else if (d < target.addDays(-7))
                    break;
            } else if (d == target) {
                match = &pts[i];
            } else if (d < target) {
                break;
            }
        }
        if (match) {
            econ_pbd::set_change(stat_yoy_, stat_yoy_sub_, last.v, match->v, pct, bp, tr("vs %1").arg(match->label));
            if (yoy_card_)
                yoy_card_->show();
        }
    }

    double mn = pts.first().v;
    double mx = pts.first().v;
    double sum = 0.0;
    for (const auto& x : std::as_const(pts)) {
        mn = std::min(mn, x.v);
        mx = std::max(mx, x.v);
        sum += x.v;
    }
    if (stat_min_)
        stat_min_->setText(fmt(mn));
    if (stat_max_)
        stat_max_->setText(fmt(mx));
    if (stat_avg_)
        stat_avg_->setText(fmt(sum / static_cast<double>(pts.size())));
    if (stat_count_)
        stat_count_->setText(QString::number(pts.size()));
    apply_stats_visibility();
}

QString EconPanelBase::default_source() const {
    static const QHash<QString, QString> kSources = {
        {QStringLiteral("worldbank"), QStringLiteral("World Bank — World Development Indicators")},
        {QStringLiteral("imf"), QStringLiteral("IMF — World Economic Outlook (DataMapper)")},
        {QStringLiteral("fred"), QStringLiteral("FRED, Federal Reserve Bank of St. Louis")},
        {QStringLiteral("fred_analytics"), QStringLiteral("FRED, Federal Reserve Bank of St. Louis")},
        {QStringLiteral("oecd"), QStringLiteral("OECD")},
        {QStringLiteral("ecb"), QStringLiteral("European Central Bank — Data Portal")},
        {QStringLiteral("eurostat"), QStringLiteral("Eurostat")},
        {QStringLiteral("bls"), QStringLiteral("U.S. Bureau of Labor Statistics")},
        {QStringLiteral("census"), QStringLiteral("U.S. Census Bureau — ACS 5-year")},
        {QStringLiteral("econdb"), QStringLiteral("EconDB")},
        {QStringLiteral("trading_economics"), QStringLiteral("Trading Economics")},
        {QStringLiteral("econ_calendar"), QStringLiteral("Forex Factory (scraped)")},
        {QStringLiteral("bcb"), QStringLiteral("Banco Central do Brasil — SGS")},
        {QStringLiteral("akshare_cn"), QStringLiteral("China NBS via AkShare")},
        {QStringLiteral("ons"), QStringLiteral("UK Office for National Statistics")},
        {QStringLiteral("global_cb"), QStringLiteral("National central bank statistics")},
        {QStringLiteral("federal_reserve"), QStringLiteral("Federal Reserve / New York Fed")},
        {QStringLiteral("fiscal_data"), QStringLiteral("U.S. Treasury — FiscalData")},
        {QStringLiteral("nber"), QStringLiteral("NBER Business Cycle Dating Committee")},
        {QStringLiteral("owid"), QStringLiteral("Our World in Data")},
        {QStringLiteral("statcan"), QStringLiteral("Statistics Canada")},
        {QStringLiteral("ilostat"), QStringLiteral("ILO — ILOSTAT")},
        {QStringLiteral("un_comtrade"), QStringLiteral("UN Comtrade")},
        {QStringLiteral("wb_health"), QStringLiteral("World Bank — Health, Nutrition & Population")},
        {QStringLiteral("bis"), QStringLiteral("Bank for International Settlements")},
        {QStringLiteral("adb"), QStringLiteral("Asian Development Bank — KIDB")},
        {QStringLiteral("cftc"), QStringLiteral("CFTC — Commitments of Traders")},
        {QStringLiteral("eia"), QStringLiteral("U.S. Energy Information Administration")},
        {QStringLiteral("wto"), QStringLiteral("World Trade Organization")},
        {QStringLiteral("unesco"), QStringLiteral("UNESCO Institute for Statistics")},
        {QStringLiteral("bea"), QStringLiteral("U.S. Bureau of Economic Analysis — NIPA")},
        {QStringLiteral("fincept"), QStringLiteral("Fincept")},
    };
    return kSources.value(source_id_);
}

void EconPanelBase::refresh_provenance() {
    if (!meta_lbl_)
        return;
    QStringList parts;
    if (!meta_.units.trimmed().isEmpty())
        parts << tr("Units: %1").arg(meta_.units.trimmed());
    if (!freq_label_.isEmpty())
        parts << freq_label_;
    if (!meta_.seasonal_adj.trimmed().isEmpty())
        parts << meta_.seasonal_adj.trimmed();
    const QString src = meta_.source.trimmed().isEmpty() ? default_source() : meta_.source.trimmed();
    if (!src.isEmpty())
        parts << tr("Source: %1").arg(src);
    if (!latest_period_.isEmpty())
        parts << tr("Latest obs: %1").arg(latest_period_);
    if (!meta_.last_updated.trimmed().isEmpty())
        parts << tr("Updated by source: %1").arg(meta_.last_updated.trimmed());
    if (retrieved_at_.isValid()) {
        const QString when = retrieved_at_.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        parts << (retrieved_from_cache_ ? tr("Retrieved %1 (cached)").arg(when) : tr("Retrieved %1").arg(when));
    }
    if (stats_applicable_ && stats_allowed_ && !stats_column_.isEmpty() && !econ_pbd::value_keys().contains(stats_column_))
        parts << tr("Cards: %1").arg(stats_column_);
    if (excluded_projections_ > 0)
        parts << tr("%n projected period(s) kept out of the cards", "", excluded_projections_);
    if (!meta_.note.trimmed().isEmpty())
        parts << meta_.note.trimmed();
    const QString text = parts.join(QStringLiteral("  ·  "));
    meta_lbl_->setText(text);
    meta_lbl_->setToolTip(text);
    meta_lbl_->setVisible(!text.isEmpty());
}

// ── CSV / clipboard ───────────────────────────────────────────────────────────

void EconPanelBase::export_csv() {
    // Export the FULL result set (all_rows_), not just the page currently
    // rendered in table_. The table only ever holds one page, so the previous
    // table-driven export silently truncated every multi-page series.
    if (all_rows_.isEmpty() || columns_.isEmpty())
        return;

    QString path =
        QFileDialog::getSaveFileName(this, tr("Export CSV"), source_id_ + "_data.csv", tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // Say so — silently returning looked like a successful export.
        LOG_WARN("EconPanel", QString("CSV export failed to open %1: %2").arg(path, file.errorString()));
        QMessageBox::warning(this, tr("Export CSV"), tr("Could not write %1:\n%2").arg(path, file.errorString()));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    auto quote = [](QString v) {
        if (v.contains(',') || v.contains('"') || v.contains('\n'))
            v = "\"" + v.replace("\"", "\"\"") + "\"";
        return v;
    };

    // Provenance header — an economics export is useless without its source, units and vintage.
    out << "# source=" << (meta_.source.isEmpty() ? default_source() : meta_.source) << " (" << source_id_ << ")"
        << "  exported=" << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << "  rows=" << all_rows_.size()
        << "\n";
    out << "# title=" << title_ << "\n";
    if (!meta_.units.isEmpty() || !freq_label_.isEmpty() || !meta_.seasonal_adj.isEmpty())
        out << "# units=" << meta_.units << "  frequency=" << freq_label_ << "  seasonal_adjustment=" << meta_.seasonal_adj
            << "\n";
    if (retrieved_at_.isValid())
        out << "# retrieved=" << retrieved_at_.toString(Qt::ISODate) << (retrieved_from_cache_ ? " (cached)" : "")
            << "\n";

    QStringList hdrs;
    for (const auto& c : std::as_const(columns_))
        hdrs << quote(c);
    out << hdrs.join(",") << "\n";

    for (const auto& rv : std::as_const(all_rows_)) {
        const QJsonObject obj = rv.toObject();
        QStringList row;
        // 'g' with 15 significant digits round-trips a double without the
        // scientific-notation truncation the UI formatter applies.
        for (const auto& col : std::as_const(columns_))
            row << quote(econ_pbd::cell_text(obj[col]));
        out << row.join(",") << "\n";
    }
}

void EconPanelBase::copy_rows(bool selection_only) const {
    if (columns_.isEmpty() || all_rows_.isEmpty() || !table_)
        return;
    auto clean = [](QString s) { return s.replace(QLatin1Char('\t'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' ')); };
    QStringList lines;
    lines << columns_.join(QLatin1Char('\t'));
    QList<int> indices;
    if (selection_only) {
        const int start = pager_ ? pager_->offset() : 0;
        if (table_->selectionModel()) {
            const auto rows = table_->selectionModel()->selectedRows();
            for (const auto& idx : rows)
                indices << start + idx.row();
        }
        if (indices.isEmpty() && table_->currentRow() >= 0)
            indices << start + table_->currentRow();
        std::sort(indices.begin(), indices.end());
    } else {
        for (int i = 0; i < all_rows_.size(); ++i)
            indices << i;
    }
    for (int i : std::as_const(indices)) {
        if (i < 0 || i >= all_rows_.size())
            continue;
        const QJsonObject obj = all_rows_[i].toObject();
        QStringList cells;
        for (const auto& col : std::as_const(columns_))
            cells << clean(econ_pbd::cell_text(obj[col]));
        lines << cells.join(QLatin1Char('\t'));
    }
    if (lines.size() > 1)
        QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

// ── Input persistence ─────────────────────────────────────────────────────────

QList<QWidget*> EconPanelBase::persistable_inputs() const {
    QList<QWidget*> out;
    const QList<QWidget*> all = findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (stack_ && (w == stack_ || stack_->isAncestorOf(w)))
            continue; // results table, pager, panel-specific result pages
        const econ_pbd::InputKind kind = econ_pbd::kind_of(w);
        if (kind == econ_pbd::InputKind::Other)
            continue;
        if (kind == econ_pbd::InputKind::Text) {
            auto* le = static_cast<QLineEdit*>(w);
            if (le->echoMode() != QLineEdit::Normal)
                continue; // never persist secrets
            QWidget* p = le->parentWidget();
            if (qobject_cast<QComboBox*>(p) || qobject_cast<QAbstractSpinBox*>(p))
                continue; // the internal editor of a combo / spin box
        }
        out << w;
    }
    return out;
}

QHash<QString, QWidget*> EconPanelBase::keyed_inputs(QStringList* signature) const {
    QHash<QString, QWidget*> keyed;
    QHash<QString, int> counters;
    const QList<QWidget*> inputs = persistable_inputs();
    for (QWidget* w : inputs) {
        const QString cls = QString::fromLatin1(w->metaObject()->className());
        keyed.insert(cls + QLatin1Char('#') + QString::number(counters[cls]++), w);
        if (signature)
            *signature << cls;
    }
    return keyed;
}

QVariantMap EconPanelBase::save_panel_state() const {
    QStringList sig;
    const QHash<QString, QWidget*> keyed = keyed_inputs(&sig);
    QVariantMap inputs;
    for (auto it = keyed.cbegin(); it != keyed.cend(); ++it) {
        const QVariant v = econ_pbd::read_input(it.value());
        if (v.isValid())
            inputs.insert(it.key(), v);
    }
    // A choice whose list has not been filled yet this session (it loads asynchronously) is carried
    // over unchanged, so saving before the list arrives does not forget it.
    for (auto it = pending_inputs_.cbegin(); it != pending_inputs_.cend(); ++it)
        inputs.insert(it.key(), it.value());
    if (inputs.isEmpty())
        return {};
    return {{QString::fromLatin1(econ_pbd::kInputsKey), inputs},
            {QString::fromLatin1(econ_pbd::kInputsSigKey), sig.join(QLatin1Char(','))}};
}

void EconPanelBase::restore_panel_state(const QVariantMap& state) {
    const QVariantMap inputs = state.value(QString::fromLatin1(econ_pbd::kInputsKey)).toMap();
    if (inputs.isEmpty())
        return;
    QStringList sig;
    const QHash<QString, QWidget*> keyed = keyed_inputs(&sig);
    if (sig.join(QLatin1Char(',')) != state.value(QString::fromLatin1(econ_pbd::kInputsSigKey)).toString()) {
        // The panel's inputs changed shape since the state was saved (app update) — positional keys
        // would land values in the wrong fields, so start from the defaults instead.
        LOG_INFO("EconPanel", QString("Saved inputs for %1 no longer match the panel layout — not restored")
                                  .arg(source_id_));
        return;
    }
    pending_inputs_.clear();
    // Combos first (their change handlers repopulate dependent combos and may overwrite text
    // fields with preset defaults), then lists, spin/date/check boxes, and text fields last.
    const econ_pbd::InputKind order[] = {econ_pbd::InputKind::Combo, econ_pbd::InputKind::List, econ_pbd::InputKind::Toggle,
                                     econ_pbd::InputKind::Text};
    const QList<QWidget*> ordered_inputs = persistable_inputs();
    for (const econ_pbd::InputKind pass : order) {
        QHash<QString, int> counters;
        for (QWidget* w : ordered_inputs) {
            const QString cls = QString::fromLatin1(w->metaObject()->className());
            const QString key = cls + QLatin1Char('#') + QString::number(counters[cls]++);
            if (econ_pbd::kind_of(w) != pass || !inputs.contains(key))
                continue;
            if (!econ_pbd::apply_input(w, inputs.value(key)))
                pending_inputs_.insert(key, inputs.value(key));
        }
    }
}

void EconPanelBase::reapply_pending_inputs() {
    if (pending_inputs_.isEmpty())
        return;
    const QHash<QString, QWidget*> keyed = keyed_inputs();
    for (auto it = pending_inputs_.begin(); it != pending_inputs_.end();) {
        QWidget* w = keyed.value(it.key());
        if (!w || econ_pbd::apply_input(w, it.value()))
            it = pending_inputs_.erase(it);
        else
            ++it;
    }
}

// ── Shared utilities ──────────────────────────────────────────────────────────

void EconPanelBase::filter_list(QListWidget* list, const QString& text) {
    if (!list)
        return;
    for (int i = 0; i < list->count(); ++i) {
        auto* item = list->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EconPanelBase::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EconPanelBase::retranslateUi() {
    // Toolbar buttons
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));
    if (retry_btn_)
        retry_btn_->setText(tr("RETRY"));
    if (copy_sel_act_)
        copy_sel_act_->setText(tr("Copy selected rows"));
    if (copy_all_act_)
        copy_all_act_->setText(tr("Copy all rows (tab-separated)"));
    if (export_act_)
        export_act_->setText(tr("Export CSV…"));

    // Stat-card titles
    if (stat_latest_lbl_)
        stat_latest_lbl_->setText(tr("LATEST"));
    if (stat_yoy_lbl_)
        stat_yoy_lbl_->setText(tr("YoY"));
    if (stat_min_lbl_)
        stat_min_lbl_->setText(tr("MIN"));
    if (stat_max_lbl_)
        stat_max_lbl_->setText(tr("MAX"));
    if (stat_avg_lbl_)
        stat_avg_lbl_->setText(tr("AVG"));
    if (stat_count_lbl_)
        stat_count_lbl_->setText(tr("POINTS"));

    // Card values / change tag / provenance line carry translated fragments — recompute them.
    if (!all_rows_.isEmpty()) {
        update_stats();
        if (row_count_)
            row_count_->setText(tr("%n record(s)", "", static_cast<int>(all_rows_.size())));
        refresh_provenance();
    } else if (stat_change_lbl_) {
        stat_change_lbl_->setText(tr("CHG vs PREV"));
    }

    // Current status message. The empty default re-translates; loading/error
    // messages keep the message that was last shown (data-derived prefixes are
    // re-translated on the next fetch).
    if (empty_lbl_ && stack_ && stack_->currentIndex() == 0) {
        switch (status_kind_) {
            case StatusKind::Empty:
                empty_lbl_->setText(status_msg_);
                break;
            case StatusKind::Loading:
                empty_lbl_->setText(status_msg_);
                break;
            case StatusKind::Error:
                empty_lbl_->setText(tr("Error: %1").arg(status_msg_));
                break;
        }
    }
}

} // namespace fincept::screens
