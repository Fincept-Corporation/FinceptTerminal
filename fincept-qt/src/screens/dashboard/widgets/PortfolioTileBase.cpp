#include "screens/dashboard/widgets/PortfolioTileBase.h"

#include "core/events/EventBus.h"
#include "services/portfolio/PortfolioService.h"
#include "services/portfolio/PortfolioValuation.h"
#include "storage/repositories/PortfolioRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::widgets {

PortfolioTileBase::PortfolioTileBase(const QString& title, QWidget* parent, const QString& accent)
    : BaseWidget(title, parent, accent) {
    auto* bar = new QWidget(this);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    hl->setSpacing(6);
    auto* lbl = new QLabel(tr("PORTFOLIO"), bar);
    lbl->setObjectName("ptbLabel");
    lbl->setStyleSheet(QString("QLabel#ptbLabel{color:%1;font-size:%2px;font-weight:bold;background:transparent;}")
                           .arg(ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::px(-4)));
    hl->addWidget(lbl);
    picker_ = new QComboBox(bar);
    picker_->setAccessibleName(tr("Portfolio"));
    hl->addWidget(picker_, 1);
    content_layout()->addWidget(bar);

    connect(picker_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (filling_)
            return;
        select(picker_->currentData().toString(), true);
        emit config_changed(config());
    });
    auto& svc = services::PortfolioService::instance();
    connect(&svc, &services::PortfolioService::portfolios_loaded, this, [this]() { reload_portfolios(); });
    connect(&svc, &services::PortfolioService::portfolio_created, this, [this]() { reload_portfolios(); });
    connect(&svc, &services::PortfolioService::portfolio_deleted, this, [this]() { reload_portfolios(); });
}

void PortfolioTileBase::update_empty_state() {
    if (!portfolios_.isEmpty()) {
        hide_empty_state();
        return;
    }
    // The shared empty state (built on first need, follows the theme by itself)
    // instead of a hand-styled panel restyled on every theme change.
    ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
        tr("No portfolio yet"),
        tr("Create a portfolio, import one from a file, or load the demo in the Portfolio screen — this tile fills "
           "in as soon as one exists."),
        tr("OPEN PORTFOLIO"));
    spec.glyph = QStringLiteral("◔"); // ◔
    spec.glyph_tone = ui::EmptyState::Tone::Accent;
    show_empty_state(spec, []() {
        EventBus::instance().publish("nav.switch_screen",
                                     QVariantMap{{"screen_id", QStringLiteral("portfolio")}, {"exclusive", true}});
    });
}

QString PortfolioTileBase::base_currency() const {
    // The service's rule (a row stored without a currency is valued in the display
    // currency, logged once per portfolio), so the tile labels what it is given.
    return portfolio::base_currency_of(portfolio_.currency.trimmed(), portfolio_.id).toUpper();
}

QJsonObject PortfolioTileBase::config() const {
    QJsonObject o;
    if (!portfolio_.id.isEmpty())
        o.insert("portfolio_id", portfolio_.id);
    return o;
}

void PortfolioTileBase::apply_config(const QJsonObject& cfg) {
    wanted_id_ = cfg.value("portfolio_id").toString();
    reload_portfolios();
}

void PortfolioTileBase::retranslateUi() {
    BaseWidget::retranslateUi();
    if (portfolios_.isEmpty())
        update_empty_state();
}

void PortfolioTileBase::reload_portfolios() {
    const auto r = PortfolioRepository::instance().list_portfolios();
    portfolios_ = r.is_ok() ? r.value() : QVector<portfolio::Portfolio>{};
    filling_ = true;
    picker_->clear();
    for (const auto& p : portfolios_)
        picker_->addItem(QStringLiteral("%1  (%2)").arg(p.name, p.currency.toUpper()), p.id);
    filling_ = false;
    QString id = !portfolio_.id.isEmpty() ? portfolio_.id : wanted_id_;
    bool found = false;
    for (const auto& p : portfolios_)
        found = found || p.id == id;
    if (!found)
        id = portfolios_.isEmpty() ? QString() : portfolios_.first().id;
    const bool changed = id != portfolio_.id || portfolio_.id.isEmpty();
    select(id, changed);
}

void PortfolioTileBase::select(const QString& id, bool notify) {
    portfolio_ = portfolio::Portfolio{};
    portfolio_.id.clear();
    for (const auto& p : portfolios_)
        if (p.id == id)
            portfolio_ = p;
    filling_ = true;
    const int idx = picker_->findData(portfolio_.id);
    if (idx >= 0)
        picker_->setCurrentIndex(idx);
    filling_ = false;
    picker_->setEnabled(!portfolios_.isEmpty());
    update_empty_state();
    if (notify)
        on_portfolio_selected();
}

} // namespace fincept::screens::widgets
