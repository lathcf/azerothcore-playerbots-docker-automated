-- RaidRoster: one-click minimap menu for the .raidroster commands (WotLK 3.3.5a).
-- The addon only sends commands; all behavior/validation lives server-side in
-- mod-raid-roster, so command replies (incl. "disabled"/"No roster") show in chat.

-- Run a server dot-command the standard way: push it through the chat edit box.
local function Run(cmd)
    local eb = DEFAULT_CHAT_FRAME.editBox
    eb:SetText(cmd)
    ChatEdit_SendText(eb, 0)
end

-- Dropdown menu (built-in EasyMenu against a hidden UIDropDownMenu frame).
local menuFrame = CreateFrame("Frame", "RaidRosterMenuFrame", UIParent, "UIDropDownMenuTemplate")

local SIZES = { 5, 10, 25, 40 }

-- A submenu of the four raid sizes; each appends `suffix` to the login command
-- ("" = auto-detect your role; " tank"/" heal"/" dps" = override).
local function SizeMenu(suffix)
    local t = {}
    for _, n in ipairs(SIZES) do
        t[#t + 1] = {
            text = tostring(n), notCheckable = true,
            func = function() Run(".raidroster login " .. n .. suffix) end,
        }
    end
    return t
end

local menu = {
    { text = "RaidRoster", isTitle = true, notCheckable = true },
    { text = "Create",   notCheckable = true, func = function() Run(".raidroster create") end },
    { text = "Login 5",  notCheckable = true, func = function() Run(".raidroster login 5") end },
    { text = "Login 10", notCheckable = true, func = function() Run(".raidroster login 10") end },
    { text = "Login 25", notCheckable = true, func = function() Run(".raidroster login 25") end },
    { text = "Login 40", notCheckable = true, func = function() Run(".raidroster login 40") end },
    { text = "Login as", notCheckable = true, hasArrow = true, menuList = {
        { text = "Tank",   notCheckable = true, hasArrow = true, menuList = SizeMenu(" tank") },
        { text = "Healer", notCheckable = true, hasArrow = true, menuList = SizeMenu(" heal") },
        { text = "DPS",    notCheckable = true, hasArrow = true, menuList = SizeMenu(" dps") },
    } },
    { text = "Sync",        notCheckable = true, func = function() Run(".raidroster sync") end },
    { text = "Logoff",      notCheckable = true, func = function() Run(".raidroster logout") end },
    { text = "Reset locks", notCheckable = true, func = function() Run(".raidroster reset") end },
    { text = "Status",      notCheckable = true, func = function() Run(".raidroster status") end },
}

-- Minimap button (self-contained; no libraries).
local RADIUS = 80

local btn = CreateFrame("Button", "RaidRosterMinimapButton", Minimap)
btn:SetWidth(31); btn:SetHeight(31)
btn:SetFrameStrata("MEDIUM")
btn:SetFrameLevel(8)
btn:RegisterForClicks("LeftButtonUp")
btn:RegisterForDrag("LeftButton")

local icon = btn:CreateTexture(nil, "BACKGROUND")
icon:SetWidth(20); icon:SetHeight(20)
icon:SetPoint("CENTER", 0, 0)
icon:SetTexture("Interface\\Icons\\INV_Misc_GroupLooking")

local border = btn:CreateTexture(nil, "OVERLAY")
border:SetWidth(53); border:SetHeight(53)
border:SetPoint("TOPLEFT")
border:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")

-- Place the button on the minimap edge at the saved angle (degrees).
local function UpdatePos()
    local angle = math.rad(RaidRosterMinimapDB.pos or 200)
    btn:ClearAllPoints()
    btn:SetPoint("CENTER", Minimap, "CENTER", RADIUS * math.cos(angle), RADIUS * math.sin(angle))
end

-- Drag: recompute the angle from the cursor while held; persist it.
btn:SetScript("OnDragStart", function(self)
    self:SetScript("OnUpdate", function()
        local mx, my = Minimap:GetCenter()
        local scale = Minimap:GetEffectiveScale()
        local px, py = GetCursorPosition()
        px, py = px / scale, py / scale
        RaidRosterMinimapDB.pos = math.deg(math.atan2(py - my, px - mx))
        UpdatePos()
    end)
end)
btn:SetScript("OnDragStop", function(self) self:SetScript("OnUpdate", nil) end)

-- Click (no drag) opens the menu at the cursor.
btn:SetScript("OnClick", function()
    EasyMenu(menu, menuFrame, "cursor", 0, 0, "MENU")
end)

btn:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine("RaidRoster")
    GameTooltip:AddLine("Click for roster commands.", 1, 1, 1)
    GameTooltip:Show()
end)
btn:SetScript("OnLeave", function() GameTooltip:Hide() end)

-- Init saved vars + position once the addon's SavedVariables are loaded.
local loader = CreateFrame("Frame")
loader:RegisterEvent("ADDON_LOADED")
loader:SetScript("OnEvent", function(self, _, name)
    if name ~= "RaidRoster" then return end
    RaidRosterMinimapDB = RaidRosterMinimapDB or {}
    UpdatePos()
    self:UnregisterEvent("ADDON_LOADED")
end)
