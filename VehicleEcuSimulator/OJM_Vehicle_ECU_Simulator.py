from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from pathlib import Path
from tkinter import messagebox, ttk

try:
    import can
except ModuleNotFoundError:
    raise SystemExit(
        'Missing dependency. Run: py -m pip install "python-can[serial]"'
    )


APP_TITLE = "OJM Systems · Vehicle ECU Simulator"
CAN_BITRATE = 500_000
CAN_STATE_ID = 0x201
CAN_COMMAND_ID = 0x301
OFFLINE_TIMEOUT_S = 0.80
ACK_TIMEOUT_S = 0.45
MAX_COMMAND_ATTEMPTS = 3

CMD_TEMPERATURE = 0x01
CMD_FAN = 0x02
CMD_AIRFLOW = 0x03
CMD_SEAT = 0x04
CMD_AC = 0x05
CMD_POWER = 0x06

AIRFLOW_NAMES = ("AUTO", "FACE", "FEET", "SCREEN")
PAGE_NAMES = ("TEMPERATURE", "FAN SPEED", "AIRFLOW", "SEAT HEAT")

BG = "#090B0F"
PANEL = "#11151B"
PANEL_ALT = "#171C24"
EDGE = "#282F3A"
TEXT = "#F5F7FA"
MUTED = "#8F98A6"
BLUE = "#1687FF"
RED = "#FF244D"
GREEN = "#2AD37F"
AMBER = "#FFB020"


def resource_path(filename: str) -> Path:
    """Resolve data files both from source and from a PyInstaller bundle."""
    bundle_root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return bundle_root / filename

def xor_checksum(data: bytes) -> int:
    value = 0
    for byte in data:
        value ^= byte
    return value


@dataclass(frozen=True)
class DialState:
    power: bool
    ac: bool
    temperature: float
    fan_raw: int
    airflow: int
    seat: int
    page: int
    alive: int
    checksum_ok: bool
    raw: bytes
    received_at: float

    @property
    def fan_text(self) -> str:
        if self.fan_raw == 0xFF:
            return "AUTO"
        if self.fan_raw == 0:
            return "OFF"
        return str(self.fan_raw)

    @property
    def airflow_text(self) -> str:
        if 0 <= self.airflow < len(AIRFLOW_NAMES):
            return AIRFLOW_NAMES[self.airflow]
        return f"UNKNOWN ({self.airflow})"

    @property
    def page_text(self) -> str:
        if 0 <= self.page < len(PAGE_NAMES):
            return PAGE_NAMES[self.page]
        return f"UNKNOWN ({self.page})"


def decode_state(message: can.Message) -> DialState | None:
    if message.is_extended_id or message.is_remote_frame:
        return None
    if message.arbitration_id != CAN_STATE_ID or len(message.data) != 8:
        return None

    data = bytes(message.data)
    return DialState(
        power=bool(data[0] & 0x01),
        ac=bool(data[0] & 0x02),
        temperature=data[1] * 0.5,
        fan_raw=data[2],
        airflow=data[3],
        seat=data[4],
        page=data[5],
        alive=data[6] & 0x0F,
        checksum_ok=xor_checksum(data[:7]) == data[7],
        raw=data,
        received_at=time.monotonic(),
    )


@dataclass
class PendingCommand:
    command: int
    value: int
    description: str
    attempts: int = 0
    last_sent_at: float = 0.0

    def is_confirmed_by(self, state: DialState) -> bool:
        if self.command == CMD_TEMPERATURE:
            return state.raw[1] == self.value
        if self.command == CMD_FAN:
            return state.raw[2] == self.value
        if self.command == CMD_AIRFLOW:
            return state.raw[3] == self.value
        if self.command == CMD_SEAT:
            return state.raw[4] == self.value
        if self.command == CMD_AC:
            return int(state.ac) == self.value
        if self.command == CMD_POWER:
            return int(state.power) == self.value
        return False


class CanWorker(threading.Thread):
    def __init__(self, port: str, events: queue.Queue):
        super().__init__(daemon=True, name="ojm-can-worker")
        self.port = port
        self.events = events
        self.commands: queue.Queue[PendingCommand] = queue.Queue()
        self.stop_event = threading.Event()
        self.bus: can.BusABC | None = None

    def stop(self) -> None:
        self.stop_event.set()

    def submit(self, command: PendingCommand) -> None:
        self.commands.put(command)

    def emit(self, kind: str, payload=None) -> None:
        self.events.put((kind, payload))

    def send_pending(self, pending: PendingCommand) -> bool:
        if self.bus is None:
            return False

        message = can.Message(
            arbitration_id=CAN_COMMAND_ID,
            is_extended_id=False,
            data=[pending.command, pending.value],
        )
        try:
            self.bus.send(message, timeout=0.20)
        except can.CanError as error:
            self.emit("log", f"TX error: {error}")
            return False

        pending.attempts += 1
        pending.last_sent_at = time.monotonic()
        self.emit(
            "log",
            f"TX 301  [{pending.command:02X} {pending.value:02X}]  "
            f"{pending.description}  attempt {pending.attempts}",
        )
        return True

    def run(self) -> None:
        online = False
        last_valid_state_at = 0.0
        pending: PendingCommand | None = None

        try:
            self.emit("connection", "OPENING")
            self.bus = can.Bus(
                interface="slcan",
                channel=self.port,
                bitrate=CAN_BITRATE,
                sleep_after_open=2.0,
                timeout=0.01,
            )
            self.emit("connection", "WAITING")
            self.emit("log", f"SLCAN opened on {self.port} at 500 kbit/s")

            while not self.stop_event.is_set():
                now = time.monotonic()

                if pending is None:
                    try:
                        pending = self.commands.get_nowait()
                    except queue.Empty:
                        pass

                if pending is not None and online:
                    should_send = pending.attempts == 0
                    should_retry = (
                        pending.attempts > 0
                        and now - pending.last_sent_at >= ACK_TIMEOUT_S
                    )

                    if should_send or should_retry:
                        if pending.attempts >= MAX_COMMAND_ATTEMPTS:
                            self.emit(
                                "command_failed",
                                f"No confirmation: {pending.description}",
                            )
                            pending = None
                        else:
                            self.send_pending(pending)

                try:
                    message = self.bus.recv(timeout=0.05)
                except can.CanError as error:
                    self.emit("error", f"CAN receive error: {error}")
                    break

                if message is not None:
                    state = decode_state(message)
                    if state is not None:
                        if not state.checksum_ok:
                            self.emit("bad_checksum", state)
                        else:
                            last_valid_state_at = state.received_at
                            if not online:
                                online = True
                                self.emit("connection", "ONLINE")
                                self.emit("log", "Dial detected on CAN ID 0x201")
                            self.emit("state", state)

                            if pending is not None and pending.is_confirmed_by(state):
                                self.emit(
                                    "command_confirmed",
                                    f"Confirmed: {pending.description}",
                                )
                                pending = None

                if online and time.monotonic() - last_valid_state_at > OFFLINE_TIMEOUT_S:
                    online = False
                    self.emit("connection", "OFFLINE")
                    self.emit("log", "Dial timeout: no valid 0x201 state frame")

        except (can.CanError, OSError, ValueError) as error:
            hint = " Close Cangaroo first." if "access" in str(error).lower() else ""
            self.emit("error", f"Could not open {self.port}: {error}.{hint}")
        finally:
            if self.bus is not None:
                try:
                    self.bus.shutdown()
                except Exception:
                    pass
            self.bus = None
            self.emit("connection", "DISCONNECTED")
            self.emit("stopped")


class VehicleEcuSimulator(tk.Tk):
    def __init__(self, default_port: str, show_intro: bool = True):
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("1080x780")
        self.minsize(960, 700)
        self.configure(bg=BG)

        self.events: queue.Queue = queue.Queue()
        self.worker: CanWorker | None = None
        self.latest_state: DialState | None = None
        self.last_alive: int | None = None
        self.missed_frames = 0
        self.bad_checksums = 0
        self.command_widgets: list[tk.Widget] = []
        self.intro_canvas: tk.Canvas | None = None
        self.intro_started_at = 0.0

        self.logo_image = tk.PhotoImage(file=str(resource_path("OJM_Logo.png")))
        self.splash_image = tk.PhotoImage(file=str(resource_path("OJM_Splash.png"))).subsample(2, 2)
        self.iconphoto(True, self.logo_image)

        self.port_var = tk.StringVar(value=default_port)
        self.connection_var = tk.StringVar(value="DISCONNECTED")
        self.connection_detail_var = tk.StringVar(value="CAN interface is closed")
        self.power_var = tk.StringVar(value="—")
        self.ac_var = tk.StringVar(value="—")
        self.temperature_var = tk.StringVar(value="—")
        self.fan_var = tk.StringVar(value="—")
        self.airflow_var = tk.StringVar(value="—")
        self.seat_var = tk.StringVar(value="—")
        self.page_var = tk.StringVar(value="—")
        self.alive_var = tk.StringVar(value="—")
        self.integrity_var = tk.StringVar(value="WAITING")
        self.raw_var = tk.StringVar(value="-- -- -- -- -- -- -- --")
        self.temp_command_var = tk.StringVar(value="22.0")
        self.fan_command_var = tk.StringVar(value="AUTO")
        self.airflow_command_var = tk.StringVar(value="AUTO")
        self.seat_command_var = tk.StringVar(value="0")

        self.build_styles()
        self.build_ui()
        self.set_commands_enabled(False)

        self.after(50, self.process_events)
        self.after(100, self.update_age)
        if show_intro:
            self.after(120, self.start_intro)
        self.protocol("WM_DELETE_WINDOW", self.close_app)

    def build_styles(self) -> None:
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure(
            "OJM.TCombobox",
            fieldbackground=PANEL_ALT,
            background=PANEL_ALT,
            foreground=TEXT,
            arrowcolor=TEXT,
            bordercolor=EDGE,
            lightcolor=EDGE,
            darkcolor=EDGE,
            padding=8,
        )
        style.map(
            "OJM.TCombobox",
            fieldbackground=[("readonly", PANEL_ALT), ("disabled", PANEL)],
            foreground=[("readonly", TEXT), ("disabled", MUTED)],
        )

    def build_ui(self) -> None:
        header = tk.Frame(self, bg=BG)
        header.pack(fill="x", padx=28, pady=(24, 18))

        tk.Label(
            header,
            image=self.logo_image,
            bg=BG,
            borderwidth=0,
        ).pack(side="left", padx=(0, 22))

        tk.Frame(header, bg=EDGE, width=1, height=58).pack(
            side="left", padx=(0, 22)
        )

        title_box = tk.Frame(header, bg=BG)
        title_box.pack(side="left")
        tk.Label(
            title_box,
            text="SMART DIAL CAN BENCH",
            bg=BG,
            fg=MUTED,
            font=("Segoe UI Semibold", 10),
        ).pack(anchor="w")
        tk.Label(
            title_box,
            text="VEHICLE ECU SIMULATOR",
            bg=BG,
            fg=TEXT,
            font=("Segoe UI Semibold", 25),
        ).pack(anchor="w")

        connect_box = tk.Frame(header, bg=BG)
        connect_box.pack(side="right", anchor="e")
        tk.Label(
            connect_box,
            text="SLCAN PORT",
            bg=BG,
            fg=MUTED,
            font=("Segoe UI Semibold", 9),
        ).grid(row=0, column=0, sticky="w", pady=(0, 5))

        self.port_entry = tk.Entry(
            connect_box,
            textvariable=self.port_var,
            width=10,
            bg=PANEL_ALT,
            fg=TEXT,
            insertbackground=TEXT,
            relief="flat",
            font=("Consolas", 11),
        )
        self.port_entry.grid(row=1, column=0, ipady=9, padx=(0, 8))

        self.connect_button = self.make_button(
            connect_box,
            "CONNECT",
            self.toggle_connection,
            BLUE,
            width=13,
        )
        self.connect_button.grid(row=1, column=1)

        status_panel = tk.Frame(self, bg=PANEL, highlightthickness=1, highlightbackground=EDGE)
        status_panel.pack(fill="x", padx=28, pady=(0, 16))

        self.status_label = tk.Label(
            status_panel,
            textvariable=self.connection_var,
            bg=PANEL,
            fg=MUTED,
            font=("Segoe UI Semibold", 15),
            padx=18,
            pady=15,
        )
        self.status_label.pack(side="left")
        tk.Label(
            status_panel,
            textvariable=self.connection_detail_var,
            bg=PANEL,
            fg=MUTED,
            font=("Segoe UI", 10),
            padx=4,
        ).pack(side="left")
        tk.Label(
            status_panel,
            text="STATE 0x201  ·  COMMAND 0x301  ·  500 KBIT/S",
            bg=PANEL,
            fg=MUTED,
            font=("Consolas", 10),
            padx=18,
        ).pack(side="right")

        content = tk.Frame(self, bg=BG)
        content.pack(fill="both", expand=True, padx=28)
        content.grid_columnconfigure(0, weight=3)
        content.grid_columnconfigure(1, weight=2)
        content.grid_rowconfigure(0, weight=1)

        state_panel = self.section(content, "LIVE DIAL STATE")
        state_panel.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        command_panel = self.section(content, "REMOTE COMMANDS")
        command_panel.grid(row=0, column=1, sticky="nsew", padx=(8, 0))

        cards = tk.Frame(state_panel, bg=PANEL)
        cards.pack(fill="x", padx=16, pady=(2, 14))
        for column in range(3):
            cards.grid_columnconfigure(column, weight=1)

        self.metric(cards, 0, 0, "TEMPERATURE", self.temperature_var, BLUE)
        self.metric(cards, 0, 1, "FAN", self.fan_var, TEXT)
        self.metric(cards, 0, 2, "AIRFLOW", self.airflow_var, TEXT)
        self.metric(cards, 1, 0, "SEAT HEAT", self.seat_var, RED)
        self.metric(cards, 1, 1, "A/C", self.ac_var, RED)
        self.metric(cards, 1, 2, "POWER", self.power_var, GREEN)

        details = tk.Frame(state_panel, bg=PANEL_ALT)
        details.pack(fill="x", padx=16, pady=(0, 12))
        self.detail_row(details, 0, "VISIBLE PAGE", self.page_var)
        self.detail_row(details, 1, "ALIVE COUNTER", self.alive_var)
        self.detail_row(details, 2, "FRAME INTEGRITY", self.integrity_var)
        self.detail_row(details, 3, "RAW 0x201", self.raw_var, monospace=True)

        self.build_commands(command_panel)

        log_panel = self.section(self, "EVENT LOG")
        log_panel.pack(fill="both", padx=28, pady=(16, 24))
        self.log_text = tk.Text(
            log_panel,
            height=7,
            bg="#0C0F14",
            fg="#C9D1DC",
            insertbackground=TEXT,
            relief="flat",
            borderwidth=0,
            font=("Consolas", 9),
            state="disabled",
            padx=12,
            pady=8,
        )
        self.log_text.pack(fill="both", expand=True, padx=16, pady=(0, 14))

    def start_intro(self) -> None:
        self.intro_canvas = tk.Canvas(
            self,
            bg="#000000",
            highlightthickness=0,
            borderwidth=0,
        )
        self.intro_canvas.place(x=0, y=0, relwidth=1, relheight=1)
        self.update_idletasks()

        width = self.intro_canvas.winfo_width()
        height = self.intro_canvas.winfo_height()
        self.intro_canvas.create_image(
            width // 2,
            height // 2,
            image=self.splash_image,
        )
        self.bind("<Escape>", lambda _event: self.finish_intro())
        self.after(3000, self.finish_intro)

    def finish_intro(self) -> None:
        if self.intro_canvas is not None:
            self.intro_canvas.destroy()
            self.intro_canvas = None
        self.unbind("<Escape>")

    def section(self, parent: tk.Widget, title: str) -> tk.Frame:
        frame = tk.Frame(parent, bg=PANEL, highlightthickness=1, highlightbackground=EDGE)
        tk.Label(
            frame,
            text=title,
            bg=PANEL,
            fg=MUTED,
            font=("Segoe UI Semibold", 10),
        ).pack(anchor="w", padx=16, pady=(14, 10))
        return frame

    def metric(
        self,
        parent: tk.Widget,
        row: int,
        column: int,
        caption: str,
        variable: tk.StringVar,
        accent: str,
    ) -> None:
        frame = tk.Frame(parent, bg=PANEL_ALT, highlightthickness=1, highlightbackground=EDGE)
        frame.grid(row=row, column=column, sticky="nsew", padx=4, pady=4)
        tk.Frame(frame, bg=accent, height=3).pack(fill="x")
        tk.Label(
            frame,
            text=caption,
            bg=PANEL_ALT,
            fg=MUTED,
            font=("Segoe UI Semibold", 8),
        ).pack(anchor="w", padx=12, pady=(10, 2))
        tk.Label(
            frame,
            textvariable=variable,
            bg=PANEL_ALT,
            fg=TEXT,
            font=("Segoe UI Semibold", 20),
        ).pack(anchor="w", padx=12, pady=(0, 12))

    def detail_row(
        self,
        parent: tk.Widget,
        row: int,
        caption: str,
        variable: tk.StringVar,
        monospace: bool = False,
    ) -> None:
        parent.grid_columnconfigure(1, weight=1)
        tk.Label(
            parent,
            text=caption,
            bg=PANEL_ALT,
            fg=MUTED,
            font=("Segoe UI Semibold", 8),
        ).grid(row=row, column=0, sticky="w", padx=12, pady=7)
        tk.Label(
            parent,
            textvariable=variable,
            bg=PANEL_ALT,
            fg=TEXT,
            font=(("Consolas", 10) if monospace else ("Segoe UI Semibold", 10)),
        ).grid(row=row, column=1, sticky="e", padx=12, pady=7)

    def build_commands(self, panel: tk.Frame) -> None:
        body = tk.Frame(panel, bg=PANEL)
        body.pack(fill="both", expand=True, padx=16, pady=(2, 14))
        body.grid_columnconfigure(0, weight=1)
        body.grid_columnconfigure(1, weight=1)

        tk.Label(body, text="TEMPERATURE", bg=PANEL, fg=MUTED, font=("Segoe UI Semibold", 8)).grid(
            row=0, column=0, columnspan=2, sticky="w", pady=(0, 5)
        )
        temp_spin = tk.Spinbox(
            body,
            from_=16.0,
            to=30.0,
            increment=0.5,
            textvariable=self.temp_command_var,
            format="%.1f",
            bg=PANEL_ALT,
            fg=TEXT,
            buttonbackground=PANEL_ALT,
            insertbackground=TEXT,
            relief="flat",
            font=("Segoe UI Semibold", 11),
        )
        temp_spin.grid(row=1, column=0, sticky="ew", ipady=8, padx=(0, 5))
        temp_send = self.make_button(body, "APPLY", self.send_temperature, BLUE)
        temp_send.grid(row=1, column=1, sticky="ew", padx=(5, 0))

        fan_combo = self.command_combo(body, 2, "FAN SPEED", self.fan_command_var, ["AUTO", "OFF"] + [str(i) for i in range(1, 9)])
        fan_send = self.make_button(body, "SEND FAN", self.send_fan, TEXT, dark_text=True)
        fan_send.grid(row=3, column=1, sticky="ew", padx=(5, 0))

        airflow_combo = self.command_combo(body, 4, "AIRFLOW", self.airflow_command_var, list(AIRFLOW_NAMES))
        airflow_send = self.make_button(body, "SEND AIRFLOW", self.send_airflow, TEXT, dark_text=True)
        airflow_send.grid(row=5, column=1, sticky="ew", padx=(5, 0))

        seat_combo = self.command_combo(body, 6, "SEAT HEAT", self.seat_command_var, ["0", "1", "2", "3"])
        seat_send = self.make_button(body, "SEND HEAT", self.send_seat, RED)
        seat_send.grid(row=7, column=1, sticky="ew", padx=(5, 0))

        divider = tk.Frame(body, bg=EDGE, height=1)
        divider.grid(row=8, column=0, columnspan=2, sticky="ew", pady=14)

        self.ac_button = self.make_button(body, "TOGGLE A/C", self.toggle_ac, RED)
        self.ac_button.grid(row=9, column=0, sticky="ew", padx=(0, 5))
        self.power_button = self.make_button(body, "TOGGLE POWER", self.toggle_power, PANEL_ALT)
        self.power_button.grid(row=9, column=1, sticky="ew", padx=(5, 0))

        self.command_widgets.extend(
            [
                temp_spin,
                temp_send,
                fan_combo,
                fan_send,
                airflow_combo,
                airflow_send,
                seat_combo,
                seat_send,
                self.ac_button,
                self.power_button,
            ]
        )

    def command_combo(
        self,
        parent: tk.Widget,
        label_row: int,
        caption: str,
        variable: tk.StringVar,
        values: list[str],
    ) -> ttk.Combobox:
        tk.Label(
            parent,
            text=caption,
            bg=PANEL,
            fg=MUTED,
            font=("Segoe UI Semibold", 8),
        ).grid(row=label_row, column=0, columnspan=2, sticky="w", pady=(14, 5))
        combo = ttk.Combobox(
            parent,
            textvariable=variable,
            values=values,
            state="readonly",
            style="OJM.TCombobox",
        )
        combo.grid(row=label_row + 1, column=0, sticky="ew", padx=(0, 5))
        return combo

    def make_button(
        self,
        parent: tk.Widget,
        text: str,
        command,
        color: str,
        width: int | None = None,
        dark_text: bool = False,
    ) -> tk.Button:
        return tk.Button(
            parent,
            text=text,
            command=command,
            width=width,
            bg=color,
            fg=(BG if dark_text else TEXT),
            activebackground=color,
            activeforeground=(BG if dark_text else TEXT),
            disabledforeground=MUTED,
            relief="flat",
            borderwidth=0,
            cursor="hand2",
            font=("Segoe UI Semibold", 9),
            padx=12,
            pady=10,
        )

    def toggle_connection(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        port = self.port_var.get().strip().upper()
        if not port:
            messagebox.showerror(APP_TITLE, "Enter a serial port, for example COM5.")
            return

        self.latest_state = None
        self.last_alive = None
        self.missed_frames = 0
        self.bad_checksums = 0
        self.set_commands_enabled(False)
        self.port_entry.configure(state="disabled")
        self.connect_button.configure(text="DISCONNECT", bg=RED, activebackground=RED)
        self.worker = CanWorker(port, self.events)
        self.worker.start()

    def disconnect(self) -> None:
        if self.worker is not None:
            self.worker.stop()
        self.set_commands_enabled(False)

    def submit_command(self, command: int, value: int, description: str) -> None:
        if self.worker is None or not self.worker.is_alive() or self.latest_state is None:
            self.append_log("Command blocked: dial is not online")
            return
        self.worker.submit(PendingCommand(command, value, description))

    def send_temperature(self) -> None:
        try:
            temperature = float(self.temp_command_var.get().replace(",", "."))
        except ValueError:
            messagebox.showerror(APP_TITLE, "Temperature must be a number from 16.0 to 30.0.")
            return
        temperature = round(temperature * 2.0) / 2.0
        if not 16.0 <= temperature <= 30.0:
            messagebox.showerror(APP_TITLE, "Temperature must be from 16.0 to 30.0 °C.")
            return
        self.temp_command_var.set(f"{temperature:.1f}")
        self.submit_command(CMD_TEMPERATURE, int(temperature * 2), f"temperature {temperature:.1f} °C")

    def send_fan(self) -> None:
        choice = self.fan_command_var.get()
        value = 0xFF if choice == "AUTO" else 0 if choice == "OFF" else int(choice)
        self.submit_command(CMD_FAN, value, f"fan {choice}")

    def send_airflow(self) -> None:
        choice = self.airflow_command_var.get()
        value = AIRFLOW_NAMES.index(choice)
        self.submit_command(CMD_AIRFLOW, value, f"airflow {choice}")

    def send_seat(self) -> None:
        value = int(self.seat_command_var.get())
        self.submit_command(CMD_SEAT, value, f"seat heat {value}")

    def toggle_ac(self) -> None:
        if self.latest_state is None:
            return
        value = 0 if self.latest_state.ac else 1
        self.submit_command(CMD_AC, value, f"A/C {'ON' if value else 'OFF'}")

    def toggle_power(self) -> None:
        if self.latest_state is None:
            return
        value = 0 if self.latest_state.power else 1
        self.submit_command(CMD_POWER, value, f"climate power {'ON' if value else 'OFF'}")

    def process_events(self) -> None:
        try:
            while True:
                kind, payload = self.events.get_nowait()
                if kind == "connection":
                    self.set_connection_state(payload)
                elif kind == "state":
                    self.apply_state(payload)
                elif kind == "log":
                    self.append_log(payload)
                elif kind == "command_confirmed":
                    self.append_log(payload)
                elif kind == "command_failed":
                    self.append_log(payload)
                elif kind == "bad_checksum":
                    self.bad_checksums += 1
                    self.integrity_var.set(f"CHECKSUM ERROR · {self.bad_checksums} total")
                    self.append_log("Rejected 0x201 frame with invalid XOR checksum")
                elif kind == "error":
                    self.append_log(payload)
                    messagebox.showerror(APP_TITLE, payload)
                elif kind == "stopped":
                    self.worker = None
        except queue.Empty:
            pass

        self.after(50, self.process_events)

    def set_connection_state(self, state: str) -> None:
        self.connection_var.set(state)
        if state == "OPENING":
            self.status_label.configure(fg=AMBER)
            self.connection_detail_var.set("Opening SLCAN interface…")
        elif state == "WAITING":
            self.status_label.configure(fg=AMBER)
            self.connection_detail_var.set("Waiting for a valid 0x201 state frame")
        elif state == "ONLINE":
            self.status_label.configure(fg=GREEN)
            self.connection_detail_var.set("Smart Dial is responding")
            self.set_commands_enabled(True)
        elif state == "OFFLINE":
            self.status_label.configure(fg=RED)
            self.connection_detail_var.set("No valid state frame for 800 ms")
            self.set_commands_enabled(False)
            self.latest_state = None
        elif state == "DISCONNECTED":
            self.status_label.configure(fg=MUTED)
            self.connection_detail_var.set("CAN interface is closed")
            self.set_commands_enabled(False)
            self.port_entry.configure(state="normal")
            self.connect_button.configure(text="CONNECT", bg=BLUE, activebackground=BLUE)

    def set_commands_enabled(self, enabled: bool) -> None:
        for widget in self.command_widgets:
            if isinstance(widget, ttk.Combobox):
                widget.configure(state="readonly" if enabled else "disabled")
            else:
                widget.configure(state="normal" if enabled else "disabled")

    def apply_state(self, state: DialState) -> None:
        if self.last_alive is not None:
            expected = (self.last_alive + 1) & 0x0F
            if state.alive != expected and state.alive != self.last_alive:
                self.missed_frames += (state.alive - expected) & 0x0F
        self.last_alive = state.alive
        self.latest_state = state

        self.power_var.set("ON" if state.power else "OFF")
        self.ac_var.set("ON" if state.ac else "OFF")
        self.temperature_var.set(f"{state.temperature:.1f} °C")
        self.fan_var.set(state.fan_text)
        self.airflow_var.set(state.airflow_text)
        self.seat_var.set(str(state.seat))
        self.page_var.set(state.page_text)
        self.alive_var.set(f"{state.alive} · missed {self.missed_frames}")
        self.integrity_var.set(f"CHECKSUM OK · bad {self.bad_checksums}")
        self.raw_var.set(" ".join(f"{byte:02X}" for byte in state.raw))
        self.ac_button.configure(text=f"TURN A/C {'OFF' if state.ac else 'ON'}")
        self.power_button.configure(text=f"TURN POWER {'OFF' if state.power else 'ON'}")

    def update_age(self) -> None:
        if self.latest_state is not None:
            age_ms = int((time.monotonic() - self.latest_state.received_at) * 1000)
            self.connection_detail_var.set(f"Smart Dial is responding · last frame {age_ms} ms ago")
        self.after(100, self.update_age)

    def append_log(self, message: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.configure(state="normal")
        self.log_text.insert("end", f"{timestamp}  {message}\n")
        self.log_text.see("end")
        self.log_text.configure(state="disabled")

    def close_app(self) -> None:
        if self.worker is not None:
            self.worker.stop()
        self.destroy()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="OJM Smart Dial vehicle ECU simulator")
    parser.add_argument("--port", default="COM5", help="CANable SLCAN serial port")
    parser.add_argument(
        "--skip-intro",
        action="store_true",
        help="open the simulator without the startup animation",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    app = VehicleEcuSimulator(args.port, show_intro=not args.skip_intro)
    app.mainloop()


if __name__ == "__main__":
    main()
