# Custom message localization tests

Standalone CMake/CTest target using the production message parser, shared locale
resolver, folder/ZIP loader, XML parser and UTF-8/internal encoding conversion.
No game installation, SDL, external test framework or network is required.

From the repository root, in a C compiler environment:

```sh
cmake -S tests/custom_messages_localization -B build-message-localization -DCMAKE_BUILD_TYPE=Debug
cmake --build build-message-localization --config Debug
ctest --test-dir build-message-localization -C Debug --output-on-failure -V
```

Repeat in Release for optimized coverage. Run out of tree: synthetic fixtures are
created in a new subdirectory of the build working directory on each invocation.
The test does not inspect or modify installed campaigns, saves or profiles.

Every case runs against an unpacked directory and a real ZIP `.campaign` file.
Coverage includes valid/partial/missing translations, unknown and duplicate UIDs,
invalid XML/version, `.mapx`/`.svx` scenario names, exact-directory precedence,
locale normalization, aliases/default detection, directories and aliases with
spaces or accents, ambiguous/invalid manifests, language switching and clearing.
Portuguese and French strings use the real encoding converter.

Expected result: `58 cases, 444 checks, 0 failures`.

Adapters supply game state, UID lookup, selected/detected language and filesystem
roots. Fallback is checked as a null result from each localization getter: the
canonical message consumer, scenario execution and save serialization are not
linked. UI rendering, font glyphs and full gameplay are not validated here.
