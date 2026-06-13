#include "screens/alpha_arena/NewCompetitionWizard.h"
#include "screens/alpha_arena/panels/Geofence.h"
#include "screens/alpha_arena/panels/LiveModeGateDialog.h"
#include "services/llm/ProviderCatalog.h"
#include "trading/exchanges/hyperliquid/HyperliquidSigner.h"
#include <QColor>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace fincept::screens::alpha_arena {
using fincept::ai_chat::ProviderCatalog;
using fincept::arena::ArenaConfig;
using fincept::arena::ArenaAgentSpec;
using fincept::arena::ArenaModelOption;
using fincept::arena::ArenaModelRegistry;

namespace {
const QStringList kCoins = {"BTC", "ETH", "SOL", "BNB", "DOGE", "XRP", "AVAX", "LINK", "ARB", "OP"};
constexpr int kDefaultChecked = 6;

// Colored "●" for the provider rows (a QListWidgetItem can't color a substring).
QIcon dot_icon(const QString& hex) {
    QPixmap pm(12, 12);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(hex));
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 10, 10);
    return QIcon(pm);
}
} // namespace

NewCompetitionWizard::NewCompetitionWizard(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("New Competition"));
    setModal(true);
    resize(560, 540);
    auto* root = new QVBoxLayout(this);
    steps_ = new QStackedWidget;
    root->addWidget(steps_, 1);
    build_step1();
    build_step2();

    auto* nav = new QHBoxLayout;
    back_ = new QPushButton(tr("BACK"));
    next_ = new QPushButton(tr("NEXT"));
    auto* cancel = new QPushButton(tr("CANCEL"));
    nav->addWidget(cancel); nav->addStretch(1); nav->addWidget(back_); nav->addWidget(next_);
    root->addLayout(nav);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(back_, &QPushButton::clicked, this, [this]() { steps_->setCurrentIndex(0); update_nav(); });
    connect(next_, &QPushButton::clicked, this, &NewCompetitionWizard::on_next_or_create);
    connect(&ArenaModelRegistry::instance(), &ArenaModelRegistry::models_changed,
            this, &NewCompetitionWizard::populate_models);
    update_nav();
}

void NewCompetitionWizard::build_step1() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    auto* title = new QLabel(tr("1 · Pick the competing models"));
    title->setStyleSheet("color:#FF8800;font-weight:700;font-size:14px;");
    v->addWidget(title);
    auto* panes = new QHBoxLayout;
    provider_list_ = new QListWidget;
    provider_list_->setFixedWidth(190);
    provider_list_->setStyleSheet("background:#111;");
    panes->addWidget(provider_list_);
    model_list_ = new QListWidget;
    model_list_->setStyleSheet("background:#111;");
    panes->addWidget(model_list_, 1);
    v->addLayout(panes, 1);
    selection_summary_ = new QLabel(tr("Selected: none"));
    selection_summary_->setWordWrap(true);
    selection_summary_->setStyleSheet("color:#BBB;");
    v->addWidget(selection_summary_);
    model_hint_ = new QLabel(tr("Pick a provider on the left, then check its models — selections "
                                "from several providers combine. Greyed entries need an API key in "
                                "Settings → LLM Configuration. Ollama and Fincept work without one."));
    model_hint_->setWordWrap(true);
    model_hint_->setStyleSheet("color:#888;");
    v->addWidget(model_hint_);
    steps_->addWidget(page);
    connect(provider_list_, &QListWidget::currentRowChanged,
            this, &NewCompetitionWizard::on_provider_row_changed);
    connect(model_list_, &QListWidget::itemChanged,
            this, &NewCompetitionWizard::on_model_item_changed);
    populate_models();
}

void NewCompetitionWizard::populate_models() {
    repopulating_ = true;
    const QString prev_provider = current_provider();

    options_by_provider_.clear();
    QSet<QString> valid_keys;
    for (const auto& o : ArenaModelRegistry::instance().available_models()) {
        options_by_provider_[o.provider.toLower()].append(o);
        valid_keys.insert(o.provider + "/" + o.model_id);
    }
    // Drop selections whose model vanished from the registry (deleted profile/
    // config row) — they could no longer resolve credentials at round time.
    for (auto it = selected_models_.begin(); it != selected_models_.end();)
        it = valid_keys.contains(it.key()) ? std::next(it) : selected_models_.erase(it);

    // Provider pane, ordered by display name; keep the previous selection when
    // still present, else default to the first provider with a ready option.
    QStringList providers = options_by_provider_.keys();
    std::sort(providers.begin(), providers.end(), [](const QString& a, const QString& b) {
        return ProviderCatalog::display_name(a).compare(ProviderCatalog::display_name(b),
                                                        Qt::CaseInsensitive) < 0;
    });
    provider_list_->clear();
    int restore_row = -1, first_ready = -1;
    for (const QString& p : providers) {
        auto* it = new QListWidgetItem(provider_list_);
        it->setData(Qt::UserRole, p);
        const int row = provider_list_->count() - 1;
        if (p == prev_provider) restore_row = row;
        if (first_ready < 0)
            for (const auto& o : options_by_provider_.value(p))
                if (o.ready) { first_ready = row; break; }
    }
    refresh_provider_decorations();
    repopulating_ = false;
    if (provider_list_->count() > 0)
        // currentRowChanged → populate_model_pane (re-applies checks).
        provider_list_->setCurrentRow(restore_row >= 0 ? restore_row
                                                       : (first_ready >= 0 ? first_ready : 0));
    else
        populate_model_pane(QString());
    update_selection_summary();
}

void NewCompetitionWizard::on_provider_row_changed() {
    if (repopulating_) return;
    populate_model_pane(current_provider());
}

QString NewCompetitionWizard::current_provider() const {
    auto* it = provider_list_ ? provider_list_->currentItem() : nullptr;
    return it ? it->data(Qt::UserRole).toString() : QString();
}

void NewCompetitionWizard::populate_model_pane(const QString& provider) {
    repopulating_ = true;
    model_list_->clear();
    for (const auto& o : options_by_provider_.value(provider)) {
        auto* it = new QListWidgetItem(model_list_);
        const QString key = o.provider + "/" + o.model_id;
        it->setText(o.ready ? o.display_name : o.display_name + tr("  — needs API key"));
        it->setData(Qt::UserRole, key);
        it->setData(Qt::UserRole + 1, QVariant::fromValue(QStringList{o.provider, o.model_id,
                    o.display_name, o.color_hex, o.source_kind, o.source_ref}));
        it->setForeground(QColor(o.ready ? "#DDD" : "#555"));
        it->setFlags(o.ready ? (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable) : Qt::NoItemFlags);
        it->setCheckState(selected_models_.contains(key) ? Qt::Checked : Qt::Unchecked);
    }
    repopulating_ = false;
}

void NewCompetitionWizard::on_model_item_changed(QListWidgetItem* it) {
    if (repopulating_ || !it) return;
    const QString key = it->data(Qt::UserRole).toString();
    if (it->checkState() == Qt::Checked) {
        const auto f = it->data(Qt::UserRole + 1).value<QStringList>();
        if (f.size() < 6) return;
        ArenaModelOption o;
        o.provider = f[0]; o.model_id = f[1]; o.display_name = f[2];
        o.color_hex = f[3]; o.source_kind = f[4]; o.source_ref = f[5];
        o.ready = true;   // only ready rows are checkable
        selected_models_.insert(key, o);
    } else {
        selected_models_.remove(key);
    }
    refresh_provider_decorations();
    update_selection_summary();
}

void NewCompetitionWizard::refresh_provider_decorations() {
    QHash<QString, int> counts;   // checked models per provider (source of truth)
    for (const auto& o : selected_models_) counts[o.provider.toLower()]++;
    for (int i = 0; i < provider_list_->count(); ++i) {
        auto* it = provider_list_->item(i);
        const QString p = it->data(Qt::UserRole).toString();
        bool any_ready = false;
        for (const auto& o : options_by_provider_.value(p))
            if (o.ready) { any_ready = true; break; }
        QString text = ProviderCatalog::display_name(p);
        if (const int n = counts.value(p); n > 0) text += QStringLiteral(" (%1)").arg(n);
        if (!any_ready) text += tr(" — needs key");
        it->setText(text);
        it->setIcon(dot_icon(any_ready ? ProviderCatalog::brand_color(p)
                                       : QStringLiteral("#555555")));
        it->setForeground(QColor(any_ready ? "#DDD" : "#777"));
    }
}

void NewCompetitionWizard::update_selection_summary() {
    QStringList parts;
    for (const auto& o : selected_models_)
        parts << QStringLiteral("%1 (%2)").arg(o.model_id, ProviderCatalog::display_name(o.provider));
    selection_summary_->setText(parts.isEmpty() ? tr("Selected: none")
                                                : tr("Selected: ") + parts.join(", "));
}

void NewCompetitionWizard::build_step2() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    auto* title = new QLabel(tr("2 · Arena settings"));
    title->setStyleSheet("color:#FF8800;font-weight:700;font-size:14px;");
    v->addWidget(title);
    auto* grid = new QGridLayout;
    int row = 0;
    grid->addWidget(new QLabel(tr("Name")), row, 0);
    name_ = new QLineEdit(tr("Arena-%1").arg(QDateTime::currentDateTime().toString("MMdd-HHmm")));
    grid->addWidget(name_, row++, 1);
    grid->addWidget(new QLabel(tr("Instruments")), row, 0);
    instruments_ = new QListWidget;
    instruments_->setMaximumHeight(110);
    for (int i = 0; i < kCoins.size(); ++i) {
        auto* it = new QListWidgetItem(kCoins[i], instruments_);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        it->setCheckState(i < kDefaultChecked ? Qt::Checked : Qt::Unchecked);
    }
    grid->addWidget(instruments_, row++, 1);
    grid->addWidget(new QLabel(tr("Capital / agent ($)")), row, 0);
    capital_ = new QDoubleSpinBox;
    capital_->setRange(100, 10'000'000); capital_->setValue(10000); capital_->setDecimals(0);
    grid->addWidget(capital_, row++, 1);
    grid->addWidget(new QLabel(tr("Round cadence (s)")), row, 0);
    cadence_ = new QSpinBox;
    cadence_->setRange(60, 3600); cadence_->setValue(180);
    grid->addWidget(cadence_, row++, 1);
    grid->addWidget(new QLabel(tr("Max leverage")), row, 0);
    max_lev_ = new QDoubleSpinBox;
    max_lev_->setRange(1, 50); max_lev_->setValue(10); max_lev_->setDecimals(0);
    grid->addWidget(max_lev_, row++, 1);
    v->addLayout(grid);

    auto* adv = new QGroupBox(tr("Advanced"));
    adv->setCheckable(true);
    adv->setChecked(false);
    auto* av = new QVBoxLayout(adv);
    av->addWidget(new QLabel(tr("Custom system prompt (JSON output contract is always appended):")));
    custom_prompt_ = new QPlainTextEdit;
    custom_prompt_->setPlaceholderText(tr("Leave empty for the default arena prompt."));
    custom_prompt_->setMaximumHeight(90);
    av->addWidget(custom_prompt_);
    auto* vh = new QHBoxLayout;
    venue_paper_ = new QRadioButton(tr("Paper (real market data, simulated fills)"));
    venue_live_ = new QRadioButton(tr("Hyperliquid LIVE (real funds — beta: order mirroring not yet active)"));
    venue_paper_->setChecked(true);
    vh->addWidget(venue_paper_); vh->addWidget(venue_live_);
    av->addLayout(vh);
    // Hide content when unchecked. Collapsing Advanced also reverts the venue
    // to Paper — live mode must never ride along invisibly.
    const auto sync_adv = [adv, this]() {
        for (auto* w : adv->findChildren<QWidget*>()) w->setVisible(adv->isChecked());
        if (!adv->isChecked() && venue_live_->isChecked()) venue_paper_->setChecked(true);
    };
    connect(adv, &QGroupBox::toggled, this, sync_adv);
    sync_adv();
    v->addWidget(adv);
    v->addStretch(1);
    steps_->addWidget(page);
}

void NewCompetitionWizard::update_nav() {
    const bool last = steps_->currentIndex() == 1;
    back_->setVisible(last);
    next_->setText(last ? tr("CREATE && START") : tr("NEXT"));
}

void NewCompetitionWizard::on_next_or_create() {
    if (steps_->currentIndex() == 0) {
        if (selected_models_.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Pick at least one model."));
            return;
        }
        steps_->setCurrentIndex(1);
        update_nav();
        return;
    }
    // Build config.
    cfg_ = ArenaConfig{};
    cfg_.name = name_->text().trimmed().isEmpty() ? QStringLiteral("Arena") : name_->text().trimmed();
    for (int i = 0; i < instruments_->count(); ++i)
        if (instruments_->item(i)->checkState() == Qt::Checked)
            cfg_.instruments << instruments_->item(i)->text();
    if (cfg_.instruments.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Pick at least one instrument."));
        return;
    }
    cfg_.capital_per_agent = capital_->value();
    cfg_.cadence_seconds = cadence_->value();
    cfg_.max_leverage = max_lev_->value();
    cfg_.system_prompt_override = custom_prompt_->toPlainText().trimmed();
    for (const auto& o : selected_models_) {   // QMap values → deterministic order
        ArenaAgentSpec a;
        a.provider = o.provider; a.model_id = o.model_id; a.display_name = o.display_name;
        a.color_hex = o.color_hex; a.source_kind = o.source_kind; a.source_ref = o.source_ref;
        cfg_.agents.append(a);
    }
    if (venue_live_->isChecked()) {
        if (!engage_live_gates()) return;   // stays on the wizard
        cfg_.venue = QStringLiteral("hyperliquid");
        cfg_.live_mode = true;              // carried from the START (fixes the old ordering bug)
    }
    accept();
}

bool NewCompetitionWizard::engage_live_gates() {
    if (!fincept::trading::hyperliquid::HyperliquidSigner::is_wired()) {
        QMessageBox::critical(this, windowTitle(),
            tr("Live mode unavailable: the Hyperliquid signer failed to initialise in this build."));
        return false;
    }
#ifndef FINCEPT_UNSAFE_DISABLE_GEOFENCE
    if (is_jurisdiction_blocked(QLocale::system().territory())) {
        QMessageBox::critical(this, windowTitle(), tr("Live mode is unavailable in your jurisdiction."));
        return false;
    }
#endif
    // competition id doesn't exist yet — gate stores the wallet key under the
    // name we pass; engine re-keys it after create (Task 6).
    LiveModeGateDialog dlg(QStringLiteral("pending"), this);
    return dlg.exec() == QDialog::Accepted;
}

fincept::arena::ArenaConfig NewCompetitionWizard::config() const { return cfg_; }

} // namespace fincept::screens::alpha_arena
