#pragma once
// DataSourcesScreen — the Data Sources tab: one three-pane workspace.
//   SOURCES (left): every connection type and every catalogue connector, searchable and
//     filterable, each with a health badge for THIS build on THIS machine.
//   SAVED CONNECTIONS (middle): name, type, status of the last real TEST, when, latency.
//   DETAIL (right): a type card, a connection (settings form, TEST, schema browser, sample
//     rows) or a catalogue connector (inputs, run, typed result).
// Built on first show (a skeleton paints first); every list is model/view; every load, test,
// save and import runs off the GUI thread. There is no background polling: a status is the
// result of a real TEST the user ran (or a workflow / AI tool ran through ConnectionService).

#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/data_sources/DataSourcesModels.h"

#include <QVariantMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTimer;

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens::datasources {

class CatalogConnectorView;
class ConnectionDetailView;

class DataSourcesScreen : public QWidget,
                          public fincept::screens::IStatefulScreen,
                          public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit DataSourcesScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("data_sources"); }
    int state_version() const override { return 2; }

    // ISectionLinked — "connector:<name>" (a catalogue connector, as the search box's Data rows
    // send), "type:<id>" (a connection type), "connection:<id>" (a saved connection): selected in
    // its list as a click does (filters that hide the row are cleared first). "sources" /
    // "catalog" ("connectors") / "types" focus the SOURCES list (catalog / types set its kind
    // filter), "connections" the SAVED CONNECTIONS list. `focus` = a connector name (with
    // "connector", "sources", "catalog" or an empty section), a type id ("types") or a connection
    // id ("connections"). Before the workspace has loaded the link is kept and applied then.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    void build_workspace();
    QWidget* build_sources_pane(QWidget* parent);
    QWidget* build_connections_pane(QWidget* parent);
    QWidget* build_detail_pane(QWidget* parent);

    void reload(bool with_sources);
    void apply_connections(const QVector<DataSource>& rows);
    void apply_restore();
    void update_counts();
    void show_notice(const QString& text, bool error = false);

    void on_source_selected();
    void on_connection_selected();
    void show_type_card(const QString& type_id);
    void select_source(SourceKind kind, const QString& id);
    void select_connection(const QString& id);
    bool confirm_leave_edits();
    void show_connection_menu(const QPoint& pos);

    bool built_ = false;
    bool loaded_sources_ = false;
    bool loading_ = false;
    bool reload_again_ = false;
    QVariantMap pending_restore_;
    QString pending_select_connection_;
    // A deep link (open_section) that arrived before the sources loaded.
    QString pending_link_section_;
    QString pending_link_focus_;
    bool has_pending_link_ = false;
    QString current_type_;        ///< selected connection type (for + NEW / "only this type")
    QString shown_connection_;    ///< connection shown in the detail pane
    QString shown_source_;        ///< "t:<type id>" / "c:<connector>" shown in the detail pane
    qint64 sources_loaded_ms_ = 0; ///< when health badges / key presence were last computed

    QStackedWidget* root_stack_ = nullptr;
    fincept::ui::LoadingSkeleton* boot_skeleton_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QLabel* notice_ = nullptr;
    QTimer* notice_timer_ = nullptr;
    QTimer* health_debounce_ = nullptr;
    QSplitter* splitter_ = nullptr;

    // Sources
    QLineEdit* source_search_ = nullptr;
    QComboBox* kind_filter_ = nullptr;
    QComboBox* category_filter_ = nullptr;
    QCheckBox* usable_only_ = nullptr;
    QLabel* sources_count_ = nullptr;
    QStackedWidget* sources_stack_ = nullptr;
    QTableView* sources_view_ = nullptr;
    fincept::ui::EmptyState* sources_empty_ = nullptr;
    SourceCatalogModel* sources_model_ = nullptr;
    SourceFilterProxy* sources_proxy_ = nullptr;

    // Connections
    QLineEdit* conn_search_ = nullptr;
    QCheckBox* only_type_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QLabel* conns_count_ = nullptr;
    QStackedWidget* conns_stack_ = nullptr;
    QTableView* conns_view_ = nullptr;
    fincept::ui::EmptyState* conns_empty_ = nullptr;
    ConnectionListModel* conns_model_ = nullptr;
    ConnectionFilterProxy* conns_proxy_ = nullptr;

    // Detail
    QStackedWidget* detail_stack_ = nullptr;
    fincept::ui::EmptyState* intro_ = nullptr;
    fincept::ui::EmptyState* type_card_ = nullptr;
    ConnectionDetailView* conn_view_ = nullptr;
    CatalogConnectorView* connector_view_ = nullptr;
};

} // namespace fincept::screens::datasources

namespace fincept::screens {
using DataSourcesScreen = datasources::DataSourcesScreen;
}
