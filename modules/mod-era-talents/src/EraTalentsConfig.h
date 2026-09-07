#ifndef MOD_ERA_TALENTS_CONFIG_H
#define MOD_ERA_TALENTS_CONFIG_H
#include <string>
class EraTalentsConfig
{
public:
    static EraTalentsConfig* instance();
    void Load();
    bool Enabled() const { return _enabled; }
    bool Debug() const { return _debug; }
    bool BotTalents() const { return _botTalents; }
    bool GlyphGate() const { return _glyphGate; }
    bool AdvanceGossip() const { return _advanceGossip; }
    std::string const& AdvanceTextTBC() const { return _advanceTextTBC; }
    std::string const& AdvanceTextWotLK() const { return _advanceTextWotLK; }
private:
    bool _enabled = true;
    bool _debug = false;
    bool _botTalents = false;
    bool _glyphGate = true;
    bool _advanceGossip = true;
    std::string _advanceTextTBC;
    std::string _advanceTextWotLK;
};
#define sEraTalentsConfig EraTalentsConfig::instance()
#endif
