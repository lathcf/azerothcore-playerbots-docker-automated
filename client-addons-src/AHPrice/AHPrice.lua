local NS = _G.AHPriceNS or {}
_G.AHPriceNS = NS
local C = NS.Comm
local M = NS.Model

-- ---- theme: gilded auction ledger ----------------------------------------
local COL = {
  bg      = { 0.075, 0.062, 0.050 },
  inset   = { 0.035, 0.030, 0.024 },
  gold    = { 0.87, 0.72, 0.39 },
  goldDim = { 0.52, 0.42, 0.24 },
  cream   = { 0.91, 0.87, 0.78 },
  muted   = { 0.60, 0.56, 0.47 },
  green   = { 0.49, 0.84, 0.46 },
}
local F_DISPLAY = "Fonts\\MORPHEUS.TTF"   -- WotLK quest/parchment face
local F_BODY    = "Fonts\\FRIZQT__.TTF"
local F_NUM     = "Fonts\\ARIALN.TTF"     -- condensed numerals for prices
local WHITE     = "Interface\\Buttons\\WHITE8X8"

local ROWS, ROW_H = 6, 18
local W = 300
local CARD_H = 142

local function fill(parent, layer, c, a)
  local t = parent:CreateTexture(nil, layer or "ARTWORK")
  t:SetTexture(WHITE)
  t:SetVertexColor(c[1], c[2], c[3], a or 1)
  return t
end
local function qcolor(q)
  local c = ITEM_QUALITY_COLORS and ITEM_QUALITY_COLORS[q]
  if c then return c.r, c.g, c.b end
  return COL.cream[1], COL.cream[2], COL.cream[3]
end

-- ---- window ---------------------------------------------------------------
local frame = CreateFrame("Frame", "AHPriceFrame", UIParent)
frame:SetWidth(W); frame:SetHeight(370)
frame:SetPoint("CENTER")
frame:SetBackdrop({
  bgFile = WHITE,
  edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 14,
  insets = { left = 4, right = 4, top = 4, bottom = 4 },
})
frame:SetBackdropColor(COL.bg[1], COL.bg[2], COL.bg[3], 0.97)
frame:SetBackdropBorderColor(COL.goldDim[1], COL.goldDim[2], COL.goldDim[3], 0.95)
frame:SetMovable(true); frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:SetClampedToScreen(true)
frame:Hide()

-- soft gold glow bleeding down from the header for depth
local glow = frame:CreateTexture(nil, "BORDER")
glow:SetTexture(WHITE)
glow:SetPoint("TOPLEFT", 5, -5); glow:SetPoint("TOPRIGHT", -5, -5); glow:SetHeight(64)
glow:SetGradientAlpha("VERTICAL", COL.gold[1], COL.gold[2], COL.gold[3], 0.0,
                                  COL.gold[1], COL.gold[2], COL.gold[3], 0.10)

local title = frame:CreateFontString(nil, "OVERLAY")
title:SetFont(F_DISPLAY, 19)
title:SetPoint("TOP", 0, -12)
title:SetText("AHPrice")
title:SetTextColor(COL.gold[1], COL.gold[2], COL.gold[3])
title:SetShadowColor(0, 0, 0, 0.9); title:SetShadowOffset(1, -1)

local subtitle = frame:CreateFontString(nil, "OVERLAY")
subtitle:SetFont(F_NUM, 10)
subtitle:SetPoint("TOP", title, "BOTTOM", 0, 0)
subtitle:SetText("AUCTION  BOT  BUY  RANGE")
subtitle:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetWidth(26); close:SetHeight(26)
close:SetPoint("TOPRIGHT", -4, -4)

-- gold hairline under the header, faded at both ends
local divider = fill(frame, "ARTWORK", COL.gold, 1)
divider:SetHeight(1)
divider:SetPoint("TOPLEFT", 14, -46); divider:SetPoint("TOPRIGHT", -14, -46)
divider:SetGradientAlpha("HORIZONTAL", COL.gold[1], COL.gold[2], COL.gold[3], 0.05,
                                       COL.gold[1], COL.gold[2], COL.gold[3], 0.55)

-- ---- search box (custom-skinned) ------------------------------------------
local eb = CreateFrame("EditBox", "AHPriceSearch", frame)
eb:SetPoint("TOPLEFT", 14, -54); eb:SetPoint("TOPRIGHT", -14, -54)
eb:SetHeight(22)
eb:SetAutoFocus(false)
eb:SetFont(F_BODY, 13)
eb:SetTextColor(COL.cream[1], COL.cream[2], COL.cream[3])
eb:SetTextInsets(8, 8, 0, 0)
eb:SetBackdrop({
  bgFile = WHITE, edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10,
  insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
eb:SetBackdropColor(COL.inset[1], COL.inset[2], COL.inset[3], 0.95)
eb:SetBackdropBorderColor(COL.goldDim[1], COL.goldDim[2], COL.goldDim[3], 0.6)
eb:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)

local placeholder = eb:CreateFontString(nil, "OVERLAY")
placeholder:SetFont(F_BODY, 13)
placeholder:SetPoint("LEFT", 8, 0)
placeholder:SetText("Search an item, or shift-click one...")
placeholder:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3], 0.8)
local function refreshPlaceholder()
  if eb:HasFocus() or (eb:GetText() or "") ~= "" then placeholder:Hide() else placeholder:Show() end
end
eb:SetScript("OnEditFocusGained", refreshPlaceholder)
eb:SetScript("OnEditFocusLost", refreshPlaceholder)
eb:SetScript("OnTextChanged", refreshPlaceholder)

local status = frame:CreateFontString(nil, "OVERLAY")
status:SetFont(F_NUM, 11)
status:SetPoint("TOPLEFT", 15, -80)
status:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])

-- ---- results list (mouse-wheel virtualized) -------------------------------
local listTop = -94
local list = CreateFrame("Frame", nil, frame)
list:SetPoint("TOPLEFT", 12, listTop)
list:SetPoint("TOPRIGHT", -12, listTop)
list:SetHeight(ROWS * ROW_H + 6)
list:SetBackdrop({
  bgFile = WHITE, edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10,
  insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
list:SetBackdropColor(COL.inset[1], COL.inset[2], COL.inset[3], 0.9)
list:SetBackdropBorderColor(COL.goldDim[1], COL.goldDim[2], COL.goldDim[3], 0.4)
list:EnableMouseWheel(true)

local state = { results = {}, offset = 0, selected = nil }
local rows = {}
for i = 1, ROWS do
  local b = CreateFrame("Button", nil, list)
  b:SetHeight(ROW_H)
  b:SetPoint("TOPLEFT", 5, -4 - (i - 1) * ROW_H)
  b:SetPoint("TOPRIGHT", -5, -4 - (i - 1) * ROW_H)

  b.sel = fill(b, "BACKGROUND", COL.gold, 0.16); b.sel:SetAllPoints(); b.sel:Hide()
  b:SetHighlightTexture(WHITE)
  b:GetHighlightTexture():SetVertexColor(COL.gold[1], COL.gold[2], COL.gold[3], 0.10)

  b.dot = fill(b, "ARTWORK", COL.cream, 1)
  b.dot:SetWidth(4); b.dot:SetHeight(ROW_H - 8); b.dot:SetPoint("LEFT", 2, 0)

  b.text = b:CreateFontString(nil, "OVERLAY")
  b.text:SetFont(F_BODY, 12)
  b.text:SetPoint("LEFT", 12, 0); b.text:SetPoint("RIGHT", -4, 0)
  b.text:SetJustifyH("LEFT")
  b:Hide()
  rows[i] = b
end

local listHint = list:CreateFontString(nil, "OVERLAY")
listHint:SetFont(F_BODY, 11)
listHint:SetPoint("CENTER", list, "CENTER", 0, 0)
listHint:SetWidth(240); listHint:SetJustifyH("CENTER")
listHint:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])
listHint:SetText("Type a name and press Enter,\nor shift-click an item.")

local showDetail, clearDetail, doItem, doSearch
local function renderRows()
  local total = #state.results
  local maxOff = math.max(0, total - ROWS)
  if state.offset > maxOff then state.offset = maxOff end
  if state.offset < 0 then state.offset = 0 end
  for i = 1, ROWS do
    local b, e = rows[i], state.results[state.offset + i]
    if e then
      b.text:SetText(e.name)
      b.text:SetTextColor(qcolor(e.quality))
      b.dot:SetVertexColor(qcolor(e.quality))
      b.itemID = e.itemID
      if state.selected == e.itemID then b.sel:Show() else b.sel:Hide() end
      b:Show()
    else
      b:Hide(); b.itemID = nil
    end
  end
  if total > ROWS then
    status:SetText(string.format("%d matches  ·  scroll (%d-%d)", total,
      state.offset + 1, math.min(total, state.offset + ROWS)))
  elseif total > 0 then
    status:SetText(total .. (total == 1 and " match" or " matches"))
  end
  if total == 0 then listHint:Show() else listHint:Hide() end
end
local function scrollBy(delta)
  state.offset = state.offset - delta
  renderRows()
end
list:SetScript("OnMouseWheel", function(_, delta) scrollBy(delta) end)
for i = 1, ROWS do
  -- rows sit on top of the list; wheel events don't bubble, so forward them.
  rows[i]:EnableMouseWheel(true)
  rows[i]:SetScript("OnMouseWheel", function(_, delta) scrollBy(delta) end)
  rows[i]:SetScript("OnClick", function(self)
    if self.itemID then state.selected = self.itemID; renderRows(); doItem(self.itemID) end
  end)
end

-- ---- detail card ----------------------------------------------------------
local card = CreateFrame("Frame", nil, frame)
card:SetPoint("TOPLEFT", 12, listTop - (ROWS * ROW_H + 6) - 10)
card:SetPoint("TOPRIGHT", -12, listTop - (ROWS * ROW_H + 6) - 10)
card:SetHeight(CARD_H)
card:SetBackdrop({
  bgFile = WHITE, edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10,
  insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
card:SetBackdropColor(COL.inset[1], COL.inset[2], COL.inset[3], 0.92)
card:SetBackdropBorderColor(COL.goldDim[1], COL.goldDim[2], COL.goldDim[3], 0.5)

local cardTop = fill(card, "ARTWORK", COL.gold, 0.7)
cardTop:SetHeight(1)
cardTop:SetPoint("TOPLEFT", 6, -4); cardTop:SetPoint("TOPRIGHT", -6, -4)

local dv = {}  -- value widgets shown only when an item is selected

dv.iconBorder = fill(card, "BACKGROUND", COL.gold, 0.9)
dv.iconBorder:SetWidth(40); dv.iconBorder:SetHeight(40)
dv.iconBorder:SetPoint("TOPLEFT", 12, -12)
dv.icon = card:CreateTexture(nil, "ARTWORK")
dv.icon:SetWidth(36); dv.icon:SetHeight(36)
dv.icon:SetPoint("CENTER", dv.iconBorder, "CENTER")
dv.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

dv.name = card:CreateFontString(nil, "OVERLAY")
dv.name:SetFont(F_DISPLAY, 15)
dv.name:SetPoint("TOPLEFT", 58, -14); dv.name:SetPoint("RIGHT", -12, 0)
dv.name:SetJustifyH("LEFT"); dv.name:SetHeight(20)

dv.vendor = card:CreateFontString(nil, "OVERLAY")
dv.vendor:SetFont(F_NUM, 11)
dv.vendor:SetPoint("TOPLEFT", 58, -36)
dv.vendor:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])

dv.caption = card:CreateFontString(nil, "OVERLAY")
dv.caption:SetFont(F_NUM, 10)
dv.caption:SetPoint("TOPLEFT", 12, -58)
dv.caption:SetText("BOT  WILL  PAY   ·   PER  UNIT")
dv.caption:SetTextColor(COL.gold[1], COL.gold[2], COL.gold[3])

-- range gauge: dark track, green "guaranteed" zone, gold "possible" zone
local trackInset = 12
dv.track = fill(card, "ARTWORK", COL.bg, 1)
dv.track:SetHeight(10)
dv.track:SetPoint("TOPLEFT", trackInset, -74); dv.track:SetPoint("TOPRIGHT", -trackInset, -74)
dv.green = fill(card, "OVERLAY", COL.green, 0.85)
dv.green:SetPoint("TOPLEFT", dv.track, "TOPLEFT", 1, -1); dv.green:SetHeight(8)
dv.gold = fill(card, "OVERLAY", COL.gold, 0.7)
dv.gold:SetPoint("TOPLEFT", dv.green, "TOPRIGHT", 0, 0); dv.gold:SetHeight(8)

dv.minVal = card:CreateFontString(nil, "OVERLAY")
dv.minVal:SetFont(F_NUM, 14); dv.minVal:SetPoint("TOPLEFT", trackInset, -90)
dv.minVal:SetTextColor(COL.green[1], COL.green[2], COL.green[3])
dv.maxVal = card:CreateFontString(nil, "OVERLAY")
dv.maxVal:SetFont(F_NUM, 14); dv.maxVal:SetPoint("TOPRIGHT", -trackInset, -90)
dv.maxVal:SetJustifyH("RIGHT"); dv.maxVal:SetTextColor(COL.gold[1], COL.gold[2], COL.gold[3])

dv.minCap = card:CreateFontString(nil, "OVERLAY")
dv.minCap:SetFont(F_BODY, 9); dv.minCap:SetPoint("TOPLEFT", trackInset, -108)
dv.minCap:SetText("guaranteed sell"); dv.minCap:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])
dv.maxCap = card:CreateFontString(nil, "OVERLAY")
dv.maxCap:SetFont(F_BODY, 9); dv.maxCap:SetPoint("TOPRIGHT", -trackInset, -108)
dv.maxCap:SetJustifyH("RIGHT"); dv.maxCap:SetText("possible up to")
dv.maxCap:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])

dv.stack = card:CreateFontString(nil, "OVERLAY")
dv.stack:SetFont(F_NUM, 11); dv.stack:SetPoint("TOP", 0, -126)
dv.stack:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])

local hint = card:CreateFontString(nil, "OVERLAY")
hint:SetFont(F_BODY, 11)
hint:SetPoint("LEFT", 16, 0); hint:SetPoint("RIGHT", -16, 0)
hint:SetJustifyH("CENTER")
hint:SetText("Search an item and click a result\nto see what the auction bot will pay.")
hint:SetTextColor(COL.muted[1], COL.muted[2], COL.muted[3])

function clearDetail()
  for _, w in pairs(dv) do w:Hide() end
  hint:Show()
end
function showDetail(p)
  hint:Hide()
  for _, w in pairs(dv) do w:Show() end

  local tex = select(10, GetItemInfo(p.itemID))
  dv.icon:SetTexture(tex or "Interface\\Icons\\INV_Misc_QuestionMark")
  dv.name:SetText(p.name); dv.name:SetTextColor(qcolor(p.quality))
  dv.vendor:SetText("vendor sell  " .. M.money(p.sell))

  local function gauge(frac)
    if frac < 0.06 then frac = 0.06 elseif frac > 1 then frac = 1 end
    local trackW = card:GetWidth() - (trackInset * 2) - 2
    if trackW < 20 then trackW = 20 end
    dv.green:SetWidth(trackW * frac)
    dv.gold:SetWidth(trackW * (1 - frac))
  end

  local n = (p.maxStack and p.maxStack > 1) and p.maxStack or nil
  if p.sellMax and p.sellMax > 0 then
    -- Current server: the bot BUYS at one fixed price (green) and SELLS within a range (gold).
    dv.caption:SetText("BOT  BUYS   ·   BOT  SELLS   ·   PER  UNIT")
    gauge(p.buy / p.sellMax)
    dv.minVal:SetText(M.money(p.buy)); dv.minCap:SetText("bot buys at")
    dv.maxVal:SetText(M.money(p.sellMin) .. " - " .. M.money(p.sellMax)); dv.maxCap:SetText("bot sells for")
    if n then
      dv.stack:SetText(string.format("full stack x%d:  buys %s  ·  sells %s - %s", n,
        M.money(p.buy * n), M.money(p.sellMin * n), M.money(p.sellMax * n)))
      dv.stack:Show()
    else
      dv.stack:Hide()
    end
  else
    -- Older server: a buy band.
    dv.caption:SetText("BOT  WILL  PAY   ·   PER  UNIT")
    gauge(p.maxBuy > 0 and (p.minBuy / p.maxBuy) or 0.5)
    dv.minVal:SetText(M.money(p.minBuy)); dv.minCap:SetText("guaranteed sell")
    dv.maxVal:SetText(M.money(p.maxBuy)); dv.maxCap:SetText("possible up to")
    if n then
      dv.stack:SetText(string.format("full stack x%d:  %s  -  %s", n, M.money(p.minBuy * n), M.money(p.maxBuy * n)))
      dv.stack:Show()
    else
      dv.stack:Hide()
    end
  end
end

-- ---- queries --------------------------------------------------------------
function doSearch(term)
  local acc = {}
  state.selected = nil
  status:SetText("searching...")
  clearDetail()
  C.request("ahprice search " .. term,
    function(body)
      local m = M.parseLine(body)
      if m and m.kind == "R" then acc[#acc + 1] = m
      elseif m and m.kind == "N" then status:SetText("no matches for '" .. (m.term or term) .. "'") end
    end,
    function()
      state.results = acc; state.offset = 0; renderRows()
      if #acc == 0 and (status:GetText() or ""):find("^searching") then status:SetText("no matches") end
    end)
end

function doItem(itemID)
  C.request("ahprice item " .. itemID,
    function(body)
      local m = M.parseLine(body)
      if m and m.kind == "P" then showDetail(m)
      elseif m and m.kind == "E" then clearDetail(); status:SetText("item " .. tostring(m.itemID) .. " not found") end
    end,
    function() end)
end

clearDetail()

-- Enter: an item link/id -> price it; otherwise a name search.
eb:SetScript("OnEnterPressed", function(self)
  local text = self:GetText() or ""
  local id = string.match(text, "item:(%d+)")
  if id then
    doItem(tonumber(id))
  else
    local term = string.gsub(text, "^%s*(.-)%s*$", "%1")
    if #term >= 3 then doSearch(term) else status:SetText("type 3+ letters") end
  end
  self:ClearFocus()
end)

-- Shift-clicking an item fires HandleModifiedItemClick. Interacting with bags drops
-- keyboard focus from our search box, so we can't gate on eb:HasFocus() -- instead
-- act whenever the AHPrice window is open, but yield to chat when a chat edit box is
-- active (so linking an item into a chat message still works). Only the link modifier
-- (shift by default) counts, not ctrl-dressup etc.
if type(hooksecurefunc) == "function" then
  hooksecurefunc("HandleModifiedItemClick", function(link)
    if not link or not frame:IsShown() then return end
    if IsModifiedClick and not IsModifiedClick("CHATLINK") then return end
    if ChatEdit_GetActiveWindow and ChatEdit_GetActiveWindow() then return end
    local id = string.match(link, "item:(%d+)")
    if id then
      eb:SetText(GetItemInfo(tonumber(id)) or ("item:" .. id))
      eb:ClearFocus()
      doItem(tonumber(id))
    end
  end)
end

-- ---- minimap button (mirrors BotGrid pattern) ----------------------------
local function toggleWindow()
  if frame:IsShown() then frame:Hide() else frame:Show(); eb:SetFocus() end
end

local mb = CreateFrame("Button", "AHPriceMinimapButton", Minimap)
mb:SetWidth(31); mb:SetHeight(31); mb:SetFrameStrata("MEDIUM"); mb:SetFrameLevel(8)
mb:RegisterForClicks("LeftButtonUp"); mb:RegisterForDrag("LeftButton")
local micon = mb:CreateTexture(nil, "BACKGROUND")
micon:SetWidth(20); micon:SetHeight(20); micon:SetPoint("CENTER")
micon:SetTexture("Interface\\Icons\\INV_Misc_Coin_01")
local mborder = mb:CreateTexture(nil, "OVERLAY")
mborder:SetWidth(53); mborder:SetHeight(53); mborder:SetPoint("TOPLEFT")
mborder:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
local function mbPos()
  local angle = math.rad((NS.db and NS.db.minimapPos) or 200)
  mb:ClearAllPoints()
  mb:SetPoint("CENTER", Minimap, "CENTER", 80 * math.cos(angle), 80 * math.sin(angle))
end
mb:SetScript("OnDragStart", function(self)
  self:SetScript("OnUpdate", function()
    local mx, my = Minimap:GetCenter()
    local scale = Minimap:GetEffectiveScale()
    local px, py = GetCursorPosition(); px, py = px / scale, py / scale
    if NS.db then NS.db.minimapPos = math.deg(math.atan2(py - my, px - mx)) end
    mbPos()
  end)
end)
mb:SetScript("OnDragStop", function(self) self:SetScript("OnUpdate", nil) end)
mb:SetScript("OnClick", function() toggleWindow() end)
mb:SetScript("OnEnter", function(self)
  GameTooltip:SetOwner(self, "ANCHOR_LEFT")
  GameTooltip:AddLine("AHPrice")
  GameTooltip:AddLine("Click to open the buy-range lookup.", 1, 1, 1)
  GameTooltip:Show()
end)
mb:SetScript("OnLeave", function() GameTooltip:Hide() end)
mbPos()

-- ---- fade-in on show ------------------------------------------------------
frame:SetScript("OnShow", function(self)
  self._t = 0; self:SetAlpha(0)
  self:SetScript("OnUpdate", function(s, e)
    s._t = s._t + e
    local a = s._t / 0.16
    if a >= 1 then a = 1; s:SetScript("OnUpdate", nil) end
    s:SetAlpha(a)
  end)
  refreshPlaceholder()
end)

-- ---- events + slash -------------------------------------------------------
local ev = CreateFrame("Frame")
ev:RegisterEvent("ADDON_LOADED")
ev:RegisterEvent("CHAT_MSG_ADDON")
ev:SetScript("OnEvent", function(_, event, a1, a2)
  if event == "ADDON_LOADED" and a1 == "AHPrice" then
    AHPriceDB = AHPriceDB or {}
    if AHPriceDB.minimapPos == nil then AHPriceDB.minimapPos = 200 end
    NS.db = AHPriceDB
    mbPos()
    if DEFAULT_CHAT_FRAME then
      DEFAULT_CHAT_FRAME:AddMessage("|cff33ff99AHPrice|r loaded. /ahprice to open, /ahprice debug to trace.")
    end
  elseif event == "CHAT_MSG_ADDON" then
    C.dispatch(a1, a2)
  end
end)

SLASH_AHPRICE1 = "/ahprice"
SlashCmdList["AHPRICE"] = function(msg)
  msg = string.lower(string.gsub(msg or "", "^%s*(.-)%s*$", "%1"))
  if msg == "debug" then
    C.debug = not C.debug
    DEFAULT_CHAT_FRAME:AddMessage("AHPrice debug: " .. (C.debug and "ON (traces TX/RX)" or "off"))
    return
  end
  toggleWindow()
end
