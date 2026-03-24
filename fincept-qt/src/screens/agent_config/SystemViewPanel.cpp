// src/screens/agent_config/SystemViewPanel.cpp
#include "screens/agent_config/SystemViewPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QScrollArea>
#include <QShowEvent>

namespace fincept::screens {

// ── Helpers ──────────────────────────────────────────────────────────────────

static QLabel* make_stat_value(const QString& text, const char* color) {
    auto* lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color:%1;font-size:22px;font-weight:700;").arg(color));
    return lbl;
}

static QLabel* make_stat_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:600;letter-spacing:1px;padding-top:2px;")
                           .arg(ui::colors::TEXT_TERTIARY));
    return lbl;
}

static QWidget* make_section_card(const QString& title, QVBoxLayout** content_layout,
                                  QPushButton** action_btn = nullptr, const QString& action_text = {}) {
    auto* card = new QFrame;
    card->setStyleSheet(
        QString("QFrame { background:%1;border:1px solid %2; }").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* header = new QWidget;
    header->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hdr_layout = new QHBoxLayout(header);
    hdr_layout->setContentsMargins(12, 6, 12, 6);

    auto* title_lbl = new QLabel(title);
    title_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    hdr_layout->addWidget(title_lbl);
    hdr_layout->addStretch();

    if (action_btn && !action_text.isEmpty()) {
        *action_btn = new QPushButton(action_text);
        (*action_btn)->setCursor(Qt::PointingHandCursor);
        (*action_btn)
            ->setStyleSheet(QString("QPushButton { background:transparent;color:%1;border:1px solid %2;"
                                    "padding:2px 8px;font-size:9px;font-weight:600; }"
                                    "QPushButton:hover { background:%2; }")
                                .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED));
        hdr_layout->addWidget(*action_btn);
    }

    vl->addWidget(header);

    // Content area
    auto* content = new QWidget;
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
}

// ── UI construction ──────────────────────────────────────────────────────────

void SystemViewPanel::build_ui() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { background:%1;border:none; }"
                                  "QScrollBar:vertical { background:%1;width:6px; }"
                                  "QScrollBar::handle:vertical { background:%2;min-height:30px; }")
                              .arg(ui::colors::BG_BASE, ui::colors::BORDER_BRIGHT));

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(12);

    // Header
    auto* header_row = new QHBoxLayout;
    auto* title = new QLabel("SYSTEM CAPABILITIES");
    title->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;letter-spacing:2px;").arg(ui::colors::AMBER));
    header_row->addWidget(title);
    header_row->addStretch();

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setStyleSheet(QString("QPushButton { background:transparent;color:%1;border:1px solid %2;"
                                       "padding:4px 12px;font-size:10px;font-weight:600;letter-spacing:1px; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED));
    connect(refresh_btn, &QPushButton::clicked, this, [this]() {
        data_loaded_ = false;
        refresh_data();
    });
    header_row->addWidget(refresh_btn);
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
    auto* row = new QWidget;
    auto* grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    auto make_stat_card = [&](int col, const QString& label, const char* color) -> QLabel* {
        auto* card = new QFrame;
        card->setStyleSheet(QString("QFrame { background:%1;border:1px solid %2;padding:8px; }")
                                .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(8, 8, 8, 8);
        cvl->setSpacing(2);
        cvl->setAlignment(Qt::AlignCenter);

        auto* val = make_stat_value("--", color);
        cvl->addWidget(val);
        cvl->addWidget(make_stat_label(label));

        grid->addWidget(card, 0, col);
        return val;
    };

    agents_count_ = make_stat_card(0, "AGENTS", ui::colors::AMBER);
    tools_count_ = make_stat_card(1, "TOOLS", ui::colors::CYAN);
    llms_count_ = make_stat_card(2, "LLMS", ui::colors::POSITIVE);
    cache_count_ = make_stat_card(3, "CACHED", ui::colors::INFO);

    return row;
}

QWidget* SystemViewPanel::build_llm_section() {
    QPushButton* refresh = nullptr;
    auto* card = make_section_card("CONFIGURED LLM PROVIDERS", &llm_list_layout_, &refresh, "REFRESH");
    if (refresh) {
        connect(refresh, &QPushButton::clicked, this, [this]() {
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

    auto* placeholder = new QLabel("Loading LLM providers...");
    placeholder->setStyleSheet(QString("color:%1;font-size:11px;font-style:italic;").arg(ui::colors::TEXT_TERTIARY));
    llm_list_layout_->addWidget(placeholder);

    return card;
}

QWidget* SystemViewPanel::build_tools_section() {
    auto* card = make_section_card("AVAILABLE TOOLS", &tools_list_layout_);

    auto* placeholder = new QLabel("Loading tools...");
    placeholder->setStyleSheet(QString("color:%1;font-size:11px;font-style:italic;").arg(ui::colors::TEXT_TERTIARY));
    tools_list_layout_->addWidget(placeholder);

    return card;
}

QWidget* SystemViewPanel::build_sysinfo_section() {
    auto* card = make_section_card("SYSTEM INFO", &features_layout_);

    auto* info_grid = new QGridLayout;
    info_grid->setSpacing(4);

    auto* ver_title = new QLabel("VERSION");
    ver_title->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:600;letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY));
    version_label_ = new QLabel("--");
    version_label_->setStyleSheet(QString("color:%1;font-size:12px;").arg(ui::colors::CYAN));
    info_grid->addWidget(ver_title, 0, 0);
    info_grid->addWidget(version_label_, 0, 1);

    auto* fw_title = new QLabel("FRAMEWORK");
    fw_title->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:600;letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY));
    framework_label_ = new QLabel("--");
    framework_label_->setStyleSheet(QString("color:%1;font-size:12px;").arg(ui::colors::CYAN));
    info_grid->addWidget(fw_title, 1, 0);
    info_grid->addWidget(framework_label_, 1, 1);
    info_grid->setColumnStretch(1, 1);

    features_layout_->addLayout(info_grid);

    return card;
}

// ── Connections ──────────────────────────────────────────────────────────────

void SystemViewPanel::setup_connections() {
    // Agents count
    connect(&services::AgentService::instance(), &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory>) {
                agents_count_->setText(QString::number(agents.size()));
                cache_count_->setText(QString::number(services::AgentService::instance().cached_agent_count()));
            });

    // Tools
    connect(&services::AgentService::instance(), &services::AgentService::tools_loaded, this,
            [this](services::AgentToolsInfo info) {
                tools_count_->setText(QString::number(info.total_count));

                // Clear and populate tools list
                while (tools_list_layout_->count() > 0) {
                    auto* item = tools_list_layout_->takeAt(0);
                    if (auto* w = item->widget())
                        w->deleteLater();
                    delete item;
                }

                for (const auto& cat : info.categories) {
                    QJsonArray tools = info.tools[cat].toArray();
                    auto* cat_lbl = new QLabel(QString("%1 (%2)").arg(cat.toUpper()).arg(tools.size()));
                    cat_lbl->setStyleSheet(
                        QString("color:%1;font-size:10px;font-weight:600;letter-spacing:1px;padding-top:4px;")
                            .arg(ui::colors::AMBER));
                    tools_list_layout_->addWidget(cat_lbl);

                    QString tool_names;
                    for (const auto& t : tools) {
                        if (!tool_names.isEmpty())
                            tool_names += ", ";
                        tool_names += t.toString();
                    }
                    auto* tools_lbl = new QLabel(tool_names);
                    tools_lbl->setWordWrap(true);
                    tools_lbl->setStyleSheet(
                        QString("color:%1;font-size:11px;padding-left:8px;").arg(ui::colors::TEXT_PRIMARY));
                    tools_list_layout_->addWidget(tools_lbl);
                }
            });

    // System info
    connect(&services::AgentService::instance(), &services::AgentService::system_info_loaded, this,
            [this](services::AgentSystemInfo info) {
                version_label_->setText(info.version.isEmpty() ? "N/A" : info.version);
                framework_label_->setText(info.framework.isEmpty() ? "N/A" : info.framework);

                // Features
                if (!info.features.isEmpty()) {
                    auto* feat_header = new QLabel("FEATURES");
                    feat_header->setStyleSheet(
                        QString("color:%1;font-size:9px;font-weight:600;letter-spacing:1px;padding-top:8px;")
                            .arg(ui::colors::TEXT_TERTIARY));
                    features_layout_->addWidget(feat_header);

                    auto* flow = new QWidget;
                    auto* flow_layout = new QHBoxLayout(flow);
                    flow_layout->setContentsMargins(0, 0, 0, 0);
                    flow_layout->setSpacing(4);

                    for (const auto& feat : info.features) {
                        auto* badge = new QLabel(feat.toUpper().replace('_', ' '));
                        badge->setStyleSheet(
                            QString("color:%1;font-size:9px;background:%2;padding:2px 6px;border-radius:2px;")
                                .arg(ui::colors::TEXT_PRIMARY, ui::colors::BG_RAISED));
                        flow_layout->addWidget(badge);
                    }
                    flow_layout->addStretch();
                    features_layout_->addWidget(flow);
                }
            });
}

// ── Data refresh ─────────────────────────────────────────────────────────────

void SystemViewPanel::refresh_data() {
    auto& svc = services::AgentService::instance();

    // Load LLM providers from repository
    auto providers = LlmConfigRepository::instance().list_providers();
    if (providers.is_ok()) {
        while (llm_list_layout_->count() > 0) {
            auto* item = llm_list_layout_->takeAt(0);
            if (auto* w = item->widget())
                w->deleteLater();
            delete item;
        }

        llms_count_->setText(QString::number(providers.value().size()));

        for (const auto& p : providers.value()) {
            auto* row = new QWidget;
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 2, 0, 2);
            hl->setSpacing(8);

            if (p.is_active) {
                auto* badge = new QLabel("ACTIVE");
                badge->setStyleSheet(QString("color:%1;font-size:8px;font-weight:700;background:%2;"
                                             "padding:1px 4px;border-radius:2px;")
                                         .arg(ui::colors::POSITIVE, QString("%1").arg(ui::colors::BG_RAISED)));
                hl->addWidget(badge);
            }

            auto* name = new QLabel(p.provider.toUpper());
            name->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;").arg(ui::colors::TEXT_PRIMARY));
            hl->addWidget(name);

            auto* model = new QLabel(p.model);
            model->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY));
            hl->addWidget(model);

            hl->addStretch();

            auto* key_status = new QLabel(p.api_key.isEmpty() ? "NO KEY" : "KEY SET");
            key_status->setStyleSheet(QString("color:%1;font-size:9px;font-weight:600;")
                                          .arg(p.api_key.isEmpty() ? ui::colors::NEGATIVE : ui::colors::POSITIVE));
            hl->addWidget(key_status);

            llm_list_layout_->addWidget(row);
        }

        if (providers.value().isEmpty()) {
            auto* empty = new QLabel("No LLM providers configured. Go to Settings to add one.");
            empty->setStyleSheet(QString("color:%1;font-size:11px;font-style:italic;").arg(ui::colors::TEXT_TERTIARY));
            llm_list_layout_->addWidget(empty);
        }
    }

    // Trigger async loads
    svc.get_system_info();
    svc.list_tools();
}

// ── Visibility ───────────────────────────────────────────────────────────────

void SystemViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        refresh_data();
    }
}

} // namespace fincept::screens
