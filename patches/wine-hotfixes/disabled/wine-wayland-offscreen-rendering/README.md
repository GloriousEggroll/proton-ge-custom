These wine-wayland patches are intentionally not part of the active series.

They implement the basic offscreen and CPU-readback cross-process rendering
path added to the wine-wayland base. That path overlaps with the later
`wineland-child-rendering` dma-buf implementation used by this project. Applying
both changes the same client-surface lifecycle and produces conflicting or
duplicated rendering behavior.

Keep these patches for upstream provenance, but do not apply them together with
`patches/wine-hotfixes/wineland-child-rendering/`.
