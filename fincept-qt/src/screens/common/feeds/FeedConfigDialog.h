#pragma once
#include "services/feeds/FeedScraper.h" // DiscoveredSchema
#include "services/feeds/FeedTypes.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QLabel;
class QWidget;
class QVBoxLayout;
class QCheckBox;

namespace fincept::feeds {

/// Add/edit one FeedSubscription. Manual parse mode offers one-click field discovery
/// — fetch the feed once, list every tag, and let the user pick/rename/role them via
/// dropdowns (no path syntax). Fields are unlimited and fully editable.
class FeedConfigDialog : public QDialog {
    Q_OBJECT
  public:
    explicit FeedConfigDialog(const FeedSubscription& initial, QWidget* parent = nullptr);
    FeedSubscription result_subscription() const { return sub_; }
    bool delete_requested() const { return delete_requested_; }

  private:
    FeedSubscription collect() const; // read widgets -> FeedSubscription
    void apply(const FeedSubscription& s);
    void on_test();
    void on_discover();
    void apply_discovery(const DiscoveredSchema& schema);
    void update_manual_visibility();
    QWidget* add_field_row(const MappedField& f); // append a mapping row, return container
    void clear_field_rows();

    FeedSubscription sub_;
    QLineEdit* name_ = nullptr;
    QLineEdit* url_ = nullptr;
    QSpinBox* refresh_ = nullptr;
    QComboBox* display_ = nullptr;
    QComboBox* parse_ = nullptr;
    QComboBox* format_ = nullptr;
    QCheckBox* persist_ = nullptr; // store items for offline cache + history

    // Manual-mapping section
    QWidget* manual_box_ = nullptr;
    QComboBox* item_combo_ = nullptr;     // repeating record selector (editable)
    QLabel* discover_status_ = nullptr;
    QVBoxLayout* fields_layout_ = nullptr; // holds the row widgets + a trailing stretch
    QVector<QWidget*> field_rows_;
    QStringList discovered_paths_;        // populates each row's source dropdown

    QLabel* test_result_ = nullptr;
    bool delete_requested_ = false;
};

} // namespace fincept::feeds
