"""The approved Torus startup ("infinity -> laser", HaloStartup.h) for CANertia Studio.

Geometry and timing are the firmware's, formula by formula (taken over from the V6.0.6 Pillow port); the drawing
goes through a painter interface (line, dot, stroke, polygon, logo) that studio_paint.py implements with QPainter.
"""
from pathlib import Path
import json, math, sys

ROOT=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parents[1]))
DEG=math.pi/180
WHITE=(242,241,236);BLUE=(46,91,255)
STARTUP_WHITE=(246,246,246);LOGO_WHITE=(255,255,255);LASER=(220,229,255);SPARK=(207,224,255);TITAN_WING=BLUE

def clamp(v,lo,hi):return lo if v<lo else hi if v>hi else v
def smooth(t):t=clamp(t,0,1);return t*t*(3-2*t)
def soft(p):p=clamp(p,0,1);return p*p*p*(10+p*(-15+6*p))
def phase(p,a,b):return clamp((p-a)/(b-a),0,1)

# ---------------------------------------------------------------- shared geometry
class Track:
    def __init__(self,points):
        self.p=points;self.cum=[0.]
        for a,b in zip(points,points[1:]):self.cum.append(self.cum[-1]+math.hypot(b[0]-a[0],b[1]-a[1]))
        self.total=self.cum[-1]
    def at(self,d):
        if d<=0:return self.p[0]
        if d>=self.total:return self.p[-1]
        lo,hi=1,len(self.p)-1
        while lo<hi:
            mid=(lo+hi)//2
            if self.cum[mid]<d:lo=mid+1
            else:hi=mid
        seg=self.cum[lo]-self.cum[lo-1];f=(d-self.cum[lo-1])/seg if seg>0 else 0
        a,b=self.p[lo-1],self.p[lo];return (a[0]+(b[0]-a[0])*f,a[1]+(b[1]-a[1])*f)
    def upto(self,length):
        out=[self.p[0]]
        for i in range(1,len(self.p)):
            if self.cum[i-1]>=length:break
            if self.cum[i]>length:out.append(self.at(length));break
            out.append(self.p[i])
        return out

def cubic(a,b,c,d,n=72):
    out=[]
    for i in range(n):
        p=i/(n-1);q=1-p
        out.append((q*q*q*a[0]+3*q*q*p*b[0]+3*q*p*p*c[0]+p*p*p*d[0],q*q*q*a[1]+3*q*q*p*b[1]+3*q*p*p*c[1]+p*p*p*d[1]))
    return out

class Assets:
    def __init__(self):
        self.paths=json.loads((ROOT/'assets/preview/titan_paths.json').read_text())
        scale=280/640;left,top=233-140,233-259*scale/2
        self.contours=[Track([(left+x*scale,top+y*scale) for x,y in c['points']]) for c in json.loads((ROOT/'assets/source/logo_contours.json').read_text())]

# ---------------------------------------------------------------- startup (HaloStartup.h)
class Startup:
    HOLD,CLOSE,TITAN,FILL=900,800,1100,850
    CX,CY,RX,RY=233,224,150,52
    def __init__(self,assets):
        self.a=assets;CX,CY,RX,RY=self.CX,self.CY,self.RX,self.RY;LX=CX-RX
        inf=[(CX+RX*math.sin(-math.pi/2+2*math.pi*i/419),CY+RY*math.sin(2*(-math.pi/2+2*math.pi*i/419))) for i in range(420)]
        self.half_a=Track(inf[:210]);self.half_b=Track(inf[209:])
        self.tour=assets.contours;self.tour_len=sum(t.total for t in self.tour)
        t0=self.tour_point(0,0);t1=self.tour_point(0,.02);tx,ty=t1[0]-t0[0],t1[1]-t0[1];tl=math.hypot(tx,ty) or 1;tx/=tl;ty/=tl
        entry=cubic((CX-252,CY+46),(CX-205,CY+42),(LX,CY+46),(LX,CY));link=cubic((LX,CY),(LX,CY-34),(t0[0]-tx*34,t0[1]-ty*34),t0)
        self.track_a=Track(entry+inf[1:210]);self.track_b=Track([(2*CX-x,y) for x,y in entry]+inf[210:]+link[1:])
        self.e=Track(entry).total;self.h1=self.half_a.total;self.lk=Track(link).total
        V1,VL=.5,1.72;lend=self.e+self.h1+self.lk+self.tour_len
        d=tm=0.;nxt=1;self.dist=[0.]
        while d<lend and len(self.dist)<3400:
            if d<self.e+self.h1:v=V1
            elif d<self.e+self.h1+self.lk:v=V1+(VL-V1)*soft((d-self.e-self.h1)/self.lk)
            else:v=VL*(.35+.65*soft((lend-d)/120))
            d+=v*.25;tm+=.25
            if tm>=nxt:self.dist.append(min(d,lend));nxt+=1
        if self.dist[-1]<lend:self.dist.append(lend)
        self.trace_end=len(self.dist)-1
        self.close_at=next(i for i,v in enumerate(self.dist) if v>=self.e+self.h1)
        self.fill_start=self.trace_end-60;self.draw_ms=math.ceil(self.fill_start+self.FILL+150)
        self.hold_end=self.draw_ms+self.HOLD;self.titan_at=self.hold_end+self.CLOSE;self.total=self.titan_at+self.TITAN
        self.taper=[(i/55)**.72 for i in range(56)]
    def tour_point(self,i,f):t=self.tour[i];return t.at(clamp(f,0,1)*t.total)
    def tour_head(self,q):
        l=clamp(q,0,1)*self.tour_len
        for i,t in enumerate(self.tour):
            if l<=t.total or i==len(self.tour)-1:return i,min(1.,l/t.total if t.total>0 else 1.)
            l-=t.total
        return 0,0.
    def star(self,g,x,y,alpha,size):
        if alpha<=0:return
        g.dot(x,y,size*2.5,STARTUP_WHITE,.038*alpha*.72);g.dot(x,y,size*1.65,STARTUP_WHITE,.075*alpha*.72)
        g.dot(x,y,size,STARTUP_WHITE,alpha);g.dot(x-.8,y-.9,size*.31,STARTUP_WHITE,alpha)
    def ribbon(self,g,track,travel,span,alpha,band,twist):
        if alpha<=0:return
        begin=max(0,travel-span);pts=[];depth=[]
        for i in range(56):
            f=begin+(travel-begin)*i/55;pts.append(track.at(f*track.total));depth.append(.5+.5*math.sin(f*2*math.pi*4+twist))
        pieces=[]
        for i in range(1,56):
            dx,dy=pts[i][0]-pts[i-1][0],pts[i][1]-pts[i-1][1];l=math.hypot(dx,dy)
            if l<.001:continue
            z=clamp(depth[i],0,1);pieces.append((z,pts[i-1],pts[i],-dy/l,dx/l,band*(.23+.77*z)*self.taper[i]))
        pieces.sort(key=lambda p:p[0])
        for z,a,b,nx,ny,w in pieces:
            off=w*.42
            g.line(a[0],a[1],b[0],b[1],max(.2,w*.5),STARTUP_WHITE,alpha*(.18+.52*z))
            g.line(a[0]+nx*off,a[1]+ny*off,b[0]+nx*off,b[1]+ny*off,.35,STARTUP_WHITE,alpha*(.35+.6*z))
            if w>3:g.line(a[0]-nx*off,a[1]-ny*off,b[0]-nx*off,b[1]-ny*off,.25,STARTUP_WHITE,alpha*.12)
        self.star(g,pts[-1][0],pts[-1][1],alpha,3.05)
    def logo(self,g,alpha,cache,clip=None):
        # The OJM logo (assets/preview/halo_logo.png, 280x114 at 93,176), optionally only the columns x0..x1.
        if alpha<=0:return
        g.logo(93,176,alpha,clip)
    @staticmethod
    def spark_hash(i):x=math.sin(i*12.9898+78.233)*43758.5453;return x-math.floor(x)
    def stage(self,e):return 0 if e<self.draw_ms else 1 if e<self.hold_end else 2 if e<self.titan_at else 3
    def paint(self,g,e,wing_target,cache):
        if e<self.draw_ms:self.draw_phase(g,e,cache)
        elif e<self.hold_end:self.logo(g,1,cache)
        elif e<self.titan_at:self.close(g,e,cache)
        else:self.titan(g,e,wing_target)
    def draw_phase(self,g,tm,cache):
        dB=self.dist[min(self.trace_end,int(tm))];dA=min(dB,self.e+self.h1);tourD=dB-self.e-self.h1-self.lk
        k=clamp((dA-self.e)/self.h1,0,1);infA=.5*(1-soft((tm-self.close_at)/380));aOut=clamp((tm-self.close_at)/220,0,1)
        q=clamp(tourD/self.tour_len,0,1);hi,hp=self.tour_head(q)
        outline=1-soft(phase(tm,self.fill_start+self.FILL*.4,self.fill_start+self.FILL))
        bright=soft((tm-self.fill_start)/self.FILL) if tm>self.fill_start else 0
        frame=math.floor(tm/4400*900)
        if infA>0 and k>0:
            for h in (self.half_a,self.half_b):g.stroke(h.upto(k*h.total),.45,STARTUP_WHITE,infA)
        ball_a=.88*soft(tm/80)*(1-soft(aOut))
        if aOut<1:self.ribbon(g,self.track_a,clamp(dA/self.track_a.total,0,1),.11,ball_a,5,-tm/900)
        if 0<aOut<1:g.dot(self.CX+self.RX,self.CY,3+8*aOut,STARTUP_WHITE,.4*(1-aOut))
        ball_b=.88*soft(tm/80)*(1-soft((tourD+10)/40)) if tourD<30 else 0
        if ball_b>0:self.ribbon(g,self.track_b,clamp(dB/self.track_b.total,0,1),.11,ball_b,5,tm/900)
        if tourD>0 and outline>0:
            for i in range(hi+1):t=self.tour[i];g.stroke(t.upto((1 if i<hi else hp)*t.total),.45,LASER,.85*outline)
        if 0<q<1:
            x,y=self.tour_point(hi,hp);self.star(g,x,y,1,2.6)
            for n in range(10):
                age=(n+.5)*.004;te=q-age*1.2
                if te<=0:continue
                i,p=self.tour_head(te);o=self.tour_point(i,p);seed=frame-n
                g.dot(o[0]+(self.spark_hash(seed)-.5)*120*age,o[1]-(40+self.spark_hash(seed+3)*60)*age+900*age*age,.9,SPARK,1-age/.04)
        if bright>0:self.logo(g,bright,cache)
    def close(self,g,e,cache):
        u=.54+.16*(e-self.hold_end)/self.CLOSE;p=smooth((u-.54)/.11)
        if p<1:
            half=math.ceil(140*(1-p));self.logo(g,1,cache,(233-half,233+half));g.dot(233,233,2.5,LOGO_WHITE,smooth(p))
        if u>.635:q=smooth((u-.635)/.065);g.dot(233,233-34*q,2.5,LOGO_WHITE,1-phase(u,.69,.70))
    def titan(self,g,e,wing_target):
        time=(e-self.titan_at)*1280/self.TITAN
        for i,path in enumerate(self.a.paths):
            delay=0 if i<2 else 130 if i==2 else 260;progress=smooth((time-delay)/900)
            amount=progress*(len(path)-1)
            if amount<=0:continue
            n=int(amount);pts=[tuple(p) for p in path[:n+1]]
            if n<len(path)-1:
                a,b=path[n],path[n+1];f=amount-n;pts.append((a[0]+(b[0]-a[0])*f,a[1]+(b[1]-a[1])*f))
            g.stroke(pts,.85,WHITE,230/255)
            if progress<1:g.dot(pts[-1][0],pts[-1][1],2,WHITE,230/255)
        if time>770:
            u=clamp((time-770)/460,0,1);q=u-1;wing(g,1,0,0,wing_target*(1+1.6*q*q*q+.6*q*q),smooth((time-770)/120),0)

def wing(g,s,ox,oy,deg,alpha,zoom):
    """The Active Aero wing: a flush airfoil in the deck line (white edges, blue fill), pivot 392,232."""
    if alpha<=.002:return
    A=deg*DEG+math.atan2(1,18.6);ch=math.hypot(18.6,1);dx,dy,nx,ny=math.cos(A),-math.sin(A),math.sin(A),math.cos(A)
    upper=[];lower=[]
    for i in range(17):
        u=i/16;th=2.3*max(0,math.sin(math.pi*min(1,u*1.08)))**.75*(1-.55*u);x=392+u*ch*dx;y=232+u*ch*dy
        upper.append((x*s+ox,y*s+oy));lower.append(((x+th*nx)*s+ox,(y+th*ny)*s+oy))
    g.polygon(upper+lower[::-1],TITAN_WING,.9*alpha)
    g.stroke(upper,.85,WHITE,218/255*alpha)
    la=.55*alpha*min(1,deg/4+zoom)
    if la>0:g.stroke(lower,.55,WHITE,la)

