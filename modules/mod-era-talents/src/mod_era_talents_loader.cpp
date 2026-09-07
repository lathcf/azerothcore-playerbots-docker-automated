#include "EraTalentsConfig.h"
#include "EraTalentContent.h"
#include "EraBandClassifier.h"
#include "EraTalentBots.h"
#include "ScriptMgr.h"
#include "Config.h"
#include "Log.h"

void AddSC_era_talent_pin();
void AddSC_era_talents_comms();
void AddSC_era_talents_commandscript();
void AddSC_era_talent_proc_scripts();
void AddSC_era_talent_pet_scripts();
void AddSC_era_talent_totem_scripts();
void AddSC_era_glyph_gate();
void AddSC_era_advance_gossip();

class era_talents_worldscript : public WorldScript
{
public:
    era_talents_worldscript() : WorldScript("era_talents_worldscript") {}
    void OnAfterConfigLoad(bool /*reload*/) override { sEraTalentsConfig->Load(); }
    void OnStartup() override
    {
        LOG_INFO("module", "[mod-era-talents] startup (enable={})", sEraTalentsConfig->Enabled());
        sEraTalentContent->Load();
        EraBandClassifier::BuildIndex();   // Phase 9.5: reverse index for the band sweep; logs its own canary
        EraTalentBots::BuildTrainerLevelIndex();   // bracket down-move residue index; logs its own count
    }
};

void AddSC_era_talents_worldscript() { new era_talents_worldscript(); }

// Aggregated entry point invoked by the module system.
void Addmod_era_talentsScripts()
{
    AddSC_era_talents_worldscript();
    AddSC_era_talent_pin();
    AddSC_era_talents_comms();
    AddSC_era_talents_commandscript();
    AddSC_era_talent_proc_scripts();
    AddSC_era_talent_pet_scripts();
    AddSC_era_talent_totem_scripts();
    AddSC_era_glyph_gate();
    AddSC_era_advance_gossip();
}
