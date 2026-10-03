return function(H)
  local NS = H.load("Model.lua")
  local M = NS.Model

  -- parseLine: R (search result)
  local r = M.parseLine("R\t2770\t1\tCopper Ore")
  H.eq(r.kind, "R", "R kind")
  H.eq(r.itemID, 2770, "R itemID")
  H.eq(r.quality, 1, "R quality")
  H.eq(r.name, "Copper Ore", "R name")

  -- parseLine: P (item detail)
  local p = M.parseLine("P\t2770\t1\tCopper Ore\t50\t720\t1250\t20")
  H.eq(p.kind, "P", "P kind")
  H.eq(p.itemID, 2770, "P itemID")
  H.eq(p.quality, 1, "P quality")
  H.eq(p.name, "Copper Ore", "P name")
  H.eq(p.sell, 50, "P sell")
  H.eq(p.minBuy, 720, "P min")
  H.eq(p.maxBuy, 1250, "P max")
  H.eq(p.maxStack, 20, "P maxStack")

  -- parseLine: P from a current server — fields 9/10 carry the bot's sell range; 6/7 = buy price
  local n = M.parseLine("P\t17193\t4\tSulfuron Hammer\t122311\t3280000\t3280000\t1\t3500000\t5100000")
  H.eq(n.buy, 3280000, "P buy")
  H.eq(n.sellMin, 3500000, "P sellMin")
  H.eq(n.sellMax, 5100000, "P sellMax")
  H.eq(n.maxStack, 1, "P maxStack (new)")
  -- an older server sends 8 fields: no sell range
  H.eq(p.sellMin, nil, "P old sellMin")
  H.eq(p.buy, 1250, "P old buy = maxBuy")

  -- control lines
  H.eq(M.parseLine("N\tcopper").kind, "N", "N kind (no results)")
  H.eq(M.parseLine("E\t9999").kind, "E", "E kind (not found)")

  -- malformed
  H.eq(M.parseLine("garbage"), nil, "unknown tag -> nil")
  H.eq(M.parseLine("R\t2770"), nil, "short R -> nil")

  -- copper formatting
  H.eq(M.money(0), "0c", "zero copper")
  H.eq(M.money(50), "50c", "copper only")
  H.eq(M.money(720), "7s 20c", "silver+copper")
  H.eq(M.money(1250), "12s 50c", "silver+copper 2")
  H.eq(M.money(1234567), "123g 45s 67c", "gold silver copper")
  H.eq(M.money(10000), "1g", "exact gold")
end
