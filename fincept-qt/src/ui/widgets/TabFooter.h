#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QString>
#include <QVector>

namespace fincept::ui {

/// Info item displayed in the footer's left section.
struct FooterInfo {
    QString label;
    QString value;    // optional — if empty, only label is shown
    QString color;    // optional — defaults to white
};

/// Reusable tab footer matching Tauri TabFooter.
/// Orange accent top border: "FINCEPT TERMINAL v4.0.0 | TAB_NAME | info…" left, status right.
class TabFooter : public QWidget {
    Q_OBJECT
public:
    explicit TabFooter(const QString& tab_name, QWidget* parent = nullptr);

    void set_tab_name(const QString& name);
    void set_left_info(const QVector<FooterInfo>& items);
    void set_status(const QString& status_text);

private:
    QLabel*      brand_label_  = nullptr;
    QLabel*      tab_label_    = nullptr;
    QHBoxLayout* info_layout_  = nullptr;
    QLabel*      status_label_ = nullptr;

    void rebuild_info(const QVector<FooterInfo>& items);
};

} // namespace fincept::ui
