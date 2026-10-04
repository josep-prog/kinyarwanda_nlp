"""Do other speakers make the book's falling (âa) vs rising (aâ) long vowels? Radio Rwanda + Mwanafunzi."""
import csv, io, zipfile, collections, warnings, json, numpy as np, soundfile as sf
warnings.filterwarnings("ignore")
from amasaku.align import VitsAligner, measure
D='/home/joe/Documents/kinyarwanda-tts/data/prepared_v2/'
WORDS={'umwana':(1,'falling'),'umwaka':(1,'falling'),'imyaka':(1,'falling'),'ubwato':(1,'falling'),'abana':(1,'falling'),
       'umwami':(1,'rising'),'abami':(1,'rising')}
rows=list(csv.DictReader(open(D+'metadata.csv'),delimiter='|'))
z=zipfile.ZipFile(D+'prepared_dataset.zip'); al=VitsAligner()
per=collections.Counter(); out=[]
for r in rows:
    ws=r['text'].split()
    hits=[(i,w) for i,w in enumerate(ws) if w in WORDS and i<len(ws)-1]   # not sentence-final
    hits=[(i,w) for i,w in hits if per[(r['channel'],w)]<40]
    if not hits: continue
    wav,_=sf.read(io.BytesIO(z.read(f"wavs/{r['id']}.wav")),dtype='float32')
    try: vs=measure(wav, al.align(wav, r['text']), ws)
    except Exception: continue
    for i,w in hits:
        mine=[v for v in vs if v['word']==i]
        k=WORDS[w][0]
        if len(mine)>k and mine[k]['pitch_1'] is not None and mine[k]['pitch_2'] is not None:
            out.append({'channel':r['channel'],'word':w,'slope':mine[k]['pitch_2']-mine[k]['pitch_1'],'dur':mine[k]['dur'],'id':r['id']})
            per[(r['channel'],w)]+=1
json.dump(out,open('/tmp/claude-1000/-home-joe-Documents/f5257b76-da9e-420e-afec-1397f955e62f/scratchpad/radio.json','w'))
for ch in ('Radio Rwanda','ISMAËL MWANAFUNZI DOKS'):
    for w,(k,kind) in WORDS.items():
        x=np.array([o['slope'] for o in out if o['channel']==ch and o['word']==w])
        if len(x): print(f"{ch[:12]:12s} {w:7s} book {kind:7s} n={len(x):2d}  2nd-1st half median {np.median(x):+.1f} st  rising {np.mean(x>0)*100:3.0f}%")
