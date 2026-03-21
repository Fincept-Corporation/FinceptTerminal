#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QVector>

namespace fincept::screens {

/// Top filter/search bar — F-key categories, time range, view mode, search, refresh.
class NewsFilterBar : public QWidget {
    Q_OBJECT
public:
    explicit NewsFilterBar(QWidget* parent = nullptr);

    void set_active_category(const QString& cat);
    void set_loading(bool loading);
    void set_article_count(int count);
    void set_alert_count(int count);

signals:
    void category_changed(const QString& category);
    void time_range_changed(const QString& range);
    void view_mode_changed(const QString& mode);
    void search_changed(const QString& query);
    void refresh_clicked();

private:
    QPushButton* make_filter_btn(const QString& label, const QString& id);
    QPushButton* make_toggle_btn(const QString& label, const QString& id);

    QVector<QPushButton*> category_btns_;
    QVector<QPushButton*> time_btns_;
    QVector<QPushButton*> view_btns_;
    QLineEdit*   search_input_ = nullptr;
    QPushButton* refresh_btn_  = nullptr;
    QLabel*      count_label_  = nullptr;
    QLabel*      alert_label_  = nullptr;
    QLabel*      live_dot_     = nullptr;

    QString active_category_ = "ALL";
    QString active_time_     = "24H";
    QString active_view_     = "WIRE";

    void update_btn_styles(QVector<QPushButton*>& btns, const QString& active_id);
};

} // namespace fincept::screens
