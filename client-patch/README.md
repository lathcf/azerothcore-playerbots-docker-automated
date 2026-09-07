# client-patch — era-talents client display patch

A **small, additive** client-side `Spell.dbc` patch (MPQ) that lets the 3.3.5a client render the
handful of **custom visible auras** `mod-era-talents` applies which the stock client doesn't know
(e.g. the Improved Blizzard **Chilled** debuff). Everything else about a talent works server-side;
this is *only* so the icon + name show on the unit frame. It does **not** recreate any base player
spell — that was the shelved era-wow approach. Only dataset entries marked `client: {name, icon}`
are included.

> This is a deliberate, user-approved exception to the project's "no client patch" rule, scoped to
> custom **displayable** debuffs/buffs. Passive talent auras stay hidden; talents that trigger a
> **stock** spell (Ignite, the Counterspell silence, Fire Vulnerability, Winter's Chill, …) already
> render with no patch.

## Files

| File | What |
|------|------|
| `../tools/build_client_dbc.py` | Appends `client:`-marked helper rows to a base `Spell.dbc`, built from `gen_era_talents`' SAME field logic (so client & server rows match — a divergence rubber-bands the client). Replaces a same-id row if present (idempotent). |
| `../tools/spell_dbc_coltypes.txt` | Per-column types (string/signed/unsigned/float) for the DBC binary encode. |
| `mpqpack` / `mpqpack.c` | MPQ packer (StormLib, statically linked). Binary is gitignored. |
| `mpqread` / `mpqread.c` | Extract one file from an MPQ. Binary is gitignored. |
| `build-mpqpack.sh` | Builds BOTH binaries above. Self-bootstrapping: auto-clones + builds `deps/StormLib` (gitignored) if absent. No sudo needed beyond a basic toolchain (`git cmake gcc g++ make` + zlib/bz2 dev headers — the docker-wow LXC ships none of the compilers, so `apt install` them there first). |
| `build-client-patch.sh` | Build a **standalone** `out/patch-4.MPQ` (base client `Spell.dbc` + our rows). |
| `merge-into-patch.sh` | Merge our rows **into an existing** patch MPQ (see gotcha below). |

## The load-order gotcha (why `merge-into-patch.sh` exists)

WoW loads `patch`, `patch-2`…`patch-9`, then `patch-A`…`patch-Z`; **later archives win**. The IP mod
ships `patch-V.mpq` **with its own full `Spell.dbc`**, so a standalone `patch-4.MPQ` from us is
silently overridden and our rows vanish. The fix is to **merge our rows into `patch-V.mpq`** so both
coexist:

```bash
client-patch/merge-into-patch.sh client-addons/_data-patches/patch-V.mpq
# -> client-patch/out/patch-V.mpq   (IP's Spell.dbc + our custom rows; all IP files preserved)
```

This is **wired into `fetch-client-addons.sh`** (right after it stages IP's `patch-V.mpq`), so the
normal client-staging run produces the merged patch automatically. It degrades gracefully (a NOTE)
if `mpqpack`/`mpqread` aren't built. Re-running is idempotent.

## Install / verify (players)

Drop the merged `patch-V.mpq` into `World of Warcraft/Data/`, then **fully restart WoW** (MPQs load
only at launch — a `/reload` or relog won't pick up a new patch). Verify a row loaded in-game:

```
/run print(GetSpellInfo(932980))   -- prints "Chilled" if the patch is loaded, else nil
```

## Adding a class

Add the class's dataset yaml to the `DATASETS=( … )` list in `build-client-patch.sh` **and**
`merge-into-patch.sh` (and the wired copy in `fetch-client-addons.sh`), mark any custom visible
aura with a `client: {name, icon}` block, and rebuild.
