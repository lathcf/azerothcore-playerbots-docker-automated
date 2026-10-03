// Pure string helper for PBChatterAmbientPrompt::ScrubReply (see the header for the
// contract). Deliberately std-only — no core/Player headers — so it compiles standalone in
// tests/test_scrub_reply.cpp.
#include "PBChatterAmbientPrompt.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace
{
    // ASCII-only case fold; UTF-8 continuation bytes pass through unchanged.
    char Lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

    // Word character for boundary tests. Bytes >= 0x80 (UTF-8 letters) count as word chars
    // so a name never "ends" in the middle of an accented letter.
    bool IsWord(char c)
    {
        unsigned char u = static_cast<unsigned char>(c);
        return u >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

    // Separators that may follow a leading address or a strong affirmation.
    bool IsLeadSep(char c) { return c == ',' || c == ':' || c == '!' || c == '.' || c == '-' || IsSpace(c); }

    // Case-insensitive "s starts with word at pos".
    bool MatchAt(std::string const& s, size_t pos, std::string const& word)
    {
        if (word.empty() || pos + word.size() > s.size())
            return false;
        for (size_t i = 0; i < word.size(); ++i)
            if (Lower(s[pos + i]) != Lower(word[i]))
                return false;
        return true;
    }

    // Drop leading whitespace and leftover punctuation (", " / ": " / "- " / "!! ").
    void TrimLeft(std::string& s)
    {
        size_t i = 0;
        while (i < s.size() && (IsSpace(s[i]) || s[i] == ',' || s[i] == ':' || s[i] == ';' ||
                                s[i] == '.' || s[i] == '!' || s[i] == '-'))
            ++i;
        s.erase(0, i);
    }

    void TrimSpaceLeft(std::string& s)
    {
        size_t i = 0;
        while (i < s.size() && IsSpace(s[i]))
            ++i;
        s.erase(0, i);
    }

    void TrimRight(std::string& s)
    {
        while (!s.empty() && IsSpace(s.back()))
            s.pop_back();
    }

    // Third-person verdicts that, right after a leading name, make the opener pure
    // affirmation ("Thrandil is right, ..."). Longest first.
    char const* const kVerdicts[] = {
        "is spot on", "is correct", "nailed it", "is right", "said it",
    };

    // Names longest-first so "Thran" never shadows "Thrandil".
    std::vector<std::string> SortedNames(std::vector<std::string> const& names)
    {
        std::vector<std::string> out;
        for (std::string const& n : names)
            if (!n.empty())
                out.push_back(n);
        std::sort(out.begin(), out.end(),
                  [](std::string const& a, std::string const& b) { return a.size() > b.size(); });
        return out;
    }

    // (a) leading "@?name" + separator (or end). Returns true if it stripped something.
    bool StripLeadingName(std::string& s, std::vector<std::string> const& names)
    {
        TrimSpaceLeft(s);
        size_t pos = (!s.empty() && s[0] == '@') ? 1 : 0;
        for (std::string const& n : names)
        {
            if (!MatchAt(s, pos, n))
                continue;
            size_t end = pos + n.size();
            if (end < s.size() && !IsLeadSep(s[end]))
                continue; // "Thrandilson", "Thrandil's" — not an address
            std::string rest = s.substr(end);
            TrimLeft(rest);
            // "<name> is right[, ...]" — the third-person verdict goes with the name.
            for (char const* v : kVerdicts)
            {
                std::string const verdict = v;
                if (MatchAt(rest, 0, verdict) &&
                    (rest.size() == verdict.size() || !IsWord(rest[verdict.size()])))
                {
                    rest.erase(0, verdict.size());
                    TrimLeft(rest);
                    break;
                }
            }
            s = rest;
            return true;
        }
        return false;
    }

    // (b) trailing ", name" / " name" (+ optional trailing . ! ?, which is kept).
    void StripTrailingName(std::string& s, std::vector<std::string> const& names)
    {
        TrimRight(s);
        size_t bodyEnd = s.size();
        while (bodyEnd > 0 && (s[bodyEnd - 1] == '.' || s[bodyEnd - 1] == '!' || s[bodyEnd - 1] == '?'))
            --bodyEnd;
        std::string const punct = s.substr(bodyEnd);
        for (std::string const& n : names)
        {
            if (n.size() > bodyEnd)
                continue;
            size_t start = bodyEnd - n.size();
            if (!MatchAt(s, start, n))
                continue;
            size_t cut = start;
            if (cut > 0 && s[cut - 1] == '@')
                --cut;
            if (cut == 0)
            {
                s.clear(); // the whole line was the name
                return;
            }
            char before = s[cut - 1];
            if (!(IsSpace(before) || before == ','))
                continue; // "Thrandil" must be its own trailing word
            std::string body = s.substr(0, cut);
            while (!body.empty() && (IsSpace(body.back()) || body.back() == ',' ||
                                     body.back() == '-' || body.back() == ':'))
                body.pop_back();
            s = body.empty() ? std::string() : body + punct;
            return;
        }
    }

    // Unambiguous affirmations: stripped when followed by . , ! - or whitespace.
    char const* const kStrongAffirm[] = {
        "that\xE2\x80\x99s right", "that's right", "thats right", "so true",
        "exactly", "agreed", "facts", "100%", "yep", "fr",
    };
    // Ambiguous ones that are also ordinary sentence openers ("this raid is cursed",
    // "true, but ..."): stripped only when followed by . or !.
    char const* const kWeakAffirm[] = {
        "yeah", "same", "real", "true", "this", "ya",
    };

    // (c) ONE leading affirmation token. Returns true if stripped; `s` may become empty
    // when the line was nothing but the affirmation.
    bool StripAffirmation(std::string& s)
    {
        TrimSpaceLeft(s);
        auto tryList = [&s](char const* const* list, size_t count, bool strong) -> bool
        {
            for (size_t i = 0; i < count; ++i)
            {
                std::string const tok = list[i];
                if (!MatchAt(s, 0, tok))
                    continue;
                size_t end = tok.size();
                if (end < s.size())
                {
                    char c = s[end];
                    bool ok = strong ? IsLeadSep(c) : (c == '.' || c == '!');
                    if (!ok)
                        continue;
                }
                std::string rest = s.substr(end);
                TrimLeft(rest);
                s = rest; // empty => the line was only the affirmation
                return true;
            }
            return false;
        };
        return tryList(kStrongAffirm, sizeof(kStrongAffirm) / sizeof(kStrongAffirm[0]), true) ||
               tryList(kWeakAffirm, sizeof(kWeakAffirm) / sizeof(kWeakAffirm[0]), false);
    }
}

std::string PBChatterAmbientPrompt::ScrubReply(std::string reply, std::vector<std::string> const& names)
{
    std::vector<std::string> const sorted = SortedNames(names);
    TrimSpaceLeft(reply);
    TrimRight(reply);
    std::string const original = reply;

    StripLeadingName(reply, sorted);              // a
    if (StripAffirmation(reply))                  // c
        StripLeadingName(reply, sorted);          //   ... then (a) once more ("Facts, Name, ...")
    if (!reply.empty())
        StripTrailingName(reply, sorted);         // b

    // d: whitespace always; leftover leading punctuation only when something was stripped
    // (an untouched "...anyway" keeps its ellipsis). Case is left as the model wrote it.
    TrimSpaceLeft(reply);
    TrimRight(reply);
    if (reply != original)
        TrimLeft(reply);
    return reply;                                 // e: "" when nothing real remained
}
