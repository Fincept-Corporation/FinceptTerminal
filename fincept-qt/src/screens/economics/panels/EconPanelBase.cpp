// src/screens/economics/panels/EconPanelBase.cpp
#include "screens/economics/panels/EconPanelBase.h"

#include "core/logging/Logger.h"
#include "screens/economics/panels/EconPageModel.h"
#include "screens/economics/panels/EconTableAnalysis.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTableView>
#include <QThreadPool>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

namespace econ_panel_base_detail {

using econ_table::Gran;

/// A query input that only filters the result on screen (mark_local_input): not in the signature.
constexpr const char* kLocalInputProp = "econLocalInput";

/// Memory the results pane may keep for the views of one panel (AutoLoadPane::set_memory_budget).
/// The view on screen is never dropped, and most panels have one view; the panels with a VIEW
/// selector (Market Models, Fincept Macro) keep every view of a session within this — the largest
/// remembered table is ~1.6 MB by the pane's estimate (the Bank of England's 13,113 daily rates),
/// so 8 MB keeps four or more of the largest results per panel while 33 panels stay bounded.
constexpr qint64 kPaneMemoryBudget = 8LL * 1024 * 1024;

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

/// Display text of a number in the results table. Whole numbers print in full, grouped from five
/// digits up in groupable columns (years and codes never are); fractions keep about nine
/// significant digits (no float noise like 0.039900000000000005, no exponent), trailing zeros
/// trimmed, in the user's number locale.
QString number_cell(double v, bool groupable) {
    if (v == std::floor(v) && std::abs(v) < 1e15)
        return (groupable && std::abs(v) >= 1e4) ? ui::formatting::number(v, 0)
                                                 : QString::number(static_cast<qint64>(v));
    const double a = std::abs(v);
    const int int_digits = a >= 1.0 ? static_cast<int>(std::floor(std::log10(a))) + 1 : 0;
    const int decimals = std::clamp(9 - int_digits, 0, 8);
    if (a > 0.0 && a < 1e-8)
        return QString::number(v, 'g', 6); // too small for fixed decimals
    return ui::formatting::quantity(v, decimals);
}

/// Memory a remembered result holds, for the pane's budget: the analysis' estimate stored in the
/// payload ("bytes"), else a rough count — never a walk over a large payload on the GUI thread.
qint64 payload_bytes(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QJsonValue bytes = p.value(QStringLiteral("bytes"));
    if (bytes.isDouble() && bytes.toDouble() > 0)
        return static_cast<qint64>(bytes.toDouble());
    qint64 n = 1024;
    const QJsonArray rows = p.value(QStringLiteral("table")).toObject().value(QStringLiteral("rows")).toArray();
    n += qint64(rows.size()) * 6 * 64;
    const QJsonValue custom = p.value(QStringLiteral("custom"));
    if (custom.isObject() || custom.isArray())
        n += ui::autoload::approx_json_bytes(custom); // panel pages: cards, small tables
    return n;
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

// ── Failure classification ────────────────────────────────────────────────────
// Scripts report failures three ways: EconomicsService's "[CODE] message" prefix (the script's
// error_code / code), the connector contract's codes (missing_key | auth_failed | rate_limited |
// upstream_down | bad_args | no_data | not_supported) and plain text. Each kind gets its own page.
enum class FailKind { Other, MissingKey, AuthFailed, RateLimited, UpstreamDown, NoData, BadArgs, NotSupported, Cancelled };

struct Failure {
    FailKind kind = FailKind::Other;
    QString message;   ///< one plain sentence (no code prefix, no traceback)
    QString technical; ///< the full text when it holds more than the sentence
    QString env_key;   ///< credential named by the failure ("FRED_API_KEY")
    int retry_after_s = -1;
};

Failure classify_failure(const QString& raw_in) {
    Failure f;
    QString raw = raw_in.trimmed();
    QString code;
    static const QRegularExpression kCode(QStringLiteral("^\\[([A-Za-z_]+)\\]\\s*"));
    if (const auto m = kCode.match(raw); m.hasMatch()) {
        code = m.captured(1).toLower();
        raw = raw.mid(m.capturedLength());
    }
    // The sentence: the text before a traceback, its first line, at most 300 characters.
    QString plain = raw;
    const qsizetype tb = plain.indexOf(QLatin1String("Traceback"));
    if (tb > 0)
        plain = plain.left(tb).trimmed();
    const qsizetype nl = plain.indexOf(QLatin1Char('\n'));
    if (nl > 0)
        plain = plain.left(nl).trimmed();
    if (plain.startsWith(QLatin1String("Error: "), Qt::CaseInsensitive))
        plain = plain.mid(7);
    if (plain.size() > 300)
        plain = plain.left(300) + QStringLiteral("…");
    f.message = plain;
    if (raw.size() > plain.size() + 2)
        f.technical = raw.left(4000);

    static const QRegularExpression kEnvKey(
        QStringLiteral("\\b([A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*_(?:API_KEY|APP_TOKEN|TOKEN|KEY))\\b"));
    if (const auto m = kEnvKey.match(raw); m.hasMatch())
        f.env_key = m.captured(1);
    static const QRegularExpression kRetry(QStringLiteral("(?:retry|try again)[^0-9]{0,20}(\\d{1,5})\\s*(?:s\\b|sec)"),
                                           QRegularExpression::CaseInsensitiveOption);
    if (const auto m = kRetry.match(raw); m.hasMatch())
        f.retry_after_s = m.captured(1).toInt();

    const QString low = raw.toLower();
    if (code == QLatin1String("missing_api_key") || code == QLatin1String("missing_key") ||
        code == QLatin1String("no_api_key"))
        f.kind = FailKind::MissingKey;
    else if (code == QLatin1String("invalid_api_key") || code == QLatin1String("auth_failed") ||
             code == QLatin1String("unauthorized"))
        f.kind = FailKind::AuthFailed;
    else if (code == QLatin1String("rate_limited") || code == QLatin1String("too_many_requests"))
        f.kind = FailKind::RateLimited;
    else if (code == QLatin1String("timeout") || code == QLatin1String("upstream_down") ||
             code == QLatin1String("network_error") || code == QLatin1String("connection_error") ||
             code == QLatin1String("service_unavailable"))
        f.kind = FailKind::UpstreamDown;
    else if (code == QLatin1String("no_data") || code == QLatin1String("not_found"))
        f.kind = FailKind::NoData;
    else if (code == QLatin1String("bad_args") || code == QLatin1String("invalid_args") ||
             code == QLatin1String("invalid_parameter"))
        f.kind = FailKind::BadArgs;
    else if (code == QLatin1String("not_supported") || code == QLatin1String("unsupported"))
        f.kind = FailKind::NotSupported;
    else if (raw == QLatin1String("Cancelled"))
        f.kind = FailKind::Cancelled;
    else if ((low.contains(QLatin1String("api key")) || low.contains(QLatin1String("api_key")) ||
              low.contains(QLatin1String("subscription key"))) &&
             (low.contains(QLatin1String("required")) || low.contains(QLatin1String("not configured")) ||
              low.contains(QLatin1String("not set")) || low.contains(QLatin1String("missing"))))
        f.kind = FailKind::MissingKey;
    else if (low.contains(QLatin1String("rate limit")) || low.contains(QLatin1String("rate-limit")) ||
             low.contains(QLatin1String("too many requests")) || low.contains(QLatin1String("http 429")))
        f.kind = FailKind::RateLimited;
    else if (low.contains(QLatin1String("invalid api key")) || low.contains(QLatin1String("api key is invalid")) ||
             low.contains(QLatin1String("http 401")))
        f.kind = FailKind::AuthFailed;
    else if (low.contains(QLatin1String("timed out")) || low.contains(QLatin1String("timeout")) ||
             low.contains(QLatin1String("connectionerror")) || low.contains(QLatin1String("max retries exceeded")) ||
             low.contains(QLatin1String("name or service not known")) || low.contains(QLatin1String("getaddrinfo")) ||
             low.contains(QLatin1String("unreachable")) || low.contains(QLatin1String("http 502")) ||
             low.contains(QLatin1String("http 503")) || low.contains(QLatin1String("http 504")))
        f.kind = FailKind::UpstreamDown;
    else if (low.startsWith(QLatin1String("no data")) || low.contains(QLatin1String("no observations")) ||
             low.contains(QLatin1String("no records")) || low.contains(QLatin1String("returned no data")))
        f.kind = FailKind::NoData;
    return f;
}

} // namespace econ_panel_base_detail

namespace econ_pbd = econ_panel_base_detail;

// ── Stylesheet (token-based) ──────────────────────────────────────────────────

QString EconPanelBase::panel_style() const {
    using namespace ui::colors;
    // color_ is the provider's identity colour, picked against the dark theme: legible() keeps it
    // readable on the light one, and text on a fill of it takes whichever ink contrasts with it.
    const QColor accent = legible_color(QColor(color_));
    return QString(
               // Toolbar control labels ("DATASET", "COUNTRY", …): panels set
               // objectName "econCtrlLabel" and inherit the look from this container
               // stylesheet (parsed once) instead of an inline setStyleSheet per label.
               // %9 is TEXT_SECONDARY — the toolbar is a raised surface, too dark for TEXT_TERTIARY.
               "QLabel#econCtrlLabel { color:%9; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;"
               "  background:transparent; }"
               "QLabel#econCreditHint { color:%9; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }"
               "#econToolbar { background:%5; border-bottom:1px solid %6; }"
               "QScrollArea#econCtrlScroll, QWidget#econCtrlHost { background:transparent; border:none; }"
               "QLabel#econAsOf { color:%9; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }"
               "QLabel[econTone=\"ok\"] { color:%19; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }"
               "QLabel[econTone=\"warn\"] { color:%17; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }"
               "#econFetchBtn { background:%1; color:%4; border:none;"
               "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 14px; }"
               "#econFetchBtn:hover { background:%2; }"
               "#econFetchBtn:disabled { background:%12; color:%8; border:1px solid %6; }"
               "#econCsvBtn { background:transparent; color:%9; border:1px solid %6;"
               "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 10px; }"
               "#econCsvBtn:hover { color:%10; background:%11; }"
               "#econRetryBtn { background:transparent; color:%1; border:1px solid %1;"
               "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 16px; }"
               "#econRetryBtn:hover { background:%3; }"
               "#econActionBtn { background:%1; color:%4; border:none;"
               "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; padding:4px 16px; }"
               "#econActionBtn:hover { background:%2; }"
               "QComboBox, QSpinBox, QDateEdit, QLineEdit {"
               "  background:%7; color:%10; border:1px solid %6;"
               "  font-size:" + fincept::ui::fonts::px(-3) + "px; padding:2px 6px; }"
               "QComboBox::drop-down, QDateEdit::drop-down { border:none; }"
               "QComboBox QAbstractItemView { background:%5; color:%10;"
               "  border:1px solid %6; }"
               "QCheckBox { color:%9; font-size:" + fincept::ui::fonts::px(-4) + "px; background:transparent; }"
               // QTableView rules also reach the QTableWidgets of panel pages (a subclass).
               "QTableView { background:%7; color:%10; border:none;"
               "  gridline-color:%6; font-size:" + fincept::ui::fonts::px(-3) + "px; alternate-background-color:%12; }"
               "QTableView::item { padding:5px 8px; border-bottom:1px solid %6; }"
               "QTableView::item:selected { background:%3; color:%1; }"
               "QHeaderView::section { background:%5; color:%9; border:none;"
               "  border-bottom:2px solid %6; border-right:1px solid %6;"
               "  padding:5px 8px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.5px; }"
               "#econCardsRow  { background:%7; border-bottom:1px solid %6; }"
               "#econTitleBar  { background:%12; border-bottom:1px solid %6; }"
               "#econStatCard { background:%12; border:1px solid %6; }"
               "#econStatCard:hover { border-color:%13; }"
               "#econStatLabel { color:%9; font-size:" + fincept::ui::fonts::px(-6) + "px; font-weight:700;"
               "  letter-spacing:1px; background:transparent; }"
               "#econStatVal  { color:%1;  font-size:" + fincept::ui::fonts::px(1) + "px; font-weight:700; background:transparent; }"
               "#econStatPos  { color:%16; font-size:" + fincept::ui::fonts::px(1) + "px; font-weight:700; background:transparent; }"
               "#econStatNeg  { color:%15; font-size:" + fincept::ui::fonts::px(1) + "px; font-weight:700; background:transparent; }"
               "#econStatSub  { color:%14; font-size:" + fincept::ui::fonts::px(-5) + "px;  background:transparent; }"
               "#econEmptyPage { background:%7; }"
               "#econEmptyMsg   { color:%9;  font-size:" + fincept::ui::fonts::px(-1) + "px; background:transparent; }"
               "#econLoadingMsg { color:%1;  font-size:" + fincept::ui::fonts::px(-1) + "px; background:transparent; }"
               "#econErrMsg     { color:%18; font-size:" + fincept::ui::fonts::px(-2) + "px; background:transparent; }"
               "#econTitleLbl { color:%10; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
               "  background:transparent; }"
               "#econRowCount { color:%14; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }"
               "#econMetaLbl  { color:%14; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }"
               "#econBannerLbl { color:%17; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; background:transparent; }"
               "#econResultsSplit::handle { background:%6; }"
               "QScrollBar:vertical { background:%7; width:5px; }"
               "QScrollBar::handle:vertical { background:%6; min-height:20px; }"
               "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
               "QScrollBar:horizontal { background:%7; height:5px; }"
               "QScrollBar::handle:horizontal { background:%6; min-width:20px; }"
               "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }")
        .arg(accent.name())                     // %1: source accent
        .arg(alpha(accent, 0.75))               // %2: accent fill, hovered
        .arg(tint(accent, 0.12))                // %3: accent wash (selection, hover)
        .arg(contrast_text_on(accent).name())   // %4: text on an accent fill
        .arg(BG_RAISED())      // %5
        .arg(BORDER_DIM())     // %6
        .arg(BG_BASE())        // %7
        .arg(TEXT_TERTIARY())  // %8
        .arg(TEXT_SECONDARY()) // %9
        .arg(TEXT_PRIMARY())   // %10
        .arg(BG_HOVER())       // %11
        .arg(BG_SURFACE())     // %12
        .arg(BORDER_BRIGHT())  // %13
        .arg(TEXT_TERTIARY())  // %14
        .arg(NEGATIVE())       // %15
        .arg(POSITIVE())       // %16
        .arg(WARNING())        // %17
        .arg(DANGER())         // %18
        .arg(SUCCESS());       // %19
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
    return QString("color:%1; font-size:" + fincept::ui::fonts::px(-6) + "px; font-weight:700;"
                   " letter-spacing:1px; background:transparent; padding:4px 10px;")
        .arg(TEXT_SECONDARY()); // sits on the raised section header bar
}

QString EconPanelBase::search_input_style() {
    using namespace ui::colors;
    return QString("background:%1; color:%2; border:none;"
                   " border-bottom:1px solid %3; padding:4px 10px; font-size:" + fincept::ui::fonts::px(-3) + "px;")
        .arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM());
}

QString EconPanelBase::ctrl_label_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; background:transparent;").arg(TEXT_SECONDARY());
}

QString EconPanelBase::list_style() const {
    using namespace ui::colors;
    // Provider identity colour, made legible for the active theme (see panel_style()).
    const QColor accent = legible_color(QColor(color_));
    return QString("QListWidget { background:transparent; border:none; outline:none; }"
                   "QListWidget::item { color:%1; padding:5px 10px;"
                   "  border-bottom:1px solid %2; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                   "QListWidget::item:hover { color:%3; background:%4; }"
                   "QListWidget::item:selected { color:%5; background:%6;"
                   "  border-left:2px solid %5; font-weight:700; }")
        .arg(TEXT_SECONDARY(), BORDER_DIM(), TEXT_PRIMARY(), BG_HOVER())
        .arg(accent.name(), tint(accent, 0.1));
}

QString EconPanelBase::notice_style() {
    using namespace ui::colors;
    return QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent;").arg(WARNING());
}

void EconPanelBase::set_text_width(QWidget* w, int design_px) {
    if (!w)
        return;
    // Toolbar inputs use the base size - 3 (11px at the default size); the width grows with it.
    const auto apply = [w, design_px]() {
        const int font_px = ui::fonts::font_px(-3);
        w->setFixedWidth(std::max(design_px, design_px * font_px / 11));
    };
    apply();
    // A geometry change: done once the new appearance has settled, never inside theme_changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, w, [apply]() { apply(); });
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

QThreadPool* EconPanelBase::worker_pool() {
    // Shared by every economics panel; a child of the application, so its destructor (app teardown)
    // waits for a task in flight. Tasks capture values only; their results come back through
    // QFuture::then(panel, …), which drops them when the panel is gone.
    static QThreadPool* const pool = []() {
        auto* p = new QThreadPool(QCoreApplication::instance());
        p->setObjectName(QStringLiteral("EconomicsPanels"));
        p->setMaxThreadCount(2);
        return p;
    }();
    return pool;
}

// ── Constructor ───────────────────────────────────────────────────────────────

EconPanelBase::EconPanelBase(const QString& source_id, const QString& color, QWidget* parent)
    : QWidget(parent), source_id_(source_id), color_(color) {
    // Stylesheets follow theme and font changes through ThemeManager's re-style pass; what they do not
    // reach (the table's colours) is re-applied once the new appearance has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &EconPanelBase::refresh_panel_theme);
    // Connected before any subclass connects its on_result(), so the retrieval time of the result a
    // panel is about to display() is already recorded when it does. Only this panel's own requests
    // count: another screen may ask the same source (Portfolio asks FRED).
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            [this](const QString& request_id, const services::EconomicsResult& r) {
                if (!run_requests_.contains(request_id))
                    return;
                if (r.success && r.source_id == source_id_)
                    set_retrieval_info(r.fetched_at, r.from_cache);
                else if (!r.success && !r.env_key.isEmpty())
                    run_env_key_ = r.env_key; // the credential a missing-key / rejected-key failure names
            });
}

EconPanelBase::~EconPanelBase() {
    // The pane is deleted with the children, after this object's members: it must not call back.
    if (pane_) {
        pane_->set_cancel({});
        pane_->set_start({});
        pane_->set_render({});
        pane_->set_requirement_check({});
        pane_->set_size_estimator({});
    }
    for (const QString& rid : std::as_const(run_requests_))
        services::EconomicsService::instance().cancel(rid);
}

// ── Build ─────────────────────────────────────────────────────────────────────

void EconPanelBase::build_base_ui(QWidget* container) {
    container_ = container;
    container->setStyleSheet(panel_style()); // once, before the children exist

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar: the panel's inputs scroll sideways when they do not fit (1280 px, large fonts); the
    // "as of" status, FETCH and CSV stay in view on the right.
    toolbar_ = new QWidget(container);
    toolbar_->setObjectName("econToolbar");
    auto* thl_outer = new QHBoxLayout(toolbar_);
    thl_outer->setContentsMargins(10, 0, 10, 0);
    thl_outer->setSpacing(6);

    auto* ctrl_scroll = new QScrollArea(toolbar_);
    ctrl_scroll->setObjectName(QStringLiteral("econCtrlScroll"));
    ctrl_scroll->setFrameShape(QFrame::NoFrame);
    ctrl_scroll->setWidgetResizable(true);
    ctrl_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ctrl_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* ctrl_host = new QWidget(ctrl_scroll);
    ctrl_host->setObjectName(QStringLiteral("econCtrlHost"));
    auto* thl = new QHBoxLayout(ctrl_host);
    thl->setContentsMargins(0, 4, 0, 4);
    thl->setSpacing(6);
    build_controls(thl);
    thl->addStretch(1);
    ctrl_scroll->setWidget(ctrl_host);
    thl_outer->addWidget(ctrl_scroll, 1);

    asof_lbl_ = new QLabel(toolbar_);
    asof_lbl_->setObjectName(QStringLiteral("econAsOf"));
    asof_lbl_->setToolTip(tr("When the data on screen was retrieved"));
    thl_outer->addWidget(asof_lbl_);

    fetch_btn_ = new QPushButton(tr("FETCH"), toolbar_);
    fetch_btn_->setObjectName("econFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    fetch_btn_->setAccessibleName(tr("Fetch data"));
    fetch_btn_->setAccessibleDescription(tr("Run the query against the %1 data source").arg(source_id_));
    fetch_btn_->setToolTip(tr("Run the query (Enter)"));
    connect(fetch_btn_, &QPushButton::clicked, this, &EconPanelBase::trigger_fetch);

    export_btn_ = new QPushButton(tr("CSV"), toolbar_);
    export_btn_->setObjectName("econCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    export_btn_->setAccessibleName(tr("Export results as CSV"));
    export_btn_->setToolTip(tr("Export the full result set (all pages) as CSV"));
    export_btn_->setEnabled(false); // until a result is on screen
    connect(export_btn_, &QPushButton::clicked, this, &EconPanelBase::export_csv);

    thl_outer->addWidget(fetch_btn_);
    thl_outer->addWidget(export_btn_);
    root->addWidget(toolbar_);
    fit_toolbar();

    // Explicit keyboard tab order across the query form, in visual order,
    // ending on FETCH → CSV. Without this Qt falls back to construction order,
    // which for the sidebar panels starts focus in the results table.
    // Also make Enter in any toolbar text field run the query (it supersedes one in flight).
    {
        QWidget* prev = nullptr;
        for (int i = 0; i < thl->count(); ++i) {
            QWidget* w = thl->itemAt(i)->widget();
            if (!w || !(w->focusPolicy() & Qt::TabFocus))
                continue;
            if (prev)
                QWidget::setTabOrder(prev, w);
            prev = w;
            if (auto* le = qobject_cast<QLineEdit*>(w))
                connect(le, &QLineEdit::returnPressed, this, &EconPanelBase::trigger_fetch);
        }
        if (prev)
            QWidget::setTabOrder(prev, fetch_btn_);
        QWidget::setTabOrder(fetch_btn_, export_btn_);
    }

    // Results: the pane shows the remembered result, the loading skeleton, a preview, the needs-setup
    // page or an error; its content is the stat cards, the title bar and the result stack.
    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, container);
    auto* results = new QWidget(pane_->content_parent());
    auto* rvl = new QVBoxLayout(results);
    rvl->setContentsMargins(0, 0, 0, 0);
    rvl->setSpacing(0);

    // Stat cards row
    cards_row_ = new QWidget(results);
    cards_row_->setObjectName("econCardsRow");
    auto* crhl = new QHBoxLayout(cards_row_);
    crhl->setContentsMargins(10, 6, 10, 6);
    crhl->setSpacing(6);

    auto make_card = [&](const QString& label, QLabel*& out, QLabel*& title_out, QLabel** sub_out) {
        auto* card = new QWidget(cards_row_);
        card->setObjectName("econStatCard");
        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(10, 6, 10, 6);
        vl->setSpacing(1);
        auto* lbl = new QLabel(label, card);
        lbl->setObjectName("econStatLabel");
        title_out = lbl;
        out = new QLabel(QStringLiteral("—"), card);
        out->setObjectName("econStatVal");
        vl->addWidget(lbl);
        vl->addWidget(out);
        if (sub_out) {
            *sub_out = new QLabel(card);
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
    cards_row_->hide(); // shown for a single dated series
    rvl->addWidget(cards_row_);

    // Title bar: title + record count, then the provenance line, then an optional warning banner.
    title_bar_ = new QWidget(results);
    title_bar_->setObjectName("econTitleBar");
    auto* tbvl = new QVBoxLayout(title_bar_);
    tbvl->setContentsMargins(12, 4, 12, 4);
    tbvl->setSpacing(1);
    auto* tbhl = new QHBoxLayout;
    tbhl->setContentsMargins(0, 0, 0, 0);
    title_lbl_ = new QLabel(title_bar_);
    title_lbl_->setObjectName("econTitleLbl");
    title_lbl_->setWordWrap(true);
    row_count_ = new QLabel(title_bar_);
    row_count_->setObjectName("econRowCount");
    tbhl->addWidget(title_lbl_, 1);
    tbhl->addWidget(row_count_);
    tbvl->addLayout(tbhl);
    meta_lbl_ = new QLabel(title_bar_);
    meta_lbl_->setObjectName("econMetaLbl");
    meta_lbl_->setWordWrap(true);
    meta_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    meta_lbl_->hide();
    tbvl->addWidget(meta_lbl_);
    banner_lbl_ = new QLabel(title_bar_);
    banner_lbl_->setObjectName("econBannerLbl");
    banner_lbl_->setWordWrap(true);
    banner_lbl_->hide();
    tbvl->addWidget(banner_lbl_);
    rvl->addWidget(title_bar_);

    // Content stack: 0 = chart + table + pager, >= 1 = subclass pages.
    stack_ = new QStackedWidget(results);

    auto* table_page = new QWidget(stack_);
    auto* tpl = new QVBoxLayout(table_page);
    tpl->setContentsMargins(0, 0, 0, 0);
    tpl->setSpacing(0);

    // The table shows one page of the result through a model: no item per cell, text formatted when
    // a row is first painted, colours handed in (re-applied on an appearance change).
    table_ = new QTableView(table_page);
    model_ = new econ_table::EconPageModel(table_);
    model_->set_number_text([](double v, bool groupable) { return econ_pbd::number_cell(v, groupable); });
    model_->set_colors(ui::colors::legible_color(QColor(color_)), QColor(ui::colors::POSITIVE()),
                       QColor(ui::colors::NEGATIVE()));
    table_->setModel(model_);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(true);
    table_->setWordWrap(false);
    table_->setAccessibleName(tr("Results table"));
    // Interactive columns: render_page() sizes them once after each fill (never resize-to-contents
    // while rows are being filled).
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    // Header click sorts the FULL result set (all pages), not just the visible page — on a worker.
    table_->horizontalHeader()->setSectionsClickable(true);
    table_->horizontalHeader()->setToolTip(tr("Click a column header to sort all rows"));
    connect(table_->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int section) {
        if (!analysis_ || section < 0 || section >= model_->columns().size())
            return;
        const Qt::SortOrder order = (section == sort_column_ && sort_order_ == Qt::DescendingOrder)
                                        ? Qt::AscendingOrder
                                        : Qt::DescendingOrder;
        request_sort(section, order);
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

    // Results = chart of the series (when the rows are one) above the detail table; the splitter
    // lets the user trade chart height for table rows.
    results_split_ = new QSplitter(Qt::Vertical, table_page);
    results_split_->setObjectName(QStringLiteral("econResultsSplit"));
    results_split_->setChildrenCollapsible(true);
    results_split_->setHandleWidth(3);
    chart_ = new ui::TimeSeriesChart(results_split_);
    chart_->set_show_mode_buttons(false);
    chart_->set_utc(true); // x = period start in UTC: a period never shifts a day with the time zone
    chart_->setMinimumHeight(140);
    chart_->setAccessibleName(tr("Results chart"));
    chart_->hide();
    // The lines are plotted thinned to the pixel width; zooming / panning re-thins the visible part.
    connect(chart_, &ui::TimeSeriesChart::range_changed, this, [this](double, double) { schedule_chart_refine(); });
    results_split_->addWidget(chart_);
    results_split_->addWidget(table_);
    results_split_->setStretchFactor(0, 2);
    results_split_->setStretchFactor(1, 3);
    tpl->addWidget(results_split_, 1);

    pager_ = new ui::PaginationBar(table_page);
    connect(pager_, &ui::PaginationBar::page_changed, this, [this]() { render_page(); });
    tpl->addWidget(pager_);

    stack_->addWidget(table_page); // index 0
    rvl->addWidget(stack_, 1);

    pane_->set_content(results);
    root->addWidget(pane_, 1);

    // The user's number format changed: re-format the page and the cards from what is held.
    ui::formatting::on_format_changed(this, [this]() {
        if (model_)
            model_->reformat();
        apply_stats();
    });
}

void EconPanelBase::fit_toolbar() {
    if (!toolbar_)
        return;
    // Room for the inputs (26 px at the default size, growing with the font) plus a thin scroll bar.
    const int h = std::max(40, ui::fonts::font_px(-3) * 26 / 11 + 14);
    if (toolbar_->minimumHeight() != h)
        toolbar_->setMinimumHeight(h);
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void EconPanelBase::refresh_panel_theme() {
    fit_toolbar();
    // The first column (source accent) and change columns (gain / loss) are model colours, which no
    // stylesheet reaches: hand the new theme's colours to the model (no walk over cells), and size
    // the columns for the new font.
    if (model_) {
        model_->set_colors(ui::colors::legible_color(QColor(color_)), QColor(ui::colors::POSITIVE()),
                           QColor(ui::colors::NEGATIVE()));
        if (model_->rowCount() > 0)
            econ_table::fit_page_columns(table_, model_, ui::fonts::font_px(-3) * 30);
    }
}

QWidget* EconPanelBase::content_host() const {
    return stack_;
}

// ── Results pane ──────────────────────────────────────────────────────────────

EconPanelBase::LoadPolicy EconPanelBase::load_policy() const {
    return LoadPolicy{};
}

QJsonValue EconPanelBase::page_payload(int /*page*/) const {
    return QJsonValue();
}

bool EconPanelBase::restore_page(int /*page*/, const QJsonValue& /*payload*/) {
    return false;
}

void EconPanelBase::showEvent(QShowEvent* event) {
    // The subclass is complete and its restored inputs are in place: set the pane up before its own
    // (deferred) first presentation.
    configure_pane();
    QWidget::showEvent(event);
}

void EconPanelBase::configure_pane() {
    if (pane_configured_ || !pane_)
        return;
    pane_configured_ = true;
    const LoadPolicy pol = load_policy();
    // Order matters: the pane may present itself while it is being configured (it is visible), and
    // its first presentation reads the persistent cache and draws through the render function.
    pane_->set_cache_key(QStringLiteral("economics/") + source_id_, 30);
    // Series of a few thousand observations exceed the default 256 KB; parsing 2 MB on the next
    // first show stays well under the 100 ms stall budget.
    pane_->set_persist_limit(2 * 1024 * 1024);
    // Results of several views stay in memory within this budget; their size is the analysis'
    // estimate carried in the payload — the pane never walks a large payload to measure it.
    pane_->set_memory_budget(econ_pbd::kPaneMemoryBudget);
    pane_->set_size_estimator([](const QJsonValue& p) { return econ_pbd::payload_bytes(p); });
    pane_->set_start([this](quint64 id) { start_run(id); });
    pane_->set_cancel([this](quint64 id) { cancel_run(id); });
    pane_->set_render(
        [this](const QJsonValue& p, const QDateTime&, bool from_cache) { render_payload(p, from_cache); });
    pane_->set_status_label(asof_lbl_);
    pane_->skeleton()->set_shape(pol.shape);
    const QString src = default_source();
    pane_->set_loading_text(tr("Loading from %1…").arg(src.isEmpty() ? source_id_ : src));
    pane_->set_timeout(pol.timeout_s);
    pane_->set_auto_delay(250); // users flick through the source badges
    connect(pane_, &ui::AutoLoadPane::notice_action, this, &EconPanelBase::run_query);
    if (pol.manual) {
        pane_->preview()->apply(ui::EmptyState::preview_spec(
            pol.preview_title, pol.preview_message, pol.preview_run.isEmpty() ? tr("FETCH") : pol.preview_run,
            pol.preview_note, ui::EmptyState::Tone::Warning, pol.shape, input_summary(), tr("WHAT WILL BE LOADED")));
        if (!pol.preview_secondary.isEmpty())
            pane_->preview()->set_secondary_action(pol.preview_secondary);
    }

    // The remembered result is the last query the user ran, and the panel reopens on that query's
    // inputs (EconomicsScreen saves them when a query starts). Each run is tagged with the inputs'
    // signature when it starts (start_run); when the inputs on screen no longer match the result's,
    // the pane says "Inputs changed — RUN AGAIN". Saved values still waiting for their selector
    // list count as set (query_signature), so lists that fill after opening raise nothing.
    watch_inputs();
    pane_->set_signature(query_signature());

    pane_->set_max_age(pol.max_age_s);
    pane_->set_requirement_check([this]() { return ui::check_requirements(load_requirements()); });
    pane_->set_policy(pol.manual ? ui::AutoLoadPane::Policy::Manual : ui::AutoLoadPane::Policy::AutoRun);
}

void EconPanelBase::watch_inputs() {
    if (inputs_watched_)
        return;
    inputs_watched_ = true;
    const bool manual = load_policy().manual;
    // Any change of an input re-checks the signature (and, for a manual panel, the preview's list of
    // what a run would use), once per event-loop turn.
    for (QWidget* w : persistable_inputs()) {
        auto changed = [this, manual]() {
            schedule_signature();
            if (manual)
                schedule_preview_refresh();
        };
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            connect(cb, &QComboBox::currentIndexChanged, this, changed);
            if (cb->isEditable())
                connect(cb, &QComboBox::editTextChanged, this, changed);
        } else if (auto* le = qobject_cast<QLineEdit*>(w)) {
            connect(le, &QLineEdit::textChanged, this, changed);
        } else if (auto* dt = qobject_cast<QDateTimeEdit*>(w)) {
            connect(dt, &QDateTimeEdit::dateTimeChanged, this, changed);
        } else if (auto* sb = qobject_cast<QSpinBox*>(w)) {
            connect(sb, &QSpinBox::valueChanged, this, changed);
        } else if (auto* ds = qobject_cast<QDoubleSpinBox*>(w)) {
            connect(ds, &QDoubleSpinBox::valueChanged, this, changed);
        } else if (auto* ck = qobject_cast<QCheckBox*>(w)) {
            connect(ck, &QCheckBox::toggled, this, changed);
        } else if (auto* lw = qobject_cast<QListWidget*>(w)) {
            connect(lw, &QListWidget::currentItemChanged, this, changed);
        }
    }
}

QString EconPanelBase::query_signature() const {
    QVariantMap values;
    const QHash<QString, QWidget*> keyed = keyed_inputs();
    for (auto it = keyed.cbegin(); it != keyed.cend(); ++it) {
        if (it.value()->property(econ_pbd::kLocalInputProp).toBool())
            continue;
        // A saved value still waiting for its list counts as chosen: the query uses it.
        const QVariant v = pending_inputs_.contains(it.key()) ? pending_inputs_.value(it.key())
                                                              : econ_pbd::read_input(it.value());
        if (v.isValid())
            values.insert(it.key(), v);
    }
    return ui::autoload::signature_of(values);
}

void EconPanelBase::schedule_signature() {
    if (signature_posted_ || !pane_configured_)
        return;
    signature_posted_ = true;
    QTimer::singleShot(0, this, [this]() {
        signature_posted_ = false;
        if (pane_)
            pane_->set_signature(query_signature());
    });
}

void EconPanelBase::mark_local_input(QWidget* input) {
    if (input)
        input->setProperty(econ_pbd::kLocalInputProp, true);
}

QList<QPair<QString, QString>> EconPanelBase::input_summary() const {
    // "LABEL: value" for each toolbar label followed by its input, as the user sees them.
    QList<QPair<QString, QString>> rows;
    if (!toolbar_)
        return rows;
    const auto* host = toolbar_->findChild<QWidget*>(QStringLiteral("econCtrlHost"));
    if (!host || !host->layout())
        return rows;
    QLayout* l = host->layout();
    QString label;
    for (int i = 0; i < l->count() && rows.size() < 6; ++i) {
        QWidget* w = l->itemAt(i)->widget();
        if (!w)
            continue;
        if (auto* lbl = qobject_cast<QLabel*>(w)) {
            label = lbl->text().trimmed();
            continue;
        }
        QString value;
        if (auto* cb = qobject_cast<QComboBox*>(w))
            value = cb->currentText();
        else if (auto* le = qobject_cast<QLineEdit*>(w))
            value = le->text().isEmpty() ? le->placeholderText() : le->text();
        else if (auto* sb = qobject_cast<QAbstractSpinBox*>(w))
            value = sb->text();
        if (!label.isEmpty() && !value.trimmed().isEmpty())
            rows.append({label, value.trimmed()});
        label.clear();
    }
    return rows;
}

void EconPanelBase::refresh_preview() {
    if (!pane_ || pane_->policy() != ui::AutoLoadPane::Policy::Manual)
        return;
    pane_->preview()->set_details(input_summary(), tr("WHAT WILL BE LOADED"));
}

void EconPanelBase::schedule_preview_refresh() {
    if (preview_posted_)
        return;
    preview_posted_ = true;
    QTimer::singleShot(0, this, [this]() {
        preview_posted_ = false;
        refresh_preview();
    });
}

void EconPanelBase::recheck_load_requirements() {
    if (pane_)
        pane_->recheck_requirements();
}

void EconPanelBase::set_load_view(const QString& view) {
    // The view is entered with the inputs' signature, so its remembered result is compared with the
    // inputs on screen before anything shows.
    if (pane_)
        pane_->set_view(view, pane_->policy(), query_signature());
}

void EconPanelBase::set_view_load_policy(const QString& view, ui::LoadingSkeleton::Shape shape, int max_age_s) {
    if (!pane_)
        return;
    pane_->set_view_skeleton(view, shape);
    pane_->set_view_max_age(view, max_age_s);
}

void EconPanelBase::invalidate_result() {
    if (pane_)
        pane_->invalidate();
}

QVariant EconPanelBase::pending_input(QWidget* input) const {
    if (pending_inputs_.isEmpty() || !input)
        return {};
    const QHash<QString, QWidget*> keyed = keyed_inputs();
    for (auto it = keyed.cbegin(); it != keyed.cend(); ++it)
        if (it.value() == input)
            return pending_inputs_.value(it.key());
    return {};
}

void EconPanelBase::set_label_tone(QLabel* label, const char* tone) {
    if (!label)
        return;
    const QString t = QString::fromLatin1(tone);
    if (label->property("econTone").toString() == t)
        return;
    label->setProperty("econTone", t);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

bool EconPanelBase::keeps_state_on_activate() const {
    return pane_ && (pane_->has_result() || pane_->is_running());
}

bool EconPanelBase::query_running() const {
    return pane_ && run_id_ != 0 && pane_->is_current(run_id_);
}

bool EconPanelBase::query_is_automatic() const {
    return pane_ && pane_->is_automatic_run(run_id_);
}

void EconPanelBase::trigger_fetch() {
    emit query_started();
    run_query();
}

void EconPanelBase::run_query() {
    configure_pane();
    if (pane_)
        pane_->run(); // supersedes a query in flight (cancelled through cancel_run)
}

void EconPanelBase::start_run(quint64 id) {
    run_id_ = id;
    run_issued_ = false;
    run_requests_.clear();
    run_env_key_.clear();
    pending_ = QJsonObject();
    live_table_pending_ = false;
    pending_table_drawn_ = false;
    retrieved_at_ = QDateTime();
    retrieved_from_cache_ = false;
    // Inside StartFn: this run's result carries the inputs it was started with.
    if (pane_)
        pane_->set_signature(query_signature());
    if (adopting_)
        return; // the result is already on its way (it arrived outside a run)
    on_fetch();
    // A query that sent nothing and reported nothing (inputs it silently refused) must not leave the
    // skeleton up until the time-out.
    QTimer::singleShot(0, this, [this, id]() { check_run_started(id); });
}

void EconPanelBase::check_run_started(quint64 id) {
    if (!pane_ || !pane_->is_current(id) || run_issued_ || delivery_posted_ || live_table_pending_)
        return;
    pane_->fail(id,
                ui::EmptyState::no_data_spec(tr("Nothing to fetch for these inputs"),
                                             tr("Complete the inputs above, then press FETCH."), tr("FETCH")),
                {}, {});
    finish_run();
}

void EconPanelBase::cancel_run(quint64 id) {
    if (id != run_id_)
        return;
    auto& svc = services::EconomicsService::instance();
    for (const QString& rid : std::as_const(run_requests_))
        svc.cancel(rid); // the script is killed; its answer never reaches on_result()
    run_requests_.clear();
    run_id_ = 0;
    pending_ = QJsonObject();
    live_table_pending_ = false; // its analysis is dropped when it comes back
    pending_table_drawn_ = false;
    on_query_cancelled();
}

void EconPanelBase::finish_run() {
    // The run is over: anything it still waits for is not wanted any more.
    auto& svc = services::EconomicsService::instance();
    for (const QString& rid : std::as_const(run_requests_))
        if (svc.is_running(rid))
            svc.cancel(rid);
    run_requests_.clear();
    run_id_ = 0;
    live_table_pending_ = false;
}

quint64 EconPanelBase::ensure_run() {
    configure_pane();
    if (!pane_)
        return 0;
    if (run_id_ != 0 && pane_->is_current(run_id_))
        return run_id_;
    // A result (or a failure) arrived outside a run — a panel that fetches on its own (a list click,
    // a second step): open a run for it without calling on_fetch() again.
    adopting_ = true;
    pane_->run();
    adopting_ = false;
    return pane_->is_current(run_id_) ? run_id_ : 0;
}

void EconPanelBase::execute_query(const QString& source_id, const QString& script, const QString& command,
                                  const QStringList& args, const QString& request_id, bool fresh) {
    ensure_run();
    run_issued_ = true;
    if (!run_requests_.contains(request_id))
        run_requests_.append(request_id);
    auto& svc = services::EconomicsService::instance();
    if (fresh) // refresh() drops the cached answer, then runs the call (tracked like execute())
        svc.refresh({svc.remember_topic(source_id, script, command, args, request_id)});
    else
        svc.execute(source_id, script, command, args, request_id);
}

void EconPanelBase::mark_query_pending() {
    ensure_run();
    run_issued_ = true;
}

void EconPanelBase::show_loading(const QString& msg) {
    if (in_render_ || !pane_)
        return;
    if (!ensure_run())
        return;
    const QString text = msg.isEmpty() ? tr("Fetching data…") : msg;
    pane_->set_loading_text(text);
    pane_->skeleton()->set_message(text);
}

void EconPanelBase::show_error(const QString& msg) {
    if (in_render_ || !pane_)
        return;
    const econ_pbd::Failure f = econ_pbd::classify_failure(msg);
    if (f.kind == econ_pbd::FailKind::Cancelled)
        return;
    const quint64 id = ensure_run();
    if (!id)
        return;
    pending_ = QJsonObject();
    live_table_pending_ = false;
    using Tone = ui::EmptyState::Tone;
    ui::EmptyState::Spec spec;
    ui::AutoLoadPane::ActionFn primary; // empty = RETRY
    ui::AutoLoadPane::ActionFn secondary;
    const QString sentence = f.message.isEmpty() ? tr("The source did not answer.") : f.message;
    // The credential the failure names: in its text, else the one the script reported (env_key).
    const QString named_key = f.env_key.isEmpty() ? run_env_key_ : f.env_key;
    switch (f.kind) {
        case econ_pbd::FailKind::MissingKey: {
            QString key = named_key;
            if (key.isEmpty()) {
                for (const auto& r : load_requirements()) {
                    if (r.kind == ui::RequirementKind::ApiKey) {
                        key = r.name;
                        break;
                    }
                }
            }
            const ui::RequirementStatus st =
                key.isEmpty() ? ui::RequirementStatus{}
                              : ui::check_requirements({ui::Requirement::api_key(key, default_source())});
            if (!st.ok && !st.retry) {
                // The script says the key is missing and the store agrees: the setup page names it.
                spec = ui::EmptyState::needs_setup_spec(st);
                spec.secondary = tr("RETRY");
                const ui::SetupLink link = st.link;
                primary = [this, link]() { ui::open_setup_link(link, this); };
                secondary = [this]() { run_query(); };
            } else {
                spec = ui::EmptyState::error_spec(sentence, tr("The source asks for an API key"), tr("RETRY"),
                                                  f.technical);
            }
            break;
        }
        case econ_pbd::FailKind::AuthFailed: {
            spec = ui::EmptyState::error_spec(sentence, tr("The API key was rejected"),
                                              tr("OPEN SETTINGS > CREDENTIALS"), f.technical);
            spec.secondary = tr("RETRY");
            const ui::SetupLink link{QStringLiteral("settings"), QStringLiteral("credentials"), named_key};
            primary = [this, link]() { ui::open_setup_link(link, this); };
            secondary = [this]() { run_query(); };
            break;
        }
        case econ_pbd::FailKind::RateLimited:
            spec = ui::EmptyState::error_spec(
                f.retry_after_s > 0 ? tr("%1 Try again in %n second(s).", nullptr, f.retry_after_s).arg(sentence)
                                    : tr("%1 Wait a moment, then retry.").arg(sentence),
                tr("Rate limited by the source"), tr("RETRY"), f.technical);
            spec.glyph_tone = Tone::Warning;
            break;
        case econ_pbd::FailKind::UpstreamDown:
            spec = ui::EmptyState::error_spec(sentence, tr("The source is not reachable"), tr("RETRY"), f.technical);
            break;
        case econ_pbd::FailKind::NoData:
            spec = ui::EmptyState::no_data_spec(tr("No data for these inputs"), sentence, tr("RETRY"));
            spec.technical = f.technical;
            break;
        case econ_pbd::FailKind::BadArgs:
            spec = ui::EmptyState::error_spec(sentence, tr("Check the inputs"), tr("RETRY"), f.technical);
            spec.glyph_tone = Tone::Warning;
            break;
        case econ_pbd::FailKind::NotSupported: {
            // The source refuses this kind of access (EconDB blocks anonymous API calls): not a fault
            // to retry at once, and not a missing key.
            const QString src = default_source();
            spec = ui::EmptyState::no_data_spec(
                src.isEmpty() ? tr("Not available from this source") : tr("Not available from %1").arg(src), sentence,
                tr("RETRY"));
            spec.glyph = QStringLiteral("⊘");
            spec.technical = f.technical;
            break;
        }
        case econ_pbd::FailKind::Other:
        case econ_pbd::FailKind::Cancelled:
            spec = ui::EmptyState::error_spec(sentence, QString(), QString(), f.technical);
            break;
    }
    pane_->fail(id, spec, primary, secondary);
    finish_run();
}

void EconPanelBase::show_error_with_action(const QString& msg, const QString& action_label,
                                           std::function<void()> action) {
    if (in_render_ || !pane_)
        return;
    if (action_label.isEmpty() || !action) {
        show_error(msg);
        return;
    }
    const quint64 id = ensure_run();
    if (!id)
        return;
    pending_ = QJsonObject();
    live_table_pending_ = false;
    const econ_pbd::Failure f = econ_pbd::classify_failure(msg);
    ui::EmptyState::Spec spec = ui::EmptyState::error_spec(
        f.message.isEmpty() ? tr("The source did not answer.") : f.message, QString(), tr("RETRY"), f.technical);
    spec.secondary = action_label;
    pane_->fail(id, spec, {}, [action]() { action(); });
    finish_run();
}

void EconPanelBase::show_empty(const QString& msg) {
    if (in_render_ || !pane_)
        return;
    // Outside a query this changes nothing: the pane already shows the remembered result, the
    // default query or the preview.
    if (!query_running())
        return;
    pending_ = QJsonObject();
    live_table_pending_ = false;
    pane_->fail(run_id_,
                ui::EmptyState::no_data_spec(tr("No data for these inputs"),
                                             msg.isEmpty() ? tr("The source returned nothing for this selection.")
                                                           : msg,
                                             tr("RETRY")),
                {}, {});
    finish_run();
}

void EconPanelBase::show_table() {
    if (!stack_)
        return;
    stack_->setCurrentIndex(0);
    if (!in_render_ && query_running()) {
        pending_.insert(QStringLiteral("page"), 0);
        queue_delivery();
    }
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
    if (in_render_) {
        page_shown_in_render_ = true;
        return;
    }
    // The page is the query's result (a panel-specific page, e.g. CFTC sentiment cards).
    if (!ensure_run())
        return;
    pending_.insert(QStringLiteral("page"), index);
    pending_.insert(QStringLiteral("custom"), page_payload(index));
    queue_delivery();
}

void EconPanelBase::queue_delivery() {
    if (delivery_posted_)
        return;
    delivery_posted_ = true;
    // Delivered on the next turn of the event loop, so what the panel sets right after display()
    // (banner, stat visibility) is part of the remembered result.
    QTimer::singleShot(0, this, &EconPanelBase::flush_delivery);
}

void EconPanelBase::flush_delivery() {
    delivery_posted_ = false;
    if (!pane_ || pending_.isEmpty())
        return;
    if (live_table_pending_)
        return; // the query's table is still being analysed: delivered when it is applied
    const quint64 id = ensure_run();
    if (!id) {
        pending_ = QJsonObject();
        return;
    }
    static quint64 serial_counter = 0;
    static const QString session = QString::number(QDateTime::currentMSecsSinceEpoch(), 36);
    QJsonObject payload = pending_;
    pending_ = QJsonObject();
    if (!payload.contains(QStringLiteral("page")))
        payload.insert(QStringLiteral("page"), stack_ ? stack_->currentIndex() : 0);
    payload.insert(QStringLiteral("banner"), banner_lbl_ ? banner_lbl_->text() : QString());
    payload.insert(QStringLiteral("stats"), stats_allowed_);
    const QString serial = session + QLatin1Char('.') + QString::number(++serial_counter);
    payload.insert(QStringLiteral("serial"), serial);
    // Only a result whose table is on screen is "drawn live": one whose analysis was overtaken (the
    // user switched the view meanwhile) must be drawn by the pane when its view comes back.
    if (pending_table_drawn_ || !payload.contains(QStringLiteral("table")))
        rendered_serial_ = serial;
    pending_table_drawn_ = false;
    const QDateTime at = retrieved_at_.isValid() ? retrieved_at_ : QDateTime::currentDateTimeUtc();
    pane_->deliver(id, payload, at);
    finish_run();
}

void EconPanelBase::render_payload(const QJsonValue& payload, bool from_cache) {
    const QJsonObject p = payload.toObject();
    const int page = p.value(QStringLiteral("page")).toInt(0);
    if (!from_cache && !rendered_serial_.isEmpty() &&
        p.value(QStringLiteral("serial")).toString() == rendered_serial_) {
        // Drawn live a moment ago by display() / the panel: only make sure its page is current.
        if (stack_ && page >= 0 && page < stack_->count() && stack_->currentIndex() != page)
            stack_->setCurrentIndex(page);
        if (export_btn_)
            export_btn_->setEnabled(analysis_ && !analysis_->rows.isEmpty());
        return;
    }
    in_render_ = true;
    const QJsonObject table = p.value(QStringLiteral("table")).toObject();
    if (!table.isEmpty()) {
        // The remembered rows are analysed on the worker like a fresh result; the table, cards and
        // chart fill when that is done.
        begin_table(table);
        start_table_analysis(table, 0);
    }
    bool shown = true;
    if (page >= 1) {
        // restore_page() may build a lazily created page first (its index can then differ from the
        // remembered one) and show it itself; otherwise the remembered index is shown.
        page_shown_in_render_ = false;
        shown = stack_ && restore_page(page, p.value(QStringLiteral("custom")));
        if (shown && !page_shown_in_render_) {
            shown = page < stack_->count();
            if (shown)
                stack_->setCurrentIndex(page);
        }
    } else if (stack_) {
        stack_->setCurrentIndex(0);
    }
    set_stats_visible(p.value(QStringLiteral("stats")).toBool(true));
    set_banner(p.value(QStringLiteral("banner")).toString());
    rendered_serial_ = p.value(QStringLiteral("serial")).toString();
    in_render_ = false;
    if (export_btn_)
        export_btn_->setEnabled(table.isEmpty() && analysis_ && !analysis_->rows.isEmpty());
    if (!shown && pane_) {
        // This page cannot be rebuilt from what was remembered: run the query again.
        QTimer::singleShot(0, pane_, [guard = QPointer<ui::AutoLoadPane>(pane_)]() {
            if (guard)
                guard->invalidate();
        });
    }
}

void EconPanelBase::set_stats_visible(bool visible) {
    stats_allowed_ = visible;
    apply_stats_visibility();
}

void EconPanelBase::apply_stats_visibility() {
    if (cards_row_)
        cards_row_->setVisible(stats_allowed_ && analysis_ && analysis_->stats.applicable);
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
    const SeriesMeta m = std::exchange(next_meta_, SeriesMeta{});
    if (!pane_)
        return;
    if (rows.isEmpty()) {
        show_empty(tr("The source returned no rows for this selection."));
        return;
    }
    // Everything needed to draw this result again — after a restart too (the pane remembers it).
    QJsonObject meta{{QStringLiteral("units"), m.units},
                     {QStringLiteral("frequency"), m.frequency},
                     {QStringLiteral("seasonal_adj"), m.seasonal_adj},
                     {QStringLiteral("source"), m.source},
                     {QStringLiteral("last_updated"), m.last_updated},
                     {QStringLiteral("note"), m.note},
                     {QStringLiteral("percent"), m.percent},
                     {QStringLiteral("value_column"), m.value_column}};
    if (m.stats_cutoff.isValid())
        meta.insert(QStringLiteral("stats_cutoff"), m.stats_cutoff.toString(Qt::ISODate));
    QJsonObject table{{QStringLiteral("rows"), rows},
                      {QStringLiteral("title"), title},
                      {QStringLiteral("order"), QJsonArray::fromStringList(column_order)},
                      {QStringLiteral("keep"), keep_row_order},
                      {QStringLiteral("meta"), meta}};
    if (retrieved_at_.isValid()) {
        table.insert(QStringLiteral("fetched"), retrieved_at_.toString(Qt::ISODate));
        table.insert(QStringLiteral("cached"), retrieved_from_cache_);
    }
    // The query this table answers (a result that arrived outside a run opens one); none while the
    // pane draws a remembered result.
    const quint64 live = in_render_ ? 0 : ensure_run();
    begin_table(table);
    if (stack_)
        stack_->setCurrentIndex(0);
    if (live) {
        // Delivered to the pane once the analysis has been applied (apply → queue_delivery). Set up
        // before the analysis starts: a small table can come back before start_table_analysis returns.
        run_issued_ = true; // the result is being prepared — not "nothing to fetch"
        pending_.insert(QStringLiteral("table"), table);
        pending_.insert(QStringLiteral("page"), 0);
        pending_.remove(QStringLiteral("custom"));
        pending_.remove(QStringLiteral("bytes"));
    }
    start_table_analysis(table, live);
}

void EconPanelBase::begin_table(const QJsonObject& t) {
    // The facts the provenance line and the delivery need now; the rows come with the analysis.
    if (t.contains(QStringLiteral("fetched")))
        set_retrieval_info(QDateTime::fromString(t.value(QStringLiteral("fetched")).toString(), Qt::ISODate),
                           t.value(QStringLiteral("cached")).toBool());
    // Cleared now: what the panel sets right after display() (a source notice) must stay.
    set_banner({});
    if (in_render_ || !query_running()) {
        // A remembered result replaces what is on screen at once: never another result's rows under
        // this title. (A query's own result keeps the previous one dimmed until it is ready.)
        title_ = t.value(QStringLiteral("title")).toString();
        if (title_lbl_)
            title_lbl_->setText(title_);
        if (row_count_)
            row_count_->setText(tr("%n record(s)", "", static_cast<int>(t.value(QStringLiteral("rows")).toArray().size())));
        analysis_.reset();
        if (model_)
            model_->clear();
        if (pager_)
            pager_->set_total(0);
        if (chart_) {
            chart_->clear();
            chart_->hide();
        }
        chart_series_.clear();
        apply_stats_visibility();
        if (meta_lbl_)
            meta_lbl_->hide();
        if (export_btn_)
            export_btn_->setEnabled(false);
    }
}

void EconPanelBase::start_table_analysis(const QJsonObject& table, quint64 live_run) {
    const quint64 gen = ++table_gen_;
    ++sort_gen_; // a sort still running is for the rows being replaced
    if (live_run) {
        live_gen_ = gen;
        live_table_pending_ = true;
        pending_table_drawn_ = false;
    }
    QPointer<EconPanelBase> self(this);
    (void)QtConcurrent::run(worker_pool(), [table]() {
        return std::make_shared<const econ_table::Analysis>(econ_table::analyse(table));
    }).then(this, [self, gen, live_run](std::shared_ptr<const econ_table::Analysis> a) {
        if (!self)
            return;
        EconPanelBase* p = self.data();
        const bool latest = gen == p->table_gen_;
        if (live_run) {
            if (gen != p->live_gen_)
                return; // an earlier table of the same query, replaced by a later display()
            if (live_run != p->run_id_ || !p->query_running() || !p->live_table_pending_)
                return; // the query was cancelled / failed meanwhile: its rows are not shown (the
                        // previous result stayed on screen — a query's own table never clears it)
            p->live_table_pending_ = false;
            if (latest) {
                p->apply_table(a);
                p->pending_table_drawn_ = true;
                p->pending_.insert(QStringLiteral("bytes"), static_cast<double>(a->approx_bytes));
            }
            // Overtaken by a remembered result of another view (the user switched views): still
            // delivered, so the run ends and its view remembers the result — the pane draws it when
            // that view comes back.
            p->queue_delivery();
            return;
        }
        if (latest)
            p->apply_table(a);
    });
}

void EconPanelBase::apply_table(const std::shared_ptr<const econ_table::Analysis>& a) {
    analysis_ = a;
    const econ_table::Meta& m = a->meta;
    meta_ = SeriesMeta{};
    meta_.units = m.units;
    meta_.frequency = m.frequency;
    meta_.seasonal_adj = m.seasonal_adj;
    meta_.source = m.source;
    meta_.last_updated = m.last_updated;
    meta_.note = m.note;
    meta_.percent = m.percent;
    meta_.value_column = m.value_column;
    meta_.stats_cutoff = m.stats_cutoff;
    title_ = a->title;

    sort_column_ = a->sort_column;
    sort_order_ = a->sort_descending ? Qt::DescendingOrder : Qt::AscendingOrder;
    table_->horizontalHeader()->setSortIndicatorShown(sort_column_ >= 0);
    if (sort_column_ >= 0)
        table_->horizontalHeader()->setSortIndicator(sort_column_, sort_order_);
    model_->set_table(a->rows, a->columns, a->order);
    pager_->set_total(model_->total_rows());
    render_page();

    if (title_lbl_)
        title_lbl_->setText(title_);
    apply_stats();
    apply_chart();
    if (row_count_)
        row_count_->setText(tr("%n record(s)", "", static_cast<int>(a->rows.size())));
    refresh_provenance();
    if (export_btn_)
        export_btn_->setEnabled(!a->rows.isEmpty());
}

void EconPanelBase::request_sort(int column, Qt::SortOrder order) {
    if (!analysis_ || column < 0 || column >= analysis_->columns.size())
        return;
    // The indicator answers the click at once; the rows follow when the worker has sorted them.
    table_->horizontalHeader()->setSortIndicatorShown(true);
    table_->horizontalHeader()->setSortIndicator(column, order);
    const quint64 gen = ++sort_gen_;
    const quint64 tgen = table_gen_;
    const QJsonArray rows = analysis_->rows;
    const QString key = analysis_->columns[column];
    const bool ascending = order == Qt::AscendingOrder;
    QPointer<EconPanelBase> self(this);
    (void)QtConcurrent::run(worker_pool(), [rows, key, ascending]() {
        return econ_table::sorted_order(rows, key, ascending);
    }).then(this, [self, gen, tgen, column, order](const QVector<int>& sorted) {
        if (!self || gen != self->sort_gen_ || tgen != self->table_gen_)
            return; // another sort or another result since
        self->sort_column_ = column;
        self->sort_order_ = order;
        self->model_->set_order(sorted);
        self->pager_->set_total(self->model_->total_rows());
        self->render_page();
    });
}

void EconPanelBase::render_page() {
    if (!model_ || !pager_)
        return;
    const int start = pager_->offset();
    const int count = std::min<int>(pager_->page_size(), model_->total_rows() - start);

    // Preserve the user's vertical scroll offset across a re-render (page-size change, language
    // switch) so the row they were reading doesn't jump.
    const int scroll_pos = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;
    const int sel_row = table_->currentIndex().isValid() ? table_->currentIndex().row() : -1;

    table_->setUpdatesEnabled(false);
    model_->set_page(start, std::max(0, count));
    // Columns sized once for this page (header + first rows); very long text is capped.
    econ_table::fit_page_columns(table_, model_, ui::fonts::font_px(-3) * 30);
    table_->setUpdatesEnabled(true);

    if (sel_row >= 0 && sel_row < count)
        table_->setCurrentIndex(model_->index(sel_row, 0));
    if (table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(scroll_pos);
}

void EconPanelBase::apply_stats() {
    for (QLabel* l : {stat_latest_, stat_change_, stat_yoy_, stat_min_, stat_max_, stat_avg_})
        if (l)
            l->setText(QStringLiteral("—"));
    for (QLabel* l : {stat_latest_sub_, stat_change_sub_, stat_yoy_sub_})
        if (l)
            l->clear();
    if (stat_change_lbl_)
        stat_change_lbl_->setText(tr("CHG vs PREV"));
    if (yoy_card_)
        yoy_card_->hide();
    freq_label_ = meta_.frequency.trimmed();
    if (!analysis_) {
        if (stat_count_)
            stat_count_->setText(QStringLiteral("—"));
        apply_stats_visibility();
        return;
    }
    const econ_table::Stats& st = analysis_->stats;
    if (stat_count_)
        stat_count_->setText(QString::number(st.applicable ? st.count : static_cast<int>(analysis_->rows.size())));
    if (!st.applicable) {
        apply_stats_visibility();
        return;
    }
    if (freq_label_.isEmpty())
        freq_label_ = econ_pbd::gran_name(st.gran);
    const QString suffix = st.pct ? QStringLiteral("%") : (st.bp ? tr(" bp") : QString());
    auto fmt = [&suffix](double v) { return EconPanelBase::format_number(v) + suffix; };

    if (stat_latest_)
        stat_latest_->setText(fmt(st.latest));
    if (stat_latest_sub_)
        stat_latest_sub_->setText(tr("as of %1").arg(st.latest_label));
    const QString tag = econ_pbd::change_tag(st.gran);
    if (stat_change_lbl_)
        stat_change_lbl_->setText(tag.isEmpty() ? tr("CHG vs PREV") : tr("CHG %1").arg(tag));
    if (st.has_prev)
        econ_pbd::set_change(stat_change_, stat_change_sub_, st.latest, st.prev, st.pct, st.bp,
                             tr("vs %1").arg(st.prev_label));
    if (st.has_yoy) {
        econ_pbd::set_change(stat_yoy_, stat_yoy_sub_, st.latest, st.yoy, st.pct, st.bp, tr("vs %1").arg(st.yoy_label));
        if (yoy_card_)
            yoy_card_->show();
    }
    if (stat_min_)
        stat_min_->setText(fmt(st.min));
    if (stat_max_)
        stat_max_->setText(fmt(st.max));
    if (stat_avg_)
        stat_avg_->setText(fmt(st.avg));
    apply_stats_visibility();
}

int EconPanelBase::chart_px() const {
    // The plot's pixel width (device pixels): lines are thinned to it.
    const int w = chart_ && chart_->width() > 100 ? chart_->width() : (results_split_ ? results_split_->width() : 0);
    const qreal dpr = chart_ ? chart_->devicePixelRatioF() : 1.0;
    return std::max(400, static_cast<int>(std::max(w, 600) * dpr));
}

void EconPanelBase::apply_chart() {
    if (!chart_)
        return;
    chart_->clear();
    chart_series_.clear();
    if (!analysis_ || analysis_->lines.isEmpty()) {
        chart_->hide();
        return;
    }
    const econ_table::Analysis& a = *analysis_;
    const QString suffix = a.chart_pct ? QStringLiteral("%") : (a.chart_bp ? tr(" bp") : QString());
    const auto value_fmt = [suffix](double v) { return EconPanelBase::format_number(v) + suffix; };

    const bool single = a.lines.size() == 1;
    const int px = chart_px();
    for (int i = 0; i < a.lines.size(); ++i) {
        const econ_table::Line& line = a.lines[i];
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = line.name;
        spec.id = line.name;
        // Thinned to the pixel width (min / max per pixel column, extremes kept); a zoom re-thins.
        spec.points = econ_table::decimate(line.points, line.points.first().x(), line.points.last().x(), px);
        if (single) {
            // The provider's identity colour (made legible on the theme by the chart).
            spec.style = ui::TimeSeriesChart::Style::Area;
            spec.color = ui::TimeSeriesChart::ColorRole::Custom;
            spec.custom_color = QColor(color_);
        } else {
            spec.color = ui::TimeSeriesChart::ColorRole::Palette;
            spec.palette_index = i;
        }
        spec.formatter = value_fmt;
        chart_series_.append(chart_->add_series(spec));
    }
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, value_fmt);
    chart_->set_show_legend(!single);
    // Unrelated scales side by side: offer the =100 / % rebasing chips.
    chart_->set_show_mode_buttons(!single);
    const QString units = meta_.units.trimmed();
    chart_->set_title((single && units.isEmpty() ? a.lines.first().name : units).toUpper());
    chart_->set_export_name(source_id_ + QLatin1Char('_') + (title_.isEmpty() ? QStringLiteral("series") : title_));

    // Range presets that suit the spacing of the observations.
    switch (a.range_kind) {
        case econ_table::RangeKind::Daily:
            chart_->set_range_presets({QStringLiteral("1M"), QStringLiteral("3M"), QStringLiteral("1Y"),
                                       QStringLiteral("5Y"), QStringLiteral("ALL")});
            break;
        case econ_table::RangeKind::Monthly:
            chart_->set_range_presets(
                {QStringLiteral("1Y"), QStringLiteral("5Y"), QStringLiteral("10Y"), QStringLiteral("ALL")});
            break;
        case econ_table::RangeKind::Coarse:
            chart_->set_range_presets(
                {QStringLiteral("5Y"), QStringLiteral("10Y"), QStringLiteral("20Y"), QStringLiteral("ALL")});
            break;
    }
    chart_->reset_x_range();

    // Projections (IMF WEO, EIA STEO): shaded, so they are never read as reported data.
    if (meta_.stats_cutoff.isValid()) {
        const double cut =
            static_cast<double>(meta_.stats_cutoff.addDays(1).startOfDay(QTimeZone::UTC).toMSecsSinceEpoch());
        if (a.x_max >= cut)
            chart_->add_band(cut, a.x_max, ui::TimeSeriesChart::ColorRole::Muted, tr("Projection"));
    }
    chart_->set_state(ui::TimeSeriesChart::State::Ready);
    chart_->show();
}

void EconPanelBase::schedule_chart_refine() {
    if (chart_refine_posted_ || chart_series_.isEmpty())
        return;
    chart_refine_posted_ = true;
    QTimer::singleShot(0, this, [this]() {
        chart_refine_posted_ = false;
        refine_chart();
    });
}

void EconPanelBase::refine_chart() {
    if (!chart_ || !analysis_ || chart_series_.size() != analysis_->lines.size())
        return;
    const double from = chart_->x_from();
    const double to = chart_->x_to();
    const int px = chart_px();
    // O(points) per line — a few thousand doubles; the chart redraws once for all of them.
    for (int i = 0; i < chart_series_.size(); ++i) {
        const QVector<QPointF>& full = analysis_->lines[i].points;
        if (full.size() <= 2 * px + 2)
            continue; // never thinned
        chart_->set_series_data(chart_series_[i], econ_table::decimate(full, from, to, px));
    }
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
    const econ_table::Stats* st = analysis_ ? &analysis_->stats : nullptr;
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
    if (st && st->applicable && !st->latest_label.isEmpty())
        parts << tr("Latest obs: %1").arg(st->latest_label);
    if (!meta_.last_updated.trimmed().isEmpty())
        parts << tr("Updated by source: %1").arg(meta_.last_updated.trimmed());
    if (retrieved_at_.isValid()) {
        const QString when = retrieved_at_.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        parts << (retrieved_from_cache_ ? tr("Retrieved %1 (cached)").arg(when) : tr("Retrieved %1").arg(when));
    }
    if (st && st->applicable && stats_allowed_ && !st->column.isEmpty() &&
        !econ_table::value_keys().contains(st->column))
        parts << tr("Cards: %1").arg(st->column);
    if (st && st->applicable && st->excluded > 0)
        parts << tr("%n projected period(s) kept out of the cards", "", st->excluded);
    if (!meta_.note.trimmed().isEmpty())
        parts << meta_.note.trimmed();
    const QString text = parts.join(QStringLiteral("  ·  "));
    meta_lbl_->setText(text);
    meta_lbl_->setToolTip(text);
    meta_lbl_->setVisible(!text.isEmpty());
}

// ── CSV / clipboard ───────────────────────────────────────────────────────────

void EconPanelBase::export_csv() {
    // Export the FULL result set in the order on screen, not just the page the table holds. The text
    // is built and written on a worker: a 23k-row series is megabytes of CSV.
    if (!analysis_ || analysis_->rows.isEmpty() || analysis_->columns.isEmpty())
        return;

    const QString path =
        QFileDialog::getSaveFileName(this, tr("Export CSV"), source_id_ + "_data.csv", tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    // Provenance header — an economics export is useless without its source, units and vintage.
    QString header;
    header += QStringLiteral("# source=") + (meta_.source.isEmpty() ? default_source() : meta_.source) +
              QStringLiteral(" (") + source_id_ + QStringLiteral(")") + QStringLiteral("  exported=") +
              QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + QStringLiteral("  rows=") +
              QString::number(analysis_->rows.size()) + QLatin1Char('\n');
    header += QStringLiteral("# title=") + title_ + QLatin1Char('\n');
    if (!meta_.units.isEmpty() || !freq_label_.isEmpty() || !meta_.seasonal_adj.isEmpty())
        header += QStringLiteral("# units=") + meta_.units + QStringLiteral("  frequency=") + freq_label_ +
                  QStringLiteral("  seasonal_adjustment=") + meta_.seasonal_adj + QLatin1Char('\n');
    if (retrieved_at_.isValid())
        header += QStringLiteral("# retrieved=") + retrieved_at_.toString(Qt::ISODate) +
                  (retrieved_from_cache_ ? QStringLiteral(" (cached)") : QString()) + QLatin1Char('\n');

    const QJsonArray rows = analysis_->rows;
    const QStringList columns = analysis_->columns;
    const QVector<int> order = model_ ? model_->order() : QVector<int>{};
    QPointer<EconPanelBase> self(this);
    (void)QtConcurrent::run(worker_pool(), [path, header, rows, columns, order]() {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return file.errorString();
        const QByteArray text = (header + econ_table::rows_text(rows, columns, order, true)).toUtf8();
        if (file.write(text) != text.size() || !file.commit())
            return file.errorString().isEmpty() ? QStringLiteral("write failed") : file.errorString();
        return QString();
    }).then(this, [self, path](const QString& error) {
        if (!self || error.isEmpty())
            return;
        // Say so — silently returning looked like a successful export.
        LOG_WARN("EconPanel", QString("CSV export failed to write %1: %2").arg(path, error));
        QMessageBox::warning(self.data(), tr("Export CSV"), tr("Could not write %1:\n%2").arg(path, error));
    });
}

void EconPanelBase::copy_rows(bool selection_only) {
    if (!analysis_ || analysis_->columns.isEmpty() || analysis_->rows.isEmpty() || !table_)
        return;
    QVector<int> indices;
    if (selection_only) {
        // A selection lies on the page on screen: at most a page of rows, copied at once.
        if (table_->selectionModel()) {
            const auto rows = table_->selectionModel()->selectedRows();
            for (const auto& idx : rows)
                indices << idx.row();
        }
        if (indices.isEmpty() && table_->currentIndex().isValid())
            indices << table_->currentIndex().row();
        std::sort(indices.begin(), indices.end());
        QVector<int> source;
        for (int r : std::as_const(indices)) {
            const int src = model_->source_row(r);
            if (src >= 0)
                source << src;
        }
        if (source.isEmpty())
            return;
        QGuiApplication::clipboard()->setText(
            econ_table::rows_text(analysis_->rows, analysis_->columns, source, false).trimmed());
        return;
    }
    // Every row, in the order on screen: built on a worker (a large series is megabytes of text).
    const QJsonArray rows = analysis_->rows;
    const QStringList columns = analysis_->columns;
    const QVector<int> order = model_ ? model_->order() : QVector<int>{};
    (void)QtConcurrent::run(worker_pool(), [rows, columns, order]() {
        return econ_table::rows_text(rows, columns, order, false).trimmed();
    }).then(this, [](const QString& text) {
        if (!text.isEmpty())
            QGuiApplication::clipboard()->setText(text);
    });
}

// ── Input persistence ─────────────────────────────────────────────────────────

QList<QWidget*> EconPanelBase::persistable_inputs() const {
    QList<QWidget*> out;
    const QList<QWidget*> all = findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (pane_ && (w == pane_ || pane_->isAncestorOf(w)))
            continue; // results pane: table, pager, panel-specific result pages, state pages
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
    schedule_signature();
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
    schedule_signature();
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

    // Card values / change tag / provenance line / chart band carry translated fragments — re-worded
    // from the analysis on screen (nothing is recomputed).
    if (analysis_) {
        apply_stats();
        apply_chart();
        if (row_count_)
            row_count_->setText(tr("%n record(s)", "", static_cast<int>(analysis_->rows.size())));
        refresh_provenance();
    } else if (stat_change_lbl_) {
        stat_change_lbl_->setText(tr("CHG vs PREV"));
    }
}

} // namespace fincept::screens
