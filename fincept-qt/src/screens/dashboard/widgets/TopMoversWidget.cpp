#include "screens/dashboard/widgets/TopMoversWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QLineEdit>
#include <QRegularExpression>

namespace fincept::screens::widgets {

namespace {
constexpr int kTopMoversMaxSymbols = 60;

QStringList top_movers_parse_symbols(const QString& text) {
    static const QRegularExpression kSep(QStringLiteral("[,\\s]+"));
    QStringList out;
    for (const QString& p : text.split(kSep, Qt::SkipEmptyParts)) {
        const QString s = p.trimmed().toUpper();
        if (!s.isEmpty() && !out.contains(s))
            out << s;
    }
    return out.mid(0, kTopMoversMaxSymbols);
}
} // namespace

TopMoversWidget::TopMoversWidget(const QJsonObject& cfg, QWidget* parent) : BaseWidget(tr("TOP MOVERS"), parent) {
    // Tab toggle
    auto* tab_bar = new QWidget(this);
    tab_bar->setFixedHeight(26);
    auto* tl = new QHBoxLayout(tab_bar);
    tl->setContentsMargins(4, 2, 4, 2);
    tl->setSpacing(0);

    gainers_btn_ = new QPushButton(QString(QChar(0x25B2)) + " " + tr("GAINERS"));
    losers_btn_ = new QPushButton(QString(QChar(0x25BC)) + " " + tr("LOSERS"));
    gainers_btn_->setAccessibleName(tr("Show top gainers"));
    losers_btn_->setAccessibleName(tr("Show top losers"));

    connect(gainers_btn_, &QPushButton::clicked, this, [this]() { show_tab(true); });
    connect(losers_btn_, &QPushButton::clicked, this, [this]() { show_tab(false); });

    tl->addWidget(gainers_btn_, 1);
    tl->addWidget(losers_btn_, 1);

    content_layout()->addWidget(tab_bar);

    universe_label_ = new QLabel(this);
    universe_label_->setObjectName("topMoversUniverse");
    universe_label_->setContentsMargins(6, 2, 6, 2);
    content_layout()->addWidget(universe_label_);

    // Table
    table_ = new ui::DataTable;
    table_->set_headers({tr("SYMBOL"), tr("PRICE"), tr("CHG%")});
    table_->set_column_widths({100, 90, 80});
    table_->setToolTip(tr("Double-click a row to open it in Equity Research · right-click for more"));
    content_layout()->addWidget(table_);
    install_symbol_menu(table_, 0);

    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        if (auto* it = table_->item(row, 0))
            open_symbol(it->text());
    });

    connect(this, &BaseWidget::refresh_requested, this, &TopMoversWidget::refresh_data);

    symbols_ = services::MarketDataService::mover_symbols();
    set_configurable(true);
    apply_styles();
    set_loading(true);
    apply_config(cfg);
}

QJsonObject TopMoversWidget::config() const {
    // Persist only a user-chosen universe; the default keeps tracking
    // MarketDataService::mover_symbols() if that list is ever updated.
    QJsonObject o;
    if (symbols_ != services::MarketDataService::mover_symbols())
        o.insert("symbols", QJsonArray::fromStringList(symbols_));
    return o;
}

void TopMoversWidget::apply_config(const QJsonObject& cfg) {
    QStringList next;
    for (const auto& v : cfg.value("symbols").toArray()) {
        const QString s = v.toString().trimmed().toUpper();
        if (!s.isEmpty() && !next.contains(s))
            next << s;
    }
    if (next.isEmpty())
        next = services::MarketDataService::mover_symbols();
    next = next.mid(0, kTopMoversMaxSymbols);
    if (next != symbols_) {
        symbols_ = next;
        row_cache_.clear();
        all_quotes_.clear();
        if (isVisible()) {
            datahub::DataHub::instance().unsubscribe(this);
            hub_active_ = false;
            hub_subscribe_all();
        }
    }
    show_tab(showing_gainers_);
}

QDialog* TopMoversWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Top Movers"));
    auto* form = new QFormLayout(dlg);
    auto* edit = new QLineEdit(dlg);
    edit->setText(symbols_.join(QStringLiteral(", ")));
    edit->setPlaceholderText(tr("e.g. AAPL, MSFT, NVDA (empty = default list)"));
    edit->setToolTip(tr("Movers are ranked within these symbols only (up to %1).").arg(kTopMoversMaxSymbols));
    form->addRow(tr("Universe"), edit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit]() {
        QJsonObject cfg;
        cfg.insert("symbols", QJsonArray::fromStringList(top_movers_parse_symbols(edit->text())));
        apply_config(cfg);
        emit config_changed(config());
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void TopMoversWidget::apply_styles() {
    if (universe_label_)
        universe_label_->setStyleSheet(
            QString("color:%1;font-size:9px;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
    // Theme changed → force the tab-button stylesheets to be re-applied.
    tab_style_applied_ = false;
    show_tab(showing_gainers_);
}

void TopMoversWidget::on_theme_changed() {
    apply_styles();
}

void TopMoversWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void TopMoversWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void TopMoversWidget::refresh_data() {
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(symbols_.size());
    for (const auto& sym : symbols_)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics, /*force=*/true); // user-triggered: bypass min_interval
}

void TopMoversWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    set_loading_progress(row_cache_.size(), symbols_.size());
    for (const auto& sym : symbols_) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading_progress(row_cache_.size(), symbols_.size());
            // One sort + redraw per delivery burst, not one per symbol.
            schedule_render([this]() { rebuild_from_cache(); });
        });
    }
    hub_active_ = true;
}

void TopMoversWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void TopMoversWidget::rebuild_from_cache() {
    all_quotes_.clear();
    all_quotes_.reserve(row_cache_.size());
    for (const auto& sym : symbols_) {
        auto it = row_cache_.constFind(sym);
        if (it != row_cache_.constEnd())
            all_quotes_.append(it.value());
    }
    std::sort(all_quotes_.begin(), all_quotes_.end(),
              [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });
    show_tab(showing_gainers_);
}

void TopMoversWidget::show_tab(bool gainers) {
    // show_tab() runs on every hub delivery (via rebuild_from_cache), but the
    // two tab-button stylesheets only depend on `gainers` + the theme. Each
    // setStyleSheet is a full CSS reparse, so re-applying them per tick was
    // pure waste — only touch them when the state actually changed.
    const bool tab_style_dirty = (!tab_style_applied_ || showing_gainers_ != gainers);
    showing_gainers_ = gainers;

    if (tab_style_dirty) {
        tab_style_applied_ = true;
        auto active_g = QString("QPushButton { background: %1; color: %2; border: none; "
                                "font-size: 9px; font-weight: bold; padding: 4px; }")
                            .arg(ui::colors::POSITIVE(), ui::colors::BG_BASE());
        auto active_l = QString("QPushButton { background: %1; color: %2; border: none; "
                                "font-size: 9px; font-weight: bold; padding: 4px; }")
                            .arg(ui::colors::NEGATIVE(), ui::colors::BG_BASE());
        auto inactive = QString("QPushButton { background: %1; color: %2; border: none; "
                                "font-size: 9px; font-weight: bold; padding: 4px; }")
                            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY());

        gainers_btn_->setStyleSheet(gainers ? active_g : inactive);
        losers_btn_->setStyleSheet(gainers ? inactive : active_l);
    }

    table_->clear_data();

    QVector<services::QuoteData> filtered;
    for (const auto& q : all_quotes_) {
        if (gainers && q.change_pct > 0)
            filtered.append(q);
        if (!gainers && q.change_pct < 0)
            filtered.append(q);
    }

    // Show top 6
    const int count = static_cast<int>(std::min(filtered.size(), qsizetype(6)));
    for (int i = 0; i < count; ++i) {
        const auto& q = gainers ? filtered[i] : filtered[filtered.size() - 1 - i];
        table_->add_row({q.symbol, dash_fmt::price(q.price, q.symbol), dash_fmt::signed_pct(q.change_pct)});
        int row = table_->rowCount() - 1;
        table_->set_cell_color(row, 2, ui::change_color(q.change_pct));
    }

    // Say what the ranking is drawn from — and why the list can be empty.
    if (universe_label_) {
        const int have = static_cast<int>(all_quotes_.size());
        const int total = static_cast<int>(symbols_.size());
        QString text = tr("Ranked among %1 tracked symbols").arg(total);
        if (have > 0 && have < total)
            text += tr(" (%1 quoted)").arg(have);
        if (have > 0 && count == 0)
            text += gainers ? tr(" — none is up today") : tr(" — none is down today");
        universe_label_->setText(text);
        universe_label_->setToolTip(symbols_.join(QStringLiteral(", ")));
    }
}

void TopMoversWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("TOP MOVERS"));
    // Keep the ▲ / ▼ glyph prefixes the constructor applied — dropping them
    // here silently changed the button labels on any language switch.
    if (gainers_btn_) {
        gainers_btn_->setText(QString(QChar(0x25B2)) + " " + tr("GAINERS"));
        gainers_btn_->setAccessibleName(tr("Show top gainers"));
    }
    if (losers_btn_) {
        losers_btn_->setText(QString(QChar(0x25BC)) + " " + tr("LOSERS"));
        losers_btn_->setAccessibleName(tr("Show top losers"));
    }
    // Table headers are set once in the ctor — re-apply them here, which the
    // old rebuild_from_cache() call never did.
    if (table_)
        table_->set_headers({tr("SYMBOL"), tr("PRICE"), tr("CHG%")});
    rebuild_from_cache();
}

} // namespace fincept::screens::widgets
