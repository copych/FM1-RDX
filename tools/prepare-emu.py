from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = root / 'platform/fm1-base/web/emu/index.html'
page = source.read_text()
# Keep the original MPL notice on this adapted page.
page = page.replace('<title>{{NAME}} in the browser</title>', '<title>FM1-RDX Emulator</title>')
page = '\n'.join(line for line in page.splitlines() if not ('<link' in line and ('fonts.googleapis.com' in line or 'fonts.gstatic.com' in line)))
page = page.replace('<h1>{{NAME}}</h1>', '<h1>FM1-RDX Emulator</h1><p>Up to 8 adaptive voices · 33 factory/init voices + 32 user slots · source effects</p>')
# Keep the physical panel while providing a large, crisp LCD mirror.
page = page.replace('</style>', '''
.emulator-layout { display:grid; grid-template-columns:minmax(560px,1fr) 320px; gap:24px; align-items:start; }
main, body > header { max-width:1280px; }
.large-display { background:var(--card); padding:16px; border:1px solid var(--rule); border-radius:16px; }
.large-display h2 { margin:0 0 12px; font-size:22px; }
.large-display canvas { display:block; width:100%; height:auto; aspect-ratio:1; background:#000; image-rendering:pixelated; border-radius:8px; }
.large-display p { font-size:16px; margin:12px 0 0; }
@media(max-width:980px) { .emulator-layout { display:flex; flex-direction:column; } .devicebox { width:100%; } .large-display { width:min(480px,100%); box-sizing:border-box; align-self:center; } }
</style>''',1)
page = page.replace('<div class="devicebox">','<div class="emulator-layout">\n<div class="devicebox">',1)
page = page.replace('</div>\n</div>\n<p id="panel-keys"', '''</div>
</div>
<aside class="large-display" aria-label="Large instrument display">
  <h2>Instrument screen</h2>
  <canvas id="large-screen" width="480" height="480" role="img" aria-label="Enlarged live FM1-RDX screen"></canvas>
  <p>Live display at a readable size. The same larger text is used on the FM-1.</p>
</aside>
</div>
<p id="panel-keys"''',1)
page = page.replace('ctx.drawImage(lcd, 0, 0, screenEl.width, screenEl.height);', '''ctx.drawImage(lcd, 0, 0, screenEl.width, screenEl.height);
  const large = document.getElementById("large-screen");
  if (large) {
    const zoom = large.getContext("2d");
    zoom.imageSmoothingEnabled = false;
    zoom.drawImage(lcd, 0, 0, large.width, large.height);
  }''',1)
page = page.replace('<div class="bar">', '''<aside class="help" style="margin:16px 0">
  <p><strong>Switch on</strong>, then click the piano keys or focus the panel and play
  <kbd>Q</kbd>–<kbd>I</kbd> / <kbd>A</kbd>–<kbd>K</kbd> for white keys;
  <kbd>1</kbd>–<kbd>0</kbd> and <kbd>-</kbd> for black keys.</p>
  <p>Turn <strong>PRESET</strong> to browse the bank. Drag a knob up/down, use its mouse wheel,
  or focus it and use arrow keys. <strong>MASTER</strong> sets volume;
  <strong>OCT− / OCT+</strong> transpose. <strong>SELECT</strong> cycles operators or related pages in the current section;
  <strong>K1–K4</strong> edit the displayed parameters. Tap <strong>SEL</strong> to select an operator;
  hold it while turning a frequency knob for fine tuning.</p>
  <p>From <strong>Home</strong>, turning <strong>K1–K4</strong> opens the operator Level screen and changes that operator's level immediately.</p>
  <p>Turn parameter knobs slowly for small steps or quickly for accelerated movement. Hold <strong>SEL</strong> for precise steps. Presets, pages, switches and type selectors remain stepwise.</p>
  <p><strong>EDIT</strong>, <strong>ENV</strong>, <strong>LFO</strong> and <strong>FX</strong> cycle their pages.
  <strong>GLO</strong> cycles voice and MIDI settings. On MIDI, K1 selects the receive channel and K2 enables FM control CCs.
  <strong>ENV</strong> also reaches per-operator pitch-EG switches. <strong>SEL + GLO</strong> opens Phaser Lab.
  <strong>SEL + FX</strong> toggles the effects chain. <strong>HOME</strong> returns home.
  <strong>SAVE</strong> opens Store: SELECT chooses a user slot, K1 selects a name character and K2 changes it.
  SAVE stores; occupied slots ask for confirmation. HOME cancels. Saving briefly mutes audio; stop the looper first.</p>
  <p><strong>SEQ</strong> opens the phrase looper. <strong>REC</strong> arms first-note recording;
  REC again finishes and loops the phrase. <strong>PLAY</strong> stops or restarts.
  REC during playback arms one overdub pass at the next loop; K4 adjusts tempo.
  <strong>SEL + SAVE</strong> clears a stopped phrase. Four takes of up to 512 MIDI events each;
  phrases are lost when switched off. HOME returns to editing while playback continues.</p>
  <p>This tests the current prototype's app and synth code in your browser.
  The original ESP screen drawings are adapted to the FM-1 display and controls.
  Browser performance does not measure FM-1 timing.</p>
</aside>
<div class="bar">''', 1)
target = root / 'app/web/emu/index.html'
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(page, encoding='utf-8')
print('Prepared local emulator page; upstream license notice retained.')
