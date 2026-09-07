-- ─────────────────────────────────────────────────────────────────────────────
-- Local supplement to mod-individual-progression's zz_optional_spell_damage_and_healing.sql
-- ─────────────────────────────────────────────────────────────────────────────
-- IP's optional restores the vanilla/TBC "spell damage AND healing" split on caster items by a
-- HAND-CURATED list of ~1777 item entries. That list has gaps: 51 TBC HEALER caster items (Spirit,
-- no spell-hit) were left with the WotLK-consolidated BALANCED "spell power" on-equip spell
-- (equal heal/dmg) instead of IP's heal-weighted 81xxx spell. On a healer that means the piece
-- gives ~half the healing the restored economy intends (and shows "Spell Power", not "damage and
-- healing"), so it's a real difference for healers, not just cosmetic.
--
-- This file assigns each missed healer item the correct heal-weighted 81xxx spell, chosen
-- BUDGET-PRESERVING: a balanced S/S item maps to the 81xxx with dmg = round(0.669*S) (heal ≈ 2.95*dmg,
-- IP's own ratio). Verified against IP's actual curation (e.g. covered ilvl-141 healer boots use
-- 81038 = 29/86; this maps Belt of the Crescent Moon 44/44 -> 81038, matching). Power-neutral: total
-- item budget is unchanged, only rebalanced toward healing.
--
-- REQUIRES the same prerequisites as the IP optional it supplements: IP_OPT_SPELL_DMG_HEALING=1 and
-- patch-V/patch-S (server Spell.dbc defining the 81xxx spells). setup.sh copies this into IP's
-- data/sql/world/base/ only when IP_OPT_SPELL_DMG_HEALING=1. Idempotent (UPDATE by entry).
-- Generated 2026-08-12; source item list = healer caster items absent from IP's optional. See
-- docs/verification/ip-integration.md.
-- ─────────────────────────────────────────────────────────────────────────────

UPDATE `item_template` SET `spellid_1` = 81031 WHERE `entry` = 29093; -- Antlers of Malorne head il120 36/36->24/70
UPDATE `item_template` SET `spellid_1` = 81036 WHERE `entry` = 29076; -- Collar of the Aldor head il120 41/41->27/81
UPDATE `item_template` SET `spellid_1` = 81054 WHERE `entry` = 30206; -- Cowl of Tirisfal head il133 55/55->37/118
UPDATE `item_template` SET `spellid_1` = 81060 WHERE `entry` = 31064; -- Hood of Absolution head il146 62/62->41/122
UPDATE `item_template` SET `spellid_1` = 81066 WHERE `entry` = 34403; -- Cover of Ursoc the Mighty head il159 71/71->47/141
UPDATE `item_template` SET `spellid_1` = 81068 WHERE `entry` = 34405; -- Helm of Arcane Purity head il164 75/75->51/152
UPDATE `item_template` SET `spellid_1` = 81016 WHERE `entry` = 31338; -- Charlotte's Ivy neck il100 23/23->15/44
UPDATE `item_template` SET `spellid_1` = 81024 WHERE `entry` = 29060; -- Soul-Mantle of the Incarnate shoulder il120 29/29->19/57
UPDATE `item_template` SET `spellid_1` = 81021 WHERE `entry` = 29079; -- Pauldrons of the Aldor shoulder il120 27/27->18/53
UPDATE `item_template` SET `spellid_1` = 81031 WHERE `entry` = 29095; -- Pauldrons of Malorne shoulder il120 36/36->24/70
UPDATE `item_template` SET `spellid_1` = 81036 WHERE `entry` = 30210; -- Mantle of Tirisfal shoulder il133 40/40->27/81
UPDATE `item_template` SET `spellid_1` = 81036 WHERE `entry` = 30163; -- Wings of the Avatar shoulder il133 41/41->27/81
UPDATE `item_template` SET `spellid_1` = 81042 WHERE `entry` = 31059; -- Mantle of the Tempest shoulder il146 46/46->31/92
UPDATE `item_template` SET `spellid_1` = 81049 WHERE `entry` = 34393; -- Shoulderpads of Knowledge's Pursuit shoulder il159 53/53->35/105
UPDATE `item_template` SET `spellid_1` = 81049 WHERE `entry` = 34391; -- Spaulders of Devastation shoulder il159 53/53->35/105
UPDATE `item_template` SET `spellid_1` = 81058 WHERE `entry` = 34903; -- Embrace of Starlight chest il141 60/60->40/119
UPDATE `item_template` SET `spellid_1` = 81066 WHERE `entry` = 34398; -- Utopian Tunic of Elune chest il159 71/71->47/141
UPDATE `item_template` SET `spellid_1` = 81024 WHERE `entry` = 29257; -- Sash of Arcane Visions belt il110 28/28->19/57
UPDATE `item_template` SET `spellid_1` = 81030 WHERE `entry` = 28565; -- Nethershard Girdle belt il115 35/35->23/68
UPDATE `item_template` SET `spellid_1` = 81038 WHERE `entry` = 30914; -- Belt of the Crescent Moon belt il141 44/44->29/86
UPDATE `item_template` SET `spellid_1` = 81031 WHERE `entry` = 30532; -- Kirin Tor Master's Trousers legs il110 36/36->24/70
UPDATE `item_template` SET `spellid_1` = 81038 WHERE `entry` = 29094; -- Britches of Malorne legs il120 44/44->29/86
UPDATE `item_template` SET `spellid_1` = 81038 WHERE `entry` = 29059; -- Leggings of the Incarnate legs il120 43/43->29/86
UPDATE `item_template` SET `spellid_1` = 81051 WHERE `entry` = 29972; -- Trousers of the Astromancer legs il128 54/54->36/108
UPDATE `item_template` SET `spellid_1` = 81051 WHERE `entry` = 30234; -- Nordrassil Wrath-Kilt legs il133 54/54->36/108
UPDATE `item_template` SET `spellid_1` = 81060 WHERE `entry` = 34905; -- Crystalwind Leggings legs il141 61/61->41/122
UPDATE `item_template` SET `spellid_1` = 81060 WHERE `entry` = 31067; -- Leggings of Absolution legs il146 62/62->41/122
UPDATE `item_template` SET `spellid_1` = 81066 WHERE `entry` = 34386; -- Pantaloons of Growing Strife legs il159 71/71->47/141
UPDATE `item_template` SET `spellid_1` = 81029 WHERE `entry` = 29258; -- Boots of Ethereal Manipulation boots il110 33/33->22/66
UPDATE `item_template` SET `spellid_1` = 81030 WHERE `entry` = 28670; -- Boots of the Infernal Coven boots il115 34/34->23/68
UPDATE `item_template` SET `spellid_1` = 81038 WHERE `entry` = 32239; -- Slippers of the Seacaller boots il141 44/44->29/86
UPDATE `item_template` SET `spellid_1` = 81020 WHERE `entry` = 29255; -- Bands of Rarefied Magic wrist il110 25/25->17/51
UPDATE `item_template` SET `spellid_1` = 81020 WHERE `entry` = 28453; -- Bracers of the White Stag wrist il115 26/26->17/51
UPDATE `item_template` SET `spellid_1` = 81020 WHERE `entry` = 28477; -- Harbinger Bands wrist il115 26/26->17/51
UPDATE `item_template` SET `spellid_1` = 81024 WHERE `entry` = 33285; -- Fury of the Ursine wrist il128 29/29->19/57
UPDATE `item_template` SET `spellid_1` = 81030 WHERE `entry` = 30870; -- Cuffs of Devastation wrist il141 34/34->23/68
UPDATE `item_template` SET `spellid_1` = 81034 WHERE `entry` = 34447; -- Bracers of the Tempest wrist il154 39/39->26/77
UPDATE `item_template` SET `spellid_1` = 81034 WHERE `entry` = 34434; -- Bracers of Absolution wrist il154 39/39->26/77
UPDATE `item_template` SET `spellid_1` = 81029 WHERE `entry` = 29092; -- Gloves of Malorne hands il120 33/33->22/66
UPDATE `item_template` SET `spellid_1` = 81025 WHERE `entry` = 29057; -- Gloves of the Incarnate hands il120 30/30->20/59
UPDATE `item_template` SET `spellid_1` = 81036 WHERE `entry` = 30232; -- Nordrassil Gauntlets hands il133 41/41->27/81
UPDATE `item_template` SET `spellid_1` = 81036 WHERE `entry` = 30205; -- Gloves of Tirisfal hands il133 41/41->27/81
UPDATE `item_template` SET `spellid_1` = 81037 WHERE `entry` = 29987; -- Gauntlets of the Sun King hands il138 42/42->28/84
UPDATE `item_template` SET `spellid_1` = 81042 WHERE `entry` = 34406; -- Gloves of Tyri's Power hands il164 47/47->31/92
UPDATE `item_template` SET `spellid_1` = 81016 WHERE `entry` = 31921; -- Yor's Collapsing Band finger il100 23/23->15/44
UPDATE `item_template` SET `spellid_1` = 81024 WHERE `entry` = 31339; -- Lola's Eve finger il100 29/29->19/57
UPDATE `item_template` SET `spellid_1` = 81027 WHERE `entry` = 28602; -- Robe of the Elder Scribes chest il115 32/32->21/62
UPDATE `item_template` SET `spellid_1` = 81046 WHERE `entry` = 29077; -- Vestments of the Aldor chest il120 49/49->33/99
UPDATE `item_template` SET `spellid_1` = 81051 WHERE `entry` = 33317; -- Robe of Departed Spirits chest il128 54/54->36/108
UPDATE `item_template` SET `spellid_1` = 81054 WHERE `entry` = 30196; -- Robes of Tirisfal chest il133 55/55->37/118
UPDATE `item_template` SET `spellid_1` = 81066 WHERE `entry` = 34399; -- Robes of Ghostly Hatred chest il159 71/71->47/141
