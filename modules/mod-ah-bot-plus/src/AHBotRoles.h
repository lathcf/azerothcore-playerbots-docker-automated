/*
 * Copyright (C) 2026 azerothcore-playerbots-docker-automated contributors
 * Part of the in-house mod-ah-bot-plus fork; GNU AGPL v3 — see ../LICENSE-AGPL3 and ../NOTICE.
 */

#ifndef AHBOT_ROLES_H
#define AHBOT_ROLES_H

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Seller-role rules (in-house; see NOTICE). Pure: no server headers, so tests/test_ahbot_pricing.cpp builds
// standalone with g++.
namespace AHBotRoles
{
    enum Role : uint8_t { ROLE_MATERIALS = 0, ROLE_CRAFTED = 1, ROLE_GEAR = 2, ROLE_COUNT = 3 };

    using RoleArray = std::array<uint32_t, ROLE_COUNT>;

    // ItemTemplate::Class values that are always Materials, crafted or not.
    constexpr uint32_t CLASS_REAGENT     = 5;
    constexpr uint32_t CLASS_TRADE_GOODS = 7;
    constexpr uint32_t CLASS_RECIPE      = 9;

    char const* RoleName(Role r);

    // Materials beats Crafted: smelted bars, leather and cloth are crafted but sold as mats.
    Role Classify(uint32_t itemClass, bool producedByProfession);

    // "Materials:1501,Crafted:1502,Gear:1503" -> guid per role (0 = unset). Role names are
    // case-insensitive. Unknown roles / malformed pairs are skipped and described in `error`.
    RoleArray ParseSellers(std::string const& text, std::string& error);

    // "45,30,25" -> share per role. Anything but three numbers with a non-zero sum returns the
    // defaults {45,30,25} and sets `error`.
    RoleArray ParseShares(std::string const& text, std::string& error);

    // Per-role listing targets for one house: maxItems * share / sum(shares).
    RoleArray Targets(uint32_t maxItems, RoleArray const& shares);

    // Splits a cycle's listing budget across roles below target, proportionally to each role's
    // deficit (floored), remainder one at a time to the largest deficit. Never exceeds a role's
    // deficit or the budget.
    RoleArray AllocateListings(RoleArray const& counts, RoleArray const& targets, uint32_t budget);

    // Scarcest-first listing: of the drawn candidates, the one with the fewest current listings in
    // the house (ties: earliest draw); 0 if none. Zero entries in `draws` are ignored; an id absent
    // from `listed` counts as 0.
    uint32_t PickLeastListed(std::vector<uint32_t> const& draws, std::unordered_map<uint32_t, uint32_t> const& listed);
}

#endif
