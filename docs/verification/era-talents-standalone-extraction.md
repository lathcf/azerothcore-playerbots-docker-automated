# Verification — mod-era-talents standalone extraction (2026-09-07)

Spec `docs/superpowers/specs/2026-09-07-era-talents-standalone-repo-design.md`, plan
`docs/superpowers/plans/2026-09-07-era-talents-standalone-repo.md`. New repo
`/home/lewist/Projects/mod-era-talents` HEAD `e10f859`; overlay migration commit `2cdfaab`; pin `mod-era-talents e10f859`.

| Check | Result |
|-------|--------|
| Generation reproducibility (`tools/era-regen.sh` in the new repo) | 97 SQL files, allowlist header, 18 addon Lua byte-identical; stamp `b35a304d → c8293d48` (generator sources are hashed by design; two of them changed for the path relocation) |
| Tests / audit | 249 passed (`python -m pytest tools`); `era_audit.py` 0 findings (with and without `AC_ROOT`) |
| Regen scripts round-trip (fork at pin, overlay extras passed) | 10/10 regenerate; 6 byte-identical, 4 differed only in hunk offsets/blob ids (pre-existing staleness) — refreshed in `0b36900` |
| `apply-patches.sh` scratch worktree at pins | full stack 10 applied / 10 already-applied; production order after overlay 0001–0017: 10 applied; core+IP only: 7 applied + 2 skipped |
| Stock core `c80c4b9` + IP `977e200`, no playerbots | 7/7 core+IP patches apply; cmake `mod-playerbots not found — bot support compiled out`; 16/16 module TUs compile (docker deps image) |
| Client patch | `tools/fetch-base-dbc.sh` reproduces the base DBCs (sha match); `client-patch/build-client-patch.sh --from-ip` → `out/patch-V.mpq` carrying `EraTalents Gen c8293d48` |
| Dev-box full build (`./setup.sh` after `2cdfaab`) | pin applied, overlay patches then `==> mod-era-talents patches` 10 applied, cmake `mod-playerbots found — bot era talents compiled in`, build exit 0, stack up |
| DB / startup | `era_talent_meta` = `c8293d48` after `ac-db-import`; worldserver `loaded 1011 talent nodes (generation c8293d48)`, `band classifier: 2580 node-grant ids indexed, 86 allowlist entries`, trainer level map present; no "built without mod-playerbots" warning |
| `.eratalents doctor Kreethkick` (rogue bot L70, TBC band) | era=TBC spent=61, 0 ORPHAN/PROBLEM/UNCLASSIFIED, `AI resolve 13964 → 939484` |
| `.character level Reluu 65` (shaman bot 40→65) then `doctor` | era=TBC spent=56; Stoneskin 8071→947026, Searing 3599→3599 (TBC keeps stock), Healing Stream 5394→947174 — all three quest-taught totems present after the era arm (the grant log line is DEBUG for bots since `ef01ba8`, so Server.log shows none) |
| mod-raid-roster / mod-arena-roster | compiled in the same build against the clone's `EraTalentBots.h` |

Not done here: link-level check of a no-playerbots build (compile-only), real-client session, prod deploy.
The owner still has to create the two remotes and push the new repo before `setup.sh` can clone it on another host.
