"""Exercise and capture only the optional CAN mirror window, with no real bus."""
from pathlib import Path
import sys, argparse, json, time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Vehicle_ECU_Simulator'))
from main import App
from protocol import *
def run(capture=False):
    root=Path(__file__).resolve().parents[1];app=App(demo=True);report=[];worker=None
    if not capture:app.withdraw()
    def wait(predicate,timeout=3):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            app.update()
            if predicate():return
            time.sleep(.005)
        raise AssertionError('GUI timeout')
    def snapshot(name):
        if capture:
            from PIL import ImageGrab
            ImageGrab.grab(window=int(app.frame(),0)).save(root/'validation'/name)
    try:
        app.select_mode(AERO,2);end=time.monotonic()+.6;wait(lambda:time.monotonic()>=end);snapshot('gui_standalone.png')
        app.connect();worker=app.worker;wait(lambda:app.online)
        for page,mode in ((0,2),(1,3),(2,3)):
            app.select_mode(RWS+page,mode);wait(lambda:app.state.page==page and not app.requested)
            end=time.monotonic()+.7;wait(lambda:time.monotonic()>=end);snapshot(f'gui_{page}.png')
            report.append({'page':PAGES[page],'mode':mode,'ack':app.ack.get(),'simulation_enabled':worker.simulation,'callback_errors':list(app.callback_errors)})
        app.geometry('900x620');app.update();snapshot('gui_small.png')
        app.set_diagnostics(True);end=time.monotonic()+.2;wait(lambda:time.monotonic()>=end);snapshot('gui_diagnostics.png')
        for widget in app.mode_widgets:
            assert widget.winfo_height()>=34, 'compressed control'
            assert widget.winfo_rooty()+widget.winfo_height()<=app.body.winfo_rooty()+app.body.winfo_height(), 'clipped control'
        assert not app.callback_errors,app.callback_errors
        report.append({'minimum_window':[app.winfo_width(),app.winfo_height()],'diagnostics_visible':bool(app.diagnostics_footer.winfo_ismapped()),'callback_errors':app.callback_errors})
        (root/'validation/gui_smoke.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
    finally:
        app.close()
        if worker:worker.join(2)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--capture',action='store_true');run(parser.parse_args().capture)
