# Vanilla item ↔ era-clone interaction audit (2026-08-26)

**Question:** vanilla items (class set bonuses, relics, equip/use effects) that modify or proc
off *specific spells* do so by SpellFamilyName + SpellClassMask matching — do they still work
when a Vanilla-era character's spell is one of our `[920000,933000)` / `[931000,931399]` clones
instead of the stock id?

**Method:** `tools/era_item_audit.py` (see its header for input extraction). Swept **187
vanilla item sets (488 set-bonus spells)** + **3,728 equip/on-use item spells** on items below
the TBC id threshold (23728), against all **1,635 live clone rows** in `acore_world.spell_dbc`.
Mask auras (107/108/109/112) matched directly; aura-42 procs matched through their real proc
condition (`spell_proc` row — this fork loads ONLY `spell_proc`; `spell_proc_event` is dead
data — falling back to the effect's own class mask, mirroring `SpellMgr::LoadSpellProcs`
defaults). Raw flags cross-checked against `SpellInfoCorrections.cpp` (which rewrites masks at
runtime): of all swept bonus spells only **21895, 28851, 28853** get mask corrections, and
**23591/26135** get a ProcFlags correction — all folded into the verdicts below. Machine
output archived at the end.

## Verdict summary

The clone pipeline held up. Out of 1,126 distinct bonus spells swept, **four real losses**
(all narrow), everything else verified working or not applicable.

**2026-08-26 FIX ROUND (same day, gen `2b88784f`):** findings 1, 3 and 4 are FIXED — see
"Fixes shipped" below. Finding 2 (Cryptstalker/Aimed Shot) stays an accepted gap. A
correction to the audit tool also landed mid-round: the per-effect class-mask columns in
Spell.dbc/`spell_dbc` are LETTER=effect / `_1/_2/_3`=word (effect 1's 96-bit mask =
`EffectSpellClassMaskA_1/A_2/A_3`); the first pass mis-read `(A_i, B_i, C_i)` as one effect's
three words, so word-B/C-only masks were invisible. The corrected sweep surfaced finding 4
(Redemption 2pc) and the Seal of Light/Wisdom libram rows (benign — cost mod, payload half),
and confirmed everything else unchanged.

### Real losses for Vanilla-era characters

1. **Rank-10/13 PvP paladin gloves — “+20 Judgement damage” (bonus 23300, items 16410 /
   16471 / 23274).** SPELLMOD_EFFECT1 +20 binds fam 10, word-A bit23 (0x800000). Era
   judgement damage lives on the JoR payload clones **932716-932723**, which carry only
   word-C bit10 (1024, the Improved-SoR bind) — the +20 never applies. The Judgement button
   932746 *does* match (bit23) but its effect 1 is the script dummy, so the mod lands on
   nothing. **Fix candidate:** add 0x800000 to `SpellClassMask_1` of 932716-932723. Over-bind
   check at authoring time: bit23 also re-admits the payloads to the T2-8pc/EternalJustice-3pc
   proc conditions (stock JoR 20187 carried bit23+bit10, so that *restores* stock parity), and
   to Improved Judgement (cooldown) / Benediction (cost) mods, both inert on a no-cost
   no-cooldown triggered payload. bit23 is not a seal-identity bit (no SPELL_SPECIFIC_SEAL
   risk). **FIXED — see below.**

2. **Cryptstalker (T3 hunter) 8pc — “-20 mana on Aimed Shot and Multi-Shot” (28751).** Mask
   fam 9, bits 12+17. The Aimed Shot clones **932952-932957 deliberately zero family+mask**
   to escape the `SpellInfoCorrections` Aimed-Shot sweep (`SpellFamilyFlags[0] & 0x20000` →
   forces the WotLK 10s RecoveryTime + NO_CATEGORY_COOLDOWN_MODS), so the Aimed Shot half of
   the discount is lost. Restoring the bit would re-impose the WotLK cooldown — worse than the
   loss. Multi-Shot half unaffected (stock spell). **Recommended: accepted gap** (ledger it).

3. **The Earthfury (T1 shaman) 3pc — “+totem radius” (21895, runtime-corrected mask
   0x0603E000/0x00200100).** Radius mods bind the *pulse*, and the pulse clones for Healing
   Stream / Mana Spring / Stoneskin / Strength of Earth / all three resistances / Grounding /
   Windfury all match → those work. Lost only for: **Mana Tide** (the live flat-pulse rebuild
   931371-931373 was family 0 — *no family-bound mod could ever reach it, item or talent*) and
   **Flametongue Totem** (the enchant-EFFECT clones 931148-931151 were fam 11 / flags 0).
   **FIXED — see below.**

4. **Redemption (T3 paladin) 2pc — “+20 Judgement of Light healing” (28775, ALL_EFFECTS on
   fam-10 word-B bit0 = stock JoL triggered-heal 20267's exact identity).** Surfaced by the
   corrected word-B/C sweep. The era JoL heal lands via carrier 932747 (cast by the attacker
   with the paladin as original caster — same machinery as stock 20267), which shipped with
   no family identity, so the +20 never applied. **FIXED — see below.**

### Fixes shipped (2026-08-26, gen `2b88784f`)

All four are YAML mask/family additions shipped via `era-regen.sh` (era_audit 0 findings,
150/150 host tests, dev-DB rows verified by SELECT, worldserver restarted):

* **JoR damage payloads 932716-723 + word-A bit23 (8388608)** — restores the PvP gloves' +20
  (finding 1) and stock proc-source parity for T2-8pc/Eternal-Justice-3pc. Benediction (cost)
  and Improved Judgement (cooldown) also bind bit23 but are inert on the no-cost no-cooldown
  payloads (their "no over-bind" comments updated). Deliberately NOT added to the JoL/JoW
  debuffs (their Effect_1 amount drives the era heal/mana scripts) or the shared SoC
  proc/judgement 932607 (doubles as the seal's swing proc) — **accepted sub-gap: a judged
  Seal of Command doesn't get the gloves' +20.**
* **Mana Tide pulses 931371-373 → family 11 + word-A bit26 (67108864)** — stock 16191's own
  identity. Earthfury 3pc reaches them, and era Totemic Mastery's mask already carried bit26
  (shared resistance-pulse bit), so TM now extends Mana Tide too (Vanilla-authentic). Only
  radius mods bind bit26; Restorative Totems deliberately binds bits 13/14 and stays MS+HS-only.
* **FT enchant-EFFECT clones 931148-151 → word-A bit25 (33554432)** — stock 52109's unique
  identity (NOT the donor 52041's mask, which would leak Healing Stream binds). Earthfury 3pc
  radius reaches the 20yd imbue area; **era Totemic Mastery's word-A mask extended by bit25**
  (67231744 → 100786176, `affects` + comment updated) — Vanilla TM covered FT Totem. Improved
  Weapon Totems' magnitude bind on bit25 stays inert (enchant basePoints 0) — that ACCEPTED
  GAP (SpellItemEnchantment magnitude, Rockbiter-shaped) still stands and is re-documented.
* **JoL heal carrier 932747 → family 10 + word-B bit0 (maskB 1)** — fixes finding 4. Full
  fam-10 word-B bit0 sweep: the only binders are 28775 itself and TBC's 37182 (same "+20 JoL
  heal" effect — harmless/desired later); no spell_proc row or core hook keys the bit.

Remaining runtime spot-checks for the next real-client pass: T2 Judgement 8pc / Eternal
Justice 3pc proccing off the era Judgement button; Earthfury 3pc visibly growing totem radii
(incl. Mana Tide + FT); the PvP gloves' +20 showing in JoR damage; Redemption 2pc +20 on JoL
heals.

### Notable verified-working (the ones that looked broken but aren't)

* **T2 Judgement 8pc (23591) + Battlegear of Eternal Justice 3pc (26135):** proc condition is
  fam 10 + bit23, ProcFlags corrected to DONE_SPELL_MELEE_DMG_CLASS, SpellTypeMask 0 (no type
  filter). The era Judgement button 932746 keeps bit23 **and** DefenseType 3 (melee, verified
  in `spell_dbc`), so the button's hit event satisfies family, proc-flag, phase (HIT) and
  default hit-mask checks in `SpellMgr::CanSpellTriggerProcOnEvent`. Era payloads no longer
  double as proc sources — same one-proc-per-judge as Vanilla. *Worth one live smoke test.*
* **Librams of Hope (22401) / Fervor (23203) seal-cost:** bind the seal *castables* — all era
  seal ranks (SoR 932700-707, SoC 932606/932750-753, SoJ 932724) match; the “broken” rows are
  the hidden on-hit payloads, where a cost mod is meaningless.
* **Librams of Light / Divinity (+FoL healing):** corrections re-mask them to Flash of Light
  (0x40000000) — stock spell, era doesn't clone it.
* **Lawbringer 3pc (JoL/JoW proc chance, bit19) and Avenger's Battlegear 3pc (judgement
  duration):** the era JoL/JoW debuff clones (932733-736 / 932743-745) inherit bit19 → match.
* **Redemption 4pc (LoH cooldown):** era LoH clones 932668-670 match.
* **Freethinker's 5pc (Blessing duration):** era BoS castables match; only the hidden
  block-damage triggers miss (duration is meaningless there).
* **Earthshatterer 2pc (totem cost):** binds the *summons* — era summon clones match
  (incl. Mana Tide 932407/931369-70 and Windfury). 4pc (Mana Spring +25%) binds the pulse —
  matches. 6pc “Totemic Power” actually procs off Healing Wave/LHW (mask 192) — stock.
* **Earthfury 5pc / Ten Storms 8pc:** proc off HW/LHW (mask 192) — stock spells.
* **Unmarred Vision of Voodress (totem cost) / totem relics (Rage, Storm, Life, Sustaining,
  Flowing Water) / Idols (Rejuvenation, Moon, Longevity):** all key on stock baseline spells
  (shocks, LB/CL, LHW, HT, Rejuv, Moonfire) that the Vanilla era does not clone.
* **Bonescythe 2pc/4pc/6pc:** the Hemorrhage clones 932985-987 keep their family bits — all
  three interactions (energize proc, head-rush proc, threat mod) match.
* **Plagueheart 6pc (Siphon Life +%):** era Siphon Life clones match.
* **Battlegear of Wrath 5pc:** proc mask matches the Death Wish clone 932808; the “missed”
  Concussion Blow rows are a false positive — stock player CB 12809 (bit26) never matched the
  proc mask either (only fam-4 NPC variants 22427/32588/52719 do).
* **T3 dummy bonuses (Dreamwalker 2pc/6pc/8pc, Faith 4pc/8pc, Redemption 6pc “Holy Power”,
  Oracle 3pc, Nightslayer 8pc, Plagueheart 8pc, Frostfire, Dragonstalker 8pc, Enigma):** all
  resolve to scripts/procs keyed on stock baseline spells (HT, Regrowth, GH, HL/FoL bits
  30+31, Vanish by id, Life Tap, Blizzard) or have no family filter — unaffected.
* Every aura-42 weapon/trinket proc with no family filter (72 of them — Darkmoon cards,
  weapon procs, etc.): fire on generic proc flags, era-safe by construction.

### Scope notes

* “Vanilla item” = max member/entry id < 23728. A few TBC low-id crafted sets (Frozen
  Shadoweave, Spring Tuxedo) leak through the filter; their rows were checked and are
  irrelevant (they target stock TBC-era spells).
* Enchantments not swept — vanilla-obtainable enchants are generic stat/proc effects with no
  spell-mask binding.
* Side finding (talent-side, not item-side): the family-0 Mana Tide flat pulse also means the
  era **Totemic Mastery / Restorative Totems**-style family-bound talent mods cannot reach
  Mana Tide's pulse. Vanilla Totemic Mastery did extend Mana Tide's radius. Same per-case
  decision as finding 3.

### Re-running

```bash
# extract inputs per the tools/era_item_audit.py header, then:
python3 tools/era_item_audit.py <workdir> > report.txt
```

Re-run whenever a new era (TBC) or class ships clones, swapping the item-id threshold for the
new era's band. Raw 2026-08-26 output: 64 mask rows + 23 proc rows adjudicated as above.
