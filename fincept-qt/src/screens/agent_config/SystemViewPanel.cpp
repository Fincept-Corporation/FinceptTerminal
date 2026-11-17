// src/screens/agent_config/SystemViewPanel.cpp
#include "screens/agent_config/SystemViewPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QScrollArea>
#include <QShowEvent>

namespace fincept::screens {

// ── Helpers ──────────────────────────────────────────────────────────────────

static QLabel* make_stat_value(const QString& text, const char* color) {
    auto* lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(8) + "px;font-weight:700;").arg(color));
    return lbl;
}

static QLabel* make_stat_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;letter-spacing:1px;padding-top:2px;")
                           .arg(ui::colors::TEXT_TERTIARY()));
    return lbl;
}

static QWidget* make_section_card(const QString& title, QVBoxLayout** content_layout,
                                  QPushButton** action_btn = nullptr, const QString& action_text = {},
                                  QLabel** title_out = nullptr) {
    auto* card = new QFrame;
    card->setStyleSheet(QString("QFrame { background:%1;border:1px solid %2; }")
                            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget(nullptr);
    header->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hdr_layout = new QHBoxLayout(header);
    hdr_layout->setContentsMargins(12, 6, 12, 6);

    auto* title_lbl = new QLabel(title);
    title_lbl->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    hdr_layout->addWidget(title_lbl);
    hdr_layout->addStretch();
    if (title_out)
        *title_out = title_lbl;

    if (action_btn && !action_text.isEmpty()) {
        *action_btn = new QPushButton(action_text);
        (*action_btn)->setCursor(Qt::PointingHandCursor);
        (*action_btn)
            ->setStyleSheet(QString("QPushButton { background:transparent;color:%1;border:1px solid %2;"
                                    "padding:2px 8px;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600; }"
                                    "QPushButton:hover { background:%2; }")
                                .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED()));
        hdr_layout->addWidget(*action_btn);
    }

    vl->addWidget(header);

    // Content area
    auto* content = new QWidget(nullptr);
    *content_layout = new QVBoxLayout(content);
    (*content_layout)->setContentsMargins(12, 8, 12, 8);
    (*content_layout)->setSpacing(4);
    vl->addWidget(content);

    return card;
}

// ── Constructor ──────────────────────────────────────────────────────────────

SystemViewPanel::SystemViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("SystemViewPanel");
    build_ui();
    setup_connections();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

// ── UI construction ──────────────────────────────────────────────────────────

void SystemViewPanel::build_ui() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { background:%1;border:none; }"
                                  "QScrollBar:vertical { background:%1;width:6px; }"
                                  "QScrollBar::handle:vertical { background:%2;min-height:30px; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT()));

    auto* content = new QWidget(this);
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(12);

    // Header
    auto* header_row = new QHBoxLayout;
    header_title_ = new QLabel(tr("SYSTEM CAPABILITIES"));
    header_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:700;letter-spacing:2px;").arg(ui::colors::AMBER()));
    header_row->addWidget(header_title_);
    header_row->addStretch();

    header_refresh_btn_ = new QPushButton(tr("REFRESH"));
    header_refresh_btn_->setCursor(Qt::PointingHandCursor);
    header_refresh_btn_->setStyleSheet(QString("QPushButton { background:transparent;color:%1;border:1px solid %2;"
                                               "padding:4px 12px;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;letter-spacing:1px; }"
                                               "QPushButton:hover { background:%2; }")
                                           .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED()));
    connect(header_refresh_btn_, &QPushButton::clicked, this, [this]() {
        data_loaded_ = false;
        // list_tools / get_system_info / discover_agents are served from a 5-10 min
        // cache, so without dropping it REFRESH re-displayed the same stale numbers.
        auto& svc = services::AgentService::instance();
        svc.clear_cache();
        svc.discover_agents();
        refresh_data();
    });
    header_row->addWidget(header_refresh_btn_);
    vl->addLayout(header_row);

    // Stats row
    vl->addWidget(build_stats_row());

    // LLM Providers
    vl->addWidget(build_llm_section());

    // Tools
    vl->addWidget(build_tools_section());

    // System info
    vl->addWidget(build_sysinfo_section());

    vl->addStretch();
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

QWidget* SystemViewPanel::build_stats_row() {
    auto* row = new QWidget(this);
    auto* grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    auto make_stat_card = [&](int col, const QString& label, const char* color, QLabel** caption_out) -> QLabel* {
        auto* card = new QFrame;
        card->setStyleSheet(QString("QFrame { background:%1;border:1px solid %2;padding:8px; }")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(8, 8, 8, 8);
        cvl->setSpacing(2);
        cvl->setAlignment(Qt::AlignCenter);

        auto* val = make_stat_value("--", color);
        cvl->addWidget(val);
        auto* caption = make_stat_label(label);
        cvl->addWidget(caption);
        if (caption_out)
            *caption_out = caption;

        grid->addWidget(card, 0, col);
        return val;
    };

    agents_count_ = make_stat_card(0, tr("AGENTS"), ui::colors::AMBER, &agents_caption_);
    tools_count_ = make_stat_card(1, tr("TOOLS"), ui::colors::CYAN, &tools_caption_);
    llms_count_ = make_stat_card(2, tr("LLMS"), ui::colors::SUCCESS, &llms_caption_);
    cache_count_ = make_stat_card(3, tr("CACHED"), ui::colors::INFO, &cache_caption_);

    return row;
}

QWidget* SystemViewPanel::build_llm_section() {
    auto* card = make_section_card(tr("CONFIGURED LLM PROVIDERS"), &llm_list_layout_, &llm_section_refresh_,
                                   tr("REFRESH"), &llm_section_title_);
    if (llm_section_refresh_) {
        connect(llm_section_refresh_, &QPushButton::clicked, this, [this]() {
            // Clear and reload LLM list
            while (llm_list_layout_->count() > 0) {
                auto* item = llm_list_layout_->takeAt(0);
                if (auto* w = item->widget())
                    w->deleteLater();
                delete item;
            }
            refresh_data();
        });
    }

    auto* placeholder = new QLabel(tr("Loading LLM providers..."));
    placeholder->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-style:italic;").arg(ui::colors::TEXT_TERTIARY()));
    llm_list_layout_->addWidget(placeholder);

    return card;
}

QWidget* SystemViewPanel::build_tools_section() {
    auto* card = make_section_card(tr("AVAILABLE TOOLS"), &tools_list_layout_, nullptr, {}, &tools_section_title_);

    auto* placeholder = new QLabel(tr("Loading tools..."));
    placeholder->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-style:italic;").arg(ui::colors::TEXT_TERTIARY()));
    tools_list_layout_->addWidget(placeholder);

    return card;
}

QWidget* SystemViewPanel::build_sysinfo_section() {
    auto* card = make_section_card(tr("SYSTEM INFO"), &features_layout_, nullptr, {}, &sysinfo_section_title_);

    auto* info_grid = new QGridLayout;
    info_grid->setSpacing(4);

    version_title_ = new QLabel(tr("VERSION"));
    version_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY()));
    version_label_ = new QLabel("--");
    version_label_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;").arg(ui::colors::CYAN()));
    info_grid->addWidget(version_title_, 0, 0);
    info_grid->addWidget(version_label_, 0, 1);

    framework_title_ = new QLabel(tr("FRAMEWORK"));
    framework_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY()));
    framework_label_ = new QLabel("--");
    framework_label_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;").arg(ui::colors::CYAN()));
    info_grid->addWidget(framework_title_, 1, 0);
    info_grid->addWidget(framework_label_, 1, 1);
    info_grid->setColumnStretch(1, 1);

    features_layout_->addLayout(info_grid);

    return card;
}

// ── Helper: clear all items from a layout ────────────────────────────────────

static void clear_layout(QLayout* layout) {
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (auto* w = item->widget())
            w->deleteLater();
        delete item;
    }
}

// ── Helper: CACHED tile ──────────────────────────────────────────────────────
// get_cache_stats() reports `agents_cached` and `total_entries`; the tile used
// to add a `response_cache_size` key that does not exist, so it always equalled
// the AGENTS tile.
static void set_cache_tile(QLabel* tile) {
    const QJsonObject stats = services::AgentService::instance().get_cache_stats();
    const int agents = stats["agents_cached"].toInt(0);
    const int entries = stats["total_entries"].toInt(0);
    tile->setText(QString::number(entries));
    tile->setToolTip(QCoreApplication::translate("SystemViewPanel", "Agents: %1  |  Cache entries: %2")
                         .arg(agents)
                         .arg(entries));
}

// ── LLM list population ───────────────────────────────────────────────────────

void SystemViewPanel::populate_llm_list() {
    clear_layout(llm_list_layout_);

    auto providers = LlmConfigRepository::instance().list_providers();
    if (!providers.is_ok()) {
        auto* err = new QLabel(tr("Failed to load LLM providers."));
        err->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(ui::colors::DANGER()));
        llm_list_layout_->addWidget(err);
        return;
    }

    const auto& list = providers.value();
    llms_count_->setText(QString::number(list.size()));

    if (list.isEmpty()) {
        auto* empty = new QLabel(tr("No LLM providers configured. Go to Settings → LLM to add one."));
        empty->setWordWrap(true);
        empty->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-style:italic;").arg(ui::colors::TEXT_TERTIARY()));
        llm_list_layout_->addWidget(empty);
        return;
    }

    for (const auto& p : list) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 2, 0, 2);
        hl->setSpacing(8);

        if (p.is_active) {
            auto* badge = new QLabel(tr("ACTIVE"));
            badge->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-6) + "px;font-weight:700;background:%2;padding:1px 5px;")
                                     .arg(ui::colors::SUCCESS(), ui::colors::BG_RAISED()));
            hl->addWidget(badge);
        }

        auto* name = new QLabel(p.provider.toUpper());
        name->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;font-weight:600;").arg(ui::colors::TEXT_PRIMARY()));
        hl->addWidget(name);

        auto* model = new QLabel(p.model);
        model->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(ui::colors::TEXT_SECONDARY()));
        hl->addWidget(model);

        hl->addStretch();

        const bool has_key = !p.api_key.isEmpty();
        auto* key_status = new QLabel(has_key ? tr("KEY SET") : tr("NO KEY"));
        key_status->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;")
                                      .arg(has_key ? ui::colors::SUCCESS() : ui::colors::DANGER()));
        hl->addWidget(key_status);

        llm_list_layout_->addWidget(row);
    }
}

// ── Tools list population ─────────────────────────────────────────────────────

void SystemViewPanel::populate_tools_list(const services::AgentToolsInfo& info) {
    clear_layout(tools_list_layout_);
    tools_count_->setText(QString::number(info.total_count));

    if (info.categories.isEmpty()) {
        auto* empty = new QLabel(tr("No tools loaded."));
        empty->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-style:italic;").arg(ui::colors::TEXT_TERTIARY()));
        tools_list_layout_->addWidget(empty);
        return;
    }

    for (const auto& cat : info.categories) {
        QJsonArray tools = info.tools[cat].toArray();

        auto* cat_lbl = new QLabel(QString("%1  (%2)").arg(cat.toUpper()).arg(tools.size()));
        cat_lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;letter-spacing:1px;padding-top:4px;")
                                   .arg(ui::colors::AMBER()));
        tools_list_layout_->addWidget(cat_lbl);

        QStringList names;
        for (const auto& t : tools)
            names << t.toString();

        auto* tools_lbl = new QLabel(names.join(", "));
        tools_lbl->setWordWrap(true);
        tools_lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;padding-left:8px;").arg(ui::colors::TEXT_PRIMARY()));
        tools_list_layout_->addWidget(tools_lbl);
    }
}

// ── Connections ──────────────────────────────────────────────────────────────

void SystemViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    // Agents count → update stat badge + cache
    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                agents_count_->setText(QString::number(agents.size()));
                set_cache_tile(cache_count_);
            });

    // Tools loaded → repopulate tools section
    connect(&svc, &services::AgentService::tools_loaded, this,
            [this](services::AgentToolsInfo info) { populate_tools_list(info); });

    // A failed tools / system-info call (e.g. a missing Python library) used to
    // leave "Loading tools..." and "--" on screen with no explanation.
    connect(&svc, &services::AgentService::error_occurred, this, [this](const QString& ctx, const QString& msg) {
        if (ctx == QLatin1String("list_tools")) {
            clear_layout(tools_list_layout_);
            tools_count_->setText("--");
            auto* err = new QLabel(tr("Could not load tools: %1").arg(msg));
            err->setWordWrap(true);
            err->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(ui::colors::DANGER()));
            tools_list_layout_->addWidget(err);
        } else if (ctx == QLatin1String("system_info")) {
            version_label_->setText("N/A");
            framework_label_->setText("N/A");
            // Keep the info grid at index 0; replace any feature badges with the error.
            while (features_layout_->count() > 1) {
                auto* item = features_layout_->takeAt(1);
                if (auto* w = item->widget())
                    w->deleteLater();
                delete item;
            }
            auto* err = new QLabel(tr("Could not load system info: %1").arg(msg));
            err->setWordWrap(true);
            err->setStyleSheet(
                QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;padding-top:8px;").arg(ui::colors::DANGER()));
            features_layout_->addWidget(err);
        }
    });

    // System info → fill version/framework/features
    connect(&svc, &services::AgentService::system_info_loaded, this, [this](services::AgentSystemInfo info) {
        version_label_->setText(info.version.isEmpty() ? "N/A" : info.version);
        framework_label_->setText(info.framework.isEmpty() ? "N/A" : info.framework);

        // Remove stale feature badges (keep the info_grid at index 0)
        while (features_layout_->count() > 1) {
            auto* item = features_layout_->takeAt(1);
            if (auto* w = item->widget())
                w->deleteLater();
            delete item;
        }

        if (!info.features.isEmpty()) {
            auto* feat_header = new QLabel(tr("FEATURES"));
            feat_header->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:600;"
                                               "letter-spacing:1px;padding-top:8px;")
                                           .arg(ui::colors::TEXT_TERTIARY()));
            features_layout_->addWidget(feat_header);

            // Wrapped grid: one non-wrapping row of ~17 badges was clipped by the
            // scroll area (horizontal scroll bar off) and forced the panel wide.
            auto* flow = new QWidget(this);
            auto* flow_layout = new QGridLayout(flow);
            flow_layout->setContentsMargins(0, 0, 0, 0);
            flow_layout->setHorizontalSpacing(4);
            flow_layout->setVerticalSpacing(4);
            constexpr int kBadgesPerRow = 5;

            int idx = 0;
            for (const auto& feat : info.features) {
                auto* badge = new QLabel(feat.toUpper().replace('_', ' '));
                badge->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;background:%2;padding:2px 6px;")
                                         .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED()));
                flow_layout->addWidget(badge, idx / kBadgesPerRow, idx % kBadgesPerRow);
                ++idx;
            }
            flow_layout->setColumnStretch(kBadgesPerRow, 1);
            features_layout_->addWidget(flow);
        }
    });
}

// ── Cross-panel public slots ──────────────────────────────────────────────────

void SystemViewPanel::on_agents_changed() {
    auto& svc = services::AgentService::instance();
    agents_count_->setText(QString::number(svc.cached_agent_count()));
    set_cache_tile(cache_count_);
}

void SystemViewPanel::on_llm_config_changed() {
    populate_llm_list();
}

void SystemViewPanel::on_tools_changed(const services::AgentToolsInfo& info) {
    populate_tools_list(info);
}

// ── Data refresh ─────────────────────────────────────────────────────────────

void SystemViewPanel::refresh_data() {
    auto& svc = services::AgentService::instance();

    // LLM providers (sync from DB)
    populate_llm_list();

    // Agent count (from cache)
    agents_count_->setText(QString::number(svc.cached_agent_count()));

    // Cache stats
    set_cache_tile(cache_count_);

    // Async: tools list + system info (results arrive via signals)
    svc.list_tools();
    svc.get_system_info();
}

// ── Visibility ───────────────────────────────────────────────────────────────

void SystemViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        refresh_data();
    }
}

// ── Re-translation ───────────────────────────────────────────────────────────

void SystemViewPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void SystemViewPanel::retranslateUi() {
    // Header.
    if (header_title_)
        header_title_->setText(tr("SYSTEM CAPABILITIES"));
    if (header_refresh_btn_)
        header_refresh_btn_->setText(tr("REFRESH"));

    // Stat card captions (values hold live data — not re-applied here).
    if (agents_caption_)
        agents_caption_->setText(tr("AGENTS"));
    if (tools_caption_)
        tools_caption_->setText(tr("TOOLS"));
    if (llms_caption_)
        llms_caption_->setText(tr("LLMS"));
    if (cache_caption_)
        cache_caption_->setText(tr("CACHED"));

    // Section card titles + the LLM section refresh action.
    if (llm_section_title_)
        llm_section_title_->setText(tr("CONFIGURED LLM PROVIDERS"));
    if (llm_section_refresh_)
        llm_section_refresh_->setText(tr("REFRESH"));
    if (tools_section_title_)
        tools_section_title_->setText(tr("AVAILABLE TOOLS"));
    if (sysinfo_section_title_)
        sysinfo_section_title_->setText(tr("SYSTEM INFO"));

    // System info field titles (values hold live data — not re-applied here).
    if (version_title_)
        version_title_->setText(tr("VERSION"));
    if (framework_title_)
        framework_title_->setText(tr("FRAMEWORK"));
}

} // namespace fincept::screens
