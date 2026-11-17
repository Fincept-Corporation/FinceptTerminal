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
//
// Harness profiles (provider "harness:<id>", e.g. "harness:claude"): a CLI agent
// harness answering terminal AI — model_id is the harness's model, effort in
// reasoning_effort, never an API key. They are chosen through ModelRouter
// (AI Chat, Quick Chat, Quick Ask, the tool picker, Chat Mode) and are NOT
// usable by the Python agent runtime: list_profiles() and resolve_for_context()
// leave them out, so Agent Studio, teams, workflows, node editors and AI Quant
// Lab never see one; list_all_profiles() includes them (Settings, ModelRouter).
// A harness profile cannot be the global default.

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
    QString reasoning_effort; // harness profiles: "" = the CLI's default

    /// provider is "harness:<id>".
    bool is_harness() const { return provider.startsWith(QLatin1String("harness:"), Qt::CaseInsensitive); }
    /// "claude" for "harness:claude"; empty for other providers.
    QString harness_id() const { return is_harness() ? provider.mid(8).toLower() : QString(); }
};

/// One row of llm_profile_assignments (context_id empty = the type-level row).
struct LlmProfileAssignment {
    QString context_type;
    QString context_id;
    QString profile_id;
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

    /// All profiles the agent runtime can use, ordered by name — harness
    /// profiles are left out (see the header comment).
    Result<QVector<LlmProfile>> list_profiles() const;

    /// Every profile including harness profiles, ordered by name.
    Result<QVector<LlmProfile>> list_all_profiles() const;

    /// Every assignment row (ModelRouter's explicit choices included).
    Result<QVector<LlmProfileAssignment>> list_assignments() const;

    /// Single profile by id.
    Result<LlmProfile> get_profile(const QString& id) const;

    /// Save (insert or replace).
    Result<void> save_profile(const LlmProfile& p);

    /// Delete profile and all its assignments.
    Result<void> delete_profile(const QString& id);

    /// Mark one profile as the global default (clears all others). A harness
    /// profile is refused (the default also serves the agent runtime).
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
    /// For the agent runtime: an assignment to a harness profile is passed over
    /// (the next level answers). Terminal-AI features resolve through ModelRouter.
    ResolvedLlmProfile resolve_for_context(const QString& context_type, const QString& context_id = {}) const;

  private:
    LlmProfileRepository() = default;

    static LlmProfile map_profile(QSqlQuery& q);
    static QString type_default_key(const QString& context_type);
    ResolvedLlmProfile profile_to_resolved(const LlmProfile& p) const;
    ResolvedLlmProfile legacy_fallback() const;
};

} // namespace fincept
