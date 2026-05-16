# Agentic System Research — FinceptTerminal

This directory contains the research and design output for upgrading FinceptTerminal from a tool-using chatbot into a **true agentic system** capable of long-running autonomous tasks (e.g., "research company X for the next hour and write a report").

**Method:** ~1 hour of structured research using parallel research agents (foundational papers, production frameworks, durability patterns), all claims verified against arxiv abstracts and primary engineering blog URLs. Zero hallucination policy enforced.

## Layout

```
00_PROGRESS.md            Progress log + running decision record
01_self_grill.md          Pre-research scope-and-constraints questioning
papers/SUMMARY.md         10 foundational papers, arxiv-verified
frameworks/SUMMARY.md     11 production frameworks (LangGraph, OpenAI, Anthropic, etc.)
patterns/SUMMARY.md       10 long-running/durable-execution patterns + pattern menu
design/
  02_codebase_inventory.md   What already exists in this repo
  03_architecture.md         Target architecture, fully cited
  04_impl_plan.md            Three-phase implementation plan
```

## Read order (suggested)

1. `design/03_architecture.md` — start here for the design (sections 1, 4, 5 if short on time).
2. `design/02_codebase_inventory.md` — what we already have (≈80% of agentic primitives already in place).
3. `design/04_impl_plan.md` — concrete file-by-file changes per phase.
4. `papers/SUMMARY.md`, `frameworks/SUMMARY.md`, `patterns/SUMMARY.md` — source material with citations.

## Headline conclusions

- **What we have:** `ResumableTaskRunner` + `TaskStateManager` (`scripts/agents/finagent_core/task_state.py`) already implement Plan-and-Solve with SQLite checkpointing. Python `main.py` already has `start_task/resume_task/get_task/list_tasks` actions (lines 833, 850, 865, 876). RD-Agent integrated (autonomous quant R&D).
- **What's missing:** (1) C++ `AgentService` doesn't expose the task lifecycle methods, (2) no Tasks UI panel, (3) per-step events not streamed during long runs, (4) plan is generated once, no adaptive re-planning, (5) no reflection / self-correction, (6) no budget controller, (7) no HITL interrupt, (8) no skill library / cross-task memory, (9) no eval harness.
- **The simplest durable design** that closes the long-running-task gap is the LangGraph-style `(thread_id, step, state_blob)` checkpoint on SQLite — which `TaskStateManager` already approximates. The work is wiring, exposure, and adding the reflection + budget layers.
- **Reframed scope:** Original framing of "build true agentic from scratch" was wrong. Real work = 2–3 days for MVP (Phase 1), 5–7 days for the agentic upgrades (Phase 2), 1–2 weeks for compounding memory & skills (Phase 3).

## Key citations

- Anthropic, "Building Effective Agents" (Dec 2024): https://www.anthropic.com/research/building-effective-agents
- Anthropic, "How we built our multi-agent research system" (Jun 2025): https://www.anthropic.com/engineering/multi-agent-research-system
- Cognition, "Don't Build Multi-Agents" (2025): https://cognition.ai/blog/dont-build-multi-agents
- LangGraph persistence: https://docs.langchain.com/oss/python/langgraph/persistence
- LangGraph interrupts: https://docs.langchain.com/oss/python/langgraph/interrupts
- Letta/MemGPT: https://docs.letta.com/concepts/memgpt/, https://arxiv.org/abs/2310.08560
- Voyager: https://arxiv.org/abs/2305.16291
- Reflexion: https://arxiv.org/abs/2303.11366
- ReAct: https://arxiv.org/abs/2210.03629
- Plan-and-Solve: https://arxiv.org/abs/2305.04091
- ReWOO: https://arxiv.org/abs/2305.18323
- Temporal durable execution: https://temporal.io/blog/what-is-durable-execution
- Manus context engineering: https://manus.im/blog/Context-Engineering-for-AI-Agents-Lessons-from-Building-Manus
- Devin 2.0: https://cognition.ai/blog/devin-2
- OpenTelemetry GenAI semconv: https://opentelemetry.io/docs/specs/semconv/gen-ai/gen-ai-agent-spans/
- Berkeley benchmark audit (Apr 2026): https://rdi.berkeley.edu/blog/trustworthy-benchmarks-cont/
