-- test_tooltip.lua — headless regression for the tooltip rewrite (base-cast overrides ONLY).
--
-- Guards the DOUBLE-APPLICATION bug: era spellmod talents are real aura-107/108 spellmods whose
-- SMSG_SET_FLAT/PCT_SPELL_MODIFIER packets the client applies to tooltips NATIVELY (live report:
-- Improved Mind Blast 4/5 showed a 4 sec cooldown for an actual 6, base 8 — native -2s + the old
-- addon rewrite -2s). ApplyToTooltip must therefore leave spellmod-only lines UNTOUCHED and rewrite
-- only castTimeBase spells (whose client DBC base is wrong by design, e.g. Vanilla Corruption),
-- idempotently and with learned castTime mods folded in.
--
-- Run: lua5.1 client-addons-src/EraTalents/test_tooltip.lua   (cwd = repo root)

-- --- Minimal WoW API mock (must exist before Tooltip.lua's functions run) ---
local function FontString(text) return { _t = text, GetText = function(s) return s._t end,
                                          SetText = function(s, v) s._t = v end } end

local function makeTooltip(name, leftLines, rightLines)
  local tt = { _name = name, GetName = function(s) return s._name end,
               NumLines = function() return math.max(#leftLines, #rightLines) end,
               Show = function() end }
  for i, v in ipairs(leftLines)  do _G[name .. "TextLeft"  .. i] = FontString(v) end
  for i, v in ipairs(rightLines) do _G[name .. "TextRight" .. i] = FontString(v) end
  return tt
end

hooksecurefunc = function() end   -- InitTooltips must be a no-op at load

dofile("client-addons-src/EraTalents/Tooltip.lua")
local ET = EraTalents

ET._classId = 8
ET.state = { isEraChar = true, era = 0, ranks = { [18021] = 3, [18250] = 4 } }
EraTalentsData = { [0] = { [8] = {
  spellMods = {
    -- A pure spellmod talent (3/3 Improved Fire Blast, -1500ms cooldown): the client renders this
    -- natively from the modifier packets — the addon must NOT touch the line.
    ["Fire Blast"] = { { talent = 18021, attr = "cooldown", kind = "flat", vals = { -500, -1000, -1500 } } },
    -- castTime mods on a castTimeBase spell (4/5 "Improved Corruption"-alike, -1600ms).
    ["Corruption"] = { { talent = 18250, attr = "castTime", kind = "flat", vals = { -400, -800, -1200, -1600, -2000 } } },
  },
  castTimeBase = { ["Corruption"] = 2000 },
} } }

-- 1) Spellmod-only spell: EVERY line stays exactly as the client populated it — including a line
--    the client has ALREADY natively modified (the double-application regression).
local tt = makeTooltip("MockTT", { "Fire Blast" }, { "", "6.5 sec cooldown" })
ET.ApplyToTooltip(tt)
assert(_G["MockTTTextRight2"]:GetText() == "6.5 sec cooldown",
  "natively-modified cooldown line must be left alone, got " .. _G["MockTTTextRight2"]:GetText())
local tt1b = makeTooltip("MockTT1b", { "Fire Blast" }, { "", "8 sec cooldown" })
ET.ApplyToTooltip(tt1b)
assert(_G["MockTT1bTextRight2"]:GetText() == "8 sec cooldown",
  "spellmod cooldown lines are the client's job, got " .. _G["MockTT1bTextRight2"]:GetText())

-- 2) castTimeBase spell: the wrong "Instant" base line is recomputed from baseMs + learned
--    castTime mods (2000 - 1600 = 0.4 sec), and re-fires are idempotent.
local tt2 = makeTooltip("MockTT2", { "Corruption", "Instant cast" }, { "" })
ET.ApplyToTooltip(tt2)
assert(_G["MockTT2TextLeft2"]:GetText() == "0.4 sec cast",
  "castTimeBase 2000 with -1600 learned should show 0.4 sec cast, got " .. _G["MockTT2TextLeft2"]:GetText())
ET.ApplyToTooltip(tt2)
ET.ApplyToTooltip(tt2)   -- re-fires without repopulate must not compound
assert(_G["MockTT2TextLeft2"]:GetText() == "0.4 sec cast",
  "re-fires must be idempotent, got " .. _G["MockTT2TextLeft2"]:GetText())

-- 3) castTimeBase at 0 learned ranks still overrides the wrong base (2 sec), and at a full -2000
--    it renders Instant.
ET.state.ranks[18250] = 0
local tt3 = makeTooltip("MockTT3", { "Corruption", "Instant" }, { "" })
ET.ApplyToTooltip(tt3)
assert(_G["MockTT3TextLeft2"]:GetText() == "2 sec cast",
  "0 ranks: Instant -> 2 sec cast, got " .. _G["MockTT3TextLeft2"]:GetText())
ET.state.ranks[18250] = 5
local tt4 = makeTooltip("MockTT4", { "Corruption", "2 sec cast" }, { "" })
ET.ApplyToTooltip(tt4)
assert(_G["MockTT4TextLeft2"]:GetText() == "Instant",
  "5/5 ranks: -> Instant, got " .. _G["MockTT4TextLeft2"]:GetText())

-- 4) RewriteCastBase primitive: matches only cast lines, recomputes from baseMs (idempotent).
assert(ET.RewriteCastBase("Instant cast", 2000, -1200, 1) == "0.8 sec cast",
  "-1200 -> 0.8 sec cast, got " .. tostring(ET.RewriteCastBase("Instant cast", 2000, -1200, 1)))
assert(ET.RewriteCastBase("2 sec cast", 2000, -1200, 1) == "0.8 sec cast",
  "re-fire on an already-rewritten line recomputes from baseMs (idempotent)")
assert(ET.RewriteCastBase("100 Mana", 2000, 0, 1) == nil,
  "non-cast lines are not touched by the base-cast rewriter")

-- 5) A WotLK (non-era) char: no rewriting at all, even for castTimeBase names.
ET.state.isEraChar = false
local tt5 = makeTooltip("MockTT5", { "Corruption", "Instant cast" }, { "" })
ET.ApplyToTooltip(tt5)
assert(_G["MockTT5TextLeft2"]:GetText() == "Instant cast",
  "WotLK chars keep native tooltips, got " .. _G["MockTT5TextLeft2"]:GetText())

print("tooltip OK")
