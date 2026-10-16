"""Local static launcher for SAT motor pilot."""

from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import os
import subprocess
import sys
import threading
import webbrowser
from urllib.request import urlopen


APP_DIR = Path(sys._MEIPASS) / "web" if getattr(sys, "frozen", False) else Path(__file__).resolve().parent
URL = "http://127.0.0.1:8765/"


class QuietHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(APP_DIR), **kwargs)

    def log_message(self, fmt, *args):
        pass


def open_browser():
    candidates = (
        Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
        Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"),
        Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft/Edge/Application/msedge.exe",
        Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
        Path(os.environ.get("LOCALAPPDATA", "")) / "Google/Chrome/Application/chrome.exe",
    )
    browser = next((path for path in candidates if path.is_file()), None)
    if browser:
        subprocess.Popen([str(browser), URL], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        webbrowser.open_new(URL)


def main():
    no_browser = "--no-browser" in sys.argv[1:]
    os.chdir(APP_DIR)
    try:
        server = ThreadingHTTPServer(("127.0.0.1", 8765), QuietHandler)
    except OSError as exc:
        try:
            with urlopen(URL, timeout=1) as response:
                running_page = response.read(4096)
            if b"SAT motor pilot" in running_page:
                print("SAT motor pilot เปิดอยู่แล้ว · เปิดหน้าต่างแอปอีกครั้ง")
                if not no_browser:
                    open_browser()
                return 0
        except OSError:
            pass
        print(f"เปิด SAT motor pilot ไม่ได้: ใช้พอร์ต 8765 ไม่ได้ ({exc})")
        input("กด Enter เพื่อปิดหน้าต่าง")
        return 1

    print("SAT motor pilot กำลังทำงานบนเครื่องนี้")
    print("ต่อบอร์ด STM32F405 แล้วเปิดแอปใน Edge/Chrome เพื่อเลือก USB Serial")
    print("ปิดหน้าต่างนี้เพื่อหยุดแอป")
    if not no_browser:
        threading.Timer(0.8, open_browser).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
