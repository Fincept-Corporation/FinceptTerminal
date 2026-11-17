#pragma once
// Models behind the Data Mapping tab:
//   MappingListModel — saved mappings with their last run (status, rows, cell errors).
//   FieldGridModel   — the mapping grid: one row per target field (target, type, source column,
//                      format, scale, default, required) plus the live check of the preview
//                      (first mapped value, cells ok / empty / failed with the first reason).
//   FieldGridDelegate — combo editors for type / source column / format.

#include "services/data_mapping/MappingEngine.h"
#include "services/data_mapping/MappingSpec.h"
#include "storage/repositories/DataMappingRepository.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QVector>

namespace fincept::screens::datamapping {

class MappingListModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int { ColName = 0, ColSchema, ColRun, ColCount };
    enum Role : int { IdRole = Qt::UserRole + 1, SortRole };

    explicit MappingListModel(QObject* parent = nullptr);
    void set_mappings(const QVector<fincept::DataMapping>& rows);
    const fincept::DataMapping* find(const QString& id) const;
    int row_of(const QString& id) const;
    void refresh_colours();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
    QVector<fincept::DataMapping> rows_;
};

class FieldGridModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int { ColTarget = 0, ColType, ColSource, ColFormat, ColScale, ColDefault, ColRequired, ColSample, ColCheck, ColCount };

    explicit FieldGridModel(QObject* parent = nullptr);

    void set_fields(const QVector<fincept::services::datamapping::FieldMapping>& fields, bool targets_editable);
    const QVector<fincept::services::datamapping::FieldMapping>& fields() const { return fields_; }
    void set_source_columns(const QStringList& columns) { source_columns_ = columns; }
    const QStringList& source_columns() const { return source_columns_; }
    /// The latest preview: per-field stats and first values.
    void set_preview(const fincept::services::datamapping::MappingOutput& out);
    void clear_preview();
    void add_field(const fincept::services::datamapping::FieldMapping& f);
    void remove_field(int row);
    void refresh_colours();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  signals:
    void fields_edited();

  private:
    QVector<fincept::services::datamapping::FieldMapping> fields_;
    QStringList source_columns_;
    bool targets_editable_ = true;
    QVector<fincept::services::datamapping::FieldStats> stats_;
    QStringList samples_;
    bool has_preview_ = false;
};

class FieldGridDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit FieldGridDelegate(FieldGridModel* model, QObject* parent = nullptr);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

    /// Format presets for a target type ("auto", "dd.MM.yyyy", "epoch_ms", "start", …).
    static QStringList formats_for(fincept::services::datamapping::TargetType type);

  private:
    FieldGridModel* model_;
};

} // namespace fincept::screens::datamapping
