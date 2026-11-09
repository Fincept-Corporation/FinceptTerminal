// src/ui/widgets/PythonSetupBanner.h
//
// Main-window strip (between the tab bar and the dock area) that tells the user
// the essential Python environment is missing and why data is not loading —
// shown when first-run setup was skipped, or failed before the essentials were in.
//
//   offer     "PYTHON NOT SET UP" + why + INSTALL ESSENTIALS (background install)
//   running   "INSTALLING PYTHON ESSENTIALS 42%" (essentials not in yet)
//   failed    "PYTHON SETUP FAILED" + RETRY
//   hidden    essentials installed, or dismissed for this session (a new
//             failure brings it back)
//
// Pure view over PythonSetupManager; styled once through objectName selectors.
#pragma once

#include <QWidget>

class QEvent;
class QLabel;
class QPushButton;

namespace fincept::ui {

class PythonSetupBanner : public QWidget {
    Q_OBJECT
  public:
    explicit PythonSetupBanner(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    enum class Mode { Hidden, Offer, Running, Failed };

    void refresh();
    void refresh_theme();
    void set_mode(Mode mode);
    void on_action_clicked();

    Mode mode_ = Mode::Hidden;
    bool dismissed_ = false;
    QLabel* title_ = nullptr;
    QLabel* body_ = nullptr;
    QPushButton* action_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;
};

} // namespace fincept::ui
