from array import array
from pathlib import Path
import json,math
frames=768*128
names=sorted(p.name for p in Path('RDX_before_UAC/data/patches').glob('*.syx'))
report={}
for mode in ['dry','wet']:
    a=array('f');b=array('f')
    with open(f'build/{mode}-reference.f32','rb') as f:a.frombytes(f.read())
    with open(f'build/{mode}-port.f32','rb') as f:b.frombytes(f.read())
    assert len(a)==len(b)==33*frames*2
    rows=[]
    for p,name in enumerate(names):
        start=p*frames*2;end=start+frames*2
        ref=a[start:end];port=b[start:end]
        assert all(math.isfinite(v) for v in ref+port)
        error=sum((x-y)**2 for x,y in zip(ref,port))
        power=sum(x*x for x in ref)
        maximum=max(abs(x-y) for x,y in zip(ref,port))
        relative=math.sqrt(error/max(power,1e-30))
        rows.append(dict(preset=name,max_abs=maximum,relative_rms=relative))
        print(f'{mode:3} {name:24} max={maximum:.7g} relative_rms={relative:.7g}')
    report[mode]=rows
Path('build/sound-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
# Effects are checked against source formulas, not merely for nonzero output.
assert all(row['max_abs']<0.000001 for row in report['wet']), 'effects source comparison failed'
assert all(row['relative_rms']<0.00001 for row in report['dry']), 'dry source comparison failed'
print('PASS: dry relative RMS < 1e-5 and wet maximum absolute error < 1e-6 across all 33 source patches.')
