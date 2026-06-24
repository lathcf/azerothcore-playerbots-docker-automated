#ifndef MOD_PB_CHATTER_OLLAMA_H
#define MOD_PB_CHATTER_OLLAMA_H
#include <string>
namespace PBChatterOllama
{
    // Blocking: builds request, POSTs, returns a sanitized reply ("" on failure/empty).
    std::string Ask(std::string const& systemPrompt, std::string const& prompt);
}
#endif
