// src/screens/dbnomics/DBnomicsSelectionPanel.cpp
#include "screens/dbnomics/DBnomicsSelectionPanel.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QScrollArea>
#include <QSizePolicy>

// ── Shared constants ─────────────────────────────────────────────────────────
namespace col = fincept::ui::colors;

namespace {

static QString kSpinStyle() {
    return QString("QLabel { color: %1; font-family: 'Consolas','Courier New',monospace;"
                   " font-size: 11px; background: transparent; }")
        .arg(col::TEXT_TERTIARY());
}

// ── Style constants ─────────────────────────────────────────────────────────

static QString kListStyle() {
    return QString("QListWidget {"
                   "  background: %1; color: %2;"
                   "  border: none;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "  font-size: 11px; }"
                   "QListWidget::item { padding: 3px 6px; border-bottom: 1px solid %3; }"
                   "QListWidget::item:hover { background: %4; color: %5; }"
                   "QListWidget::item:selected { background: rgba(217,119,6,0.15); color: %6; }")
        .arg(col::BG_BASE(), col::TEXT_SECONDARY(), col::BORDER_DIM(), col::BG_HOVER(), col::TEXT_PRIMARY(),
             col::AMBER());
}

static QString kInputStyle() {
    return QString("QLineEdit {"
                   "  background: %1; color: %2; border: 1px solid %3;"
                   "  padding: 4px 8px;"
                   "  font-family: 'Consolas','Courier New',monospace; font-size: 11px; }"
                   "QLineEdit:focus { border: 1px solid %4; }")
        .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BORDER_MED(), col::AMBER());
}

static QString kLoadMoreStyle() {
    return QString("QPushButton {"
                   "  background: transparent; color: %1;"
                   "  border: 1px solid %2; padding: 3px 8px;"
                   "  font-family: 'Consolas','Courier New',monospace; font-size: 10px; }"
                   "QPushButton:hover { color: %3; border-color: %3; }")
        .arg(col::TEXT_TERTIARY(), col::BORDER_DIM(), col::AMBER());
}

} // namespace

namespace fincept::screens {

static QLabel* make_spin_label(const QString& text, QWidget* parent) {
    auto* lbl = new QLabel(text, parent);
    lbl->setFixedHeight(80);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(kSpinStyle());
    lbl->hide();
    return lbl;
}

// ── Constructor ──────────────────────────────────────────────────────────────

DBnomicsSelectionPanel::DBnomicsSelectionPanel(QWidget* parent) : QWidget(parent) {
    build_ui();

    anim_timer_ = new QTimer(this);
    anim_timer_->setInterval(120);
    connect(anim_timer_, &QTimer::timeout, this, &DBnomicsSelectionPanel::tick_anim);
}

// ── Helper builders ──────────────────────────────────────────────────────────

QLabel* DBnomicsSelectionPanel::make_section_label(const QString& text) {
    auto* label = new QLabel(text);
    label->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                 "font-family: 'Consolas','Courier New',monospace; "
                                 "padding: 5px 8px; background: %2; "
                                 "border-bottom: 1px solid %3;")
                             .arg(col::AMBER)
                             .arg(col::BG_RAISED)
                             .arg(col::BORDER_DIM));
    return label;
}

QListWidget* DBnomicsSelectionPanel::make_styled_list(int fixed_height) {
    auto* list = new QListWidget();
    list->setStyleSheet(kListStyle());
    list->setFixedHeight(fixed_height);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return list;
}

QPushButton* DBnomicsSelectionPanel::make_load_more_button() {
    auto* btn = new QPushButton("LOAD MORE");
    btn->setStyleSheet(kLoadMoreStyle());
    btn->setFixedHeight(22);
    btn->hide();
    return btn;
}

// ── Section builders ─────────────────────────────────────────────────────────

QWidget* DBnomicsSelectionPanel::build_search_section() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(make_section_label("GLOBAL SEARCH"));

    global_search_input_ = new QLineEdit(w);
    global_search_input_->setStyleSheet(kInputStyle());
    global_search_input_->setFixedHeight(28);
    global_search_input_->setPlaceholderText("Search providers, datasets...");
    layout->addWidget(global_search_input_);

    // Loading spinner (hidden by default, appears between input and results)
    search_spin_ = make_spin_label("⣾  SEARCHING...", w);
    layout->addWidget(search_spin_);

    // Search results content
    search_content_ = new QWidget(w);
    auto* sc_layout = new QVBoxLayout(search_content_);
    sc_layout->setContentsMargins(0, 0, 0, 0);
    sc_layout->setSpacing(0);

    search_results_list_ = make_styled_list(120);
    search_results_list_->hide();
    sc_layout->addWidget(search_results_list_);

    search_load_more_btn_ = make_load_more_button();
    sc_layout->addWidget(search_load_more_btn_);

    layout->addWidget(search_content_);

    // Connect global search input
    connect(global_search_input_, &QLineEdit::textChanged, this, [this](const QString& q) {
        const QString trimmed = q.trimmed();
        if (!trimmed.isEmpty()) {
            set_search_loading(true);
            emit global_search_changed(trimmed);
        } else {
            set_search_loading(false);
            search_results_list_->hide();
            search_load_more_btn_->hide();
        }
    });

    // Connect search results selection
    connect(search_results_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const QString provider_code = item->data(Qt::UserRole).toString();
        const QString dataset_code = item->data(Qt::UserRole + 1).toString();
        selected_provider_ = provider_code;
        selected_dataset_ = dataset_code;
        emit search_result_selected(provider_code, dataset_code);
        emit provider_selected(provider_code);
        emit dataset_selected(dataset_code);
    });

    // Connect load more search
    connect(search_load_more_btn_, &QPushButton::clicked, this,
            [this]() { emit load_more_search_requested(search_next_offset_); });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_provider_section() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(make_section_label("PROVIDERS"));

    prov_spin_ = make_spin_label("⣾  LOADING PROVIDERS...", w);
    layout->addWidget(prov_spin_);

    prov_content_ = new QWidget(w);
    auto* pc_layout = new QVBoxLayout(prov_content_);
    pc_layout->setContentsMargins(0, 0, 0, 0);
    pc_layout->setSpacing(0);

    provider_filter_input_ = new QLineEdit(prov_content_);
    provider_filter_input_->setStyleSheet(kInputStyle());
    provider_filter_input_->setFixedHeight(26);
    provider_filter_input_->setPlaceholderText("Filter providers...");
    pc_layout->addWidget(provider_filter_input_);

    provider_list_ = make_styled_list(130);
    pc_layout->addWidget(provider_list_);

    layout->addWidget(prov_content_);

    // Filter providers by code or name
    connect(provider_filter_input_, &QLineEdit::textChanged, this, [this](const QString& q) {
        const QString lower = q.toLower();
        for (int i = 0; i < provider_list_->count(); ++i) {
            QListWidgetItem* item = provider_list_->item(i);
            const QString code = item->data(Qt::UserRole).toString().toLower();
            const QString text = item->text().toLower();
            item->setHidden(!code.contains(lower) && !text.contains(lower));
        }
    });

    // Handle provider selection
    connect(provider_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selected_provider_ = item->data(Qt::UserRole).toString();
        selected_dataset_.clear();
        selected_series_.clear();
        dataset_list_->clear();
        series_list_->clear();
        dataset_load_more_btn_->hide();
        series_load_more_btn_->hide();
        emit provider_selected(selected_provider_);
    });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_dataset_section() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(make_section_label("DATASETS"));

    ds_spin_ = make_spin_label("⣾  LOADING DATASETS...", w);
    layout->addWidget(ds_spin_);

    ds_content_ = new QWidget(w);
    auto* dc_layout = new QVBoxLayout(ds_content_);
    dc_layout->setContentsMargins(0, 0, 0, 0);
    dc_layout->setSpacing(0);

    dataset_list_ = make_styled_list(130);
    dc_layout->addWidget(dataset_list_);

    dataset_load_more_btn_ = make_load_more_button();
    dc_layout->addWidget(dataset_load_more_btn_);

    layout->addWidget(ds_content_);

    // Handle dataset selection
    connect(dataset_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selected_dataset_ = item->data(Qt::UserRole).toString();
        selected_series_.clear();
        series_list_->clear();
        series_search_input_->clear();
        series_load_more_btn_->hide();
        emit dataset_selected(selected_dataset_);
    });

    // Load more datasets
    connect(dataset_load_more_btn_, &QPushButton::clicked, this,
            [this]() { emit load_more_datasets_requested(datasets_next_offset_); });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_series_section() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(make_section_label("SERIES"));

    series_spin_ = make_spin_label("⣾  LOADING SERIES...", w);
    layout->addWidget(series_spin_);

    series_content_ = new QWidget(w);
    auto* sc_layout = new QVBoxLayout(series_content_);
    sc_layout->setContentsMargins(0, 0, 0, 0);
    sc_layout->setSpacing(0);

    series_search_input_ = new QLineEdit(series_content_);
    series_search_input_->setStyleSheet(kInputStyle());
    series_search_input_->setFixedHeight(26);
    series_search_input_->setPlaceholderText("Search series...");
    sc_layout->addWidget(series_search_input_);

    series_list_ = make_styled_list(130);
    sc_layout->addWidget(series_list_);

    series_load_more_btn_ = make_load_more_button();
    sc_layout->addWidget(series_load_more_btn_);

    layout->addWidget(series_content_);

    // Search series when typing
    connect(series_search_input_, &QLineEdit::textChanged, this, [this](const QString& q) {
        if (!selected_provider_.isEmpty() && !selected_dataset_.isEmpty()) {
            emit series_search_changed(selected_provider_, selected_dataset_, q.trimmed());
        }
    });

    // Handle series selection
    connect(series_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        selected_series_ = item->data(Qt::UserRole).toString();
        emit series_selected(selected_provider_, selected_dataset_, selected_series_);
    });

    // Load more series
    connect(series_load_more_btn_, &QPushButton::clicked, this,
            [this]() { emit load_more_series_requested(series_next_offset_); });

    return w;
}

QWidget* DBnomicsSelectionPanel::build_action_buttons() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    // "ADD TO SINGLE VIEW" button — amber style
    auto* add_btn = new QPushButton("ADD TO SINGLE VIEW");
    add_btn->setFixedHeight(26);
    add_btn->setStyleSheet(
        QString("QPushButton { background: rgba(217,119,6,0.1); color: %1; "
                "border: 1px solid %2; padding: 3px 8px; "
                "font-family: 'Consolas','Courier New',monospace; font-size: 10px; font-weight: 700; }"
                "QPushButton:hover { background: rgba(217,119,6,0.2); }")
            .arg(col::AMBER)
            .arg(col::AMBER_DIM));
    connect(add_btn, &QPushButton::clicked, this, [this]() { emit add_to_single_view_clicked(); });
    layout->addWidget(add_btn);

    // "CLEAR ALL" button — red style
    auto* clear_btn = new QPushButton("CLEAR ALL");
    clear_btn->setFixedHeight(26);
    clear_btn->setStyleSheet(
        QString("QPushButton { background: rgba(220,38,38,0.1); color: %1; "
                "border: 1px solid %2; padding: 3px 8px; "
                "font-family: 'Consolas','Courier New',monospace; font-size: 10px; font-weight: 700; }"
                "QPushButton:hover { background: rgba(220,38,38,0.2); }")
            .arg(col::NEGATIVE)
            .arg("#7f1d1d"));
    connect(clear_btn, &QPushButton::clicked, this, [this]() { emit clear_all_clicked(); });
    layout->addWidget(clear_btn);

    return w;
}

QWidget* DBnomicsSelectionPanel::build_comparison_slots_section() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(make_section_label("COMPARISON SLOTS"));

    // "+ ADD SLOT" button — green style
    auto* add_slot_btn = new QPushButton("+ ADD SLOT");
    add_slot_btn->setFixedHeight(24);
    add_slot_btn->setStyleSheet(
        QString("QPushButton { background: rgba(22,163,74,0.1); color: %1; "
                "border: 1px solid rgba(22,163,74,0.4); padding: 3px 8px; "
                "font-family: 'Consolas','Courier New',monospace; font-size: 10px; font-weight: 700; }"
                "QPushButton:hover { background: rgba(22,163,74,0.2); }")
            .arg(col::POSITIVE));
    connect(add_slot_btn, &QPushButton::clicked, this, [this]() { emit add_slot_clicked(); });
    layout->addWidget(add_slot_btn);

    // Container for slots
    auto* slots_container = new QWidget();
    slots_layout_ = new QVBoxLayout(slots_container);
    slots_layout_->setContentsMargins(4, 4, 4, 4);
    slots_layout_->setSpacing(4);
    layout->addWidget(slots_container);

    return w;
}

// ── Main UI builder ──────────────────────────────────────────────────────────

void DBnomicsSelectionPanel::build_ui() {
    setFixedWidth(280);
    setStyleSheet(QString("background: %1;").arg(col::BG_BASE));

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    // Scrollable content area
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: %1; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(col::BG_BASE)
                              .arg(col::BORDER_DIM));

    auto* content = new QWidget();
    auto* content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(0);

    content_layout->addWidget(build_search_section());
    content_layout->addWidget(build_provider_section());
    content_layout->addWidget(build_dataset_section());
    content_layout->addWidget(build_series_section());
    content_layout->addWidget(build_action_buttons());
    content_layout->addWidget(build_comparison_slots_section());
    content_layout->addStretch();

    scroll->setWidget(content);
    root_layout->addWidget(scroll);

    // Status label — fixed outside scroll at bottom
    status_label_ = new QLabel("Ready");
    status_label_->setStyleSheet(QString("color: %1; font-size: 10px; "
                                         "font-family: 'Consolas','Courier New',monospace; "
                                         "padding: 3px 8px; background: %2; "
                                         "border-top: 1px solid %3;")
                                     .arg(col::TEXT_TERTIARY)
                                     .arg(col::BG_RAISED)
                                     .arg(col::BORDER_DIM));
    root_layout->addWidget(status_label_);
}

// ── Public slots ─────────────────────────────────────────────────────────────

void DBnomicsSelectionPanel::add_comparison_slot() {
    const int slot_idx = slot_count_++;

    auto* slot_widget = new QWidget();
    slot_widget->setStyleSheet(
        QString("QWidget { background: %1; border: 1px solid %2; }").arg(col::BG_SURFACE).arg(col::BORDER_DIM));

    auto* slot_layout = new QVBoxLayout(slot_widget);
    slot_layout->setContentsMargins(4, 4, 4, 4);
    slot_layout->setSpacing(3);

    // Header row: slot label + remove button
    auto* header_row = new QWidget();
    auto* header_layout = new QHBoxLayout(header_row);
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(4);

    auto* slot_label = new QLabel(QString("SLOT %1").arg(slot_idx + 1));
    slot_label->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                      "font-family: 'Consolas','Courier New',monospace;")
                                  .arg(col::AMBER));
    header_layout->addWidget(slot_label);
    header_layout->addStretch();

    auto* remove_btn = new QPushButton("−");
    remove_btn->setFixedSize(18, 18);
    remove_btn->setStyleSheet(QString("QPushButton { background: rgba(220,38,38,0.1); color: %1; "
                                      "border: 1px solid rgba(220,38,38,0.3); font-size: 11px; font-weight: 700; }"
                                      "QPushButton:hover { background: rgba(220,38,38,0.25); }")
                                  .arg(col::NEGATIVE));
    connect(remove_btn, &QPushButton::clicked, this, [this, slot_idx]() { emit remove_slot_clicked(slot_idx); });
    header_layout->addWidget(remove_btn);
    slot_layout->addWidget(header_row);

    // "+ ADD CURRENT SERIES" button
    auto* add_series_btn = new QPushButton("+ ADD CURRENT SERIES");
    add_series_btn->setFixedHeight(20);
    add_series_btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; "
                                          "border: 1px solid %2; "
                                          "font-family: 'Consolas','Courier New',monospace; font-size: 9px; }"
                                          "QPushButton:hover { color: %3; border-color: %3; }")
                                      .arg(col::TEXT_TERTIARY)
                                      .arg(col::BORDER_DIM)
                                      .arg(col::AMBER));
    connect(add_series_btn, &QPushButton::clicked, this, [this, slot_idx]() { emit add_to_slot_clicked(slot_idx); });
    slot_layout->addWidget(add_series_btn);

    slots_layout_->addWidget(slot_widget);
}

void DBnomicsSelectionPanel::remove_comparison_slot(int index) {
    if (index < 0 || index >= slots_layout_->count()) {
        // Fall back: remove the last slot
        index = slots_layout_->count() - 1;
    }
    if (index < 0)
        return;

    QLayoutItem* item = slots_layout_->takeAt(index);
    if (item) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    if (slot_count_ > 0)
        --slot_count_;
}

void DBnomicsSelectionPanel::clear_slots() {
    while (slots_layout_->count() > 0) {
        QLayoutItem* item = slots_layout_->takeAt(0);
        if (item) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }
    slot_count_ = 0;
}

// ── Public data population methods ──────────────────────────────────────────

void DBnomicsSelectionPanel::populate_providers(const QVector<services::DbnProvider>& providers) {
    all_providers_ = providers;
    provider_list_->clear();

    for (const auto& p : providers) {
        // Code left-padded to 8 chars, then name
        const QString code_padded = p.code.leftJustified(8, ' ');
        auto* item = new QListWidgetItem(code_padded + "  " + p.name);
        item->setData(Qt::UserRole, p.code);
        item->setToolTip(p.name);
        provider_list_->addItem(item);
    }

    set_status(QString("%1 providers").arg(providers.size()));
}

void DBnomicsSelectionPanel::populate_datasets(const QVector<services::DbnDataset>& datasets,
                                               const services::DbnPagination& page, bool append) {
    if (!append) {
        dataset_list_->clear();
    }

    for (const auto& ds : datasets) {
        const QString display = ds.name.isEmpty() ? ds.code : ds.name;
        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, ds.code);
        item->setToolTip(ds.code);
        dataset_list_->addItem(item);
    }

    datasets_next_offset_ = page.offset + page.limit;
    if (page.has_more()) {
        dataset_load_more_btn_->show();
    } else {
        dataset_load_more_btn_->hide();
    }
}

void DBnomicsSelectionPanel::populate_series(const QVector<services::DbnSeriesInfo>& series,
                                             const services::DbnPagination& page, bool append) {
    if (!append) {
        series_list_->clear();
    }

    for (const auto& s : series) {
        const QString display = s.name.isEmpty() ? s.code : s.name;
        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, s.code);
        item->setToolTip(s.code + " \u2014 " + s.name);
        series_list_->addItem(item);
    }

    series_next_offset_ = page.offset + page.limit;
    if (page.has_more()) {
        series_load_more_btn_->show();
    } else {
        series_load_more_btn_->hide();
    }
}

void DBnomicsSelectionPanel::populate_search_results(const QVector<services::DbnSearchResult>& results,
                                                     const services::DbnPagination& page, bool append) {
    if (!append) {
        search_results_list_->clear();
    }

    for (const auto& r : results) {
        const QString display = "[" + r.provider_code + "] " + r.dataset_name;
        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, r.provider_code);
        item->setData(Qt::UserRole + 1, r.dataset_code);
        item->setToolTip(r.provider_code + "/" + r.dataset_code + " (" + QString::number(r.nb_series) + " series)");
        search_results_list_->addItem(item);
    }

    search_next_offset_ = page.offset + page.limit;

    if (!results.isEmpty()) {
        search_results_list_->show();
    }
    if (page.has_more()) {
        search_load_more_btn_->show();
    } else {
        search_load_more_btn_->hide();
    }
}

void DBnomicsSelectionPanel::set_loading(bool loading) {
    if (loading) {
        status_label_->setText("LOADING...");
    }
}

void DBnomicsSelectionPanel::set_status(const QString& message) {
    status_label_->setText(message);
}

// ── Loading state ─────────────────────────────────────────────────────────────

void DBnomicsSelectionPanel::tick_anim() {
    static const QString frames[] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"};
    const QString f = frames[anim_frame_ % 8];
    ++anim_frame_;
    if (prov_loading_ && prov_spin_)
        prov_spin_->setText(f + "  LOADING PROVIDERS...");
    if (ds_loading_ && ds_spin_)
        ds_spin_->setText(f + "  LOADING DATASETS...");
    if (series_loading_ && series_spin_)
        series_spin_->setText(f + "  LOADING SERIES...");
    if (search_loading_ && search_spin_)
        search_spin_->setText(f + "  SEARCHING...");
}

static void apply_loading(bool on, bool& flag, QLabel* spin, QWidget* content, QTimer* timer, int& frame) {
    flag = on;
    if (on) {
        if (content)
            content->hide();
        if (spin)
            spin->show();
        if (!timer->isActive()) {
            frame = 0;
            timer->start();
        }
    } else {
        if (spin)
            spin->hide();
        if (content)
            content->show();
        // timer stopped only when all flags are false — caller handles that
    }
}

void DBnomicsSelectionPanel::set_providers_loading(bool on) {
    apply_loading(on, prov_loading_, prov_spin_, prov_content_, anim_timer_, anim_frame_);
    if (!on && !ds_loading_ && !series_loading_ && !search_loading_)
        anim_timer_->stop();
}

void DBnomicsSelectionPanel::set_datasets_loading(bool on) {
    apply_loading(on, ds_loading_, ds_spin_, ds_content_, anim_timer_, anim_frame_);
    if (!on && !prov_loading_ && !series_loading_ && !search_loading_)
        anim_timer_->stop();
}

void DBnomicsSelectionPanel::set_series_loading(bool on) {
    apply_loading(on, series_loading_, series_spin_, series_content_, anim_timer_, anim_frame_);
    if (!on && !prov_loading_ && !ds_loading_ && !search_loading_)
        anim_timer_->stop();
}

void DBnomicsSelectionPanel::set_search_loading(bool on) {
    apply_loading(on, search_loading_, search_spin_, search_content_, anim_timer_, anim_frame_);
    if (!on && !prov_loading_ && !ds_loading_ && !series_loading_)
        anim_timer_->stop();
}

void DBnomicsSelectionPanel::update_slot_series(int slot_index, const QVector<services::DbnDataPoint>& series) {
    QLayoutItem* layout_item = slots_layout_->itemAt(slot_index);
    if (!layout_item || !layout_item->widget())
        return;

    QWidget* slot_widget = layout_item->widget();
    auto* slot_layout = qobject_cast<QVBoxLayout*>(slot_widget->layout());
    if (!slot_layout)
        return;

    // Remove all items below index 2 (header row at 0, add button at 1)
    while (slot_layout->count() > 2) {
        QLayoutItem* old_item = slot_layout->takeAt(2);
        if (old_item) {
            if (old_item->widget()) {
                old_item->widget()->deleteLater();
            }
            delete old_item;
        }
    }

    // Dot colors for series rows
    static const QStringList dot_colors = {"#ea580c", "#d97706", "#16a34a", "#2563eb",
                                           "#0891b2", "#9333ea", "#dc2626", "#ca8a04"};

    for (int i = 0; i < series.size(); ++i) {
        const auto& dp = series[i];

        auto* row = new QWidget();
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->setSpacing(4);

        // Colored dot
        const QString dot_color = dot_colors[i % dot_colors.size()];
        auto* dot = new QLabel("\u25CF");
        dot->setStyleSheet(QString("color: %1; font-size: 8px;").arg(dot_color));
        dot->setFixedWidth(12);
        row_layout->addWidget(dot);

        // Series name truncated to 22 chars
        QString name = dp.series_name;
        if (name.length() > 22) {
            name = name.left(19) + "...";
        }
        auto* name_label = new QLabel(name);
        name_label->setStyleSheet(QString("color: %1; font-size: 10px; "
                                          "font-family: 'Consolas','Courier New',monospace;")
                                      .arg(col::TEXT_SECONDARY));
        name_label->setToolTip(dp.series_name);
        row_layout->addWidget(name_label, 1);

        // Remove button
        const QString series_id = dp.series_id;
        auto* remove_btn = new QPushButton("\u00D7");
        remove_btn->setFixedSize(14, 14);
        remove_btn->setStyleSheet(
            QString("QPushButton { background: transparent; color: %1; border: none; font-size: 10px; }"
                    "QPushButton:hover { color: %2; }")
                .arg(col::TEXT_TERTIARY)
                .arg(col::NEGATIVE));
        connect(remove_btn, &QPushButton::clicked, this,
                [this, slot_index, series_id]() { emit remove_from_slot_clicked(slot_index, series_id); });
        row_layout->addWidget(remove_btn);

        slot_layout->addWidget(row);
    }
}

} // namespace fincept::screens
