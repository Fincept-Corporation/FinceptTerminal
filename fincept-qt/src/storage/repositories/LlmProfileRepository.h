#pragma once
// LlmProfileRepository.h — CRUD + resolution for the LLM Profile system.
//
// A "profile" is a named, portable LLM configuration that can be assigned to
// any context (AI Chat, a specific agent, a team coordinator, etc.).
//
// Resolution order for any context:
//   1. Entity-specific assignment  (context_type="agent",  context_id=<agent_id>)
//   2. Type-level default          (context_type="agent_default", context_id=NULL)
//   3. Global default profile      (is_default = 1)
//   4. Active provider fallback    (legacy — from llm_configs)

#include "storage/repositories/BaseRepository.h"

namespace fincept {

// ── Data types ────────────────────────────────────────────────────────────────

struct LlmProfile {
    QString id;
    QString name;
    QString provider;
    QString model_id;
    QString api_key;
    QString base_url;
    double temperature = 0.7;
    int max_tokens = 4096;
    QString system_prompt;
    bool is_default = false;
    QString created_at;
    QString updated_at;
};

/// Convenience struct — everything a caller needs to make an LLM request.
/// Returned by resolve_for_context(); never contains nulls after resolution.
struct ResolvedLlmProfile {
    QString profile_id;   // which profile was chosen (empty = legacy fallback)
    QString profile_name; // human-readable label
    QString provider;
    QString model_id;
    QString api_key;
    QString base_url;
    double temperature = 0.7;
    int max_tokens = 4096;
    QString system_prompt;
};

// ── Repository ────────────────────────────────────────────────────────────────

class LlmProfileRepository : public BaseRepository<LlmProfile> {
  public:
    static LlmProfileRepository& instance();

    // ── Profile CRUD ─────────────────────────────────────────────────────────

    /// All profiles ordered by name.
    Result<QVector<LlmProfile>> list_profiles() const;

    /// Single profile by id.
    Result<LlmProfile> get_profile(const QString& id) const;

    /// Save (insert or replace).
    Result<void> save_profile(const LlmProfile& p);

    /// Delete profile and all its assignments.
    Result<void> delete_profile(const QString& id);

    /// Mark one profile as the global default (clears all others).
    Result<void> set_default(const QString& id);

    // ── Assignment CRUD ───────────────────────────────────────────────────────

    /// Assign a profile to a context slot.
    /// context_type: "ai_chat" | "agent_default" | "team_default" |
    ///               "agent" | "team" | "team_coordinator"
    /// context_id:   agent/team id, or empty string for type-level defaults.
    Result<void> assign_profile(const QString& context_type, const QString& context_id, const QString& profile_id);

    /// Remove assignment for a context slot (falls back to next level).
    Result<void> remove_assignment(const QString& context_type, const QString& context_id);

    /// Get the profile_id currently assigned to a slot, or empty if none.
    QString get_assignment(const QString& context_type, const QString& context_id) const;

    // ── Resolution ────────────────────────────────────────────────────────────

    /// Resolve the effective LLM profile for a context using the 4-level chain.
    /// context_type examples: "ai_chat", "agent", "team", "team_coordinator"
    /// context_id:  agent/team id, or empty for type-level queries.
    /// Never fails — worst case returns a zero-valued ResolvedLlmProfile.
    ResolvedLlmProfile resolve_for_context(const QString& context_type, const QString& context_id = {}) const;

  private:
    LlmProfileRepository() = default;

    static LlmProfile map_profile(QSqlQuery& q);
    static QString type_default_key(const QString& context_type);
    ResolvedLlmProfile profile_to_resolved(const LlmProfile& p) const;
    ResolvedLlmProfile legacy_fallback() const;
};

} // namespace fincept
