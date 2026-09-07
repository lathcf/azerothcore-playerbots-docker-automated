# Era Talents Phase 4 — Vanilla Warlock: Verification Record

> **SUPERSEDED for the 9 parity-debt nodes + Demonic Sacrifice (2026-08-16).** This record covers the
> Phase-4 authoring pass (generation `be34f44f`, the 9 debt nodes shipped as `display_only`). The
> parity-debt scripting pass has since made 7 of those nodes functional and upgraded Demonic Sacrifice
> to authentic Vanilla values — current generation is **`d40e39fa`**. See
> `docs/superpowers/plans/2026-08-15-era-talents-warlock-parity-scripting.md` and its spec; the
> outstanding runtime checklist is that plan's Task 11 (real-client pass).

**Date:** 2026-08-15
**Branch:** `feat/era-talents` (merged to master 2026-09-07)
**Generation stamp:** `be34f44f` (server meta + client patch-V sentinel reconciled)

## Scope delivered

Full Vanilla Warlock talent tree (50 nodes, ids 18201–18250) across Affliction / Demonology /
Destruction, on the existing `mod-era-talents` pipeline plus the two Warlock-first additions: the
native **pet-talent path** (`pet:` generator mechanic + `spell_pet_auras`) and the **Corruption
era cast-time core patch (0019)** + reconcile-managed marker (932930, DUMMY misc 5).

## Headless verification (dev-box, this record)

Boot: `[mod-era-talents] loaded 146 talent nodes (generation be34f44f)` — 49 Mage + 47 Priest +
50 Warlock. Clean boot, no `spell_pet_auras`/`spell_proc`/`sql.sql` errors for warlock ids, no crash.

DB integrity (`acore_world`):

| Metric | Value | Expected |
|---|---|---|
| warlock_nodes (era_talent classId=9) | 50 | 50 |
| wired (distinct talentId with ranks) | 41 | 41 |
| unwired (honest display_only gaps) | 9 | 9 (see below) |
| warlock proc rows (921608–922015) | 14 | ISB 5 + Aftermath 5 + Pyroclasm 2 + Nightfall 2 |
| warlock pet-aura rows (spell_pet_auras) | 21 | pet nodes' per-rank mappings |
| SPELLMOD family-5 guard violations | 0 | 0 (every SPELLMOD binds family 5) |
| Corruption marker 932930 spell_dbc row | 1 | 1 (aura 4 DUMMY, misc 5) |
| Cataclysm cost rows negative (921888–892) | 5/5 | 5 (reduction, post-fix) |
| ISB debuffs 932940–932944 | 5 | 5 (aura 87 shadow-taken +4/8/12/16/20%) |
| Pyroclasm stun 932945 | 1 | 1 (MECHANIC_STUN 12, 3s) |

Binary (Task-9 build, unchanged since — Task 10/11 were data-only): scripts
`era_demonic_sacrifice` / `era_soul_link` / `era_soul_link_relink` all present; patch 0019 present
in the fork source (`SpellInfo.cpp:2759/2773`) and compiled into the Task-9 build. (Note: the
`disassemble | grep 932930` canary needs gdb, which is not installed in the container by default —
confirmed via source + build lineage instead.)

Tooling: `tools/era_audit.py` → 0 findings (incl. Warlock family 5). `gen_era_talents.py --sql`
lint → exit 0. Per-tab `eratalents learn ×maxRank` / `status` / `reset` verified during authoring
(Tasks 6/8/10); prereq gates confirmed (Soul Link needs Demonic Sacrifice; Master Demonologist needs
Unholy Power; Ruin needs Devastation; Conflagrate needs Improved Immolate; Curse of Exhaustion needs
Amplify Curse). Corruption marker 932930 confirmed granted to Vanilla warlocks only (Task 9: 27
warlocks, zero non-warlocks/WotLK-era).

## The 9 tracked `display_only` parity-debt nodes (need C++ scripts for full parity)

These are wired as honest gaps (grid slot visible, not learnable) because their mechanic is not
data-expressible — each would need a bespoke script. **The pet STAT talents the user asked to be at
full parity (Fel Stamina/Intellect, Unholy Power, Improved Imp/Succubus) ARE done and working.**

| Node | id | Why it needs a script |
|---|---|---|
| Improved Voidwalker | 18222 | Torment/Sacrifice/Suffering effectiveness (threat/shield, not a stat) |
| Master Demonologist | 18232 | per-active-demon warlock+pet effects (needs demon-detection) |
| Improved Enslave Demon | 18229 | enslave slow-penalty/resist (scripted stock op) |
| Improved Firestone | 18231 | item-buff enhancer (zero family mask) |
| Improved Spellstone | 18234 | item-buff enhancer (family 0) |
| Improved Drain Soul | 18204 | on-death mana proc gated on active channel + regen-while-casting |
| Improved Drain Mana | 18212 | derived-quantity (% of drained mana → damage) |
| Improved Firebolt | 18239 | Imp Firebolt cast-time (single pet-ability mod) |
| Improved Lash of Pain | 18240 | Succubus Lash of Pain cooldown (single pet-ability mod) |

Additional tracked fidelity notes (functional but imperfect):
- **Improved Shadow Bolt**: ~~no 4-charge cap (generator has no ProcCharges) — debuff runs the full
  12s. Direction/magnitude correct; over-delivers only in window scope. Future scripted pass.~~
  **CLOSED 2026-09-05 (TBC Phase 8 back-port):** helpers 932940-932944 now carry `procCharges: 4`
  plus the consume `spell_proc` block — a charge with no proc row is never spent. The consume
  contract is wago 2.5.4's own 17794-17800 (ProcCharges 4, ProcTypeMask **139936** = the TAKEN
  classes with no TAKEN_PERIODIC, chance 100, **no phase mask** — a TAKEN-only row may not carry
  one), because live 17800 has no `spell_proc` row at all (WotLK Shadow Mastery has no charges).
  Shipped on `feat/era-talents` with the TBC warlock trees; see
  `docs/verification/era-talents-tbc-phase8-warlock.md` 11.
- **Conflagrate 932780-932783 and Soul Link 932900 double-charged mana** — FIXED 2026-09-05 (TBC
  Phase 8, spec Amendment B.9). Their templates 17962 / 19028 carry `ManaCostPercentage 16`, and
  `Spell::CalculatePowerCost` **adds** the percentage cost on top of the authored flat cost, so
  both clones cost their flat mana PLUS 16% of base mana. Both now author `manaCostPct: 0`; every
  TBC clone authors it explicitly. Re-check on the real client: Conflagrate and Soul Link cost
  their authored flat mana only.
- **Historic orphan auto-passives 921745 / 921754 / 921788** — FIXED 2026-09-05 (TBC Phase 8).
  Three Vanilla auto-passives that `StripOrphanedGrants` never caught (it only resolves node
  grants), so they lingered on a WotLK-band warlock; all three are now in the cross-era leftover
  strip, verified gone at the L80 rung of the Phase-8 ladder on the very bot that carried them.
- **Demonic Sacrifice buffs**: reuse stock client-known buffs at WotLK-retuned magnitudes
  (+10% fire/shadow, 2% HP, 3% mana) vs authentic Vanilla (+15%/+15%, 3% HP, 2% mana). Tooltip is
  honest. Path B (custom buffs at authentic values + client rows) is the upgrade if desired.

## Task 12 — real-client checklist (requires a live 3.3.5a Vanilla Warlock; only the user can run)

Install the fresh `client-addons/_data-patches/patch-V.mpq` + the restaged `EraTalents` addon, relaunch:

1. **Canary green** at login (no red generation-mismatch warning; server + client both `be34f44f`).
2. **Panel**: 3-tab Warlock tree renders; all 50 nodes positioned; prereq arrows; 9 gaps show but aren't learnable.
3. **Pet stat** (Fel Stamina/Intellect, Unholy Power): the demon's HP/mana/damage actually rises; **dismiss+resummon → still applied** (spell_pet_auras re-cast).
4. **Improved Corruption cast bar**: 0/5 Corruption casts in ~2s; 5/5 is instant. (Base spellbook tooltip still reads instant — known global-DBC cosmetic gap; the cast bar is the truth.)
5. **Demonic Sacrifice**: sacrificing each demon consumes it and applies the right 30-min buff (icon visible).
6. **Soul Link**: with a demon out, ~30% of damage taken redirects to the pet; **demon dies→resummon → redirect resumes** (OnPetAddToWorld re-link); solo → no redirect.
7. **Nightfall** → Shadow Trance instant Shadow Bolt; **Improved Shadow Bolt** shadow-vuln debuff icon on target; **Pyroclasm** stun icon; **Aftermath** daze — all render from patch-V.
8. **Spellmod tooltips** rewrite (Bane cast time, Improved Immolate damage, Cataclysm cost DOWN, etc.).
9. **Siphon Life** castable renders (custom clone 932902) — verify it's categorized in the Affliction tab (SkillLineAbility merge reported +0 rows; confirm custom castables don't dump into "General").
10. Up/down era transition symmetric, no relog; multi-chunk SYNC intact at full-tree scale.

## Status

Headless: **PASS** (structure, counts, guards, audit, lint, marker gating, cosmetic polish all green).
Real-client (Task 12): **pending user** — the runtime behaviors above cannot be verified headlessly.
Branch merged to master 2026-09-07.
