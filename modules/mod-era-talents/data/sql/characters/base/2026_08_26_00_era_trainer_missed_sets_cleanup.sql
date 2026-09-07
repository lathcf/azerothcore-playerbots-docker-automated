-- mod-era-talents Phase 3: retro-cleanup for the missed-set trainer holes (IP's own idiom,
-- cf. its zz_optional_limit_spells_to_expansion.sql). Removes spells learned through the
-- holes from characters below the gate; the factory trainer walk (Trainer::CanTeachSpell
-- respects ReqLevel) self-heals go-forward. Applied at worldserver startup, before any
-- session loads, so in-memory spellbooks cannot resurrect the rows. Idempotent.
DELETE cs FROM `character_spell` cs JOIN `characters` c ON c.`guid` = cs.`guid`
  WHERE cs.`spell` = 31789 AND c.`class` = 2 AND c.`level` < 61;
DELETE cs FROM `character_spell` cs JOIN `characters` c ON c.`guid` = cs.`guid`
  WHERE cs.`spell` = 62124 AND c.`class` = 2 AND c.`level` < 71;

-- Stale grants from before setup.sh's IP mount config (the Use*MountAtMinLevel knobs only
-- gate NEW grants, they never revoke): Expert/Artisan Riding below the level-70 grant
-- point, Cold Weather Flying below the WotLK band. A real sub-70 character cannot
-- legitimately hold these (IP trainer ReqLevel 70 + the era level cap). The riding SKILL
-- value is left alone — mount use keys off the spell.
DELETE cs FROM `character_spell` cs JOIN `characters` c ON c.`guid` = cs.`guid`
  WHERE cs.`spell` IN (34090, 34091) AND c.`level` < 70;
DELETE cs FROM `character_spell` cs JOIN `characters` c ON c.`guid` = cs.`guid`
  WHERE cs.`spell` = 54197 AND c.`level` < 71;
