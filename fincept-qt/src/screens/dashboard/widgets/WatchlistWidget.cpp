#include "screens/dashboard/widgets/WatchlistWidget.h"

#include "core/events/EventBus.h"
#include "core/market/MarketProfile.h"
#include "screens/dashboard/widgets/SymbolListDialog.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>

#include <algorithm>

namespace fincept::screens::widgets {

WatchlistWidget::WatchlistWidget(const QJsonObject& cfg, QWidget* parent)
    : QuoteTableWidget(QuoteListKind::Custom, cfg, parent) {
    auto* bar = new QWidget(this);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    hl->setSpacing(6);
    auto* lbl = new QLabel(tr("LIST"), bar);
    lbl->setObjectName("wlSourceLabel");
    lbl->setStyleSheet(QString("QLabel#wlSourceLabel{color:%1;font-size:%2px;font-weight:bold;background:transparent;}")
                           .arg(ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::px(-4)));
    hl->addWidget(lbl);
    source_ = new QComboBox(bar);
    source_->setAccessibleName(tr("Watchlist source"));
    source_->setToolTip(tr("Show one of your saved watchlists (edited in the Watchlist screen) or a list kept in "
                           "this tile (edit it with the gear icon)"));
    hl->addWidget(source_, 1);
    content_layout()->insertWidget(0, bar);

    connect(source_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (filling_)
            return;
        bind(source_->currentData().toString());
        region_chosen_ = watchlist_id_.isEmpty() && follow_region_;
        emit config_changed(config());
    });
    EventBus::instance().subscribe(this, QStringLiteral("watchlist.updated"), [this](const QVariantMap&) {
        reload_sources();
        if (!watchlist_id_.isEmpty())
            set_entries(default_entries());
        update_footer();
        update_empty_state();
    });

    reload_sources();
    apply_config(cfg);
}

QString WatchlistWidget::base_title() const {
    return tr("WATCHLIST");
}

void WatchlistWidget::retranslateUi() {
    QuoteTableWidget::retranslateUi();
    reload_sources();
    if (is_empty_state_shown())
        update_empty_state();
}

void WatchlistWidget::reload_sources() {
    filling_ = true;
    source_->clear();
    source_->addItem(tr("This tile's list"), QString());
    const auto r = WatchlistRepository::instance().list_all();
    if (r.is_ok()) {
        auto lists = r.value();
        std::stable_sort(lists.begin(), lists.end(),
                         [](const Watchlist& a, const Watchlist& b) { return a.sort_order < b.sort_order; });
        for (const auto& w : lists)
            source_->addItem(w.name + (w.is_default ? tr(" (default)") : QString()), w.id);
    }
    const int idx = source_->findData(watchlist_id_);
    source_->setCurrentIndex(idx >= 0 ? idx : 0);
    filling_ = false;
}

void WatchlistWidget::bind(const QString& watchlist_id) {
    watchlist_id_ = watchlist_id;
    watchlist_name_.clear();
    if (!watchlist_id_.isEmpty()) {
        const auto w = WatchlistRepository::instance().get(watchlist_id_);
        if (w.is_ok())
            watchlist_name_ = w.value().name;
        else
            watchlist_id_.clear(); // deleted meanwhile — fall back to the tile's list
        follow_region_ = false;
        set_entries(default_entries());
    } else if (grid()->entries().isEmpty()) {
        follow_region_ = true;
        set_entries(default_entries());
    }
    filling_ = true;
    const int idx = source_->findData(watchlist_id_);
    source_->setCurrentIndex(idx >= 0 ? idx : 0);
    filling_ = false;
    update_title();
    update_footer();
    update_empty_state();
}

void WatchlistWidget::update_empty_state() {
    if (!grid()->entries().isEmpty()) {
        hide_empty_state();
        return;
    }
    // Only a bound saved watchlist can be empty here (the tile's own list and the
    // region list never are); the name says which one.
    const QString name = watchlist_id_.isEmpty() ? tr("This tile's list")
                                                 : (watchlist_name_.isEmpty() ? tr("This watchlist") : watchlist_name_);
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("%1 is empty").arg(name),
        tr("Add symbols to it in the Watchlist screen and they show here with live prices — or show your region's "
           "large caps in this tile meanwhile."),
        tr("OPEN WATCHLIST"));
    spec.secondary = tr("SHOW REGION LARGE CAPS");
    show_empty_state(
        spec,
        []() {
            EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                         QVariantMap{{"screen_id", QStringLiteral("watchlist")}, {"exclusive", true}});
        },
        [this]() { follow_region_large_caps(); });
}

void WatchlistWidget::follow_region_large_caps() {
    // The fresh-tile fallback path (apply_config): the region's large caps,
    // following region changes, kept as this tile's configuration.
    watchlist_id_.clear();
    watchlist_name_.clear();
    follow_region_ = true;
    region_chosen_ = true;
    set_entries(default_entries());
    bind(QString());
    emit config_changed(config());
}

QVector<QuoteGridEntry> WatchlistWidget::default_entries() const {
    QVector<QuoteGridEntry> out;
    if (!watchlist_id_.isEmpty()) {
        const auto r = WatchlistRepository::instance().get_stocks(watchlist_id_);
        if (r.is_ok()) {
            auto stocks = r.value();
            std::stable_sort(stocks.begin(), stocks.end(),
                             [](const WatchlistStock& a, const WatchlistStock& b) { return a.sort_order < b.sort_order; });
            for (const auto& s : stocks)
                if (!s.symbol.trimmed().isEmpty())
                    out.append({s.symbol.trimmed(), s.name, {}});
        }
        return out;
    }
    return profile_entries(QuoteListKind::Custom);
}

QString WatchlistWidget::source_text() const {
    if (!watchlist_id_.isEmpty())
        return tr("Saved watchlist: %1").arg(watchlist_name_.isEmpty() ? watchlist_id_ : watchlist_name_);
    if (follow_region_)
        return tr("%1 large caps — pick a saved watchlist above or edit with the gear")
            .arg(core::market::market_profile_name(core::market::MarketProfileStore::instance().current()));
    return tr("This tile's list");
}

QJsonObject WatchlistWidget::config() const {
    if (!watchlist_id_.isEmpty())
        return QJsonObject{{"watchlist_id", watchlist_id_}};
    QJsonObject o = QuoteTableWidget::config();
    if (region_chosen_ && follow_region_)
        o.insert("list_source", QStringLiteral("region"));
    return o;
}

void WatchlistWidget::apply_config(const QJsonObject& cfg) {
    if (!source_)
        return; // base-class construction: the real apply runs from our constructor
    region_chosen_ = false;
    if (cfg.value("list_source").toString() == QLatin1String("region")) {
        region_chosen_ = true;
        watchlist_id_.clear();
        follow_region_ = true;
        set_entries(default_entries());
        bind(QString());
        return;
    }
    const QString id = cfg.value("watchlist_id").toString();
    if (!id.isEmpty()) {
        bind(id);
        return;
    }
    const auto list = entries_from_json(cfg);
    if (!list.isEmpty()) {
        watchlist_id_.clear();
        follow_region_ = false;
        set_entries(list);
        bind(QString());
        return;
    }
    // Fresh tile: the default saved watchlist when the user has one.
    QString default_id;
    const auto r = WatchlistRepository::instance().list_all();
    if (r.is_ok()) {
        for (const auto& w : r.value())
            if (w.is_default) {
                default_id = w.id;
                break;
            }
    }
    if (!default_id.isEmpty()) {
        bind(default_id);
        return;
    }
    watchlist_id_.clear();
    follow_region_ = true;
    set_entries(default_entries());
    bind(QString());
}

QDialog* WatchlistWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new SymbolListDialog(watchlist_id_.isEmpty()
                                         ? tr("Edit watchlist tile")
                                         : tr("Edit a copy of %1 (the tile then keeps its own list)").arg(watchlist_name_),
                                     grid()->entries(), {}, parent);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        const auto list = dlg->entries();
        if (list.isEmpty())
            return;
        watchlist_id_.clear();
        follow_region_ = false;
        set_entries(list);
        bind(QString());
        emit config_changed(config());
    });
    return dlg;
}

} // namespace fincept::screens::widgets
