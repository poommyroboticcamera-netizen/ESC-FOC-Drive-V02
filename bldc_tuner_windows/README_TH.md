# SAT motor pilot

แอปดูสถานะและควบคุมมอเตอร์ BLDC บน Windows ในหน้าตาแนวรถแข่ง เชื่อมกับ STM32F405 ผ่าน USB CDC โดยใช้เฟิร์มแวร์ `../arduino_sixstep_releases/stm32f405_sixstep_app_v33` (`v33-proto1`, protocol 1) ในโปรเจกต์เดียวกัน แอปใช้ Web Serial ของ Microsoft Edge หรือ Google Chrome ข้อมูลบนหน้าจอมาจากบอร์ดจริง ไม่มีโหมดจำลอง

โลโก้สมบูรณ์ใน `somboon-logo.png` คัดลอกจาก `Desktop/SAT intern/Project/dashboard/dashboardesp32/dashboard/assets/somboon-logo.png` โดยเก็บไฟล์ต้นทางไว้

## เปิดแอป

1. คอมไพล์และแฟลช `../arduino_sixstep_releases/stm32f405_sixstep_app_v33/stm32f405_sixstep_app_v33.ino` ให้บอร์ด STM32F405 ที่ตรงกับฮาร์ดแวร์จริง
2. จากซอร์สโค้ดให้ดับเบิลคลิก `Start SAT motor pilot.bat` (ต้องมี Python 3) ตัวเปิดจะใช้ Edge ก่อน Chrome และเปิด `http://127.0.0.1:8765/` หรือดาวน์โหลด `SAT-motor-pilot-1.0.0-win-x64.exe` จาก GitHub Releases ซึ่งไม่ต้องติดตั้ง Python
3. ต่อ USB แล้วกด **เชื่อมต่อบอร์ด** และเลือก COM port ของ STM32F405 ในหน้าต่างเบราว์เซอร์
4. แอปตรวจ `HELLO`, protocol 1, `current_protection=OFF`, รับ `STATUS` และเปิด `STREAM ON 100` ก่อนเปิดปุ่มควบคุม

เบราว์เซอร์ต้องรองรับ Web Serial และหน้าเว็บต้องเปิดจาก localhost หากไม่มี Python ให้ใช้เซิร์ฟเวอร์ static HTTP ที่ localhost กับโฟลเดอร์นี้แทน การเปิด `index.html` ตรงจากไฟล์อาจทำให้ Web Serial ไม่พร้อมใช้งาน

## การทำงาน

- แอปส่ง `HEARTBEAT` ประมาณทุก 350 ms และขอ `STATUS` ประมาณทุก 500 ms เมื่อแท็บยังแสดงอยู่ เฟิร์มแวร์มี timeout 1500 ms; เมื่อซ่อนแท็บขณะมอเตอร์ทำงาน แอปพยายามส่ง `STOP` และหยุดต่ออายุ heartbeat
- กด `ARM` แล้ว `RUN` เพื่อเริ่มลำดับ ALIGN → STARTUP → CLOSED_LOOP ปุ่ม STOP ส่งคำสั่งทันทีโดยไม่ต้องรอค่า STATUS ล่าสุด เมื่อกดตัดการเชื่อมต่อ แอปพยายามส่ง STOP ก่อนปิดพอร์ต
- เกจ Duty มีศูนย์อยู่ตรงกลาง ด้านซ้ายเป็นค่าลบ ด้านขวาเป็นค่าบวก สีส้มคือค่าที่กำลังปรับ สีฟ้าคือ effective duty ที่เฟิร์มแวร์รายงานตามทิศที่ตั้งไว้ (ไม่ใช่ค่าที่วัดจากเพลา) ช่วงสั่งงานคือ ±5–80% ตามขอบเขตในเฟิร์มแวร์; ตั้ง 0 แล้วกด SET DUTY จะส่ง STOP การกลับทิศต้องหยุดมอเตอร์ก่อน คำสั่งจะส่งต่อเมื่อกด SET DUTY; period 600–12000 µs และ direction เปลี่ยนได้เฉพาะเมื่อหยุด แอปอ่าน `OK`/`ERR` และขอ STATUS เพื่อแสดงค่าที่บอร์ดรับจริง
- เมนูแยกเป็น **ภาพรวม**, **ควบคุมมอเตอร์**, **ค่าสามเฟส** และ **วินิจฉัย** บนจอคอมพิวเตอร์ใช้การสลับหน้า แทนหน้าที่ยาวต่อกัน
- ค่า Phase U/V/W มาจาก `OK PHASE` โดยใช้ timestamp เดียวกัน เก็บสูงสุด 1800 ตัวอย่างบนคอมพิวเตอร์; หน้า RT วาดกราฟสด และ Export CSV เก็บค่า ADC ดิบ timestamp และสถานะ validity
- เฟิร์มแวร์ส่งข้อมูลสามเฟสได้สูงสุด 10 ครั้ง/วินาทีและแอปขอสถานะประมาณ 2 ครั้ง/วินาที แอปเป็นผู้เก็บข้อมูลและวาดหน้าจอ; ไม่สร้างค่าที่วัดขึ้นมาแทนตัวอย่างที่ขาด กราฟสดอยู่ในหน้า RT ของแอปหลัก
- หน้า Diagnostics มีช่อง Serial Console ส่งคำสั่งบรรทัดเดียวไปยังเฟิร์มแวร์ได้ โดยจำกัดข้อความไว้ไม่เกิน 63 ตัวอักษร ASCII ตามบัฟเฟอร์ของบอร์ด ตัวอย่างคำสั่ง: HELP, HELLO, STATUS, DIAG, ARM, RUN, STOP, DUTY ±5–80%, DIR FWD/REV, STARTPERIOD, RAMP, RESPONSE และ SCOPE; คำสั่ง ARM/RUN/DUTY ควบคุมมอเตอร์โดยตรงและต้องตรวจสถานะบอร์ดก่อนส่ง

## ขอบเขตค่าที่แสดง

Duty, state, fault, direction, period, BEMF lock, step, PWM และ phase ADC counts อ่านจากเฟิร์มแวร์ v33 โดยตรง ค่าเฟสเป็น ADC counts จาก PA0/PA1/PA2 และ **ไม่ใช่แรงดันเฟสที่สอบเทียบแล้ว** ค่า `phase_valid` และ `phase_age_us` ใช้บอกคุณภาพ/อายุข้อมูล สตรีม 10 ตัวอย่าง/วินาทีเป็นภาพรวม telemetry ไม่ใช่รูปคลื่น PWM ความถี่สูง

เฟิร์มแวร์ยังไม่มีค่าแรงดันบัส กระแสที่ยืนยันเป็นแอมแปร์ RPM อุณหภูมิ หรือพลังงาน แอปไม่แสดงตัวเลขสมมติให้ค่ากลุ่มนี้ การวัดกระแสในเฟิร์มแวร์ยังเป็น diagnostic ที่ไม่ยืนยัน และ current protection ยังปิดอยู่

## สถานะการตรวจสอบ

แอปและเฟิร์มแวร์ยังต้องทดสอบกับบอร์ดจริง โดยเฉพาะการอ่าน USB ต่อเนื่องเมื่อมอเตอร์ทำงาน, การหยุดเมื่อ USB หลุด/ซ่อนแท็บ, timeout, reset/IWDG และรูปคลื่นเกต ใช้แหล่งจ่ายจำกัดกระแสและตรวจการหยุดทางกายภาพก่อนใช้งานจริง

## Live RT charts and language

The RT page is part of the main application. It plots only received board samples: phase U/V/W ADC counts at up to 10 Hz and duty, commutation period, and BEMF lock from STATUS at about 2 Hz. The browser keeps a 30-second view; missing or invalid phase samples leave gaps. No motor values are simulated or interpolated. The firmware keeps no plot history.

English is the default interface language. Use the language button in the header to switch to Thai; the choice is saved in this browser. Run the app from localhost in Edge or Chrome for Web Serial. Opening index.html directly from a file URL is not the supported connection path. Chart behavior with a real motor and USB cable is not yet bench verified.

The **Overview** page now places ARM, RUN, STOP MOTOR, signed duty, startup period, and direction above the live drive data. These buttons use the same command handlers and board-side limits as the detailed Motor control page. The Overview duty field is the draft command; the DUTY OUTPUT card is the value reported by the board.

Startup tuning in Overview and Motor control uses `INITIAL STEP` (12,000–30,000 µs) and `STARTUP ACCELERATION` (Gentle / Standard / Faster). Set them while stopped; the board reports the applied initial period and ramp divisor in STATUS. A shorter initial step or Faster ramp can make some motors fail to synchronize, so start with Standard and confirm each change on the bench. The default ramp is modestly quicker than the previous fixed firmware value; ALIGN timing and the 5% startup duty cap have not changed.

`DUTY RESPONSE` is also selectable as Gentle (10), Standard (20), or Faster (30) percentage points per second while stopped. The board applies the slew and reports its actual setting. Faster response can raise current more quickly; this firmware does not have a verified current trip, so confirm the change on a current-limited bench supply before using a motor under load.

## Scope waveform snapshot

The separate **Scope / BEMF** page uses the updated firmware capability `scope_capture`. In CLOSED_LOOP, press **CAPTURE WAVEFORM**. The board records up to 512 timed PWM-ON U/V/W ADC samples in RAM without calculating a waveform. Press **STOP & SHOW WAVEFORM** to request STOP, confirm the board is stopped, and download the stored samples; **STOP** then **DOWNLOAD CAPTURE** remains available as separate actions. The board transfers the stored samples only while stopped. The PC draws the three raw phase-terminal traces and separate floating-phase BEMF differential points. The normal 10 Hz phase stream continues as a trend view.

Each RT and Scope plot has independent X−/X+ and Y−/Y+ scale buttons and RESET. These controls change only PC rendering, not board sampling or measured values. The Overview shows Id and Iq as N/A because this six-step firmware does not produce validated d/q currents; calibration of phase current and a rotor angle estimate would be needed before values can be shown.

The screenshot-like traces are raw terminal samples, not calibrated volts. Only floating-phase points can be treated as a BEMF proxy. No measured segments are filled in or synthesized. This capture path has been compiled but has not been checked with a real STM32F405 and motor; verify its timing and gate behavior on the bench before relying on it.

## Live Scope waveform

With the updated firmware installed, connect the board, run to CLOSED_LOOP, open **Scope / BEMF**, then press **START LIVE VIEW**. The board captures 256 measured PWM-ON U/V/W samples per frame, transfers complete frames through USB only when output queue space is available, and starts another frame after at least 300 ms. The app redraws only complete real frames. **PAUSE LIVE VIEW** stops graph transfer, not the motor. **STOP** remains the motor stop command. If the board or USB cannot keep up, displayed frames may be skipped or become stale; this is a sequence of short captures, not a gap-free oscilloscope trace. The existing 10 Hz trend and 2 Hz status remain separate.

## Windows desktop app

Download `SAT-motor-pilot-1.0.0-win-x64.exe` from GitHub Releases for the standalone Windows desktop app. It embeds the app and Chromium runtime, opens in its own window, and does not require Python, a separate browser, or a local web server. Generated executables are intentionally not committed into the source tree.

The COM dropdown lists Windows serial ports detected on that computer. Plug in the STM32F405 with a USB data cable, press the refresh button beside the port list, choose its COM port, then press **Connect board**. If no port appears, check Windows Device Manager and install the board's USB CDC driver if needed. The port number can differ between computers, so choose it again on each computer.

To use the app on another Windows x64 computer, copy the downloaded release executable to it (for example, the Desktop) and run it. No project folder or source files are required. The app has not been connected to an STM32F405 board or motor as part of this packaging check; firmware handshake, live telemetry, and motor stop behavior still need hardware verification before operation.

To rebuild on Windows, run `npm install` and then `npm run dist:win` from `electron-shell`. The build copies the canonical root web files into the ignored `electron-shell/web/` staging directory and writes the portable executable to `electron-shell/dist/`.
