-- EraTalents bootstrap: shared namespace, state, and the PLAYER_ENTERING_WORLD boot.
EraTalents = EraTalents or {}
local ET = EraTalents

ET.PREFIX = "ERATAL"
-- Server-authoritative state; populated ONLY from SYNC (the client never computes points).
ET.state = { era = nil, availPts = 0, ranks = {}, isEraChar = false }

-- Wire the subsystems (each Init* is defined in its own file; safe to call once).
function ET.Init()
  if ET._inited then return end
  ET._inited = true
  if ET.InitComms then ET.InitComms() end
  if ET.InitUI then ET.InitUI() end
  if ET.InitHook then ET.InitHook() end
  if ET.InitTooltips then ET.InitTooltips() end
end

local boot = CreateFrame("Frame")
boot:RegisterEvent("PLAYER_ENTERING_WORLD")
boot:SetScript("OnEvent", function()
  ET.Init()
  if ET.RequestSync then ET.RequestSync() end   -- pull fresh state (HELLO)
end)

-- /pettalents -- open the stock WotLK talent frame so a hunter can reach the Pet talent
-- side-tab. The era window replaces only the player's own tree; the pet tree is untouched
-- but lives inside the stock frame we normally suppress. Guaranteed route regardless of the
-- on-frame "Pet Talents" button.
SLASH_ERAPETTALENTS1 = "/pettalents"
SLASH_ERAPETTALENTS2 = "/pettalent"
SlashCmdList["ERAPETTALENTS"] = function()
  if ET.OpenStockTalents then ET.OpenStockTalents() end
end
