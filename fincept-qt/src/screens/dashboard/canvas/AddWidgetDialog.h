#pragma once
#include "screens/dashboard/canvas/WidgetRegistry.h"

#include <QButtonGroup>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVector>

namespace fincept::screens {

/// Modal dialog for choosing a widget to add to the dashboard canvas.
/// Dynamically pulls categories from WidgetRegistry — no hardcoded list.
class AddWidgetDialog : public QDialog {
    Q_OBJECT
  public:
    explicit AddWidgetDialog(QWidget* parent = nullptr);

  signals:
    void widget_selected(const QString& type_id);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void filter_changed(const QString& text);
    void category_clicked(const QString& category);
    void card_clicked(const QString& type_id);
    void confirm();

  private:
    void build_category_bar(QVBoxLayout* root);
    void populate_cards(const QString& filter = {}, const QString& category = {});
    static QString accent_for_category(const QString& category);
    static QString icon_for_widget(const QString& type_id);
    void retranslateUi();

    QLabel* title_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLineEdit* search_bar_ = nullptr;
    QWidget* card_container_ = nullptr;
    QGridLayout* card_grid_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QButtonGroup* cat_group_ = nullptr;
    QVector<QPushButton*> cat_buttons_;
    QVector<QString>      cat_source_keys_;
    QString selected_id_;
    QString active_category_;
};

} // namespace fincept::screens
