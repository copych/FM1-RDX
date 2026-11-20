from pathlib import Path
import json,math,sys

def read(file):
    transitions={'A':{},'P':{}};samples={}
    for line in Path(file).read_text().splitlines():
        tag,case,sample,x,y=line.split(',');case=int(case);sample=int(sample)
        if tag=='S':samples[(case,sample)]=(float(x),float(y))
        else:transitions[tag].setdefault(case,[]).append((sample,int(x)))
    return transitions,samples
a,sa=read('build/envelope-reference.csv');b,sb=read('build/envelope-port.csv')
assert sa.keys()==sb.keys(),'checkpoint coverage mismatch'
assert all(math.isfinite(v) for values in list(sa.values())+list(sb.values()) for v in values),'non-finite envelope checkpoint'
result={}
for kind in ['A','P']:
    changed=[];maximum=0
    for case in a[kind]:
        x=a[kind][case];y=b[kind][case]
        if x!=y:
            delta=max((abs(i[0]-j[0]) for i,j in zip(x,y)),default=0)
            maximum=max(maximum,delta)
            changed.append({'case':case,'patch':case//36,'operator':(case//9)%4,'mode':(case//3)%3,'profile':case%3,'reference':x,'port':y,'max_aligned_delta_samples':delta})
    result[kind]={'cases':len(a[kind]),'changed_cases':len(changed),'max_aligned_delta_samples':maximum,'differences':changed}
result['max_checkpoint_amplitude_error']=max(abs(x-y) for key in sa for x,y in zip(sa[key],sb[key]))
Path('build/envelope-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
for kind in ['A','P']:print(kind,{k:v for k,v in result[kind].items() if k!='differences'})
print('max checkpoint amplitude error',result['max_checkpoint_amplitude_error'])
if '--check' in sys.argv:
    assert result['A']['changed_cases']==0,'amplitude stage transition mismatch'
    assert result['P']['changed_cases']==0,'pitch stage transition mismatch'
    assert result['max_checkpoint_amplitude_error']<0.00001,'envelope shape mismatch'
    print('PASS: sample-identical AEG/PEG stage transitions across 1,188 preset/operator/mode/gate scenarios.')
