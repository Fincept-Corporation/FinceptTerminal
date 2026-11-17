#include "screens/dashboard/widgets/SymbolListDialog.h"

#include "services/markets/MarketSearchService.h"
#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

namespace {
constexpr int kSymDlgSymbolRole = Qt::UserRole + 1;
constexpr int kSymDlgNameRole = Qt::UserRole + 2;
} // namespace

SymbolListDialog::SymbolListDialog(const QString& title, const QVector<QuoteGridEntry>& current, DefaultsFn defaults,
                                   QWidget* parent, int max_count)
    : QDialog(parent), defaults_(std::move(defaults)), max_count_(std::max(1, max_count)) {
    setWindowTitle(title);
    setMinimumSize(520, max_count_ == 1 ? 360 : 520);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(6);

    auto* hint = new QLabel(max_count_ == 1 ? tr("Search by name or ticker, then pick a result.")
                                            : tr("Search by name or ticker and add results. Yahoo tickers such as "
                                                 "^GSPC, EURUSD=X or GC=F can be added as typed."),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1;font-size:%2px;").arg(ui::colors::TEXT_SECONDARY()).arg(ui::fonts::px(-2)));
    root->addWidget(hint);

    auto* search_row = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Search symbols…"));
    search_->setClearButtonEnabled(true);
    search_row->addWidget(search_, 1);
    auto* add_typed = new QPushButton(tr("Add as typed"), this);
    add_typed->setToolTip(tr("Use the text exactly as typed as a Yahoo Finance ticker"));
    search_row->addWidget(add_typed);
    root->addLayout(search_row);

    results_ = new QListWidget(this);
    results_->setMinimumHeight(110);
    root->addWidget(results_, max_count_ == 1 ? 1 : 0);

    status_ = new QLabel(this);
    status_->setStyleSheet(QString("color:%1;font-size:%2px;").arg(ui::colors::TEXT_TERTIARY()).arg(ui::fonts::px(-3)));
    root->addWidget(status_);

    list_ = new QTableWidget(0, 2, this);
    list_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("LABEL (editable)")});
    list_->horizontalHeader()->setStretchLastSection(true);
    list_->verticalHeader()->setVisible(false);
    list_->setSelectionBehavior(QAbstractItemView::SelectRows);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setColumnWidth(0, 150);
    list_->setVisible(max_count_ > 1);
    root->addWidget(list_, 1);

    auto* edit_row = new QHBoxLayout;
    auto* up = new QPushButton(tr("Up"), this);
    auto* down = new QPushButton(tr("Down"), this);
    auto* remove = new QPushButton(tr("Remove"), this);
    edit_row->addWidget(up);
    edit_row->addWidget(down);
    edit_row->addWidget(remove);
    edit_row->addStretch();
    QPushButton* reset = nullptr;
    if (defaults_) {
        reset = new QPushButton(tr("Region defaults"), this);
        reset->setToolTip(tr("Use the list for the dashboard's market region; it then follows region changes"));
        edit_row->addWidget(reset);
    }
    if (max_count_ > 1)
        root->addLayout(edit_row);
    else {
        for (auto* b : {up, down, remove})
            b->hide();
        if (reset)
            reset->hide();
    }

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, this, [this]() {
        // Single-symbol mode: the highlighted search result (if the user
        // searched) replaces the current symbol.
        if (max_count_ == 1) {
            if (auto* it = results_->currentItem())
                add_entry(it->data(kSymDlgSymbolRole).toString(), it->data(kSymDlgNameRole).toString());
            else if (list_->rowCount() == 0 && !search_->text().trimmed().isEmpty())
                add_entry(search_->text().trimmed().toUpper(), {});
        }
        if (list_->rowCount() > 0)
            accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(280);
    connect(debounce_, &QTimer::timeout, this, &SymbolListDialog::run_search);
    connect(search_, &QLineEdit::textChanged, this, [this]() { debounce_->start(); });
    connect(search_, &QLineEdit::returnPressed, this, [this]() {
        if (results_->count() > 0 && results_->currentItem()) {
            auto* it = results_->currentItem();
            add_entry(it->data(kSymDlgSymbolRole).toString(), it->data(kSymDlgNameRole).toString());
        }
    });
    connect(add_typed, &QPushButton::clicked, this, [this]() {
        const QString t = search_->text().trimmed().toUpper();
        if (!t.isEmpty())
            add_entry(t, {});
    });
    connect(results_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        add_entry(it->data(kSymDlgSymbolRole).toString(), it->data(kSymDlgNameRole).toString());
        if (max_count_ == 1)
            accept();
    });

    auto& svc = services::MarketSearchService::instance();
    connect(&svc, &services::MarketSearchService::results_ready, this,
            [this](const QString& request_id, const QString&, const QList<services::MarketSearchService::Item>& items) {
                if (request_id != pending_query_)
                    return;
                results_->clear();
                for (const auto& i : items) {
                    const QString sym = i.yf_symbol.isEmpty() ? i.symbol : i.yf_symbol;
                    QStringList bits{sym};
                    if (!i.name.isEmpty())
                        bits << i.name;
                    QStringList meta;
                    if (!i.exchange.isEmpty())
                        meta << i.exchange;
                    if (!i.type.isEmpty())
                        meta << i.type.toUpper();
                    QString text = bits.join(QStringLiteral(" — "));
                    if (!meta.isEmpty())
                        text += QStringLiteral("  ·  ") + meta.join(QStringLiteral(" · "));
                    auto* it = new QListWidgetItem(text, results_);
                    it->setData(kSymDlgSymbolRole, sym);
                    it->setData(kSymDlgNameRole, i.name);
                }
                if (results_->count() > 0)
                    results_->setCurrentRow(0);
                status_->setText(items.isEmpty() ? tr("No matches — you can still add the text as typed.")
                                                 : tr("%n result(s) — double-click to add", "", int(items.size())));
            });
    connect(&svc, &services::MarketSearchService::search_failed, this,
            [this](const QString& request_id, const QString&, const QString& reason) {
                if (request_id == pending_query_)
                    status_->setText(tr("Search unavailable: %1").arg(reason));
            });

    connect(up, &QPushButton::clicked, this, [this]() { move_selected(-1); });
    connect(down, &QPushButton::clicked, this, [this]() { move_selected(+1); });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int r = list_->currentRow();
        if (r >= 0) {
            list_->removeRow(r);
            follows_defaults_ = false;
        }
    });
    if (reset) {
        connect(reset, &QPushButton::clicked, this, [this]() {
            fill(defaults_());
            follows_defaults_ = true;
        });
    }
    connect(list_, &QTableWidget::itemChanged, this, [this]() {
        if (!filling_)
            follows_defaults_ = false;
    });

    fill(current);
    search_->setFocus();
}

void SymbolListDialog::run_search() {
    const QString q = search_->text().trimmed();
    if (q.size() < 1) {
        results_->clear();
        status_->clear();
        return;
    }
    pending_query_ = q;
    status_->setText(tr("Searching…"));
    services::MarketSearchService::instance().search(q, QString(), 20, q);
}

void SymbolListDialog::fill(const QVector<QuoteGridEntry>& list) {
    filling_ = true;
    list_->setRowCount(0);
    for (const auto& e : list) {
        const int r = list_->rowCount();
        list_->insertRow(r);
        auto* s = new QTableWidgetItem(e.symbol);
        s->setFlags(s->flags() & ~Qt::ItemIsEditable);
        s->setData(kSymDlgNameRole, e.group); // group rides along unchanged
        list_->setItem(r, 0, s);
        list_->setItem(r, 1, new QTableWidgetItem(e.label));
    }
    filling_ = false;
}

void SymbolListDialog::add_entry(const QString& symbol_in, const QString& label) {
    const QString symbol = symbol_in.trimmed();
    if (symbol.isEmpty())
        return;
    if (max_count_ == 1) {
        fill({{symbol, label, {}}});
        follows_defaults_ = false;
        return;
    }
    for (int r = 0; r < list_->rowCount(); ++r) {
        if (list_->item(r, 0) && list_->item(r, 0)->text().compare(symbol, Qt::CaseInsensitive) == 0) {
            list_->selectRow(r);
            status_->setText(tr("%1 is already in the list").arg(symbol));
            return;
        }
    }
    if (list_->rowCount() >= max_count_) {
        status_->setText(tr("The list is full (%1 symbols)").arg(max_count_));
        return;
    }
    filling_ = true;
    const int r = list_->rowCount();
    list_->insertRow(r);
    auto* s = new QTableWidgetItem(symbol);
    s->setFlags(s->flags() & ~Qt::ItemIsEditable);
    list_->setItem(r, 0, s);
    list_->setItem(r, 1, new QTableWidgetItem(label));
    list_->selectRow(r);
    filling_ = false;
    follows_defaults_ = false;
    status_->setText(tr("Added %1").arg(symbol));
}

void SymbolListDialog::move_selected(int delta) {
    const int r = list_->currentRow();
    const int to = r + delta;
    if (r < 0 || to < 0 || to >= list_->rowCount())
        return;
    filling_ = true;
    for (int c = 0; c < list_->columnCount(); ++c) {
        auto* a = list_->takeItem(r, c);
        auto* b = list_->takeItem(to, c);
        list_->setItem(r, c, b);
        list_->setItem(to, c, a);
    }
    filling_ = false;
    list_->selectRow(to);
    follows_defaults_ = false;
}

QVector<QuoteGridEntry> SymbolListDialog::entries() const {
    QVector<QuoteGridEntry> out;
    for (int r = 0; r < list_->rowCount(); ++r) {
        auto* s = list_->item(r, 0);
        if (!s || s->text().trimmed().isEmpty())
            continue;
        QuoteGridEntry e;
        e.symbol = s->text().trimmed();
        e.group = s->data(kSymDlgNameRole).toString();
        if (auto* l = list_->item(r, 1))
            e.label = l->text().trimmed();
        out.append(e);
    }
    return out;
}

} // namespace fincept::screens::widgets
