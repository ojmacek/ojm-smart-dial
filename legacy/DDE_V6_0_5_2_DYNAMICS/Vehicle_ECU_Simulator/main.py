"""Optional CAN mirror for the autonomous dial; no ECU simulation is required."""
import argparse, queue, time
from dataclasses import replace
from pathlib import Path
import sys
import tkinter as tk
from tkinter import ttk, filedialog
from serial.tools import list_ports
from PIL import Image, ImageTk
from protocol import *
from can_link import CanWorker, DemoBus, EventMailbox
from controls import Segmented
from view import DialPreview
from presentation import local_presentation
BG='#0B0B0D';PANEL='#141416';SURFACE='#202024';WHITE='#F2F2F4';MUTED='#929299';RED='#FF2345'
class App(tk.Tk):
    def __init__(self,port='COM8',demo=False):
        super().__init__();self.title('OJM Systems DDE 6.0.5 · CAN Mirror'+(' · IN-MEMORY DEMO' if demo else ''))
        self.geometry('1080x720');self.minsize(900,620);self.configure(bg=BG);self.option_add('*Font',('Segoe UI',10))
        self.demo=demo;self.events=EventMailbox();self.worker=None;self.state=DialState();self.online=False
        self.requested={};self.logs=[];self.callback_errors=[];self.syncing=False;self.advanced=False;self.last_stats={};self.rate_stats=None
        self.mode_widgets=[];self.mode_vars={};self.mode_rows=[];self.mode_labels=[];self.last_draw=0.;self.pump_job=None;self.burst_job=None;self.burst_left=0
        self.port=tk.StringVar(value=port);self.status=tk.StringVar(value='LOCAL PREVIEW');self.ack=tk.StringVar(value='—')
        self.stats=tk.StringVar(value='');self.metric_vars={k:tk.StringVar(value='—') for k in ('tx','rx')}
        self.page_var=tk.StringVar(value='AERO');self.active_page=tk.StringVar(value=PAGES[2]);self.started=time.monotonic()
        style=ttk.Style(self);style.theme_use('clam');style.configure('TCombobox',fieldbackground=SURFACE,foreground=WHITE,background=SURFACE,arrowcolor=WHITE)
        self.build_ui();self.scan_ports();self.bind('<Configure>',self.resize_layout)
        self.protocol('WM_DELETE_WINDOW',self.close);self.pump_job=self.after(16,self.pump)
    def label(self,parent,text='',var=None,size=10,color=MUTED):
        return tk.Label(parent,text=text,textvariable=var,bg=parent.cget('bg'),fg=color,font=('Segoe UI',size),anchor='w')
    def button(self,parent,text,command,accent=False):
        return tk.Button(parent,text=text,command=command,bg=RED if accent else SURFACE,fg=WHITE,activebackground=SURFACE,activeforeground=WHITE,relief='flat',bd=0,padx=14,pady=8)
    def build_ui(self):
        header=tk.Frame(self,bg=BG);header.pack(fill='x',padx=24,pady=(20,16))
        root=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parent));im=Image.open(root/'OJM_Logo.png').convert('RGBA');im.thumbnail((108,44));self.logo=ImageTk.PhotoImage(im)
        tk.Label(header,image=self.logo,bg=BG).pack(side='left',padx=(0,24));self.label(header,'VEHICLE CONTROL',size=19,color=WHITE).pack(side='left')
        self.label(header,'DYNAMICS / 6.0.5',size=9).pack(side='right')
        conn=tk.Frame(self,bg=BG);conn.pack(fill='x',padx=24,pady=(0,16))
        self.port_box=ttk.Combobox(conn,textvariable=self.port,width=10);self.port_box.pack(side='left')
        self.button(conn,'Scan',self.scan_ports).pack(side='left',padx=6)
        self.connect_button=self.button(conn,'Connect',self.connect,True);self.connect_button.pack(side='left')
        self.label(conn,var=self.status,color=WHITE).pack(side='left',padx=14)
        self.diagnostics_button=self.button(conn,'CAN log',lambda:self.set_diagnostics(not self.advanced));self.diagnostics_button.pack(side='right')
        # Only a compact footer is reserved for transport figures.
        footer=tk.Frame(self,bg=BG);footer.pack(side='bottom',fill='x',padx=24,pady=12)
        self.label(footer,'DEMO / MEMORY' if self.demo else 'CAN 500 kbit/s',size=9).pack(side='left')
        for key,title in (('tx','TX/s'),('rx','RX/s'),('ack','ACK')):
            self.label(footer,title,size=9).pack(side='left',padx=(20,5));self.label(footer,var=self.ack if key=='ack' else self.metric_vars[key],size=9,color=WHITE).pack(side='left')
        self.label(footer,'Dial ↔ PC',size=9).pack(side='right')
        self.diagnostics_footer=tk.Frame(self,bg=PANEL)
        self.label(self.diagnostics_footer,var=self.stats,size=9).pack(anchor='w',padx=10,pady=4)
        self.log=tk.Text(self.diagnostics_footer,height=3,bg=PANEL,fg=MUTED,font=('Consolas',9),relief='flat',state='disabled');self.log.pack(fill='x',padx=10)
        self.button(self.diagnostics_footer,'Save log',self.save_log).pack(anchor='e',padx=10,pady=4)
        self.body=tk.Frame(self,bg=BG);self.body.pack(fill='both',expand=True,padx=24)
        left=tk.Frame(self.body,bg=PANEL,width=398);left.pack(side='left',fill='both',padx=(0,18));left.pack_propagate(False)
        self.label(left,'DIAL VIEW',size=10).pack(anchor='w',padx=18,pady=(18,0))
        self.preview=DialPreview(left);self.preview.set_scale(.78);self.preview.pack(expand=True)
        self.mirror_status=tk.StringVar(value='Local preview · connect to synchronize')
        self.label(left,var=self.mirror_status,size=9).pack(anchor='w',padx=18,pady=(0,18))
        right=tk.Frame(self.body,bg=PANEL);right.pack(side='left',fill='both',expand=True)
        self.section_label=self.label(right,'DRIVING FUNCTIONS',size=11,color=WHITE);self.section_label.pack(anchor='w',padx=20,pady=(24,20))
        for command,title,options in ((RWS,'Rear wheel steering',RWS_MODES),(TV,'Torque vectoring',TV_MODES),(AERO,'Active aero',AERO_MODES)):
            row=tk.Frame(right,bg=PANEL);row.pack(fill='x',padx=20,pady=(0,24))
            label=self.label(row,title,size=12,color=WHITE);label.pack(anchor='w',pady=(0,10))
            self.mode_rows.append(row);self.mode_labels.append(label)
            var=tk.StringVar(value=options[self.state.value(command)]);self.mode_vars[command]=var
            segment=Segmented(row,var,options,lambda i,c=command:self.select_mode(c,i));segment.pack(fill='x');self.mode_widgets.append(segment)
        row=tk.Frame(right,bg=PANEL);row.pack(fill='x',padx=20,pady=(4,20));self.page_row=row
        self.page_label=self.label(row,'DISPLAY PAGE',size=9);self.page_label.pack(anchor='w',pady=(0,8))
        segment=Segmented(row,self.page_var,('AERO','RWS','VECTORING'),lambda i:self.send(PAGE,(2,0,1)[i]));segment.pack(fill='x');self.mode_widgets.append(segment)
    def resize_layout(self,event):
        if event.widget is self:
            factor=.44 if self.advanced else .67 if event.height<680 else .78
            if factor!=self.preview.scale_factor:self.preview.set_scale(factor)
    def set_diagnostics(self,visible):
        self.advanced=visible;self.diagnostics_button.configure(text='Hide log' if visible else 'CAN log')
        if visible:self.diagnostics_footer.pack(side='bottom',before=self.body,fill='x',padx=24,pady=8)
        else:self.diagnostics_footer.pack_forget()
        self.section_label.pack_configure(pady=(8,4) if visible else (24,20))
        for row,label in zip(self.mode_rows,self.mode_labels):
            row.pack_configure(pady=(0,4) if visible else (0,24))
            label.pack_configure(pady=(0,2) if visible else (0,10))
            label.configure(font=('Segoe UI',9 if visible else 12))
        self.page_row.pack_configure(pady=(0,4) if visible else (4,20))
        self.page_label.pack_configure(pady=(0,2) if visible else (0,8))
        self.preview.set_scale(.44 if visible else .67 if self.winfo_height()<680 else .78)
    def report_callback_exception(self,exc,val,tb):self.callback_errors.append(str(val));self.add_log('error',str(val),time.time())
    def scan_ports(self):self.port_box.configure(values=[p.device for p in list_ports.comports()])
    def connect(self):
        if self.worker:
            self.worker.stop();self.online=False;self.connect_button.configure(text='Closing…',state='disabled');return
        self.requested.clear();self.events=EventMailbox()
        self.worker=CanWorker(self.port.get().strip(),self.events,DemoBus if self.demo else None,simulation=False)
        self.worker.start();self.connect_button.configure(text='Disconnect');self.port_box.configure(state='disabled')
    def select_mode(self,command,value):
        self.send(PAGE,command-RWS);self.send(command,value)
    def send(self,command,value):
        if self.syncing or not valid_value(command,value):return
        if self.worker and self.online:
            if self.worker.submit(command,value):self.requested[command]=value
        else:
            self.state=replace(self.state,**{{PAGE:'page',RWS:'rws',TV:'tv',AERO:'aero'}[command]:value})
        self.sync_state()
    def sync_state(self):
        self.syncing=True
        try:
            for command,var in self.mode_vars.items():var.set(MODES[command][self.requested.get(command,self.state.value(command))])
            page=self.requested.get(PAGE,self.state.page);self.page_var.set(('RWS','VECTORING','AERO')[page]);self.active_page.set(PAGES[page])
        finally:self.syncing=False
    def add_log(self,kind,text,stamp):
        line=time.strftime('%H:%M:%S',time.localtime(stamp))+f'  {kind.upper()}  {text}';self.logs.append(line);self.logs=self.logs[-2000:]
        self.log.configure(state='normal');self.log.insert('end',line+'\n')
        if int(self.log.index('end-1c').split('.')[0])>300:self.log.delete('1.0','2.0')
        self.log.see('end-2c');self.log.configure(state='disabled')
    def update_metrics(self,payload):
        now=time.monotonic();self.last_stats=payload
        self.stats.set('   '.join(f'{k.upper()} {payload[k]}' for k in ('rx','tx','retry','superseded','missed')))
        if self.online and self.rate_stats:
            at,previous=self.rate_stats;dt=now-at
            if dt>=.45:
                for key in ('rx','tx'):self.metric_vars[key].set(f'{max(0,payload[key]-previous[key])/dt:.0f}')
                self.rate_stats=(now,payload.copy())
        elif self.online:self.rate_stats=(now,payload.copy())
    def pump(self):
        start=time.monotonic();latest=None
        for _ in range(200):
            if time.monotonic()-start>.006:break
            try:kind,payload,stamp=self.events.get_nowait()
            except queue.Empty:break
            if kind=='state':latest=payload
            elif kind=='connection':
                self.online=payload=='ONLINE';self.status.set(('DEMO · ' if self.demo else '')+payload)
                self.mirror_status.set('Modes synchronized · animation runs on dial' if self.online else 'Local preview · connect to synchronize')
                if not self.online:
                    self.requested.clear();latest=None;self.ack.set('—');self.rate_stats=None
                    for var in self.metric_vars.values():var.set('—')
                    self.sync_state()
                self.add_log(kind,payload,stamp)
            elif kind=='stopped':self.worker=None;self.connect_button.configure(text='Connect',state='normal');self.port_box.configure(state='normal')
            elif kind=='stats':self.update_metrics(payload)
            elif kind in ('confirmed','failed'):
                command,value=payload[:2]
                if self.requested.get(command)==value:self.requested.pop(command)
                if kind=='confirmed':self.ack.set(f'{payload[2]:.1f} ms');self.add_log(kind,payload[3],stamp)
                else:self.add_log(kind,payload[2],stamp)
            elif kind not in ('inputs','telemetry'):self.add_log(kind,str(payload),stamp)
        if latest is not None:self.state=latest;self.sync_state()
        now=time.monotonic()
        if now-self.last_draw>=1/60:
            values={{PAGE:'page',RWS:'rws',TV:'tv',AERO:'aero'}[c]:v for c,v in self.requested.items()}
            shown=replace(self.state,**values);self.preview.draw(shown,local_presentation(shown,now-self.started),now,True);self.last_draw=now
        self.pump_job=self.after(16,self.pump)
    def save_log(self):
        path=filedialog.asksaveasfilename(defaultextension='.log',initialfile='OJM_DDE_CAN.log')
        if path:Path(path).write_text('\n'.join(self.logs),encoding='utf-8')
    def close(self):
        if self.worker:self.worker.stop()
        if self.pump_job:self.after_cancel(self.pump_job)
        self.destroy()
if __name__=='__main__':
    parser=argparse.ArgumentParser(description='Optional CAN mirror for the autonomous OJM dial')
    parser.add_argument('--port',default='COM8');parser.add_argument('--demo',action='store_true')
    args=parser.parse_args();App(args.port,args.demo).mainloop()
