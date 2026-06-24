# lore-sidecar

Tier-2 of mod-playerbot-chatter: answers a whispered factual question from real
WoW world data, in the bot's voice. Pipeline: **classify** (Ollama JSON) →
**retrieve** (MySQL `acore_world`) → **phrase** (Ollama). Started as the `ac-lore`
container by `setup.sh` when `LORE_ENABLE=1`.

- Run tests: `python -m pytest -q`
- Run locally: `LORE_DB_PASS=... LORE_OLLAMA_URL=http://HOST:11434 \
  uvicorn app:build_default_app --factory --port 8091`
- Container self-check: `docker compose exec ac-lore python selfcheck.py`

Config is env-only (see `config.py`). The worldserver reaches this at
`http://ac-lore:8091/ask`. Reads are read-only via the least-privilege `lore`
MySQL user. Swapping Ollama for another provider is isolated to `llm.py`'s
`_generate`.
