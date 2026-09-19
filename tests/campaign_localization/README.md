# Campaign display localization tests

Standalone CMake/CTest target; no SDL, game installation, game assets, account,
network access or external test framework is required.

From the repository root, using a C compiler environment (for example, an MSVC x64
developer prompt):

```sh
cmake -S tests/campaign_localization -B build-campaign-localization -DCMAKE_BUILD_TYPE=Debug
cmake --build build-campaign-localization --config Debug
ctest --test-dir build-campaign-localization -C Debug --output-on-failure -V
```

The test creates synthetic fixtures in a new subdirectory of its build working
directory on every invocation, so reruns cannot reuse a previous overlay. It does not
read or modify user campaign data, profiles or saves. Repeat with Release for
optimized coverage. Run out of tree to keep generated fixtures out of the source.

Production code linked directly:

- campaign lifecycle, missions, canonical XML and metadata XML;
- locale resolution and actual campaign folder/ZIP loading;
- UTF-8/internal encoding conversion, including French, Portuguese, Russian,
  Greek and Japanese examples.

Adapters isolate language configuration/detection, filesystem roots, progression
persistence, emperor rank, original campaign setup and scenario execution.
The original campaign test verifies fallback/lifecycle with a stubbed original
setup, not the shipped original missions. Canonical getters used by save logic
are checked for unchanged strings, identity and paths; actual save serialization
and playback are not linked into this test.

Covered behavior includes missing/partial/empty overlays, invalid XML/version,
duplicates, ambiguity, unknown keys, aliases/defaults, same-file reload, locale
switching, clear/suspend/restore (including after switching campaigns), reordered
missions, changed first scenario, `.mapx`/`.svx` stems and long text. Campaign-name
previews cover the same locale and malformed/duplicate/ambiguous metadata cases,
and verify that reading multiple folder/ZIP campaigns leaves the active campaign,
mission pointers, translated fields, rank, progression calls and open archive
unchanged. Name previews are also checked before any campaign is active.

Expected result: `0 failures`. Each case runs with an
unpacked directory and a real ZIP `.campaign` file.

This does not validate UI rendering, font glyphs, audio output, physical drivers,
full gameplay or the user-facing translation quality. The shared XML parser's
existing entity/text semantics are unchanged.
