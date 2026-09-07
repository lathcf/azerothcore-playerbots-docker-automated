-- Replace the stock talent frame for era characters. WotLK chars keep Blizzard's.
-- Path-independent: guard PlayerTalentFrame:OnShow so EVERY opener (micro-button, the
-- N keybind, a macro) is intercepted. Hooking ToggleTalentFrame alone missed the
-- micro-button on 3.3.5a (only the keybind routed through it).
EraTalents = EraTalents or {}
local ET = EraTalents

local function GuardStockFrame()
  if not PlayerTalentFrame or PlayerTalentFrame._eraGuarded then return end
  PlayerTalentFrame._eraGuarded = true
  PlayerTalentFrame:HookScript("OnShow", function(self)
    -- ET.OpenStockTalents sets _allowStock so an era char can still reach the WotLK frame
    -- ON PURPOSE -- the only route to the hunter PET talent side-tab, which lives inside this
    -- frame (we era-gate the player's own tree, never the pet's).
    if ET._allowStock then return end
    if ET.state and ET.state.isEraChar then
      HideUIPanel(self)     -- era chars never see the WotLK tree...
      ET.TogglePanel()      -- ...they get ours instead (toggles on each open attempt)
    end
  end)
end

-- Land the stock frame directly on the PET talent view. The pet spec tab is the PlayerSpecTab
-- whose specIndex is a "petspec..." string (diag-confirmed on this 3.3.5a build: player specs are
-- "spec1"/"spec2", the pet is "petspec1"); clicking it shows the CURRENT pet's OWN tree -- Blizzard's
-- GetPetTalentTree() resolves Ferocity / Cunning / Tenacity from the active pet, so swapping pets and
-- reopening shows the right tree with no hardcoding -- and it auto-hides the player talent-tree tabs
-- and the Glyphs tab (they only show on a player spec). No pet out => no pet spec tab; hint instead.
local function IsPetSpecTab(tab)
  return tab and type(tab.specIndex) == "string" and tab.specIndex:find("petspec") ~= nil
end

local function SelectPetSpec()
  for i = 1, 6 do
    local tab = _G["PlayerSpecTab" .. i]
    if tab and tab:IsShown() and IsPetSpecTab(tab) then
      tab:Click()
      return true
    end
  end
  return false
end

-- Confine an era hunter to the PET spec tab: while ET._petOnlyMode is set, the player (non-pet)
-- spec tabs are made invisible AND unclickable so the WotLK player talent tree is unreachable.
-- We SetAlpha(0)+EnableMouse(false) rather than Hide() -- Hide() flips IsShown(), which Blizzard's
-- own PlayerTalentFrame layout reads and would fight (and it is the more taint-prone path), whereas
-- alpha/mouse leave its bookkeeping untouched. Re-applied through each tab's OnShow (Blizzard
-- re-shows tabs on every frame update) and fully restored when the frame closes, so a WotLK-era
-- char reaching the frame normally keeps every tab interactive.
local function ApplyPetOnly(tab)
  if ET._petOnlyMode and not IsPetSpecTab(tab) then
    tab:SetAlpha(0); tab:EnableMouse(false)
  else
    tab:SetAlpha(1); tab:EnableMouse(true)
  end
end

local function GuardPlayerSpecTabs()
  for i = 1, 6 do
    local tab = _G["PlayerSpecTab" .. i]
    if tab then
      if not tab._eraPetGuarded then
        tab._eraPetGuarded = true
        tab:HookScript("OnShow", function(self) ApplyPetOnly(self) end)
      end
      ApplyPetOnly(tab)   -- apply to whatever is shown right now
    end
  end
  if PlayerTalentFrame and not PlayerTalentFrame._eraPetHideHooked then
    PlayerTalentFrame._eraPetHideHooked = true
    PlayerTalentFrame:HookScript("OnHide", function()
      ET._petOnlyMode = false
      for i = 1, 6 do
        local t = _G["PlayerSpecTab" .. i]
        if t then t:SetAlpha(1); t:EnableMouse(true) end
      end
    end)
  end
end

-- Open the STOCK WotLK talent frame on demand, bypassing the era guard exactly once, jump straight
-- to the Pet talent tab that the era window deliberately does not replace, and confine the view to
-- pet-only. Requires a pet out (the pet spec tab only exists then). Blizzard_TalentUI is
-- load-on-demand, and LoadAddOn fires ADDON_LOADED synchronously, so GuardStockFrame is attached
-- before we Show; ShowUIPanel fires OnShow synchronously too, so the spec tabs are populated.
function ET.OpenStockTalents()
  if not UnitExists("pet") then
    DEFAULT_CHAT_FRAME:AddMessage("|cffffd000EraTalents:|r summon a pet to view its talent tree.")
    return
  end
  ET._allowStock = true
  if not IsAddOnLoaded("Blizzard_TalentUI") then LoadAddOn("Blizzard_TalentUI") end
  if PlayerTalentFrame then
    ET._petOnlyMode = true
    ShowUIPanel(PlayerTalentFrame)
    GuardPlayerSpecTabs()   -- attach OnShow guards + hide player tabs now (catches the click cascade)
    SelectPetSpec()         -- land on the pet's own tree (correct per active pet)
  end
  ET._allowStock = false
end

function ET.InitHook()
  if ET._hooked then return end
  ET._hooked = true
  -- Blizzard_TalentUI is load-on-demand: guard now if already loaded, else when it loads.
  if IsAddOnLoaded("Blizzard_TalentUI") then GuardStockFrame() end
  local w = CreateFrame("Frame")
  w:RegisterEvent("ADDON_LOADED")
  w:SetScript("OnEvent", function(self, event, name)
    if name == "Blizzard_TalentUI" then GuardStockFrame() end
  end)
end
