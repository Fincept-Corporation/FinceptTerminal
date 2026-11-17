// src/screens/ai_quant_lab/QuantModulePanel.cpp
//
// Core: widget-factory helpers, refresh_theme, connect_service, build_ui,
// build_generic_panel, clear_results, display_error, show_loading, on_error.
// Result rendering (display_result + the huge on_result dispatch) lives in
// QuantModulePanel_Results.cpp.
#include "screens/ai_quant_lab/QuantModulePanel.h"

#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"

#include <QAbstractAxis>
#include <QAbstractSeries>
#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDate>
#include <QDateTimeAxis>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLegend>
#include <QLocale>
#include <QLineSeries>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QValueAxis>
#include <QVariant>
#include <QXYSeries>

#include <algorithm>
#include <cmath>
#include <memory>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

namespace {

/// Colours and fonts handed to Qt objects that keep them — table item brushes,
/// chart backgrounds, pens, label colours and label fonts — still hold the
/// outgoing appearance after a theme or font switch; the central re-style pass
/// only rewrites stylesheets. Maps each colour that is a token of the other
/// preset (and not also a value of the active one) to the same token of the
/// active preset, re-adapts series tagged with kSeriesColorProp, and re-applies
/// the user's font to chart labels.
void refresh_kept_styles(QWidget* root) {
    using Field = const char* ui::ThemeTokens::*;
    static constexpr Field kFields[] = {
        &ui::ThemeTokens::bg_base,        &ui::ThemeTokens::bg_surface,    &ui::ThemeTokens::bg_raised,
        &ui::ThemeTokens::bg_hover,       &ui::ThemeTokens::border_dim,    &ui::ThemeTokens::border_med,
        &ui::ThemeTokens::border_bright,  &ui::ThemeTokens::text_primary,  &ui::ThemeTokens::text_secondary,
        &ui::ThemeTokens::text_tertiary,  &ui::ThemeTokens::text_dim,      &ui::ThemeTokens::accent,
        &ui::ThemeTokens::accent_dim,     &ui::ThemeTokens::text_on_accent, &ui::ThemeTokens::icon_dim,
        &ui::ThemeTokens::icon_hover,     &ui::ThemeTokens::positive,      &ui::ThemeTokens::positive_dim,
        &ui::ThemeTokens::negative,       &ui::ThemeTokens::negative_dim,  &ui::ThemeTokens::warning,
        &ui::ThemeTokens::info,           &ui::ThemeTokens::cyan,          &ui::ThemeTokens::accent_bg,
        &ui::ThemeTokens::positive_bg,    &ui::ThemeTokens::negative_bg,   &ui::ThemeTokens::row_alt,
    };
    const ui::ThemeTokens& active = ui::ThemeManager::instance().tokens();
    const ui::ThemeTokens& other = active.is_dark ? ui::THEME_DAYLIGHT : ui::THEME_OBSIDIAN;
    const auto rgb = [](const char* hex) { return QColor(hex).rgb() & 0xffffffu; };
    QSet<QRgb> in_active;
    for (const Field f : kFields)
        in_active.insert(rgb(active.*f));
    QHash<QRgb, QRgb> map;
    for (const Field f : kFields) {
        const QRgb from = rgb(other.*f);
        if (!in_active.contains(from) && !map.contains(from))
            map.insert(from, rgb(active.*f));
    }
    const auto remap = [&map](const QColor& c) {
        const auto it = map.constFind(c.rgb() & 0xffffffu);
        if (it == map.constEnd())
            return c;
        QColor out = QColor::fromRgb(*it);
        out.setAlpha(c.alpha());
        return out;
    };
    const auto adapted = [](const QVariant& source, int alpha) {
        QColor c = ui::colors::legible_color(source.value<QColor>());
        c.setAlpha(alpha);
        return c;
    };

    for (auto* table : root->findChildren<QTableWidget*>()) {
        for (int r = 0; r < table->rowCount(); ++r) {
            for (int c = 0; c < table->columnCount(); ++c) {
                QTableWidgetItem* item = table->item(r, c);
                if (!item || item->foreground().style() == Qt::NoBrush)
                    continue;
                const QColor was = item->foreground().color();
                const QColor is = remap(was);
                if (is != was)
                    item->setForeground(is);
            }
        }
    }

    for (auto* view : root->findChildren<QChartView*>()) {
        QChart* chart = view->chart();
        if (!chart)
            continue;
        if (chart->backgroundBrush().style() != Qt::NoBrush)
            chart->setBackgroundBrush(remap(chart->backgroundBrush().color()));
        chart->legend()->setLabelColor(remap(chart->legend()->labelColor()));
        chart->legend()->setFont(ui::fonts::font(-3));
        for (QAbstractAxis* axis : chart->axes()) {
            axis->setLabelsColor(remap(axis->labelsColor()));
            axis->setLabelsFont(ui::fonts::font(-3));
            axis->setGridLineColor(remap(axis->gridLineColor()));
            QPen line = axis->linePen();
            line.setColor(remap(line.color()));
            axis->setLinePen(line);
        }
        for (QAbstractSeries* series : chart->series()) {
            const QVariant source = series->property(kSeriesColorProp);
            if (auto* area = qobject_cast<QAreaSeries*>(series)) {
                const QColor fill = area->brush().color();
                area->setBrush(source.isValid() ? adapted(source, fill.alpha()) : remap(fill));
            } else if (auto* xy = qobject_cast<QXYSeries*>(series)) {
                QPen pen = xy->pen();
                pen.setColor(source.isValid() ? adapted(source, pen.color().alpha()) : remap(pen.color()));
                xy->setPen(pen);
            }
        }
    }
}

// ── Local qlib dataset bar ───────────────────────────────────────────────────
// Factor Discovery, Model Library, Live Signals and Rolling Retraining read a local qlib
// dataset (scripts/ai_quant_lab/qlib_datasets.py): the public qlib snapshot (1999-2020) or
// one built here from Yahoo Finance. A bar under the panel header shows which dataset is in
// use and its range, switches between them, builds one from Yahoo or brings it up to date,
// and moves the panel's date defaults inside the chosen dataset. Its probe is an ordinary
// asynchronous run_module(..., "dataset_info") answered on result_ready (on_result skips
// it). The chosen id reaches the panel's requests through the panel property "qlibDataset";
// "qlibDatasetState" is "none" when there is no dataset at all, so a run explains what to
// do instead of failing in Python.

bool is_qlib_dataset_module(const QString& id) {
    return id == QLatin1String("factor_discovery") || id == QLatin1String("model_library") ||
           id == QLatin1String("live_signals") || id == QLatin1String("rolling_retraining");
}

/// The dataset last picked in any panel this session: a panel opened later starts on it.
QString& qlib_session_choice() {
    static QString id;
    return id;
}

/// One line about a dataset entry of dataset_info: range, size and freshness.
QString qlib_dataset_summary(const QJsonObject& ds) {
    const QLocale loc;
    QString text = QuantModulePanel::tr("%1 → %2  ·  %3 days  ·  %4 instruments")
                       .arg(ds.value("calendar_start").toString(), ds.value("calendar_end").toString())
                       .arg(loc.toString(ds.value("trading_days").toInt()))
                       .arg(loc.toString(ds.value("instruments").toInt()));
    if (ds.value("source").toString() == QLatin1String("qlib_public"))
        text += QuantModulePanel::tr("  ·  frozen snapshot");
    else if (ds.value("up_to_date").toBool())
        text += QuantModulePanel::tr("  ·  up to date");
    else
        text += QuantModulePanel::tr("  ·  newer bars available");
    return text;
}

/// Puts the panel's defaults inside the chosen dataset, from its "defaults" windows
/// (train / valid / test / recent / last_year). Dates follow the dataset: they are reset
/// when the dataset changes, and otherwise only while they still hold what the bar put
/// there (a date the user typed survives a re-read of the same dataset). Instrument and
/// field examples are filled only into empty fields, or ones still holding the bar's value.
void apply_qlib_dataset_defaults(const QString& module_id, const QJsonObject& ds, bool dataset_changed,
                                 const QHash<QString, QLineEdit*>& text, const QHash<QString, QDateEdit*>& dates) {
    const QJsonObject d = ds.value(QStringLiteral("defaults")).toObject();
    auto range = [&d](const char* key) {
        const QJsonArray a = d.value(QLatin1String(key)).toArray();
        return a.size() == 2 ? qMakePair(a.at(0).toString(), a.at(1).toString()) : qMakePair(QString(), QString());
    };
    const QDate first = QDate::fromString(ds.value("calendar_start").toString(), Qt::ISODate);
    const QDate last = QDate::fromString(ds.value("calendar_end").toString(), Qt::ISODate);
    auto refill = [](QWidget* w, const QString& current, bool follow) {
        return follow || current.isEmpty() || current == w->property("qlibAutoFill").toString();
    };
    auto set_text = [&](const char* key, const QString& value, bool follow_dataset) {
        QLineEdit* le = text.value(QLatin1String(key));
        if (!le || value.isEmpty() || !refill(le, le->text(), follow_dataset && dataset_changed))
            return;
        le->setText(value);
        le->setProperty("qlibAutoFill", value);
    };
    auto set_dates = [&](const char* start_key, const char* end_key, const QPair<QString, QString>& r) {
        if (r.first.isEmpty() || !first.isValid() || !last.isValid())
            return;
        const QPair<const char*, QString> parts[] = {{start_key, r.first}, {end_key, r.second}};
        for (const auto& part : parts) {
            QDateEdit* de = dates.value(QLatin1String(part.first));
            if (!de)
                continue;
            const bool fill = refill(de, de->date().toString(Qt::ISODate), dataset_changed);
            de->setDateRange(first, last);
            if (fill) {
                de->setDate(QDate::fromString(part.second, Qt::ISODate));
                de->setProperty("qlibAutoFill", part.second);
            }
        }
    };

    QStringList pools;
    for (const auto& m : ds.value("markets").toArray())
        if (m.toString() != QLatin1String("all"))
            pools << m.toString();
    QStringList sample;
    for (const auto& s : ds.value("sample_instruments").toArray())
        sample << s.toString();
    const QString tickers = sample.join(QLatin1Char(','));
    // A pool trains / backtests the whole universe: only offered for one that runs in minutes.
    const QString pool =
        ds.value("universe_size").toInt() <= 300 ? ds.value("default_market").toString() : QString();
    auto hint = [&](const char* key) {
        if (QLineEdit* le = text.value(QLatin1String(key)))
            le->setPlaceholderText(pools.isEmpty() ? QuantModulePanel::tr("Tickers, e.g. %1").arg(tickers)
                                                   : QuantModulePanel::tr("Tickers (e.g. %1) or a pool: %2")
                                                         .arg(tickers, pools.join(QStringLiteral(", "))));
    };
    const auto train = range("train");
    const auto valid = range("valid");
    const auto test = range("test");
    const auto recent = range("recent");
    const auto year = range("last_year");
    const QString fields = QStringLiteral("$close,$volume");

    if (module_id == QLatin1String("factor_discovery")) {
        set_dates("fd_start", "fd_end", year);
        set_dates("fd_cal_start", "fd_cal_end", recent);
        hint("fd_instruments");
        set_text("fd_instruments", tickers, false);
        set_text("fd_fields", fields, false);
    } else if (module_id == QLatin1String("model_library")) {
        set_text("ml_start", train.first, true);
        set_text("ml_train_end", train.second, true);
        set_text("ml_valid_start", valid.first, true);
        set_text("ml_end", valid.second, true);
        set_text("ml_bt_start", test.first, true);
        set_text("ml_bt_end", test.second, true);
        hint("ml_instruments");
        hint("ml_bt_instr");
        // A pool too large to train in minutes: the dataset's sample tickers instead (an empty field
        // made the first TRAIN fail).
        set_text("ml_instruments", pool.isEmpty() ? tickers : pool, false);
        set_text("ml_bt_instr", pool.isEmpty() ? tickers : pool, false);
    } else if (module_id == QLatin1String("live_signals")) {
        set_text("ls_start", recent.first, true);
        set_text("ls_end", recent.second, true);
        set_text("ls_fa_start", year.first, true);
        set_text("ls_fa_end", year.second, true);
        set_text("ls_pr_start", recent.first, true);
        set_text("ls_pr_end", recent.second, true);
        hint("ls_instruments");
        hint("ls_fa_instr");
        set_text("ls_instruments", tickers, false);
        set_text("ls_fields", fields, false);
        set_text("ls_fa_instr", pool.isEmpty() ? tickers : pool, false);
    } else if (module_id == QLatin1String("rolling_retraining")) {
        if (QLineEdit* le = text.value(QStringLiteral("rr_window")); le && !train.first.isEmpty())
            le->setPlaceholderText(
                QuantModulePanel::tr("Trading days (blank: %1 → %2)").arg(train.first, train.second));
    }
}

/// What the bar needs from its (private-member) panel.
struct QlibBarHooks {
    std::function<QPushButton*(const QString&, QWidget*)> run_button;
    std::function<void(const QString&)> show_loading;
    std::function<void(const QString&)> show_error;
    std::function<void(const QJsonObject&, bool)> apply_defaults; // (dataset entry, dataset changed)
    std::function<void()> state_changed;                          // a probe answered ("ready" / "none")
};

QWidget* make_qlib_dataset_bar(QWidget* panel, const QString& module_id, const QlibBarHooks& hooks) {
    auto* bar = new QWidget(panel);
    bar->setObjectName(QStringLiteral("qlibDatasetBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setStyleSheet(QString("QWidget#qlibDatasetBar { background:%1; border-bottom:1px solid %2; }")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(bar);
    vl->setContentsMargins(16, 6, 16, 6);
    vl->setSpacing(6);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* caption = new QLabel(QuantModulePanel::tr("DATASET"), bar);
    caption->setStyleSheet(QString("color:%1; font-size:" + ui::fonts::px(-4) +
                                   "px; font-weight:700; letter-spacing:1px; background:transparent;")
                               .arg(ui::colors::TEXT_SECONDARY()));
    auto* combo = new QComboBox(bar);
    combo->setStyleSheet(combo_ss());
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(22);
    combo->setAccessibleName(QuantModulePanel::tr("Market dataset"));
    combo->addItem(QuantModulePanel::tr("Checking..."));
    combo->setEnabled(false);
    auto* info = new QLabel(QuantModulePanel::tr("Reading local datasets..."), bar);
    info->setWordWrap(true);
    info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    info->setStyleSheet(QString("color:%1; font-size:" + ui::fonts::px(-3) + "px; background:transparent;")
                            .arg(ui::colors::TEXT_SECONDARY()));
    auto* update_btn = hooks.run_button(QuantModulePanel::tr("UPDATE TO TODAY"), bar);
    update_btn->setVisible(false);
    // Not a run button: it only opens the build form (Ctrl+Enter must not pick it).
    auto* toggle = new QPushButton(QuantModulePanel::tr("BUILD FROM YAHOO"), bar);
    toggle->setObjectName(QStringLiteral("quantQuietButton"));
    toggle->setCursor(Qt::PointingHandCursor);
    toggle->setMinimumHeight(24);
    toggle->setEnabled(false);
    toggle->setToolTip(QuantModulePanel::tr("Build a local qlib dataset from Yahoo Finance daily bars"));
    row->addWidget(caption);
    row->addWidget(combo);
    row->addWidget(info, 1);
    row->addWidget(update_btn);
    row->addWidget(toggle);
    vl->addLayout(row);

    // First-run explanation (no dataset at all).
    auto* notice = new QLabel(bar);
    notice->setWordWrap(true);
    notice->setStyleSheet(QString("color:%1; background:%2; border-left:3px solid %3; padding:6px 10px;"
                                  "font-size:" + ui::fonts::px(-2) + "px;")
                              .arg(ui::colors::TEXT_PRIMARY(), ui::colors::tint(ui::colors::WARNING),
                                   ui::colors::WARNING()));
    notice->hide();
    vl->addWidget(notice);
    // The build form is created on first use (see ensure_form): most panel openings never need it.
    auto* form = new QWidget(bar);
    form->setObjectName(QStringLiteral("qlibBuildForm"));
    form->setStyleSheet(QStringLiteral("QWidget#qlibBuildForm { background:transparent; }"));
    form->hide();
    vl->addWidget(form);

    struct State {
        QJsonArray datasets; // full entries of the last dataset_info answer
        QJsonArray universes;
        QJsonObject default_build;
        bool probe_pending = false;
        QString applied; // id whose defaults the panel's inputs hold
    };
    auto state = std::make_shared<State>();

    auto probe = [state, info, module_id](const QString& want) {
        state->probe_pending = true;
        info->setText(QuantModulePanel::tr("Reading local datasets..."));
        QJsonObject params;
        if (!want.isEmpty())
            params["dataset"] = want;
        AIQuantLabService::instance().run_module(module_id, QStringLiteral("dataset_info"), params);
    };

    auto select = [panel, state, info, update_btn, hooks](const QJsonObject& ds) {
        const QString id = ds.value("id").toString();
        panel->setProperty("qlibDataset", id);
        panel->setProperty("qlibDatasetState", QStringLiteral("ready"));
        info->setText(qlib_dataset_summary(ds));
        info->setToolTip(QuantModulePanel::tr("%1\n%2\nUpdated %3")
                             .arg(ds.value("name").toString(), ds.value("path").toString(),
                                  ds.value("updated_at").toString()));
        update_btn->setVisible(ds.value("can_update").toBool());
        update_btn->setToolTip(QuantModulePanel::tr("Download the bars after %1 from Yahoo Finance (only the new days)")
                                   .arg(ds.value("calendar_end").toString()));
        const bool changed = state->applied != id;
        state->applied = id;
        hooks.apply_defaults(ds, changed);
    };

    auto ensure_form = [form, state, module_id, hooks]() {
        if (form->layout())
            return;
        auto* hl = new QHBoxLayout(form);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);
        const QString label_ss = QString("color:%1; background:transparent;").arg(ui::colors::TEXT_SECONDARY());
        auto* uni_lbl = new QLabel(QuantModulePanel::tr("Universe"), form);
        uni_lbl->setStyleSheet(label_ss);
        auto* uni = new QComboBox(form);
        uni->setStyleSheet(combo_ss());
        uni->setAccessibleName(QuantModulePanel::tr("Universe"));
        for (const auto& v : state->universes) {
            const QJsonObject u = v.toObject();
            const int n = u.value("count").toInt();
            uni->addItem(n > 0 ? QuantModulePanel::tr("%1 (%2)").arg(u.value("label").toString()).arg(n) : u.value("label").toString(),
                         u.value("id").toString());
            uni->setItemData(uni->count() - 1, u.value("description").toString(), Qt::ToolTipRole);
        }
        uni->setCurrentIndex(std::max(0, uni->findData(state->default_build.value("universe").toString())));
        auto* tickers = new QLineEdit(form);
        tickers->setStyleSheet(input_ss());
        tickers->setPlaceholderText(QuantModulePanel::tr("Yahoo symbols, e.g. AAPL,MSFT,BRK-B"));
        tickers->setAccessibleName(QuantModulePanel::tr("Custom tickers"));
        tickers->setVisible(uni->currentData().toString() == QLatin1String("custom"));
        auto* from_lbl = new QLabel(QuantModulePanel::tr("From"), form);
        from_lbl->setStyleSheet(label_ss);
        auto* start = new QDateEdit(form);
        start->setCalendarPopup(true);
        start->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        start->setDateRange(QDate(1990, 1, 1), QDate::currentDate().addMonths(-6));
        const QDate def = QDate::fromString(state->default_build.value("start_date").toString(), Qt::ISODate);
        start->setDate(def.isValid() ? def : QDate::currentDate().addYears(-10));
        start->setStyleSheet(dateedit_ss());
        start->setAccessibleName(QuantModulePanel::tr("Start date"));
        auto* build = hooks.run_button(QuantModulePanel::tr("BUILD"), form);
        build->setToolTip(QuantModulePanel::tr("Daily bars adjusted for splits and dividends. Tickers already downloaded are reused; "
                             "US large caps take a few minutes the first time."));
        hl->addWidget(uni_lbl);
        hl->addWidget(uni);
        hl->addWidget(tickers, 1);
        hl->addWidget(from_lbl);
        hl->addWidget(start);
        hl->addWidget(build);
        hl->addStretch();
        QObject::connect(uni, QOverload<int>::of(&QComboBox::currentIndexChanged), form, [uni, tickers](int) {
            tickers->setVisible(uni->currentData().toString() == QLatin1String("custom"));
        });
        QObject::connect(build, &QPushButton::clicked, form, [uni, tickers, start, module_id, hooks]() {
            const QString universe = uni->currentData().toString();
            QJsonObject params{{"universe", universe},
                               {"start_date", start->date().toString(Qt::ISODate)},
                               {"region", QStringLiteral("us")}};
            if (universe == QLatin1String("custom")) {
                static const QRegularExpression seps(QStringLiteral("[,;\\s]+"));
                QJsonArray list;
                for (const QString& t : tickers->text().split(seps, Qt::SkipEmptyParts))
                    list.append(t.trimmed().toUpper());
                if (list.isEmpty()) {
                    hooks.show_error(QuantModulePanel::tr("Enter the Yahoo symbols of the custom universe, e.g. "
                                                          "AAPL,MSFT,BRK-B."));
                    return;
                }
                params["tickers"] = list;
            }
            hooks.show_loading(QuantModulePanel::tr("Building a dataset from Yahoo Finance: %1 from %2. Tickers "
                                                    "already downloaded are reused; US large caps take a few "
                                                    "minutes the first time.")
                                   .arg(uni->currentText(), params["start_date"].toString()));
            AIQuantLabService::instance().run_module(module_id, QStringLiteral("build_dataset"), params);
        });
    };

    auto apply_probe = [panel, state, combo, info, notice, form, toggle, update_btn, select, ensure_form,
                        hooks](const QJsonObject& r) {
        // Whatever the answer, the panel's views waiting for it go on (or show "no dataset yet").
        const auto announce = qScopeGuard([&hooks]() {
            if (hooks.state_changed)
                hooks.state_changed();
        });
        state->probe_pending = false;
        state->datasets = r.value("datasets").toArray();
        state->universes = r.value("universes").toArray();
        state->default_build = r.value("default_build").toObject();
        toggle->setEnabled(true);
        const QSignalBlocker block(combo);
        combo->clear();
        for (const auto& v : state->datasets) {
            const QJsonObject ds = v.toObject();
            combo->addItem(ds.value("name").toString(), ds.value("id").toString());
            combo->setItemData(combo->count() - 1, qlib_dataset_summary(ds), Qt::ToolTipRole);
        }
        if (state->datasets.isEmpty()) {
            combo->addItem(QuantModulePanel::tr("No dataset yet"));
            combo->setEnabled(false);
            panel->setProperty("qlibDataset", QString());
            panel->setProperty("qlibDatasetState", QStringLiteral("none"));
            state->applied.clear();
            info->setText(QuantModulePanel::tr("No local market data"));
            info->setToolTip(QString());
            update_btn->setVisible(false);
            notice->setText(r.value("message").toString());
            notice->show();
            ensure_form();
            form->show();
            toggle->setText(QuantModulePanel::tr("HIDE BUILD"));
            return;
        }
        notice->hide();
        combo->setEnabled(true);
        const int idx = std::max(0, combo->findData(r.value("selected").toString()));
        combo->setCurrentIndex(idx);
        select(state->datasets.at(idx).toObject());
    };

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), bar, [state, combo, select](int) {
        const QString id = combo->currentData().toString();
        for (const auto& v : state->datasets) {
            if (v.toObject().value("id").toString() == id) {
                qlib_session_choice() = id;
                select(v.toObject());
                return;
            }
        }
    });
    QObject::connect(toggle, &QPushButton::clicked, bar, [form, toggle, ensure_form]() {
        ensure_form();
        form->setVisible(!form->isVisible());
        toggle->setText(form->isVisible() ? QuantModulePanel::tr("HIDE BUILD") : QuantModulePanel::tr("BUILD FROM YAHOO"));
    });
    QObject::connect(update_btn, &QPushButton::clicked, bar, [panel, module_id, hooks]() {
        const QString id = panel->property("qlibDataset").toString();
        if (id.isEmpty())
            return;
        hooks.show_loading(QuantModulePanel::tr("Updating %1 with the bars Yahoo Finance has after its last day "
                                                "(only the new days are downloaded)...")
                               .arg(id));
        AIQuantLabService::instance().run_module(module_id, QStringLiteral("update_dataset"),
                                                 QJsonObject{{"dataset", id}});
    });

    auto& svc = AIQuantLabService::instance();
    QObject::connect(&svc, &AIQuantLabService::result_ready, bar,
                     [module_id, apply_probe](const QString& mod, const QString& command, const QJsonObject& data) {
                         if ((command == QLatin1String("dataset_info") || command == QLatin1String("list_datasets")) &&
                             mod == module_id)
                             apply_probe(data);
                     });
    // A dataset was built, updated or downloaded -- here or in another panel, from the bar or from
    // a view (DOWNLOAD QLIB DATASET): read the list again. The panel that built one switches to it.
    QObject::connect(&svc, &AIQuantLabService::datasets_changed, bar,
                     [panel, module_id, probe](const QString& mod, const QString& command, const QJsonObject& data) {
                         if (!is_qlib_dataset_module(mod))
                             return;
                         QString want = panel->property("qlibDataset").toString();
                         if (mod == module_id && command == QLatin1String("build_dataset"))
                             want = data.value("dataset_id").toString();
                         probe(want);
                     });
    // A probe that failed as a process (not a JSON answer) is reported by on_error in the
    // results pane; the bar only stops saying it is reading.
    QObject::connect(&svc, &AIQuantLabService::error_occurred, bar, [state, info, module_id, panel,
                                                                    hooks](const QString& mod, const QString&) {
        if (mod != module_id || !state->probe_pending)
            return;
        state->probe_pending = false;
        info->setText(QuantModulePanel::tr("Could not read the local datasets — see the notice below."));
        // Unknown is not "none": views that wait for the dataset try anyway (the script says why).
        if (panel->property("qlibDatasetState").toString().isEmpty())
            panel->setProperty("qlibDatasetState", QStringLiteral("unknown"));
        if (hooks.state_changed)
            hooks.state_changed();
    });

    probe(qlib_session_choice());
    return bar;
}

} // namespace

void QuantModulePanel::apply_qlib_defaults_to_view(const QString& key) {
    // A tab built after the dataset bar answered: its fields get the chosen dataset's windows now
    // (as if the dataset had just been picked — only this tab's fields are touched).
    if (!is_qlib_dataset_module(module_.id) || qlib_dataset_.isEmpty())
        return;
    const auto it = views_.constFind(key);
    if (it == views_.cend())
        return;
    QHash<QString, QLineEdit*> text;
    QHash<QString, QDateEdit*> dates;
    for (const QString& name : it->input_keys) {
        if (QLineEdit* le = text_inputs_.value(name))
            text.insert(name, le);
        if (QDateEdit* de = date_inputs_.value(name))
            dates.insert(name, de);
    }
    apply_qlib_dataset_defaults(module_.id, qlib_dataset_, /*dataset_changed=*/true, text, dates);
}

QDoubleSpinBox* QuantModulePanel::make_double_spin(double min, double max, double val, int decimals,
                                                   const QString& suffix, QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setDecimals(decimals);
    if (!suffix.isEmpty())
        spin->setSuffix(suffix);
    spin->setStyleSheet(spinbox_ss());
    return spin;
}

QPushButton* QuantModulePanel::make_run_button(const QString& text, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(32);
    // Tagged so the panel-wide Ctrl+Enter shortcut (build_ui) can find the run
    // button belonging to the currently visible tab, and so screen readers
    // announce something meaningful.
    btn->setObjectName(QStringLiteral("quantRunButton"));
    btn->setAccessibleName(text);
    btn->setAccessibleDescription(tr("Run this analysis (Ctrl+Enter)"));
    const QString shape = QStringLiteral("font-weight:700; border:none; padding:0 20px; border-radius:2px;"
                                         "letter-spacing:0.8px;");
    btn->setProperty(kAccentShapeProp, shape);
    btn->setStyleSheet(accent_button_ss(module_.color, shape));
    return btn;
}

QPushButton* QuantModulePanel::make_sample_button(QLineEdit* edit, QWidget* parent, const QString& tip,
                                                  std::function<QString()> generator) {
    // Styled by the panel stylesheet (refresh_theme, #quantSampleButton) — no
    // per-button setStyleSheet.
    auto* btn = new QPushButton(tr("LOAD SYNTHETIC SAMPLE"), parent);
    btn->setObjectName(QStringLiteral("quantSampleButton"));
    btn->setProperty("quantSampleFor", QVariant::fromValue<QObject*>(edit));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(22);
    btn->setToolTip(tr("%1\nGenerated (synthetic) data for trying the module — not market data. Results computed "
                       "on it are flagged SAMPLE INPUT.")
                        .arg(tip));
    btn->setAccessibleName(tr("Load synthetic sample input"));
    QPointer<QLineEdit> guard(edit);
    // The generators are seeded: the sample text is computed once and recognised whenever the box
    // holds it again (a restored input, an undo), so such a run is still flagged SAMPLE INPUT.
    auto sample = std::make_shared<QString>();
    auto text_of = [generator, sample]() -> QString {
        if (sample->isEmpty())
            *sample = generator();
        return *sample;
    };
    connect(btn, &QPushButton::clicked, this, [this, btn, guard, text_of]() {
        if (!guard)
            return;
        // A series box of a view with a ticker source: the sample replaces the ticker as the source.
        const QString view = guard->property("quantSeriesView").toString();
        if (!view.isEmpty()) {
            if (auto* mode = findChild<QComboBox*>(QStringLiteral("quantSeriesMode_") + view))
                mode->setCurrentIndex(1);
        }
        const QString text = text_of();
        guard->setText(text);
        guard->setProperty("quantSampleText", text);
        // The same view's other series boxes that are still empty get their own sample too, so a
        // two-series op (portfolio vs benchmark) is not left with one half missing.
        if (!view.isEmpty()) {
            for (auto* other : findChildren<QPushButton*>(QStringLiteral("quantSampleButton"))) {
                auto* oe = qobject_cast<QLineEdit*>(other->property("quantSampleFor").value<QObject*>());
                if (other != btn && oe && oe->property("quantSeriesView").toString() == view &&
                    oe->text().trimmed().isEmpty())
                    other->click();
            }
        }
    });
    if (edit) {
        connect(edit, &QLineEdit::textChanged, btn, [guard, text_of](const QString& t) {
            if (guard && !t.isEmpty() && guard->property("quantSampleText").toString() != t && t == text_of())
                guard->setProperty("quantSampleText", t);
        });
    }
    return btn;
}

bool QuantModulePanel::visible_inputs_are_sample() const {
    for (auto* e : findChildren<QLineEdit*>()) {
        const QString sample = e->property("quantSampleText").toString();
        if (!sample.isEmpty() && e->isVisible() && e->text() == sample)
            return true;
    }
    return false;
}

QWidget* QuantModulePanel::build_input_row(const QString& label, QWidget* input, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 2, 0, 2);
    hl->setSpacing(8);
    auto* lbl = new QLabel(row);
    set_text_width(lbl, 160);
    // The column is narrow; a long label ("Max Single-Asset Weight (0 = none)") wraps onto a
    // second line instead of being cut off at the input's edge.
    lbl->setWordWrap(true);
    // Amounts are typed in the display currency: "($)" / "($B)" unit hints name it and follow
    // Settings; others stay plain. Results computed on these amounts use the same currency.
    if (label.endsWith(QLatin1String("($)"))) {
        QString base = label;
        base.chop(3); // strip "($)"
        ui::formatting::bind_currency_label(lbl, base + QStringLiteral("(%1)"));
    } else if (label.endsWith(QLatin1String("($B)"))) {
        QString base = label;
        base.chop(4); // strip "($B)"
        const auto set_unit = [lbl, base]() {
            lbl->setText(base + QLatin1Char('(') +
                         ui::formatting::scaled_unit(QString(), ui::formatting::Scale::Billion) + QLatin1Char(')'));
        };
        set_unit();
        ui::formatting::on_format_changed(lbl, set_unit);
    } else {
        lbl->setText(label);
    }
    lbl->setStyleSheet(QString("color:%1; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    lbl->setBuddy(input);
    hl->addWidget(lbl);
    hl->addWidget(input, 1);
    // Every parameter control gets an accessible name — without this a screen
    // reader announces bare "edit"/"combo box" for ~200 unlabeled inputs.
    if (input && input->accessibleName().isEmpty())
        input->setAccessibleName(lbl->text().isEmpty() ? label : lbl->text());
    return row;
}

// ── LLM picker helpers ────────────────────────────────────────────────────────

QWidget* QuantModulePanel::build_llm_picker(QWidget* parent, QComboBox** out_combo) {
    auto* combo = new QComboBox(parent);
    combo->setStyleSheet(combo_ss());

    // Populate from saved profiles
    auto profiles_result = LlmProfileRepository::instance().list_profiles();
    if (profiles_result.is_ok()) {
        for (const auto& p : profiles_result.value()) {
            combo->addItem(QString("%1  [%2 / %3]").arg(p.name, p.provider, p.model_id), p.id);
        }
    }
    if (combo->count() == 0)
        combo->addItem(tr("No LLM profiles — configure in Settings → LLM Config"), QVariant());

    // Pre-select the global default if present
    auto resolved = LlmProfileRepository::instance().resolve_for_context("ai_quant_lab");
    if (!resolved.profile_id.isEmpty()) {
        int idx = combo->findData(resolved.profile_id);
        if (idx >= 0)
            combo->setCurrentIndex(idx);
    }

    if (out_combo)
        *out_combo = combo;
    return build_input_row(tr("LLM Profile"), combo, parent);
}

QJsonObject QuantModulePanel::llm_config_from_combo(QComboBox* combo) const {
    if (!combo)
        return {};
    QString profile_id = combo->currentData().toString();
    if (profile_id.isEmpty())
        return {};

    // resolve_for_context() takes the context id, not a profile id — the combo
    // already holds a concrete profile id, so read it directly.
    auto result = LlmProfileRepository::instance().get_profile(profile_id);
    if (!result.is_ok())
        return {};

    const auto& p = result.value();
    QJsonObject cfg;
    cfg["llm_provider"] = p.provider;
    cfg["llm_api_key"] = p.api_key;
    cfg["llm_model"] = p.model_id;
    cfg["llm_base_url"] = p.base_url;
    return cfg;
}

// ── Constructor ──────────────────────────────────────────────────────────────

namespace {
/// A stylesheet is set only when its text changes: setting one re-polishes the widget's whole
/// subtree, and the theme manager's re-style pass may already have rewritten it.
void qmp_set_sheet(QWidget* w, const QString& sheet) {
    if (w && w->styleSheet() != sheet)
        w->setStyleSheet(sheet);
}
} // namespace

QuantModulePanel::QuantModulePanel(const QuantModule& mod, QWidget* parent) : QWidget(parent), module_(mod) {
    // The panel-wide sheet BEFORE any child exists: every child is polished once, with it. (Set
    // after build_ui() it re-polished the whole tree — part of the 470-680 ms first-open stall.)
    refresh_theme();
    build_ui();
    connect_service();
    // Theme / font changes: stylesheets the panel built itself are re-derived once the change has
    // settled (never inside theme_changed); colours handed to Qt objects that keep them (table item
    // brushes, chart pens) follow in the same pass.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
}

QuantModulePanel::~QuantModulePanel() {
    // The pane (a child) outlives this body: stop it calling back into a half-destroyed panel, and
    // stop the scripts still running for it.
    if (pane_) {
        pane_->set_start({});
        pane_->set_cancel({});
        pane_->set_render({});
        pane_->set_requirement_check({});
    }
    for (auto it = pending_.begin(); it != pending_.end(); ++it)
        if (it->cancel)
            it->cancel->store(true);
    pending_.clear();
}

void QuantModulePanel::refresh_theme() {
    // Panel-wide base colours plus the shared table rules, so a result table that is created
    // without its own style sheet still matches the theme.
    qmp_set_sheet(this,
                  QString("* { background:%1; color:%2; }").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()) +
                      table_ss() +
                      QString("QPushButton#quantSampleButton, QPushButton#quantQuietButton { color:%1; "
                              "background:transparent; border:1px solid %2; padding:0 8px; font-size:%5px; "
                              "font-weight:700; letter-spacing:0.5px; border-radius:2px; }"
                              "QPushButton#quantSampleButton:hover, QPushButton#quantQuietButton:hover { color:%3;"
                              "border-color:%3; background:%4; }")
                          .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(),
                               ui::colors::legible(module_.color), ui::colors::overlay(0.04), ui::fonts::px(-5)));

    qmp_set_sheet(panel_header_, QString("background:%1; border-bottom:1px solid %2;")
                                     .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
    qmp_set_sheet(header_title_, QString("color:%1; font-weight:700; letter-spacing:1px; background:transparent;")
                                     .arg(ui::colors::legible(module_.color)));
    qmp_set_sheet(header_cat_, QString("color:%1; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    qmp_set_sheet(status_label_, QString("color:%1; background:transparent;").arg(ui::colors::TEXT_SECONDARY()));

    // Module-colour buttons: their hover / pressed shades are derived for the active theme.
    for (auto* b : findChildren<QPushButton*>()) {
        const QString shape = b->property(kAccentShapeProp).toString();
        if (!shape.isEmpty())
            qmp_set_sheet(b, accent_button_ss(module_.color, shape, b->property(kAccentFontProp)));
    }
    // Text-sized fixed widths follow the font size.
    for (auto* w : findChildren<QWidget*>()) {
        const QVariant width = w->property(kTextWidthProp);
        if (width.isValid())
            set_text_width(w, width.toInt());
    }
    // The Deep Agent report is Markdown rendered to HTML with the theme's colours inline:
    // render it again for the new theme.
    if (agent_output_) {
        const QVariant md = agent_output_->property("quantMarkdown");
        if (md.isValid())
            agent_output_->setHtml(ui::MarkdownRenderer::render(md.toString()));
    }
    for (auto* report : findChildren<QLabel*>(QStringLiteral("quantMarkdownReport"))) {
        const QVariant md = report->property("quantMarkdown");
        if (md.isValid())
            report->setText(ui::MarkdownRenderer::render(md.toString()));
    }
    refresh_kept_styles(this);
}

// ── Re-translation ───────────────────────────────────────────────────────────
// Static chrome only: re-apply tr() text to the persistent, member-stored
// widgets. The module header (label/category) is data-derived and kept
// verbatim; status_label_ is transient state set per-operation; dynamically
// built input forms and result cards re-render in the active language on their
// next build/fetch.

void QuantModulePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

// Loading states are the result pane's skeleton (no timer runs while hidden). The RD-Agent task
// monitor's poll timer is the only timer of the panel.
void QuantModulePanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // RD-Agent monitor auto-refresh resumes only if the last listing had a
    // running task (its timeout stops itself if the monitor tab is not shown).
    if (auto* poll = findChild<QTimer*>(QStringLiteral("rdTaskPollTimer")); poll && poll->property("rdActive").toBool())
        poll->start();
}

void QuantModulePanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (auto* poll = findChild<QTimer*>(QStringLiteral("rdTaskPollTimer")))
        poll->stop();
}

void QuantModulePanel::retranslateUi() {
    // deep_agent / rd_agent persistent output surfaces
    if (agent_output_)
        agent_output_->setPlaceholderText(tr("Analysis results will appear here..."));
    if (rd_agent_output_)
        rd_agent_output_->setPlaceholderText(tr("Select a task and click GET FACTORS / GET MODEL to view results..."));
    if (rd_task_table_)
        rd_task_table_->setHorizontalHeaderLabels(
            {tr("Task ID"), tr("Type"), tr("Status"), tr("Progress"), tr("Best IC"), tr("Elapsed")});

    // rl_trading persistent controls
    if (rl_train_button_)
        rl_train_button_->setText(tr("TRAIN RL AGENT"));
}

void QuantModulePanel::connect_service() {
    auto& svc = AIQuantLabService::instance();
    connect(&svc, &AIQuantLabService::result_ready, this, &QuantModulePanel::on_result);
    connect(&svc, &AIQuantLabService::error_occurred, this, &QuantModulePanel::on_error);

    // RL Trading live-streaming signals. Filtered by module_id so only the RL panel
    // updates its widgets; other panels ignore these.
    connect(&svc, &AIQuantLabService::training_progress, this,
            [this](const QString& module_id, int step, int total, double reward_mean, double loss) {
                if (module_id != "rl_trading" || !rl_progress_bar_ || !rl_progress_stats_)
                    return;
                const int pct = (total > 0) ? qBound(0, int(100.0 * step / total), 100) : 0;
                rl_progress_bar_->setValue(pct);
                // NaN = not available yet (no finished episode / no update so far).
                const auto num = [](double v, int decimals) {
                    return std::isfinite(v) ? QString::number(v, 'f', decimals) : QString::fromUtf8("—");
                };
                rl_progress_stats_->setText(tr("step %1 / %2 · reward %3 · loss %4")
                                                .arg(step)
                                                .arg(total)
                                                .arg(num(reward_mean, 3), num(loss, 4)));
            });

    connect(&svc, &AIQuantLabService::training_log, this,
            [this](const QString& module_id, const QString& line, bool is_stderr) {
                if (module_id != "rl_trading" || !rl_log_console_)
                    return;
                const QString prefix = is_stderr ? QStringLiteral("[stderr] ") : QString();
                rl_log_console_->appendPlainText(prefix + line);
            });
}

void QuantModulePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Module header — stored for refresh_theme()
    panel_header_ = new QWidget(this);
    panel_header_->setFixedHeight(36);
    auto* hhl = new QHBoxLayout(panel_header_);
    hhl->setContentsMargins(16, 0, 16, 0);
    hhl->setSpacing(8);

    header_title_ = new QLabel(module_.label.toUpper(), panel_header_);
    hhl->addWidget(header_title_);

    auto* div = new QWidget(panel_header_);
    div->setObjectName("panelHeaderDiv");
    div->setFixedSize(1, 14);
    hhl->addWidget(div);

    QString cat_label = module_.category;
    cat_label.replace('_', '/');
    header_cat_ = new QLabel(cat_label, panel_header_);
    hhl->addWidget(header_cat_);

    hhl->addStretch();

    status_label_ = new QLabel(panel_header_);
    status_label_->setObjectName(QStringLiteral("quantStatusLabel")); // the result pane's "as of" line
    hhl->addWidget(status_label_);

    root->addWidget(panel_header_);
    refresh_theme(); // the header's own sheets (the panel sheet is already set)

    // Content
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    QWidget* content = nullptr;
    if (module_.id == "gs_quant")
        content = build_gs_quant_panel();
    else if (module_.id == "functime")
        content = build_functime_panel();
    else if (module_.id == "statsmodels")
        content = build_statsmodels_panel();
    else if (module_.id == "fortitudo")
        content = build_fortitudo_panel();
    else if (module_.id == "gluonts")
        content = build_gluonts_panel();
    else if (module_.id == "cfa_quant")
        content = build_cfa_quant_panel();
    else if (module_.id == "backtesting")
        content = build_backtesting_panel();
    else if (module_.id == "rl_trading")
        content = build_rl_trading_panel();
    else if (module_.id == "advanced_models")
        content = build_advanced_models_panel();
    else if (module_.id == "deep_agent")
        content = build_deep_agent_panel();
    else if (module_.id == "factor_discovery")
        content = build_factor_discovery_panel();
    else if (module_.id == "model_library")
        content = build_model_library_panel();
    else if (module_.id == "live_signals")
        content = build_live_signals_panel();
    else if (module_.id == "online_learning")
        content = build_online_learning_panel();
    else if (module_.id == "meta_learning")
        content = build_meta_learning_panel();
    else if (module_.id == "hft")
        content = build_hft_panel();
    else if (module_.id == "rolling_retraining")
        content = build_rolling_retraining_panel();
    else if (module_.id == "feature_engineering")
        content = build_feature_engineering_panel();
    else if (module_.id == "portfolio_opt")
        content = build_portfolio_opt_panel();
    else if (module_.id == "factor_evaluation")
        content = build_factor_evaluation_panel();
    else if (module_.id == "strategy_builder")
        content = build_strategy_builder_panel();
    else if (module_.id == "data_processors")
        content = build_data_processors_panel();
    else if (module_.id == "quant_reporting")
        content = build_quant_reporting_panel();
    else
        content = build_generic_panel();

    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // The panels that read a local qlib dataset get the dataset bar between the header and
    // the content, visible from every tab. Built after the content, so the Ctrl+Enter
    // shortcut below still finds the visible tab's run button first.
    if (is_qlib_dataset_module(module_.id)) {
        QlibBarHooks hooks;
        hooks.run_button = [this](const QString& text, QWidget* parent) { return make_run_button(text, parent); };
        // BUILD / UPDATE run outside the tabs' views: their progress and outcome are a notice over
        // the result pane (the tab's own result stays), the bar re-reads the list when they finish.
        hooks.show_loading = [this](const QString& message) {
            if (pane_)
                pane_->set_notice(message, ui::EmptyState::Tone::Info);
        };
        hooks.show_error = [this](const QString& message) {
            if (pane_)
                pane_->set_notice(message, ui::EmptyState::Tone::Danger, tr("DISMISS"));
        };
        hooks.apply_defaults = [this](const QJsonObject& dataset, bool changed) {
            qlib_dataset_ = dataset;
            apply_qlib_dataset_defaults(module_.id, dataset, changed, text_inputs_, date_inputs_);
        };
        hooks.state_changed = [this]() {
            if (property("qlibDatasetState").toString() == QLatin1String("none"))
                qlib_dataset_ = {};
            // A remembered result of a qlib view was made on the dataset it names: the signature
            // includes the dataset, so another dataset shows "Inputs changed".
            if (pane_ && !current_view_.isEmpty())
                pane_->set_signature(view_signature(current_view_));
            qlib_dataset_known();
        };
        root->insertWidget(1, make_qlib_dataset_bar(this, module_.id, hooks));
    }

    // ── Ctrl+Enter runs the visible tab's action ─────────────────────────────
    // One shortcut per panel rather than QPushButton::setShortcut on every run
    // button: N buttons sharing a window-level shortcut is an "ambiguous
    // shortcut overload" in Qt and silently does nothing. Scoped to this widget
    // subtree so it never fires for another screen.
    auto* run_shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    run_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(run_shortcut, &QShortcut::activated, this, [this]() {
        // isVisible() is false for widgets on a non-current QTabWidget page, so
        // this resolves to the run button the user is actually looking at.
        for (auto* btn : this->findChildren<QPushButton*>(QStringLiteral("quantRunButton"))) {
            if (btn->isVisible() && btn->isEnabled()) {
                btn->click();
                return;
            }
        }
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// GENERIC PANEL — for modules without specialized UI
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_generic_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* desc = new QLabel(module_.description, w);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; line-height:1.5;")
                            .arg(ui::colors::TEXT_PRIMARY())
                            .arg(ui::fonts::px(-1))
                            .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(desc);

    auto* script_lbl = new QLabel(tr("Python script: %1").arg(module_.script), w);
    script_lbl->setStyleSheet(QString("color:%1; font-family:%2;"
                                      "padding:6px; background:%3; border:1px solid %4; border-radius:2px;")
                                  .arg(ui::colors::TEXT_SECONDARY())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::colors::BG_RAISED())
                                  .arg(ui::colors::BORDER_DIM()));
    vl->addWidget(script_lbl);

    // Command input
    auto* cmd = new QLineEdit(w);
    cmd->setPlaceholderText(tr("Command (e.g. analyze, train, list_models)"));
    cmd->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:6px 8px; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::px(-1)));
    text_inputs_["gen_command"] = cmd;
    vl->addWidget(build_input_row(tr("Command"), cmd, w));

    // JSON params
    auto* params_edit = new QTextEdit(w);
    params_edit->setPlaceholderText(tr("JSON parameters (optional)\ne.g. {\"ticker\":\"AAPL\"}"));
    params_edit->setMaximumHeight(100);
    params_edit->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                       "font-family:%4; font-size:%5px; padding:6px; }")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                   .arg(ui::fonts::DATA_FAMILY)
                                   .arg(ui::fonts::px(-1)));
    vl->addWidget(params_edit);

    auto* run = make_run_button(tr("EXECUTE"), w);
    run->setProperty("fincept_agent_risk", "code"); // runs a free-form script command
    connect(run, &QPushButton::clicked, this, [this, params_edit]() {
        auto cmd_text = text_inputs_["gen_command"]->text().trimmed();
        if (cmd_text.isEmpty())
            cmd_text = QStringLiteral("analyze");
        const auto json_text = params_edit->toPlainText().trimmed();
        QJsonObject params;
        if (!json_text.isEmpty()) {
            // A malformed body used to be dropped silently and the command ran
            // with no parameters at all.
            QJsonParseError perr{};
            const auto doc = QJsonDocument::fromJson(json_text.toUtf8(), &perr);
            if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                display_error(tr("JSON parameters must be an object like {\"ticker\":\"AAPL\"} — %1")
                                  .arg(perr.errorString()));
                return;
            }
            params = doc.object();
        }
        show_loading(tr("Running %1 %2...").arg(module_.script, cmd_text));
        AIQuantLabService::instance().run_module(module_.id, cmd_text, params);
    });
    vl->addWidget(run);

    auto* rc = new QWidget(w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(rc);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RESULT DISPLAY
// ═══════════════════════════════════════════════════════════════════════════════

void QuantModulePanel::clear_results() {
    // Only the result area. (Run buttons are no longer disabled while a request is in flight: a new
    // run supersedes the old one; a Heavy view's own buttons are locked by the pane's running state.)
    if (!results_layout_)
        return;
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

namespace {
/// A failure as one plain sentence (the exception line of a Python traceback), capped.
QString qmp_plain_error(const QString& raw) {
    QString text = raw.trimmed();
    if (text.contains(QLatin1String("Traceback (most recent call last)"))) {
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (int i = lines.size() - 1; i >= 0; --i) {
            const QString l = lines.at(i).trimmed();
            if (!l.isEmpty() && !l.startsWith(QLatin1String("File ")) && !l.startsWith(QLatin1Char('^'))) {
                text = l;
                break;
            }
        }
    }
    if (text.size() > 400)
        text = text.left(400) + QStringLiteral("…");
    return text;
}
} // namespace

void QuantModulePanel::display_error(const QString& msg) {
    // Inside a view's run (its RUN handler's checks): that run fails — the pane shows the reason
    // with RETRY; the result on screen, if any, stays.
    if (run_ctx_) {
        if (!run_ctx_->failed) {
            run_ctx_->failed = true;
            if (pane_)
                pane_->fail(run_ctx_->pane_id, msg, tr("Check the inputs"));
        }
        return;
    }
    // While a result is drawn (a renderer met a reply it cannot show): a designed error state in
    // the result area, never a traceback.
    if (rendering_ || !pane_) {
        clear_results();
        if (!results_layout_)
            return;
        auto* es = ui::EmptyState::create(ui::EmptyState::error_spec(qmp_plain_error(msg), QString(), QString()),
                                          results_layout_->parentWidget());
        es->set_compact(true);
        es->set_alignment(Qt::AlignTop);
        connect(es, &ui::EmptyState::primary_clicked, this, [this]() {
            if (pane_)
                pane_->run();
        });
        results_layout_->addWidget(es);
        return;
    }
    // Outside a view's run (the dataset bar, an RD-Agent reply): a notice over the result pane.
    pane_->set_notice(qmp_plain_error(msg), ui::EmptyState::Tone::Danger, tr("DISMISS"));
}

// ── Loading ──────────────────────────────────────────────────────────────────
// The result pane's skeleton is the loading state; a run's handler only names what it does.

void QuantModulePanel::show_loading(const QString& message) {
    if (run_ctx_) {
        if (pane_)
            pane_->set_loading_text(message);
        return;
    }
    if (pane_)
        pane_->set_notice(message, ui::EmptyState::Tone::Info);
}

void QuantModulePanel::on_error(const QString& module_id, const QString& message) {
    if (module_id != module_.id)
        return;
    sample_input_pending_ = false;
    // A saved-id list whose script failed as a process: the next train / create / tab may ask again.
    for (auto it = saved_ids_.begin(); it != saved_ids_.end(); ++it)
        it->pending = false;
    // Only a failed TRAIN resets the training widgets: an EVALUATE SAVED MODEL error comes
    // through the same module id and must not report "Training failed".
    if (module_id == "rl_trading" && property("rlTraining").toBool()) {
        setProperty("rlTraining", false);
        if (rl_train_button_)
            rl_train_button_->setEnabled(true);
        if (rl_progress_bar_)
            rl_progress_bar_->setValue(0);
        if (rl_progress_stats_)
            rl_progress_stats_->setText(tr("Training failed — see the log below."));
    }
    // Rolling Retraining owns a bespoke progress bar; leave it in a readable
    // failed state rather than a stale "Starting..." forever.
    if (auto* pb = this->findChild<QProgressBar*>(QStringLiteral("rr_progress")))
        pb->setFormat(tr("Failed"));
    if (module_id == QLatin1String("rolling_retraining"))
        set_retrain_busy(this, false);

    // message is the script's stderr (PythonRunner::PythonResult::error), so the
    // user sees the actual traceback. Prefix it with the script that failed —
    // otherwise a bare traceback gives no clue which module produced it.
    const QString text = message.trimmed().isEmpty() ? tr("%1 failed with no diagnostic output.").arg(module_.script)
                                                     : tr("%1 failed:\n\n%2").arg(module_.script, message.trimmed());

    // The Deep Agent panel has two tabs. display_error() paints into the Deep Analysis tab's
    // results pane, so a failed RD-Agent request (status check, task list, stop, ...) showed
    // nothing in the tab the user was looking at. When the RD-Agent output box is the visible
    // one, report there and on its status strip instead.
    if (module_id == QLatin1String("deep_agent")) {
        for (auto* b : findChildren<QPushButton*>(QStringLiteral("quantRunButton")))
            if (b->property("deepRun").toBool())
                b->setEnabled(true);
        if (agent_output_ && agent_output_->isVisible())
            agent_output_->setPlainText(text);
    }
    if (module_id == QLatin1String("deep_agent") && current_view_ == QLatin1String("rd_agent")) {
        // The RD-Agent tab (its own status strip; the result pane is hidden there).
        if (rd_agent_output_) {
            rd_agent_output_->show();
            rd_agent_output_->setPlainText(text);
        }
        if (auto* lbl = this->findChild<QLabel*>(QStringLiteral("rdStatusTxt")))
            lbl->setText(tr("Error — see output below"));
        if (status_label_)
            status_label_->setText(tr("Error"));
        return;
    }
    display_error(text);
}

} // namespace fincept::screens
