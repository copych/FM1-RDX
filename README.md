# FM1-RDX

A four-operator FM synthesizer for the M-VAVE FM-1, inspired by the Yamaha reface DX. Explore 33 sounds, shape each operator and envelope, and add two effects.

**[Play in your browser](https://copych.github.io/FM1-RDX/emu/)** · **[Install on your FM-1](https://copych.github.io/FM1-RDX/install/)** · **[Project home](https://copych.github.io/FM1-RDX/)**

No hardware or download is needed to try the browser version.

**Latest preview:** corrected LFO fade-in removes reproduced steps in isolated WobbleBass operator 1. USB recording is available at 44.1 kHz stereo/16-bit PCM. Polyphony remains four voices; adaptive budgeting is pending. Hold SEL and press REC for Audio Timing; SAVE resets measurements.

The sounds are arranged as **four banks of eight**, starting with **1–1 DigiChord**. **Init Voice** is separate, before Bank 1 on the PRESET knob. MIDI programs 1–32 select the bank sounds; program 33 selects Init (raw program bytes 0–31 and 32).

## Start playing

1. Open the [browser synth](https://copych.github.io/FM1-RDX/emu/) and click **Switch on**.
2. Click the piano keys, or focus the panel and play with your computer keyboard: **Q–I / A–K** for white keys and **1–0 / −** for black keys.
3. Turn **PRESET** to try another sound. Drag a knob up or down, scroll over it, or focus it and use the arrow keys.
4. Use **MASTER** for volume and **OCT− / OCT+** to change octave.

From Home, turning **K1–K4** opens the operator Level screen and changes that operator's level immediately. Inside an editor page, the knobs edit the parameters shown on that page.

Turn slowly for one-unit steps, or keep turning quickly to cover the range faster (up to 8×). Reverse direction or pause to return to small steps. **Hold SEL** to keep precise steps; on frequency controls it also selects fine tuning. Presets, pages, switches and type selectors remain stepwise.

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
| **SELECT** | Browse every editor page |
| **ALGORITHM** | Change how the four operators connect |
| **GLO / REC / HOME** | Voice/MIDI settings / Phaser Lab / home screen |
| **Hold SEL + press FX** | Switch effects off or on |

Choose from distortion, touch wah, chorus, flanger, phaser, delay and reverb. There are 12 FM algorithms and four voices of polyphony. The supplied presets include basses, keys, bells, pads, leads and percussion.

**Edits are temporary.** SAVE does not store a patch yet. Loading another preset or restarting discards your edits. ARP, SEQ and PLAY are not connected to a sequencer in this preview.

## Install on an FM-1

Open the [web installer](https://copych.github.io/FM1-RDX/install/) in desktop **Chrome or Edge**, connect your **M-VAVE FM-1** with a USB data cable, and follow the page. Allow MIDI access when asked. Keep the cable connected until installation finishes.

This is an **experimental preview**. The first hardware test confirmed boot, playback and USB MIDI; the latest fixes still need a hardware retest. Back up anything you want to keep using your current firmware's tools before replacing it. The installer includes a way to return to M-VAVE's stock firmware. It does not offer a bootloader replacement.

**Close the installer tab before opening your DAW or MIDI app.** It can hold the MIDI port and prevent another app from opening it. If you already see an error, close the installer, reconnect the FM-1 and reopen the MIDI device in your app.

MIDI receives on channel 1 by default. Press **GLO** twice for MIDI settings: **K1** selects channel 1–16 or ALL; **K2** enables the optional Reface FM control CCs. Pitch bend, modulation (CC1), volume (CC7), expression (CC11) and sustain (CC64) work without enabling FM CC mode. The **BEND** setting on the voice page sets the range from −24 to +24 semitones; a negative range reverses the bend direction.

With FM CC mode on, CC80 changes the algorithm. Operator 1 uses CC85–90; operators 2, 3 and 4 use CC102–107, 108–113 and 114–119. Each group controls level, feedback amount, feedback type, frequency mode, coarse frequency and fine frequency, in that order.

To record over USB, select **FM1-RDX prototype** as your recording input and use **44.1 kHz, stereo, 16-bit PCM**. Save as WAV or FLAC. MASTER adjusts the speaker/headphones without changing the recording level; MIDI volume and expression still affect the recording. USB MIDI remains available alongside recording. Computer audio playback through the FM-1 is not enabled. If the new audio input does not appear after updating, reconnect USB and reopen your recording app.

Built-in keys currently use a fixed velocity. Live SysEx, permanent patch saving and phrase looping are not available yet. Keyboard rate scaling still awaits calibration; its stored KSC-R value does not change envelope rates. Some edited effect combinations exceed the available memory; the synth rejects those combinations and displays a memory-limit message.

## Feedback

Found a problem? [Open an issue](https://github.com/copych/FM1-RDX/issues) with the preset name, what you changed, and whether you were using the browser or an FM-1. For browser problems, **Copy session** can help describe what you played. Hardware feedback is especially useful while the preview is being tested.

Based on the RDX ESP32 project, [fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp). Independent community firmware; not an official Yamaha or M-VAVE product.

Build instructions and technical validation are kept in [the development notes](docs/BUILD.md).
