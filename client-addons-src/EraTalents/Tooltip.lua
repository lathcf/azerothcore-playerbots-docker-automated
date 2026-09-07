-- Client-side spell-tooltip rewrite — era BASE-CAST overrides ONLY (castTimeBase).
--
-- History: this file used to rewrite EVERY talent-modified discrete tooltip line (cooldown, range,
-- cost, cast time) on the premise that era-talent spellmods are SPELL_ATTR0_PASSIVE auras the
-- client never sees. That premise was WRONG: Aura::CanBeSentToClient() hides the AURA, but
-- Player::AddSpellMod unconditionally sends SMSG_SET_FLAT/PCT_SPELL_MODIFIER for every real
-- spellmod (aura 107/108) — the exact mechanism stock WotLK talents use — so the 3.3.5 client
-- already renders those lines modified natively. The old rewrite then applied the reduction a
-- SECOND time on top of the natively-modified line (live report: Improved Mind Blast 4/5 showed a
-- 4 sec cooldown for an actual 6; base 8), because the "first text seen = true base" cache captured
-- an already-modified line. Every generator-emitted `mechanic: spellmod` talent is a real aura
-- 107/108, so the whole generic rewrite was redundant and is now REMOVED.
--
-- What must stay is the era base-cast override: spells whose CLIENT DBC base is wrong by design
-- (generated castTimeBase, e.g. Vanilla Corruption = 2000ms restored by core patch 0019 while the
-- client Spell.dbc still says Instant). Native mods can't fix a wrong base — a flat castTime mod
-- against a 0ms base still renders "Instant" — so for exactly these spells we recompute the cast
-- line from the authoritative baseMs plus the learned castTime mods (spellMods castTime entries,
-- e.g. Improved Corruption). This is idempotent by construction: it never reads the number out of
-- the line, so any number of re-fires yields the same text.
--
-- No WoW API calls at file scope (so the pure fns load under headless lua5.1); the hook installer
-- (ET.InitTooltips) and ApplyToTooltip touch WoW only inside functions.
EraTalents = EraTalents or {}
local ET = EraTalents

-- ---------------------------------------------------------------------------
-- Pure helpers (headless-testable: client-addon/tests/test_tooltip.lua).
-- ---------------------------------------------------------------------------

-- Aggregate a spell's learned mods for one attribute into a flat delta + a percent factor.
-- `entries` is spellMods[spellName]; each has {talent, attr, kind='flat'|'pct', vals[]}. Reads the
-- current rank from state.ranks and picks vals[rank] (the EFFECTIVE signed value at that rank).
-- Returns flat, pctFactor, any -- `any` is false when no learned talent touches this attr.
function ET.AggregateAttr(state, entries, attr)
  local flat, pct, any = 0, 1, false
  local ranks = (state and state.ranks) or {}
  for _, m in ipairs(entries or {}) do
    if m.attr == attr then
      local rank = ranks[m.talent] or 0
      if rank > 0 then
        local v = m.vals[rank]
        if v then
          any = true
          if m.kind == "pct" then pct = pct * (1 + v / 100) else flat = flat + v end
        end
      end
    end
  end
  return flat, pct, any
end

-- Format seconds like the client's time lines: one decimal, trailing ".0" stripped ("3", "3.4").
function ET.FormatSecs(secs)
  if secs < 0 then secs = 0 end
  local tenths = math.floor(secs * 10 + 0.5)
  local whole = math.floor(tenths / 10)
  local frac = tenths - whole * 10
  if frac == 0 then return tostring(whole) end
  return whole .. "." .. frac
end

-- Era base-cast override (generated castTimeBase). It does NOT read the base out of the line — the
-- line is WRONG by definition — it recomputes from baseMs + the learned castTime mods, matching
-- both an "Instant..." line and a "X sec cast" line (idempotent re-fires).
function ET.RewriteCastBase(text, baseMs, flat, pct)
  if not text then return nil end
  if not (string.match(text, "^([%d%.]+)%s+sec cast$") or string.match(text, "^Instant")) then return nil end
  local newSecs = (baseMs / 1000) * (pct or 1) + (flat or 0) / 1000
  if newSecs <= 0 then return "Instant" end
  return ET.FormatSecs(newSecs) .. " sec cast"
end

-- ---------------------------------------------------------------------------
-- Tooltip integration (WoW API -- only inside functions).
-- ---------------------------------------------------------------------------

-- The active tree (spellMods + castTimeBase live on it).
local function ActiveTree()
  if not (EraTalentsData and ET.state) then return nil end
  local byClass = EraTalentsData[ET.state.era]
  return byClass and byClass[ET._classId or 8]
end

-- Rewrite the cast line of a freshly-populated spell tooltip for castTimeBase spells only.
-- Exposed on ET (not local) so the headless test can drive it with a mock tooltip.
function ET.ApplyToTooltip(tooltip)
  if not (ET.state and ET.state.isEraChar) then return end   -- WotLK chars keep native tooltips
  local tree = ActiveTree()
  if not tree then return end
  local name = tooltip.GetName and tooltip:GetName()
  if not name then return end
  local titleFS = _G[name .. "TextLeft1"]
  local spellName = titleFS and titleFS:GetText()
  local baseMs = spellName and tree.castTimeBase and tree.castTimeBase[spellName]
  if not baseMs then return end

  -- The learned castTime mods for this spell (e.g. Improved Corruption); native client mods can't
  -- render against the wrong DBC base, so the override folds them in itself.
  local flat, pct = ET.AggregateAttr(ET.state, tree.spellMods and tree.spellMods[spellName], "castTime")

  -- The cast line sits on the LEFT (rarely RIGHT); RewriteCastBase only matches cast-line text, so
  -- trying every line on both sides is safe. Idempotent: recomputes from baseMs every fire.
  local changed = false
  for i = 1, tooltip:NumLines() do
    for _, side in ipairs({ "TextLeft", "TextRight" }) do
      local fs = _G[name .. side .. i]
      local newText = fs and ET.RewriteCastBase(fs:GetText(), baseMs, flat, pct)
      if newText and newText ~= fs:GetText() then fs:SetText(newText); changed = true end
    end
  end
  if changed then tooltip:Show() end   -- re-fit after changing lines
end

-- Hook every spell-tooltip entry point: SetSpell (spellbook) + SetAction (action bars). These are
-- distinct C entry points that each repopulate the tooltip from Spell.dbc before our post-hook
-- runs, so the rewrite is idempotent even when the button refreshes the tooltip on OnUpdate.
function ET.InitTooltips()
  if ET._tooltipHooked then return end
  ET._tooltipHooked = true
  local function handler(self) ET.ApplyToTooltip(self) end
  if GameTooltip then
    hooksecurefunc(GameTooltip, "SetSpell", handler)
    hooksecurefunc(GameTooltip, "SetAction", handler)
  end
end
