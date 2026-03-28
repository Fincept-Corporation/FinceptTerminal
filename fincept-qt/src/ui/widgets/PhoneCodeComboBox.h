#pragma once
#include <QComboBox>
#include <QLabel>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QString>

namespace fincept::ui {

/// Searchable country dial-code selector using QStandardItemModel.
/// Dropdown shows: flag + country name + dial code (searchable by any field).
/// Closed field shows compact: flag + dial code only (e.g. "🇺🇸  +1").
/// Use dial_code() to retrieve the E.164 prefix (e.g. "+1").
class PhoneCodeComboBox : public QComboBox {
    Q_OBJECT
  public:
    explicit PhoneCodeComboBox(QWidget* parent = nullptr);

    QString dial_code() const;
    void set_dial_code(const QString& code);

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    QStandardItemModel* model_ = nullptr;
    QLabel* arrow_lbl_ = nullptr;
    void populate();
    void sync_display(int index);
};

} // namespace fincept::ui
