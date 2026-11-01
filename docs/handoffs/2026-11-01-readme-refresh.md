# Handoff 2026-11-01: README refresh for the controller launcher

Both game READMEs (crackdown-recomp, crackdown-2-recomp) were rewritten around the 1.3.2 / 1.0.1 launcher:
install via Quiver or the release ZIPs, first run, controller shortcuts, settings pages, Archipelago
(archipelago.gg + port, or Custom address), troubleshooting (incl. the CD2 >720p crash fixed in 1.0.1).

## Quiver
Quiver shows the README on the app page (`apps/launcher/src/detail.tsx`: the site's copy, else GitHub's
`/repos/<repo>/readme`). Rendering (`packages/ui/src/markdown.tsx`): always dark theme, rehype-sanitize default
schema plus `align`/`width`/`height`, headings shifted down one level, lone images centred, GitHub alerts work.
Relative URLs resolve against raw.githubusercontent.com only when fetched from GitHub, so every image and link
in the READMEs is absolute (main branch). Images 404 until the PR merges.

## Screenshots
`docs/images/launcher-{home,display,archipelago}.png` are rendered on the Linux server from launcher main
(eff4eaf, 1.3.2) with `--review-ui <dir> 1600 900 --pad-script` (SDL offscreen driver, assets/ copied next to the
exe for fonts and button glyphs), using a stub install (fake one-entry manifest + verified-files stamp) so home
shows "Ready to play" without real game art. A profile "Agent" with Archipelago on fills the AP page.
CD2 hero/tutorial JPEGs come from earlier in-game captures (project files city3_160.png, quiver/cd2-screenshot.jpg).

## Next
- CD1 SETUP.md still uses old launcher tab names (GRAPHICS, SAVES, ARCHIPELAGO tabs); refresh it to the
  Display / Gameplay / Saves & Archipelago / About pages.
- Quiver catalog switch to GitHub (pending separately).
