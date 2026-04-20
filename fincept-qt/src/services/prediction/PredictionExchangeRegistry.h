#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace fincept::services::prediction {

class PredictionExchangeAdapter;

/// Singleton registry of prediction-market exchange adapters.
///
/// Owns one adapter instance per exchange id for the process lifetime.
/// Screens do NOT own adapters — they look up the active adapter via
/// active() and listen for active_changed() when the user switches
/// exchanges in the command bar.
class PredictionExchangeRegistry : public QObject {
    Q_OBJECT
  public:
    static PredictionExchangeRegistry& instance();

    /// Register an adapter. Takes ownership. Must be called from main.cpp
    /// once per supported exchange, before any screen is constructed.
    void register_adapter(std::unique_ptr<PredictionExchangeAdapter> adapter);

    QStringList available_ids() const;
    PredictionExchangeAdapter* adapter(const QString& id) const;
    PredictionExchangeAdapter* active() const;
    QString active_id() const;

    /// Switch the active exchange. Emits active_changed() on transition.
    /// No-op if id is already active or not registered.
    void set_active(const QString& id);

  signals:
    void active_changed(const QString& new_id);

  private:
    PredictionExchangeRegistry();
    ~PredictionExchangeRegistry() override;

    struct Entry {
        QString id;
        std::unique_ptr<PredictionExchangeAdapter> adapter;
    };
    // std::vector (not QVector/QList) because Entry is move-only —
    // QList's instantiation pulls in copy-based QGenericArrayOps paths
    // that static_assert-fail on std::unique_ptr fields.
    std::vector<Entry> entries_;
    QString active_id_;
};

} // namespace fincept::services::prediction
