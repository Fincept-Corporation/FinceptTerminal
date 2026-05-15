// src/screens/excel/ExcelScreen.h
// Full-featured Excel/Spreadsheet screen with multi-sheet tabs,
// formula bar, import/export (.xlsx), snapshots, and dark theme.
#pragma once

#include "screens/common/IStatefulScreen.h"

#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class SpreadsheetWidget;

class ExcelScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit ExcelScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "excel"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_import();
    void on_export();
    void on_add_sheet();
    void on_delete_sheet();
    void on_rename_sheet();
    void on_tab_changed(int index);
    void on_export_csv();

  private:
    void build_ui();
    QWidget* build_toolbar();
    void update_status();
    SpreadsheetWidget* current_sheet() const;
    QString generate_sheet_name() const;

    QTabWidget* sheet_tabs_ = nullptr;
    QLabel* status_label_ = nullptr;
    QString file_name_ = "Untitled.xlsx";
    QString file_path_;
    int sheet_counter_ = 1;
};

} // namespace fincept::screens
