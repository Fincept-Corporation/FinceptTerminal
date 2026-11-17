#include "screens/markets/MarketPanelEditor.h"

#include "core/instrument/VenueSymbols.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsSymbolEntry.h"
#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidgetItem>
#include <QMoveEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {




static QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;"
                   "padding:4px 8px;selection-background-color:%4;}"
                   "QLineEdit:focus{border-color:%5;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER(),
             ui::colors::AMBER());
}

static QString list_ss() {
    return QString("QListWidget{background:%1;border:1px solid %2;outline:none;}"
                   "QListWidget::item{padding:5px 10px;border-bottom:1px solid %3;color:%4;}"
                   "QListWidget::item:hover{background:%5;color:%6;}"
                   "QListWidget::item:selected{background:%5;color:%6;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BG_HOVER(), ui::colors::AMBER());
}

static QString section_lbl_ss() {
    return QString("color:%1;font-weight:700;letter-spacing:1px;background:transparent;font-size:" + fincept::ui::fonts::px(-3) + "px;")
        .arg(ui::colors::TEXT_SECONDARY());
}

MarketPanelEditor::MarketPanelEditor(const MarketPanelConfig& config, QWidget* parent)
    : QDialog(parent), config_(config) {
    build_ui();
}

void MarketPanelEditor::build_ui() {
    setWindowTitle(config_.id.isEmpty() ? tr("New Panel") : tr("Edit Panel — %1").arg(config_.title));
    setModal(true);
    setMinimumSize(440, 520);
    setStyleSheet(QString("QDialog{background:%1;color:%2;}").arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(10);

    // Title
    title_lbl_ = new QLabel(tr("PANEL TITLE"));
    title_lbl_->setStyleSheet(section_lbl_ss());
    root->addWidget(title_lbl_);

    title_edit_ = new QLineEdit(config_.title);
    title_edit_->setPlaceholderText(tr("e.g. My Tech Stocks"));
    title_edit_->setStyleSheet(input_ss());
    connect(title_edit_, &QLineEdit::textChanged, this, [this](const QString&) {
        if (error_lbl_)
            error_lbl_->setVisible(false);
    });
    root->addWidget(title_edit_);

    auto make_sep = [&]() {
        auto* s = new QFrame;
        s->setFixedHeight(1);
        s->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
        return s;
    };
    root->addWidget(make_sep());

    // Ticker list header
    auto* tickers_hdr = new QWidget;
    auto* tickers_hl = new QHBoxLayout(tickers_hdr);
    tickers_hl->setContentsMargins(0, 0, 0, 0);
    tickers_lbl_ = new QLabel(tr("TICKERS"));
    tickers_lbl_->setStyleSheet(section_lbl_ss());
    tickers_hl->addWidget(tickers_lbl_);
    tickers_hl->addStretch();
    remove_btn_ = new QPushButton(tr("✕ REMOVE"));
    remove_btn_->setMinimumHeight(20);
    remove_btn_->setCursor(Qt::PointingHandCursor);
    remove_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%3;border:1px solid %1;"
                                       "padding:0 8px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:bold;}"
                                       "QPushButton:hover{color:%2;border-color:%2;}")
                                   .arg(ui::colors::BORDER_MED(), ui::colors::DANGER(), ui::colors::TEXT_SECONDARY()));
    connect(remove_btn_, &QPushButton::clicked, this, &MarketPanelEditor::on_remove_selected);
    tickers_hl->addWidget(remove_btn_);
    root->addWidget(tickers_hdr);

    ticker_list_ = new QListWidget;
    ticker_list_->setStyleSheet(list_ss());
    ticker_list_->setFixedHeight(150);
    connect(ticker_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { on_remove_selected(); });
    root->addWidget(ticker_list_);

    root->addWidget(make_sep());

    // Search / add
    search_lbl_ = new QLabel(tr("ADD TICKERS  ·  ticker, name or ISIN; several separated by commas"));
    search_lbl_->setStyleSheet(section_lbl_ss());
    root->addWidget(search_lbl_);

    // The shared symbol box (multi entry): what is added is always a resolved
    // instrument's market-data symbol, never typed text.
    MarketsSymbolEntryOptions entry_opts;
    entry_opts.multi = true;
    entry_opts.compact = false;
    entry_opts.clear_on_pick = true;
    entry_opts.recent_key = QStringLiteral("markets");
    entry_opts.placeholder = tr("Search symbol or name: AAPL, Reliance, Bitcoin ...");
    entry_ = markets_make_symbol_entry(
        this, entry_opts,
        [this](const QVector<MarketsSymbolPick>& picks) {
            for (const MarketsSymbolPick& p : picks) {
                markets_learn(p);
                on_add_symbol(p.symbol);
            }
        },
        [this](const QStringList&, const QString& message) { show_validation_error(message); });
    root->addWidget(entry_);

    // Dialog buttons
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 16px;font-weight:bold;}"
                "QPushButton:hover{border-color:%4;color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::AMBER()));
    connect(btns, &QDialogButtonBox::accepted, this, [this]() {
        const QString title = title_edit_->text().trimmed();
        // MarketPanelStore::load() drops any config with an empty title, so a
        // panel saved without one vanished at the next restart with no warning.
        // Block it at the source instead.
        if (title.isEmpty()) {
            show_validation_error(tr("Enter a panel title."));
            title_edit_->setFocus();
            return;
        }
        if (ticker_list_->count() == 0) {
            show_validation_error(tr("Add at least one ticker."));
            markets_symbol_entry_focus(entry_);
            return;
        }
        config_.title = title;
        config_.symbols.clear();
        for (int i = 0; i < ticker_list_->count(); ++i)
            config_.symbols << ticker_list_->item(i)->data(Qt::UserRole).toString(); // the stored symbol
        if (config_.id.isEmpty())
            config_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        accept();
    });
    connect(btns, &QDialogButtonBox::rejected, this, [this]() {

        reject();
    });
    // Inline validation message (hidden until something is wrong) — a modal
    // QMessageBox for "you forgot a title" is heavier than the problem.
    error_lbl_ = new QLabel;
    error_lbl_->setWordWrap(true);
    error_lbl_->setVisible(false);
    error_lbl_->setStyleSheet(
        QString("color:%1;background:transparent;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:bold;").arg(ui::colors::DANGER()));
    root->addWidget(error_lbl_);

    root->addWidget(btns);

    // ── Accessibility + keyboard ──
    title_edit_->setAccessibleName(tr("Panel title"));
    entry_->setAccessibleName(tr("Search for a ticker to add"));
    ticker_list_->setAccessibleName(tr("Tickers in this panel"));
    remove_btn_->setAccessibleName(tr("Remove the selected ticker"));
    setTabOrder(title_edit_, ticker_list_);
    setTabOrder(ticker_list_, remove_btn_);
    setTabOrder(remove_btn_, entry_);
    setTabOrder(entry_, btns);
    title_edit_->setFocus();

    refresh_ticker_list();
}

void MarketPanelEditor::show_validation_error(const QString& message) {
    if (!error_lbl_)
        return;
    error_lbl_->setText(message);
    error_lbl_->setVisible(true);
}

void MarketPanelEditor::refresh_ticker_list() {
    ticker_list_->clear();
    for (const auto& s : config_.symbols) {
        // What the screen shows ("USD/JPY", "NIFTY 50"), with the stored market-data
        // symbol beside it when the two differ; the item keeps the stored one.
        const QString display = core::instrument::display_symbol(s);
        auto* item = new QListWidgetItem(display.isEmpty() || display == s ? s
                                                                           : QStringLiteral("%1   %2").arg(display, s));
        item->setData(Qt::UserRole, s);
        ticker_list_->addItem(item);
    }
}

void MarketPanelEditor::set_other_symbols(const QStringList& symbols) {
    other_symbols_ = symbols;
}

void MarketPanelEditor::on_add_symbol(const QString& symbol) {
    // The canonical market-data key (USDJPY=X -> JPY=X, brk.b -> BRK-B, 700.HK -> 0700.HK);
    // text canonical_symbol cannot place is kept as typed, upper-cased.
    const QString sym = markets_logic::canonical_or_kept(symbol);
    if (sym.isEmpty() || config_.symbols.contains(sym))
        return;
    if (config_.symbols.size() >= markets_logic::kMaxSymbolsPerPanel) {
        show_validation_error(tr("A panel can hold up to %1 symbols.").arg(markets_logic::kMaxSymbolsPerPanel));
        return;
    }
    QSet<QString> all(other_symbols_.cbegin(), other_symbols_.cend());
    for (const QString& s : config_.symbols)
        all.insert(s);
    if (!all.contains(sym) && all.size() >= markets_logic::kMaxSymbolsOnScreen) {
        show_validation_error(tr("This layout already tracks %1 symbols. Remove some before adding more.")
                                  .arg(markets_logic::kMaxSymbolsOnScreen));
        return;
    }
    config_.symbols << sym;
    if (error_lbl_)
        error_lbl_->setVisible(false);
    refresh_ticker_list();
    markets_symbol_entry_focus(entry_); // the box cleared itself on the pick
}

void MarketPanelEditor::on_remove_selected() {
    auto* item = ticker_list_->currentItem();
    if (!item)
        return;
    config_.symbols.removeAll(item->data(Qt::UserRole).toString());
    refresh_ticker_list();
}

MarketPanelConfig MarketPanelEditor::result_config() const {
    return config_;
}

// ---------------------------------------------------------------------------
// i18n — live language switch
// ---------------------------------------------------------------------------

void MarketPanelEditor::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void MarketPanelEditor::retranslateUi() {
    setWindowTitle(config_.id.isEmpty() ? tr("New Panel") : tr("Edit Panel — %1").arg(config_.title));
    if (title_lbl_)
        title_lbl_->setText(tr("PANEL TITLE"));
    if (title_edit_)
        title_edit_->setPlaceholderText(tr("e.g. My Tech Stocks"));
    if (tickers_lbl_)
        tickers_lbl_->setText(tr("TICKERS"));
    if (remove_btn_)
        remove_btn_->setText(tr("✕ REMOVE"));
    if (search_lbl_)
        search_lbl_->setText(tr("ADD TICKERS  ·  ticker, name or ISIN; several separated by commas"));
    if (entry_)
        markets_symbol_entry_set_placeholder(entry_, tr("Search symbol or name: AAPL, Reliance, Bitcoin ..."));
}

} // namespace fincept::screens
