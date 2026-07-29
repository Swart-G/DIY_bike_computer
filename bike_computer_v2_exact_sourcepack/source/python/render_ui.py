#!/usr/bin/env python3
# Exact source renderer for this package. It uses the layout JSON and the same drawing primitives as the reference generation.
# Run: python source/python/render_ui.py --all --output generated_png
from pathlib import Path
import argparse, json, math
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[2]
W,H=480,320
C={"bg":"#090C0F","panel":"#11161B","panel2":"#151C22","border":"#26313A","text":"#F3F6F8","muted":"#84929D","accent":"#53D6BE","accent_dim":"#173A35","success":"#66D98A","warning":"#F4C95D","danger":"#FF6B72","blue":"#69B7FF","black":"#000000","white":"#FFFFFF"}
FONTS={"xs":(10,False),"sm":(12,False),"sm_b":(12,True),"md":(14,False),"md_b":(14,True),"lg":(18,True),"xl":(24,True),"xxl":(34,True),"speed":(74,True)}
REG="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"; BOLD="/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
def cv(v): return None if v is None else C.get(v,v)
def fonts(): return {k:ImageFont.truetype(BOLD if b else REG,s) for k,(s,b) in FONTS.items()}
F=fonts(); cache={}
def icon(d,k,cx,cy,color="text",scale=1.0):
 color=cv(color); w=max(1,int(2*scale))
 if k=="bike":
  r=int(8*scale);lx=int(cx-20*scale);rx=int(cx+20*scale);yy=int(cy+8*scale);d.ellipse((lx-r,yy-r,lx+r,yy+r),outline=color,width=w);d.ellipse((rx-r,yy-r,rx+r,yy+r),outline=color,width=w);p1=(lx,yy);p2=(int(cx-6*scale),int(cy-7*scale));p3=(int(cx+5*scale),yy);d.line((p1,p2,p3,p1),fill=color,width=w);d.line((p2,(int(cx+12*scale),int(cy-7*scale)),(rx,yy)),fill=color,width=w)
 elif k=="phone": d.rounded_rectangle((cx-8,cy-13,cx+8,cy+13),radius=3,outline=color,width=w);d.ellipse((cx-1,cy+9,cx+1,cy+11),fill=color)
 elif k=="history": d.arc((cx-13,cy-13,cx+13,cy+13),30,330,fill=color,width=w);d.polygon([(cx-13,cy-7),(cx-16,cy-1),(cx-9,cy-2)],fill=color);d.line((cx,cy,cx,cy-7),fill=color,width=w);d.line((cx,cy,cx+6,cy+3),fill=color,width=w)
 elif k=="settings":
  d.ellipse((cx-11,cy-11,cx+11,cy+11),outline=color,width=w);d.ellipse((cx-4,cy-4,cx+4,cy+4),outline=color,width=w)
  for a in range(0,360,45):
   dx=math.cos(math.radians(a))*16;dy=math.sin(math.radians(a))*16;d.line((cx+dx*.65,cy+dy*.65,cx+dx,cy+dy),fill=color,width=w)
 elif k=="music": d.line((cx+4,cy-12,cx+4,cy+7),fill=color,width=w);d.line((cx+4,cy-12,cx+12,cy-9),fill=color,width=w);d.ellipse((cx-3,cy+4,cx+5,cy+12),fill=color)
 elif k=="nav": d.polygon([(cx,cy-15),(cx+11,cy+11),(cx,cy+6),(cx-8,cy+12)],outline=color)
 elif k=="diag": d.line((cx-12,cy,cx-5,cy,cx,cy-10,cx+6,cy+9,cx+12,cy+9),fill=color,width=w)
 elif k=="usb": d.line((cx,cy-13,cx,cy+11),fill=color,width=w);d.line((cx,cy-2,cx-9,cy+4),fill=color,width=w);d.line((cx,cy-4,cx+8,cy-9),fill=color,width=w);d.polygon([(cx,cy-16),(cx-4,cy-10),(cx+4,cy-10)],fill=color);d.ellipse((cx-11,cy+2,cx-7,cy+6),fill=color);d.rectangle((cx+7,cy-12,cx+11,cy-8),fill=color)
 elif k=="info": d.ellipse((cx-12,cy-12,cx+12,cy+12),outline=color,width=w);d.text((cx,cy+1),"i",font=ImageFont.truetype(BOLD,int(18*scale)),fill=color,anchor="mm")
 elif k=="sd": d.polygon([(cx-11,cy-12),(cx+5,cy-12),(cx+11,cy-6),(cx+11,cy+12),(cx-11,cy+12)],outline=color);d.text((cx,cy+2),"SD",font=ImageFont.truetype(BOLD,7),fill=color,anchor="mm")
 elif k=="battery": d.rounded_rectangle((cx-14,cy-7,cx+12,cy+7),radius=2,outline=color,width=w);d.rectangle((cx+12,cy-3,cx+15,cy+3),fill=color)
 elif k=="display": d.rounded_rectangle((cx-14,cy-10,cx+14,cy+9),radius=2,outline=color,width=w);d.line((cx-5,cy+13,cx+5,cy+13),fill=color,width=w)
 elif k=="touch": d.ellipse((cx-5,cy-5,cx+5,cy+5),outline=color,width=w);d.arc((cx-13,cy-13,cx+13,cy+13),200,340,fill=color,width=w)
 elif k=="system":
  d.rectangle((cx-11,cy-11,cx+11,cy+11),outline=color,width=w)
  for dx in (-15,15):
   for yy in (-6,0,6):d.line((cx+dx,cy+yy,cx+dx/1.4,cy+yy),fill=color,width=w)
 elif k=="trash": d.rectangle((cx-7,cy-8,cx+7,cy+10),outline=color,width=w);d.line((cx-10,cy-10,cx+10,cy-10),fill=color,width=w);d.line((cx-4,cy-14,cx+4,cy-14),fill=color,width=w)
 elif k=="check": d.ellipse((cx-18,cy-18,cx+18,cy+18),outline=color,width=3);d.line((cx-8,cy,cx-1,cy+7,cx+11,cy-8),fill=color,width=4)
def render(name):
 if name in cache:return cache[name].copy()
 q=json.loads((ROOT/"source/layouts"/f"{name}.json").read_text());bg=q["background"]
 if bg.get("base_screen"):
  im=render(bg["base_screen"]);a=bg.get("dim_alpha",0)
  if a:im=Image.alpha_composite(im.convert("RGBA"),Image.new("RGBA",(W,H),(0,0,0,a))).convert("RGB")
 else:im=Image.new("RGB",(W,H),C["bg"])
 d=ImageDraw.Draw(im)
 for e in q["elements"]:
  t=e["type"]
  if t in ("background_ref","rain_unlock_progress","status_chip","test_color_box"):continue
  if t=="text":d.text((e["x"],e["y"]),e["text"],font=F[e["font"]],fill=cv(e["color"]),anchor=e.get("anchor","la"))
  elif t=="line":d.line(tuple(e["points"]),fill=cv(e.get("color")),width=e.get("width",1))
  elif t=="ellipse":d.ellipse((e["x"],e["y"],e["x"]+e["w"],e["y"]+e["h"]),fill=cv(e.get("fill")),outline=cv(e.get("outline")),width=e.get("width",1))
  elif t=="rect":d.rectangle((e["x"],e["y"],e["x"]+e["w"],e["y"]+e["h"]),fill=cv(e.get("fill")),outline=cv(e.get("outline")),width=e.get("width",1))
  elif t=="polygon":d.polygon([tuple(p) for p in e["points"]],fill=cv(e.get("fill")),outline=cv(e.get("outline")))
  elif t=="battery_icon":
   x,y=e["x"],e["y"];lv=e["level"];d.rounded_rectangle((x,y,x+29,y+14),radius=3,outline=C["muted"],width=1);d.rectangle((x+29,y+4,x+32,y+10),fill=C["muted"]);fw=max(2,int(25*max(0,min(100,lv))/100));d.rectangle((x+2,y+2,x+2+fw,y+12),fill=C["success"] if lv>20 else C["danger"])
  elif t=="phone_icon":x,y=e["x"],e["y"];d.rounded_rectangle((x,y,x+13,y+17),radius=3,outline=C["accent"],width=1);d.ellipse((x+5,y+14,x+7,y+16),fill=C["accent"])
  elif t=="rain_button":x,y=e["x"],e["y"];d.rounded_rectangle((x,y,x+20,y+18),radius=6,outline=C["warning"],width=1);d.polygon([(x+10,y+4),(x+5,y+11),(x+10,y+16),(x+15,y+11)],outline=C["warning"])
  elif t=="icon":icon(d,e["kind"],e["cx"],e["cy"],e.get("color","text"),e.get("scale",1.0))
  elif t=="page_dots":
   n=e["count"];a=e["active"];y=e["y"];sx=(W-(n*8+(n-1)*8))//2
   for i in range(n): d.rounded_rectangle((sx+i*16,y-3,sx+i*16+8,y+3),radius=3,fill=C["accent"]) if i==a else d.ellipse((sx+i*16+1,y-2,sx+i*16+5,y+2),fill=C["muted"])
  elif t=="button":
   x,y,w,h=e["x"],e["y"],e["w"],e["h"];k=e.get("kind","default");en=e.get("enabled",True);fill,ol=("panel","border") if not en else (("accent","accent") if k=="primary" else (("#32171A","danger") if k=="danger" else (("#17321F","success") if k=="success" else ("panel2","border"))));d.rounded_rectangle((x,y,x+w,y+h),radius=10,fill=cv(fill),outline=cv(ol),width=1)
  elif t=="card":x,y,w,h=e["x"],e["y"],e["w"],e["h"];d.rounded_rectangle((x,y,x+w,y+h),radius=12,fill=C["panel"],outline=C["border"],width=1)
  elif t=="menu_tile":x,y,w,h=e["x"],e["y"],e["w"],e["h"];a=e.get("active",False);d.rounded_rectangle((x,y,x+w,y+h),radius=14,fill=C["panel2"] if a else C["panel"],outline=C["accent"] if a else C["border"],width=2 if a else 1)
  elif t=="hero_button":x,y,w,h=e["x"],e["y"],e["w"],e["h"];d.rounded_rectangle((x,y,x+w,y+h),radius=e.get("radius",16),fill=C["accent"],outline=C["accent"],width=1)
  elif t in ("panel","modal","overlay","pill","code_box","credit_box","file_box","graph_panel","list_item","progress_track","progress_fill","setting_row","warning_box"):
   x,y,w,h=e["x"],e["y"],e["w"],e["h"];d.rounded_rectangle((x,y,x+w,y+h),radius=e.get("radius",0),fill=cv(e.get("fill")),outline=cv(e.get("outline")),width=e.get("width",1))
  else:raise ValueError(t)
 cache[name]=im.copy();return im
if __name__=="__main__":
 ap=argparse.ArgumentParser();ap.add_argument("screen",nargs="?");ap.add_argument("--all",action="store_true");ap.add_argument("--output",default="generated_png");a=ap.parse_args();o=Path(a.output);o.mkdir(parents=True,exist_ok=True);names=[p.stem for p in (ROOT/"source/layouts").glob("*.json")] if a.all else [a.screen];
 for n in sorted(names):render(n).save(o/f"{n}.png")
