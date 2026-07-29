#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
import re,json,sys
ROOT=Path(__file__).resolve().parents[1]
def rgb565(p):
 r,g,b=p;return ((r>>3)<<11)|((g>>2)<<5)|(b>>3)
res={};all_ok=True
for cpp in sorted((ROOT/"source/esp32/generated").glob("*.cpp")):
 text=cpp.read_text();packed=[int(x,16) for x in re.findall(r"0x([0-9A-Fa-f]{8})u",text)]
 vals=[]
 for x in packed:
  count=(x>>16)&0xFFFF;color=x&0xFFFF;vals.extend([color]*count)
 ref=Image.open(ROOT/"rgb565_expected"/(cpp.stem+".png")).convert("RGB")
 exp=[rgb565(p) for p in ref.getdata()]
 mismatch=sum(a!=b for a,b in zip(vals,exp))+abs(len(vals)-len(exp))
 ok=len(vals)==480*320 and mismatch==0
 res[cpp.stem]={"pixels":len(vals),"mismatches":mismatch,"ok":ok};all_ok &= ok
print(json.dumps({"all_exact_rgb565":all_ok,"screens":res},indent=2));sys.exit(0 if all_ok else 1)
