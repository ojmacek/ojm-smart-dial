"""Small native Tk controls. No UI callback performs CAN I/O."""
import tkinter as tk
import time

RED='#FF2345'; WHITE='#F2F2F4'; MUTED='#929299'; SURFACE='#202024'

class Segmented(tk.Canvas):
    def __init__(self,parent,variable,values,command,width=360):
        super().__init__(parent,width=width,height=34,bg=SURFACE,highlightthickness=0,takefocus=1)
        self.variable=variable;self.values=tuple(values);self.command=command;self.enabled=True
        self.position=0.;self.target=0.;self.job=None;self.stamp=time.monotonic()
        self.bind('<Configure>',lambda e:self.draw());self.bind('<Button-1>',self.click)
        self.bind('<B1-Motion>',self.click)
        self.bind('<Left>',lambda e:self.step(-1));self.bind('<Right>',lambda e:self.step(1))
        self.trace=variable.trace_add('write',self.changed);self.changed()
        self.bind('<Destroy>',self.cleanup)
    def cleanup(self,event):
        if event.widget is not self:return
        if self.job:self.after_cancel(self.job);self.job=None
        self.variable.trace_remove('write',self.trace)
    def configure(self,cnf=None,**kw):
        if 'state' in kw:self.enabled=kw.pop('state')!='disabled';self.draw()
        return super().configure(cnf,**kw)
    config=configure
    def changed(self,*args):
        try:self.target=self.values.index(self.variable.get())
        except ValueError:return
        if not self.job:self.animate()
    def animate(self):
        self.job=None;now=time.monotonic();dt=min(.1,now-self.stamp);self.stamp=now
        self.position+=(self.target-self.position)*min(1,dt*24)
        if abs(self.position-self.target)<.005:self.position=float(self.target)
        self.draw()
        if self.position!=self.target:self.job=self.after(16,self.animate)
    def draw(self):
        self.delete('all');w=max(1,self.winfo_width())/len(self.values)
        self.create_rectangle(self.position*w+2,2,(self.position+1)*w-2,32,fill='#351B22' if self.enabled else '#303035',outline='')
        if self.enabled:self.create_rectangle(self.position*w+2,30,(self.position+1)*w-2,33,fill=RED,outline='')
        for i,v in enumerate(self.values):
            self.create_text((i+.5)*w,17,text=v,fill=WHITE if self.enabled else MUTED,font=('Segoe UI',10))
    def choose(self,index):
        if not self.enabled:return
        index=max(0,min(len(self.values)-1,index))
        if self.variable.get()!=self.values[index]:self.variable.set(self.values[index]);self.command(index)
    def click(self,event):self.focus_set();self.choose(int(event.x*len(self.values)/max(1,self.winfo_width())))
    def step(self,delta):self.choose(int(self.target)+delta)

class Switch(tk.Canvas):
    def __init__(self,parent,title,variable,command):
        super().__init__(parent,width=210,height=27,bg=parent.cget('bg'),highlightthickness=0,takefocus=1)
        self.variable=variable;self.command=command;self.title=title
        self.trace=variable.trace_add('write',self.draw)
        self.bind('<Button-1>',self.toggle);self.bind('<space>',self.toggle)
        self.bind('<Destroy>',lambda e:self.variable.trace_remove('write',self.trace) if e.widget is self else None)
        self.draw()
    def toggle(self,event=None):self.variable.set(not self.variable.get());self.command()
    def draw(self,*args):
        self.delete('all');on=self.variable.get();color=RED if on else '#44444C'
        self.create_line(10,13,30,13,width=16,fill=color,capstyle='round')
        x=30 if on else 10;self.create_oval(x-6,7,x+6,19,fill=WHITE,outline='')
        self.create_text(47,13,text=self.title,fill=MUTED,anchor='w',font=('Segoe UI',9))

class PageStack(tk.Frame):
    def __init__(self,parent,on_change):
        super().__init__(parent,bg=parent.cget('bg'));self.panels=[];self.current=2;self.on_change=on_change
        self.rowconfigure(0,weight=1);self.columnconfigure(0,weight=1)
    def add(self,panel,text=''):
        self.panels.append(panel);panel.grid(row=0,column=0,sticky='nsew')
        if len(self.panels)==3:self.select(2)
    def select(self,index):
        self.current=int(index);self.panels[self.current].tkraise()
        if self.on_change:self.on_change()
    def index(self,which):return self.current
