# Handoff 2026-11-01: crash after the boot logo above 720p

Issue: pacific-city-recompiled/crackdown-2-recomp#5. CD2 1.0.0 crashes right after the Xbox 360 logo when the
launcher's Resolution sets `resolution_scale=2` or `3`. 720p is fine. CD1 is not affected.

## Cause

The log ends with `D3D12 device removed: HRESULT 0x887A0001 - INVALID_CALL` about 1-2 s after boot.

The bug is in the SDK, D3D12 only: `src/graphics/d3d12/texture_cache.cpp`,
`D3D12TextureCache::LoadTextureDataFromResidentMemoryImpl`, the final CopyTextureRegion loop.
- A scaled-resolve texture is created at guest size * scale, so its host mips are `max((size * scale) >> level, 1)`.
- The copy buffer footprint is laid out as `max(size >> level, 1) * scale`.
- Levels below the packed mip tail were copied with no source box.

CD2 resolves a mipmapped 256x1 RGBA8 texture. At 2x its mip 1 is 256x1 on the host but 256x2 in the footprint, so
the copy runs out of bounds. The command list then fails to close and ExecuteCommandLists removes the device. Vulkan
already clamps this copy (`imageExtent`).

## Fix

SDK patch `patches/sdk/0032-d3d12-scaled-texture-mip-copy-box.patch`. For scaled textures, levels below the packed
tail get a source box: the overlap of the host mip and the footprint, aligned to the host block size. The footprint
is the smaller of the two for some non-power-of-two sizes. The patch reverse-applies on its own, and no other patch
touches the file. 0011/0017 still fail the stack check, as they already did.

## Evidence (RTX 5080, release exe, windowed, isolated user data)

- Before: 2x crashed about 1 s after boot (`crackdown2_110.log`). With `--d3d12_async_submission=false` it still
  crashed, so patch 0021 is not involved. Under cdb with `--d3d12_debug=true`: 44x
  `COPYTEXTUREREGION_DSTREGIONOUTOFBOUNDS`, then `EXECUTECOMMANDLISTS_FAILEDCOMMANDLIST` and device removal
  (`crackdown2_112.log`).
- After: 1x, 2x and 3x each ran 100 s through the menus into the New Game training area, rendering correctly
  (`crackdown2_114`-`116.log`). 2x under cdb + debug layer for 120 s: 0 D3D12 errors, 0 out-of-bounds copies.
- The v1.0.0 Quiver ZIP (clean extract, 720p) launches fine. There is no second crash.
- Not tested: the open city at 2x/3x (only the training area).

## Review

Codex (`-s read-only`). It caught that the first version of the box could exceed the footprint for NPOT sizes (fixed
by the clamp).

## Follow-ups

- Codex also flagged a pre-existing issue that this patch leaves alone. The scaled load of block-compressed textures
  dispatches `ceil(h/4) * scale` block rows but allocates `ceil(h * scale / 4)`, so it may write past its slice when
  the height isn't a multiple of 4 (same for X). Check it with the debug layer / GPU-based validation.
- Next: release a build with the new `rexgpu-xenos.dll` (Austin runs publish). Until then, the workaround for users
  is Resolution 720p.
