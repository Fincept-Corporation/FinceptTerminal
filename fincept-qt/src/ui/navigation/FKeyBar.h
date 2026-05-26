#pragma once
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVector>
#include <QWidget>

namespace fincept::ui {

/// Scrollable tab bar.
/// Tabs: Dashboard, Markets, Portfolio, News, Report Builder,
///       Settings, Profile, About, Support
///
/// Internationalised: tab labels retranslate via tr() on
/// QEvent::LanguageChange. Tab IDs stay stable (Latin keys) so screen
/// routing isn't affected by language switching.
class TabBar : public QWidget {
    Q_OBJECT
  public:
    explicit TabBar(QWidget* parent = nullptr);

    void set_active(const QString& tab_id);
    QString active_tab() const { return active_id_; }

  signals:
    void tab_changed(const QString& tab_id);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    struct TabDef {
        QString id;
        QString source_label;  // English source key (used for tr() lookup)
    };

    QHBoxLayout* tab_layout_ = nullptr;
    QVector<QPushButton*> tab_buttons_;
    QVector<TabDef>       tab_defs_;
    QString active_id_ = "dashboard";

    void add_tab(const TabDef& def);
    void refresh_theme();
    void retranslateUi();
    void update_styles();
};

} // namespace fincept::ui
