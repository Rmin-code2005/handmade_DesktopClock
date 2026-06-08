import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import subprocess
import threading
from datetime import datetime
from PIL import Image, ImageTk
import io
import struct
from zoneinfo import ZoneInfo


# ── palette ──
BG        = "#0d0f14"
PANEL     = "#13161f"
BORDER    = "#1e2433"
ACCENT    = "#00c8ff"
ACCENT2   = "#00ff9d"
TEXT      = "#cdd6f4"
MUTED     = "#45475a"
RED       = "#f38ba8"
YELLOW    = "#f9e2af"
FONT_MONO = "Courier New"

IMG_W, IMG_H = 240, 280
JPEG_QUALITY = 75

COUNTRIES = {
    "Iran":               ("Asia/Tehran",     "UTC+3:30"),
    "Germany":            ("Europe/Berlin",   "UTC+1"),
    "United States (NY)": ("America/New_York","UTC-5"),
    "United Kingdom":     ("Europe/London",   "UTC+0"),
    "Turkey":             ("Europe/Istanbul", "UTC+3"),
    "UAE":                ("Asia/Dubai",      "UTC+4"),
    "Saudi Arabia":       ("Asia/Riyadh",     "UTC+3"),
    "India":              ("Asia/Kolkata",    "UTC+5:30"),
    "China":              ("Asia/Shanghai",   "UTC+8"),
    "Japan":              ("Asia/Tokyo",      "UTC+9"),
    "Russia (Moscow)":    ("Europe/Moscow",   "UTC+3"),
    "Pakistan":           ("Asia/Karachi",    "UTC+5"),
    "Iraq":               ("Asia/Baghdad",    "UTC+3"),
}

def get_wifi_info():
    ssid, password = "", ""
    try:
        out = subprocess.check_output(["netsh","wlan","show","interfaces"],
                                      encoding="utf-8", errors="ignore")
        for line in out.splitlines():
            if "SSID" in line and "BSSID" not in line:
                ssid = line.split(":",1)[1].strip(); break
        if ssid:
            out2 = subprocess.check_output(
                ["netsh","wlan","show","profile", ssid,"key=clear"],
                encoding="utf-8", errors="ignore")
            for line in out2.splitlines():
                if "Key Content" in line:
                    password = line.split(":",1)[1].strip(); break
    except Exception as e:
        print("WiFi error:", e)
    return ssid, password

def image_to_jpeg(path: str) -> bytes:
    """resize به 240×280 + 180° rotate + swap R↔B قبل از ارسال به ESP"""
    img = Image.open(path).convert("RGB")
    img = img.resize((IMG_W, IMG_H), Image.LANCZOS)
    img = img.rotate(180)                         # برعکس کردن (180°)
    r, g, b = img.split()
    img = Image.merge("RGB", (b, g, r))           # swap R↔B
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=JPEG_QUALITY)
    return buf.getvalue()

def make_thumbnail(path: str, size=(80, 94)) -> ImageTk.PhotoImage:
    img = Image.open(path).convert("RGB")
    img.thumbnail(size, Image.LANCZOS)
    return ImageTk.PhotoImage(img)

# ── helpers ──────────────────────────────────
def card(parent, title, accent=ACCENT, pady=(0,0)):
    outer = tk.Frame(parent, bg=BORDER, padx=1, pady=1)
    outer.pack(fill="x", padx=12, pady=pady)
    inner = tk.Frame(outer, bg=PANEL)
    inner.pack(fill="both", expand=True)
    hdr = tk.Frame(inner, bg=PANEL)
    hdr.pack(fill="x")
    tk.Frame(hdr, bg=accent, width=3).pack(side="left", fill="y")
    tk.Label(hdr, text=f"  {title}", font=(FONT_MONO,9,"bold"),
             bg=PANEL, fg=accent).pack(side="left", pady=6)
    tk.Frame(inner, bg=BORDER, height=1).pack(fill="x")
    body = tk.Frame(inner, bg=PANEL, padx=12, pady=10)
    body.pack(fill="both", expand=True)
    return body

def styled_btn(parent, text, cmd, color=ACCENT, width=12):
    b = tk.Button(parent, text=text, command=cmd,
                  font=(FONT_MONO,8,"bold"),
                  bg=color, fg=BG,
                  activebackground="#ffffff", activeforeground=BG,
                  relief="flat", bd=0, padx=10, pady=5,
                  cursor="hand2", width=width)
    b.bind("<Enter>", lambda e: b.config(bg="#ffffff"))
    b.bind("<Leave>", lambda e: b.config(bg=color))
    return b

def ghost_btn(parent, text, cmd, width=10):
    return tk.Button(parent, text=text, command=cmd,
                     font=(FONT_MONO,8),
                     bg=BORDER, fg=TEXT,
                     activebackground=MUTED, activeforeground=TEXT,
                     relief="flat", bd=0, padx=8, pady=5,
                     cursor="hand2", width=width)

def entry(parent, var, show=None, width=22):
    e = tk.Entry(parent, textvariable=var,
                 font=(FONT_MONO,10),
                 bg="#1a1d2e", fg=TEXT,
                 insertbackground=ACCENT,
                 relief="flat", bd=6,
                 highlightthickness=1,
                 highlightcolor=ACCENT,
                 highlightbackground=BORDER,
                 width=width)
    if show: e.config(show=show)
    return e

def lbl(parent, text, fg=MUTED, size=8):
    return tk.Label(parent, text=text, font=(FONT_MONO,size), bg=PANEL, fg=fg)


# ── Main App ─────────────────────────────────
class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("ESP Controller")
        self.geometry("520x960")
        self.resizable(False, True)
        self.configure(bg=BG)

        self.serial_conn  = None
        self.port_var     = tk.StringVar()
        self.ssid_var     = tk.StringVar()
        self.pass_var     = tk.StringVar()
        self.country_var  = tk.StringVar(value="Iran")

        # image slots
        self.img_path     = [None, None]
        self.img_thumb    = [None, None]   # PhotoImage refs

        self._build()
        self._refresh_ports()
        self._auto_wifi()
        self._tick()

    # ─────────────────────────────────────────
    def _build(self):
        # header
        hdr = tk.Frame(self, bg=BG)
        hdr.pack(fill="x", padx=12, pady=(16,8))
        tk.Label(hdr, text="◈", font=(FONT_MONO,18,"bold"),
                 bg=BG, fg=ACCENT).pack(side="left")
        tk.Label(hdr, text=" ESP CONTROLLER",
                 font=(FONT_MONO,14,"bold"), bg=BG, fg=TEXT).pack(side="left")
        self.clock_lbl = tk.Label(hdr, text="", font=(FONT_MONO,9), bg=BG, fg=MUTED)
        self.clock_lbl.pack(side="right", padx=4)
        tk.Frame(self, bg=BORDER, height=1).pack(fill="x", padx=12, pady=(0,8))

        # serial
        b1 = card(self, "SERIAL PORT", pady=(0,6))
        row1 = tk.Frame(b1, bg=PANEL); row1.pack(fill="x")
        style = ttk.Style(); style.theme_use("clam")
        style.configure("D.TCombobox",
                        fieldbackground="#1a1d2e", background=BORDER,
                        foreground=TEXT, selectbackground=ACCENT,
                        selectforeground=BG, arrowcolor=ACCENT)
        self.port_cb = ttk.Combobox(row1, textvariable=self.port_var,
                                    width=16, state="readonly", style="D.TCombobox")
        self.port_cb.pack(side="left")
        ghost_btn(row1,"↻ Refresh", self._refresh_ports, width=9).pack(side="left", padx=6)
        self.conn_btn = styled_btn(row1,"⏻ Connect", self._toggle_serial, color=ACCENT2, width=11)
        self.conn_btn.pack(side="left")
        self.conn_lbl = tk.Label(b1, text="○  not connected",
                                 font=(FONT_MONO,8), bg=PANEL, fg=RED)
        self.conn_lbl.pack(anchor="w", pady=(6,0))

        # wifi
        b2 = card(self, "NETWORK CREDENTIALS", pady=(0,6))
        for lbl_text, var, show in [("SSID", self.ssid_var, None),
                                     ("PASSWORD", self.pass_var, "●")]:
            r = tk.Frame(b2, bg=PANEL); r.pack(fill="x", pady=3)
            lbl(r, f"{lbl_text:<10}").pack(side="left")
            entry(r, var, show=show, width=24).pack(side="left", padx=(4,0))
        row2 = tk.Frame(b2, bg=PANEL); row2.pack(fill="x", pady=(8,0))
        ghost_btn(row2,"⟳ Auto Fill", self._auto_wifi, width=11).pack(side="left")
        styled_btn(row2,"▶ Send WiFi", self._send_wifi, width=11).pack(side="left", padx=8)

        # region
        b3 = card(self, "REGION & LOCAL TIME", pady=(0,6))
        row3 = tk.Frame(b3, bg=PANEL); row3.pack(fill="x")
        self.country_cb = ttk.Combobox(row3, textvariable=self.country_var,
                                        values=sorted(COUNTRIES.keys()),
                                        width=22, state="readonly", style="D.TCombobox")
        self.country_cb.pack(side="left")
        self.country_cb.bind("<<ComboboxSelected>>", self._update_preview)
        styled_btn(row3,"▶ Send Time", self._send_time, width=11).pack(side="left", padx=8)
        self.preview_lbl = tk.Label(b3, text="", font=(FONT_MONO,13,"bold"),
                                    bg=PANEL, fg=ACCENT2)
        self.preview_lbl.pack(anchor="w", pady=(8,0))
        self.utc_lbl = tk.Label(b3, text="", font=(FONT_MONO,8), bg=PANEL, fg=MUTED)
        self.utc_lbl.pack(anchor="w")
        self._update_preview()

        # ── images panel ──────────────────────
        b4 = card(self, "SLIDESHOW IMAGES  (240×280 · JPEG)", accent=YELLOW, pady=(0,6))

        self._img_slots = []
        for i in range(2):
            row = tk.Frame(b4, bg=PANEL)
            row.pack(fill="x", pady=(0 if i else 0, 6))

            # thumbnail canvas
            canvas = tk.Canvas(row, width=80, height=94,
                                bg="#0a0c10", highlightthickness=1,
                                highlightbackground=BORDER)
            canvas.pack(side="left")
            canvas.create_text(40, 47, text="NO\nIMAGE",
                                fill=MUTED, font=(FONT_MONO,7), justify="center",
                                tags="placeholder")

            # right side
            right = tk.Frame(row, bg=PANEL)
            right.pack(side="left", fill="both", expand=True, padx=(10,0))

            path_var = tk.StringVar(value="—")
            tk.Label(right, text=f"IMAGE {i+1}", font=(FONT_MONO,8,"bold"),
                     bg=PANEL, fg=YELLOW).pack(anchor="w")
            path_lbl = tk.Label(right, textvariable=path_var,
                                 font=(FONT_MONO,7), bg=PANEL, fg=MUTED,
                                 wraplength=290, justify="left")
            path_lbl.pack(anchor="w", pady=(2,6))

            btn_row = tk.Frame(right, bg=PANEL)
            btn_row.pack(anchor="w")

            idx = i
            ghost_btn(btn_row, "📂 Browse",
                      lambda i=idx: self._browse_image(i), width=9).pack(side="left")
            styled_btn(btn_row, "▶ Send",
                       lambda i=idx: self._send_image(i),
                       color=YELLOW, width=9).pack(side="left", padx=(6,0))

            self._img_slots.append({"canvas": canvas, "path_var": path_var})

        # send both button
        send_both_row = tk.Frame(b4, bg=PANEL)
        send_both_row.pack(fill="x", pady=(4,0))
        styled_btn(send_both_row, "▶▶ Send Both Images",
                   self._send_both, color=YELLOW, width=22).pack(side="left")

        # log
        log_outer = tk.Frame(self, bg=BORDER, padx=1, pady=1)
        log_outer.pack(fill="both", expand=True, padx=12, pady=(0,12))
        log_inner = tk.Frame(log_outer, bg=PANEL)
        log_inner.pack(fill="both", expand=True)
        log_hdr = tk.Frame(log_inner, bg=PANEL)
        log_hdr.pack(fill="x")
        tk.Frame(log_hdr, bg=ACCENT2, width=3).pack(side="left", fill="y")
        tk.Label(log_hdr, text="  SERIAL LOG", font=(FONT_MONO,9,"bold"),
                 bg=PANEL, fg=ACCENT2).pack(side="left", pady=6)
        ghost_btn(log_hdr,"✕ Clear",
                  lambda: [self.log.config(state="normal"),
                            self.log.delete("1.0","end"),
                            self.log.config(state="disabled")],
                  width=7).pack(side="right", padx=6)
        tk.Frame(log_inner, bg=BORDER, height=1).pack(fill="x")
        self.log = tk.Text(log_inner, height=8, bg="#0a0c10", fg=ACCENT2,
                           font=(FONT_MONO,8), relief="flat",
                           insertbackground=ACCENT, state="disabled",
                           padx=8, pady=6)
        self.log.pack(fill="both", expand=True)
        self.log.tag_config("sent", foreground=ACCENT)
        self.log.tag_config("recv", foreground=ACCENT2)
        self.log.tag_config("info", foreground=MUTED)
        self.log.tag_config("err",  foreground=RED)
        self.log.tag_config("img",  foreground=YELLOW)

    # ── image methods ─────────────────────────
    def _browse_image(self, idx):
        path = filedialog.askopenfilename(
            title=f"Select Image {idx+1}",
            filetypes=[("Images","*.jpg *.jpeg *.png *.bmp *.gif *.webp"),
                       ("All","*.*")])
        if not path: return

        self.img_path[idx] = path
        self._img_slots[idx]["path_var"].set(path.split("/")[-1].split("\\")[-1])

        # thumbnail
        try:
            thumb = make_thumbnail(path)
            self.img_thumb[idx] = thumb   # prevent GC
            c = self._img_slots[idx]["canvas"]
            c.delete("all")
            c.create_image(40, 47, image=thumb)
        except Exception as e:
            self._log(f"thumbnail error: {e}", "err")

        self._log(f"image {idx+1} loaded: {path}", "img")

    def _send_image(self, idx):
        path = self.img_path[idx]
        if not path:
            messagebox.showwarning("Image", f"ابتدا Image {idx+1} رو انتخاب کن!")
            return
        if not self.serial_conn or not self.serial_conn.is_open:
            messagebox.showwarning("Serial", "ابتدا Connect کن!")
            return

        threading.Thread(target=self._send_image_thread,
                         args=(idx, path), daemon=True).start()

    def _send_image_thread(self, idx, path):
        try:
            self._log(f"encoding image {idx+1}...", "img")
            jpeg_data = image_to_jpeg(path)
            size = len(jpeg_data)

            self._log(f"sending IMG{idx+1} ({size} bytes)...", "img")

            # پروتکل:
            # خط اول:  IMG:index,size\n   (index = 1 یا 2)
            # بعدش:    raw jpeg bytes
            # بعدش:    END_IMG\n
            header = f"IMG:{idx+1},{size}\n".encode()
            self.serial_conn.write(header)
            self.serial_conn.flush()

            # ارسال به صورت chunk برای جلوگیری از overflow
            chunk = 256
            sent = 0
            while sent < size:
                self.serial_conn.write(jpeg_data[sent:sent+chunk])
                self.serial_conn.flush()
                sent += chunk

            self.serial_conn.write(b"END_IMG\n")
            self.serial_conn.flush()

            self._log(f"image {idx+1} sent OK ({size} bytes)", "img")

        except Exception as e:
            self._log(f"image send error: {e}", "err")

    def _send_both(self):
        if not self.img_path[0] and not self.img_path[1]:
            messagebox.showwarning("Images","هیچ تصویری انتخاب نشده!")
            return
        for i in range(2):
            if self.img_path[i]:
                self._send_image(i)

    # ── serial ───────────────────────────────
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cb["values"] = ports
        if ports: self.port_var.set(ports[0])
        self._log("ports: " + (", ".join(ports) or "none found"), "info")

    def _toggle_serial(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            self.conn_lbl.config(text="○  not connected", fg=RED)
            self.conn_btn.config(text="⏻ Connect", bg=ACCENT2)
            self.conn_btn.bind("<Leave>", lambda e: self.conn_btn.config(bg=ACCENT2))
            self._log("disconnected", "info")
            return
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("Port","پورتی انتخاب نشده!"); return
        try:
            self.serial_conn = serial.Serial(port, 115200, timeout=2)
            self.conn_lbl.config(text=f"●  {port}  @  115200", fg=ACCENT2)
            self.conn_btn.config(text="⏻ Disconnect", bg=RED)
            self.conn_btn.bind("<Leave>", lambda e: self.conn_btn.config(bg=RED))
            self._log(f"connected → {port}", "info")
            threading.Thread(target=self._read_loop, daemon=True).start()
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _read_loop(self):
        while self.serial_conn and self.serial_conn.is_open:
            try:
                line = self.serial_conn.readline().decode("utf-8",errors="ignore").strip()
                if line: self._log(f"← {line}", "recv")
            except: break

    def _send(self, data):
        if not self.serial_conn or not self.serial_conn.is_open:
            messagebox.showwarning("Serial","ابتدا Connect کن!"); return
        try:
            self.serial_conn.write((data+"\n").encode())
            self._log(f"→ {data}", "sent")
        except Exception as e:
            self._log(f"error: {e}", "err")

    # ── wifi ─────────────────────────────────
    def _auto_wifi(self):
        ssid, pwd = get_wifi_info()
        self.ssid_var.set(ssid); self.pass_var.set(pwd)
        self._log(f"wifi detected: {ssid or 'not found'}", "info")

    def _send_wifi(self):
        ssid = self.ssid_var.get().strip()
        if not ssid:
            messagebox.showwarning("WiFi","SSID خالی است!"); return
        self._send(f"WIFI:{ssid},{self.pass_var.get().strip()}")

    # ── time ─────────────────────────────────
    def _update_preview(self, event=None):
        country = self.country_var.get()
        if country in COUNTRIES:
            tz, utc = COUNTRIES[country]
            now = datetime.now(ZoneInfo(tz))
            self.preview_lbl.config(text=now.strftime("%H : %M : %S"))
            self.utc_lbl.config(text=f"{country}   ·   {utc}")

    def _send_time(self):
        country = self.country_var.get()
        if country not in COUNTRIES: return
        tz, utc = COUNTRIES[country]
        now = datetime.now(ZoneInfo(tz))
        self._send(f"TIME:{now.hour},{now.minute},{now.second},{utc}")

    # ── tick ─────────────────────────────────
    def _tick(self):
        self.clock_lbl.config(text=datetime.now().strftime("%Y-%m-%d  %H:%M:%S"))
        # آپدیت ساعت preview بدون دست زدن به combobox
        country = self.country_var.get()
        if country in COUNTRIES:
            tz, utc = COUNTRIES[country]
            now = datetime.now(ZoneInfo(tz))
            self.preview_lbl.config(text=now.strftime("%H : %M : %S"))
            self.utc_lbl.config(text=f"{country}   ·   {utc}")
        self.after(1000, self._tick)

    # ── log ──────────────────────────────────
    def _log(self, msg, tag="info"):
        self.log.config(state="normal")
        ts = datetime.now().strftime("%H:%M:%S")
        self.log.insert("end", f"[{ts}]  ", "info")
        self.log.insert("end", f"{msg}\n", tag)
        self.log.see("end")
        self.log.config(state="disabled")


if __name__ == "__main__":
    App().mainloop()