// src/screens/alt_investments/AltInvestmentsScreen_Layout.cpp
//
// UI construction: setup_ui, create_header, create_left/center/right panels,
// create_status_bar.
//
// Part of the partial-class split of AltInvestmentsScreen.cpp.

#include "screens/alt_investments/AltInvestmentsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/python_cli/PythonCliService.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>

namespace fincept::screens {

using namespace fincept::ui;

void AltInvestmentsScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(create_header());

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* left = create_left_panel();
    left->setMinimumWidth(170);
    left->setMaximumWidth(210);

    auto* center = create_center_panel();

    auto* right = create_right_panel();
    right->setMinimumWidth(300);
    right->setMaximumWidth(420);

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({190, 550, 380});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());

    on_category_changed(0);
}

QWidget* AltInvestmentsScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("altHeader");
    bar->setFixedHeight(46);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(8);

    auto* tc = new QVBoxLayout;
    tc->setSpacing(1);
    auto* title = new QLabel("ALTERNATIVE INVESTMENTS");
    title->setObjectName("altHeaderTitle");
    auto* sub = new QLabel("27 ANALYZERS  \xB7  10 ASSET CLASSES  \xB7  MULTI-ASSET ANALYTICS");
    sub->setObjectName("altHeaderSub");
    tc->addWidget(title);
    tc->addWidget(sub);
    hl->addLayout(tc);
    hl->addStretch(1);

    auto* badge = new QLabel("PYTHON ANALYTICS ENGINE");
    badge->setObjectName("altHeaderBadge");
    hl->addWidget(badge);
    return bar;
}

QWidget* AltInvestmentsScreen::create_left_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("altLeftPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* title = new QLabel("ASSET CLASSES");
    title->setObjectName("altLeftTitle");
    vl->addWidget(title);

    for (int i = 0; i < categories_.size(); ++i) {
        auto* btn = new QPushButton(categories_[i].name);
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

QWidget* AltInvestmentsScreen::create_center_panel() {
    auto* outer = new QScrollArea;
    outer->setObjectName("altCenterPanel");
    outer->setWidgetResizable(true);
    outer->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Title bar ─────────────────────────────────────────────────────────────
    auto* title_bar = new QWidget(this);
    title_bar->setObjectName("altCenterTitleBar");
    title_bar->setFixedHeight(54);
    auto* tbl = new QHBoxLayout(title_bar);
    tbl->setContentsMargins(16, 0, 16, 0);
    tbl->setSpacing(12);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(2);
    center_title_ = new QLabel;
    center_title_->setObjectName("altCenterTitle");
    center_desc_ = new QLabel;
    center_desc_->setObjectName("altCenterDesc");
    title_col->addWidget(center_title_);
    title_col->addWidget(center_desc_);
    tbl->addLayout(title_col, 1);

    auto* combo_col = new QVBoxLayout;
    combo_col->setSpacing(3);
    auto* combo_lbl = new QLabel("ANALYZER");
    combo_lbl->setObjectName("altComboLabel");
    analyzer_combo_ = new QComboBox;
    analyzer_combo_->setFixedWidth(210);
    connect(analyzer_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AltInvestmentsScreen::on_analyzer_changed);
    combo_col->addWidget(combo_lbl);
    combo_col->addWidget(analyzer_combo_);
    tbl->addLayout(combo_col);
    vl->addWidget(title_bar);

    // ── Form panel ────────────────────────────────────────────────────────────
    auto* form_panel = new QWidget(this);
    form_panel->setObjectName("altFormPanel");
    auto* fpvl = new QVBoxLayout(form_panel);
    fpvl->setContentsMargins(0, 0, 0, 0);
    fpvl->setSpacing(0);

    auto* fhdr = new QWidget(this);
    fhdr->setObjectName("altFormHeader");
    fhdr->setFixedHeight(36);
    auto* fhl = new QHBoxLayout(fhdr);
    fhl->setContentsMargins(12, 0, 12, 0);
    auto* fhtitle = new QLabel("INPUT PARAMETERS");
    fhtitle->setObjectName("altFormTitle");
    analyze_btn_ = new QPushButton("ANALYZE");
    analyze_btn_->setObjectName("altAnalyzeBtn");
    analyze_btn_->setCursor(Qt::PointingHandCursor);
    analyze_btn_->setFixedHeight(26);
    connect(analyze_btn_, &QPushButton::clicked, this, &AltInvestmentsScreen::on_analyze);
    fhl->addWidget(fhtitle);
    fhl->addStretch(1);
    fhl->addWidget(analyze_btn_);
    fpvl->addWidget(fhdr);

    form_container_ = new QWidget(this);
    form_layout_ = new QVBoxLayout(form_container_);
    form_layout_->setContentsMargins(12, 12, 12, 12);
    form_layout_->setSpacing(8);
    fpvl->addWidget(form_container_);

    auto* wrap = new QWidget(this);
    auto* wvl = new QVBoxLayout(wrap);
    wvl->setContentsMargins(12, 12, 12, 12);
    wvl->addWidget(form_panel);
    wvl->addStretch(1);
    vl->addWidget(wrap, 1);

    outer->setWidget(content);
    return outer;
}

QWidget* AltInvestmentsScreen::create_right_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("altRightPanel");
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QLabel("ANALYSIS RESULTS");
    hdr->setObjectName("altRightTitle");
    vl->addWidget(hdr);

    // Verdict summary
    auto* verdict_area = new QWidget(this);
    verdict_area->setFixedHeight(108);
    auto* val = new QVBoxLayout(verdict_area);
    val->setContentsMargins(12, 10, 12, 8);
    val->setSpacing(5);

    verdict_badge_ = new QLabel("AWAITING ANALYSIS");
    verdict_badge_->setObjectName("altVerdictBadge");
    verdict_badge_->setAlignment(Qt::AlignCenter);
    verdict_badge_->setStyleSheet(QString("color:%1; background:%2; font-size:11px; font-weight:700;"
                                          " padding:4px 14px; letter-spacing:0.5px;")
                                      .arg(colors::TEXT_DIM(), colors::BG_RAISED()));
    val->addWidget(verdict_badge_);

    verdict_rating_ = new QLabel;
    verdict_rating_->setObjectName("altVerdictRating");
    verdict_rating_->setAlignment(Qt::AlignCenter);
    val->addWidget(verdict_rating_);

    verdict_rec_ = new QLabel;
    verdict_rec_->setObjectName("altVerdictRec");
    verdict_rec_->setWordWrap(true);
    verdict_rec_->setAlignment(Qt::AlignCenter);
    val->addWidget(verdict_rec_);
    vl->addWidget(verdict_area);

    auto* div = new QWidget(this);
    div->setFixedHeight(1);
    div->setStyleSheet(QString("background:%1;").arg(colors::BORDER_DIM()));
    vl->addWidget(div);

    // Scrollable metric rows
    metrics_scroll_ = new QScrollArea;
    metrics_scroll_->setWidgetResizable(true);
    metrics_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    metrics_scroll_->setFrameShape(QFrame::NoFrame);

    metrics_container_ = new QWidget(this);
    metrics_layout_ = new QVBoxLayout(metrics_container_);
    metrics_layout_->setContentsMargins(0, 0, 0, 0);
    metrics_layout_->setSpacing(0);
    metrics_layout_->addStretch(1);

    metrics_scroll_->setWidget(metrics_container_);
    vl->addWidget(metrics_scroll_, 1);
    return panel;
}

QWidget* AltInvestmentsScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("altStatusBar");
    bar->setFixedHeight(26);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(8);
    auto* lbl = new QLabel("ALTERNATIVE INVESTMENTS");
    lbl->setObjectName("altStatusText");
    hl->addWidget(lbl);
    hl->addStretch(1);
    status_category_ = new QLabel;
    status_category_->setObjectName("altStatusText");
    hl->addWidget(status_category_);
    hl->addSpacing(10);
    status_analyzer_ = new QLabel;
    status_analyzer_->setObjectName("altStatusHigh");
    hl->addWidget(status_analyzer_);
    return bar;
}

// ── Dynamic form ─────────────────────────────────────────────────────────────

} // namespace fincept::screens
