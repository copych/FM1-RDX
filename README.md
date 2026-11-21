# FM1-RDX

A four-operator FM synthesizer for the M-VAVE FM-1, inspired by the Yamaha reface DX. Explore 33 sounds, shape each operator and envelope, and add two effects.

**[Play in your browser](https://copych.github.io/FM1-RDX/emu/)** · **[Install on your FM-1](https://copych.github.io/FM1-RDX/install/)** · **[Project home](https://copych.github.io/FM1-RDX/)**

No hardware or download is needed to try the browser version.

The sounds are arranged as **four banks of eight**, starting with **1–1 DigiChord**. **Init Voice** is separate, before Bank 1 on the PRESET knob. MIDI programs 1–32 select the bank sounds; program 33 selects Init (raw program bytes 0–31 and 32).

## Start playing

1. Open the [browser synth](https://copych.github.io/FM1-RDX/emu/) and click **Switch on**.
2. Click the piano keys, or focus the panel and play with your computer keyboard: **Q–I / A–K** for white keys and **1–0 / −** for black keys.
3. Turn **PRESET** to try another sound. Drag a knob up or down, scroll over it, or focus it and use the arrow keys.
4. Use **MASTER** for volume and **OCT− / OCT+** to change octave.

The large screen beside or below the instrument mirrors its display so you can read the editor comfortably. On a phone, landscape orientation gives the controls more room. If you hear nothing, check **Mute**, MASTER and your device volume, then click Switch on again if your browser paused audio.

## Shape the sound

| Control | What it does |
| --- | --- |
| **EDIT** | Browse levels, feedback, frequency, algorithm and operator settings |
| **ENV** | Edit amplitude and pitch envelope rates and levels |
| **LFO** | Edit modulation waveform, speed and depth |
| **FX** | Open the first or second effect slot |
| **K1–K4** | Change the parameters shown on the screen |
| **SEL** | Select the next operator |
| **Hold SEL + turn frequency** | Adjust fine tuning |
| **SELECT** | Browse every editor page |
| **ALGORITHM** | Change how the four operators connect |
| **GLO / REC / HOME** | Voice settings / Phaser Lab / home screen |
| **Hold SEL + press FX** | Switch effects off or on |

Choose from distortion, touch wah, chorus, flanger, phaser, delay and reverb. There are 12 FM algorithms and four voices of polyphony. The supplied presets include basses, keys, bells, pads, leads and percussion.

**Edits are temporary.** SAVE does not store a patch yet. Loading another preset or restarting discards your edits. ARP, SEQ and PLAY are not connected to a sequencer in this preview.

## Install on an FM-1

Open the [web installer](https://copych.github.io/FM1-RDX/install/) in desktop **Chrome or Edge**, connect your **M-VAVE FM-1** with a USB data cable, and follow the page. Allow MIDI access when asked. Keep the cable connected until installation finishes.

This is an **experimental preview**. The first hardware test confirmed boot, playback and USB MIDI; the latest fixes still need a hardware retest. Back up anything you want to keep using your current firmware's tools before replacing it. The installer includes a way to return to M-VAVE's stock firmware. It does not offer a bootloader replacement.

**Close the installer tab before opening your DAW or MIDI app.** It can hold the MIDI port and prevent another app from opening it. If you already see an error, close the installer, reconnect the FM-1 and reopen the MIDI device in your app.

On hardware, MIDI channel 1 accepts notes, sustain and program changes for the 33 presets. Built-in keys currently use a fixed velocity. USB audio, pitch bend/mod wheel input, live SysEx and permanent patch saving are not available yet. Some edited effect combinations exceed the available memory; the synth rejects those combinations and displays a memory-limit message.

## Feedback

Found a problem? [Open an issue](https://github.com/copych/FM1-RDX/issues) with the preset name, what you changed, and whether you were using the browser or an FM-1. For browser problems, **Copy session** can help describe what you played. Hardware feedback is especially useful while the preview is being tested.

Based on the RDX ESP32 project, [fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp). Independent community firmware; not an official Yamaha or M-VAVE product.

Build instructions and technical validation are kept in [the development notes](docs/BUILD.md).
