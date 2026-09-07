#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 tools/gen_era_talents.py --sql era-data/vanilla/mage.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_12_10_era_talent_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/mage.yaml \
  -o client-addons-src/EraTalents/data/generated/MageVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/mage.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_13_00_era_talent_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/priest.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_13_10_era_talent_priest_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/priest.yaml \
  -o client-addons-src/EraTalents/data/generated/PriestVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/priest.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_13_11_era_talent_priest_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/warlock.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_14_12_era_talent_warlock_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/warlock.yaml \
  -o client-addons-src/EraTalents/data/generated/WarlockVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/warlock.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_14_13_era_talent_warlock_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/hunter.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_16_20_era_talent_hunter_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/hunter.yaml \
  -o client-addons-src/EraTalents/data/generated/HunterVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/hunter.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_16_21_era_talent_hunter_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/rogue.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_17_10_era_talent_rogue_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/rogue.yaml \
  -o client-addons-src/EraTalents/data/generated/RogueVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/rogue.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_17_11_era_talent_rogue_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/warrior.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_18_10_era_talent_warrior_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/warrior.yaml \
  -o client-addons-src/EraTalents/data/generated/WarriorVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/warrior.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_18_11_era_talent_warrior_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/paladin.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_18_30_era_talent_paladin_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/paladin.yaml \
  -o client-addons-src/EraTalents/data/generated/PaladinVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/paladin.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_18_31_era_talent_paladin_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/druid.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_20_50_era_talent_druid_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/druid.yaml \
  -o client-addons-src/EraTalents/data/generated/DruidVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/druid.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_20_51_era_talent_druid_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/vanilla/shaman.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_21_60_era_talent_shaman_data.sql
python3 tools/gen_era_talents.py --lua era-data/vanilla/shaman.yaml \
  -o client-addons-src/EraTalents/data/generated/ShamanVanilla.lua
python3 tools/gen_era_talents.py --custom-sql era-data/vanilla/shaman.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_21_61_era_talent_shaman_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/paladin.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_27_00_era_talent_tbc_paladin_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/paladin.yaml \
  -o client-addons-src/EraTalents/data/generated/PaladinTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/paladin.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_27_01_era_talent_tbc_paladin_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/druid.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_31_00_era_talent_tbc_druid_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/druid.yaml \
  -o client-addons-src/EraTalents/data/generated/DruidTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/druid.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_31_01_era_talent_tbc_druid_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/shaman.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_31_10_era_talent_tbc_shaman_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/shaman.yaml \
  -o client-addons-src/EraTalents/data/generated/ShamanTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/shaman.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_08_31_11_era_talent_tbc_shaman_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/warrior.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_03_00_era_talent_tbc_warrior_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/warrior.yaml \
  -o client-addons-src/EraTalents/data/generated/WarriorTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/warrior.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_03_01_era_talent_tbc_warrior_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/rogue.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_04_00_era_talent_tbc_rogue_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/rogue.yaml \
  -o client-addons-src/EraTalents/data/generated/RogueTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/rogue.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_04_01_era_talent_tbc_rogue_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/priest.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_05_00_era_talent_tbc_priest_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/priest.yaml \
  -o client-addons-src/EraTalents/data/generated/PriestTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/priest.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_05_01_era_talent_tbc_priest_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/hunter.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_06_00_era_talent_tbc_hunter_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/hunter.yaml \
  -o client-addons-src/EraTalents/data/generated/HunterTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/hunter.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_06_01_era_talent_tbc_hunter_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/warlock.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_07_00_era_talent_tbc_warlock_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/warlock.yaml \
  -o client-addons-src/EraTalents/data/generated/WarlockTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/warlock.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_07_01_era_talent_tbc_warlock_custom_spells.sql

python3 tools/gen_era_talents.py --sql era-data/tbc/mage.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_08_00_era_talent_tbc_mage_data.sql
python3 tools/gen_era_talents.py --lua era-data/tbc/mage.yaml \
  -o client-addons-src/EraTalents/data/generated/MageTBC.lua
python3 tools/gen_era_talents.py --custom-sql era-data/tbc/mage.yaml \
  -o modules/mod-era-talents/data/sql/world/base/2026_09_08_01_era_talent_tbc_mage_custom_spells.sql
