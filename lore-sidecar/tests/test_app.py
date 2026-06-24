from fastapi.testclient import TestClient
from app import create_app

BOT = {"name": "Wizard", "level": 34, "class": "mage", "race": "Gnome",
       "faction": "Alliance", "map": 0, "zone": "Elwynn",
       "x": 0.0, "y": 0.0, "z": 0.0, "active_quests": [], "snapshot": "snap"}


class FakeLlm:
    def __init__(self, intent, reply="head north"):
        self._intent = intent
        self._reply = reply
        self.phrased = False

    def classify(self, q, recent=None):
        return self._intent

    def phrase(self, q, facts, bot):
        self.phrased = True
        return self._reply


class FakeDb:
    pass


def _client(llm, facts, capture=None):
    import app as appmod

    def fake_dispatch(skill, ent, bot, db, player_quests=None):
        if capture is not None:
            capture["player_quests"] = player_quests
        return facts

    app_ = create_app(llm=llm, db=FakeDb(), dispatch=fake_dispatch)
    return TestClient(app_)


def test_ask_happy_path():
    llm = FakeLlm({"skill": "find_service_npc", "entities": {"service": "vendor"}})
    c = _client(llm, facts={"name": "Bob", "direction": "north"})
    r = c.post("/ask", json={"bot": BOT, "player": "Lewis", "question": "vendor?"})
    body = r.json()
    assert r.status_code == 200
    assert body["ok"] is True
    assert body["reply"] == "head north"
    assert body["matched_skill"] == "find_service_npc"


def test_ask_chitchat_returns_ok_false_without_phrasing():
    llm = FakeLlm({"skill": "chitchat", "entities": {}})
    c = _client(llm, facts=None)
    r = c.post("/ask", json={"bot": BOT, "player": "Lewis", "question": "ugh"})
    assert r.json() == {"ok": False, "reply": "", "matched_skill": "chitchat"}
    assert llm.phrased is False


def test_ask_no_facts_returns_ok_false():
    llm = FakeLlm({"skill": "item_info", "entities": {"item": "nope"}})
    c = _client(llm, facts=None)
    r = c.post("/ask", json={"bot": BOT, "player": "Lewis", "question": "what is nope"})
    body = r.json()
    assert body["ok"] is False and body["reply"] == ""


def test_ask_internal_error_returns_ok_false():
    class BoomLlm:
        def classify(self, q, recent=None):
            raise RuntimeError("ollama down")

    c = _client(BoomLlm(), facts=None)
    r = c.post("/ask", json={"bot": BOT, "player": "Lewis", "question": "vendor?"})
    assert r.status_code == 200
    assert r.json()["ok"] is False


def test_healthz():
    llm = FakeLlm({"skill": "chitchat", "entities": {}})
    c = _client(llm, facts=None)
    assert c.get("/healthz").json() == {"ok": True}


def test_ask_forwards_player_quests_to_dispatch():
    llm = FakeLlm({"skill": "quest_info", "entities": {"quest": "red linen"}})
    cap = {}
    c = _client(llm, facts={"title": "Red Linen Goods"}, capture=cap)
    pq = [{"id": 26, "title": "Red Linen Goods", "objectives": []}]
    r = c.post("/ask", json={"bot": BOT, "player": "Lewis", "question": "what do I do for red linen?",
                             "player_quests": pq})
    assert r.json()["ok"] is True
    assert cap["player_quests"] == pq


def test_ask_forwards_recent_to_classify():
    captured = {}

    class CapLlm:
        def classify(self, q, recent=None):
            captured["recent"] = recent
            return {"skill": "chitchat", "entities": {}}

        def phrase(self, q, facts, bot):
            return "x"

    import app as appmod
    app_ = appmod.create_app(llm=CapLlm(), db=FakeDb(), dispatch=lambda s, e, b, d, pq=None: None)
    c = TestClient(app_)
    recent = [{"player": "p", "bot": "b"}]
    c.post("/ask", json={"bot": BOT, "player": "L", "question": "what town is he in?", "recent": recent})
    assert captured["recent"] == recent
