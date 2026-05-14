"""
AgenticRunner — true-agentic extension of ResumableTaskRunner.

Adds, on top of the base runner:
  * per-step event emission via an injectable emitter callable
  * cooperative pause/cancel signalling, polled from the DB status between steps

What this MVP does NOT yet add (deliberately scoped to Phase 2):
  * reflector / adaptive re-plan
  * token / dollar / wall-clock budget caps
  * HITL pause-for-question on PlanStep.WAIT

Control signalling protocol (between subprocesses):
  * The agentic task runs synchronously inside one Python subprocess.
  * A separate `agentic_pause_task` / `agentic_cancel_task` call from C++ is a
    SHORT-lived subprocess that flips `agent_tasks.status` to
    `pause_requested` or `cancel_requested`.
  * The running runner re-reads `status` between steps; on a pending signal
    it updates the status to `paused` / `cancelled`, emits the matching
    event, and returns.
"""
from typing import Any, Callable, Dict, List, Optional

from finagent_core.agentic.archival_memory import ArchivalMemoryStore
from finagent_core.agentic.budget import BudgetGuard
from finagent_core.agentic.reflector import Reflector, ReflectorDecision
from finagent_core.agentic.reflexion_store import ReflexionStore
from finagent_core.agentic.skill_library import SkillLibrary
from finagent_core.task_state import ResumableTaskRunner, _DEFAULT_DB

EventEmitter = Callable[[str, Dict[str, Any]], None]


class AgenticRunner(ResumableTaskRunner):
    """ResumableTaskRunner with event emission, pause/cancel polling, and budget caps."""

    def __init__(
        self,
        api_keys: Dict[str, str],
        db_path: str = _DEFAULT_DB,
        emit: Optional[EventEmitter] = None,
    ):
        super().__init__(api_keys=api_keys, db_path=db_path)
        # Default to a no-op so direct unit tests don't require an emitter.
        self._emit: EventEmitter = emit or (lambda kind, payload: None)
        # BudgetGuard is constructed per-run inside _run_loop; this is the
        # active instance the overridden _execute_step records against.
        self._budget: Optional[BudgetGuard] = None
        # Reflector is shared across steps within one run.
        self._reflector = Reflector(api_keys=api_keys)
        # Skill library: shared across runs. Same DB file as the tasks table.
        self._skills = SkillLibrary(db_path=db_path)
        # Tracks skill ids retrieved at plan time, so we can record_use() on success.
        self._retrieved_skill_ids: List[str] = []
        # Archival memory (Letta tier 3): cross-task durable fact store.
        self._archival = ArchivalMemoryStore(db_path=db_path)
        # Reflexion store: persisted per-step critiques used to season the
        # next planning attempt for similar queries (Shinn 2023).
        self._reflexion = ReflexionStore(db_path=db_path)

    # ── Control-signal polling ───────────────────────────────────────────────

    def _check_control(self, task_id: str) -> Optional[str]:
        """Return 'pause_requested'|'cancel_requested' if the user signalled, else None."""
        task = self.state_mgr.get_task(task_id)
        if not task:
            return "cancel_requested"
        status = task.get("status")
        if status in ("pause_requested", "cancel_requested"):
            return status
        return None

    # ── Public lifecycle ─────────────────────────────────────────────────────

    def start_task(self, query: str, config: Dict[str, Any]) -> Dict[str, Any]:
        task_id = self.state_mgr.create_task(query, config)
        self.state_mgr.update_status(task_id, "running")
        self._emit("task_started", {"task_id": task_id, "query": query})
        return self._run_loop(task_id, query, config, completed_steps=[], saved_plan=None)

    def resume_task(self, task_id: str) -> Dict[str, Any]:
        task = self.state_mgr.get_task(task_id)
        if not task:
            return {"success": False, "error": f"Task {task_id} not found"}
        if task["status"] == "completed":
            return {
                "success": True,
                "task_id": task_id,
                "response": task["result"],
                "already_completed": True,
            }
        # Honor a pre-existing pause/cancel signal before we overwrite status to running.
        if task["status"] == "cancel_requested":
            at_step = len(task["steps"])
            self.state_mgr.update_status(task_id, "cancelled")
            self._emit("cancelled", {"task_id": task_id, "at_step": at_step})
            return {"success": False, "task_id": task_id, "cancelled": True}
        if task["status"] == "pause_requested":
            at_step = len(task["steps"])
            self.state_mgr.update_status(task_id, "paused")
            self._emit("paused", {"task_id": task_id, "at_step": at_step})
            return {"success": True, "task_id": task_id, "paused": True}

        # HITL: if the user replied to a clarifying question, consume the
        # answer atomically so it isn't replayed if resume is hit twice.
        pending_answer = self.state_mgr.consume_answer(task_id)

        self.state_mgr.update_status(task_id, "running")
        self._emit(
            "task_resumed",
            {"task_id": task_id, "from_step": len(task["steps"]),
             "with_user_input": bool(pending_answer)},
        )
        return self._run_loop(
            task_id, task["query"], task["config"], task["steps"], task.get("plan"),
            pending_answer=pending_answer,
        )

    # ── Inner loop ───────────────────────────────────────────────────────────

    def _run_loop(
        self,
        task_id: str,
        query: str,
        config: Dict[str, Any],
        completed_steps: List[Dict[str, Any]],
        saved_plan: Optional[Dict[str, Any]],
        pending_answer: Optional[str] = None,
    ) -> Dict[str, Any]:
        # One BudgetGuard per run; survives across step boundaries so the
        # _execute_step override can record tokens against it.
        self._budget = BudgetGuard.from_config(config)
        # Resume scenario: pre-pay the steps_used counter so we don't grant a
        # fresh budget every time the user clicks Resume.
        self._budget.steps_used = len(completed_steps)
        try:
            if saved_plan and saved_plan.get("steps"):
                plan = saved_plan
            else:
                # Voyager-style retrieval: search the persistent skill library
                # for recipes matching this query and inject the top-K into
                # the planner prompt as guidance. Library hits raise the
                # chance of plan reuse over time without locking in any specific
                # recipe — the planner is free to ignore.
                skills_block = self._compose_skill_hints(query)
                # Letta tier-3: pull relevant cross-task facts and prepend as
                # background context the planner should consider (preferences,
                # prior conclusions, portfolio facts).
                memory_block = self._compose_memory_hints(query, config)
                # Reflexion: lessons learned on similar queries in the past.
                lessons_block = self._compose_reflexion_hints(query, task_id)
                blocks = [b for b in (memory_block, lessons_block, skills_block) if b]
                planner_query = query if not blocks else (
                    query + "\n\n" + "\n\n".join(blocks)
                )
                plan = self._plan_steps(planner_query, config)
                self.state_mgr.save_plan(task_id, plan)
            self._emit("plan_ready", {"task_id": task_id, "plan": plan})

            # Reflector / re-plan support: `i` indexes into the CURRENT plan
            # and resets to 0 on each replan; `global_idx` is monotonic across
            # replans so step checkpoints stay densely packed in steps_json.
            all_results: List[Dict[str, Any]] = list(completed_steps)
            global_idx = len(completed_steps)
            i = global_idx
            reflector_enabled = bool(config.get("agentic", {}).get("reflector", True))

            while True:
                steps = plan.get("steps") or []
                if i >= len(steps):
                    break

                breach = self._budget.breach()
                if breach is not None:
                    self.state_mgr.update_status(task_id, "failed_budget")
                    self._emit(
                        "budget_stop",
                        {"task_id": task_id, "at_step": global_idx, "breach": breach,
                         "budget": self._budget.snapshot()},
                    )
                    return {"success": False, "task_id": task_id, "budget_stop": breach}

                ctrl = self._check_control(task_id)
                if ctrl == "cancel_requested":
                    self.state_mgr.update_status(task_id, "cancelled")
                    self._emit("cancelled", {"task_id": task_id, "at_step": global_idx})
                    return {"success": False, "task_id": task_id, "cancelled": True}
                if ctrl == "pause_requested":
                    self.state_mgr.update_status(task_id, "paused")
                    self._emit("paused", {"task_id": task_id, "at_step": global_idx})
                    return {"success": True, "task_id": task_id, "paused": True}

                step = steps[i]
                # If the user just answered a clarifying question, thread that
                # answer into the next step's effective query so the agent
                # benefits from the clarification. Single-use: cleared after.
                effective_step = step
                if pending_answer:
                    effective_step = dict(step)
                    cfg = dict(effective_step.get("config") or {})
                    base_q = cfg.get("query") or effective_step.get("name") or ""
                    cfg["query"] = f"User clarification: {pending_answer}\n\n{base_q}"
                    effective_step["config"] = cfg
                    pending_answer = None

                self._emit(
                    "step_start",
                    {"task_id": task_id, "step_index": global_idx, "step": effective_step,
                     "budget": self._budget.snapshot()},
                )
                step_result = self._execute_step(effective_step, query, config, global_idx, all_results)
                self._budget.record_step()
                self.state_mgr.save_checkpoint(task_id, global_idx, step_result)
                all_results.append(step_result)
                self._emit(
                    "step_end",
                    {"task_id": task_id, "step_index": global_idx, "result": step_result,
                     "budget": self._budget.snapshot()},
                )

                # Reflector decides whether to continue, re-plan, ask, or finish.
                if reflector_enabled:
                    decision = self._reflector.assess(
                        query=query, plan=plan, step=step, step_result=step_result,
                        prior_results=all_results, config=config,
                        budget_snapshot=self._budget.snapshot(),
                    )
                    # Persist the verdict to the Reflexion store. Only
                    # interesting decisions (replan/question/done) are
                    # surfaced at retrieval time, but we save them all so an
                    # eval/audit pass can replay full traces.
                    try:
                        self._reflexion.save_reflection(
                            task_id=task_id, query=query, step_idx=global_idx,
                            decision=decision.decision, reason=decision.reason,
                            replan_hint=decision.replan_hint, question=decision.question,
                        )
                    except Exception:
                        pass
                    self._emit(
                        "reflection",
                        {"task_id": task_id, "step_index": global_idx,
                         "decision": decision.to_dict()},
                    )
                    if decision.decision == "done":
                        break
                    if decision.decision == "replan":
                        plan = self._replan(query, config, all_results, decision.replan_hint)
                        self.state_mgr.save_plan(task_id, plan)
                        self._emit("replanned",
                                   {"task_id": task_id, "plan": plan,
                                    "reason": decision.reason})
                        i = 0
                        global_idx += 1
                        continue
                    if decision.decision == "question":
                        # HITL handoff: persist the question so a fresh subprocess
                        # spawned by the UI's get_task call can surface it, and
                        # status flips to paused_for_input. The user replies via
                        # agentic_reply_question; resume_task consumes the answer
                        # and threads it into the next step.
                        question_text = decision.question or "Need user input"
                        self.state_mgr.save_question(task_id, question_text)
                        self._emit(
                            "question",
                            {"task_id": task_id, "step_index": global_idx,
                             "question": question_text,
                             "reason": decision.reason},
                        )
                        return {"success": True, "task_id": task_id,
                                "paused_for_input": True,
                                "question": question_text}

                i += 1
                global_idx += 1

            final = self._synthesize(query, all_results, config)
            self.state_mgr.complete_task(task_id, final)

            # Compounding payoff: distill a new reusable recipe if existing
            # skills did NOT cover this task; otherwise bump usage on the ones
            # that did. Both are best-effort and guarded by budget so they
            # never break a successful completion. Order matters — distillation
            # must run before record_used clears _retrieved_skill_ids.
            self._try_distill_skill(task_id, query, plan, all_results, config)
            self._record_used_skills()
            # Archive a compact memory of this task's outcome so future tasks
            # with similar queries can leverage what we learned (Letta tier 3).
            self._archive_task_outcome(query, final, config)

            self._emit(
                "done",
                {"task_id": task_id, "final": final, "steps_completed": len(all_results)},
            )
            result: Dict[str, Any] = {
                "success": True,
                "task_id": task_id,
                "response": final,
                "steps_completed": len(all_results),
                "steps": all_results,
                "budget": self._budget.snapshot(),
            }
            if completed_steps:
                result["resumed_from_step"] = len(completed_steps)
            return result

        except Exception as e:
            self.state_mgr.fail_task(task_id, str(e))
            self._emit("error", {"task_id": task_id, "error": str(e)})
            return {"success": False, "task_id": task_id, "error": str(e)}

    # ── Reflexion: cross-task lessons ────────────────────────────────────────

    def _compose_reflexion_hints(self, query: str, task_id: str, k: int = 3) -> str:
        try:
            lessons = self._reflexion.search_lessons(
                query=query, k=k, exclude_task_id=task_id, only_interesting=True,
            )
        except Exception:
            lessons = []
        if not lessons:
            return ""
        lines = ["[Lessons from prior similar tasks — consider as you plan]"]
        for i, r in enumerate(lessons, start=1):
            kind = r.get("decision", "")
            reason = (r.get("reason") or "").strip()
            hint = (r.get("replan_hint") or "").strip()
            tail = f" — {hint}" if hint else ""
            lines.append(f"{i}. ({kind}) {reason}{tail}")
        return "\n".join(lines)

    # ── Archival memory: retrieval + outcome archival ────────────────────────

    def _compose_memory_hints(self, query: str, config: Dict[str, Any], k: int = 3) -> str:
        """Search the archival store; render top-K as a planner-prompt block."""
        try:
            user_id = (config.get("user_id") or "").strip() or None
            hits = self._archival.search(query, k=k, user_id=user_id)
        except Exception:
            hits = []
        if not hits:
            return ""
        lines = ["[Relevant facts from prior tasks — treat as background, verify before relying on numbers]"]
        for i, h in enumerate(hits, start=1):
            snippet = (h.get("content") or "")[:300]
            lines.append(f"{i}. ({h.get('type', 'fact')}) {snippet}")
        return "\n".join(lines)

    def _archive_task_outcome(self, query: str, final: str, config: Dict[str, Any]) -> None:
        """Persist a compact summary of this run so similar future tasks benefit.

        We store a one-paragraph summary, NOT the full report. Long final
        reports are kept in agent_tasks.result; archival memory is meant to be
        keyword-searchable hints, not blob storage.
        """
        try:
            user_id = (config.get("user_id") or "").strip() or None
            summary = (final or "").strip()
            if not summary:
                return
            summary = summary.replace("\n\n", " ").replace("\n", " ")
            if len(summary) > 800:
                summary = summary[:800] + " …"
            content = f"For query: \"{query[:140]}\" → {summary}"
            self._archival.save(
                content=content, user_id=user_id, memory_type="task_outcome",
                metadata={"query": query},
            )
        except Exception:
            # Archival is best-effort; never break the success path.
            pass

    # ── Skill library: retrieval + distillation ──────────────────────────────

    def _compose_skill_hints(self, query: str, k: int = 3) -> str:
        """Search the library; return a planner-prompt-ready block, or empty string."""
        try:
            hits = self._skills.search(query, k=k)
        except Exception:
            hits = []
        if not hits:
            self._retrieved_skill_ids = []
            return ""
        self._retrieved_skill_ids = [h["id"] for h in hits]
        lines = ["[Available reusable recipes from past similar tasks — adapt or ignore as you see fit]"]
        for i, h in enumerate(hits, start=1):
            recipe = h.get("recipe") or {}
            steps = recipe.get("steps") or []
            step_summary = " → ".join(s.get("name", "") for s in steps[:6])
            lines.append(
                f"{i}. {h['name']} — {h['description']}\n   Recipe: {step_summary}"
            )
        return "\n".join(lines)

    def _record_used_skills(self) -> None:
        for sid in self._retrieved_skill_ids:
            try:
                self._skills.record_use(sid)
            except Exception:
                pass
        self._retrieved_skill_ids = []

    def _try_distill_skill(
        self,
        task_id: str,
        query: str,
        plan: Dict[str, Any],
        all_results: List[Dict[str, Any]],
        config: Dict[str, Any],
    ) -> None:
        """Ask the LLM if a reusable recipe emerged from this run; save if yes.

        Skipped when the run is already over 80% budget — distillation is a
        bonus, never a blocker. Distillation off by config flag for tests.

        Also skipped when the plan is trivially small (< 2 steps) — single-step
        tasks rarely generalise, so we save the LLM call. (Task #23.)
        """
        if not bool(config.get("agentic", {}).get("distill_skills", True)):
            return
        # Triviality skip: a 0- or 1-step plan is almost never a reusable
        # recipe. Avoid the LLM round-trip on simple lookups.
        plan_steps = plan.get("steps") or []
        if len(plan_steps) < 2:
            return
        # If existing skills already covered this task, we don't need a new one.
        # The retrieved list is reset to [] inside _record_used_skills, but at
        # the moment we reach here it still holds the run's retrieved ids
        # (distillation is called *before* _record_used_skills... checking order).
        if self._retrieved_skill_ids:
            return
        budget = self._budget.snapshot() if self._budget else None
        if budget:
            burn_pct = max(
                budget["tokens_used"] / max(budget["max_tokens"], 1),
                budget["cost_used"] / max(budget["max_cost_usd"], 1e-9),
                budget["wall_s"] / max(budget["max_wall_s"], 1),
                budget["steps_used"] / max(budget["max_steps"], 1),
            )
            if burn_pct > 0.8:
                return

        outline = "\n".join(
            f"- {s.get('name', '')}: {s.get('config', {}).get('query', '')}"
            for s in plan_steps[:12]
        )
        # Distillation prompt — focuses on PLAN SHAPE generalisability, not the
        # specific entities mentioned in the query. The previous version was
        # too conservative (rejected almost every real task as "too specific").
        # (Task #21.)
        prompt = (
            "You are extracting a reusable skill recipe from a successfully completed task.\n\n"
            "Decision rule: propose a skill whenever the PLAN SHAPE (the sequence of step types) "
            "would apply to a different entity by trivial substitution. The query's specific "
            "company / ticker / sector / number / date is IRRELEVANT — only the workflow shape "
            "matters. Example: a plan that does (1) fetch filings, (2) compute ratios, (3) draft "
            "report for company X *is* reusable because the same shape applies to any company Y.\n\n"
            "Only return is_reusable=false if the plan is genuinely one-off — e.g. a single "
            "lookup with no transformation, or steps that hard-code unique identifiers no "
            "substitution could repair.\n\n"
            "When you DO propose a skill: the recipe's step queries should be templated — replace "
            "the original entity references with placeholders like {ticker}, {sector}, {date_range}, "
            "etc. so the skill is genuinely reusable.\n\n"
            f"USER QUERY:\n{query}\n\n"
            f"PLAN STEPS ({len(plan_steps)}):\n{outline}\n\n"
            "Respond with ONLY a JSON object:\n"
            '{ "is_reusable": true|false,\n'
            '  "name": "short kebab-case slug",\n'
            '  "description": "one sentence describing what plan-shape this captures",\n'
            '  "recipe": { "version": 1, "steps": [{"name": "...", "query": "templated query with {placeholders}"}] } }'
        )
        try:
            from finagent_core.core_agent import CoreAgent
            agent = CoreAgent(api_keys=self.api_keys)
            critic_config: Dict[str, Any] = {
                **config,
                "instructions": (
                    "You distill reusable recipes from finished tasks. Lean toward "
                    "is_reusable=true when the plan shape (not the specific entities) "
                    "could apply to other inputs."
                ),
                "tools": [],
                "markdown": False,
                "reasoning": False,
            }
            # Distillation can route to a cheaper/faster model — same lever as
            # reflector_model. (Task #22, applied here too.)
            distill_model = (config.get("agentic", {}) or {}).get("distill_model")
            if distill_model:
                critic_config["model"] = distill_model
            response = agent.run(prompt, critic_config)
            raw = (agent.get_response_content(response) or "").strip()
            if self._budget is not None:
                try:
                    self._budget.record_llm_call(prompt, response)
                except Exception:
                    pass

            parsed = self._parse_distill_json(raw)
            if not parsed or not parsed.get("is_reusable"):
                return
            name = (parsed.get("name") or "").strip()
            description = (parsed.get("description") or "").strip()
            recipe = parsed.get("recipe") or {}
            if not name or not description or not recipe.get("steps"):
                return
            sid = self._skills.add(name=name, description=description, recipe=recipe)
            self._emit("skill_saved",
                       {"task_id": task_id, "skill_id": sid, "name": name})
        except Exception:
            # Distillation is best-effort; never raise into the success path.
            pass

    @staticmethod
    def _parse_distill_json(raw: str) -> Optional[Dict[str, Any]]:
        import re
        text = raw.strip()
        text = re.sub(r"^```(?:json)?\s*", "", text)
        text = re.sub(r"\s*```$", "", text).strip()
        if not text.startswith("{"):
            m = re.search(r"\{.*\}", text, re.DOTALL)
            if m:
                text = m.group(0)
        try:
            import json as _json
            return _json.loads(text)
        except Exception:
            return None

    # ── Re-plan helper ───────────────────────────────────────────────────────

    def _replan(
        self,
        original_query: str,
        config: Dict[str, Any],
        prior_results: List[Dict[str, Any]],
        replan_hint: Optional[str],
    ) -> Dict[str, Any]:
        """
        Ask the planner for a NEW plan covering only the remaining work,
        threading the reflector's critique into the prompt so the planner can
        course-correct. Falls back to the trivial single-step plan on error.
        """
        prior_summary = "\n".join(
            f"- {r.get('description', '')}: {(r.get('response') or '')[:200]}"
            for r in prior_results[-5:]
        ) or "(none)"
        annotated = (
            f"{original_query}\n\n"
            f"[REPLAN] Previously completed {len(prior_results)} step(s):\n"
            f"{prior_summary}\n\n"
            f"Critique from reviewer: {replan_hint or '(no hint)'}\n\n"
            f"Plan ONLY the remaining steps."
        )
        return self._plan_steps(annotated, config)

    # ── Step execution override — records token usage against BudgetGuard ────

    def _execute_step(
        self,
        step: Dict[str, Any],
        original_query: str,
        config: Dict[str, Any],
        step_index: int,
        prior_results: List[Dict[str, Any]],
    ) -> Dict[str, Any]:
        """
        Overrides ResumableTaskRunner._execute_step to record token usage. The
        base method builds a step_prompt and calls CoreAgent.run; we mirror
        that exactly here so we can capture the prompt and Agno response object
        for BudgetGuard accounting. Keeping the prompt format identical to the
        base avoids semantic drift.
        """
        from finagent_core.core_agent import CoreAgent

        context_parts = []
        for j, r in enumerate(prior_results):
            if isinstance(r, dict) and r.get("response"):
                context_parts.append(f"Step {j+1} result: {r['response'][:500]}")
        context = "\n".join(context_parts)

        step_cfg = step.get("config") or {}
        step_desc = step.get("name") or step_cfg.get("query") or original_query
        step_query = step_cfg.get("query") or step_desc

        step_prompt = (
            f"Original goal: {original_query}\n\n"
            f"Prior steps completed:\n{context if context else '(none)'}\n\n"
            f"Current step {step_index + 1}: {step_desc}\n{step_query}\n\n"
            f"Execute this step thoroughly using available tools."
        )

        agent = CoreAgent(api_keys=self.api_keys)
        response = agent.run(step_prompt, config)
        content = agent.get_response_content(response)

        # Best-effort token accounting (records nothing if response.metrics absent).
        if self._budget is not None:
            try:
                self._budget.record_llm_call(step_prompt, response)
            except Exception:
                pass

        return {
            "step": step_index + 1,
            "id": step.get("id", f"step_{step_index + 1}"),
            "description": step_desc,
            "response": content,
        }
