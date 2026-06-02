#pragma once
#include <QEvent>
#include <QList>
#include <QWidget>

class QLabel;
class QPushButton;

namespace fincept::screens {

/// About & legal information — version, license, contact, resources.
class AboutScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AboutScreen(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    // Version panel
    QLabel* version_header_ = nullptr;
    QLabel* app_name_ = nullptr;
    QLabel* app_subtitle_ = nullptr;
    QPushButton* check_btn_ = nullptr;
    bool check_in_progress_ = false;
    QLabel* copyright_ = nullptr;

    // License panels
    QLabel* oss_header_ = nullptr;
    QList<QLabel*> oss_bullets_;
    QLabel* commercial_header_ = nullptr;
    QList<QLabel*> commercial_bullets_;

    // Diagnostics
    QLabel* diag_header_ = nullptr;
    QLabel* crash_dumps_label_ = nullptr;
    QPushButton* open_folder_btn_ = nullptr;

    // Trademarks
    QLabel* trademarks_header_ = nullptr;
    QLabel* trademarks_desc_ = nullptr;
    QLabel* trademarks_perm_ = nullptr;

    // Resources
    QLabel* resources_header_ = nullptr;
    QList<QPushButton*> resource_btns_;

    // Contact
    QLabel* contact_header_ = nullptr;
    QList<QLabel*> contact_labels_;
};

} // namespace fincept::screens
