#pragma once
#include "core/components/ComponentMeta.h"

#include <QFrame>
#include <QMouseEvent>

class QLabel;

namespace fincept::ui {

/// One tile in the Component Browser grid. Shows category strip, title,
/// description, and popularity stars (filled circles rendered via text
/// — no icon assets required). Emits signals on click / double-click, and
/// supports drag-out with MIME type application/x-fincept-screen-id so a
/// future "drop a card onto a dock area to open it there" flow can plug
/// in without reworking the card.
///
/// Sized for a 3-across grid at ~900px browser width. Keeps the card
/// compact so 30+ components can fit without scrolling on a 1080p display.
class ComponentCard : public QFrame {
    Q_OBJECT
  public:
    explicit ComponentCard(ComponentMeta meta, QWidget* parent = nullptr);

    const ComponentMeta& meta() const { return meta_; }
    bool is_selected() const { return selected_; }
    void set_selected(bool s);

  signals:
    void activated(const QString& id);       // double-click or Enter
    void selected_changed(const QString& id); // single click

  protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

  private:
    void build_ui();
    void restyle();

    ComponentMeta meta_;
    bool selected_ = false;
    bool hovered_ = false;

    QPoint press_pos_;
    bool pressed_ = false;
};

} // namespace fincept::ui
