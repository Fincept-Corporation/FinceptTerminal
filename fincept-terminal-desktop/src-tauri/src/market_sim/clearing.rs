use crate::market_sim::types::*;
use std::collections::HashMap;

/// Central Counterparty (CCP) clearing and settlement engine
pub struct ClearingHouse {
    /// Settlement cycle in nanos (T+1 = 1 day = 86_400_000_000_000 nanos)
    settlement_cycle_nanos: Nanos,
    /// Pending settlements
    pending_settlements: Vec<Settlement>,
    /// Completed settlements
    completed_settlements: Vec<Settlement>,
    /// Netting ledger: participant -> instrument -> net obligation
    netting_ledger: HashMap<ParticipantId, HashMap<InstrumentId, NettingEntry>>,
    /// Guarantee fund contributions
    guarantee_fund: HashMap<ParticipantId, f64>,
    /// Default waterfall layers
    waterfall: DefaultWaterfall,
    /// Fails to deliver tracking
    fails_to_deliver: Vec<FailToDeliver>,
}

#[derive(Debug, Clone)]
pub struct Settlement {
    pub trade_id: TradeId,
    pub buyer_id: ParticipantId,
    pub seller_id: ParticipantId,
    pub instrument_id: InstrumentId,
    pub quantity: Qty,
    pub price: Price,
    pub trade_timestamp: Nanos,
    pub settlement_due: Nanos,
    pub status: SettlementStatus,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SettlementStatus {
    Pending,
    Netted,
    Settled,
    Failed,
}

#[derive(Debug, Clone, Default)]
struct NettingEntry {
    pub net_quantity: Qty,     // + = receive, - = deliver
    pub net_cash: f64,        // + = receive, - = pay
}

#[derive(Debug, Clone)]
pub struct DefaultWaterfall {
    /// Layer 1: Defaulter's margin
    pub defaulter_margin_pct: f64,
    /// Layer 2: Defaulter's guarantee fund contribution
    pub defaulter_gf_pct: f64,
    /// Layer 3: CCP's own capital (skin in the game)
    pub ccp_capital: f64,
    /// Layer 4: Non-defaulting members' guarantee fund
    pub mutualized_gf_pct: f64,
}

impl Default for DefaultWaterfall {
    fn default() -> Self {
        Self {
            defaulter_margin_pct: 1.0,
            defaulter_gf_pct: 1.0,
            ccp_capital: 10_000_000.0,
            mutualized_gf_pct: 0.5,
        }
    }
}

#[derive(Debug, Clone)]
pub struct FailToDeliver {
    pub settlement: Settlement,
    pub fail_timestamp: Nanos,
    pub days_outstanding: u32,
}

impl ClearingHouse {
    pub fn new() -> Self {
        Self {
            settlement_cycle_nanos: 86_400_000_000_000, // T+1
            pending_settlements: Vec::new(),
            completed_settlements: Vec::new(),
            netting_ledger: HashMap::new(),
            guarantee_fund: HashMap::new(),
            waterfall: DefaultWaterfall::default(),
            fails_to_deliver: Vec::new(),
        }
    }

    pub fn with_settlement_cycle(mut self, nanos: Nanos) -> Self {
        self.settlement_cycle_nanos = nanos;
        self
    }

    /// Register a trade for clearing
    pub fn register_trade(&mut self, trade: &Trade) {
        let settlement = Settlement {
            trade_id: trade.id,
            buyer_id: trade.buyer_id,
            seller_id: trade.seller_id,
            instrument_id: trade.instrument_id,
            quantity: trade.quantity,
            price: trade.price,
            trade_timestamp: trade.timestamp,
            settlement_due: trade.timestamp + self.settlement_cycle_nanos,
            status: SettlementStatus::Pending,
        };

        // Update netting ledger
        // Buyer: receives shares, pays cash
        {
            let buyer_ledger = self.netting_ledger
                .entry(trade.buyer_id)
                .or_default();
            let entry = buyer_ledger
                .entry(trade.instrument_id)
                .or_insert_with(NettingEntry::default);
            entry.net_quantity += trade.quantity;
            entry.net_cash -= trade.price as f64 * trade.quantity as f64;
        }

        // Seller: delivers shares, receives cash
        {
            let seller_ledger = self.netting_ledger
                .entry(trade.seller_id)
                .or_default();
            let entry = seller_ledger
                .entry(trade.instrument_id)
                .or_insert_with(NettingEntry::default);
            entry.net_quantity -= trade.quantity;
            entry.net_cash += trade.price as f64 * trade.quantity as f64;
        }

        self.pending_settlements.push(settlement);
    }

    /// Process settlements that are due
    pub fn process_settlements(
        &mut self,
        current_time: Nanos,
        accounts: &mut HashMap<ParticipantId, ParticipantAccount>,
    ) -> Vec<SettlementResult> {
        let mut results = Vec::new();
        let mut settled_indices = Vec::new();

        for (idx, settlement) in self.pending_settlements.iter_mut().enumerate() {
            if settlement.settlement_due > current_time {
                continue;
            }

            // Check if both parties can settle
            let buyer_ok = accounts.get(&settlement.buyer_id).map_or(false, |a| a.is_active);
            let seller_ok = accounts.get(&settlement.seller_id).map_or(false, |a| a.is_active);

            if buyer_ok && seller_ok {
                // Successful settlement
                let cash_amount = settlement.price as f64 * settlement.quantity as f64;

                if let Some(buyer) = accounts.get_mut(&settlement.buyer_id) {
                    buyer.cash_balance -= cash_amount;
                }
                if let Some(seller) = accounts.get_mut(&settlement.seller_id) {
                    seller.cash_balance += cash_amount;
                }

                settlement.status = SettlementStatus::Settled;
                settled_indices.push(idx);

                results.push(SettlementResult {
                    trade_id: settlement.trade_id,
                    status: SettlementStatus::Settled,
                    fail_reason: None,
                });
            } else {
                // Fail to deliver
                settlement.status = SettlementStatus::Failed;
                self.fails_to_deliver.push(FailToDeliver {
                    settlement: settlement.clone(),
                    fail_timestamp: current_time,
                    days_outstanding: 0,
                });

                results.push(SettlementResult {
                    trade_id: settlement.trade_id,
                    status: SettlementStatus::Failed,
                    fail_reason: Some("Counterparty inactive".to_string()),
                });
            }
        }

        // Move settled/failed to completed
        for idx in settled_indices.into_iter().rev() {
            let s = self.pending_settlements.remove(idx);
            self.completed_settlements.push(s);
        }

        results
    }

    /// Calculate net obligations for a participant after netting
    pub fn net_obligations(&self, participant_id: ParticipantId) -> Vec<(InstrumentId, Qty, f64)> {
        self.netting_ledger
            .get(&participant_id)
            .map(|ledger| {
                ledger
                    .iter()
                    .map(|(inst_id, entry)| (*inst_id, entry.net_quantity, entry.net_cash))
                    .collect()
            })
            .unwrap_or_default()
    }

    /// Set guarantee fund contribution
    pub fn set_guarantee_fund(&mut self, participant_id: ParticipantId, amount: f64) {
        self.guarantee_fund.insert(participant_id, amount);
    }

    /// Process a default scenario
    pub fn process_default(
        &mut self,
        defaulter_id: ParticipantId,
        loss_amount: f64,
    ) -> DefaultWaterfallResult {
        let mut remaining_loss = loss_amount;
        let mut layers_used = Vec::new();

        // Layer 1: Defaulter's margin (handled by risk engine)
        // Layer 2: Defaulter's guarantee fund
        let gf_contribution = self.guarantee_fund.get(&defaulter_id).copied().unwrap_or(0.0);
        let gf_used = remaining_loss.min(gf_contribution);
        remaining_loss -= gf_used;
        layers_used.push(("Defaulter GF".to_string(), gf_used));

        // Layer 3: CCP capital
        let ccp_used = remaining_loss.min(self.waterfall.ccp_capital);
        remaining_loss -= ccp_used;
        layers_used.push(("CCP Capital".to_string(), ccp_used));

        // Layer 4: Mutualized guarantee fund
        if remaining_loss > 0.0 {
            let total_other_gf: f64 = self.guarantee_fund
                .iter()
                .filter(|(id, _)| **id != defaulter_id)
                .map(|(_, amount)| amount)
                .sum();
            let mutualized = (total_other_gf * self.waterfall.mutualized_gf_pct).min(remaining_loss);
            remaining_loss -= mutualized;
            layers_used.push(("Mutualized GF".to_string(), mutualized));
        }

        DefaultWaterfallResult {
            total_loss: loss_amount,
            covered: loss_amount - remaining_loss,
            uncovered: remaining_loss,
            layers_used,
        }
    }

    /// Get fails-to-deliver count
    pub fn ftd_count(&self) -> usize {
        self.fails_to_deliver.len()
    }

    /// Get pending settlement count
    pub fn pending_count(&self) -> usize {
        self.pending_settlements.len()
    }

    /// Reset netting at end of day
    pub fn reset_netting(&mut self) {
        self.netting_ledger.clear();
    }
}

#[derive(Debug, Clone)]
pub struct SettlementResult {
    pub trade_id: TradeId,
    pub status: SettlementStatus,
    pub fail_reason: Option<String>,
}

#[derive(Debug, Clone)]
pub struct DefaultWaterfallResult {
    pub total_loss: f64,
    pub covered: f64,
    pub uncovered: f64,
    pub layers_used: Vec<(String, f64)>,
}
