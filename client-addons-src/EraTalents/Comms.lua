-- ERATAL protocol: receive SYNC (replace-per-message), send HELLO / LEARN.
-- No WoW API calls at file scope (so this file loads under a headless lua5.1 test).
EraTalents = EraTalents or {}
local ET = EraTalents

-- Parse "SYNC <era> <managed> <availPts> [<seq>/<total>] <id:rank,...>". A <seq>/<total>
-- sentinel enables multi-chunk accumulation: seq==1 (or no sentinel) resets
-- era/availPts/ranks; seq>1 merges into the existing ranks. Returns true iff SYNC.
-- `managed` (1/0) is SERVER-AUTHORITATIVE: the server owns the implemented-era allowlist, so
-- the client never decides for itself whether to replace the native talent frame. A TBC-era
-- character (no trees built yet) arrives with managed=0 and keeps Blizzard's WotLK frame.
function ET.ParseSync(state, message)
  local era, managed, pts, rest = string.match(message, "^SYNC%s+(%d+)%s+(%d+)%s+(%-?%d+)%s*(.*)$")
  if not era then return false end
  local seq, total, tail = string.match(rest, "^(%d+)/(%d+)%s*(.*)$")
  local isFirst
  if seq then
    isFirst = (tonumber(seq) == 1)
    rest = tail
  else
    isFirst = true   -- sentinel-less message = single chunk = reset
  end
  state.era = tonumber(era)
  state.availPts = tonumber(pts)
  state.isEraChar = (tonumber(managed) == 1)
  if isFirst then state.ranks = {} end
  for id, rank in string.gmatch(rest, "(%d+):(%d+)") do
    state.ranks[tonumber(id)] = tonumber(rank)
  end
  return true
end

-- Generation canary. The server sends "GEN <stamp>" (from era_talent_meta) after every SYNC;
-- the installed client patch carries the same stamp in sentinel spell 932999's NAME
-- ("EraTalents Gen <stamp>"). A mismatch means the player's Data/patch-V.mpq is a different
-- generation than the server data — tooltips/spells may lie — so warn loudly.
ET.SENTINEL_SPELL_ID = 932999

function ET.CheckGeneration(serverStamp)
  ET.state.serverGen = serverStamp
  local name = GetSpellInfo and GetSpellInfo(ET.SENTINEL_SPELL_ID) or nil
  local clientStamp = name and string.match(name, "Gen (%x+)$") or nil
  ET.state.clientGen = clientStamp
  if clientStamp ~= serverStamp then
    local msg = string.format(
      "|cffff2020EraTalents: your client patch is generation %s but the server is %s. " ..
      "Replace Data/patch-V.mpq with the current one and restart the game.|r",
      clientStamp or "MISSING", serverStamp)
    if DEFAULT_CHAT_FRAME then DEFAULT_CHAT_FRAME:AddMessage(msg) end
  end
end

function ET.OnAddonMsg(prefix, message)
  if prefix ~= ET.PREFIX then return end
  local gen = string.match(message, "^GEN%s+(%x+)$")
  if gen then
    ET.CheckGeneration(gen)
    return
  end
  if ET.ParseSync(ET.state, message) then
    if ET.Refresh then ET.Refresh() end   -- TalentUI hook (may be absent in tests)
  end
end

function ET.Send(payload)
  SendAddonMessage(ET.PREFIX, payload, "WHISPER", UnitName("player"))
end
function ET.RequestSync() ET.Send("HELLO") end
function ET.Learn(id) ET.Send("LEARN " .. id) end

function ET.InitComms()
  local f = CreateFrame("Frame")
  f:RegisterEvent("CHAT_MSG_ADDON")
  f:SetScript("OnEvent", function(self, event, prefix, message)
    ET.OnAddonMsg(prefix, message)
  end)
end
