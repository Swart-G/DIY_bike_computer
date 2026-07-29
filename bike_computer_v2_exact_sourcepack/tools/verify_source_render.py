#!/usr/bin/env python3
from pathlib import Path
from PIL import Image,ImageChops
import json,subprocess,sys,shutil
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/"verification"/"rerendered";shutil.rmtree(out,ignore_errors=True)
subprocess.check_call([sys.executable,str(ROOT/"source/python/render_ui.py"),"--all","--output",str(out)])
res={};ok=True
for refp in sorted((ROOT/"reference_png").glob("*.png")):
 a=Image.open(refp).convert("RGB");b=Image.open(out/refp.name).convert("RGB");d=ImageChops.difference(a,b);n=sum(1 for p in d.getdata() if p!=(0,0,0));res[refp.stem]=n;ok &= n==0
print(json.dumps({"all_exact":ok,"mismatches":res},indent=2));sys.exit(0 if ok else 1)
