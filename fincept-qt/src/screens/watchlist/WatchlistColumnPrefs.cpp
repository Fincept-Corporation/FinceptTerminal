#include "screens/watchlist/WatchlistColumnPrefs.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <QAction>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QTableWidget>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

namespace {
constexpr int kWatchlistColumnPrefsSaveDebounceMs = 600;
constexpr const char* kWatchlistColumnPrefsCategory = "watchlist";
} // namespace

WatchlistColumnPrefs* WatchlistColumnPrefs::attach(QTableWidget* table, const QString& settings_key,
                                                   int locked_column) {
    if (!table || settings_key.isEmpty())
        return nullptr;
    return new WatchlistColumnPrefs(table, settings_key, locked_column);
}

WatchlistColumnPrefs::WatchlistColumnPrefs(QTableWidget* table, QString settings_key, int locked_column)
    : QObject(table), table_(table), key_(std::move(settings_key)), locked_column_(locked_column) {
    auto* header = table->horizontalHeader();

    // Snapshot the as-built widths so "Reset columns" can return to them.
    QJsonObject widths;
    for (int c = 0; c < table->columnCount(); ++c)
        widths.insert(QString::number(c), header->sectionSize(c));
    defaults_.insert("widths", widths);

    header->setContextMenuPolicy(Qt::CustomContextMenu);
    header->setToolTip(tr("Right-click to choose columns"));
    connect(header, &QWidget::customContextMenuRequested, this, &WatchlistColumnPrefs::show_menu);

    save_timer_.setSingleShot(true);
    save_timer_.setInterval(kWatchlistColumnPrefsSaveDebounceMs);
    connect(&save_timer_, &QTimer::timeout, this, &WatchlistColumnPrefs::save);
    connect(header, &QHeaderView::sectionResized, this, [this](int, int, int) { schedule_save(); });
    connect(header, &QHeaderView::sortIndicatorChanged, this, [this](int, Qt::SortOrder) { schedule_save(); });

    restore_async();
}

void WatchlistColumnPrefs::show_menu(const QPoint& pos) {
    if (!table_)
        return;
    auto* header = table_->horizontalHeader();
    // Heap + popup(): no nested event loop, and safe if the table goes away.
    auto* menu = new QMenu(table_);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    for (int c = 0; c < table_->columnCount(); ++c) {
        const QString name = table_->model()->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
        QAction* act = menu->addAction(name.isEmpty() ? tr("Column %1").arg(c + 1) : name);
        act->setCheckable(true);
        act->setChecked(!header->isSectionHidden(c));
        act->setEnabled(c != locked_column_);
        connect(act, &QAction::toggled, this, [this, c](bool on) {
            if (!table_)
                return;
            table_->horizontalHeader()->setSectionHidden(c, !on);
            schedule_save();
        });
    }
    menu->addSeparator();
    connect(menu->addAction(tr("Reset columns")), &QAction::triggered, this, &WatchlistColumnPrefs::reset);
    menu->popup(header->mapToGlobal(pos));
}

void WatchlistColumnPrefs::restore_async() {
    QPointer<WatchlistColumnPrefs> self = this;
    const QString key = key_;
    (void)QtConcurrent::run([self, key]() {
        QJsonObject o;
        auto r = fincept::SettingsRepository::instance().get(key);
        if (r.is_ok() && !r.value().isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(r.value().toUtf8());
            if (doc.isObject())
                o = doc.object();
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, o]() {
                if (!self)
                    return;
                if (!o.isEmpty())
                    self->apply(o);
                self->ready_ = true;
            },
            Qt::QueuedConnection);
    });
}

void WatchlistColumnPrefs::apply(const QJsonObject& o) {
    if (!table_)
        return;
    restoring_ = true;
    auto* header = table_->horizontalHeader();
    const int cols = table_->columnCount();

    const QJsonObject widths = o.value("widths").toObject();
    for (auto it = widths.constBegin(); it != widths.constEnd(); ++it) {
        bool ok = false;
        const int c = it.key().toInt(&ok);
        const int w = it.value().toInt();
        if (ok && c >= 0 && c < cols && w >= 20 && w <= 2000)
            header->resizeSection(c, w);
    }

    if (o.contains("hidden")) {
        for (int c = 0; c < cols; ++c)
            header->setSectionHidden(c, false);
        for (const auto& v : o.value("hidden").toArray()) {
            const int c = v.toInt(-1);
            if (c >= 0 && c < cols && c != locked_column_)
                header->setSectionHidden(c, true);
        }
    }

    const int sort_col = o.value("sort_col").toInt(-1);
    if (sort_col >= 0 && sort_col < cols && table_->isSortingEnabled()) {
        const auto order = o.value("sort_order").toInt(0) == 1 ? Qt::DescendingOrder : Qt::AscendingOrder;
        table_->sortItems(sort_col, order);
        header->setSortIndicator(sort_col, order);
    }
    restoring_ = false;
}

void WatchlistColumnPrefs::schedule_save() {
    if (ready_ && !restoring_)
        save_timer_.start();
}

void WatchlistColumnPrefs::save() {
    if (!table_)
        return;
    auto* header = table_->horizontalHeader();
    QJsonObject widths;
    QJsonArray hidden;
    for (int c = 0; c < table_->columnCount(); ++c) {
        if (header->isSectionHidden(c))
            hidden.append(c);
        else
            widths.insert(QString::number(c), header->sectionSize(c));
    }
    QJsonObject o;
    o.insert("widths", widths);
    o.insert("hidden", hidden);
    if (header->isSortIndicatorShown()) {
        o.insert("sort_col", header->sortIndicatorSection());
        o.insert("sort_order", header->sortIndicatorOrder() == Qt::DescendingOrder ? 1 : 0);
    }
    const QString key = key_;
    const QString value = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    (void)QtConcurrent::run([key, value]() {
        auto r = fincept::SettingsRepository::instance().set(key, value, kWatchlistColumnPrefsCategory);
        if (r.is_err())
            LOG_WARN("WatchlistColumns", "Could not save column layout: " + QString::fromStdString(r.error()));
    });
}

void WatchlistColumnPrefs::reset() {
    if (!table_)
        return;
    QJsonObject o = defaults_;
    o.insert("hidden", QJsonArray{});
    apply(o);
    save_timer_.stop();
    save();
}

} // namespace fincept::screens
