import colorsys, re, sys
# id, name, bg, surface, raised, line, text, muted, body, accent, accent_bright, accent2, grad_a, grad_b, rec
T = """neon|NEON LAB|#060419|#0d0b28|#141230|rgba(78,78,108,.55)|#ffffff|#b8b8c8|#c4c2d6|#2f39ba|#acb0e3|#ff5ec4|#2f39ba|#ff5ec4|#e5173a
magic|MAGIC BENTO|#060010|#0a041a|#120a2a|rgba(255,255,255,.06)|#ffffff|#9b93b8|#d9d4ea|#8400ff|#c084fc|#c084fc|#8400ff|#c084fc|#e5173a
command|MIDNIGHT COMMAND|#040506|#07080a|#111214|#1c1d20|#ffffff|#8b8b8d|#cfcfd0|#ff6363|#ff8a8a|#63a1ff|#ff6363|#ff9a6b|#ff6363
abyss|DEEP SEA|#012624|#011d1c|#003734|rgba(187,199,198,.12)|#ffffff|#9fb3b1|#edfffe|#00a39b|#cbfffc|#fde9ff|#00827c|#cbfffc|#ff4f7b
reef|MIDNIGHT REEF|#020b26|#020920|#0a1a45|rgba(190,200,220,.12)|#ffffff|#9aa9c7|#f2f6ff|#2f7bff|#c9e2ff|#ffe3f4|#2f7bff|#c9e2ff|#ff4d7d
aurora|AURORA|#03160f|#02110c|#0a2a1e|rgba(190,200,220,.12)|#ffffff|#9db8ab|#f2f6ff|#19c37d|#c8ffe5|#e8d4ff|#19c37d|#c8ffe5|#ff5c8a
amethyst|AMETHYST|#12062a|#0e0522|#22104a|rgba(190,200,220,.12)|#ffffff|#ab9fc9|#f2f6ff|#8e5cff|#ece2ff|#ffd6ef|#8e5cff|#ece2ff|#ff548c
ember|EMBER TIDE|#1d0b08|#170906|#3a1710|rgba(190,200,220,.12)|#ffffff|#c4a59a|#f2f6ff|#ff6a3d|#ffe4d6|#ffe9b8|#ff6a3d|#ffe4d6|#ff4060
glass|DUSK GLASS|#241656|rgba(255,255,255,.09)|rgba(255,255,255,.14)|rgba(255,255,255,.16)|#ffffff|rgba(255,255,255,.72)|rgba(255,255,255,.86)|#ff8a5c|#ffd2a8|#b9adff|#ff8a5c|#b9adff|#ff4d6d
lagoon|LAGOON GLASS|#06243f|rgba(255,255,255,.09)|rgba(255,255,255,.14)|rgba(255,255,255,.16)|#ffffff|rgba(255,255,255,.72)|rgba(255,255,255,.86)|#3fe0d0|#c8fff6|#8fb8ff|#3fe0d0|#8fb8ff|#ff5d7a
rose|ROSE GLASS|#2a0f24|rgba(255,255,255,.09)|rgba(255,255,255,.14)|rgba(255,255,255,.16)|#ffffff|rgba(255,255,255,.72)|rgba(255,255,255,.86)|#ff7aa2|#ffd9e3|#ffc2a8|#ff7aa2|#ffc2a8|#ff3d6e
northern|NORTHERN GLASS|#0a1030|rgba(255,255,255,.09)|rgba(255,255,255,.14)|rgba(255,255,255,.16)|#ffffff|rgba(255,255,255,.72)|rgba(255,255,255,.86)|#5cf2b0|#d6ffe9|#9fb6ff|#5cf2b0|#9fb6ff|#ff5c8a
nightglass|MIDNIGHT GLASS|#05060f|rgba(255,255,255,.09)|rgba(255,255,255,.14)|rgba(255,255,255,.16)|#ffffff|rgba(255,255,255,.72)|rgba(255,255,255,.86)|#8f9bff|#e0e4ff|#c99bff|#8f9bff|#c99bff|#ff4d7d
golden|GOLDEN HOUR|#2b1208|rgba(255,255,255,.09)|rgba(255,255,255,.14)|rgba(255,255,255,.16)|#ffffff|rgba(255,255,255,.72)|rgba(255,255,255,.86)|#ffb347|#fff0c9|#ff8a6b|#ffb347|#ff8a6b|#ff3f4f
lavender|LAVENDER HAZE|#1d1440|rgba(255,255,255,.09)|rgba(255,255,255,.14)|rgba(255,255,255,.16)|#ffffff|rgba(255,255,255,.72)|rgba(255,255,255,.86)|#c89bff|#f3e8ff|#ffb8e6|#c89bff|#ffb8e6|#ff4f8f
indigo|INDIGO LAB|#060419|rgba(255,255,255,.055)|rgba(255,255,255,.1)|rgba(172,176,227,.16)|#ffffff|rgba(210,212,245,.62)|rgba(240,240,255,.88)|#5a63e0|#a9ade0|#c4559f|#2a33a8|#c4559f|#e5173a
cinema|CINEMA|#000000|#0a0a0a|#161616|rgba(255,255,255,.28)|#ffffff|#9a9a9a|#ffffff|#ff2936|#ffffff|#ffffff|#ff2936|#ff2936|#ff2936
terminal|TERMINAL|#030703|#071207|#0c1f0c|rgba(51,255,102,.35)|#33ff66|#1fa647|#33ff66|#33ff66|#a6ffbd|#ffb000|#33ff66|#33ff66|#ff5040
rack|STUDIO RACK|#121314|#1c1d20|#26272b|#4a4b50|#ece9e2|#8f8b83|#d4d0c8|#ffb000|#ffc94d|#2bd47d|#ffb000|#ffb000|#ff4a3d
noir|RACK NOIR|#0b0c0e|#141519|#1d1f24|#2e3238|#e6ecf2|#7f8893|#cfd6de|#4fc3ff|#8fdcff|#4fc3ff|#4fc3ff|#4fc3ff|#ff3b50
vintage|VINTAGE CONSOLE|#17110c|#221911|#2e2218|#5a4532|#f3e6cc|#a8957a|#e6d6b8|#e8b25a|#f5cd82|#ff8a2b|#e8b25a|#e8b25a|#e63c28
redline|REDLINE|#100a0b|#1a1314|#251a1c|#3d3436|#f1e6e6|#958487|#dacfd0|#ff3b3b|#ff7a6e|#ff3b3b|#ff3b3b|#ff3b3b|#ff2828
modular|MODULAR|#0e1112|#171b1c|#202627|#323b3a|#e4f2ee|#80918d|#cbdcd8|#3ee6c1|#8cf5dc|#3ee6c1|#3ee6c1|#3ee6c1|#ff5068
vapor|VAPOR|#0d0221|rgba(24,6,52,.78)|rgba(54,16,96,.7)|rgba(255,43,214,.35)|#ffffff|#c6a6ff|#f2e6ff|#ff2bd6|#00f0ff|#00f0ff|#ff2bd6|#00f0ff|#ff2bd6"""
def parse(c, under):
    c=c.strip()
    if c.startswith('#'): return tuple(int(c[i:i+2],16) for i in (1,3,5))
    m=re.match(r'rgba\(([\d.]+),([\d.]+),([\d.]+),([\d.]+)\)', c.replace(' ',''))
    r,g,b,a=[float(x) for x in m.groups()]
    return tuple(round(u*(1-a)+v*a) for u,v in zip(under,(r,g,b)))
def mix(a,b,t): return tuple(round(x*(1-t)+y*t) for x,y in zip(a,b))
def hexs(c): return '0x%02X%02X%02X'%c
def lum(c):
    def ch(v):
        v/=255; return v/12.92 if v<=0.03928 else ((v+0.055)/1.055)**2.4
    r,g,b=[ch(v) for v in c]; return 0.2126*r+0.7152*g+0.0722*b
def contrast(a,b):
    la,lb=sorted([lum(a),lum(b)],reverse=True); return (la+0.05)/(lb+0.05)
def lift(c,bg,target):
    # brighten c until it contrasts `target` with bg
    h,l,s=colorsys.rgb_to_hls(*[v/255 for v in c]); 
    for _ in range(40):
        cc=tuple(round(v*255) for v in colorsys.hls_to_rgb(h,l,s))
        if contrast(cc,bg)>=target or l>=0.92: return cc
        l=min(0.92,l+0.03)
    return cc
SPECTRUM=['FF5EC4','FFC266','7EE0B0','8B7BFF','5AA8FF','FF7AD9','B8E86A','C77DFF','3FD6E0','FFE07A','FF5C7A','6FE39A','6F7DFF','FF9A5C','E46FFF','ACB0E3']
def shades(c):
    h,l,s=colorsys.rgb_to_hls(*[v/255 for v in c]); out=[c]
    for d in (-0.045,0.045):
        out.append(tuple(round(v*255) for v in colorsys.hls_to_rgb((h+d)%1,l,s)))
    return out
def tint(c, toward, t):
    # pull hue partly toward `toward`'s hue
    h1,l1,s1=colorsys.rgb_to_hls(*[v/255 for v in c]); h2,_,_=colorsys.rgb_to_hls(*[v/255 for v in toward])
    dh=((h2-h1+0.5)%1)-0.5
    return tuple(round(v*255) for v in colorsys.hls_to_rgb((h1+dh*t)%1,l1,s1))
rows=[]
# (Written for INDIGO X DENIM: python3 tools/palettes.py src/melodee/ui/palettes.h)
# NIGHT first: Melodee's own, exactly
night=dict(wave=0,place=0x5FD97A,name='NIGHT',bg=0x17141F,panel=0x1D1926,surf=0x221E2C,raised=0x262033,quiet=0x1A1722,line=0x2E2939,text=0xF3EEF8,sec=0xC9C2D6,mid=0x9A93A8,dim=0x5A5468,rec=0xFF4848,amber=0xFFC145,play=0x5FD97A,a=0xFF7A5C,b=0xFF6F93,grad=0)
night_tracks=[[0xFF7A5C,0xFF6F93,0xFF9A3C],[0xFFC145,0xFF9F5A,0xE2D94A],[0x46D9B0,0x4FC7E0,0x8FDB6A],[0x9D8CFF,0xC587F2,0x6FA0FF],[0x4FB8FF,0x4FFFF4,0x4F66FF],[0xFF6FA8,0xFF6FEB,0xFF796F],[0xA6E04A,0xE0D44A,0x60E04A],[0xD77BFF,0x997BFF,0xFF7BE9],[0x2FD0D6,0x2FD68E,0x2F82D6],[0xE8D94A,0xE88F4A,0xADE84A],[0xFF5C7A,0xFF5CC6,0xFF8A5C],[0x5FD97A,0x7DD95F,0x5FD9B3],[0x6F8CFF,0x6FCFFF,0x956FFF],[0xFF9A3C,0xFF3F3C,0xFFF53C],[0xF06FD8,0xCC6FF0,0xF06F9C],[0xB8C2D6,0xB8D0D6,0xBCB8D6]]
night_loops=[[0x6FD3FF,0x6FA6FF,0x6FFFE5],[0xFFA36F,0xFF7A6F,0xFFD06F],[0xC09CFF,0x9C9EFF,0xE69CFF],[0x7DEBA6,0x7DEBD9,0xA4EB7D]]
def cpp_shades(tr): return '{{'+', '.join('{'+', '.join('0x%06X'%v for v in t)+'}' for t in tr)+'}}'
out=[]
out.append(('NIGHT',night,night_tracks,night_loops))
# NEON LAB: the hand-made INDIGO palette of v0.2.0, magenta first (the theme's accent is too dark to lead a track).
neon=dict(name='NEON LAB',bg=0x060419,panel=0x0A0822,surf=0x0D0B28,raised=0x141230,quiet=0x08061D,line=0x262449,text=0xFFFFFF,sec=0xC4C2D6,mid=0x9C9AB8,dim=0x4E4E6C,rec=0xE5173A,amber=0x6CCBFF,play=0x7EE0B0,a=0x2F39BA,b=0xFF5EC4,grad=1,wave=0x5CC8FF,place=0xB46BFF)
neon_tracks=[[0xFF5EC4,0xFF5EEF,0xFF5E98],[0x4FC3FF,0x4FE3FF,0x4F9FFF],[0x7EE0B0,0x7DE095,0x7DE0CA],[0x8B7BFF,0x7B8EFF,0xAE7BFF],[0x5AA8FF,0x5AD4FF,0x5A7BFF],[0xFF7AD9,0xFF7AFC,0xFF7AB5],[0xB8E86A,0xDAE76A,0x95E76A],[0xC77DFF,0xA37DFF,0xEA7DFF],[0x3FD6E0,0x3FDFBE,0x3FAADF],[0x7FD4FF,0x7FEFFF,0x7FB4FF],[0xFF5C7A,0xFF5CA6,0xFF6A5C],[0x6FE39A,0x6FE27A,0x6FE2B9],[0x6F7DFF,0x6FA3FF,0x876FFF],[0xFF9A5C,0xFF6D5C,0xFFC65C],[0xE46FFF,0xBD6FFF,0xFF6FF3],[0xACB0E3,0xACBEE3,0xB6ACE3]]
neon_loops=[[0x6FB4FF,0x6FDAFF,0x6F8DFF],[0xFF8FD0,0xFE8FEE,0xFE8FB1],[0xA99BFF,0x9BA8FF,0xC49BFF],[0x7EE0B0,0x7DE095,0x7DE0CA]]
out.append(('NEON LAB',neon,neon_tracks,neon_loops))
for line in T.split('\n'):
    f=line.split('|'); id_,name=f[0],f[1]
    if id_=='neon': continue
    bg=parse(f[2],(0,0,0))
    surf=parse(f[3],bg); raised=parse(f[4],bg)
    if raised==surf or contrast(raised,bg)<contrast(surf,bg): raised=mix(surf,(255,255,255),0.06)
    linec=parse(f[5],bg)
    if contrast(linec,bg)<1.25: linec=mix(bg,(255,255,255),0.16)
    text=parse(f[6],bg); muted=parse(f[7],bg); body=parse(f[8],bg)
    accent=parse(f[9],bg); bright=parse(f[10],bg); acc2=parse(f[11],bg)
    ga=parse(f[12],bg); gb=parse(f[13],bg); rec=parse(f[14],bg)
    panel=mix(bg,surf,0.5); quiet=mix(bg,surf,0.25)
    mid=lift(muted,bg,4.0) if contrast(muted,bg)<4.0 else muted
    dim=mix(muted,bg,0.45)
    if contrast(dim,bg)<1.8: dim=mix(muted,bg,0.3)
    sec=body
    lead=lift(accent,bg,4.5)
    tracks=[]
    for n,hx in enumerate(SPECTRUM):
        c=tuple(int(hx[i:i+2],16) for i in (0,2,4))
        if n==0: c=lead
        elif n==15: c=lift(bright,bg,4.5)
        else: c=lift(tint(c,accent,0.18),bg,4.5)
        tracks.append(shades(c))
    loops=[shades(lift(tint(tuple(int(h[i:i+2],16) for i in (0,2,4)),accent,0.15),bg,4.5)) for h in ['6FB4FF','FF8FD0','A99BFF','7EE0B0']]
    a=ga if contrast(ga,bg)>=1.6 else lift(ga,bg,2.0)
    b=gb
    grad=1 if a!=b else 0
    if not grad: a,b=lead,lift(bright,bg,4.5)
    pal=dict(wave=0,place=(0x7E,0xE0,0xB0),name=name,bg=bg,panel=panel,surf=surf,raised=raised,quiet=quiet,line=linec,text=text,sec=sec,mid=mid,dim=dim,rec=lift(rec,bg,3.5),amber=(0xFF,0xC2,0x66),play=(0x7E,0xE0,0xB0),a=a,b=b,grad=1)
    out.append((name,pal,[[ (r<<16)|(g<<8)|bb for (r,g,bb) in t] for t in tracks],[[ (r<<16)|(g<<8)|bb for (r,g,bb) in t] for t in loops]))
def v(x): return '0x%06X'%x if isinstance(x,int) else '0x%02X%02X%02X'%x
body=[]
for name,p,tr,lo in out:
    body.append('    {"%s", %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s,\n     %s,\n     %s},'%(
        name,v(p['wave']),v(p['place']),v(p['bg']),v(p['panel']),v(p['surf']),v(p['raised']),v(p['quiet']),v(p['line']),v(p['text']),v(p['sec']),v(p['mid']),v(p['dim']),v(p['rec']),v(p['amber']),v(p['play']),v(p['a']),v(p['b']),'true' if p['grad'] else 'false',cpp_shades(tr),cpp_shades(lo)))
hdr='''// SPDX-License-Identifier: AGPL-3.0-only
// INDIGO X DENIM: the screen's palettes. NIGHT is Melodee's own; the others are INDIGO'S MEDIA LAB's themes,
// in its order (renderer/themes.js), made for a 320 x 240 screen: each theme's night, surfaces, lines, text and
// accents, its gradient's two ends, and sixteen track colours (its accent first) bright enough on its night.
// Written by tools/palettes.py; run it again rather than editing this file.
#pragma once
#include <array>
#include <cstdint>

namespace melodee::ui {

struct Palette {
    const char* name;
    uint32_t wave, place; // INDIGO X DENIM: every waveform's colour (0: the track's) and the place bar's
    uint32_t bg, panel, surf, raised, quiet, line, text, sec, mid, dim, rec, amber, play, brand_a, brand_b;
    bool gradient;
    std::array<std::array<uint32_t, 3>, 16> tracks;
    std::array<std::array<uint32_t, 3>, 4> loops;
};

inline constexpr std::array<Palette, %d> palettes {{
%s
}};
constexpr unsigned default_palette = 1; // NEON LAB: INDIGO'S MEDIA LAB's own

} // namespace melodee::ui
'''%(len(out),'\n'.join(body))
open(sys.argv[1],'w').write(hdr)
print(len(out),'palettes')
