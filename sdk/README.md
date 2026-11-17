# DeckLink SDK (not committed)

Put the Blackmagic DeckLink SDK 16.0 headers here, as `sdk/include/` (the folder holding `DeckLinkAPI.h`
and `DeckLinkAPIDispatch.cpp`). Everything in `sdk/` except this file is git-ignored: the SDK is never
committed. Brendan keeps the SDK locally; licensing notes and how to unpack an upload safely are in
`carmacoma/decklinkExperiments` (`sdk/README.md`, `docs/LICENSING.md`).

Build with it: `cmake -S engine -B build-dl -DOTC_DECKLINK_SDK=$PWD/sdk/include && cmake --build build-dl`.
