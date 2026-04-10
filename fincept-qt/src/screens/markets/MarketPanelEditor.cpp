#include "screens/markets/MarketPanelEditor.h"

#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

static constexpr int kDebounceMs = 300;
static constexpr int kMaxResults = 15;

static QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;"
                   "padding:4px 8px;selection-background-color:%4;}"
                   "QLineEdit:focus{border-color:%5;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
             ui::colors::BG_HOVER(), ui::colors::AMBER());
}

static QString list_ss() {
    return QString("QListWidget{background:%1;border:1px solid %2;outline:none;}"
                   "QListWidget::item{padding:5px 10px;border-bottom:1px solid %3;color:%4;}"
                   "QListWidget::item:hover{background:%5;color:%6;}"
                   "QListWidget::item:selected{background:%5;color:%6;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::BORDER_DIM(),
             ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER(), ui::colors::AMBER());
}

static QString section_lbl_ss() {
    return QString("color:%1;font-weight:700;letter-spacing:1px;background:transparent;font-size:11px;")
        .arg(ui::colors::TEXT_TERTIARY());
}

MarketPanelEditor::MarketPanelEditor(const MarketPanelConfig& config, QWidget* parent)
    : QDialog(parent), config_(config) {
    build_ui();
}

void MarketPanelEditor::build_ui() {
    setWindowTitle(config_.id.isEmpty() ? "New Panel" : "Edit Panel — " + config_.title);
    setModal(true);
    setMinimumSize(440, 520);
    setStyleSheet(QString("QDialog{background:%1;color:%2;}")
                      .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(10);

    // Title
    auto* title_lbl = new QLabel("PANEL TITLE");
    title_lbl->setStyleSheet(section_lbl_ss());
    root->addWidget(title_lbl);

    title_edit_ = new QLineEdit(config_.title);
    title_edit_->setPlaceholderText("e.g. My Tech Stocks");
    title_edit_->setStyleSheet(input_ss());
    root->addWidget(title_edit_);

    auto make_sep = [&]() {
        auto* s = new QFrame; s->setFixedHeight(1);
        s->setStyleSheet(QString("background:%1;border:none;").arg(ui::colors::BORDER_DIM()));
        return s;
    };
    root->addWidget(make_sep());

    // Ticker list header
    auto* tickers_hdr = new QWidget;
    auto* tickers_hl  = new QHBoxLayout(tickers_hdr);
    tickers_hl->setContentsMargins(0, 0, 0, 0);
    auto* tickers_lbl = new QLabel("TICKERS");
    tickers_lbl->setStyleSheet(section_lbl_ss());
    tickers_hl->addWidget(tickers_lbl);
    tickers_hl->addStretch();
    auto* remove_btn = new QPushButton("✕ REMOVE");
    remove_btn->setFixedHeight(20);
    remove_btn->setCursor(Qt::PointingHandCursor);
    remove_btn->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                "padding:0 8px;font-size:11px;font-weight:bold;}"
                "QPushButton:hover{color:%2;border-color:%2;}")
            .arg(ui::colors::BORDER_MED(), ui::colors::NEGATIVE()));
    connect(remove_btn, &QPushButton::clicked, this, &MarketPanelEditor::on_remove_selected);
    tickers_hl->addWidget(remove_btn);
    root->addWidget(tickers_hdr);

    ticker_list_ = new QListWidget;
    ticker_list_->setStyleSheet(list_ss());
    ticker_list_->setFixedHeight(150);
    connect(ticker_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        on_remove_selected();
    });
    root->addWidget(ticker_list_);

    root->addWidget(make_sep());

    // Search / add
    auto* search_lbl = new QLabel("ADD TICKER  ·  type to search, click or Enter to add");
    search_lbl->setStyleSheet(section_lbl_ss());
    root->addWidget(search_lbl);

    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search symbol or name: AAPL, Reliance, Bitcoin ...");
    search_edit_->setStyleSheet(input_ss());
    search_edit_->installEventFilter(this);
    root->addWidget(search_edit_);

    // Debounce timer
    search_debounce_ = new QTimer(this);
    search_debounce_->setSingleShot(true);
    search_debounce_->setInterval(kDebounceMs);
    connect(search_debounce_, &QTimer::timeout, this, [this]() {
        fire_search(pending_query_);
    });

    connect(search_edit_, &QLineEdit::textChanged, this, &MarketPanelEditor::on_search_text_changed);
    connect(search_edit_, &QLineEdit::returnPressed, this, [this]() {
        if (dropdown_->isVisible() && dropdown_->currentItem())
            on_add_symbol(dropdown_->currentItem()->data(Qt::UserRole).toString());
        else if (!search_edit_->text().trimmed().isEmpty())
            on_add_symbol(search_edit_->text().trimmed().toUpper());
    });

    // Floating dropdown — parented to dialog, floats above layout
    dropdown_ = new QListWidget(this);
    dropdown_->setStyleSheet(list_ss());
    dropdown_->setFixedHeight(160);
    dropdown_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dropdown_->hide();
    connect(dropdown_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        on_add_symbol(item->data(Qt::UserRole).toString());
    });

    // Dialog buttons
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 16px;font-weight:bold;}"
                "QPushButton:hover{border-color:%4;color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::BORDER_MED(), ui::colors::AMBER()));
    connect(btns, &QDialogButtonBox::accepted, this, [this]() {
        config_.title = title_edit_->text().trimmed();
        config_.symbols.clear();
        for (int i = 0; i < ticker_list_->count(); ++i)
            config_.symbols << ticker_list_->item(i)->text();
        if (config_.id.isEmpty())
            config_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        hide_dropdown();
        accept();
    });
    connect(btns, &QDialogButtonBox::rejected, this, [this]() {
        hide_dropdown();
        reject();
    });
    root->addWidget(btns);

    refresh_ticker_list();
}

void MarketPanelEditor::reposition_dropdown() {
    if (!search_edit_ || !dropdown_) return;
    QPoint pos = search_edit_->mapTo(this, QPoint(0, search_edit_->height()));
    dropdown_->setGeometry(pos.x(), pos.y(), search_edit_->width(), 160);
    dropdown_->raise();
}

void MarketPanelEditor::hide_dropdown() {
    search_debounce_->stop();
    if (dropdown_) dropdown_->hide();
}

void MarketPanelEditor::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    reposition_dropdown();
}

void MarketPanelEditor::moveEvent(QMoveEvent* event) {
    QDialog::moveEvent(event);
    reposition_dropdown();
}

bool MarketPanelEditor::eventFilter(QObject* obj, QEvent* event) {
    if (obj == search_edit_) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Down && dropdown_->isVisible()) {
                dropdown_->setFocus();
                dropdown_->setCurrentRow(0);
                return true;
            }
            if (ke->key() == Qt::Key_Escape) {
                hide_dropdown();
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut) {
            // Delay hide so click on dropdown item registers first
            QTimer::singleShot(150, this, [this]() { hide_dropdown(); });
        }
    }
    return QDialog::eventFilter(obj, event);
}

void MarketPanelEditor::on_search_text_changed(const QString& text) {
    const QString q = text.trimmed();
    if (q.isEmpty()) {
        hide_dropdown();
        return;
    }
    pending_query_ = q;
    search_debounce_->start();
}

void MarketPanelEditor::fire_search(const QString& query) {
    if (query.isEmpty()) return;

    const QString url = QString("/market/search?q=%1&limit=%2").arg(query).arg(kMaxResults);

    QPointer<MarketPanelEditor> self = this;
    HttpClient::instance().get(url, [self, query](Result<QJsonDocument> result) {
        if (!self) return;
        if (self->pending_query_ != query) return; // stale response
        if (!result.is_ok()) return;

        const auto doc = result.value();
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            const auto obj = doc.object();
            if (obj.contains("results"))      arr = obj["results"].toArray();
            else if (obj.contains("data"))    arr = obj["data"].toArray();
        }
        self->on_search_results(arr);
    });
}

void MarketPanelEditor::on_search_results(const QJsonArray& results) {
    dropdown_->clear();

    for (const auto& v : results) {
        const auto obj    = v.toObject();
        const QString sym = obj["symbol"].toString();
        const QString name = obj["name"].toString();
        if (sym.isEmpty()) continue;

        const QString display = name.isEmpty() ? sym : QString("%1  ·  %2").arg(sym, name);
        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, sym); // store raw symbol for adding
        dropdown_->addItem(item);
    }

    if (dropdown_->count() > 0) {
        reposition_dropdown();
        dropdown_->show();
        dropdown_->raise();
    } else {
        hide_dropdown();
    }
}

void MarketPanelEditor::refresh_ticker_list() {
    ticker_list_->clear();
    for (const auto& s : config_.symbols) {
        auto* item = new QListWidgetItem(s);
        item->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        ticker_list_->addItem(item);
    }
}

void MarketPanelEditor::on_add_symbol(const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty() || config_.symbols.contains(sym)) return;
    config_.symbols << sym;
    refresh_ticker_list();
    search_edit_->clear();
    hide_dropdown();
    search_edit_->setFocus();
}

void MarketPanelEditor::on_remove_selected() {
    auto* item = ticker_list_->currentItem();
    if (!item) return;
    config_.symbols.removeAll(item->text());
    refresh_ticker_list();
}

MarketPanelConfig MarketPanelEditor::result_config() const {
    return config_;
}

} // namespace fincept::screens
