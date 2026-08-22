# Wine-Wineland child-process rendering

This directory contains a selective rebase of Wine-Wineland's cross-process
DMA-BUF rendering work onto the Wine base used by GE-Proton.

Upstream branch:
https://github.com/nanomatters/wine-wineland/commits/wineland_20260713-reorg/

Original author: Erhan Bilgili <erhan.bilgili@gmail.com>

The original author and dates are preserved in every patch's mail header. The
patches were rebased only where the GE-Proton Wine base had diverged. The first
subject was narrowed to describe the imported rendering portion of an upstream
commit that also bundled SNI work.

GE-Proton's existing SNI implementation is applied before this series. The
older bundled SNI implementation and Wine-Wineland's native Steam-overlay work
are not imported. References to child "overlays" inside this series mean GDI
child-window composition, not the Steam overlay. Direct-toplevel presentation
and other unrelated Wine-Wineland work are also intentionally excluded.

The 58 patches selectively cover the source commits below. Most
pending-producer changes from `64f5e7717f44` are folded into the following
expose-order patch after rebasing; patch 0052 restores its managed Vulkan
producer lifecycle, which must bracket image allocation and channel teardown.
Patch 0053 adds consumer-state tracking and nonblocking managed dma-buf
publishing from `129fa64fc449` without its unrelated minimized-window changes.
Patch 0054 is a GE-Proton follow-up that retires an OpenGL frame callback when
its client surface is detached or reattached, preventing pre-attachment CEF
presents from permanently throttling the attached surface.
Patch 0055 adapts the OpenGL presentation-suspension behavior from
`aa3013f4bfcb` by deferring ordinary EGL swaps until their client surface is
attached to a live toplevel; roleless HWND dma-buf producers remain active.
Patch 0056 enables Wine's manual frame-callback throttle only when disabling
EGL's native swap throttling succeeds, and reports actual EGL swap failures.
Patch 0057 recreates an EGL window surface after its client wl_surface changes
between roleless and subsurface presentation, recovering drivers that return
`EGL_BAD_SURFACE` after that transition.
Patch 0058 mirrors Wine's X11 EGL fallback-context handling for applications
that release their WGL context before calling `SwapBuffers`, preventing Mesa
from rejecting Chromium child-window presents with `EGL_BAD_CONTEXT` and
`EGL_BAD_SURFACE`. It is based on Wine commit `de44bd7234b2c` by Paul Gofman.
`a42482e82bcb` is omitted because its deadlock fix is already present in the
rebased implementation. The portions of `a022197bcbd4` that overlap
GE-Proton's separately applied NVIDIA Reflex patch are restored by patch 0050,
and patch 0051 imports the required present-wait dispatch follow-up by Etaash
Mathamsetty.

Original commits, in patch order:

```
78188ad14a63 64f5e7717f44 07fc8d5f15a6 9b6bebd0bbe4
4118360670df 52e7bad2e79e f8714d9001cb ea810ddd9d2c
10160c027097 a055ae2a97d2 bc6dcef4e6e0 ca982936c16c
02c4eff60c0e 8ecb567773e1 4d7ea64b5fb6 1d07ecb63c35
6f5dd4672d62 e593949be430 94731b30007f 064e4dfbb645
faca9084a3ba 98287ff6478b 822480e674f2 11f3f4b345e8
4e6a0a51523d 03f22c74dff4 a3e353247a52 908f7702fbf6
e6273c7edfda 0e6b602c6bb5 149f82607b85 aa37ca012187
735a85878f6d 25fe266deea7 a022197bcbd4 4305c5705863
27a744ebe76d 7dd872c007f4 f70d57bb55d3 acdab18126f5
8c2bddc055af 5ee287fae22a 3cb2dc25ff16 23eeb25347a5 c6bae4688911
3fb774e4b7a4 0846204b119a 9e6c1976ee15 85868e53809b
a42482e82bcb ca39b2cde0a5 1c4e67d85d8f 129fa64fc449
```
