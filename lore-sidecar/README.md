# lore-sidecar

Tier-2 of mod-playerbot-chatter: answers a whispered factual question from real
WoW world data, in the bot's voice. Pipeline: **classify** (deterministic fast path
in `llm.fast_intent` for unambiguous trainer/service questions — no model call — else
Ollama in JSON mode) → **retrieve** (MySQL `acore_world`) → **phrase** (Ollama).
Started as the `ac-lore` container by `setup.sh` when `LORE_ENABLE=1`.

- Location, faction and level come from the payload's `asker` (the whispering player),
  falling back to `bot` when `asker` has no map/x/y; `bot` is only the phrasing voice.
- Service NPCs are faction-filtered on `mod_chatter_npc_area.team` (filled by the
  worldserver backfill; NULL = unknown, fails open).
- Nothing usable nearby → `{"not_nearby", "service", "try": [...]}` naming up to 3
  places world-wide (capitals first, see `curated.CAPITALS`).
- A factual question with no facts is phrased as an honest "not sure" (`not_found`,
  ok:true) — ok:false would hand the whisper to the free-form reply, which guesses.

- Run tests: `python -m pytest -q`
- Run locally: `LORE_DB_PASS=... LORE_OLLAMA_URL=http://HOST:11434 \
  uvicorn app:build_default_app --factory --port 8091`
- Container self-check: `docker compose exec ac-lore python selfcheck.py`

Config is env-only (see `config.py`). The worldserver reaches this at
`http://ac-lore:8091/ask`. Reads are read-only via the least-privilege `lore`
MySQL user. Swapping Ollama for another provider is isolated to `llm.py`'s
`_generate`.
