#pragma once
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::ui {

class PaginationBar : public QWidget {
    Q_OBJECT
  public:
    explicit PaginationBar(QWidget* parent = nullptr);

    void set_total(int total_rows);
    int page_size() const { return page_size_; }
    int current_page() const { return current_page_; }
    int total_pages() const;
    int offset() const { return (current_page_ - 1) * page_size_; }

  signals:
    void page_changed(int offset, int limit);

  private:
    void update_controls();

    QPushButton* btn_first_ = nullptr;
    QPushButton* btn_prev_ = nullptr;
    QLabel* page_label_ = nullptr;
    QPushButton* btn_next_ = nullptr;
    QPushButton* btn_last_ = nullptr;
    QLabel* status_label_ = nullptr;
    QComboBox* size_combo_ = nullptr;

    int total_rows_ = 0;
    int page_size_ = 50;
    int current_page_ = 1;
};

} // namespace fincept::ui
