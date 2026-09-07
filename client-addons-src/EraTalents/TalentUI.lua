-- EraTalents panel UI. Reads EraTalentsData[era][class] (structure) + ET.state (dynamic).
--
-- Visuals mirror the real 3.3.5a Blizzard_TalentUI frame (FrameXML: Blizzard_TalentUI.xml /
-- .lua / Templates.xml): 384x512 ornate PaperDoll chrome, a FIXED 4-quadrant per-spec parchment
-- background, the 63px talent grid + UI-TalentBranches/UI-TalentArrows connectors scrolling over
-- it in a UIPanelScrollFrame (right-side scrollbar in the strip beside the background),
-- CharacterFrameTab bottom tabs, and a Common-Input-Border footer bar ("<tab> Talents: N" /
-- "Unspent Talents: N"). The functional contract
-- (ET.InitUI/ShowTab/Refresh/TogglePanel/DrawArrows, data binding, click->Learn,
-- tooltips, no local rank mutation, frame global "EraTalentFrame", _classId=8) is preserved.
EraTalents = EraTalents or {}
local ET = EraTalents

-- ---------------------------------------------------------------------------
-- Layout constants (from the real FrameXML).
-- ---------------------------------------------------------------------------
local FRAME_W, FRAME_H = 384, 512

-- Tree background: FIXED (does not scroll). Native quadrant sizes (real: TL 256x256, TR
-- 64x256, BL 256x128, BR 64x128) sum to exactly 320x384. It's a static parchment layer on the
-- main frame; only the buttons/arrows scroll over it. Right edge is at x = BG_X + BG_W = 343.
local BG_X, BG_Y = 23, -77
local BG_W, BG_H = 320, 384

-- Scroll frame is the REAL 296px-wide talent view (narrower than the 320px background). Its
-- auto-created scrollbar anchors just off the right edge, landing in the ~24px strip on the
-- RIGHT of the background art but still INSIDE the frame border (not out over the chrome). Only
-- buttons + arrows live in the scroll child and scroll over the fixed parchment.
local SCROLL_X, SCROLL_Y = BG_X, BG_Y
local SCROLL_W, SCROLL_H = 296, 332

-- Scroll child holds the grid + arrows. 455 is the 7-tier Vanilla height, used only as the
-- initial value at BuildFrame; ShowTab re-derives the height from the ACTIVE tree's deepest
-- tier (rowY(maxTier) + BTN + 20 -- 455 again for 7 tiers, 581 for TBC's 9), so a 9-tier tree
-- scrolls to its capstone instead of clipping it outside the child (the old CHILD_H bug).
local CHILD_W = SCROLL_W
local CHILD_H = 455

-- Talent grid: real 63px pitch (INITIAL_TALENT_OFFSET_X/Y = 35/20), ItemButton icon ~37.
local BTN          = 37
local COL_X0       = 35                  -- 4 cols of 63px pitch center in 296px (35px margins)
local COL_SPACING  = 63
local ROW_Y0       = 20
local ROW_SPACING  = 63                  -- real row pitch (restored; no longer compressed)
local VANILLA_MAX_TIER = 6   -- 0-indexed last row of the 7-tier Vanilla trees; floors the
                             -- dynamic scroll height so Vanilla stays at 455 (never shrinks).

-- Real atlas TexCoords (Blizzard_TalentUI.lua). Each atlas is two rows: the top half (v 0..0.5) is
-- the bright "learnable" variant, the bottom half (v 0.5..1) the greyed "not-learnable" variant --
-- exactly how Blizzard renders connectors to still-locked talents. We pick per target availability.
local ARROW_TEX  = "Interface\\TalentFrame\\UI-TalentArrows"
local BRANCH_TEX = "Interface\\TalentFrame\\UI-TalentBranches"
local ARROW_DOWN  = { 0, 0.5, 0, 0.5 }        -- "top" arrow: points down into the node
local ARROW_RIGHT = { 1.0, 0.5, 0, 0.5 }
local ARROW_LEFT  = { 0.5, 1.0, 0, 0.5 }
local BRANCH_VERT = { 0, 0.125, 0, 0.484375 } -- "down" segment
local BRANCH_HORZ = { 0.2578125, 0.3828125, 0, 0.5 } -- "right"/"left" segment
-- Greyed variants (same u, bottom-row v) for connectors into a locked talent.
local ARROW_DOWN_OFF  = { 0, 0.5, 0.5, 1.0 }
local ARROW_RIGHT_OFF = { 1.0, 0.5, 0.5, 1.0 }
local ARROW_LEFT_OFF  = { 0.5, 1.0, 0.5, 1.0 }
local BRANCH_VERT_OFF = { 0, 0.125, 0.5, 0.984375 }
local BRANCH_HORZ_OFF = { 0.2578125, 0.3828125, 0.5, 1.0 }

-- The dataset's background tokens (MageArcane/MageFire/MageFrost) ARE the real WotLK talent
-- background file basenames, so this is a direct pass-through. Kept as a hook so a future
-- dataset with non-matching tokens can be remapped here without editing the dataset.
local function BackgroundBase(token)
  return "Interface\\TalentFrame\\" .. (token or "MageFire")
end

local function colX(col)  return COL_X0 + col * COL_SPACING end
local function rowY(tier) return ROW_Y0 + tier * ROW_SPACING end

local function TreeForState()
  local byClass = EraTalentsData and EraTalentsData[ET.state.era]
  local tree = byClass and byClass[ET._classId]
  return tree
end

-- A talent is REACHABLE (full colour, clickable) exactly like the real tree: enough points spent
-- in this tree to unlock its tier (node.prereqPoints == 5 * tier), AND its direct prerequisite
-- talent, if any, is maxed. Otherwise it's locked -> desaturated. Pure -> headless-testable
-- (client-addon/tests/test_talentui.lua); no WoW API, all inputs passed in.
function ET.IsReachable(node, spentInTab, ranks, byId)
  if not node then return false end
  if (spentInTab or 0) < (node.prereqPoints or 0) then return false end
  local pid = node.prereqTalent
  if pid and pid ~= 0 then
    local pre = byId and byId[pid]
    if pre and ((ranks and ranks[pid] or 0) < pre.maxRank) then return false end
  end
  return true
end

-- Bind IsReachable to the live state. ET._spentActive / ET._byId are recomputed per ShowTab (both
-- depend only on the active tab, which is all that's ever drawn).
local function NodeAvailable(node)
  return ET.IsReachable(node, ET._spentActive, ET.state.ranks, ET._byId)
end

-- Paint the active tab's tree background from its 4 real quadrant textures, sized to fill the
-- scroll child (CHILD_W x CHILD_H) proportionally -- no gaps or distortion of the art aspect.
local function SetTreeBackground(bgToken)
  local base = BackgroundBase(bgToken)
  ET.frame.bgTL:SetTexture(base .. "-TopLeft")
  ET.frame.bgTR:SetTexture(base .. "-TopRight")
  ET.frame.bgBL:SetTexture(base .. "-BottomLeft")
  ET.frame.bgBR:SetTexture(base .. "-BottomRight")
end

-- ---------------------------------------------------------------------------
-- Frame construction (once, lazily).
-- ---------------------------------------------------------------------------
local function BuildFrame()
  if ET.frame then return ET.frame end
  local f = CreateFrame("Frame", "EraTalentFrame", UIParent)
  f:SetFrameStrata("HIGH")
  f:SetWidth(FRAME_W)
  f:SetHeight(FRAME_H)
  f:SetPoint("CENTER")
  f:EnableMouse(true)
  f:SetMovable(true)
  f:RegisterForDrag("LeftButton")
  f:SetScript("OnDragStart", f.StartMoving)
  f:SetScript("OnDragStop", f.StopMovingOrSizing)
  f:Hide()

  -- Ornate frame chrome (real PaperDoll talent-frame border pieces).
  local chrome = {
    { "Interface\\PaperDollInfoFrame\\UI-Character-General-TopLeft",  256, 256, "TOPLEFT",     2, -1 },
    { "Interface\\PaperDollInfoFrame\\UI-Character-General-TopRight", 128, 256, "TOPRIGHT",    2, -1 },
    { "Interface\\TalentFrame\\UI-TalentFrame-BotLeft",              256, 256, "BOTTOMLEFT",  2, -1 },
    { "Interface\\TalentFrame\\UI-TalentFrame-BotRight",             128, 256, "BOTTOMRIGHT", 2, -1 },
  }
  for _, c in ipairs(chrome) do
    local t = f:CreateTexture(nil, "BORDER")
    t:SetTexture(c[1])
    t:SetWidth(c[2]); t:SetHeight(c[3])
    t:SetPoint(c[4], c[5], c[6])
  end

  -- Player portrait (top-left, like the real frame). The client renders unit portraits lazily and
  -- announces each one with UNIT_PORTRAIT_UPDATE; BuildFrame runs at the first PLAYER_ENTERING_WORLD,
  -- when the player's own portrait is usually NOT rendered yet, so a one-shot SetPortraitTexture here
  -- captured a blank -- whether the circle filled in was a login-timing race (the hunter happened to
  -- win it; every other class lost). Mirror the stock CharacterFrame: re-apply on every
  -- UNIT_PORTRAIT_UPDATE for the player and on each Show, so the texture always tracks the live render.
  f.portrait = f:CreateTexture(nil, "BACKGROUND")
  f.portrait:SetWidth(60); f.portrait:SetHeight(60)
  f.portrait:SetPoint("TOPLEFT", 7, -6)
  local function RefreshPortrait() SetPortraitTexture(f.portrait, "player") end
  RefreshPortrait()
  f:RegisterEvent("UNIT_PORTRAIT_UPDATE")
  f:SetScript("OnEvent", function(self, event, unit)
    if event == "UNIT_PORTRAIT_UPDATE" and (unit == "player" or unit == nil) then RefreshPortrait() end
  end)
  f:SetScript("OnShow", RefreshPortrait)

  f.title = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
  f.title:SetPoint("TOP", 0, -18)
  f.title:SetText("Talents")

  -- Close button: real anchor (CENTER relative to TOPRIGHT, offset -44,-25).
  local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
  close:SetPoint("CENTER", f, "TOPRIGHT", -44, -25)

  -- Pet Talents (hunters only): the era window replaces the PLAYER talent frame, but the WotLK
  -- PET talent tree lives as a side tab INSIDE that stock frame, so an era hunter otherwise has
  -- no route to it. This reopens the stock frame on demand (ET.OpenStockTalents bypasses the era
  -- guard once); click the Pet side-tab there. Visibility is re-asserted in ET.Refresh.
  f.petTalents = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
  f.petTalents:SetWidth(82); f.petTalents:SetHeight(20)
  f.petTalents:SetPoint("TOPRIGHT", f, "TOPRIGHT", -34, -44)   -- top-right slot (was the dev Reset button's)
  f.petTalents:SetText("Pet Talents")
  f.petTalents:SetScript("OnClick", function() if ET.OpenStockTalents then ET.OpenStockTalents() end end)
  if ET._classId ~= 3 then f.petTalents:Hide() end

  -- FIXED tree background: 4 real quadrants (native sizes -> 320x384) on the main frame. Drawn on
  -- ARTWORK (ABOVE the BORDER-layer chrome, whose dark stone center would otherwise bury a
  -- BACKGROUND-layer parchment) but BELOW the later ARTWORK footer and below the scroll child's
  -- buttons (child frames always render above the parent's textures). This layer never scrolls;
  -- the buttons/arrows in the scroll child slide over it. Painted per-tab by SetTreeBackground.
  f.bgTL = f:CreateTexture(nil, "ARTWORK")
  f.bgTL:SetWidth(256); f.bgTL:SetHeight(256)
  f.bgTL:SetPoint("TOPLEFT", BG_X, BG_Y)
  f.bgTR = f:CreateTexture(nil, "ARTWORK")
  f.bgTR:SetWidth(64); f.bgTR:SetHeight(256)
  f.bgTR:SetPoint("TOPLEFT", f.bgTL, "TOPRIGHT")
  f.bgBL = f:CreateTexture(nil, "ARTWORK")
  f.bgBL:SetWidth(256); f.bgBL:SetHeight(128)
  f.bgBL:SetPoint("TOPLEFT", f.bgTL, "BOTTOMLEFT")
  f.bgBR = f:CreateTexture(nil, "ARTWORK")
  f.bgBR:SetWidth(64); f.bgBR:SetHeight(128)
  f.bgBR:SetPoint("TOPLEFT", f.bgTL, "BOTTOMRIGHT")

  -- Scroll frame (real UIPanelScrollFrameTemplate). Its width == the background width, so the
  -- auto-created scrollbar sits just right of the background art (the blank strip), not the edge.
  f.scroll = CreateFrame("ScrollFrame", "EraTalentFrameScroll", f, "UIPanelScrollFrameTemplate")
  f.scroll:SetWidth(SCROLL_W); f.scroll:SetHeight(SCROLL_H)
  f.scroll:SetPoint("TOPLEFT", SCROLL_X, SCROLL_Y)

  -- Mouse-wheel scrolling over the tree.
  f.scroll:EnableMouseWheel(true)
  f.scroll:SetScript("OnMouseWheel", function(self, delta)
    self = self or this
    local d = delta or arg1 or 0
    local sb = getglobal(self:GetName() .. "ScrollBar")
    if sb then sb:SetValue(sb:GetValue() - d * ROW_SPACING) end
  end)

  -- Scroll child: grid + arrows only (the background is fixed, above). CHILD_H here is just the
  -- bootstrap value; ShowTab re-derives the real height per-tree from the active tree's depth.
  f.container = CreateFrame("Frame", "EraTalentFrameScrollChild", f.scroll)
  f.container:SetWidth(CHILD_W); f.container:SetHeight(CHILD_H)
  f.scroll:SetScrollChild(f.container)

  -- Arrow overlay frame -- higher level than the talent buttons so arrowheads draw on top.
  f.arrowFrame = CreateFrame("Frame", nil, f.container)
  f.arrowFrame:SetAllPoints()
  f.arrowFrame:SetFrameLevel(f.container:GetFrameLevel() + 10)

  -- Footer bar (real Common-Input-Border 3-slice) with spent/unspent text.
  local fbLeft = f:CreateTexture(nil, "ARTWORK")
  fbLeft:SetTexture("Interface\\Common\\Common-Input-Border")
  fbLeft:SetWidth(8); fbLeft:SetHeight(20)
  fbLeft:SetPoint("BOTTOMLEFT", 26, 80)
  fbLeft:SetTexCoord(0, 0.0625, 0, 0.625)
  local fbMid = f:CreateTexture(nil, "ARTWORK")
  fbMid:SetTexture("Interface\\Common\\Common-Input-Border")
  fbMid:SetWidth(300); fbMid:SetHeight(20)
  fbMid:SetPoint("LEFT", fbLeft, "RIGHT")
  fbMid:SetTexCoord(0.0625, 0.9375, 0, 0.625)
  local fbRight = f:CreateTexture(nil, "ARTWORK")
  fbRight:SetTexture("Interface\\Common\\Common-Input-Border")
  fbRight:SetWidth(8); fbRight:SetHeight(20)
  fbRight:SetPoint("LEFT", fbMid, "RIGHT")
  fbRight:SetTexCoord(0.9375, 1.0, 0, 0.625)

  f.footerSpent = f:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
  f.footerSpent:SetPoint("LEFT", fbLeft, "LEFT", 8, 0)
  f.footerUnspent = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
  f.footerUnspent:SetPoint("RIGHT", fbRight, "RIGHT", -8, 0)

  -- Three bottom tabs (real CharacterFrame tab buttons).
  f.tabs = {}
  for i = 1, 3 do
    local tab = CreateFrame("Button", "EraTalentFrameTab" .. i, f, "CharacterFrameTabButtonTemplate")
    if i == 1 then tab:SetPoint("BOTTOMLEFT", 15, 46)
    else tab:SetPoint("LEFT", f.tabs[i - 1], "RIGHT", -15, 0) end
    tab:SetScript("OnClick", function() ET.ShowTab(i) end)
    f.tabs[i] = tab
  end
  PanelTemplates_SetNumTabs(f, 3)

  f.buttons = {}          -- [nodeId] = button
  f._branches = {}        -- pooled branch line textures
  f._arrows = {}          -- pooled arrowhead textures

  ET.frame = f
  return f
end

-- ---------------------------------------------------------------------------
-- Talent buttons.
-- ---------------------------------------------------------------------------
local function NodeTooltip(btn)
  local node = btn.node
  local rank = ET.state.ranks[node.id] or 0
  GameTooltip:SetOwner(btn, "ANCHOR_RIGHT")

  -- A spell-granting talent shows the granted spell's REAL client tooltip via a spell hyperlink:
  -- the client renders the proper header (power cost / range / cast time / cooldown as structured
  -- lines) plus the effect description from the spell's own Spell.dbc row -- instead of the flat
  -- authored prose, which crammed those attributes into one wrapped line. The granted spell id is
  -- emitted per-rank by the generator (node.grant); a clone ships in patch-V.mpq, a stock grant in
  -- the base DBC, so SetHyperlink resolves either even before the talent is learned. We append the
  -- talent's own "Rank X/Y" line below the spell block.
  -- ...BUT only for a BAND clone (id >= 920000): those rows ship in patch-V.mpq with the era's own
  -- text. A STOCK grant's client row is the 3.3.5a one, so its hyperlink renders WotLK prose (TBC
  -- Improved Aspect of the Hawk read "Aspect of the Dragonhawk", a WotLK spell — real-client
  -- finding 2026-09-05). Stock grants therefore fall through to the authored per-rank prose below,
  -- which the importer took from the era's own tooltip source. Passive talents lose nothing; an
  -- ACTIVE stock grant loses the structured cost/range header in the talent panel only (its
  -- spellbook tooltip is the stock client row either way — framework lesson 17d).
  local gid = node.grant and (node.grant[(rank > 0) and rank or 1] or node.grant[1])
  if gid and tonumber(gid) and tonumber(gid) >= 920000 then
    GameTooltip:SetHyperlink("spell:" .. gid)
    GameTooltip:AddLine("Rank " .. rank .. "/" .. node.maxRank, 0.8, 0.8, 0.8)
    GameTooltip:Show()
    return
  end

  -- Passive/modifier talents (spellmods, stats, procs) keep the authored per-rank effect prose.
  GameTooltip:SetText(node.name, 1, 1, 1)
  GameTooltip:AddLine("Rank " .. rank .. "/" .. node.maxRank, 0.8, 0.8, 0.8)
  local shown = (rank > 0) and rank or 1
  GameTooltip:AddLine(node.tooltip[shown] or "", 1, 0.82, 0, true)
  if rank < node.maxRank then
    GameTooltip:AddLine(" ")
    GameTooltip:AddLine("Next rank:", 0.6, 0.6, 0.6)
    GameTooltip:AddLine(node.tooltip[rank + 1] or "", 0.3, 1, 0.3, true)
  end
  GameTooltip:Show()
end

-- Every displayed node is clickable; the server is the sole authority on whether a talent is
-- actually wired. Clicking an unwired node sends LEARN, the server's TryLearn rejects it (no
-- era_talent row), no SYNC follows, and the UI simply doesn't change -- a harmless no-op. As
-- nodes get wired server-side they start responding without any addon change.
local function OnNodeClick(btn)
  local node = btn.node
  local rank = ET.state.ranks[node.id] or 0
  if rank >= node.maxRank then return end
  if (ET.state.availPts or 0) < 1 then
    UIErrorsFrame:AddMessage("Not enough talent points.", 1, 0.2, 0.2, 1, 3)
    return
  end
  ET.Learn(node.id)     -- server validates; UI updates when the SYNC arrives
end

local function EnsureButtons(tree)
  local f = ET.frame
  for _, node in ipairs(tree.talents) do
    if not f.buttons[node.id] then
      local b = CreateFrame("Button", nil, f.container)
      b:SetWidth(BTN); b:SetHeight(BTN)
      b:SetPoint("TOPLEFT", colX(node.col), -rowY(node.tier))

      -- Metallic slot ring behind the icon (real talent slot art).
      b.slot = b:CreateTexture(nil, "BACKGROUND")
      b.slot:SetTexture("Interface\\Buttons\\UI-EmptySlot-White")
      b.slot:SetWidth(BTN + 25); b.slot:SetHeight(BTN + 25)
      b.slot:SetPoint("CENTER")

      -- node.icon is a bare icon name (e.g. "Spell_Fire_FireBolt02"); prepend the standard icon
      -- path. A name that already contains a backslash is treated as a full path and used as-is.
      -- SetNormalTexture with an icon path the client lacks (or an empty name) can leave the button
      -- with NO normal texture (GetNormalTexture()==nil) -> guard, and fall back to a
      -- guaranteed-present icon so a single bad/missing icon degrades to a placeholder instead of a
      -- blank slot (or crashing the whole panel).
      local iconTex = node.icon or ""
      if iconTex ~= "" and not string.find(iconTex, "\\", 1, true) then
        iconTex = "Interface\\Icons\\" .. iconTex
      end
      b:SetNormalTexture(iconTex)
      if iconTex == "" or not b:GetNormalTexture() then
        b:SetNormalTexture("Interface\\Icons\\INV_Misc_QuestionMark")
      end
      local nt = b:GetNormalTexture()
      if nt then nt:SetTexCoord(0.07, 0.93, 0.07, 0.93) end

      -- Rank plate (real TalentFrame-RankBorder) at bottom-right with the current rank.
      b.rankBorder = b:CreateTexture(nil, "OVERLAY")
      b.rankBorder:SetTexture("Interface\\TalentFrame\\TalentFrame-RankBorder")
      b.rankBorder:SetWidth(28); b.rankBorder:SetHeight(28)
      b.rankBorder:SetPoint("CENTER", b, "BOTTOMRIGHT", -2, 2)
      b.rank = b:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
      b.rank:SetPoint("CENTER", b.rankBorder, "CENTER", 0, 0)

      b.node = node
      b:SetScript("OnEnter", NodeTooltip)
      b:SetScript("OnLeave", function() GameTooltip:Hide() end)
      b:SetScript("OnClick", OnNodeClick)
      f.buttons[node.id] = b
    end
  end
end

local function RenderButtons(tree)
  local f = ET.frame
  for _, node in ipairs(tree.talents) do
    local b = f.buttons[node.id]
    local rank = ET.state.ranks[node.id] or 0
    if node.tab == ET._activeTab then
      b:Show()
      b.rank:SetText(rank .. "/" .. node.maxRank)
      local icon = b:GetNormalTexture()
      -- Mirror the real tree's state colouring (no glow box): trained/maxed = full colour + green
      -- rank, reachable-but-empty = full colour + white rank, locked tier/prereq = greyed + dim rank.
      if rank > 0 then
        if icon then icon:SetDesaturated(nil) end
        b.rank:SetTextColor(0.1, 1, 0.1)          -- has points (incl. maxed): green
      elseif NodeAvailable(node) then
        if icon then icon:SetDesaturated(nil) end
        b.rank:SetTextColor(1, 1, 1)              -- reachable now: full colour, white rank
      else
        if icon then icon:SetDesaturated(1) end
        b.rank:SetTextColor(0.5, 0.5, 0.5)        -- locked: greyed out
      end
    else
      b:Hide()
    end
  end
end

-- ---------------------------------------------------------------------------
-- Prereq connectors (branches + arrowheads), using the real atlases/TexCoords.
-- ---------------------------------------------------------------------------
function ET.DrawArrows(tree)
  local f = ET.frame
  for _, t in ipairs(f._branches) do t:Hide() end
  for _, t in ipairs(f._arrows) do t:Hide() end

  local nb, na = 0, 0
  local function branch(x, y, w, h, tc)
    nb = nb + 1
    local t = f._branches[nb]
    if not t then
      t = f.container:CreateTexture(nil, "ARTWORK")   -- behind the buttons
      t:SetTexture(BRANCH_TEX)
      f._branches[nb] = t
    end
    t:SetTexCoord(tc[1], tc[2], tc[3], tc[4])
    t:ClearAllPoints()
    t:SetPoint("TOPLEFT", x, -y)
    t:SetWidth(w); t:SetHeight(h)
    t:Show()
  end
  local function arrow(cx, cy, tc)
    na = na + 1
    local t = f._arrows[na]
    if not t then
      t = f.arrowFrame:CreateTexture(nil, "OVERLAY")  -- above the buttons
      t:SetTexture(ARROW_TEX)
      t:SetWidth(32); t:SetHeight(32)
      f._arrows[na] = t
    end
    t:SetTexCoord(tc[1], tc[2], tc[3], tc[4])
    t:ClearAllPoints()
    t:SetPoint("CENTER", f.arrowFrame, "TOPLEFT", cx, -cy)
    t:Show()
  end

  local byId = {}
  for _, x in ipairs(tree.talents) do byId[x.id] = x end

  for _, node in ipairs(tree.talents) do
    if node.tab == ET._activeTab and node.prereqTalent and node.prereqTalent ~= 0 then
      local from = byId[node.prereqTalent]
      if from and from.tab == node.tab then
        local fcx, fcy = colX(from.col) + BTN / 2, rowY(from.tier) + BTN / 2
        local ncx, ncy = colX(node.col) + BTN / 2, rowY(node.tier) + BTN / 2
        local dCol = node.col - from.col
        local dTier = node.tier - from.tier

        -- Bright connector only once the target is reachable; greyed while it's still locked.
        local on = NodeAvailable(node)
        local vert  = on and BRANCH_VERT  or BRANCH_VERT_OFF
        local horz  = on and BRANCH_HORZ  or BRANCH_HORZ_OFF
        local aDown = on and ARROW_DOWN   or ARROW_DOWN_OFF
        local aRt   = on and ARROW_RIGHT  or ARROW_RIGHT_OFF
        local aLt   = on and ARROW_LEFT   or ARROW_LEFT_OFF

        -- Vertical run down the prerequisite's column to the node's row.
        if dTier ~= 0 then
          local y0 = math.min(fcy, ncy)
          branch(fcx - 16, y0, 32, math.abs(ncy - fcy), vert)
        end
        -- Horizontal run across the node's row to the node's column.
        if dCol ~= 0 then
          local x0 = math.min(fcx, ncx)
          branch(x0, ncy - 16, math.abs(ncx - fcx), 32, horz)
        end

        -- Arrowhead at the node, pointing along the final approach direction.
        if dCol > 0 then
          arrow(ncx - BTN / 2 - 3, ncy, aRt)
        elseif dCol < 0 then
          arrow(ncx + BTN / 2 + 3, ncy, aLt)
        else
          arrow(ncx, ncy - BTN / 2 - 3, aDown)
        end
      end
    end
  end
end

-- ---------------------------------------------------------------------------
-- Footer text (spent-in-active-tab / unspent), tracks SYNC.
-- ---------------------------------------------------------------------------
local function UpdateFooter(tree)
  local f = ET.frame
  local spent = 0
  for _, node in ipairs(tree.talents) do
    if node.tab == ET._activeTab then
      spent = spent + (ET.state.ranks[node.id] or 0)
    end
  end
  local tabName = (tree.tabs[ET._activeTab] and tree.tabs[ET._activeTab].name) or "Talents"
  f.footerSpent:SetText(tabName .. " Talents: " .. spent)
  f.footerUnspent:SetText("Unspent Talents: " .. (ET.state.availPts or 0))
end

-- ---------------------------------------------------------------------------
-- Public API (functional contract).
-- ---------------------------------------------------------------------------
function ET.ShowTab(i)
  local tree = TreeForState()
  if not tree then return end
  ET._activeTab = i
  for t = 1, 3 do
    local tab = ET.frame.tabs[t]
    if tab and tree.tabs[t] then
      tab:SetText(tree.tabs[t].name)
      PanelTemplates_TabResize(tab, 0)
    end
  end
  PanelTemplates_SetTab(ET.frame, i)
  if tree.tabs[i] and tree.tabs[i].background then SetTreeBackground(tree.tabs[i].background) end

  -- Availability inputs for this tab: id lookup + points spent in the active tree (for NodeAvailable).
  ET._byId = {}
  ET._spentActive = 0
  for _, n in ipairs(tree.talents) do
    ET._byId[n.id] = n
    if n.tab == i then ET._spentActive = ET._spentActive + (ET.state.ranks[n.id] or 0) end
  end

  -- Height from the WHOLE tree's deepest tier (not per-tab, so switching tabs never jumps
  -- the scroll range): rowY(maxTier) + button + bottom margin.
  local maxTier = VANILLA_MAX_TIER
  for _, n in ipairs(tree.talents) do
    if n.tier > maxTier then maxTier = n.tier end
  end
  ET.frame.container:SetHeight(rowY(maxTier) + BTN + 20)

  EnsureButtons(tree)
  RenderButtons(tree)
  ET.DrawArrows(tree)
  UpdateFooter(tree)
end

-- Called by Comms on every SYNC.
function ET.Refresh()
  if not ET.frame then return end
  if ET.frame.petTalents then
    if ET._classId == 3 then ET.frame.petTalents:Show() else ET.frame.petTalents:Hide() end
  end
  local tree = TreeForState()
  if not tree then return end
  ET.ShowTab(ET._activeTab or 2)   -- default to Fire (where Improved Fireball lives); repaints + footer
end

function ET.TogglePanel()
  BuildFrame()
  if ET.frame:IsShown() then ET.frame:Hide() else ET.frame:Show(); ET.Refresh() end
end

-- WoW 3.3.5a's UnitClass returns only (localizedName, classToken) -- the numeric classID as a THIRD
-- return was not added until Cataclysm (4.0.1). So `select(3, UnitClass(...))` is ALWAYS nil here and
-- silently fell back to Mage(8), rendering the Mage tree for every non-mage class. Map the
-- locale-independent token (which IS present on 3.3.5a) to the numeric id the data is keyed by.
local CLASS_ID_BY_TOKEN = {
  WARRIOR = 1, PALADIN = 2, HUNTER = 3, ROGUE = 4, PRIEST = 5, DEATHKNIGHT = 6,
  SHAMAN = 7, MAGE = 8, WARLOCK = 9, DRUID = 11,
}

function ET.InitUI()
  -- Class-agnostic: the addon renders EraTalentsData[era][classId]. Derive the numeric class id from the
  -- player's class TOKEN (2nd UnitClass return, present on 3.3.5a); fall back to Mage if unavailable.
  local _, token = UnitClass("player")
  ET._classId = CLASS_ID_BY_TOKEN[token or ""] or 8
  ET._activeTab = 2
  BuildFrame()
end
