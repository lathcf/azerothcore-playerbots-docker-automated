-- test_comms.lua — headless: Comms.lua must load with no file-scope WoW API calls.
dofile("client-addons-src/EraTalents/Comms.lua")
local ET = EraTalents
ET.state = { ranks = {} }

assert(ET.ParseSync(ET.state, "SYNC 0 1 47 18001:3") == true, "should recognize SYNC")
assert(ET.state.era == 0, "era parsed")
assert(ET.state.availPts == 47, "avail parsed")
assert(ET.state.isEraChar == true, "era 0 managed => era char")
assert(ET.state.ranks[18001] == 3, "rank parsed")
assert(ET.ParseSync(ET.state, "LEARN 18001") == false, "non-SYNC returns false")

-- WotLK era => server sends managed=0 => not an era char (keeps native frame)
ET.ParseSync(ET.state, "SYNC 2 0 0 ")
assert(ET.state.isEraChar == false, "era 2 unmanaged => not an era char")

-- TBC era with no trees implemented => server sends managed=0 => keeps native WotLK frame.
-- This is the guard against regressing the TBC fallback: isEraChar must be driven by the
-- server's `managed` flag, NOT derived client-side from the era number.
ET.ParseSync(ET.state, "SYNC 1 0 51 ")
assert(ET.state.era == 1, "TBC era parsed")
assert(ET.state.availPts == 51, "TBC avail parsed")
assert(ET.state.isEraChar == false, "era 1 unmanaged => not an era char (native WotLK talents)")

-- A future managed TBC (trees shipped) would arrive managed=1 and DO replace the frame.
ET.ParseSync(ET.state, "SYNC 1 1 51 ")
assert(ET.state.isEraChar == true, "era 1 managed => era char once trees ship")

-- Multi-chunk accumulation with the <seq>/<total> sentinel: seq 1 resets ranks, seq>1 merges.
-- Guards the parse path against the header change (the new `managed` field sits ahead of the
-- sentinel, so a bad regex would misread the sentinel or drop later chunks).
ET.state = { ranks = {} }
ET.ParseSync(ET.state, "SYNC 0 1 47 1/2 18001:3,18002:1")
assert(ET.state.era == 0 and ET.state.availPts == 47, "chunk 1 header parsed past sentinel")
assert(ET.state.isEraChar == true, "chunk 1 managed flag parsed")
assert(ET.state.ranks[18001] == 3 and ET.state.ranks[18002] == 1, "chunk 1 ranks parsed")
ET.ParseSync(ET.state, "SYNC 0 1 47 2/2 18003:5")
assert(ET.state.ranks[18001] == 3, "chunk 2 (seq>1) accumulates — keeps chunk-1 ranks, no reset")
assert(ET.state.ranks[18003] == 5, "chunk 2 merged its new rank")
print("comms OK")

-- GEN canary: server stamp vs client sentinel spell 932999
ET.PREFIX = "ERATAL"   -- headless: normally set by EraTalents.lua
local warned = nil
_G.GetSpellInfo = function(id)
  if id == 932999 then return "EraTalents Gen a1b2c3d4" end
end
_G.DEFAULT_CHAT_FRAME = { AddMessage = function(_, msg) warned = msg end }
ET.state = { ranks = {} }
ET.OnAddonMsg(ET.PREFIX, "GEN a1b2c3d4")
assert(ET.state.serverGen == "a1b2c3d4", "serverGen stored")
assert(ET.state.clientGen == "a1b2c3d4", "clientGen read from sentinel")
assert(warned == nil, "no warning when stamps match")
ET.OnAddonMsg(ET.PREFIX, "GEN ffffffff")
assert(warned and string.find(warned, "ffffffff", 1, true), "warns on stamp mismatch")
-- a GEN message must NOT fall through to ParseSync/Refresh
local refreshed = false
ET.Refresh = function() refreshed = true end
ET.OnAddonMsg(ET.PREFIX, "GEN a1b2c3d4")
assert(refreshed == false, "GEN handled before SYNC parsing")
ET.Refresh = nil
print("GEN canary OK")

-- no patch installed at all: GetSpellInfo(932999) returns nil -> "MISSING" warning
warned = nil
_G.GetSpellInfo = function() return nil end
ET.OnAddonMsg(ET.PREFIX, "GEN a1b2c3d4")
assert(warned and string.find(warned, "MISSING", 1, true), "warns MISSING when sentinel absent")
print("MISSING sentinel OK")

-- Reset is trainer-driven now (spec 2026-08-25-era-talents-trainer-reset-design.md): the addon
-- must carry NO client reset path — the server ignores a spoofed RESET verb too.
assert(ET.ResetTalents == nil, "ET.ResetTalents must be removed (trainer-driven reset)")
assert(type(ET.Learn) == "function", "ET.Learn must still exist")
print("test_comms OK")
