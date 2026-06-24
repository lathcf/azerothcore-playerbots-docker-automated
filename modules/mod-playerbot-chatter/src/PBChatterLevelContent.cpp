#include "PBChatterLevelContent.h"

namespace
{
    struct Band
    {
        uint8_t maxLevel;
        std::vector<std::string> dungeons;
        std::vector<std::string> zonesAlliance;
        std::vector<std::string> zonesHorde;
        std::vector<std::string> zonesShared; // used once both factions converge
    };

    // Ordered by maxLevel. raids appear only in the level-80 band.
    const std::vector<Band> kBands = {
        { 19,
          {"the Deadmines", "Wailing Caverns", "Shadowfang Keep", "Blackfathom Deeps"},
          {"Elwynn Forest", "Westfall", "Loch Modan", "Darkshore"},
          {"Durotar", "the Barrens", "Silverpine Forest", "Ghostlands"}, {} },
        { 39,
          {"Scarlet Monastery", "Razorfen Kraul", "Gnomeregan", "Uldaman", "Razorfen Downs"},
          {"Redridge Mountains", "Duskwood", "Wetlands", "Hillsbrad Foothills"},
          {"Stonetalon Mountains", "Thousand Needles", "Hillsbrad Foothills", "Arathi Highlands"}, {} },
        { 59,
          {"Zul'Farrak", "Maraudon", "the Sunken Temple", "Blackrock Depths", "Stratholme", "Scholomance"},
          {}, {},
          {"Tanaris", "Feralas", "the Searing Gorge", "Un'Goro Crater", "the Western Plaguelands"} },
        { 69,
          {"Hellfire Ramparts", "the Blood Furnace", "the Slave Pens", "the Underbog", "Mana-Tombs"},
          {}, {},
          {"Hellfire Peninsula", "Zangarmarsh", "Terokkar Forest", "Nagrand"} },
        { 79,
          {"Utgarde Keep", "the Nexus", "Azjol-Nerub", "Gundrak", "Halls of Stone"},
          {}, {},
          {"Borean Tundra", "Howling Fjord", "Dragonblight", "Grizzly Hills", "Zul'Drak"} },
        { 80,
          {"a heroic", "Naxxramas", "Ulduar", "Trial of the Crusader", "Icecrown Citadel"},
          {}, {},
          {"Icecrown", "the Storm Peaks", "Sholazar Basin"} },
    };

    Band const& BandFor(uint8_t level)
    {
        for (Band const& b : kBands)
            if (level <= b.maxLevel)
                return b;
        return kBands.back();
    }
}

std::vector<std::string> PBChatterLevelContent::Dungeons(uint8_t level)
{
    return BandFor(level).dungeons;
}

std::vector<std::string> PBChatterLevelContent::Zones(uint8_t level, uint8_t team)
{
    Band const& b = BandFor(level);
    if (!b.zonesShared.empty())
        return b.zonesShared;
    return (team == 0) ? b.zonesAlliance : b.zonesHorde;
}
