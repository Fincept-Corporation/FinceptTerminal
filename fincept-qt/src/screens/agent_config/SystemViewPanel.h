// src/screens/agent_config/SystemViewPanel.h
#pragma once
#include "services/agents/AgentTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// SYSTEM view — live capabilities dashboard: agent count, LLM profiles,
/// tools registry, cache stats, and framework info.
class SystemViewPanel : public QWidget {
    Q_OBJECT
  public:
    explicit SystemViewPanel(QWidget* parent = nullptr);

  public slots:
    /// Called by AgentConfigScreen when agents/LLM profiles change elsewhere.
    void on_agents_changed();
    void on_llm_config_changed();
    void on_tools_changed(const services::AgentToolsInfo& info);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    void setup_connections();
    void refresh_data();
    void populate_llm_list();
    void populate_tools_list(const services::AgentToolsInfo& info);

    QWidget* build_stats_row();
    QWidget* build_llm_section();
    QWidget* build_tools_section();
    QWidget* build_sysinfo_section();

    // Stats labels
    QLabel* agents_count_ = nullptr;
    QLabel* tools_count_  = nullptr;
    QLabel* llms_count_   = nullptr;
    QLabel* cache_count_  = nullptr;

    // LLM section
    QVBoxLayout* llm_list_layout_   = nullptr;

    // Tools section
    QVBoxLayout* tools_list_layout_ = nullptr;

    // System info
    QLabel*      version_label_   = nullptr;
    QLabel*      framework_label_ = nullptr;
    QVBoxLayout* features_layout_ = nullptr;

    bool data_loaded_ = false;
};

} // namespace fincept::screens
