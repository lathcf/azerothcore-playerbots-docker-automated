import json
import llm


def test_parse_intent_clean_json():
    out = llm.parse_intent('{"skill": "item_info", "entities": {"item": "x", "aspect": "drop"}}')
    assert out["skill"] == "item_info"
    assert out["entities"]["aspect"] == "drop"


def test_parse_intent_wrapped_in_prose_and_fence():
    raw = "Sure!\n```json\n{\"skill\": \"where_to_level\", \"entities\": {}}\n```\nhope that helps"
    out = llm.parse_intent(raw)
    assert out["skill"] == "where_to_level"
    assert out["entities"] == {}


def test_parse_intent_unknown_skill_becomes_chitchat():
    assert llm.parse_intent('{"skill": "make_coffee", "entities": {}}')["skill"] == "chitchat"


def test_parse_intent_garbage_becomes_chitchat():
    out = llm.parse_intent("I have no idea what you mean")
    assert out == {"skill": "chitchat", "entities": {}}


def test_clean_strips_decoration():
    assert llm.clean('  "*waves* sure, **head** north`"  ') == "waves sure, head north"


def test_clean_collapses_newlines():
    assert llm.clean("line one\nline two") == "line one line two"


def test_classify_uses_generate(monkeypatch):
    captured = {}

    def fake_gen(self, prompt, system, json_format=False):
        captured["prompt"] = prompt
        captured["json_format"] = json_format
        return '{"skill": "find_service_npc", "entities": {"service": "vendor"}}'

    monkeypatch.setattr(llm.Llm, "_generate", fake_gen)
    out = llm.Llm(settings=None).classify("where can I sell stuff?")
    assert out["skill"] == "find_service_npc"
    assert "where can I sell stuff?" in captured["prompt"]
    assert captured["json_format"] is True  # classify asks Ollama for JSON mode


def test_phrase_uses_generate_and_cleans(monkeypatch):
    monkeypatch.setattr(llm.Llm, "_generate",
                        lambda self, prompt, system: '"head north, about 50 yards"')
    out = llm.Llm(settings=None).phrase("where's a vendor?",
                                        {"name": "Bob", "direction": "north", "distance_yards": 50},
                                        {"name": "Wiz"})
    assert out == "head north, about 50 yards"


def test_refine_intent_recovers_class_trainer_from_question():
    # model dropped the class; the question names it unambiguously -> trust the text
    out = llm.refine_intent({"skill": "chitchat", "entities": {}},
                            "where's the nearest paladin trainer?")
    assert out == {"skill": "find_service_npc",
                   "entities": {"service": "class_trainer", "class": "paladin"}}


def test_refine_intent_recovers_profession_over_class():
    out = llm.refine_intent({"skill": "find_service_npc", "entities": {"service": "vendor"}},
                            "where do I train mining")
    assert out["entities"] == {"service": "profession_trainer", "profession": "mining"}


def test_refine_intent_no_class_named_keeps_model_intent():
    # generic "where do I train" -> let the skill use the bot's own class
    base = {"skill": "find_service_npc", "entities": {"service": "class_trainer"}}
    assert llm.refine_intent(base, "where do I train") == base


def test_refine_intent_ignores_non_trainer_questions():
    base = {"skill": "item_info", "entities": {"item": "x"}}
    assert llm.refine_intent(base, "what does x sell for") == base


def test_where_is_npc_is_a_valid_skill():
    out = llm.parse_intent('{"skill": "where_is_npc", "entities": {"npc": "Thrall"}}')
    assert out["skill"] == "where_is_npc"
    assert out["entities"]["npc"] == "Thrall"


def test_classify_includes_recent_context_in_prompt(monkeypatch):
    captured = {}

    def fake_gen(self, prompt, system, json_format=False):
        captured["prompt"] = prompt
        return '{"skill": "where_is_npc", "entities": {"npc": "Brock"}}'

    monkeypatch.setattr(llm.Llm, "_generate", fake_gen)
    recent = [{"player": "where's a mining trainer?", "bot": "Brock Stoneseeker is in Lakeshire"}]
    out = llm.Llm(settings=None).classify("what town is he in?", recent=recent)
    assert out["skill"] == "where_is_npc"
    assert "Brock Stoneseeker is in Lakeshire" in captured["prompt"]


# --- deterministic fast path ---------------------------------------------------------------

def _svc(service):
    return {"skill": "find_service_npc", "entities": {"service": service}}


def test_fast_intent_service_rules_with_location_cue():
    assert llm.fast_intent("where's the nearest flight master?") == _svc("flight_master")
    assert llm.fast_intent("anyone know where the fp is") == _svc("flight_master")
    assert llm.fast_intent("where is the inn") == _svc("innkeeper")
    assert llm.fast_intent("closest bank?") == _svc("banker")
    assert llm.fast_intent("where's the auction house") == _svc("auctioneer")
    assert llm.fast_intent("where is the ah") == _svc("auctioneer")
    assert llm.fast_intent("find a mail box") == _svc("mailbox")
    assert llm.fast_intent("where can I repair") == _svc("repair")
    assert llm.fast_intent("nearest vendor?") == _svc("vendor")


def test_fast_intent_requires_location_cue_and_word_boundaries():
    assert llm.fast_intent("my repair bill sucks") is None
    assert llm.fast_intent("where did you have dinner at bankside?") is None
    assert llm.fast_intent("this vendor trash is worthless") is None
    assert llm.fast_intent("ah, where are you?") is None  # leading 'ah' is an interjection
    assert llm.fast_intent("what fps do you get?") is None
    assert llm.fast_intent("") is None


def test_fast_intent_trainer_rule():
    assert llm.fast_intent("where's the nearest paladin trainer?") == {
        "skill": "find_service_npc", "entities": {"service": "class_trainer", "class": "paladin"}}
    assert llm.fast_intent("where do I train mining")["entities"] == {
        "service": "profession_trainer", "profession": "mining"}


def test_classify_fast_path_skips_model(monkeypatch):
    def boom(self, prompt, system, json_format=False):
        raise AssertionError("model must not be called on a fast-path hit")

    monkeypatch.setattr(llm.Llm, "_generate", boom)
    out = llm.Llm(settings=None).classify("where's the nearest paladin trainer?")
    assert out["entities"] == {"service": "class_trainer", "class": "paladin"}
    assert llm.Llm(settings=None).classify("where is the bank?") == _svc("banker")


def test_classify_model_path_still_refines(monkeypatch):
    # No fast-path match ("training" with no class word) -> model runs; refine still applies.
    monkeypatch.setattr(llm.Llm, "_generate",
                        lambda self, p, s, json_format=False: '{"skill":"chitchat","entities":{}}')
    out = llm.Llm(settings=None).classify("ugh this grind sucks")
    assert out == {"skill": "chitchat", "entities": {}}


def test_generate_sends_json_format_only_when_asked(monkeypatch):
    sent = []

    class Resp:
        def raise_for_status(self):
            pass

        def json(self):
            return {"response": "{}"}

    def fake_post(url, json=None, timeout=None):
        sent.append(json)
        return Resp()

    class S:
        ollama_url = "http://x"
        model = "m"
        gen_timeout = 1

    monkeypatch.setattr(llm.requests, "post", fake_post)
    l = llm.Llm(S())
    l.classify("tell me about the deadmines")
    l.phrase("q", {"a": 1}, {})
    assert sent[0].get("format") == "json"
    assert "format" not in sent[1]
