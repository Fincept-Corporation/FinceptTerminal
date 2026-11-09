// src/ui/widgets/PythonSetupChip.h
//
// Status-bar chip for the Python environment pipeline once the main window is
// up (early launch at "core ready", a background package sync, or an install
// started later), and the "install on demand" entry point.
//
//   idle        hidden
//   running     "PYTHON ENV 63%" + slim bar; tooltip lists every stage with its
//               live detail and elapsed time
//   failed      "PYTHON ENV FAILED · RETRY" — click re-runs the pipeline
//               (only the failed lanes run again; downloads are cached)
//   finished    "PYTHON ENV READY" for a few seconds, then hidden
//   no python   "PYTHON ENV · SET UP" (setup was skipped) — click installs the
//               essentials in the background after a confirmation
//   on demand   "PYTHON LIBS · INSTALL" once a feature hit a library from the
//               full set — click installs the full set in the background
//
// Pure view over PythonSetupManager; styled through objectName selectors set
// once in refresh_theme().
#pragma once

#include <QWidget>

class QEvent;
class QLabel;
class QMouseEvent;
class QProgressBar;

namespace fincept::ui {

class PythonSetupChip : public QWidget {
    Q_OBJECT
  public:
    explicit PythonSetupChip(QWidget* parent = nullptr);

  protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    enum class Mode { Hidden, Running, Failed, Done, OfferSetup, OfferFull };

    void refresh();
    void refresh_theme();
    void set_mode(Mode mode);
    void on_offer_clicked();

    Mode mode_ = Mode::Hidden;
    bool full_offer_ = false; // a feature needed a library from the (uninstalled) full set
    QLabel* text_ = nullptr;
    QProgressBar* bar_ = nullptr;
};

} // namespace fincept::ui
