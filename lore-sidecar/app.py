"""FastAPI lore sidecar. POST /ask -> {ok, reply, matched_skill}."""
from __future__ import annotations

import logging

from fastapi import FastAPI, Request

import skills as skills_module

log = logging.getLogger("lore")


def create_app(llm, db, dispatch=skills_module.dispatch) -> FastAPI:
    app = FastAPI()

    @app.get("/healthz")
    def healthz():
        return {"ok": True}

    @app.post("/ask")
    async def ask(req: Request):
        payload = await req.json()
        bot = payload.get("bot", {})
        asker = payload.get("asker") or {}
        question = payload.get("question", "")
        try:
            intent = llm.classify(question, payload.get("recent"))
            skill = intent.get("skill", "chitchat")
            if skill == "chitchat":
                return {"ok": False, "reply": "", "matched_skill": "chitchat"}
            entities = intent.get("entities", {}) or {}
            facts = dispatch(skill, entities, bot, db,
                             payload.get("player_quests") or [], asker=asker)
            if not facts:
                # Honest miss: a factual question we can't answer must NOT return ok:false —
                # that hands the whisper to the C++ free-form reply, which invents a place.
                facts = {"not_found": True,
                         "asked_about": skills_module.describe_ask(skill, entities)}
            reply = llm.phrase(question, facts, bot)
            if not reply:
                return {"ok": False, "reply": "", "matched_skill": skill}
            return {"ok": True, "reply": reply, "matched_skill": skill}
        except Exception:  # never surface a 500 to the worldserver
            log.exception("lore /ask failed")
            return {"ok": False, "reply": "", "matched_skill": "error"}

    return app


def build_default_app() -> FastAPI:
    from config import Settings
    from db import Db
    from llm import Llm

    settings = Settings.from_env()
    logging.basicConfig(level=logging.DEBUG if settings.debug else logging.INFO)
    return create_app(llm=Llm(settings), db=Db(settings))
