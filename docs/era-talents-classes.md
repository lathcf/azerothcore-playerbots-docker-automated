# Era Talents — what the mod does, class by class

A summary of what `mod-era-talents` restores for each class in each era. It is meant as an
orientation page, not a spell-by-spell reference — every talent, value, and helper spell is
authored in the YAML under [`era-data/`](../era-data/) (`vanilla/<class>.yaml`,
`tbc/<class>.yaml`), and the authoring rules live in
[`docs/era-talents-framework.md`](era-talents-framework.md). For the player-facing overview see
the [Era talents section of the README](../README.md#era-talents-mod-era-talents).

## How to read this page

- **Era** = the character's Individual Progression stage: Vanilla (stages 0–7), TBC (8–12),
  WotLK (13+). WotLK-era characters — and Death Knights — keep the stock talent trees; the mod
  is inert for them.
- **Talent nodes** = the number of talents in the era's three trees for that class.
- **Custom spells** = distinct server-side spell rows the mod authors for that (era, class) in
  its reserved id band: per-rank talent passives, era clones of abilities WotLK changed, proc
  payloads, and helper effects (buffs/debuffs, totem pulses). None of them replace a stock spell —
  stock spell ids are never modified, because eras are per-character while spells are global.
- **Restored abilities** lists the *visible* things the class gets back — active abilities,
  buffs, debuffs, and procs whose WotLK version differed enough that a clone had to be authored.
  Talents that just retune a stock spell (a passive spell-mod or stat aura) are the bulk of every
  tree and are not itemised here.

The generated Vanilla trees use the 1.12.1 client data as the value reference; the TBC trees use
TBC Classic (2.5.4) values. Where a talent's behaviour lives in core code rather than spell data,
a small band-gated core patch carries it (called out per class below).

## Totals

| | Talent nodes | Custom spells |
|---|---:|---:|
| Vanilla (9 classes) | 432 | 1,681 |
| TBC (9 classes) | 579 | 1,912 |
| **Total** | **1,011** | **3,593** |

## Druid

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 47 | 137 |
| TBC | 62 | 171 |

- **Vanilla:** era versions of Nature's Grace, Nature's Grasp, Insect Swarm, Omen of Clarity,
  Leader of the Pack, and the feral Enrage; the rest of the three trees are passive retunes of the
  stock spells.
- **TBC:** adds Mangle (Bear and Cat), Tree of Life, and Improved Faerie Fire on top of the
  Vanilla set. Heart of the Wild's bear Stamina is a core-code value, so core patch 0023 restores
  the TBC magnitude for TBC-era druids only.

## Hunter

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 46 | 160 |
| TBC | 64 | 172 |

- **Vanilla:** talent-taught Aimed Shot, Scorpid Sting, Wyvern Sting, Bestial Wrath,
  Counterattack, Deterrence, Trueshot Aura, and Spirit Bond; the Entrapment root and the Improved
  Concussive Shot / Improved Wing Clip procs; and the Beast Mastery pet talents (Endurance
  Training, Thick Hide, Unleashed Fury, Ferocity, Bestial Discipline, …) applied to the pet
  through the hunter's own talents, as they were before pets had trees.
- **TBC:** adds The Beast Within, Readiness, Silencing Shot, Scatter Shot, Expose Weakness,
  Ferocious Inspiration, Master Tactician, Rapid Killing, and Bestial Swiftness.

## Mage

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 49 | 159 |
| TBC | 67 | 163 |

- **Vanilla:** Arcane Power, Presence of Mind, Pyroblast, Blast Wave, Combustion, Ice Barrier,
  and Cold Snap at their Vanilla values and cooldowns; the Improved Blizzard chill, Winter's
  Chill, Fire Vulnerability, Magic Absorption, the Improved Counterspell silence, and the Fire /
  Frost Ward reflect procs. Shatter's crit-vs-frozen bonus is core code, so core patch 0018
  carries it.
- **TBC:** adds Dragon's Breath, Slow, Arcane Potency, and Improved Blink. Molten Fury's TBC
  health window (20% instead of WotLK's 35%) is core patch 0026.

## Paladin

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 44 | 249 |
| TBC | 64 | 304 |

- **Vanilla:** the complete Vanilla seal-and-judgement system — Seal of Command, Righteousness,
  Justice, Light, Wisdom, and the Crusader, each with its matching Judgement effect, driven by a
  single era "Judgement" that consumes the seal — plus Holy Shock, Holy Shield, Blessing of
  Sanctuary, Repentance, Sanctity Aura, Vengeance, Vindication, Illumination, Redoubt, and
  Improved Lay on Hands.
- **TBC:** the seals and judgements again at TBC values, adding Seal of Blood / Judgement of
  Blood and Seal of Vengeance / Judgement of Vengeance (with its Holy Vengeance stack), plus
  Crusader Strike and Avenger's Shield.

## Priest

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 47 | 143 |
| TBC | 64 | 188 |

- **Vanilla:** Vampiric Embrace with its scripted party heal, Power Infusion, Blackout,
  Inspiration, and Shadow Weaving's vulnerability debuff at Vanilla values.
- **TBC:** adds Vampiric Touch, Shadowform, Silence, Mind Flay, Misery, Spirit Tap, Circle of
  Healing, Lightwell, Pain Suppression, Divine Spirit / Prayer of Spirit, Surge of Light, Focused
  Will, Reflective Shield, Holy Concentration's clearcasting, and Martyrdom's focused casting.

## Rogue

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 51 | 122 |
| TBC | 67 | 133 |

- **Vanilla:** Adrenaline Rush, Preparation, Premeditation, Hemorrhage, and Riposte at Vanilla
  values and cooldowns, and the Mace Specialization stun proc (WotLK removed the stun entirely).
- **TBC:** adds Mutilate, Find Weakness, and Blade Twisting's daze debuff, with the mace stun
  carried over.

## Shaman

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 46 | 307 |
| TBC | 61 | 367 |

Shaman is the largest class because Vanilla and TBC totems were their own summon + creature +
pulse chains that WotLK reworked or removed.

- **Vanilla:** the full Vanilla totem set as era versions — Windfury, Grace of Air, Windwall,
  Tremor, Strength of Earth, Stoneskin, Stoneclaw, Earthbind, Searing, Magma, Fire Nova (the
  self-destructing Vanilla version), Flametongue, Healing Stream, Mana Spring, Mana Tide,
  Disease / Poison Cleansing, the three resistance totems, Grounding, and Sentry — plus Elemental
  Mastery, Nature's Swiftness, Flurry, Ancestral Healing, Healing Way, and Elemental Focus.
- **TBC:** the totems again at TBC values, adding Totem of Wrath, Wrath of Air, Tranquil Air, and
  the Earth and Fire Elemental totems; Earth Shield, Nature's Guardian, Ancestral Fortitude, and
  Focused Casting. Core patch 0024 makes the era Sentry Totem's camera bind clear on an early
  unsummon (the core hardcodes the stock id).

## Warlock

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 50 | 220 |
| TBC | 64 | 177 |

- **Vanilla:** Conflagrate, Siphon Life, Dark Pact, Soul Link, Demonic Sacrifice, Amplify Curse,
  Master Demonologist, Pyroclasm, Improved Drain Soul / Drain Mana, and Improved Shadow Bolt's
  vulnerability debuff; the Vanilla Firestone / Spellstone items (Create Firestone / Spellstone
  ranks and their Improved-stone passives); and the Vanilla pet buffs (Fel Energy, Fel Stamina).
  Corruption's Vanilla cast time is core code, so core patch 0019 restores it for Vanilla-era
  warlocks.
- **TBC:** adds Unstable Affliction, Shadowfury, Shadow Embrace, Nether Protection, Fel
  Domination, Aftermath, and the Master-rank stones.

## Warrior

| Era | Nodes | Custom spells |
|---|---:|---:|
| Vanilla | 52 | 184 |
| TBC | 66 | 237 |

- **Vanilla:** Bloodthirst, Shield Slam, Death Wish, Last Stand, Concussion Blow, Deep Wounds,
  Flurry, Enrage, and Tactical Mastery at Vanilla values; the Improved Hamstring root, Improved
  Revenge stun, and Mace Specialization stun procs.
- **TBC:** adds Devastate, Rampage, Sweeping Strikes, Blood Frenzy, Blood Craze, Shield
  Specialization, and the Improved Shield Bash silence.

## Death Knight

No era trees. Death Knights are a WotLK class and always use the stock talent frame; the glyph
gate also exempts them.

## Where the rest lives

- **Authoring rules and pipeline:** [`docs/era-talents-framework.md`](era-talents-framework.md)
  (read before changing any era spell) and
  [`docs/era-talents-cross-class-gotchas.md`](era-talents-cross-class-gotchas.md).
- **Regenerating artifacts after a YAML change:** `tools/era-regen.sh` (the only supported way);
  `tools/era_audit.py` must be clean.
- **Per-class verification records:** `docs/verification/era-talents-*.md`.
- **Core patches the mod relies on:** 0018 (Shatter), 0019 (Corruption cast time), 0023 (Heart
  of the Wild), 0024 (Sentry Totem), 0025 (Wand Specialization), 0026 (Molten Fury), plus
  0020/0021/0022 (bot and BotGrid integration) and 0027 (manual expansion advance) — see
  [`patches/`](../patches/).
