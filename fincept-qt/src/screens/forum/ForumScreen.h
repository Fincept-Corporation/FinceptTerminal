// src/screens/forum/ForumScreen.h
#pragma once
#include "screens/common/IStatefulScreen.h"
#include "services/forum/ForumModels.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

class ForumFeedPanel;
class ForumThreadPanel;
class ForumSidebarPanel;

class ForumScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit ForumScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "forum"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_category_selected(int category_id, const QString& name, const QString& color);
    void on_post_selected(const services::ForumPost& post);
    void on_search(const QString& query);
    void on_trending();
    void on_new_post_requested();
    void navigate_back_to_feed();

  private:
    void build_ui();
    void load_initial_data();
    void show_new_post_dialog(int category_id);
    void show_edit_profile_dialog(const services::ForumProfile& profile);

    // Layout
    QSplitter* splitter_ = nullptr;
    ForumSidebarPanel* sidebar_ = nullptr;

    // Main content: stacked feed / thread
    QStackedWidget* main_stack_ = nullptr;
    ForumFeedPanel* feed_ = nullptr;
    ForumThreadPanel* thread_ = nullptr;

    // State
    QVector<services::ForumCategory> categories_;
    int active_category_id_ = 0;
    QString active_category_name_;
    QString active_category_color_;
    QString current_detail_uuid_;
    bool initial_load_done_ = false;
};

} // namespace fincept::screens
