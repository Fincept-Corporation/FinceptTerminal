#pragma once
#include <QWidget>

class QScrollArea;

namespace fincept::screens {

/// Contact Us — email, phone, office address, quick actions.
///
/// Internationalised: all visible strings flow through tr(). The page contents
/// are built once in the constructor and rebuilt on QEvent::LanguageChange —
/// every label/button is a local in build_page(), so caching dozens of member
/// pointers would dominate the file. Rebuilding via the scroll area is cheap
/// (static content, no live data) and keeps the source readable.
class ContactScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ContactScreen(QWidget* parent = nullptr);

  signals:
    void navigate_back();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    QWidget* build_page();
    QScrollArea* scroll_ = nullptr;
};

} // namespace fincept::screens
