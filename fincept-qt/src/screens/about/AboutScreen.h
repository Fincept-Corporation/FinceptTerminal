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
    /// Show the "update available" line when UpdateService has found a newer
    /// release (its update_available()/latest_version() accessors were unused).
    void refresh_update_status();

    // Version panel
    QLabel* version_header_ = nullptr;
    QLabel* app_name_ = nullptr;
    QLabel* app_subtitle_ = nullptr;
    QPushButton* check_btn_ = nullptr;
    QLabel* update_status_ = nullptr;
    bool check_in_progress_ = false;
    QLabel* copyright_ = nullptr;

    // Edition panels — open-source licence vs the Enterprise product
    QLabel* oss_header_ = nullptr;
    QList<QLabel*> oss_bullets_;
    QLabel* enterprise_header_ = nullptr;
    QList<QLabel*> enterprise_bullets_;

    // Diagnostics
    QLabel* diag_header_ = nullptr;
    QLabel* crash_dumps_label_ = nullptr;
    QPushButton* open_folder_btn_ = nullptr;
    QPushButton* copy_info_btn_ = nullptr;

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
