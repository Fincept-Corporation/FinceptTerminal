// src/screens/algo_trading/StrategyListPanel.cpp
#include "screens/algo_trading/StrategyListPanel.h"
#include "screens/algo_trading/AlgoDeployDialog.h"

#include "algo_engine/AlgoEngine.h"
#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingService.h"
#include "ui/theme/Theme.h"

#include <algorithm>

#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::services::algo;

// ── Constructor ──────────────────────────────────────────────────────────────

StrategyListPanel::StrategyListPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
    LOG_INFO("AlgoTrading", "StrategyListPanel constructed");
}

// ── Service connections ──────────────────────────────────────────────────────

void StrategyListPanel::connect_service() {
    auto& svc = AlgoTradingService::instance();
    connect(&svc, &AlgoTradingService::strategies_loaded, this, &StrategyListPanel::on_strategies_loaded);
    connect(&svc, &AlgoTradingService::strategy_deleted, this, [&svc](const QString&) { svc.list_strategies(); });
    connect(&svc, &AlgoTradingService::error_occurred, this, &StrategyListPanel::on_error);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void StrategyListPanel::build_ui() {
    using namespace fincept::ui;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top bar ──────────────────────────────────────────────────────────────
    auto* top_bar = new QWidget(this);
    top_bar->setFixedHeight(44);
    top_bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                               .arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* top_hl = new QHBoxLayout(top_bar);
    top_hl->setContentsMargins(12, 0, 12, 0);
    top_hl->setSpacing(8);

    search_edit_ = new QLineEdit(top_bar);
    search_edit_->setPlaceholderText(tr("Search strategies..."));
    search_edit_->setFixedHeight(28);
    search_edit_->setStyleSheet(
        QString("QLineEdit { background:%1; border:1px solid %2; color:%3;"
                " padding:4px 8px; font-size:%4px; font-family:%5; }"
                "QLineEdit:focus { border-color:%6; }")
            .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::TEXT_PRIMARY())
            .arg(fonts::SMALL)
            .arg(fonts::DATA_FAMILY(), colors::BORDER_BRIGHT()));
    top_hl->addWidget(search_edit_, 2);

    // Category filter
    cat_combo_ = new QComboBox(top_bar);
    cat_combo_->setFixedHeight(28);
    cat_combo_->setFixedWidth(150);
    cat_combo_->addItem(tr("All Categories"));
    const QString combo_style =
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                " padding:2px 6px; font-size:%4px; font-family:%5; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                " selection-background-color:%6; font-family:%5; }")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
            .arg(fonts::TINY)
            .arg(fonts::DATA_FAMILY(), colors::BG_HOVER());
    cat_combo_->setStyleSheet(combo_style);
    top_hl->addWidget(cat_combo_);

    // Sort
    sort_caption_ = new QLabel(tr("SORT:"), top_bar);
    sort_caption_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                         " background:transparent; border:none;")
                                     .arg(colors::TEXT_TERTIARY())
                                     .arg(fonts::TINY)
                                     .arg(fonts::DATA_FAMILY()));
    top_hl->addWidget(sort_caption_);

    sort_combo_ = new QComboBox(top_bar);
    sort_combo_->setFixedHeight(28);
    sort_combo_->setFixedWidth(120);
    sort_combo_->addItems({tr("Name A→Z"), tr("Name Z→A"), tr("Category")});
    sort_combo_->setStyleSheet(combo_style);
    top_hl->addWidget(sort_combo_);

    count_label_ = new QLabel(tr("%1 strategies").arg(0), top_bar);
    count_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                        " background:transparent; border:none;")
                                    .arg(colors::TEXT_SECONDARY())
                                    .arg(fonts::TINY)
                                    .arg(fonts::DATA_FAMILY()));
    top_hl->addWidget(count_label_);

    root->addWidget(top_bar);

    // ── Table ────────────────────────────────────────────────────────────────
    table_ = new QTableWidget(this);
    table_->setColumnCount(8); // #, NAME, CATEGORY, ID, EDIT, BACKTEST, DEPLOY, DELETE
    table_->setHorizontalHeaderLabels(
        {tr("#"), tr("STRATEGY NAME"), tr("CATEGORY"), tr("ID"), QString(), QString(), QString(), QString()});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setSortingEnabled(false);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    table_->setColumnWidth(0, 40);
    table_->setColumnWidth(2, 150);
    table_->setColumnWidth(3, 100);
    table_->setColumnWidth(4, 56);
    table_->setColumnWidth(5, 76);
    table_->setColumnWidth(6, 72);
    table_->setColumnWidth(7, 66);
    table_->verticalHeader()->setDefaultSectionSize(30);
    table_->setStyleSheet(
        QString("QTableWidget { background:%1; alternate-background-color:%2;"
                " color:%3; border:none; font-size:%4px; font-family:%5; gridline-color:%6; }"
                "QTableWidget::item { padding:2px 8px; border:none; }"
                "QTableWidget::item:selected { background:%7; color:%3; }"
                "QHeaderView::section { background:%8; color:%9; font-size:%4px;"
                " font-weight:700; font-family:%5; border:none;"
                " border-bottom:1px solid %6; padding:4px 8px; }")
            .arg(colors::BG_BASE(), colors::BG_SURFACE(), colors::TEXT_PRIMARY())
            .arg(fonts::SMALL)
            .arg(fonts::DATA_FAMILY(), colors::BORDER_DIM(), colors::BG_HOVER(),
                 colors::BG_RAISED(), colors::TEXT_TERTIARY()));
    root->addWidget(table_, 1);

    // ── Pagination bar ───────────────────────────────────────────────────────
    auto* page_bar = new QWidget(this);
    page_bar->setFixedHeight(36);
    page_bar->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                                .arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* page_hl = new QHBoxLayout(page_bar);
    page_hl->setContentsMargins(12, 0, 12, 0);
    page_hl->setSpacing(8);

    const QString btn_style =
        QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                " font-size:%4px; font-family:%5; padding:2px 12px; }"
                "QPushButton:hover { border-color:%6; color:%6; }"
                "QPushButton:disabled { color:%3; border-color:%3; }")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
            .arg(fonts::TINY)
            .arg(fonts::DATA_FAMILY(), colors::CYAN());

    prev_btn_ = new QPushButton(tr("◀ PREV"), page_bar);
    prev_btn_->setFixedHeight(24);
    prev_btn_->setCursor(Qt::PointingHandCursor);
    prev_btn_->setStyleSheet(btn_style);
    page_hl->addWidget(prev_btn_);

    page_label_ = new QLabel(tr("Page %1 of %2").arg(1).arg(1), page_bar);
    page_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;"
                                       " background:transparent; border:none;")
                                   .arg(colors::TEXT_SECONDARY())
                                   .arg(fonts::TINY)
                                   .arg(fonts::DATA_FAMILY()));
    page_label_->setAlignment(Qt::AlignCenter);
    page_hl->addWidget(page_label_, 1);

    next_btn_ = new QPushButton(tr("NEXT ▶"), page_bar);
    next_btn_->setFixedHeight(24);
    next_btn_->setCursor(Qt::PointingHandCursor);
    next_btn_->setStyleSheet(btn_style);
    page_hl->addWidget(next_btn_);

    root->addWidget(page_bar);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(search_edit_, &QLineEdit::textChanged, this, &StrategyListPanel::on_filter_changed);
    connect(cat_combo_,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { on_filter_changed(search_edit_->text()); });
    connect(sort_combo_,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StrategyListPanel::on_sort_changed);
    connect(prev_btn_, &QPushButton::clicked, this, [this]() { go_to_page(current_page_ - 1); });
    connect(next_btn_, &QPushButton::clicked, this, [this]() { go_to_page(current_page_ + 1); });
}

// ── Render page ──────────────────────────────────────────────────────────────

void StrategyListPanel::render_page() {
    using namespace fincept::ui;

    table_->setSortingEnabled(false);
    table_->setRowCount(0);

    const int total    = filtered_.size();
    const int start    = current_page_ * kPageSize;
    const int end      = qMin(start + kPageSize, total);
    const int page_num = total > 0 ? current_page_ + 1 : 0;
    const int total_pages = (total + kPageSize - 1) / qMax(kPageSize, 1);

    table_->setRowCount(end - start);

    for (int i = start; i < end; ++i) {
        const auto& s   = filtered_[i];
        const int   row = i - start;

        // Col 0: row number
        auto* num_item = new QTableWidgetItem(QString::number(i + 1));
        num_item->setTextAlignment(Qt::AlignCenter);
        num_item->setForeground(QColor(colors::TEXT_TERTIARY()));
        table_->setItem(row, 0, num_item);

        // Col 1: name
        auto* name_item = new QTableWidgetItem(s.name);
        name_item->setForeground(QColor(colors::AMBER()));
        table_->setItem(row, 1, name_item);

        // Col 2: category (description field)
        auto* cat_item = new QTableWidgetItem(s.description);
        cat_item->setForeground(QColor(colors::CYAN()));
        cat_item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 2, cat_item);

        // Col 3: ID
        auto* id_item = new QTableWidgetItem(s.id);
        id_item->setForeground(QColor(colors::TEXT_TERTIARY()));
        table_->setItem(row, 3, id_item);

        auto make_btn = [&](const QString& text, const QString& color, const QString& hover,
                            int col, auto handler) {
            auto* btn = new QPushButton(text, table_);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(22);
            btn->setStyleSheet(
                QString("QPushButton { background: transparent; color: %1; border: 1px solid %1;"
                        " font-size: %2px; font-weight: 700; font-family: %3; padding: 1px 8px; }"
                        "QPushButton:hover { background: %4; }")
                    .arg(color)
                    .arg(fonts::TINY)
                    .arg(fonts::DATA_FAMILY())
                    .arg(hover));
            connect(btn, &QPushButton::clicked, this, handler);
            table_->setCellWidget(row, col, btn);
        };

        // Col 4: EDIT (Builder). 5: BACKTEST. 6: DEPLOY. 7: DELETE.
        make_btn(tr("EDIT"), colors::CYAN(), "rgba(34,211,238,0.1)", 4, [this, row]() { on_edit_clicked(row); });
        make_btn(tr("BACKTEST"), colors::POSITIVE(), "rgba(22,163,74,0.1)", 5, [this, row]() { on_backtest_clicked(row); });
        make_btn(tr("DEPLOY"), colors::AMBER(), "rgba(217,119,6,0.1)", 6, [this, row]() { on_deploy_clicked(row); });
        make_btn(tr("DELETE"), colors::NEGATIVE(), "rgba(220,38,38,0.1)", 7, [this, row]() { on_delete_clicked(row); });
    }

    update_pagination_controls();

    page_label_->setText(total > 0
        ? tr("Page %1 of %2  ·  %3 strategies").arg(page_num).arg(total_pages).arg(total)
        : tr("No strategies"));

    count_label_->setText(
        tr("%1 of %2").arg(total).arg(strategies_.size()));
}

void StrategyListPanel::update_pagination_controls() {
    const int total_pages = (filtered_.size() + kPageSize - 1) / qMax(kPageSize, 1);
    prev_btn_->setEnabled(current_page_ > 0);
    next_btn_->setEnabled(current_page_ < total_pages - 1);
}

void StrategyListPanel::go_to_page(int page) {
    const int total_pages = (filtered_.size() + kPageSize - 1) / qMax(kPageSize, 1);
    current_page_ = qBound(0, page, qMax(0, total_pages - 1));
    render_page();
}

// ── Filter + sort ─────────────────────────────────────────────────────────────

void StrategyListPanel::on_filter_changed(const QString&) {
    const QString text = search_edit_->text().trimmed().toLower();
    const QString cat  = cat_combo_->currentIndex() == 0
                           ? QString()
                           : cat_combo_->currentText();

    filtered_.clear();
    filtered_.reserve(strategies_.size());
    for (const auto& s : strategies_) {
        if (!text.isEmpty() && !s.name.toLower().contains(text) &&
            !s.description.toLower().contains(text))
            continue;
        if (!cat.isEmpty() && s.description != cat)
            continue;
        filtered_.append(s);
    }

    current_page_ = 0;
    render_page();
}

void StrategyListPanel::on_sort_changed(int index) {
    if (index == 0) { // Name A→Z
        std::sort(filtered_.begin(), filtered_.end(),
                  [](const AlgoStrategy& a, const AlgoStrategy& b) { return a.name < b.name; });
    } else if (index == 1) { // Name Z→A
        std::sort(filtered_.begin(), filtered_.end(),
                  [](const AlgoStrategy& a, const AlgoStrategy& b) { return a.name > b.name; });
    } else if (index == 2) { // Category
        std::sort(filtered_.begin(), filtered_.end(),
                  [](const AlgoStrategy& a, const AlgoStrategy& b) {
                      return a.description < b.description || (a.description == b.description && a.name < b.name);
                  });
    }
    current_page_ = 0;
    render_page();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void StrategyListPanel::on_strategies_loaded(QVector<AlgoStrategy> strategies) {
    strategies_ = std::move(strategies);

    // Populate category combo from unique categories
    const QString prev_cat = cat_combo_->currentIndex() > 0 ? cat_combo_->currentText() : QString();
    cat_combo_->blockSignals(true);
    cat_combo_->clear();
    cat_combo_->addItem(tr("All Categories"));
    QStringList cats;
    for (const auto& s : strategies_)
        if (!cats.contains(s.description))
            cats.append(s.description);
    cats.sort();
    for (const auto& c : cats)
        cat_combo_->addItem(c);
    if (!prev_cat.isEmpty()) {
        int idx = cat_combo_->findText(prev_cat);
        if (idx >= 0) cat_combo_->setCurrentIndex(idx);
    }
    cat_combo_->blockSignals(false);

    // Reset filtered to full list, sort by name
    filtered_ = strategies_;
    std::sort(filtered_.begin(), filtered_.end(),
              [](const AlgoStrategy& a, const AlgoStrategy& b) { return a.name < b.name; });
    current_page_ = 0;
    render_page();

    LOG_INFO("AlgoTrading", QString("Loaded %1 strategies").arg(strategies_.size()));
}

void StrategyListPanel::on_error(const QString& context, const QString& msg) {
    LOG_ERROR("AlgoTrading", QString("StrategyList error [%1]: %2").arg(context, msg));
}

void StrategyListPanel::on_edit_clicked(int row) {
    const int abs_index = current_page_ * kPageSize + row;
    if (abs_index < 0 || abs_index >= filtered_.size())
        return;
    emit edit_requested(filtered_[abs_index]); // AlgoTradingScreen routes to the Builder
}

void StrategyListPanel::on_backtest_clicked(int row) {
    const int abs_index = current_page_ * kPageSize + row;
    if (abs_index < 0 || abs_index >= filtered_.size())
        return;
    const auto& strategy = filtered_[abs_index];

    // Prompt for symbol + date range before running.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Backtest: %1").arg(strategy.name));
    auto* form = new QFormLayout(&dlg);

    auto* symbol = new QLineEdit(QStringLiteral("RELIANCE"), &dlg);
    symbol->setPlaceholderText(tr("e.g. RELIANCE, AAPL"));
    const QDate today = QDate::currentDate();
    // Default range sized to the strategy's timeframe (daily needs years for
    // SMA200; intraday must stay under Yahoo's history cap).
    const int lookback = services::algo::algo_default_lookback_days(strategy.timeframe);
    auto* start = new QDateEdit(today.addDays(-lookback), &dlg);
    auto* end = new QDateEdit(today, &dlg);
    for (QDateEdit* d : {start, end}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        d->setMinimumDate(QDate(2000, 1, 1));
        d->setMaximumDate(today);
    }
    form->addRow(tr("Symbol"), symbol);
    form->addRow(tr("Start"), start);
    form->addRow(tr("End"), end);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Run Backtest"));
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;
    if (symbol->text().trimmed().isEmpty() || start->date() >= end->date())
        return;

    emit backtest_requested(strategy, symbol->text().trimmed(),
                            start->date().toString(QStringLiteral("yyyy-MM-dd")),
                            end->date().toString(QStringLiteral("yyyy-MM-dd")));
}

void StrategyListPanel::on_deploy_clicked(int row) {
    const int abs_index = current_page_ * kPageSize + row;
    if (abs_index < 0 || abs_index >= filtered_.size())
        return;

    const auto& strategy = filtered_[abs_index];
    AlgoDeployDialog dialog(strategy.id, strategy.name, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto deployment = dialog.result();
        fincept::algo::AlgoEngine::instance().start_deployment(deployment, strategy);
        LOG_INFO("AlgoTrading", QString("Deploy requested: %1 on %2").arg(strategy.name, deployment.symbol));
    }
}

void StrategyListPanel::on_delete_clicked(int row) {
    const int abs_index = current_page_ * kPageSize + row;
    if (abs_index < 0 || abs_index >= filtered_.size())
        return;

    const auto& strategy = filtered_[abs_index];
    auto answer = QMessageBox::question(this, tr("Delete Strategy"),
                                        tr("Delete \"%1\"?").arg(strategy.name),
                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        AlgoTradingService::instance().delete_strategy(strategy.id);
        LOG_INFO("AlgoTrading", QString("Delete requested: %1").arg(strategy.name));
    }
}

// ── Live language switch ──────────────────────────────────────────────────────

void StrategyListPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void StrategyListPanel::retranslateUi() {
    if (search_edit_)  search_edit_->setPlaceholderText(tr("Search strategies..."));
    if (sort_caption_) sort_caption_->setText(tr("SORT:"));

    // cat_combo_ item 0 is the fixed "All Categories" entry; remaining items are
    // category data names and stay as-is.
    if (cat_combo_ && cat_combo_->count() > 0)
        cat_combo_->setItemText(0, tr("All Categories"));

    // sort_combo_ — selection index drives logic, only the visible labels change.
    if (sort_combo_ && sort_combo_->count() >= 3) {
        sort_combo_->setItemText(0, tr("Name A→Z"));
        sort_combo_->setItemText(1, tr("Name Z→A"));
        sort_combo_->setItemText(2, tr("Category"));
    }

    if (prev_btn_) prev_btn_->setText(tr("◀ PREV"));
    if (next_btn_) next_btn_->setText(tr("NEXT ▶"));

    if (table_) {
        table_->setHorizontalHeaderLabels(
            {tr("#"), tr("STRATEGY NAME"), tr("CATEGORY"), tr("ID"), QString(), QString(), QString(), QString()});
    }

    // render_page() re-applies per-row button labels, page_label_, and count_label_.
    render_page();
}

} // namespace fincept::screens
