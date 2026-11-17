// src/screens/alt_investments/AltInvestmentsScreen_Layout.cpp
//
// UI construction: setup_ui, create_header, create_left/center/right panels,
// create_status_bar.
//
// Columns: asset classes (narrow) | inputs (a fixed-width column that scrolls) | results (every
// remaining pixel — the results used to be capped near 420 px while the form took the stretch).
//
// Part of the partial-class split of AltInvestmentsScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/alt_investments/AltInvestmentsScreen.h"
#include "screens/alt_investments/AltPanels.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/ResultView.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

using namespace fincept::ui;

void AltInvestmentsScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(create_header());

    // Modes: the analyzer catalogue (page 0) and the workbench panels, each built on first use.
    root->addWidget(create_mode_strip());
    mode_stack_ = new QStackedWidget(this);
    mode_stack_->setObjectName(QStringLiteral("altModeStack"));

    auto* splitter = new QSplitter(Qt::Horizontal, mode_stack_);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);

    // Widths follow the user's font size (the design is sized for the default) — also after a live
    // font change: seen running, a step from 14 to 15 px cut "Bonds & Fixed Income" off in a list
    // that kept the width of the smaller font. 26 characters: the longest class name is 20, plus the row's
    // padding and the rounding of the average character width (24 was 4 px short at 15 px).
    auto* left = create_left_panel(splitter);
    auto* center = create_center_panel(splitter);
    auto* right = create_right_panel(splitter);
    const auto panel_char = []() { return QFontMetrics(fonts::font(-1)).averageCharWidth(); };
    const auto left_width = [panel_char]() { return std::max(170, panel_char() * 26); };
    // Wide enough for the three-column cash-flow grid (date, type, amount) without a sideways scroll:
    // at 44 characters the amount column was cut off at every window width.
    // 53: one more character for the date column's full text margins (DataGridEditor).
    const auto form_width = [panel_char]() { return std::max(360, panel_char() * 53); };
    const auto apply_panel_widths = [left, center, right, panel_char, left_width, form_width]() {
        left->setMinimumWidth(left_width());
        left->setMaximumWidth(std::max(220, panel_char() * 34));
        center->setMinimumWidth(form_width());
        center->setMaximumWidth(form_width() * 2);
        right->setMinimumWidth(std::max(320, panel_char() * 40));
    };
    apply_panel_widths();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [apply_panel_widths](const ThemeTokens&) { apply_panel_widths(); });
    const int left_w = left_width();
    const int form_w = form_width();

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({left_w, form_w, 4000});

    mode_stack_->addWidget(splitter);
    root->addWidget(mode_stack_, 1);
    root->addWidget(create_status_bar());

    // Esc stops a running analysis. Enabled only while one runs (set_loading), so it never takes Esc
    // from a field or a grid cell being edited.
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setObjectName(QStringLiteral("altEscCancel"));
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    esc->setEnabled(false);
    connect(esc, &QShortcut::activated, this, [this]() {
        if (loading_)
            on_analyze(); // = cancel while running
    });

    on_category_changed(0);
}

QWidget* AltInvestmentsScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("altHeader");
    bar->setMinimumHeight(46);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(8);

    auto* tc = new QVBoxLayout;
    tc->setSpacing(1);
    header_title_ = new QLabel(tr("ALTERNATIVE INVESTMENTS"), bar);
    header_title_->setObjectName("altHeaderTitle");
    // Counts come from the catalog: the label used to hard-code "27 ANALYZERS" while only 24 exist.
    int analyzer_total = 0;
    for (const auto& cat : categories_)
        analyzer_total += static_cast<int>(cat.analyzers.size());
    header_sub_ = new QLabel(tr("%1 ANALYZERS  \xB7  %2 ASSET CLASSES  \xB7  MULTI-ASSET ANALYTICS")
                                 .arg(analyzer_total)
                                 .arg(categories_.size()),
                             bar);
    header_sub_->setObjectName("altHeaderSub");
    tc->addWidget(header_title_);
    tc->addWidget(header_sub_);
    hl->addLayout(tc);
    hl->addStretch(1);

    header_badge_ = new QLabel(tr("LOCAL PYTHON ANALYTICS"), bar);
    header_badge_->setObjectName("altHeaderBadge");
    header_badge_->setToolTip(tr("Every analysis runs on this computer from the numbers you enter; nothing is "
                                 "sent to a server."));
    hl->addWidget(header_badge_);
    return bar;
}

QWidget* AltInvestmentsScreen::create_left_panel(QWidget* parent) {
    auto* panel = new QWidget(parent);
    panel->setObjectName("altLeftPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    left_title_ = new QLabel(tr("ASSET CLASSES"), panel);
    left_title_->setObjectName("altLeftTitle");
    vl->addWidget(left_title_);

    for (int i = 0; i < categories_.size(); ++i) {
        // "&" in a button text marks a keyboard shortcut ("Bonds & Fixed Income" showed as "Bonds _Fixed Income").
        auto* btn = new QPushButton(QString(categories_[i].name).replace(QLatin1Char('&'), QLatin1String("&&")), panel);
        btn->setObjectName("altCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_changed(i); });
        vl->addWidget(btn);
        cat_btns_.append(btn);
    }
    vl->addStretch(1);
    return panel;
}

QWidget* AltInvestmentsScreen::create_center_panel(QWidget* parent) {
    auto* outer = new QScrollArea(parent);
    outer->setObjectName("altCenterPanel");
    outer->setWidgetResizable(true);
    outer->setFrameShape(QFrame::NoFrame);
    outer->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(outer);
    content->setObjectName("altCenterContent");
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Title bar: category, analyzer description ─────────────────────────────
    auto* title_bar = new QWidget(content);
    title_bar->setObjectName("altCenterTitleBar");
    auto* tbl = new QVBoxLayout(title_bar);
    tbl->setContentsMargins(14, 10, 14, 10);
    tbl->setSpacing(3);
    center_title_ = new QLabel(title_bar);
    center_title_->setObjectName("altCenterTitle");
    center_desc_ = new QLabel(title_bar);
    center_desc_->setObjectName("altCenterDesc");
    center_desc_->setWordWrap(true);
    tbl->addWidget(center_title_);
    tbl->addWidget(center_desc_);
    vl->addWidget(title_bar);

    // ── Analyzer and method pickers (full width of the column) ────────────────
    auto* pickers = new QWidget(content);
    auto* pl = new QVBoxLayout(pickers);
    pl->setContentsMargins(12, 10, 12, 4);
    pl->setSpacing(4);
    combo_label_ = new QLabel(tr("ANALYZER"), pickers);
    combo_label_->setObjectName("altComboLabel");
    analyzer_combo_ = new QComboBox(pickers);
    analyzer_combo_->setAccessibleName(tr("Analyzer"));
    connect(analyzer_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AltInvestmentsScreen::on_analyzer_changed);
    pl->addWidget(combo_label_);
    pl->addWidget(analyzer_combo_);

    // Analysis method (cli.py --method); product guides are listed after a separator.
    method_label_ = new QLabel(tr("METHOD"), pickers);
    method_label_->setObjectName("altComboLabel");
    method_combo_ = new QComboBox(pickers);
    method_combo_->setAccessibleName(tr("Analysis method"));
    connect(method_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AltInvestmentsScreen::on_method_changed);
    pl->addSpacing(4);
    pl->addWidget(method_label_);
    pl->addWidget(method_combo_);

    method_note_ = new QLabel(pickers);
    method_note_->setObjectName("altMethodNote");
    method_note_->setWordWrap(true);
    method_note_->setVisible(false);
    pl->addSpacing(4);
    pl->addWidget(method_note_);
    vl->addWidget(pickers);

    // ── Form panel ────────────────────────────────────────────────────────────
    auto* wrap = new QWidget(content);
    auto* wvl = new QVBoxLayout(wrap);
    wvl->setContentsMargins(12, 8, 12, 12);
    wvl->setSpacing(6);

    auto* form_panel = new QWidget(wrap);
    form_panel->setObjectName("altFormPanel");
    auto* fpvl = new QVBoxLayout(form_panel);
    fpvl->setContentsMargins(0, 0, 0, 0);
    fpvl->setSpacing(0);

    auto* fhdr = new QWidget(form_panel);
    fhdr->setObjectName("altFormHeader");
    fhdr->setMinimumHeight(38);
    auto* fhl = new QHBoxLayout(fhdr);
    fhl->setContentsMargins(12, 4, 8, 4);
    fhl->setSpacing(6);
    form_title_ = new QLabel(tr("INPUTS"), fhdr);
    form_title_->setObjectName("altFormTitle");
    reset_btn_ = new QPushButton(tr("RESET TO EXAMPLE"), fhdr);
    reset_btn_->setObjectName("altSecondaryBtn");
    reset_btn_->setCursor(Qt::PointingHandCursor);
    reset_btn_->setToolTip(tr("Put this analyzer's example inputs back (your entries for it are replaced)"));
    reset_btn_->setAccessibleName(tr("Reset inputs to the example"));
    connect(reset_btn_, &QPushButton::clicked, this, &AltInvestmentsScreen::reset_inputs_to_example);
    analyze_btn_ = new QPushButton(tr("ANALYZE"), fhdr);
    analyze_btn_->setObjectName("altAnalyzeBtn");
    analyze_btn_->setCursor(Qt::PointingHandCursor);
    analyze_btn_->setMinimumHeight(26);
    analyze_btn_->setAccessibleName(tr("Run the selected analyzer"));
    analyze_btn_->setToolTip(tr("Run the analyzer (Enter)"));
    connect(analyze_btn_, &QPushButton::clicked, this, &AltInvestmentsScreen::on_analyze);
    fhl->addWidget(form_title_);
    fhl->addStretch(1);
    fhl->addWidget(reset_btn_);
    fhl->addWidget(analyze_btn_);
    fpvl->addWidget(fhdr);

    // Fields are rebuilt per analyzer + method (Enter in a field runs the analysis; the data grids
    // keep Enter for themselves).
    form_container_ = new QWidget(form_panel);
    form_layout_ = new QVBoxLayout(form_container_);
    form_layout_->setContentsMargins(12, 12, 12, 12);
    form_layout_->setSpacing(8);
    fpvl->addWidget(form_container_);
    wvl->addWidget(form_panel);

    form_problems_ = new QLabel(wrap);
    form_problems_->setObjectName("altFormProblems");
    form_problems_->setWordWrap(true);
    form_problems_->setVisible(false);
    wvl->addWidget(form_problems_);
    wvl->addStretch(1);
    vl->addWidget(wrap, 1);

    outer->setWidget(content);
    return outer;
}

QWidget* AltInvestmentsScreen::create_right_panel(QWidget* parent) {
    auto* panel = new QWidget(parent);
    panel->setObjectName("altRightPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Bar: title of the shown result + actions ──────────────────────────────
    auto* bar = new QWidget(panel);
    bar->setObjectName("altRightBar");
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(12, 6, 8, 6);
    bl->setSpacing(6);
    right_title_ = new QLabel(tr("RESULTS"), bar);
    right_title_->setObjectName("altRightTitle");
    result_title_ = new QLabel(bar);
    result_title_->setObjectName("altResultTitle");
    result_title_->setMinimumWidth(40);
    result_title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bl->addWidget(right_title_);
    bl->addWidget(result_title_, 1);
    result_status_ = new QLabel(bar);
    result_status_->setObjectName("altStatusText");
    bl->addWidget(result_status_);

    auto make_action = [bar](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, bar);
        b->setObjectName("altSecondaryBtn");
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setEnabled(false);
        return b;
    };
    copy_btn_ = make_action(tr("COPY"), tr("Copy the result (inputs, figures and tables) as tab-separated text"));
    csv_btn_ = make_action(tr("CSV"), tr("Save the result (inputs, figures and tables) as a CSV file"));
    image_btn_ = make_action(tr("IMAGE"), tr("Save the result page as a PNG image"));
    // The shared Send-to-Report action: the open report, a recent one, or a new report (one undo step).
    report_btn_ = new ui::AddToReportButton(QStringLiteral("Alternative Investments"), bar);
    report_btn_->set_payload_provider([this]() { return report_payload(); });
    report_btn_->set_show_result(false); // crowded bar: the toast reports it
    report_btn_->setEnabled(false);
    connect(copy_btn_, &QPushButton::clicked, this, &AltInvestmentsScreen::copy_result);
    connect(csv_btn_, &QPushButton::clicked, this, &AltInvestmentsScreen::export_csv);
    connect(image_btn_, &QPushButton::clicked, this, &AltInvestmentsScreen::export_image);
    bl->addWidget(copy_btn_);
    bl->addWidget(csv_btn_);
    bl->addWidget(image_btn_);
    bl->addWidget(report_btn_);
    vl->addWidget(bar);

    // ── The result: the shared never-blank pane around verdict line + result view ──
    // One view per analyzer + method; the first show of a view displays its remembered result
    // (memory, else the cache of an earlier session) and runs the analysis with the inputs on
    // screen behind a loading skeleton (or as a quiet refresh). Local Python, free: AutoRun.
    pane_ = new ui::AutoLoadPane(panel);
    auto* content = new QWidget(pane_->content_parent());
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(0);

    // ── Verdict / summary line ────────────────────────────────────────────────
    auto* verdict_area = new QWidget(content);
    auto* val = new QVBoxLayout(verdict_area);
    val->setContentsMargins(12, 8, 12, 8);
    val->setSpacing(4);

    verdict_badge_ = new QLabel(tr("AWAITING ANALYSIS"), verdict_area);
    verdict_badge_->setObjectName("altVerdictBadge");
    verdict_badge_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // One line, as wide as its text. Word wrap is off on purpose: a wrapping label reports a narrow
    // preferred width, so with this size policy a method name was broken into two lines inside a
    // one-line-high box and both lines were cut. The small explicit minimum keeps a very long name
    // from widening the column (it is then cut at the right; the full text is in the tooltip).
    verdict_badge_->setWordWrap(false);
    verdict_badge_->setMinimumWidth(40);
    verdict_badge_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    verdict_badge_->setStyleSheet(QString("color:%1; background:%2; font-size:" + fincept::ui::fonts::px(-1) + "px; font-weight:700;"
                                          " padding:4px 14px; letter-spacing:0.5px;")
                                      .arg(colors::TEXT_SECONDARY(), colors::BG_RAISED()));
    val->addWidget(verdict_badge_);

    // Shown when an input is edited after the result was computed.
    stale_note_ = new QLabel(tr("Inputs changed since this result - press ANALYZE to update it."), verdict_area);
    stale_note_->setObjectName("altStaleNote");
    stale_note_->setWordWrap(true);
    stale_note_->setVisible(false);
    val->addWidget(stale_note_);

    verdict_rating_ = new QLabel(verdict_area);
    verdict_rating_->setObjectName("altVerdictRating");
    verdict_rating_->setVisible(false);
    val->addWidget(verdict_rating_);

    verdict_rec_ = new QLabel(verdict_area);
    verdict_rec_->setObjectName("altVerdictRec");
    verdict_rec_->setWordWrap(true);
    verdict_rec_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    val->addWidget(verdict_rec_);
    cvl->addWidget(verdict_area);

    auto* div = new QWidget(content);
    div->setObjectName("altDivider");
    div->setFixedHeight(1);
    cvl->addWidget(div);

    // Headline KPIs, charts and tables of the analysis (shared result renderer, RAW toggle included).
    result_view_ = new ui::ResultView(content);
    cvl->addWidget(result_view_, 1);

    pane_->set_content(content);
    pane_->set_cache_key(QStringLiteral("alt_investments"));
    pane_->set_auto_delay(300); // analyzers and categories are browsed quickly
    // Deterministic local calculations: a remembered result whose inputs match (set_signature: the form
    // JSON cli.py receives) needs no re-run; changed inputs show "Inputs changed — RUN AGAIN".
    pane_->set_max_age(ui::autoload::kNeverRefresh);
    pane_->set_status_label(result_status_);
    pane_->preview()->set_content(QStringLiteral("\u25B6"), tr("Analysis stopped"),
                                  tr("Run it again with the inputs on the left."), tr("ANALYZE"));
    pane_->set_start([this](quint64 id) { run_analysis(id); });
    pane_->set_cancel([this](quint64) {
        if (cancel_token_) {
            cancel_token_->store(true); // a queued run never starts; a running one is killed
            cancel_token_.reset();
        }
    });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) { render_result(payload); });
    connect(pane_, &ui::AutoLoadPane::running_changed, this, [this](bool) { update_running(); });
    // A change of display currency or number format re-renders the result on screen (deferred while hidden).
    fincept::ui::formatting::on_format_changed(this, [this]() {
        if (pane_ && pane_->has_result())
            render_result(pane_->payload());
    });
    connect(pane_, &ui::AutoLoadPane::state_changed, this, [this](ui::AutoLoadPane::State state) {
        // Nothing of this view on screen: COPY / CSV / IMAGE have nothing to export.
        if (state != ui::AutoLoadPane::State::Content && !pane_->has_result())
            last_result_ = QJsonObject();
        update_actions();
    });
    vl->addWidget(pane_, 1);
    return panel;
}

// ── Modes ─────────────────────────────────────────────────────────────────────

namespace {
struct AltModeDef {
    const char* id; ///< stable key saved with the screen state
    const char* label;
    const char* tip;
    QWidget* (*make)(QWidget* parent); ///< nullptr = the analyzer catalogue (page 0)
};

const AltModeDef kAltModes[] = {
    {"analyzers", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "ANALYZERS"),
     QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Single-instrument calculators and product guides by asset class"), nullptr},
    {"private_markets", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "PRIVATE MARKETS"),
     QT_TRANSLATE_NOOP("AltInvestmentsScreen",
                       "Your funds' dated calls, distributions and NAV: IRR, TVPI / DPI / RVPI, J-curve, PME against "
                       "an index, commitment pacing, the whole book"),
     &alt::make_private_markets_panel},
    {"real_estate", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "REAL ESTATE"),
     QT_TRANSLATE_NOOP("AltInvestmentsScreen",
                       "Multi-year property pro forma with financing and exit, sensitivity, and a listed-REIT comparison"),
     &alt::make_real_estate_panel},
    {"fund_analytics", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "FUND ANALYTICS"),
     QT_TRANSLATE_NOOP("AltInvestmentsScreen",
                       "A fund's or liquid alternative's returns against a benchmark: risk, drawdown, capture, rolling "
                       "statistics"),
     &alt::make_fund_analytics_panel},
    {"allocation", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "ALLOCATION & LIQUIDITY"),
     QT_TRANSLATE_NOOP("AltInvestmentsScreen",
                       "The alternatives sleeve inside a portfolio: allocation, liquidity ladder, unfunded commitments, "
                       "stress"),
     &alt::make_allocation_panel},
    {"credit_dd", QT_TRANSLATE_NOOP("AltInvestmentsScreen", "CREDIT & DUE DILIGENCE"),
     QT_TRANSLATE_NOOP("AltInvestmentsScreen", "Private credit analytics and due-diligence checklists"),
     &alt::make_due_diligence_panel},
};
} // namespace

QString AltInvestmentsScreen::mode_id(int index) const {
    const int n = int(sizeof(kAltModes) / sizeof(kAltModes[0]));
    return (index >= 0 && index < n) ? QString::fromLatin1(kAltModes[index].id) : QString();
}

int AltInvestmentsScreen::mode_index(const QString& id) const {
    const int n = int(sizeof(kAltModes) / sizeof(kAltModes[0]));
    for (int i = 0; i < n; ++i)
        if (id == QLatin1String(kAltModes[i].id))
            return i;
    return 0;
}

QWidget* AltInvestmentsScreen::create_mode_strip() {
    auto* strip = new QWidget(this);
    strip->setObjectName(QStringLiteral("altModeStrip"));
    auto* hl = new QHBoxLayout(strip);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(0);
    const int n = int(sizeof(kAltModes) / sizeof(kAltModes[0]));
    mode_pages_.fill(nullptr, n);
    for (int i = 0; i < n; ++i) {
        QString text = QCoreApplication::translate("AltInvestmentsScreen", kAltModes[i].label);
        auto* b = new QPushButton(text.replace(QLatin1Char('&'), QLatin1String("&&")), strip);
        b->setObjectName(QStringLiteral("altModeBtn"));
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(QCoreApplication::translate("AltInvestmentsScreen", kAltModes[i].tip));
        connect(b, &QPushButton::clicked, this, [this, i]() { set_mode(i); });
        hl->addWidget(b);
        mode_btns_ << b;
    }
    hl->addStretch(1);
    return strip;
}

void AltInvestmentsScreen::set_mode(int index) {
    const int n = int(sizeof(kAltModes) / sizeof(kAltModes[0]));
    if (index < 0 || index >= n || !mode_stack_)
        index = 0;
    for (int i = 0; i < mode_btns_.size(); ++i)
        mode_btns_[i]->setChecked(i == index);
    if (index == 0) {
        mode_stack_->setCurrentIndex(0);
    } else {
        if (!mode_pages_[index]) {
            // Built under its final parent, the first time the mode is chosen.
            QWidget* page = kAltModes[index].make(mode_stack_);
            mode_stack_->addWidget(page);
            mode_pages_[index] = page;
            LOG_INFO("AltInvestments", QStringLiteral("Workbench panel created mode=%1").arg(index));
        }
        mode_stack_->setCurrentWidget(mode_pages_[index]);
    }
    if (active_mode_ != index) {
        active_mode_ = index;
        if (!restoring_)
            fincept::ScreenStateManager::instance().notify_changed(this);
    }
}

QWidget* AltInvestmentsScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("altStatusBar");
    bar->setMinimumHeight(26);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(8);
    status_left_ = new QLabel(tr("ALTERNATIVE INVESTMENTS"), bar);
    status_left_->setObjectName("altStatusText");
    hl->addWidget(status_left_);
    hl->addStretch(1);
    status_category_ = new QLabel(bar);
    status_category_->setObjectName("altStatusText");
    hl->addWidget(status_category_);
    hl->addSpacing(10);
    status_analyzer_ = new QLabel(bar);
    status_analyzer_->setObjectName("altStatusHigh");
    hl->addWidget(status_analyzer_);
    return bar;
}

} // namespace fincept::screens
