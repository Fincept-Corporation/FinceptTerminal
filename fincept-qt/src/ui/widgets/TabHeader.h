#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::ui {

/// Reusable tab header matching Tauri TabHeader.
/// Orange accent bottom border, title | version, subtitle, optional right-side actions.
class TabHeader : public QWidget {
    Q_OBJECT
public:
    explicit TabHeader(const QString& title,
                       const QString& subtitle = {},
                       QWidget* parent = nullptr);

    void set_title(const QString& title);
    void set_subtitle(const QString& subtitle);

    /// Layout on the right side — add buttons/labels to it.
    QHBoxLayout* actions_layout() const { return actions_layout_; }

private:
    QLabel*      title_label_    = nullptr;
    QLabel*      version_label_  = nullptr;
    QLabel*      subtitle_label_ = nullptr;
    QHBoxLayout* actions_layout_ = nullptr;
};

} // namespace fincept::ui
