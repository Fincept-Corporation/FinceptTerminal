# Geopolitics agents

Twenty geopolitical analyst personas, defined entirely in
`configs/agent_definitions.json` and run by `../finagent_core` (see
`../README.md`). There is no per-agent Python code.

| Framework | Personas |
|---|---|
| *Prisoners of Geography* (Tim Marshall) — how terrain constrains each region | Russia, China, USA, Europe, Middle East, Africa, India-Pakistan, Japan-Korea, Latin America, Arctic |
| *World Order* (Henry Kissinger) — competing conceptions of international order | American, Chinese, European, Islamic, Multipolar |
| *The Grand Chessboard* (Zbigniew Brzezinski) — Eurasian strategy | Eurasian Balkans, Geopolitical Pivots, Active Geostrategic Players, American Primacy, Eurasian Heartland |

Each entry's `config.instructions` holds the persona's analytical framework;
`config.model` is only a default — the user's LLM settings decide the provider
and key at run time. To add or change a persona, edit the JSON.
