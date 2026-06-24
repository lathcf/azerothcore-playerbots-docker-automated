"""Environment-driven settings for the lore sidecar."""
from __future__ import annotations

import os
from dataclasses import dataclass


@dataclass(frozen=True)
class Settings:
    db_host: str
    db_port: int
    db_user: str
    db_pass: str
    db_world: str
    ollama_url: str
    model: str
    gen_timeout: int
    listen_host: str
    listen_port: int
    debug: bool

    @staticmethod
    def from_env() -> "Settings":
        listen = os.environ.get("LORE_LISTEN", "0.0.0.0:8091")
        host, _, port = listen.partition(":")
        return Settings(
            db_host=os.environ.get("LORE_DB_HOST", "ac-database"),
            db_port=int(os.environ.get("LORE_DB_PORT", "3306")),
            db_user=os.environ.get("LORE_DB_USER", "lore"),
            db_pass=os.environ.get("LORE_DB_PASS", ""),
            db_world=os.environ.get("LORE_DB_WORLD", "acore_world"),
            ollama_url=os.environ.get("LORE_OLLAMA_URL", "http://localhost:11434"),
            model=os.environ.get("LORE_MODEL", "llama3.1:8b"),
            gen_timeout=int(os.environ.get("LORE_GEN_TIMEOUT", "60")),
            listen_host=host or "0.0.0.0",
            listen_port=int(port or "8091"),
            debug=os.environ.get("LORE_DEBUG", "0") == "1",
        )
