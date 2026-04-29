#include "SurfaceControlPanel.h"

#include "SurfaceDefaults.h"
#include "services/databento/DatabentoService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStringListModel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::surface {

using namespace fincept::ui;

namespace {

QString section_title_qss() {
    return QString("QGroupBox { background:%1; border:1px solid %2; border-radius:3px; "
                   "margin-top:10px; padding:10px 8px 8px 8px; color:%3; font-size:9px; "
                   "font-weight:bold; }"
                   "QGroupBox::title { subcontrol-origin:margin; subcontrol-position:top left; "
                   "left:10px; padding:0 4px; color:%4; }")
        .arg(colors::BG_SURFACE())
        .arg(colors::BORDER_DIM())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::TEXT_SECONDARY());
}

QString line_edit_qss() {
    return QString("QLineEdit { background:%1; color:%2; border:1px solid %3; "
                   "padding:4px 6px; font-size:10px; border-radius:2px; }"
                   "QLineEdit:focus { border-color:%4; }")
        .arg(colors::BG_HOVER())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_MED())
        .arg(colors::INFO());
}

QString combo_qss() {
    return QString("QComboBox { background:%1; color:%2; border:1px solid %3; "
                   "padding:3px 6px; font-size:10px; border-radius:2px; }"
                   "QComboBox::drop-down { border:none; width:18px; }"
                   "QComboBox QAbstractItemView { background:%1; color:%2; "
                   "selection-background-color:%4; }")
        .arg(colors::BG_HOVER())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_MED())
        .arg(colors::BG_RAISED());
}

QString small_btn_qss() {
    return QString("QToolButton, QPushButton { background:%1; color:%2; border:1px solid %3; "
                   "padding:3px 8px; font-size:9px; border-radius:2px; }"
                   "QToolButton:hover, QPushButton:hover { background:%3; color:%4; }"
                   "QPushButton:disabled { background:%1; color:%5; border-color:%1; }")
        .arg(colors::BG_RAISED())
        .arg(colors::TEXT_SECONDARY())
        .arg(colors::BORDER_MED())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::TEXT_DIM());
}

QString fetch_btn_qss(bool enabled) {
    if (!enabled) {
        return QString("QPushButton { background:%1; color:%2; border:1px solid %3; "
                       "padding:8px 12px; font-size:10px; font-weight:bold; border-radius:2px; }")
            .arg(colors::BG_HOVER())
            .arg(colors::TEXT_DIM())
            .arg(colors::BORDER_DIM());
    }
    return QString("QPushButton { background:rgba(31,77,153,255); color:%1; "
                   "border:1px solid rgba(64,140,255,140); padding:8px 12px; "
                   "font-size:10px; font-weight:bold; border-radius:2px; }"
                   "QPushButton:hover { background:rgba(46,102,191,255); }")
        .arg(colors::INFO());
}

QColor tier_color(SurfaceTier t) {
    switch (t) {
        case SurfaceTier::LIVE:
            return QColor(63, 185, 80);
        case SurfaceTier::COMPUTED:
            return QColor(88, 166, 255);
        case SurfaceTier::EQUITIES:
            return QColor(217, 164, 6);
        case SurfaceTier::DEMO:
            return QColor(140, 140, 140);
    }
    return QColor(140, 140, 140);
}

void set_metric(QLabel* lbl, const QString& v) {
    if (lbl)
        lbl->setText(v);
}

} // anonymous namespace

SurfaceControlPanel::SurfaceControlPanel(QWidget* parent) : QWidget(parent) {
    // Databento publishes most schemas with a multi-hour lag, so "today" is
    // almost always after the dataset's available end. Default end to
    // *yesterday*; the screen refines via metadata.get_dataset_range when the
    // active capability has a known dataset.
    QDate yesterday = QDate::currentDate().addDays(-1);
    state_.start_date = yesterday.addDays(-defaults::LOOKBACK_DAYS);
    state_.end_date = yesterday;
    state_.strike_window_pct = defaults::STRIKE_WINDOW_PCT;
    state_.dte_min = defaults::DTE_MIN;
    state_.dte_max = defaults::DTE_MAX;
    for (auto* s : defaults::RISK_BASKET)
        state_.basket << QString::fromUtf8(s);
    setup_ui();
    apply_capability_visibility();
}

void SurfaceControlPanel::setup_ui() {
    setStyleSheet(QString("background:%1; color:%2;")
                      .arg(colors::BG_BASE())
                      .arg(colors::TEXT_PRIMARY()));
    setMinimumWidth(300);
    setMaximumWidth(360);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Header ─────────────────────────────────────────────────────────────
    auto* header = new QLabel("CONTROL PANEL", this);
    header->setStyleSheet(QString("background:%1; color:%2; font-size:10px; font-weight:bold; "
                                  "padding:8px 10px; border-bottom:1px solid %3;")
                              .arg(colors::BG_SURFACE())
                              .arg(colors::TEXT_PRIMARY())
                              .arg(colors::BORDER_DIM()));
    outer->addWidget(header);

    // ── Provider status strip (one row per provider; just Databento for now) ─
    providers_box_ = new QWidget(this);
    providers_box_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                       .arg(colors::BG_SURFACE())
                                       .arg(colors::BORDER_DIM()));
    auto* prov_layout = new QVBoxLayout(providers_box_);
    prov_layout->setContentsMargins(8, 6, 8, 6);
    prov_layout->setSpacing(3);

    auto* prov_title = new QLabel("DATA PROVIDERS", providers_box_);
    prov_title->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:bold; letter-spacing:0.5px;")
            .arg(colors::TEXT_DIM()));
    prov_layout->addWidget(prov_title);

    auto add_provider_row = [&](const QString& key, const QString& label) {
        auto* row = new QHBoxLayout();
        row->setSpacing(5);
        auto* dot = new QLabel("●", providers_box_);
        dot->setStyleSheet(QString("color:%1; font-size:11px;").arg(colors::TEXT_DIM()));
        dot->setFixedWidth(10);
        row->addWidget(dot);
        auto* name_lbl = new QLabel(label, providers_box_);
        name_lbl->setStyleSheet(QString("color:%1; font-size:10px; font-family:Consolas;")
                                    .arg(colors::TEXT_PRIMARY()));
        name_lbl->setMinimumWidth(80);
        row->addWidget(name_lbl);
        auto* state_lbl = new QLabel("not configured", providers_box_);
        state_lbl->setStyleSheet(QString("color:%1; font-size:9px;").arg(colors::TEXT_DIM()));
        row->addWidget(state_lbl, 1);
        auto* detail_lbl = new QLabel("", providers_box_);
        detail_lbl->setStyleSheet(QString("color:%1; font-size:9px; font-family:Consolas;")
                                       .arg(colors::TEXT_DIM()));
        row->addWidget(detail_lbl);
        prov_layout->addLayout(row);
        provider_dot_[key] = dot;
        provider_state_[key] = state_lbl;
        provider_detail_[key] = detail_lbl;
    };
    add_provider_row("databento", "Databento");
    // Future: add_provider_row("polygon", "Polygon");
    // Future: add_provider_row("tradier", "Tradier");

    outer->addWidget(providers_box_);

    // ── Scroll body ────────────────────────────────────────────────────────
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));
    auto* body = new QWidget(scroll);
    auto* body_layout = new QVBoxLayout(body);
    body_layout->setContentsMargins(8, 8, 8, 8);
    body_layout->setSpacing(8);

    body_layout->addWidget((asset_box_ = build_asset_section()));
    body_layout->addWidget((dates_box_ = build_dates_section()));
    body_layout->addWidget((options_box_ = build_options_section()));
    body_layout->addWidget((basket_box_ = build_basket_section()));
    body_layout->addWidget((metrics_box_ = build_metrics_section()));
    body_layout->addWidget((lineage_box_ = build_lineage_section()));
    body_layout->addStretch();

    scroll->setWidget(body);
    outer->addWidget(scroll, 1);

    // ── Footer (sticky) — FETCH ────────────────────────────────────────────
    auto* footer = new QWidget(this);
    footer->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                              .arg(colors::BG_SURFACE())
                              .arg(colors::BORDER_DIM()));
    auto* footer_layout = new QVBoxLayout(footer);
    footer_layout->setContentsMargins(8, 8, 8, 8);
    fetch_btn_ = new QPushButton("FETCH", footer);
    fetch_btn_->setStyleSheet(fetch_btn_qss(true));
    fetch_btn_->setMinimumHeight(34);
    connect(fetch_btn_, &QPushButton::clicked, this, &SurfaceControlPanel::on_fetch_clicked);
    footer_layout->addWidget(fetch_btn_);
    outer->addWidget(footer);

    prefill_datasets();
}

QGroupBox* SurfaceControlPanel::build_asset_section() {
    auto* gb = new QGroupBox("ASSET", this);
    gb->setStyleSheet(section_title_qss());
    auto* l = new QVBoxLayout(gb);
    l->setContentsMargins(8, 12, 8, 8);
    l->setSpacing(6);

    symbol_edit_ = new QLineEdit(gb);
    symbol_edit_->setPlaceholderText("Underlying / parent symbol");
    symbol_edit_->setText(state_.symbol);
    symbol_edit_->setStyleSheet(line_edit_qss());
    connect(symbol_edit_, &QLineEdit::editingFinished, this, &SurfaceControlPanel::on_symbol_edited);
    connect(symbol_edit_, &QLineEdit::textEdited, this, &SurfaceControlPanel::on_symbol_text_changed);
    l->addWidget(symbol_edit_);

    // ── Autocomplete model + completer (populated by Databento symbol_search) ─
    symbol_completer_model_ = new QStringListModel(this);
    symbol_completer_ = new QCompleter(symbol_completer_model_, this);
    symbol_completer_->setCaseSensitivity(Qt::CaseInsensitive);
    symbol_completer_->setFilterMode(Qt::MatchContains);
    symbol_completer_->setCompletionMode(QCompleter::PopupCompletion);
    symbol_completer_->setMaxVisibleItems(10);
    symbol_edit_->setCompleter(symbol_completer_);

    // 300 ms debounce — avoids hammering the Databento API on every keypress.
    symbol_search_timer_ = new QTimer(this);
    symbol_search_timer_->setSingleShot(true);
    symbol_search_timer_->setInterval(300);
    connect(symbol_search_timer_, &QTimer::timeout, this,
            &SurfaceControlPanel::on_search_debounce_fired);

    auto* row = new QHBoxLayout();
    row->setSpacing(6);
    auto* ds_lbl = new QLabel("Dataset:", gb);
    ds_lbl->setStyleSheet(QString("color:%1; font-size:9px;").arg(colors::TEXT_SECONDARY()));
    row->addWidget(ds_lbl);
    dataset_combo_ = new QComboBox(gb);
    dataset_combo_->setStyleSheet(combo_qss());
    connect(dataset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SurfaceControlPanel::on_dataset_changed);
    row->addWidget(dataset_combo_, 1);
    l->addLayout(row);

    spot_label_ = new QLabel("Spot: —", gb);
    spot_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(colors::TEXT_DIM()));
    l->addWidget(spot_label_);

    tier_badge_ = new QLabel("DEMO", gb);
    tier_badge_->setAlignment(Qt::AlignCenter);
    tier_badge_->setStyleSheet(
        QString("background:%1; color:#000; font-size:9px; font-weight:bold; "
                "padding:2px 6px; border-radius:2px; max-width:80px;")
            .arg(colors::TEXT_DIM()));
    l->addWidget(tier_badge_, 0, Qt::AlignLeft);

    return gb;
}

QGroupBox* SurfaceControlPanel::build_dates_section() {
    auto* gb = new QGroupBox("DATE RANGE", this);
    gb->setStyleSheet(section_title_qss());
    auto* l = new QVBoxLayout(gb);
    l->setContentsMargins(8, 12, 8, 8);
    l->setSpacing(6);

    // Default acceptable range — capped at yesterday since Databento data is
    // never published instantaneously. Widened/narrowed dynamically when the
    // screen calls apply_dataset_range() with concrete metadata.
    QDate default_min(2013, 1, 1);
    QDate default_max = QDate::currentDate().addDays(-1);

    auto build_date_row = [&](const QString& title, QDateEdit*& target, const QDate& def) {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto* lbl = new QLabel(title, gb);
        lbl->setStyleSheet(QString("color:%1; font-size:9px;").arg(colors::TEXT_SECONDARY()));
        lbl->setMinimumWidth(40);
        row->addWidget(lbl);
        target = new QDateEdit(def, gb);
        target->setCalendarPopup(true);
        target->setDisplayFormat("yyyy-MM-dd");
        target->setMinimumDate(default_min);
        target->setMaximumDate(default_max);
        target->setStyleSheet(combo_qss());
        connect(target, &QDateEdit::dateChanged, this, &SurfaceControlPanel::on_dates_changed);
        row->addWidget(target, 1);
        l->addLayout(row);
    };
    build_date_row("Start:", start_edit_, state_.start_date);
    build_date_row("End:", end_edit_, state_.end_date);

    // Lookback presets
    auto* presets = new QHBoxLayout();
    presets->setSpacing(4);
    auto add_preset = [&](const QString& text, int days, QToolButton*& target) {
        target = new QToolButton(gb);
        target->setText(text);
        target->setStyleSheet(small_btn_qss());
        connect(target, &QToolButton::clicked, this, [this, days]() { on_lookback_preset(days); });
        presets->addWidget(target);
    };
    add_preset("1D", 1, lb_1d_);
    add_preset("5D", 5, lb_5d_);
    add_preset("1M", 30, lb_1m_);
    add_preset("3M", 90, lb_3m_);
    add_preset("1Y", 365, lb_1y_);
    presets->addStretch();
    l->addLayout(presets);

    return gb;
}

QGroupBox* SurfaceControlPanel::build_options_section() {
    auto* gb = new QGroupBox("OPTION FILTERS", this);
    gb->setStyleSheet(section_title_qss());
    auto* l = new QVBoxLayout(gb);
    l->setContentsMargins(8, 12, 8, 8);
    l->setSpacing(6);

    auto build_spin_row = [&](const QString& title, int min_v, int max_v, int initial,
                              QSpinBox*& target, const QString& suffix = QString()) {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto* lbl = new QLabel(title, gb);
        lbl->setStyleSheet(QString("color:%1; font-size:9px;").arg(colors::TEXT_SECONDARY()));
        lbl->setMinimumWidth(110);
        row->addWidget(lbl);
        target = new QSpinBox(gb);
        target->setRange(min_v, max_v);
        target->setValue(initial);
        if (!suffix.isEmpty())
            target->setSuffix(suffix);
        target->setStyleSheet(combo_qss());
        row->addWidget(target, 1);
        l->addLayout(row);
    };

    build_spin_row("Strike window:", 1, 100, state_.strike_window_pct, strike_window_spin_, "%");
    connect(strike_window_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SurfaceControlPanel::on_strike_window_changed);

    build_spin_row("DTE min (days):", 0, 3650, state_.dte_min, dte_min_spin_);
    build_spin_row("DTE max (days):", 1, 3650, state_.dte_max, dte_max_spin_);
    connect(dte_min_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SurfaceControlPanel::on_dte_changed);
    connect(dte_max_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SurfaceControlPanel::on_dte_changed);

    auto* iv_row = new QHBoxLayout();
    iv_row->setSpacing(6);
    auto* iv_lbl = new QLabel("IV method:", gb);
    iv_lbl->setStyleSheet(QString("color:%1; font-size:9px;").arg(colors::TEXT_SECONDARY()));
    iv_lbl->setMinimumWidth(110);
    iv_row->addWidget(iv_lbl);
    iv_method_combo_ = new QComboBox(gb);
    iv_method_combo_->addItems({"Brent", "Bisection", "Newton-Raphson"});
    iv_method_combo_->setStyleSheet(combo_qss());
    connect(iv_method_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SurfaceControlPanel::on_iv_method_changed);
    iv_row->addWidget(iv_method_combo_, 1);
    l->addLayout(iv_row);

    return gb;
}

QGroupBox* SurfaceControlPanel::build_basket_section() {
    auto* gb = new QGroupBox("BASKET", this);
    gb->setStyleSheet(section_title_qss());
    auto* l = new QVBoxLayout(gb);
    l->setContentsMargins(8, 12, 8, 8);
    l->setSpacing(6);

    auto* input_row = new QHBoxLayout();
    input_row->setSpacing(4);
    basket_input_ = new QLineEdit(gb);
    basket_input_->setPlaceholderText("Add ticker");
    basket_input_->setStyleSheet(line_edit_qss());
    input_row->addWidget(basket_input_, 1);
    basket_add_btn_ = new QPushButton("+", gb);
    basket_add_btn_->setStyleSheet(small_btn_qss());
    basket_add_btn_->setFixedWidth(26);
    connect(basket_add_btn_, &QPushButton::clicked, this, &SurfaceControlPanel::on_basket_add);
    connect(basket_input_, &QLineEdit::returnPressed, this, &SurfaceControlPanel::on_basket_add);
    input_row->addWidget(basket_add_btn_);
    basket_remove_btn_ = new QPushButton("–", gb);
    basket_remove_btn_->setStyleSheet(small_btn_qss());
    basket_remove_btn_->setFixedWidth(26);
    connect(basket_remove_btn_, &QPushButton::clicked, this, &SurfaceControlPanel::on_basket_remove);
    input_row->addWidget(basket_remove_btn_);
    l->addLayout(input_row);

    basket_list_ = new QListWidget(gb);
    basket_list_->setStyleSheet(QString("QListWidget { background:%1; color:%2; "
                                        "border:1px solid %3; font-size:10px; }"
                                        "QListWidget::item:selected { background:%4; color:%5; }")
                                    .arg(colors::BG_HOVER())
                                    .arg(colors::TEXT_PRIMARY())
                                    .arg(colors::BORDER_MED())
                                    .arg(colors::BG_RAISED())
                                    .arg(colors::TEXT_PRIMARY()));
    basket_list_->setMinimumHeight(110);
    for (const QString& t : state_.basket)
        basket_list_->addItem(t);
    l->addWidget(basket_list_);

    return gb;
}

QGroupBox* SurfaceControlPanel::build_metrics_section() {
    auto* gb = new QGroupBox("METRICS", this);
    gb->setStyleSheet(section_title_qss());
    auto* l = new QVBoxLayout(gb);
    l->setContentsMargins(8, 12, 8, 8);
    l->setSpacing(2);

    auto add_row = [&](const QString& label, QLabel*& target) {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto* lbl = new QLabel(label, gb);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-family:Consolas;")
                               .arg(colors::TEXT_SECONDARY()));
        lbl->setMinimumWidth(80);
        row->addWidget(lbl);
        target = new QLabel("—", gb);
        target->setStyleSheet(QString("color:%1; font-size:10px; font-family:Consolas; font-weight:bold;")
                                  .arg(colors::TEXT_PRIMARY()));
        row->addWidget(target, 1, Qt::AlignRight);
        l->addLayout(row);
    };

    add_row("count", metrics_count_);
    add_row("min", metrics_min_);
    add_row("max", metrics_max_);
    add_row("mean", metrics_mean_);
    add_row("median", metrics_median_);
    add_row("std", metrics_std_);
    add_row("skew", metrics_skew_);
    add_row("kurt", metrics_kurt_);

    return gb;
}

QGroupBox* SurfaceControlPanel::build_lineage_section() {
    auto* gb = new QGroupBox("LINEAGE", this);
    gb->setStyleSheet(section_title_qss());
    auto* l = new QVBoxLayout(gb);
    l->setContentsMargins(8, 12, 8, 8);
    lineage_label_ = new QLabel("—", gb);
    lineage_label_->setWordWrap(true);
    lineage_label_->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:Consolas;").arg(colors::TEXT_SECONDARY()));
    l->addWidget(lineage_label_);
    return gb;
}

void SurfaceControlPanel::prefill_datasets() {
    // Static seed list — the screen calls list_datasets() asynchronously and
    // updates this combo when results arrive (see SurfaceAnalyticsScreen).
    QStringList seed = {"OPRA.PILLAR", "GLBX.MDP3", "EQUS.MINI", "XNAS.ITCH", "XNYS.PILLAR",
                        "IFEU.IMPACT", "IFUS.IMPACT"};
    dataset_combo_->blockSignals(true);
    dataset_combo_->clear();
    for (const QString& s : seed)
        dataset_combo_->addItem(s);
    dataset_combo_->blockSignals(false);
}

void SurfaceControlPanel::set_capability(ChartType type) {
    active_type_ = type;
    const auto& cap = capability_for(type);

    // Tier badge
    QColor bg = tier_color(cap.tier);
    tier_badge_->setText(tier_name(cap.tier));
    tier_badge_->setStyleSheet(
        QString("background:%1; color:#000; font-size:9px; font-weight:bold; "
                "padding:2px 6px; border-radius:2px; max-width:80px;")
            .arg(bg.name()));

    // Default dataset for this surface
    if (!QString(cap.dataset).isEmpty()) {
        int idx = dataset_combo_->findText(cap.dataset);
        if (idx < 0) {
            dataset_combo_->addItem(cap.dataset);
            idx = dataset_combo_->findText(cap.dataset);
        }
        dataset_combo_->blockSignals(true);
        dataset_combo_->setCurrentIndex(idx);
        dataset_combo_->blockSignals(false);
        state_.dataset = cap.dataset;
    }

    apply_capability_visibility();

    // FETCH button enable state
    bool can_fetch = cap.tier != SurfaceTier::DEMO;
    fetch_btn_->setEnabled(can_fetch);
    fetch_btn_->setStyleSheet(fetch_btn_qss(can_fetch));
    fetch_btn_->setToolTip(can_fetch ? QString()
                                     : "No Databento source for this surface — "
                                       "viewing synthetic data");

    update_lineage(QString::fromUtf8(cap.description));
}

void SurfaceControlPanel::apply_capability_visibility() {
    const auto& cap = capability_for(active_type_);
    asset_box_->setVisible(cap.needs_underlying || cap.needs_correlation_basket ||
                           cap.tier != SurfaceTier::DEMO);
    dates_box_->setVisible(cap.needs_date_range);
    options_box_->setVisible(cap.needs_strike_window || cap.needs_expiry_filter);
    basket_box_->setVisible(cap.needs_correlation_basket);
}

void SurfaceControlPanel::update_metrics(const std::vector<std::vector<float>>& z, const QString& units) {
    std::vector<float> flat;
    flat.reserve(64);
    for (const auto& row : z)
        for (float v : row)
            if (std::isfinite(v))
                flat.push_back(v);
    if (flat.empty()) {
        for (auto* l : {metrics_count_, metrics_min_, metrics_max_, metrics_mean_,
                        metrics_median_, metrics_std_, metrics_skew_, metrics_kurt_})
            set_metric(l, "—");
        return;
    }
    std::sort(flat.begin(), flat.end());
    double n = (double)flat.size();
    double sum = 0;
    for (float v : flat)
        sum += v;
    double mean = sum / n;
    double var = 0, m3 = 0, m4 = 0;
    for (float v : flat) {
        double d = v - mean;
        var += d * d;
        m3 += d * d * d;
        m4 += d * d * d * d;
    }
    var /= n;
    double std_dev = std::sqrt(var);
    double skew = (std_dev > 0) ? (m3 / n) / std::pow(std_dev, 3) : 0.0;
    double kurt = (std_dev > 0) ? (m4 / n) / std::pow(std_dev, 4) - 3.0 : 0.0;
    double median = flat[flat.size() / 2];

    QString suffix = units.isEmpty() ? QString() : QStringLiteral(" ") + units;
    set_metric(metrics_count_, QString::number((qint64)n));
    set_metric(metrics_min_, QString::number(flat.front(), 'f', 4) + suffix);
    set_metric(metrics_max_, QString::number(flat.back(), 'f', 4) + suffix);
    set_metric(metrics_mean_, QString::number(mean, 'f', 4) + suffix);
    set_metric(metrics_median_, QString::number(median, 'f', 4) + suffix);
    set_metric(metrics_std_, QString::number(std_dev, 'f', 4) + suffix);
    set_metric(metrics_skew_, QString::number(skew, 'f', 3));
    set_metric(metrics_kurt_, QString::number(kurt, 'f', 3));
}

void SurfaceControlPanel::update_lineage(const QString& line) {
    if (lineage_label_)
        lineage_label_->setText(line.isEmpty() ? "—" : line);
}

void SurfaceControlPanel::apply_state(const SurfaceControlsState& s) {
    state_ = s;
    if (symbol_edit_)
        symbol_edit_->setText(s.symbol);
    if (start_edit_ && s.start_date.isValid())
        start_edit_->setDate(s.start_date);
    if (end_edit_ && s.end_date.isValid())
        end_edit_->setDate(s.end_date);
    if (strike_window_spin_)
        strike_window_spin_->setValue(s.strike_window_pct);
    if (dte_min_spin_)
        dte_min_spin_->setValue(s.dte_min);
    if (dte_max_spin_)
        dte_max_spin_->setValue(s.dte_max);
    if (iv_method_combo_) {
        int idx = iv_method_combo_->findText(s.iv_method);
        if (idx >= 0)
            iv_method_combo_->setCurrentIndex(idx);
    }
    if (basket_list_) {
        basket_list_->clear();
        for (const QString& t : s.basket)
            basket_list_->addItem(t);
    }
}

// ── Slots ──────────────────────────────────────────────────────────────────
void SurfaceControlPanel::on_symbol_edited() {
    QString s = symbol_edit_->text().trimmed().toUpper();
    if (s == state_.symbol)
        return;
    state_.symbol = s;
    emit symbol_changed(s);
    emit controls_changed();
}

void SurfaceControlPanel::on_symbol_text_changed(const QString& text) {
    QString q = text.trimmed();
    if (q.size() < 1) {
        if (symbol_completer_model_)
            symbol_completer_model_->setStringList({});
        return;
    }
    pending_search_query_ = q;
    if (symbol_search_timer_)
        symbol_search_timer_->start();
}

void SurfaceControlPanel::on_search_debounce_fired() {
    if (pending_search_query_.isEmpty())
        return;
    QString query = pending_search_query_;
    QString dataset = dataset_combo_ ? dataset_combo_->currentText() : QString();
    QPointer<SurfaceControlPanel> self = this;
    DatabentoService::instance().search_symbols(
        query, dataset, [self, query](QStringList matches) {
            if (!self || !self->symbol_completer_model_)
                return;
            // Only apply if the query is still the freshest pending one.
            if (self->pending_search_query_ != query)
                return;
            self->symbol_completer_model_->setStringList(matches);
            if (self->symbol_completer_ && self->symbol_edit_ &&
                self->symbol_edit_->hasFocus() && !matches.isEmpty()) {
                self->symbol_completer_->complete();
            }
        });
}

void SurfaceControlPanel::on_dataset_changed() {
    state_.dataset = dataset_combo_->currentText();
    emit controls_changed();
}

void SurfaceControlPanel::on_dates_changed() {
    state_.start_date = start_edit_->date();
    state_.end_date = end_edit_->date();
    emit controls_changed();
}

void SurfaceControlPanel::on_lookback_preset(int days) {
    QDate end = QDate::currentDate();
    QDate start = end.addDays(-days);
    end_edit_->setDate(end);
    start_edit_->setDate(start);
    // dates_changed slot will fire and emit controls_changed
}

void SurfaceControlPanel::on_strike_window_changed(int v) {
    state_.strike_window_pct = v;
    emit controls_changed();
}

void SurfaceControlPanel::on_dte_changed() {
    state_.dte_min = dte_min_spin_->value();
    state_.dte_max = dte_max_spin_->value();
    if (state_.dte_max < state_.dte_min) {
        state_.dte_max = state_.dte_min;
        dte_max_spin_->blockSignals(true);
        dte_max_spin_->setValue(state_.dte_max);
        dte_max_spin_->blockSignals(false);
    }
    emit controls_changed();
}

void SurfaceControlPanel::on_iv_method_changed() {
    state_.iv_method = iv_method_combo_->currentText();
    emit controls_changed();
}

void SurfaceControlPanel::on_basket_add() {
    QString s = basket_input_->text().trimmed().toUpper();
    if (s.isEmpty())
        return;
    if (!state_.basket.contains(s)) {
        state_.basket << s;
        basket_list_->addItem(s);
        emit controls_changed();
    }
    basket_input_->clear();
}

void SurfaceControlPanel::on_basket_remove() {
    auto items = basket_list_->selectedItems();
    if (items.isEmpty())
        return;
    for (auto* it : items) {
        state_.basket.removeAll(it->text());
        delete basket_list_->takeItem(basket_list_->row(it));
    }
    emit controls_changed();
}

void SurfaceControlPanel::on_fetch_clicked() {
    emit fetch_requested();
}

void SurfaceControlPanel::apply_dataset_range(const QDate& available_start,
                                              const QDate& available_end) {
    if (!start_edit_ || !end_edit_)
        return;
    // Cap end at *yesterday* even if Databento says today, since intraday
    // schemas finalise hours after the close.
    QDate ui_max = available_end.isValid() ? std::min(available_end, QDate::currentDate().addDays(-1))
                                           : QDate::currentDate().addDays(-1);
    QDate ui_min = available_start.isValid() ? available_start : QDate(2013, 1, 1);

    start_edit_->blockSignals(true);
    end_edit_->blockSignals(true);
    start_edit_->setMinimumDate(ui_min);
    start_edit_->setMaximumDate(ui_max);
    end_edit_->setMinimumDate(ui_min);
    end_edit_->setMaximumDate(ui_max);

    // Clamp current values into the new range
    if (state_.end_date > ui_max) {
        state_.end_date = ui_max;
        end_edit_->setDate(state_.end_date);
    }
    if (state_.start_date < ui_min) {
        state_.start_date = ui_min;
        start_edit_->setDate(state_.start_date);
    }
    if (state_.start_date > ui_max) {
        state_.start_date = ui_max.addDays(-defaults::LOOKBACK_DAYS);
        start_edit_->setDate(state_.start_date);
    }
    start_edit_->blockSignals(false);
    end_edit_->blockSignals(false);
}

void SurfaceControlPanel::set_provider_status(const QString& provider_name,
                                              const QString& provider_state,
                                              const QString& detail) {
    auto* dot = provider_dot_.value(provider_name, nullptr);
    auto* lbl = provider_state_.value(provider_name, nullptr);
    auto* dt = provider_detail_.value(provider_name, nullptr);
    if (!dot || !lbl)
        return;
    QString color = colors::TEXT_DIM();
    QString text = provider_state;
    if (provider_state == "connected")
        color = colors::POSITIVE();
    else if (provider_state == "disconnected" || provider_state == "error")
        color = colors::NEGATIVE();
    else if (provider_state == "configured")
        color = QString("rgb(217,164,6)"); // amber, key set but not yet tested
    dot->setStyleSheet(QString("color:%1; font-size:11px;").arg(color));
    lbl->setText(text);
    lbl->setStyleSheet(QString("color:%1; font-size:9px;").arg(color));
    if (dt)
        dt->setText(detail);
}

} // namespace fincept::surface
