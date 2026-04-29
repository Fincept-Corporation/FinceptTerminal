#pragma once

#include <QString>
#include <QWidget>

class QLabel;

namespace fincept::screens {

/// Lightweight placeholder tab — used for STAKE / MARKETS / ROADMAP slots
/// that ship in later phases. Renders the tab name, the phase label, and a
/// one-line description so users know what's coming and when.
///
/// Has no data subscriptions and no timers, so it's safe to construct
/// eagerly inside the tab widget.
class ComingSoonTab : public QWidget {
    Q_OBJECT
  public:
    explicit ComingSoonTab(const QString& tab_name,
                           const QString& phase_label,
                           const QString& description,
                           QWidget* parent = nullptr);
    ~ComingSoonTab() override;

  private:
    void apply_theme();

    QLabel* tab_name_label_ = nullptr;
    QLabel* phase_label_ = nullptr;
    QLabel* description_label_ = nullptr;
};

} // namespace fincept::screens
