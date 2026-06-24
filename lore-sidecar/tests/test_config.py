import os
from config import Settings


def test_settings_from_env_defaults(monkeypatch):
    for k in ("LORE_DB_HOST", "LORE_DB_USER", "LORE_DB_PASS", "LORE_OLLAMA_URL"):
        monkeypatch.delenv(k, raising=False)
    s = Settings.from_env()
    assert s.db_host == "ac-database"
    assert s.db_world == "acore_world"
    assert s.model == "llama3.1:8b"
    assert s.gen_timeout == 60
    assert s.listen_host == "0.0.0.0"
    assert s.listen_port == 8091
    assert s.debug is False


def test_settings_from_env_overrides(monkeypatch):
    monkeypatch.setenv("LORE_DB_HOST", "db.example")
    monkeypatch.setenv("LORE_DB_PASS", "secret")
    monkeypatch.setenv("LORE_OLLAMA_URL", "http://10.0.0.5:11434")
    monkeypatch.setenv("LORE_MODEL", "qwen2.5:7b")
    monkeypatch.setenv("LORE_GEN_TIMEOUT", "45")
    monkeypatch.setenv("LORE_LISTEN", "127.0.0.1:9000")
    monkeypatch.setenv("LORE_DEBUG", "1")
    s = Settings.from_env()
    assert s.db_host == "db.example"
    assert s.db_pass == "secret"
    assert s.ollama_url == "http://10.0.0.5:11434"
    assert s.model == "qwen2.5:7b"
    assert s.gen_timeout == 45
    assert (s.listen_host, s.listen_port) == ("127.0.0.1", 9000)
    assert s.debug is True
