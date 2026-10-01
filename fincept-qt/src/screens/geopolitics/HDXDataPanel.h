// src/screens/geopolitics/HDXDataPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// HDX Humanitarian Data Exchange panel — search, browse, and explore datasets.
class HDXDataPanel : public QWidget {
    Q_OBJECT
  public:
    explicit HDXDataPanel(QWidget* parent = nullptr);

    /// Jump to the Explorer view and search HDX for `country` (used by the
    /// Relations tab). Safe to call before the panel has ever been shown.
    void explore_country(const QString& country);
    /// Same, for a topic keyword ("displacement", "food security", ...).
    void explore_topic(const QString& topic);

  protected:
    void changeEvent(QEvent* event) override;
    /// First show triggers the initial HDX fetch. Doing it in the constructor
    /// spawned a Python process whenever the Geopolitics screen opened, even
    /// when the user never visited this tab.
    void showEvent(QShowEvent* event) override;

  private slots:
    void on_hdx_results(const QString& context, QVector<fincept::services::geo::HDXDataset> datasets);
    void on_error(const QString& context, const QString& message);
    void on_view_changed(int index);

  private:
    void build_ui();
    void connect_service();
    void populate_table(const QVector<fincept::services::geo::HDXDataset>& datasets);
    void show_loading(bool on);
    /// Replace the table with a centred message (empty result, error, prompt).
    void show_message(const QString& text);
    /// Highlight the active view button and show/hide the explorer bar.
    void set_active_view(int index);
    /// Show cached results for `index` or issue its fetch (once).
    void load_view(int index);
    void retranslateUi();

    // View tab buttons
    QVector<QPushButton*> view_buttons_;
    int active_view_ = 0;

    // Search
    QLineEdit* search_edit_ = nullptr;

    // Datasets table + loading overlay
    QTableWidget* datasets_table_ = nullptr;
    QLabel* loading_label_ = nullptr;

    // Explorer filter bar (shown only on Explorer tab)
    QWidget* explorer_bar_ = nullptr;
    QComboBox* country_combo_ = nullptr;
    QComboBox* topic_combo_ = nullptr;

    // Stats badge
    QLabel* dataset_count_ = nullptr;

    // Static text widgets (cached for retranslateUi)
    QLabel* title_lbl_ = nullptr;
    QLabel* country_lbl_ = nullptr;
    QLabel* topic_lbl_ = nullptr;
    QPushButton* explore_btn_ = nullptr;
    /// HDX source + licence attribution footer (required by HDX terms).
    QLabel* attribution_lbl_ = nullptr;
    // Fixed English view-tab labels, re-applied in retranslateUi (upper-cased).
    QStringList view_labels_;

    // Per-view result cache (avoids re-fetching on tab switch). The Explorer has
    // its own slot: sharing cache_datasets_ made a country search overwrite what
    // the Datasets tab showed.
    QVector<fincept::services::geo::HDXDataset> cache_conflicts_;
    QVector<fincept::services::geo::HDXDataset> cache_humanitarian_;
    QVector<fincept::services::geo::HDXDataset> cache_explorer_;
    QVector<fincept::services::geo::HDXDataset> cache_datasets_;
    // Result contexts ("conflicts", "humanitarian", ...) with a request in flight,
    // so flipping tabs while a load is pending doesn't spawn a duplicate.
    QSet<QString> inflight_;
    bool shown_once_ = false;
};

} // namespace fincept::screens
