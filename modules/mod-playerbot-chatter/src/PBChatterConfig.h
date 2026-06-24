#ifndef MOD_PB_CHATTER_CONFIG_H
#define MOD_PB_CHATTER_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

extern bool        g_PBChatEnable;
extern bool        g_PBChatDebug;
extern std::string g_PBChatUrl;          // full /api/generate URL
extern std::string g_PBChatModel;        // gemma4:e4b
extern bool        g_PBChatThink;        // false -> send "think": false
extern std::string g_PBChatSystemPrompt;
extern uint32_t    g_PBChatReplyMaxLen;  // chars
extern uint32_t    g_PBChatMaxConcurrent;

// ── Lore sidecar (Tier-2 factual Q&A) ────────────────────────────────────────
extern bool        g_PBChatLoreEnable;   // route likely questions to the lore sidecar
extern std::string g_PBChatLoreUrl;      // full http://ac-lore:8091/ask URL
extern uint32_t    g_PBChatLoreTimeout;  // seconds; on exceed -> reactive fallback

extern float       g_PBChatSayRange;     // yards
extern uint32_t    g_PBChatSayMaxBots;
extern uint32_t    g_PBChatSayChance;    // percent
extern uint32_t    g_PBChatGroupMaxBots;
extern uint32_t    g_PBChatGroupChance;  // percent
extern bool        g_PBChatGroupGuaranteeOne; // party/raid: force >=1 reply if the roll picks none
extern uint32_t    g_PBChatWhisperChance;// percent
extern uint32_t    g_PBChatHistoryLen;

// ── Ambient (self-initiated) chatter ─────────────────────────────────────────
extern bool        g_PBChatAmbientEnable;
extern bool        g_PBChatAmbientGeneral;     // zone General channel
extern bool        g_PBChatAmbientGroup;       // party/raid
extern bool        g_PBChatAmbientGuild;       // guild chat
extern uint32_t    g_PBChatAmbientSeedMin;     // s: cold-start interval lower bound
extern uint32_t    g_PBChatAmbientSeedMax;     // s: cold-start interval upper bound
extern uint32_t    g_PBChatAmbientFollowMin;   // s: active follow-up lower bound
extern uint32_t    g_PBChatAmbientFollowMax;   // s: active follow-up upper bound
extern uint32_t    g_PBChatAmbientActiveWindow;// s: context stays "active" after a line
extern uint32_t    g_PBChatAmbientBotStreakMax;// consecutive bot lines before cooldown
extern uint32_t    g_PBChatAmbientCooldown;    // s: post-streak quiet period
extern uint32_t    g_PBChatAmbientPerBotCooldown; // s: min gap between one bot's lines
extern uint32_t    g_PBChatAmbientMaxPerMin;   // global safety ceiling (msgs/min)
extern uint32_t    g_PBChatAmbientBufferLen;   // per-context rolling buffer depth
extern uint32_t    g_PBChatAmbientWGeneric;    // content weight: generic small talk
extern uint32_t    g_PBChatAmbientWReact;      // content weight: react to recent
extern uint32_t    g_PBChatAmbientWFlavor;     // content weight: class/zone/level
extern uint32_t    g_PBChatAmbientWEvent;      // content weight: event riff

extern std::vector<std::string> g_PBChatCommandKeywords; // lowercased

void PBChatterLoadConfig();

#endif
