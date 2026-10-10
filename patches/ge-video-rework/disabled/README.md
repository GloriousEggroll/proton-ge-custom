# Disabled video patches

## Upstream WoW64 fix (2026-10-10)

`0070-winedmo-use-correct-parameter-structure-in-wow64-demuxer-destroy.patch`
is now included in Wine as `c8df1a2d652`. Original author: Nikolay Sivov;
original commit: `2110c64d89ccf90e7865031238456b5f20ed1066`.

Keep the original patch here for provenance. Do not apply it again: the GE
backend conversion preserves upstream's `demuxer_destroy_params` declaration.
The other disabled patches predate this refresh and were not changed.
