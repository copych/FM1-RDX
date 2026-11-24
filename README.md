# FM1-RDX

A four-operator FM synthesizer for the M-VAVE FM-1, inspired by the Yamaha reface DX. Explore 33 sounds, shape each operator and envelope, and add two effects.

**[Play in your browser](https://copych.github.io/FM1-RDX/emu/)** · **[Install on your FM-1](https://copych.github.io/FM1-RDX/install/)** · **[Project home](https://copych.github.io/FM1-RDX/)**

No hardware or download is needed to try the browser version.

**v1.0.0-beta:** up to **seven adaptive voices**, available immediately after startup. USB MIDI, Soundmondo support, USB recording and the full editor are included. The firmware is tested regularly on an M-VAVE FM-1; the seven-voice hardware sweep completed with zero XRUNs. Polyphony adapts to the sound, effects and available processing time.

The sounds are arranged as **four banks of eight**, starting with **1–1 DigiChord**. **Init Voice** is separate, before Bank 1 on the PRESET knob. MIDI programs 1–32 select the bank sounds; program 33 selects Init (raw program bytes 0–31 and 32).

## Start playing

1. Open the [browser synth](https://copych.github.io/FM1-RDX/emu/) and click **Switch on**.
2. Click the piano keys, or focus the panel and play with your computer keyboard: **Q–I / A–K** for white keys and **1–0 / −** for black keys.
3. Turn **PRESET** to try another sound. Drag a knob up or down, scroll over it, or focus it and use the arrow keys.
4. Use **MASTER** for volume and **OCT− / OCT+** to change octave.

Turning **ALGORITHM** opens the algorithm screen and changes the operator routing. From Home, turning **K1–K4** opens the operator Level screen and changes that operator's level immediately. Inside an editor page, the knobs edit the parameters shown on that page.

Choosing a preset with PRESET or a MIDI program change returns the display to Home.

Turn slowly for one-unit steps, or keep turning quickly to cover the range faster (up to 8×). Reverse direction or pause to return to small steps. **Hold SEL** to keep precise steps; on frequency controls it also selects fine tuning. Presets, pages, switches and type selectors remain stepwise.

**SELECT** stays within the current section. On operator-detail and AEG screens it cycles OP1–OP4 without changing the page. On LFO, FX, global and shared pitch-envelope screens it cycles the related pages. Use the section buttons to move to another menu.

The large screen beside or below the instrument mirrors its display so you can read the editor comfortably. On a phone, landscape orientation gives the controls more room. If you hear nothing, check **Mute**, MASTER and your device volume, then click Switch on again if your browser paused audio.

## Shape the sound

| Control | What it does |
| --- | --- |
| **EDIT** | Browse levels, feedback, frequency, algorithm and operator settings |
| **ENV** | Edit amplitude/pitch envelopes and pitch-EG on/off per operator |
| **LFO** | Edit modulation waveform, speed and depth |
| **FX** | Open the first or second effect slot |
| **K1–K4** | Change the parameters shown on the screen |
| **SEL** | Select the next operator |
| **Hold SEL + turn frequency** | Adjust fine tuning |
| **SELECT** | Choose an operator on operator/AEG pages; browse related screens in other menus |
| **ALGORITHM** | Change how the four operators connect |
| **GLO / HOME** | Voice/MIDI settings / home screen |
| **SEL + GLO** | Phaser Lab |
| **SAVE** | Store a named voice in a user slot |
| **SEQ / REC / PLAY** | Looper screen / record or overdub / start or stop |
| **Hold SEL + press FX** | Switch effects off or on |

Choose from distortion, touch wah, chorus, flanger, phaser, delay and reverb. There are 12 FM algorithms and up to seven adaptive voices. The supplied presets include basses, keys, bells, pads, leads and percussion.

The instrument display uses the original RDX bitmap font throughout, with a monochrome Reface-style editor, Store and Looper screens.

**Saving, drafts and looping are available in the new local preview; the public installer still serves v1.0.0-beta.**

Press **SAVE** to store a voice in one of **32 user slots**, separate from the factory bank. **SELECT** chooses a destination; **K1** selects one of the ten name characters and **K2** changes it. **K3/K4** choose bank/slot. Press SAVE again to store; an occupied slot asks for a second confirmation. **HOME** cancels. Saving briefly mutes audio, so stop the looper first. Stored voices survive power-off. In the emulator they stay in this browser while its site data is retained.

Press **REC**, then play the first note to begin recording. Press REC again to finish and play the phrase in a loop. **PLAY** stops or restarts from the beginning. During playback, REC arms one overdub pass at the next loop boundary; REC can finish that pass early. **SEQ** opens/closes the looper screen; **K4** changes tempo there. **K1** sets the click to OFF, AUTO (arming/recording/overdub) or ON (also during playback). The click uses no synth voice. Turn **PRESET** to change the live sound while staying on the looper screen; both the live and captured voice names are shown. **SEL + SAVE** clears a stopped phrase. HOME returns to sound editing while playback continues.

The first recorded note captures the current voice, including unsaved edits. That voice is used for the entire phrase and every overdub. Changing presets or editing the sound affects live playing; the loop keeps its captured FM voice. Both feed the current shared effects and output controls. The looper screen shows the captured voice name.

The looper has four takes of up to 512 events each. It records notes, velocity, pitch bend, modulation, volume, expression and sustain. Unsaved phrases disappear at power-off or browser reload. Press **ARP** for four draft slots. Each draft stores the complete phrase, captured voice and tempo. **SELECT** chooses a slot, **SAVE** stores and **PLAY** recalls; press the same button again to confirm replacement. Stop playback before saving or loading. ARP returns to the looper; HOME exits. Saved drafts survive power-off (and stay in browser site storage in the emulator). It does not record audio, patch changes or MIDI clock. Octave buttons shift new keyboard notes without interrupting held notes, effects or the loop. Keys light for held/live or looped notes in the displayed keyboard range. REC indicates recording/arming, PLAY pulses with the beat, the active menu is lit, and dim SAVE/FX/ARP indicate unsaved edits, enabled effects and available drafts.

## Install on an FM-1

Open the [web installer](https://copych.github.io/FM1-RDX/install/) in desktop **Chrome or Edge**, connect your **M-VAVE FM-1** with a USB data cable, and follow the page. Allow MIDI access when asked. Keep the cable connected until installation finishes.

This is **v1.0.0-beta**, tested regularly on M-VAVE FM-1 hardware. Back up anything you want to keep using your current firmware's tools before replacing it. The installer includes a way to return to M-VAVE's stock firmware. It does not offer a bootloader replacement.

**Close the installer tab before opening your DAW or MIDI app.** It can hold the MIDI port and prevent another app from opening it. If you already see an error, close the installer, reconnect the FM-1 and reopen the MIDI device in your app.

MIDI receives on channel 1 by default. Press **GLO** twice for MIDI settings: **K1** selects channel 1–16 or ALL; **K2** enables the optional Reface FM control CCs. Pitch bend, modulation (CC1), volume (CC7), expression (CC11) and sustain (CC64) work without enabling FM CC mode. The **BEND** setting on the voice page sets the range from −24 to +24 semitones; a negative range reverses the bend direction.

With FM CC mode on, CC80 changes the algorithm. Operator 1 uses CC85–90; operators 2, 3 and 4 use CC102–107, 108–113 and 114–119. Each group controls level, feedback amount, feedback type, frequency mode, coarse frequency and fine frequency, in that order.

To record over USB, select **FM1-RDX reface DX** as your recording input and use **44.1 kHz, stereo, 16-bit PCM**. Save as WAV or FLAC. MASTER adjusts the speaker/headphones without changing the recording level; MIDI volume and expression still affect the recording. USB MIDI remains available alongside recording. Computer audio playback through the FM-1 is not enabled. If the new audio input does not appear after updating, reconnect USB and reopen your recording app.

For **Soundmondo**, install the latest preview, reconnect USB, close the installer and open [Soundmondo](https://soundmondo.yamahasynth.com/) in Chrome or Edge. Allow MIDI access including SysEx. The device should appear as a reface DX; try loading a voice and capturing it back. Soundmondo voice exchange has been tested on the FM-1. Downloaded voices replace the current RAM edit buffer; use SAVE to keep a downloaded voice in a user slot. Soundmondo can capture the current voice without writing it to the FM-1's flash.

The browser emulator's **MIDI ports** control can exchange MIDI/SysEx with a selected port. It does not create an operating-system MIDI device for Soundmondo.

Built-in keys currently use a fixed velocity. Keyboard rate scaling still awaits calibration; its stored KSC-R value does not change envelope rates. Some edited or imported effect combinations exceed the available memory; the synth rejects those combinations. SysEx support targets the Reface voice edit buffer, not a full Yamaha system/looper implementation.

## Feedback

Found a problem? [Open an issue](https://github.com/copych/FM1-RDX/issues) with the preset name, what you changed, and whether you were using the browser or an FM-1. For browser problems, **Copy session** can help describe what you played. Hardware feedback helps improve the beta.

Based on the RDX ESP32 project, [fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp). Independent community firmware; not an official Yamaha or M-VAVE product.

Build instructions and technical validation are kept in [the development notes](docs/BUILD.md).
