/*
 * Copyright (C) 2026 azerothcore-playerbots-docker-automated contributors
 * Part of the in-house mod-ah-bot-plus fork; GNU AGPL v3 — see ../LICENSE-AGPL3 and ../NOTICE.
 */

#include "AHBotRoles.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace
{
    std::string Trim(std::string const& s)
    {
        size_t b = s.find_first_not_of(" \t");
        if (b == std::string::npos)
            return "";
        size_t e = s.find_last_not_of(" \t");
        return s.substr(b, e - b + 1);
    }

    std::string Lower(std::string s)
    {
        for (char& c : s)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    bool ParseUint(std::string const& s, uint32_t& out)
    {
        if (s.empty() || s.size() > 10)
            return false;
        for (char c : s)
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return false;
        unsigned long long v = std::strtoull(s.c_str(), nullptr, 10);
        if (v > 0xFFFFFFFFull)
            return false;
        out = uint32_t(v);
        return true;
    }

    void AddError(std::string& error, std::string const& msg)
    {
        if (!error.empty())
            error += "; ";
        error += msg;
    }
}

namespace AHBotRoles
{
    char const* RoleName(Role r)
    {
        switch (r)
        {
            case ROLE_MATERIALS: return "Materials";
            case ROLE_CRAFTED:   return "Crafted";
            case ROLE_GEAR:      return "Gear";
            default:             return "?";
        }
    }

    Role Classify(uint32_t itemClass, bool producedByProfession)
    {
        if (itemClass == CLASS_TRADE_GOODS || itemClass == CLASS_REAGENT || itemClass == CLASS_RECIPE)
            return ROLE_MATERIALS;
        return producedByProfession ? ROLE_CRAFTED : ROLE_GEAR;
    }

    RoleArray ParseSellers(std::string const& text, std::string& error)
    {
        RoleArray out{};
        std::stringstream ss(text);
        std::string pair;
        while (std::getline(ss, pair, ','))
        {
            pair = Trim(pair);
            if (pair.empty())
                continue;
            size_t colon = pair.find(':');
            if (colon == std::string::npos)
            {
                AddError(error, "'" + pair + "' is not Role:guid");
                continue;
            }
            std::string name = Lower(Trim(pair.substr(0, colon)));
            uint32_t guid = 0;
            if (!ParseUint(Trim(pair.substr(colon + 1)), guid) || guid == 0)
            {
                AddError(error, "'" + pair + "' has no valid guid");
                continue;
            }
            if (name == "materials")
                out[ROLE_MATERIALS] = guid;
            else if (name == "crafted")
                out[ROLE_CRAFTED] = guid;
            else if (name == "gear")
                out[ROLE_GEAR] = guid;
            else
                AddError(error, "unknown role '" + name + "'");
        }
        return out;
    }

    RoleArray ParseShares(std::string const& text, std::string& error)
    {
        RoleArray const defaults{ 45, 30, 25 };
        RoleArray out{};
        std::stringstream ss(text);
        std::string tok;
        size_t n = 0;
        while (std::getline(ss, tok, ','))
        {
            uint32_t v = 0;
            if (n >= ROLE_COUNT || !ParseUint(Trim(tok), v))
            {
                AddError(error, "SellerShares must be three numbers like 45,30,25");
                return defaults;
            }
            out[n++] = v;
        }
        if (n != ROLE_COUNT || uint64_t(out[0]) + out[1] + out[2] == 0)
        {
            AddError(error, "SellerShares must be three numbers like 45,30,25");
            return defaults;
        }
        return out;
    }

    RoleArray Targets(uint32_t maxItems, RoleArray const& shares)
    {
        RoleArray out{};
        uint64_t total = uint64_t(shares[0]) + shares[1] + shares[2];
        if (total == 0)
            return out;
        for (size_t r = 0; r < ROLE_COUNT; ++r)
            out[r] = uint32_t(uint64_t(maxItems) * shares[r] / total);
        return out;
    }

    RoleArray AllocateListings(RoleArray const& counts, RoleArray const& targets, uint32_t budget)
    {
        RoleArray deficit{};
        uint64_t total = 0;
        for (size_t r = 0; r < ROLE_COUNT; ++r)
        {
            deficit[r] = targets[r] > counts[r] ? targets[r] - counts[r] : 0;
            total += deficit[r];
        }
        if (total <= budget)
            return deficit;

        RoleArray out{};
        uint32_t given = 0;
        for (size_t r = 0; r < ROLE_COUNT; ++r)
        {
            out[r] = uint32_t(uint64_t(deficit[r]) * budget / total);
            given += out[r];
        }
        while (given < budget)
        {
            size_t best = ROLE_COUNT;
            for (size_t r = 0; r < ROLE_COUNT; ++r)
                if (out[r] < deficit[r] && (best == ROLE_COUNT || deficit[r] > deficit[best]))
                    best = r;
            if (best == ROLE_COUNT)
                break;
            ++out[best];
            ++given;
        }
        return out;
    }

    uint32_t PickLeastListed(std::vector<uint32_t> const& draws, std::unordered_map<uint32_t, uint32_t> const& listed)
    {
        uint32_t best = 0;
        uint32_t bestCount = 0;
        for (uint32_t id : draws)
        {
            if (id == 0)
                continue;
            auto it = listed.find(id);
            uint32_t n = it == listed.end() ? 0 : it->second;
            if (best == 0 || n < bestCount)
            {
                best = id;
                bestCount = n;
            }
        }
        return best;
    }
}
