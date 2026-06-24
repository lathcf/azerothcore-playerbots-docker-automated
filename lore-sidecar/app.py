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
        question = payload.get("question", "")
        try:
            intent = llm.classify(question, payload.get("recent"))
            skill = intent.get("skill", "chitchat")
            if skill == "chitchat":
                return {"ok": False, "reply": "", "matched_skill": "chitchat"}
            facts = dispatch(skill, intent.get("entities", {}), bot, db,
                             payload.get("player_quests") or [])
            if not facts:
                return {"ok": False, "reply": "", "matched_skill": skill}
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
