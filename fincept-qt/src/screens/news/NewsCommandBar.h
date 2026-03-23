#pragma once
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

/// Bloomberg-style command bar with search, category pills, time range,
/// sort toggle, view mode, alert count, and refresh button.
class NewsCommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit NewsCommandBar(QWidget* parent = nullptr);

    void set_active_category(const QString& cat);
    void set_loading(bool loading);
    void set_loading_progress(int done, int total);
    void set_article_count(int count);
    void set_alert_count(int count);
    void set_unseen_count(int count);

    void show_summary(const QString& summary);
    void hide_summary();
    void set_summarizing(bool busy);

  signals:
    void category_changed(const QString& category);
    void time_range_changed(const QString& range);
    void sort_changed(const QString& sort);
    void view_mode_changed(const QString& mode);
    void search_changed(const QString& query);
    void refresh_clicked();
    void summarize_clicked();
    void rtl_toggled();
    void variant_changed(const QString& variant);
    void language_filter_changed(const QString& lang);

  private:
    QPushButton* make_pill(const QString& text, const QString& value, QHBoxLayout* layout);
    void update_pill_group(const QVector<QPushButton*>& btns, const QString& active_value);

    QLineEdit* search_input_ = nullptr;
    QVector<QPushButton*> category_btns_;
    QVector<QPushButton*> time_btns_;
    QPushButton* sort_relevance_ = nullptr;
    QPushButton* sort_newest_ = nullptr;
    QPushButton* view_wire_ = nullptr;
    QPushButton* view_clusters_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* summarize_btn_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* alert_label_ = nullptr;
    QLabel* unseen_label_ = nullptr;
    QLabel* progress_label_ = nullptr;

    QComboBox* variant_combo_ = nullptr;
    QComboBox* lang_filter_combo_ = nullptr;

    QString active_category_ = "ALL";
    QString active_time_ = "24H";
    QString active_sort_ = "RELEVANCE";
    QString active_view_ = "WIRE";
};

} // namespace fincept::screens
