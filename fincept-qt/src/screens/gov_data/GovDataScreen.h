// src/screens/gov_data/GovDataScreen.h
// Main Government Data screen — provider sidebar + active provider panel
#pragma once

#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class GovDataScreen : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_provider_selected(int row);

  private:
    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_header();
    void activate_provider(int index);

    QListWidget* provider_list_ = nullptr;
    QStackedWidget* panel_stack_ = nullptr;
    QLabel* header_title_ = nullptr;
    QLabel* header_subtitle_ = nullptr;
    QWidget* header_bar_ = nullptr;

    int active_index_ = -1;
    bool initial_load_done_ = false;
};

} // namespace fincept::screens
