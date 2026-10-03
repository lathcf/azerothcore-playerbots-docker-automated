local NS = _G.AHPriceNS or {}
_G.AHPriceNS = NS

local M = {}
NS.Model = M

-- Split a tab-delimited line into fields.
local function fields(line)
  local out = {}
  for v in string.gmatch(line or "", "([^\t]*)\t?") do out[#out + 1] = v end
  -- gmatch above yields a trailing empty; drop it if present.
  if out[#out] == "" then out[#out] = nil end
  return out
end

-- Parse one server output line into a table, or nil if malformed.
function M.parseLine(line)
  local f = fields(line)
  local tag = f[1]
  if tag == "R" then
    if not (f[2] and f[3] and f[4]) then return nil end
    return { kind = "R", itemID = tonumber(f[2]), quality = tonumber(f[3]) or 0, name = f[4] }
  elseif tag == "P" then
    if not (f[2] and f[8]) then return nil end
    return {
      kind = "P",
      itemID = tonumber(f[2]), quality = tonumber(f[3]) or 0, name = f[4] or "",
      sell = tonumber(f[5]) or 0, minBuy = tonumber(f[6]) or 0,
      maxBuy = tonumber(f[7]) or 0, maxStack = tonumber(f[8]) or 1,
      -- current servers: one fixed buy price (fields 6/7) + the bot's own sell range (9/10)
      buy = tonumber(f[7]) or 0,
      sellMin = tonumber(f[9]), sellMax = tonumber(f[10]),
    }
  elseif tag == "N" then
    return { kind = "N", term = f[2] or "" }
  elseif tag == "E" then
    return { kind = "E", itemID = tonumber(f[2]) }
  end
  return nil
end

-- Format copper into "Ng Ns Nc", dropping leading zero units.
function M.money(copper)
  copper = math.floor(tonumber(copper) or 0)
  if copper <= 0 then return "0c" end
  local g = math.floor(copper / 10000)
  local s = math.floor((copper % 10000) / 100)
  local c = copper % 100
  local parts = {}
  if g > 0 then parts[#parts + 1] = g .. "g" end
  if s > 0 then parts[#parts + 1] = s .. "s" end
  if c > 0 then parts[#parts + 1] = c .. "c" end
  if #parts == 0 then return "0c" end
  return table.concat(parts, " ")
end
