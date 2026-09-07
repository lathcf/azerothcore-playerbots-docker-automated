# Era Talents — Cross-Class Real-Client Gotchas

> Superseded by docs/era-talents-framework.md — new items go THERE.

A checklist of failure modes that pass **headless** verification but break (or *look* broken) on a
**real 3.3.5a client**. Run through this whenever authoring a new class's tree, especially before/
during the real-client pass. Each item is a bug that actually shipped and cost a debug round on the
Vanilla Mage or Priest tree — verify against source, don't assume.

## Procs

- **Channels and AoEs deal damage via a *triggered sub-spell* → set `proc.attrMask: 2`.**
  A proc that should fire on "all your `<school>` spells" will silently MISS spells that deal damage
  through a triggered spell, because `Aura::GetProcEffectMask` (SpellAuras.cpp) drops procs whose
  triggering spell is itself triggered unless the aura has `SPELL_ATTR3_CAN_PROC_FROM_PROCS` **or** the
  `spell_proc` row sets `AttributesMask = 2` (`PROC_ATTR_TRIGGERED_CAN_PROC`).
  - Priest **Mind Flay** (15407) deals no periodic-damage aura; its Eff3 is aura 227
    (`PERIODIC_TRIGGER_SPELL_WITH_VALUE`) that casts a separate damage spell **58381** each tick — so
    Blackout / Shadow Weaving procced off SW:Pain (periodic-damage aura, `procSpell == null`) and Mind
    Blast (direct cast) but **not** Mind Flay until `attrMask: 2` was added.
  - Mage **Improved Blizzard** hit the identical gate (Blizzard's damage sub-spell 42208).
  - **Rule of thumb:** every "all your `<school>` spells" proc (Shadow Weaving, Blackout, Winter's
    Chill, Improved Blizzard, …) should set `proc.attrMask: 2`. Cheap insurance; harmless when unneeded.
- **Match by damage SCHOOL, not the family-flag word list, for "all your `<school>` spells."**
  Some spells' family identity lives only in `SpellFamilyFlags` word 3 (e.g. Mind Flay `0,0,0x440`);
  the word-3 proc mask matches by every static check yet not in practice. Use
  `proc: { family: <n>, school: <mask> }` (generator supports both) — unambiguous AND more faithful.
  (Combined with the `attrMask: 2` above; the school switch alone was necessary-but-insufficient for
  Mind Flay — the real gate was the triggered-proc attr.)
- **Verify proc/aura ENUM constants against the fork's headers, never memory.** Wrong values pass
  headless (the generator emits whatever you give it) and only fail in-game. Confirmed traps:
  `PROC_FLAG_KILL = 2` (not 4 = melee auto-attack), `MECHANIC_STUN = 12` (not 7 = root),
  `MOD_MECHANIC_RESISTANCE = 117` (the "chance to resist" aura — NOT the WotLK `MECHANIC_DURATION_MOD`
  = 232), `MOD_SPELL_DAMAGE_OF_STAT_PERCENT = 174` / `…HEALING… = 175`. When unsure, `dump_spell_effects.py`
  a stock analog and copy its exact encoding.

## Client visibility (the "it never procs / does nothing" illusion)

- **Any custom spell the player must SEE needs a `client:` block + a patch rebuild.** A custom aura
  (debuff, stun, buff icon) or castable spell that the 3.3.5a client doesn't ship has NO row in the
  client `Spell.dbc`, so the client renders nothing — the effect still happens server-side but
  *invisibly*, which reads as "it never fired."
  - Priest **Blackout**'s authored stun helper (932970) worked server-side from day one but had no
    `client:` block → no stun icon; combined with its authentic 2–10% chance it looked completely dead
    until `client: { name, icon }` was added and the patch rebuilt.
  - Contrast: stock triggers (Ignite, silence, Winter's Chill, the stun/debuff a *stock* spell applies)
    are already in the client DBC — no `client:` block needed.
  - **Rule:** if you author a helper/debuff/stun/castable via `helpers:` and a player or its target
    should see an icon, add `client: { name, icon }` (a real `SpellIconID`) and rebuild the patch
    (`fetch-client-addons.sh` merges it into patch-V; or `client-patch/build-client-patch.sh`).
- **`CumulativeAura`, not `StackAmount`, is the stack column.** A stacking custom debuff must set
  `stackAmount:` in the helper YAML → the generator writes `CumulativeAura` (spell_dbc col 49).
  `StackAmount` is only the in-memory C++ field name; a literal `StackAmount` key is silently dropped.
- **Talent-panel icons: the Daribon importer leaves `icon: ''` → every node shows "?".** Resolve
  authentic icons from the game DBC (bound nodes → the granted spell's `SpellIconID`; authored nodes →
  the talent-name spell), map `SpellIconID → texture` via `SpellIcon.dbc`, strip the `Interface\Icons\`
  prefix. See the Priest `icon`-population pass for the exact resolver.

## Server data tables this server actually reads

- **Trainers load from the modern `trainer` / `trainer_spell` tables, NOT legacy `npc_trainer`.**
  `ObjectMgr` reads `trainer_spell`; `npc_trainer` (and its templates like 200012) is dead weight the
  worldserver never loads. To gate a trainable spell, edit `trainer_spell` — find the `TrainerId` via
  `creature_default_trainer` (the priest trainer is `trainer.Id = 11`, "Hello, priest!"). Deleting a
  rank-1 row + chaining `ReqAbility1` on higher ranks gates the whole chain. `.reload trainer` applies
  it live. (Keep the `npc_trainer` edits too for consistency, but they're inert here.)
- **Vanilla-talent-that-became-baseline spells leak into the era.** A spell that was a Vanilla TALENT
  but a trainable BASELINE spell in TBC/WotLK (Holy Nova, and likely Mind Flay / Divine Spirit /
  Lightwell) is free to Vanilla-era chars unless gated. IP's spell-block hook only fires for
  *WotLK-stage* players (returns early below level 70 / pre-WotLK), so it does NOT gate low eras. The
  module's `ReconcileBaselineSpells` (extensible `kBaselineSpellGates` table) is the fix: remove the
  rank-1 trainer row + reconcile (grant to TBC/WotLK at the spell's level, strip from Vanilla-without-
  the-talent). Add a row to that table per such spell per class.

## Custom castable spells (stock actives whose WotLK form diverged)

- **A stock ACTIVE talent whose WotLK form diverged from Vanilla can't be fixed by overriding the
  stock id.** `spell_dbc` and the client `Spell.dbc` are **global** — overriding, say, Vampiric
  Embrace (15286) or Power Infusion (10060) to behave like their Vanilla form also changes the spell
  for WotLK-stage players, and a granted active's client tooltip is only ever correct via a
  per-spell client row, which is likewise global. There is no per-era override for an existing id.
  - Priest **Vampiric Embrace** became a self-buff in WotLK; Vanilla's was party-healing off the
    caster's Shadow damage. Priest **Power Infusion** became haste+mana in WotLK; Vanilla's was flat
    +20% spell damage/healing. Neither divergence is expressible by editing 15286/10060 in place.
  - **The era-safe fix: author a custom Vanilla-only castable in the reserved band and have the
    talent grant it instead of the stock id.** The stock id ships untouched for higher eras; the
    custom id only exists (and is only trainable/grantable) at the era that needs the old behavior.
- **The generator's `helpers:` form supports player-castable, multi-effect spells**, not just passive
  auras: `powerCost` / `rangeIndex` / `castTimeIndex` / `cooldown` plus an `effects:` list (per-effect
  `effect` / `aura` / `basePoints` / `misc` / `trigger` / `targetA`) and an optional `client:` block
  for the tooltip/icon. A helper may also carry a `proc:` block, which emits a `spell_proc` row for
  it just like a talent-node aura. **Copy the stock analog's Range/CastTime/Duration indices** via
  `tools/dump_spell_effects.py` rather than guessing — these are DBC row-index lookups, not raw
  numbers, and a wrong index silently produces a spell with the wrong range or cast time.
- **Debuff-anchored heal pattern** (used for Vampiric Embrace): cast on the enemy →
  Effect_1 applies a **visible** DUMMY debuff on the mob (needs a `client:` block — it's the icon the
  target/raid sees), Effect_2 is `TRIGGER_SPELL` and puts a **hidden** proc-buff on the caster (no
  client row — it's never meant to be seen). The proc-buff's C++ `AuraScript`, in `CheckProc`, gates
  on `victim->HasAura(debuffId, caster->GetGUID())` so only Shadow damage landed on a mob carrying
  **this caster's own** debuff triggers the heal — not any Vampiric Embrace debuff from any caster.
- **Party vs raid scope: iterate with `group->SameSubGroup(caster, member)`, never bare
  `GetFirstMember()`/full-group iteration.** `GetFirstMember()` walks the entire `Group`, which spans
  up to a 40-man raid — a real bug caught in review turned a party-only heal into a raid-wide heal
  with up to 40 casts per channel/proc tick. Vanilla's Vampiric Embrace is party-scoped; scope every
  future group-heal AuraScript the same deliberate way, don't default to "whole group."
- **The Improved-X DUMMY-marker registry** (band-gated `SPELL_AURA_DUMMY` misc values on a talent's
  passive marker aura — these must stay globally distinct across all classes, not just within one):
  the authoritative allocation registry lives in `docs/era-talents-framework.md` (do NOT trust any
  next-free value written here — this doc is superseded). **Always allocate the next free value from
  the framework registry** rather than reusing one — a collision would make an unrelated class's C++
  check see the wrong talent as present.

## Process

- **Headless proves STRUCTURE (rows exist, masks intersect, family = correct); it cannot prove
  RUNTIME behavior.** Procs firing, debuffs stacking, heals landing, tooltips rewriting, icons
  rendering, trainer gating — all are real-client-only checks. Budget a real-client pass per class and
  expect a round or two of these gotchas. When a fix can't be reproduced headlessly (no era char of
  that class on the dev box), say so and hand the live re-test to the user.
