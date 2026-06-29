"""Ollama classify + phrase. Provider-agnostic seam (_generate) for a future swap."""
from __future__ import annotations

import json
import re
from typing import Optional

import requests

_VALID_SKILLS = {"find_service_npc", "item_info", "quest_info",
                 "dungeon_info", "where_to_level", "where_is_npc", "chitchat"}

_CLASSIFY_SYSTEM = (
    "You classify a World of Warcraft player's chat message into one structured intent. "
    "Output ONLY a JSON object, no prose. Schema: "
    '{"skill": <one of find_service_npc|item_info|quest_info|dungeon_info|where_to_level|where_is_npc|chitchat>, '
    '"entities": {...}}. '
    "find_service_npc entities: service (class_trainer|profession_trainer|vendor|repair|"
    "flight_master|innkeeper|banker|auctioneer|mailbox), optional profession, optional class. "
    "item_info entities: item (name), aspect (stats|drop|value|general). "
    "quest_info entities: quest (name or empty), aspect (objective|turnin). "
    "dungeon_info entities: dungeon (name), aspect (level|location|bosses|general). "
    "where_to_level entities: {}. "
    "where_is_npc entities: npc (name). "
    "If the message is just banter or has no factual lookup, use skill chitchat with empty entities. "
    "If the message uses a pronoun like he/she/they/that npc, use the recent conversation to fill the npc name. "
    "Examples: "
    '"where do I train mining" -> {"skill":"find_service_npc","entities":{"service":"profession_trainer","profession":"mining"}}. '
    '"where is the nearest paladin trainer" -> {"skill":"find_service_npc","entities":{"service":"class_trainer","class":"paladin"}}. '
    '"what does broken fang sell for" -> {"skill":"item_info","entities":{"item":"broken fang","aspect":"value"}}. '
    '"where do I turn in this quest" -> {"skill":"quest_info","entities":{"quest":"","aspect":"turnin"}}. '
    '"where is Thrall / what town is he in" -> {"skill":"where_is_npc","entities":{"npc":"Thrall"}}. '
    '"ugh this grind sucks" -> {"skill":"chitchat","entities":{}}.'
)

_PHRASE_SYSTEM = (
    "You are a relaxed WoW player answering another player in one short, casual chat line. "
    "Use ONLY the facts provided — never invent names, places, or numbers. If a direction or "
    "distance is given, mention it naturally. If the facts say not_nearby, say it's NOT close "
    "by (a long way off / back in a city) and do NOT make up a distance or direction. "
    "No markdown, no emojis, no quotes, one line."
)


def clean(s: str) -> str:
    s = re.sub(r"[*`]+", "", s or "")
    s = re.sub(r"\s+", " ", s).strip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        s = s[1:-1].strip()
    return s


def parse_intent(raw: str) -> dict:
    """Extract the first JSON object; coerce to a valid intent; junk -> chitchat."""
    if raw:
        m = re.search(r"\{.*\}", raw, re.DOTALL)
        if m:
            try:
                obj = json.loads(m.group(0))
                skill = obj.get("skill")
                if skill not in _VALID_SKILLS:
                    skill = "chitchat"
                entities = obj.get("entities")
                if not isinstance(entities, dict):
                    entities = {}
                return {"skill": skill, "entities": entities}
            except (ValueError, TypeError):
                pass
    return {"skill": "chitchat", "entities": {}}


# Deterministic entity recovery for trainer questions. Small models frequently drop the
# named class/profession (e.g. "nearest paladin trainer"); the text is unambiguous, so we
# trust it over the model — and crucially never fall back to the bot's own class.
CLASS_WORDS = ["death knight", "warrior", "paladin", "hunter", "rogue", "priest",
               "shaman", "mage", "warlock", "druid"]
PROFESSION_WORDS = ["mining", "herbalism", "skinning", "alchemy", "blacksmithing",
                    "enchanting", "engineering", "leatherworking", "tailoring",
                    "jewelcrafting", "inscription", "first aid", "cooking", "fishing"]


def _find_word(words: list, text: str) -> Optional[str]:
    for w in words:
        if re.search(r"\b" + re.escape(w) + r"\b", text):
            return w
    return None


def refine_intent(intent: dict, question: str) -> dict:
    """Override the model when the question unambiguously names a trainer's class/profession.

    Only fires for training/trainer questions; otherwise returns the model's intent as-is.
    A profession match wins over a class match (e.g. "mining trainer").
    """
    q = (question or "").lower()
    if "train" in q:  # matches both 'train' and 'trainer'
        prof = _find_word(PROFESSION_WORDS, q)
        if prof:
            return {"skill": "find_service_npc",
                    "entities": {"service": "profession_trainer", "profession": prof}}
        klass = _find_word(CLASS_WORDS, q)
        if klass:
            return {"skill": "find_service_npc",
                    "entities": {"service": "class_trainer", "class": klass}}
    return intent


def _facts_to_text(facts: dict) -> str:
    return json.dumps(facts, ensure_ascii=False)


class Llm:
    def __init__(self, settings):
        self._settings = settings

    def _generate(self, prompt: str, system: str) -> str:
        s = self._settings
        url = s.ollama_url.rstrip("/") + "/api/generate"
        body = {"model": s.model, "system": system, "prompt": prompt,
                "stream": False, "think": False}
        # Per-call ceiling (LORE_GEN_TIMEOUT). An 8B model on a shared GPU under chatter
        # contention is slow, so match the proven chatter module's 60s default. /ask makes
        # TWO sequential calls (classify + phrase); the C++ PlayerbotChatter.LoreTimeout
        # bounds the total and falls back gracefully if the pair runs long.
        resp = requests.post(url, json=body, timeout=s.gen_timeout)
        resp.raise_for_status()
        return resp.json().get("response", "")

    def classify(self, question: str, recent=None) -> dict:
        ctx = ""
        if recent:
            lines = "\n".join(f"Player: {t.get('player','')}\nYou: {t.get('bot','')}" for t in recent)
            ctx = f"Recent conversation (for resolving pronouns):\n{lines}\n\n"
        prompt = f'{ctx}Classify this message:\n"{question}"'
        intent = parse_intent(self._generate(prompt, _CLASSIFY_SYSTEM))
        return refine_intent(intent, question)

    def phrase(self, question: str, facts: dict, bot: dict) -> str:
        prompt = (
            f"A player asked: \"{question}\"\n"
            f"Here are the real facts you must answer from:\n{_facts_to_text(facts)}\n"
            "Reply in one short, casual line."
        )
        return clean(self._generate(prompt, _PHRASE_SYSTEM))
