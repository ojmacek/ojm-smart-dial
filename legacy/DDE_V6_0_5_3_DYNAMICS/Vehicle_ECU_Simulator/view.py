"""Local illustrative mirror; modes come over CAN, animation runs independently."""
from pathlib import Path
import math, sys, json
import tkinter as tk
from PIL import Image, ImageTk
from protocol import *
RED='#FF2345';WHITE='#F2F2F4';DIM='#55555B'
def ease(t):
    t=max(0,min(1,t));return t*t*(3-2*t)
def mix(a,b,t):
    t=max(0,min(1,t));return '#'+''.join(f'{round(int(a[i:i+2],16)*(1-t)+int(b[i:i+2],16)*t):02x}' for i in (1,3,5))
def settle(value,velocity,target,dt,frequency):
    steps=max(1,math.ceil(dt/.008));h=dt/steps
    for _ in range(steps):
        velocity+=(frequency*frequency*(target-value)-1.64*frequency*velocity)*h;value+=velocity*h
    return value,velocity
class DialPreview(tk.Canvas):
    def __init__(self,parent):
        super().__init__(parent,width=308,height=308,bg='#000000',highlightthickness=0)
        self.scale_factor=.66;self.page=-1;self.angle=[0.,0.];self.velocity=[0.,0.,0.];self.aero=0.
        self.torque=[0.]*4;self.last_draw=None;self.strength=0.;self.phase=0.;self.selection=None;self.changed=0.;self.rail=211.;self.rail_velocity=0.
        root=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parents[1]))
        self.paths=json.loads((root/'assets/preview/titan_paths.json').read_text())
        self.top=Image.open(root/'assets/preview/titan_top_minimal.png').convert('RGB')
        self.set_scale(.66)
    def set_scale(self,factor):
        self.scale_factor=factor
        self.photo=ImageTk.PhotoImage(self.top.resize((round(100*factor),round(198*factor)),Image.Resampling.LANCZOS),master=self)
        self.configure(width=round(466*factor),height=round(466*factor));self.page=-1
    def line(self,*coords,tag='live',**kw):
        self.create_line(*(v*self.scale_factor for v in coords),tags=tag,capstyle='round',**kw)
    def text(self,x,y,text,size=12,fill=WHITE,tag='live'):
        self.create_text(x*self.scale_factor,y*self.scale_factor,text=text,fill=fill,font=('Segoe UI',max(8,round(size*self.scale_factor))),tags=tag)
    def dot(self,x,y,r,color):
        s=self.scale_factor;self.create_oval((x-r)*s,(y-r)*s,(x+r)*s,(y+r)*s,fill=color,outline='',tags='live')
    def draw(self,state,t,now,online):
        s=self.scale_factor;page=state.page
        if page!=self.page:
            self.page=page;self.delete('all')
            if page==2:
                for path in self.paths:self.line(*(v for point in path for v in point),tag='body',fill='#D0D0D4',width=max(1,1.7*s))
            else:self.create_image(233*s,213*s,image=self.photo,tags='body')
        self.delete('live');dt=min(.05,max(0,now-self.last_draw)) if self.last_draw is not None else 1/60;self.last_draw=now
        k=1-math.exp(-dt/.12);mode_index=(state.rws,state.tv,state.aero)[page]
        selection=(page,mode_index)
        if selection!=self.selection:self.selection=selection;self.changed=now
        age=now-self.changed
        self.strength+=((0,.32,.68,1)[state.tv]-self.strength)*(1-math.exp(-dt/.18))
        self.phase=(self.phase+dt/(1.45-.65*self.strength))%1
        self.angle[0],self.velocity[0]=settle(self.angle[0],self.velocity[0],t.front,dt,19)
        self.angle[1],self.velocity[1]=settle(self.angle[1],self.velocity[1],t.rear_actual,dt,23 if state.rws==2 else 17)
        self.aero,self.velocity[2]=settle(self.aero,self.velocity[2],t.aero_actual,dt,16)
        self.text(233,86,('REAR WHEEL STEERING','TORQUE VECTORING','ACTIVE AERO')[page],22 if page==0 else 24)
        self.text(233,349,(RWS_MODES[state.rws],TV_MODES[state.tv],AERO_MODES[state.aero])[page],40)
        # This is a PC interaction cue, not the dial's private browse/edit state.
        edit=ease(age/.18)*(1-ease((age-1.0)/.18))
        from ui_order import mode_position
        target=211+22*((page+1)%3);target+=(200+22*mode_position(RWS+page,mode_index)-target)*edit
        self.rail,self.rail_velocity=settle(self.rail,self.rail_velocity,target,dt,26)
        for i in range(4):
            x=211+22*i-11*edit;y=399-5*edit
            self.line(x-3,y,x+3,y,fill=mix('#000000',DIM,edit if i==3 else 1),width=max(1,2.5*s))
        self.line(self.rail-4,y,self.rail+4,y,fill=RED,width=max(1,3.2*s));self.dot(self.rail,y,2,RED)
        if page<2:
            for i,(x,y) in enumerate(((163,166),(303,166),(163,274),(303,274))):
                if page==0:
                    a=math.radians(self.angle[0 if i<2 else 1]);sx=math.sin(a)*11;sy=math.cos(a)*11
                    color=WHITE if i<2 else mix(WHITE,RED,220/255*min(1,abs(self.angle[1])/3))
                    self.line(x-sx,y+sy,x+sx,y-sy,fill=color,width=max(2,5.4*s))
                    cue=1-ease(age/.22)
                    if cue>0 and abs(a)>.02:
                        side=1 if i%2 else -1
                        coords=[v for n in range(8) for v in (x+side*(13+3*math.sin(n/7*math.pi)),y-12+24*n/7)]
                        self.line(*coords,fill=mix('#000000',RED,.55*cue),width=max(1,1.3*s))
                else:
                    self.torque[i]+=((t.fl,t.fr,t.rl,t.rr)[i]-self.torque[i])*k;load=max(0,min(1,self.torque[i]/100));strength=self.strength
                    p=(self.phase+(.18 if i>=2 else 0))%1
                    arrival=ease((p-.68)/.12)*(1-ease((p-.80)/.20))
                    color=mix(DIM,RED,strength*(.35+.65*arrival)*(.55+.45*load))
                    self.line(x,y-11,x,y+11,fill=color,width=max(2,5.4*s))
                    if arrival>0:self.line(x+(6 if i%2 else -6),y-12,x+(6 if i%2 else -6),y+12,fill=mix('#000000',RED,.59*arrival*strength*load),width=max(1,1.3*s))
                    if i%2==0:
                        emit=(1-ease(p/.20))*strength;self.dot(233,y,.8+1.4*emit,mix('#000000',RED,.7*emit))
                    for echo in range(2 if strength>.68 else 1):
                        pos=(p-echo*.115)/.78
                        if not 0<pos<1:continue
                        head=ease(pos);tail=max(0,head-(.14+.16*strength)*(.6+.4*load));side=1 if i%2 else -1
                        alpha=strength*(.4+.6*load)*ease(pos/.10)*(1-ease((pos-.8)/.2))*(.38*ease((strength-.68)/.32) if echo else 1)
                        self.line(233+side*(4+24*tail),y,233+side*(4+24*head),y,fill=mix('#000000',RED,alpha),width=max(1,(1.6+strength)*s))
        else:
            angle=max(0,min(1,(self.aero-5)/70))*25 if self.aero<=75 else 25+(self.aero-75)/25*33
            a=math.radians(angle);self.line(392,232,392+19*math.cos(a),232-19*math.sin(a),fill=RED,width=max(1.5,2.5*s));self.dot(392,232,1.45,RED)
            phase=((age-.7)%3.2)/.9
            if age>=.7 and phase<1:
                bend=max(0,min(1,(self.aero-5)/95));points=[(66,240),(110,218),(154,201),(195,189),(233,185),(278,188),(325,199),(369,214),(407,219),(435,219+22*bend)]
                head=phase*9;tail=max(0,head-1.15);envelope=ease(phase/.15)*(1-ease((phase-.78)/.22))
                for i,(a,b) in enumerate(zip(points,points[1:])):
                    lo=max(tail,i)-i;hi=min(head,i+1)-i
                    if hi<=lo:continue
                    self.line(a[0]+(b[0]-a[0])*lo,a[1]+(b[1]-a[1])*lo,a[0]+(b[0]-a[0])*hi,a[1]+(b[1]-a[1])*hi,fill=mix('#000000',RED,.61*envelope),width=max(1,1.3*s))
                    if i==8 and bend>.85:self.line(a[0]+(b[0]-a[0])*lo,a[1]-16*lo,a[0]+(b[0]-a[0])*hi,a[1]-16*hi,fill=mix('#000000',RED,.37*envelope),width=max(1,1.1*s))
