#pragma once

#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

// Equity Research → Sentiment tab. Shows the terminal's own, keyless sentiment
// (news headline NLP blended with price momentum, plus optional Adanos when a
// key is configured) via EquitySentimentService.
class EquitySentimentTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquitySentimentTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_sentiment_loaded(QString symbol, fincept::services::equity::EquitySentimentSnapshot snapshot);

  private:
    void build_ui();
    void retranslateUi();
    void populate(const fincept::services::equity::EquitySentimentSnapshot& snapshot);
    static void clear_layout(QLayout* layout);

    QString current_symbol_;

    // Header
    QLabel* title_lbl_ = nullptr;
    QLabel* company_label_ = nullptr;
    QLabel* caption_label_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // Status / content
    QLabel* status_label_ = nullptr;
    QWidget* content_widget_ = nullptr;

    // Net score badge
    QLabel* net_label_ = nullptr;
    QLabel* net_score_label_ = nullptr;
    QLabel* net_conf_label_ = nullptr;

    // Distribution bar
    QLabel* dist_counts_label_ = nullptr;
    QHBoxLayout* dist_layout_ = nullptr;

    // Dynamic sections
    QVBoxLayout* sources_layout_ = nullptr;
    QVBoxLayout* articles_layout_ = nullptr;

    // Panel titles (for retranslation)
    QLabel* summary_title_lbl_ = nullptr;
    QLabel* sources_title_lbl_ = nullptr;
    QLabel* articles_title_lbl_ = nullptr;

    fincept::services::equity::EquitySentimentSnapshot cached_snapshot_;
    bool snapshot_loaded_ = false;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
