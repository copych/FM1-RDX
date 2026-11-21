# GitHub Pages preview — 2026-11-21

The initial-site milestone below is superseded by [preview 2026.11.21-2 and first hardware feedback](HARDWARE-FEEDBACK.md). Current version and artifact pins are in site/preview.json; the first hardware smoke test passed, with detailed real-time validation still pending.

Public site: https://copych.github.io/FM1-RDX/ with `emu/` and `install/`. GitHub's current Pages domain is `github.io`; the requested `copych.github.com` spelling is not configured as a custom domain. The existing account home site is not modified.

The README is written for people trying the instrument. The former technical README is preserved in BUILD.md; engineering evidence remains in the other development documents.

`node tools/build-site.mjs` assembles `build/site` from the current locally built firmware and WebAssembly. It requires exact SHA-256 pins in `site/preview.json`, validates the package identity/UFW checksums/update loader, and compares current app/DSP files with `build/evidence.json`. This is the existing experimental development build, explicitly labelled Preview 2026.11.21, not an upstream release build. The platform's release metadata gate remains unchanged. Product identity FM-1_9900 remains provisional; reserve a unique identity and settle original-source licensing before a formal release.

The installer is an adapted copy of the pinned fm1-base MPL template, with its original port discovery, updater, package verification, backup confirmation, stock restore and reporting paths retained. An additional SHA-256 check pins the preview download before Install is enabled. Loading the page never requests MIDI or sends device commands. The bootloader offer is disabled and no bootloader package is published. Physical installation and recovery are not exercised here.

Generated static files are committed separately to `gh-pages`, published from its root. `.nojekyll` preserves the runtime files. `manifest.json` records every published asset's size/hash. App binaries, toolchain and SDK remain outside `main`. Apache/BSD/MPL and Barlow font licence texts accompany the download; available source notices are retained. No blanket licence is asserted for the original RDX/voices.

After a tested firmware update: build and validate the device and emulator artifacts, regenerate `build/evidence.json`, review/update preview pins/version, run the site assembler, run `node tests/site-test.mjs` and `node tests/emu-test.mjs build/site/emu`, then commit source to `main` and generated assets to `gh-pages`. The site is deployed as a whole so page metadata and its pinned package remain matched. Firmware and browser builds must be reviewed together.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS for reuse of exact previously built/checked artifacts; no firmware source, flags, platform or SDK changes. Device ABI UNVERIFIED. |
| Audio hot path/numerics | PASS for unchanged source scope and copied WebAssembly regression; site work introduces no firmware operations. Device timing UNVERIFIED. |
| Ownership/synchronization | PASS for unchanged firmware model; static page load does not request MIDI or start writes. Installer retains its explicit user confirmation. |
| Memory/storage/I/O | PASS statically: unchanged device memory; package/runtime hashes and paths validated. Flash writes are available only through an explicit installation, not performed here. Device stack/I/O contention UNVERIFIED. |
| Dependencies/includes/licenses | PASS for retained pinned installer/library checks and available notices; no runtime library added. Original RDX/voice licensing remains UNVERIFIED. |
| Compatibility/package/recovery | PASS for identity, SHA-256, UFW and loader checks, wrong-file rejection, retained stock-restore code and installer regressions. Real install/restore/recovery UNVERIFIED. |
| Host functional validation | PASS: site links/assets/hashes, valid/corrupt packages, unsupported browser, no page-load MIDI, no bootloader offer; all 33 voices and wet/dry in published runtime. Upstream stock checksum, handshake and family-port tests pass. Installer confirmation visually checked without proceeding. |
| Hardware real-time validation | UNVERIFIED: user will test physical FM-1; no flash, analog output, real-time headroom, underrun, stack, restore or soak measurement performed by this change. |

Skipped: repeat target build and DSP tests because firmware/runtime bytes are unchanged; physical install/recovery and timing; exhaustive browser/device combinations. Before hardware qualification use the load/soak plan in ORIGINAL-GUI.md and PORT.md. Successful browser or installer simulation does not establish physical device safety or sound equivalence.
