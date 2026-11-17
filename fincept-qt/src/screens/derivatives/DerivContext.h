#pragma once
// DerivContext — what the Derivatives pages share: the underlying and option chain
// in view, the position built on the STRATEGY page (which SCENARIOS analyses), and
// the hand-offs between pages ("Price in Pricer", "Add to strategy", "Run
// scenarios"). Owned by the DerivativesScreen shell; pages hold a pointer.
// GUI thread only; signals are delivered directly.

#include "services/options/BookTypes.h"
#include "services/options/FreeOptionChainService.h"
#include "services/options/StrategyMath.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace fincept::screens::deriv {

/// One listed contract, as the pricer needs it.
struct ContractRef {
    QString symbol;
    QString expiry_label; ///< "2026-12-18"
    double spot = 0;
    double strike = 0;
    double years = 0;
    bool is_call = true;
    double iv = 0;   ///< decimal
    double rate = 0; ///< continuous decimal
    double q = 0;    ///< implied carry, continuous decimal
    double mid = 0;
};

/// A leg as the STRATEGY page lists it: the maths leg plus how to show it.
struct LegView {
    services::options::strategy::Leg leg;
    QString expiry_label; ///< empty for the underlying
};

class DerivContext : public QObject {
    Q_OBJECT
  public:
    using ChainPtr = services::options::FreeOptionChainService::ChainPtr;
    explicit DerivContext(QObject* parent = nullptr) : QObject(parent) {}

    ChainPtr chain() const { return chain_; }
    void set_chain(const ChainPtr& c) {
        chain_ = c;
        emit chain_changed();
    }

    /// The position the STRATEGY page shows (SCENARIOS reads it).
    struct Position {
        QString name;
        QString symbol;
        QString currency = QStringLiteral("USD");
        QVector<LegView> legs;
        services::options::strategy::Market market;
        double multiplier = 100;
        double atm_iv = 0.2; ///< for the probability cone / POP
        bool valid = false;
    };
    const Position& position() const { return position_; }
    void set_position(const Position& p) {
        position_ = p;
        emit position_changed();
    }

    /// A position (or every position on one underlying) handed from the BOOK page to
    /// SCENARIOS; kept apart from the STRATEGY page's position.
    const Position& book_position() const { return book_position_; }
    void set_book_position(const Position& p) { book_position_ = p; }

  signals:
    void chain_changed();
    void position_changed();
    /// "Price in Pricer" on a chain contract.
    void price_contract_requested(const fincept::screens::deriv::ContractRef& contract);
    /// "Add to strategy" from the chain (appended to the legs).
    void legs_requested(const QVector<fincept::screens::deriv::LegView>& legs);
    /// "Run scenarios" on the STRATEGY page's position.
    void scenarios_requested();
    /// "Scenarios on this contract" from the chain: SCENARIOS on a single option.
    void scenarios_contract_requested(const fincept::screens::deriv::ContractRef& contract);
    /// "Run scenarios" from the BOOK on book_position().
    void book_scenarios_requested();
    /// "Add to book" (chain, strategy, curves): the BOOK page opens its add dialog on `draft`.
    void book_add_requested(const fincept::services::options::book::BookPosition& draft);
    /// "Open in STRATEGY" from the BOOK or a saved strategy: replace the legs with these.
    void strategy_load_requested(const QString& symbol, const QString& name,
                                 const QVector<fincept::services::options::book::BookLeg>& legs);

  private:
    ChainPtr chain_;
    Position position_;
    Position book_position_;
};

} // namespace fincept::screens::deriv
