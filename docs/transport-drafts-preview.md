# Transport and musical drafts — local preview

Version `1.0.0-beta.3-dev`, based on local commit `97ad171`. The user reported the preceding captured-voice looper working on hardware. This change adds the five requested behaviours; the public beta is unchanged.

## Controls

- K1 on the looper selects click OFF / AUTO / ON. AUTO sounds during arming, recording and overdubbing; ON also sounds during playback. REC arms a guide before the first note. Stopped transport is silent. The click is a short decaying sine with a four-beat accent; it consumes no synth voice. It is included in the audio output/USB recording while enabled, but is not recorded as phrase MIDI events.
- ARP opens four musical draft slots. SELECT chooses a slot. SAVE stores the complete phrase, captured voice and tempo; PLAY recalls it. Press the same button again to confirm replacement. PLAY first stops a running transport. Writes require stopped playback and explicitly mute audio. ARP returns to the looper, HOME exits. Unsaved phrases remain RAM-only; saved drafts survive power-off and browser reload while site data is retained.
- REC indicates recording and flashes while armed; PLAY pulses with the beat. The selected menu and sounding/held keys in the current keyboard range light up. Octave buttons indicate the keyboard shift. Dim FX/SAVE/ARP indicate enabled effects, unsaved voice edits and available drafts.
- PRESET changes the live voice without leaving the looper. The screen shows both LIVE and LOOP voice names; the captured phrase voice remains fixed as previously requested.
- OCT−/+ affects new key presses. Each physical key remembers its note-on pitch, so a held note continues and its eventual note-off reaches the correct pitch. No panic, effects reset or transport stop is sent. Polyphonic note-offs release one matching voice in arrival order, so overlapping keys at the same pitch do not cut each other off.

## Execution and numeric review

Audio generation remains CPU1-owned through the existing dry pipeline; mixing/output remain CPU0. The metronome runs in CPU0's output pass with a fixed 16-value float LUT, bounded integer clock and a 221-sample decay. All literals are single precision. At 44.1 kHz the musical phase increments by BPM each frame against 2,646,000 units per quarter note, preserving fractional timing without block-rounding drift. Click starts/restarts are aligned at the existing block command boundary. No heap allocation, logging, I/O, locks or waiting were added to the audio path. Beat counters are read under the existing main/IRQ exclusion. Click level/headroom and worst added callback cost still require hardware measurement.

CPU1 handles draft/menu gestures and immutable snapshots/replies. CPU0 main performs LED publication through plat_lights after a completed frame job. It captures active-note/transport data under interrupt exclusion, then calls the platform light API outside the audio callback. No register, timer, clock, pin or worker protocol changes were made.

Flash operations retain full worker retirement. Under the ownership lock, main disables the dry pipeline, panics/silences playback and publishes a maintenance flag. During the operation the callback only writes a silent output buffer and returns, touching no mutable phrase data. Main streams validation/load/save through the existing shared I/O scratch buffer, restores the pipeline under interrupt exclusion, then clears maintenance. Main never edits a renderer-owned voice or active phrase. No MIDI/GUI job executes concurrently with this main-loop transaction.

## Draft storage and compatibility

Existing object IDs 0–4 are preserved, including voice banks and panel calibration. New IDs 5–32 describe four drafts. Each draft has a manifest and three chunks in each of two generations: seven BSP A/B objects, fourteen sectors. Slot bases are 0x98000, 0xA6000, 0xB4000 and 0xC2000; the last copy ends at 0xD0000. This is inside the platform's application-data window 0x97000–0xDFFFF, below voice banks D8000–DBFFF and panel/BLE DC000–DFFFF. Settings FC000/FD000 and updater staging E0000–FBFFF are untouched.

A version-1 RDLP payload is 10,432 bytes: a 192-byte explicit header and all four takes' 2048 possible events, packed to five bytes each (tick LE16, type, data1, data2). Header stores track counts, length, tempo, the captured 150-byte patch, initial bend/modulation and reserved zeros. No compiler struct/enum layout is persisted. RDC1 chunk wrappers bind slot, generation, sequence and exact lengths; RDM1 manifest binds sequence, generation and whole-payload hash. The BSP adds its own CRC/A-B records.

Saving writes the inactive generation's three chunks, reads each back, then commits/read-verifies the manifest last. Any failed pre-commit write leaves the prior generation selected. Loading validates exact sizes, versions, indices, generation/sequence, reserved fields, patch semantics, counts, event types/data, chronological ticks, zero padding and whole-payload hash before replacing the current phrase. A second streaming pass restores it without allocating a full-phrase backup. Unexpected read failure during that second pass leaves a safely stopped empty phrase; stored data remains available for retry. No autosave or flash writing during playback is used. Older firmware ignores the new object IDs; rollback does not export drafts or migrate their format.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target/build/ABI | PASS: matched FM-1/AC79 SDK, pi32v2 Clang 4.0.1, existing O2 engine/editor and Os application/platform flags and all integrity/ABI checks retained. See build/transport-target.log and transport-drafts-artifacts.json. |
| Audio hot path/numerics | PASS (source/target audit scope): bounded click clock/LUT/decay, no double, heap, I/O, logging or waiting in the renderer; target reports no soft-double helpers or forbidden multiply pattern. Audible click level and real-time overhead UNVERIFIED on device. |
| Ownership/synchronization | PASS (source/host scope): original worker publication/hash/retirement retained; main-only LEDs/storage; maintenance silence isolates phrase mutation; ownership/queue/fallback regressions pass. |
| Memory/storage/I/O | PASS (static/host scope): image 214428 bytes, RAM code 22656/32768, data+BSS 91564/98304, pool 333572/344064; existing CPU1 stack reservation 8192. One enlarged 3840-byte shared scratch, no full phrase staging allocation. Explicit maps, two generations, fail-boundary tests and read-back verification. Peak stacks/physical flash interruption UNVERIFIED. |
| Dependencies/includes/licenses | PASS: only existing platform APIs and authored codec/click data; no new package, driver, task, timer, SDK dependency or redistributed manual asset. Existing notices retained. |
| Compatibility/package/recovery | PASS (source/host/package scope): existing IDs/voice payloads/product/loader preserved, draft v1 is explicit, installer hashes and corruption rejection checked. Device persistence/recovery test remains UNVERIFIED for these draft objects. |
| Host functional validation | PASS: full tools/test-app.sh plus final transport controls; tools/test-drafts.sh with ASAN/UBSAN covers maximum capacity, all slots, all four failed-save boundaries, malformed lengths/indices/patches/events/generations/hash and validation failure preservation. Includes the existing 1188 exact normal/dry scenarios. Browser tests cover audible click, LEDs, saved draft handoff, reload and audible recall. |
| Hardware real-time validation | UNVERIFIED for these additions. The preceding firmware's successful user test is retained as baseline evidence, not a measurement of the new click/LED/storage paths. |

Evidence logs: build/transport-host-final.log, transport-controls-final.log, transport-emu.log, transport-target.log and transport-web.log. Target-size/helper checks and installer package tests passed. Existing constructor conversion/unused-field and firmware-hub metadata warnings remain; no new target warning was introduced. Unrelated full BSP board suite, physical power interruption, peak stack probing, USB soak and device timing were not run; focused LCD/cooperative-wait regressions pass. The emulator still has its existing plain-text screen-reader fallback because voice assets are absent.

## Hardware check

1. While armed, hear a steady click; test OFF/AUTO/ON, 30/120/300 BPM and accent/level. Check speaker/headphones and USB recording. Confirm no metronome voice is counted or stolen.
2. Record/play/overdub, turn PRESET without leaving the looper and confirm the LOOP voice stays fixed. Shift octaves while holding notes, release them, then play new keys at the shifted pitch. The phrase and FX must continue.
3. Check REC/PLAY pulses, active-page, key, octave, dirty SAVE, FX and draft LEDs. Confirm colours/brightness are readable on the physical panel.
4. Save/recall each draft slot, confirm replacement, power-cycle and recall. Check tempo, all takes and captured voice. Existing saved voices must remain intact. Do not deliberately cut power during flash writes on the sole instrument.
5. Measure APP/WORK/TURN/Q and XRUN/LATE/LCD counters with seven voices, heavy intended FX/modulation, click on and active UI/MIDI/USB recording for at least five minutes. Record automatic voice shedding as well as underruns.
