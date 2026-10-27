(() => {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const L = (en, th) => window.SATLang.pick(en, th);
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();
  const ACTIVE = new Set(['ALIGN', 'STARTUP', 'CLOSED_LOOP']);
  const MAX_SAMPLES = 1800;
  const LINK_STALE_MS = 1600;
  const MAX_DUTY = 80;
  const DUTY_GAUGE_DEG = 140 / MAX_DUTY;
  const app = {
    port: null, availablePorts: [], reader: null, writer: null, readTask: null, writeChain: Promise.resolve(),
    closing: false, ready: false, rxBuffer: '', pending: new Map(),
    status: null, phase: null, phaseSamples: [], lastPhaseUs: null, phaseTimeUs: 0,
    currentSupported: false,
    startupProfileSupported: false, profileLoaded: false,
    dutyResponseSupported: false, responseLoaded: false,
    scopeSupported: false, scopeLiveSupported: false, scopeLive: false, scopeLiveFrame: false,
    scopeLiveFrames: 0, scopeLastLiveAt: 0, scopeFrameExpected: 0,
    scopeReady: false, scopeCapturing: false, scopeReading: false, scopeStopping: false,
    scopeSamples: [], scopeExpected: 0, scopePollTimer: null, scopeReadTimer: null,
    lastStatusAt: 0, lastHeartbeatAt: 0, heartbeatTimer: null, statusTimer: null,
    events: [], lastEventKey: '', log: [], page: 'dashboard'
  };

  function note(message, level = '') {
    $('notice').textContent = message;
    $('notice').className = `notice ${level}`.trim();
  }

  function logLine(line, type = 'rx') {
    const stamp = new Date().toLocaleTimeString('th-TH', { hour12: false });
    app.log.push(`${stamp} ${type === 'tx' ? '→' : '←'} ${line}`);
    if (app.log.length > 220) app.log.shift();
    $('serialLog').textContent = app.log.join('\n');
    $('serialLog').scrollTop = $('serialLog').scrollHeight;
  }

  function fields(line) {
    const result = {};
    for (const token of line.trim().split(/\s+/).slice(2)) {
      const at = token.indexOf('=');
      if (at > 0) result[token.slice(0, at)] = token.slice(at + 1);
    }
    return result;
  }

  function integer(value, fallback = null) {
    if (typeof value !== 'string' || !/^\d+$/.test(value)) return fallback;
    const number = Number(value);
    return Number.isSafeInteger(number) ? number : fallback;
  }
  function signedInteger(value, fallback = null) {
    if (typeof value !== 'string' || !/^-?\d+$/.test(value)) return fallback;
    const number = Number(value);
    return Number.isSafeInteger(number) ? number : fallback;
  }

  function resolveReply(line) {
    for (const [kind, waiting] of app.pending) {
      if (kind === 'SCOPE' && line.startsWith('OK SCOPE ') &&
          !line.startsWith('OK SCOPE state=')) continue;
      if (line === `OK ${kind}` || line.startsWith(`OK ${kind} `) ||
          line === `ERR ${kind}` || line.startsWith(`ERR ${kind} `)) {
        clearTimeout(waiting.timer);
        app.pending.delete(kind);
        if (line.startsWith('ERR ')) waiting.reject(new Error(line));
        else waiting.resolve(line);
        return;
      }
    }
  }

  function receiveLine(raw) {
    const line = raw.trim();
    if (!line) return;
    if (line.startsWith('OK STATUS ')) updateStatus(fields(line));
    else if (line.startsWith('OK PHASE ')) updatePhase(fields(line));
    else if (line.startsWith('OK SCOPE SAMPLE ')) receiveScopeSample(fields(line));
    else if (line.startsWith('OK SCOPE END ')) finishScopeRead(fields(line));
    else if (line.startsWith('OK SCOPE FRAME BEGIN ')) beginLiveScopeFrame(fields(line));
    else if (line.startsWith('OK SCOPE FRAME END ')) finishLiveScopeFrame(fields(line));
    if (!line.startsWith('OK PHASE ') && !line.startsWith('OK HEARTBEAT ') &&
        !line.startsWith('OK STATUS ') && !line.startsWith('OK SCOPE SAMPLE ')) logLine(line);
    if (line.startsWith('ERR ')) note(`${L('Board rejected command', 'บอร์ดปฏิเสธคำสั่ง')}: ${line}`, 'warn');
    resolveReply(line);
  }

  async function readLoop() {
    try {
      app.reader = app.port.readable.getReader();
      while (true) {
        const { value, done } = await app.reader.read();
        if (done) break;
        app.rxBuffer += decoder.decode(value, { stream: true });
        if (app.rxBuffer.length > 8192) throw new Error('Serial line too long');
        let end;
        while ((end = app.rxBuffer.indexOf('\n')) >= 0) {
          const line = app.rxBuffer.slice(0, end);
          app.rxBuffer = app.rxBuffer.slice(end + 1);
          receiveLine(line);
        }
      }
    } catch (error) {
      if (!app.closing) note(`${L('USB Serial connection lost', 'USB Serial ขาดการเชื่อมต่อ')}: ${error.message}`, 'error');
    } finally {
      if (app.reader) { app.reader.releaseLock(); app.reader = null; }
      if (!app.closing) void disconnect(false);
    }
  }

  function transmit(command) {
    if (!app.writer) return Promise.reject(new Error(L('USB Serial disconnected', 'USB Serial ยังไม่เชื่อมต่อ')));
    app.writeChain = app.writeChain.catch(() => {}).then(async () => {
      await app.writer.write(encoder.encode(`${command}\n`));
      if (command !== 'HEARTBEAT' && command !== 'STATUS') logLine(command, 'tx');
    });
    return app.writeChain;
  }

  async function request(command, kind, timeoutMs = 2200) {
    if (app.pending.has(kind)) {
      const waiting = app.pending.get(kind);
      if (waiting.command !== command) throw new Error(`${kind}: ${L('waiting for the previous reply','รอคำตอบคำสั่งก่อนหน้า')}`);
      return waiting.promise;
    }
    let accept, reject;
    const promise = new Promise((resolve, fail) => { accept = resolve; reject = fail; });
    const timer = setTimeout(() => {
      if (app.pending.get(kind)?.promise === promise) {
        app.pending.delete(kind);
        reject(new Error(`${kind}: ${L('no reply from board','ไม่มีคำตอบจากบอร์ด')}`));
      }
    }, timeoutMs);
    app.pending.set(kind, { command, promise, resolve: accept, reject, timer });
    try { await transmit(command); }
    catch (error) {
      if (app.pending.get(kind)?.promise === promise) {
        clearTimeout(timer); app.pending.delete(kind); reject(error);
      }
    }
    return promise;
  }

  function endPending(reason) {
    for (const waiting of app.pending.values()) {
      clearTimeout(waiting.timer);
      waiting.reject(new Error(reason));
    }
    app.pending.clear();
  }

  async function refreshPorts(preferredPort = null) {
    const select = $('portSelect');
    if (!('serial' in navigator) || !select) return;
    try {
      const previousValue = select.value;
      if (window.satNative?.listComPorts) {
        const names = await window.satNative.listComPorts();
        select.replaceChildren();
        const prompt = document.createElement('option');
        prompt.value = '';
        prompt.textContent = names.length
          ? L('Select a COM port', 'เลือก COM port')
          : L('No COM ports found · connect board and refresh', 'ไม่พบ COM port · เสียบบอร์ดแล้วกดรีเฟรช');
        select.append(prompt);
        for (const name of names) {
          const option = document.createElement('option');
          option.value = name;
          option.textContent = name;
          select.append(option);
        }
        if (names.includes(previousValue)) select.value = previousValue;
        else if (names.length === 1) select.value = names[0];
        return;
      }
      app.availablePorts = await navigator.serial.getPorts();
      select.replaceChildren();
      const prompt = document.createElement('option');
      prompt.value = '';
      prompt.textContent = L('Select a permitted USB Serial port', 'เลือกพอร์ต USB Serial ที่อนุญาตแล้ว');
      select.append(prompt);
      app.availablePorts.forEach((port, index) => {
        const info = port.getInfo();
        const ids = [info.usbVendorId, info.usbProductId]
          .map((value) => Number.isInteger(value) ? value.toString(16).padStart(4, '0').toUpperCase() : null);
        const option = document.createElement('option');
        option.value = String(index);
        option.textContent = ids.every(Boolean) ? `USB Serial · ${ids[0]}:${ids[1]}` : `USB Serial ${index + 1}`;
        select.append(option);
      });
      if (preferredPort) {
        const index = app.availablePorts.indexOf(preferredPort);
        select.value = index >= 0 ? String(index) : '';
      } else if (app.availablePorts.length === 1) select.value = '0';
    } catch (_) { /* The browser may not expose the permitted-port list. */ }
  }

  async function connect() {
    if (!window.isSecureContext) {
      note(L('USB Serial requires a secure HTTPS page or localhost. Open the published site directly in Edge or Chrome.', 'USB Serial ต้องเปิดจากหน้า HTTPS หรือ localhost ที่ปลอดภัย กรุณาเปิดเว็บที่เผยแพร่โดยตรงใน Edge หรือ Chrome'), 'error');
      return;
    }
    if (!('serial' in navigator)) {
      note(L('This browser does not support Web Serial. Open the app in Microsoft Edge or Google Chrome.','เบราว์เซอร์นี้ไม่รองรับ Web Serial กรุณาเปิดแอปด้วย Microsoft Edge หรือ Google Chrome'), 'error');
      return;
    }
    const selectedPortName = $('portSelect').value;
    if (window.satNative && !selectedPortName) {
      note(L('Select a COM port first. Press ↻ to refresh the list.', 'เลือก COM port ก่อน แล้วกด ↻ เพื่อรีเฟรชรายการ'), 'warn');
      return;
    }
    $('connectBtn').disabled = true;
    try {
      if (window.satNative?.selectComPort) {
        const accepted = await window.satNative.selectComPort(selectedPortName);
        if (!accepted) throw new Error(L('The selected COM port is invalid. Refresh and select it again.', 'COM port ที่เลือกไม่ถูกต้อง กรุณารีเฟรชและเลือกใหม่'));
      }
      // Electron maps the selected COM name in the main process. In a browser,
      // getPorts() only returns devices previously permitted, so requestPort()
      // must open the native browser picker on the first connection.
      const selected = await navigator.serial.requestPort();
      if (!selected) throw new Error(L('Selected USB Serial port is no longer available. Refresh and select it again.', 'พอร์ต USB Serial ที่เลือกไม่พร้อมใช้งาน กรุณาเลือกพอร์ตใหม่'));
      await refreshPorts();
      app.port = selected;
      await selected.open({ baudRate: 115200, bufferSize: 4096 });
      await selected.setSignals({ dataTerminalReady: true });
      app.writer = selected.writable.getWriter();
      app.closing = false;
      app.rxBuffer = '';
      app.phase = null;
      app.phaseSamples.length = 0;
      app.scopeSamples.length = 0;
      app.scopeReady = false; app.scopeCapturing = false; app.scopeReading = false; app.scopeStopping = false;
      app.scopeLive = false; app.scopeLiveFrame = false; app.scopeLiveFrames = 0; app.scopeLastLiveAt = 0;
      app.profileLoaded = false; app.responseLoaded = false;
      window.SATScopePlot.draw([]);
      window.SATCharts.reset();
      app.lastPhaseUs = null;
      app.phaseTimeUs = 0;
      app.events.length = 0;
      app.lastEventKey = '';
      renderEvents();
      renderPhase();
      app.readTask = readLoop();
      $('linkState').textContent = 'HANDSHAKE';
      const hello = fields(await request('HELLO', 'HELLO', 4000));
      if (hello.protocol !== '1' || !/^v3[34]-proto1$/.test(hello.fw || '') ||
          hello.current_protection !== 'OFF') {
        throw new Error(L('Firmware does not match SAT motor pilot v33/v34 protocol','เฟิร์มแวร์ไม่ตรงกับโปรโตคอล SAT motor pilot v33/v34'));
      }
      app.currentSupported = hello.caps?.split(',').includes('current_monitor') || false;
      app.scopeSupported = hello.caps?.split(',').includes('scope_capture') || false;
      app.scopeLiveSupported = hello.caps?.split(',').includes('scope_live') || false;
      app.startupProfileSupported = hello.caps?.split(',').includes('startup_profile') || false;
      app.dutyResponseSupported = hello.caps?.split(',').includes('duty_response') || false;
      await request('HEARTBEAT', 'HEARTBEAT');
      app.lastHeartbeatAt = performance.now();
      await request('STATUS', 'STATUS', 3000);
      await request('STREAM ON 100', 'STREAM');
      app.ready = true;
      if (app.scopeSupported) {
        try { updateScopeState(fields(await request('SCOPE STATUS', 'SCOPE'))); }
        catch (error) { note(error.message, 'warn'); }
      }
      window.SATCharts.setActive(app.page === 'rt' || app.page === 'scope');
      app.heartbeatTimer = setInterval(() => {
        if (!document.hidden && app.ready && !app.pending.has('HEARTBEAT')) {
          request('HEARTBEAT', 'HEARTBEAT').then(() => { app.lastHeartbeatAt = performance.now(); render(); })
            .catch((error) => note(error.message, 'warn'));
        }
      }, 350);
      app.statusTimer = setInterval(() => {
        if (!document.hidden && app.ready && !app.pending.has('STATUS')) {
          request('STATUS', 'STATUS', 2800).catch((error) => note(error.message, 'warn'));
        }
      }, 500);
      note(L(`Connected to STM32F405 · ${hello.fw} · phase stream 10 samples/s`,`เชื่อมต่อ STM32F405 สำเร็จ · ${hello.fw} · phase stream 10 ตัวอย่าง/วินาที`));
    } catch (error) {
      if (error.name !== 'NotFoundError') {
        const rawMessage = String(error?.message || error);
        const portOpenFailed = /failed to open serial port/i.test(rawMessage);
        const detail = portOpenFailed
          ? L(
              `Windows could not open ${selectedPortName || 'the selected COM port'}. Close Serial Monitor, Arduino IDE, browser tabs, and other SAT motor pilot windows using this port. Confirm the board's COM number in Device Manager, then refresh and retry.`,
              `Windows เปิด ${selectedPortName || 'COM port ที่เลือก'} ไม่ได้ · ปิด Serial Monitor, Arduino IDE, แท็บเบราว์เซอร์ และหน้าต่าง SAT motor pilot อื่นที่อาจใช้พอร์ตนี้ ตรวจหมายเลข COM ของบอร์ดใน Device Manager แล้วกดรีเฟรชและลองใหม่`
            )
          : rawMessage;
        note(`${L('Connection failed','เชื่อมต่อไม่สำเร็จ')}: ${detail}`, 'error');
      }
      await disconnect(false);
    } finally { render(); }
  }

  async function disconnect(sendStop = true) {
    if (app.closing) return;
    app.closing = true;
    app.ready = false;
    clearInterval(app.heartbeatTimer);
    clearInterval(app.statusTimer);
    clearTimeout(app.scopePollTimer);
    clearTimeout(app.scopeReadTimer);
    app.scopeCapturing = false; app.scopeReading = false; app.scopeStopping = false;
    app.scopeLive = false; app.scopeLiveFrame = false;
    if (sendStop && app.writer) {
      try {
        if (app.status && (ACTIVE.has(app.status.state) || app.status.state === 'ARMED_IDLE'))
          await request('STOP', 'STOP', 800);
        await request('STREAM OFF', 'STREAM', 800);
      } catch (error) { logLine(`disconnect: ${error.message}`); }
    }
    endPending('USB Serial disconnected');
    try { if (app.reader) await app.reader.cancel(); } catch (_) { /* unplugged */ }
    try { if (app.readTask) await app.readTask; } catch (_) { /* unplugged */ }
    try { if (app.writer) { app.writer.releaseLock(); app.writer = null; } } catch (_) { app.writer = null; }
    try { if (app.port) await app.port.close(); } catch (_) { /* unplugged */ }
    app.port = null;
    app.readTask = null;
    app.status = null;
    app.phase = null;
    app.scopeSupported = false; app.scopeLiveSupported = false; app.scopeReady = false;
    app.currentSupported = false;
    app.startupProfileSupported = false; app.profileLoaded = false;
    app.dutyResponseSupported = false; app.responseLoaded = false;
    window.SATCharts.setActive(app.page === 'rt' || app.page === 'scope');
    app.lastStatusAt = 0;
    app.lastHeartbeatAt = 0;
    app.closing = false;
    void refreshPorts();
    renderPhase();
    $('linkState').textContent = 'OFFLINE';
    $('linkDetail').textContent = L('USB Serial disconnected','USB Serial ยังไม่เชื่อมต่อ');
    if (sendStop) note(L('Disconnected · verify the motor has physically stopped before touching the circuit','ตัดการเชื่อมต่อแล้ว · ตรวจว่ามอเตอร์หยุดจริงก่อนทำงานกับวงจร'));
    render();
  }

  function updateStatus(data) {
    if (!data.state || !data.fault || integer(data.duty_target_pct) === null) return;
    if (app.startupProfileSupported && !app.profileLoaded) {
      const initial = integer(data.startup_initial_period_us);
      const ramp = integer(data.startup_ramp_divisor);
      if (initial !== null && [150,200,300].includes(ramp)) {
        $('startInput').value = initial; $('overviewStartInput').value = initial;
        $('rampSelect').value = ramp; $('overviewRampSelect').value = ramp;
        app.profileLoaded = true;
      }
    }
    if (app.dutyResponseSupported && !app.responseLoaded) {
      const response = integer(data.duty_ramp_pct_per_s);
      if ([10,20,30].includes(response)) {
        $('responseSelect').value = response; $('overviewResponseSelect').value = response;
        app.responseLoaded = true;
      }
    }
    app.status = data;
    app.lastStatusAt = performance.now();
    window.SATCharts.addStatus(data);
    const key = `${data.state}/${data.fault}`;
    if (key !== app.lastEventKey) {
      app.lastEventKey = key;
      app.events.unshift({ time: new Date().toLocaleTimeString('th-TH', { hour12: false }), state: data.state, fault: data.fault });
      if (app.events.length > 8) app.events.pop();
      renderEvents();
    }
    render();
  }

  function updatePhase(data) {
    const u = integer(data.u_adc_counts), v = integer(data.v_adc_counts), w = integer(data.w_adc_counts);
    const stamp = integer(data.t_us), age = integer(data.phase_age_us);
    if ([u, v, w, stamp, age].some((value) => value === null) || [u, v, w].some((value) => value > 4095)) return;
    const valid = data.phase_valid === '1' && age < 100000;
    const floating = /^[UVW]$/.test(data.f || '') ? data.f : null;
    app.phase = { u, v, w, stamp, age, valid, floating };
    if (stamp !== app.lastPhaseUs) {
      if (app.lastPhaseUs !== null) {
        const delta = (stamp - app.lastPhaseUs) >>> 0;
        if (delta > 10000000) { app.phaseSamples.length = 0; app.phaseTimeUs = 0; }
        else app.phaseTimeUs += delta;
      }
      app.lastPhaseUs = stamp;
      const bemfError = valid && floating ? 3 * ({U:u,V:v,W:w}[floating]) - u - v - w : null;
      app.phaseSamples.push({ t: app.phaseTimeUs / 1000000, boardUs: stamp, wall: new Date().toISOString(), u, v, w, valid, age, floating, bemfError });
      window.SATCharts.addPhase(app.phase, app.phaseTimeUs / 1000);
      if (app.phaseSamples.length > MAX_SAMPLES) app.phaseSamples.shift();
      $('sampleCount').textContent = `${app.phaseSamples.length.toLocaleString()} / ${MAX_SAMPLES.toLocaleString()} samples`;
      $('csvBtn').disabled = false;
    }
    renderPhase();
  }

  function updateScopeState(data) {
    app.scopeLive = data.state === 'LIVE';
    app.scopeCapturing = data.state === 'CAPTURING';
    app.scopeReady = data.state === 'READY';
    app.scopeExpected = integer(data.count) ?? 0;
    renderScopeState();
  }
  function renderScopeState() {
    const s = app.status;
    const fresh = app.ready && s && performance.now() - app.lastStatusAt < LINK_STALE_MS &&
      performance.now() - app.lastHeartbeatAt < LINK_STALE_MS;
    $('scopeCaptureBtn').disabled = !app.scopeSupported || !fresh || s.state !== 'CLOSED_LOOP' ||
      app.scopeCapturing || app.scopeReading || app.scopeStopping || app.scopeLive;
    $('scopeReadBtn').disabled = !app.scopeSupported || !app.scopeReady || !fresh ||
      ACTIVE.has(s.state) || app.scopeReading || app.scopeStopping || app.scopeLive;
    $('scopeStopViewBtn').disabled = !app.scopeSupported || !app.scopeReady || !fresh ||
      !ACTIVE.has(s.state) || app.scopeReading || app.scopeStopping || app.scopeLive;
    $('scopeLiveBtn').disabled = !app.scopeLiveSupported || !app.writer ||
      (!app.scopeLive && (!fresh || s.state !== 'CLOSED_LOOP' ||
        app.scopeCapturing || app.scopeReading || app.scopeStopping));
    $('scopeLiveBtn').textContent = app.scopeLive ? 'PAUSE LIVE VIEW' : 'START LIVE VIEW';
    $('scopeState').textContent = !app.port ? L('Connect the board · capture is available in CLOSED_LOOP','เชื่อมต่อบอร์ด · จับข้อมูลได้เมื่อมอเตอร์อยู่ใน CLOSED_LOOP')
      : !app.scopeSupported ? L('Scope capture needs the updated v33 firmware','โหมด Scope ต้องใช้เฟิร์มแวร์ v33 ที่อัปเดตแล้ว')
      : app.scopeStopping ? L('Stopping motor before downloading capture…','กำลังหยุดมอเตอร์ก่อนรับข้อมูล Scope…')
      : app.scopeReading ? L('Downloading raw samples from board…','กำลังรับตัวอย่างดิบจากบอร์ด…')
      : app.scopeLive && performance.now() - app.scopeLastLiveAt < 1800 ? L(`Live waveform · ${app.scopeLiveFrames} complete frames · ${app.scopeFrameExpected} real samples in latest frame`,`รูปคลื่นสด · ${app.scopeLiveFrames} เฟรมครบ · เฟรมล่าสุด ${app.scopeFrameExpected} ตัวอย่างจริง`)
      : app.scopeLive ? L('Waiting for the next complete live frame…','กำลังรอเฟรมสดที่รับครบ…')
      : app.scopeCapturing ? L('Capturing waveform in board RAM…','กำลังจับรูปคลื่นใน RAM ของบอร์ด…')
      : app.scopeReady && ACTIVE.has(s?.state) ? L('Capture ready · press STOP & SHOW WAVEFORM','จับข้อมูลแล้ว · กด STOP & SHOW WAVEFORM')
      : app.scopeReady ? L(`Capture ready · ${app.scopeExpected} raw samples · press DOWNLOAD CAPTURE`,`ข้อมูลพร้อม · ${app.scopeExpected} ตัวอย่างดิบ · กด DOWNLOAD CAPTURE`)
      : app.scopeLiveFrames ? L('Live view paused · last complete frame shown','พักภาพสด · แสดงเฟรมล่าสุดที่รับครบ')
      : L('Run the motor to CLOSED_LOOP, then press CAPTURE WAVEFORM','ให้มอเตอร์เข้า CLOSED_LOOP แล้วกด CAPTURE WAVEFORM');
  }
  function receiveScopeSample(data) {
    if (!app.scopeReading && !app.scopeLiveFrame) return;
    const i=integer(data.i), tUs=integer(data.t_us), u=integer(data.u), v=integer(data.v), w=integer(data.w);
    if (i !== app.scopeSamples.length || [tUs,u,v,w].some((x)=>x===null) ||
        [u,v,w].some((x)=>x>4095) || !/^[UVW]$/.test(data.f || '')) return;
    app.scopeSamples.push({tUs,u,v,w,f:data.f,step:integer(data.step)});
  }
  function beginLiveScopeFrame(data) {
    const count = integer(data.count);
    if (!app.scopeLive || count === null || count < 2 || count > 256) return;
    app.scopeSamples.length = 0;
    app.scopeFrameExpected = count;
    app.scopeLiveFrame = true;
  }
  function finishLiveScopeFrame(data) {
    if (!app.scopeLiveFrame) return;
    app.scopeLiveFrame = false;
    if (integer(data.count) === app.scopeFrameExpected &&
        app.scopeSamples.length === app.scopeFrameExpected) {
      app.scopeLiveFrames++;
      app.scopeLastLiveAt = performance.now();
      window.SATScopePlot.draw(app.scopeSamples);
      renderScopeState();
    } else app.scopeSamples.length = 0;
  }
  function finishScopeRead(data) {
    if (!app.scopeReading) return;
    clearTimeout(app.scopeReadTimer);
    app.scopeReading = false;
    const expected=integer(data.count);
    if (expected===null || expected!==app.scopeSamples.length) {
      note(L('Scope transfer incomplete · no waveform drawn','รับข้อมูล Scope ไม่ครบ · ไม่วาดรูปคลื่น'),'warn');
      app.scopeSamples.length=0;
    } else {
      window.SATScopePlot.draw(app.scopeSamples);
      note(L(`Scope capture loaded: ${expected} real samples`,`โหลดข้อมูล Scope แล้ว: ${expected} ตัวอย่างจริง`));
    }
    renderScopeState();
  }
  async function pollScope() {
    if (!app.port || !app.scopeSupported) return;
    try {
      updateScopeState(fields(await request('SCOPE STATUS','SCOPE',2800)));
      if (app.scopeCapturing || app.scopeLive) app.scopePollTimer=setTimeout(pollScope,500);
    } catch (error) { note(error.message,'warn'); app.scopeCapturing=false; renderScopeState(); }
  }

  function renderEvents() {
    const list = $('eventList');
    list.replaceChildren();
    for (const event of app.events) {
      const row = document.createElement('div');
      row.className = 'event-row';
      const time = document.createElement('time'); time.textContent = event.time;
      const state = document.createElement('strong'); state.textContent = event.state;
      const fault = document.createElement('span'); fault.textContent = event.fault;
      if (event.fault !== 'none') fault.className = 'fault';
      row.append(time, state, fault); list.append(row);
    }
  }

  function renderPhase() {
    const p = app.phase;
    for (const [id, value] of [['phaseU', p?.u], ['phaseV', p?.v], ['phaseW', p?.w]])
      $(id).textContent = value === undefined ? '—' : value.toLocaleString();
    $('phaseQuality').textContent = !p ? '—' : p.valid ? 'VALID' : 'STALE / INVALID';
    $('phaseAge').textContent = p ? `${L('Age','อายุ')} ${p.age.toLocaleString()} µs · t=${p.stamp}` : '—';
  }

  const signedText = (value) => `${value > 0 ? '+' : ''}${value}`;
  function setDraft(value) {
    value = Math.round(Math.max(-MAX_DUTY, Math.min(MAX_DUTY, value)));
    if (Math.abs(value) < 5) value = 0;
    $('dutyInput').value = value;
    $('dutySlider').value = value;
    $('overviewDutyInput').value = value;
    $('overviewDutySlider').value = value;
    renderControlGauge();
  }
  function renderControlGauge() {
    const draft = validNumber($('dutyInput'), -MAX_DUTY, MAX_DUTY);
    const s = app.status;
    const actual = integer(s?.duty_effective_pct_tenths);
    const fresh = !!s && app.ready && performance.now() - app.lastStatusAt < LINK_STALE_MS;
    const sign = s?.direction === 'rev' ? -1 : 1;
    $('draftNeedle').setAttribute('transform', `rotate(${(draft ?? 0) * DUTY_GAUGE_DEG} 220 205)`);
    $('gaugeDraft').textContent = draft === null ? '—' : `${signedText(draft)}%`;
    $('controlGauge').setAttribute('aria-valuenow', String(draft ?? 0));
    $('actualNeedle').style.display = fresh && actual !== null ? '' : 'none';
    $('actualNeedle').removeAttribute('hidden');
    $('actualNeedle').setAttribute('transform', `rotate(${sign * (actual ?? 0) / 10 * DUTY_GAUGE_DEG} 220 205)`);
    $('gaugeActual').textContent = fresh && actual !== null ? signedText(Number((sign * actual / 10).toFixed(1))) : '—';
    const target = integer(s?.duty_target_pct);
    $('gaugeApplied').textContent = `${L('Board target','เป้าหมายที่บอร์ดรับ')}: ${fresh && target !== null ? signedText(sign * target) + '%' : '—'}${fresh ? '' : L(' · no live data',' · ไม่มีข้อมูลสด')}`;
    const reverse = draft !== null && draft !== 0 && (draft > 0 ? 1 : -1) !== sign && ACTIVE.has(s?.state);
    $('gaugeHint').textContent = reverse ? L('STOP the motor before reversing','ต้อง STOP ให้บอร์ดหยุดก่อนเปลี่ยนทิศ') : draft === 0 ? L('Press SET DUTY to send STOP','กด SET DUTY เพื่อส่ง STOP') : L('Orange: draft target · cyan: board effective duty','สีส้ม: ค่าที่ยังปรับอยู่ · สีฟ้า: Effective Duty จากบอร์ด');
  }
  function initControlGauge() {
    const ns = 'http://www.w3.org/2000/svg';
    for (let value = -MAX_DUTY; value <= MAX_DUTY; value += 5) {
      const angle = value * DUTY_GAUGE_DEG * Math.PI / 180;
      const major = value % 20 === 0;
      const line = document.createElementNS(ns, 'line');
      const point = (radius) => [220 + Math.sin(angle) * radius, 205 - Math.cos(angle) * radius];
      const a = point(major ? 147 : 153), b = point(161);
      for (const [key, number] of Object.entries({x1:a[0], y1:a[1], x2:b[0], y2:b[1]})) line.setAttribute(key, number);
      line.setAttribute('class', value === 0 ? 'tick zero' : 'tick');
      $('gaugeTicks').append(line);
      if (major) {
        const label = document.createElementNS(ns, 'text'); const p = point(128);
        label.setAttribute('x', p[0]); label.setAttribute('y', p[1] + 4);
        label.setAttribute('class', 'gauge-scale'); label.textContent = signedText(value);
        $('gaugeTicks').append(label);
      }
    }
    const gauge = $('controlGauge');
    function fromPointer(event) {
      const point = new DOMPoint(event.clientX, event.clientY).matrixTransform(gauge.getScreenCTM().inverse());
      const dx = point.x - 220, dy = point.y - 205;
      if (Math.hypot(dx, dy) < 90) return;
      setDraft(Math.atan2(dx, -dy) * 180 / Math.PI / DUTY_GAUGE_DEG);
    }
    gauge.addEventListener('pointerdown', (event) => { if (event.button !== 0) return; gauge.setPointerCapture(event.pointerId); fromPointer(event); });
    gauge.addEventListener('pointermove', (event) => { if (gauge.hasPointerCapture(event.pointerId)) fromPointer(event); });
    gauge.addEventListener('pointerup', (event) => { if (gauge.hasPointerCapture(event.pointerId)) gauge.releasePointerCapture(event.pointerId); });
    gauge.addEventListener('keydown', (event) => {
      if (!['ArrowLeft','ArrowDown','ArrowRight','ArrowUp','Home'].includes(event.key)) return;
      event.preventDefault();
      const current = validNumber($('dutyInput'), -MAX_DUTY, MAX_DUTY) ?? 0;
      const delta = ['ArrowLeft','ArrowDown'].includes(event.key) ? -1 : 1;
      setDraft(event.key === 'Home' ? 0 : current + delta * (current === 0 ? 5 : 1));
    });
  }
  function render() {
    const s = app.status;
    const now = performance.now();
    const fresh = !!s && app.ready && !document.hidden && now - app.lastStatusAt < LINK_STALE_MS &&
      now - app.lastHeartbeatAt < LINK_STALE_MS;
    if (app.ready && !document.hidden && (ACTIVE.has(s?.state) || s?.state === 'ARMED_IDLE') && !fresh) {
      app.ready = false;
      note(L('Live status or heartbeat lost during RUN · sending STOP; firmware timeout remains active','ข้อมูลสดหรือ heartbeat ขาดระหว่าง RUN · ส่ง STOP และปล่อยให้ firmware timeout'), 'error');
      void transmit('STOP').catch(() => {});
    }
    $('linkLamp').className = `lamp ${fresh ? 'online' : app.port ? 'stale' : ''}`;
    $('linkState').textContent = fresh ? 'LINK LIVE' : app.port ? 'LINK STALE' : 'OFFLINE';
    $('linkDetail').textContent = fresh ? 'STM32F405 / USB CDC' : app.port ? L('Waiting for STATUS or HEARTBEAT','รอ STATUS หรือ HEARTBEAT') : L('USB Serial disconnected','USB Serial ยังไม่เชื่อมต่อ');
    $('connectBtn').disabled = !!app.port;
    $('portSelect').disabled = !!app.port;
    $('disconnectBtn').disabled = !app.port;
    const running = ACTIVE.has(s?.state);
    $('stateValue').textContent = s?.state || '—';
    $('stateValue').className = s?.state === 'FAULT' ? 'fault' : running ? 'running' : '';
    $('faultValue').textContent = `FAULT ${s?.fault || '—'}`;
    $('faultValue').className = s?.fault && s.fault !== 'none' ? 'fault' : '';
    $('statusAge').textContent = s ? `STATUS ${(now - app.lastStatusAt).toFixed(0)} ms ago` : L('Waiting for data','รอข้อมูล');
    const target = integer(s?.duty_target_pct);
    const effective = integer(s?.duty_effective_pct_tenths);
    const pulse = integer(s?.duty_pulse_pct_tenths);
    $('effectiveDuty').textContent = effective === null ? '—' : (effective / 10).toFixed(1);
    $('targetDuty').textContent = target ?? '—';
    $('pulseDuty').textContent = pulse === null ? '—' : (pulse / 10).toFixed(1);
    $('dutyDial').style.setProperty('--progress', `${Math.min(78, Math.max(0, (effective ?? 0) / 10 * .78))}%`);
    $('pwmValue').textContent = integer(s?.pwm_hz)?.toLocaleString() || '—';
    $('directionValue').textContent = s?.direction?.toUpperCase() || '—';
    $('stepValue').textContent = s?.step ? `${s.step} / ${s.floating_phase || '—'}` : '—';
    $('lockValue').textContent = s?.bemf_lock_count ? `${s.bemf_lock_count} / ${s.bemf_lock_required || '6'}` : s ? `0 / ${s.bemf_lock_required || '6'}` : '—';
    $('closedValue').textContent = integer(s?.closed_loop_steps)?.toLocaleString() || '—';
    $('periodValue').textContent = integer(s?.target_period_us)?.toLocaleString() || '—';
    $('measuredValue').textContent = integer(s?.measured_period_us)?.toLocaleString() || '—';
    const currentAge = integer(s?.current_age_us);
    const currentMa = signedInteger(s?.current_estimate_mA);
    const currentValid = fresh && app.currentSupported && s.current_sample === '1' &&
      /^[UVW]$/.test(s.current_phase || '') && currentAge !== null &&
      currentAge <= 100000 && currentMa !== null && now - app.lastStatusAt < 900;
    $('lowSideCurrentValue').textContent = currentValid ? `~${(currentMa / 1000).toFixed(2)} A` : '—';
    $('currentPhaseLabel').textContent = currentValid
      ? L(`LOW ${s.current_phase} · PWM ON · ${(currentAge / 1000).toFixed(1)} ms at capture`,
          `เฟส LOW ${s.current_phase} · PWM ON · อายุ ${(currentAge / 1000).toFixed(1)} ms ตอนส่ง`)
      : !app.currentSupported ? L('Requires v34 firmware','ต้องใช้เฟิร์มแวร์ v34')
      : s?.current_zero !== 'READY' ? L('Zero calibration required','ต้องสอบศูนย์ก่อน')
      : L('Waiting for a valid PWM sample','รอตัวอย่าง PWM ที่ใช้ได้');
    $('currentMonitorValue').textContent = !app.currentSupported ? 'N/A'
      : !fresh ? L('STALE','ข้อมูลเก่า')
      : s.current_monitor_ok !== '1' ? L('CHECK','ตรวจสอบ')
      : s.current_zero === 'BUSY' ? L('ZEROING','สอบศูนย์')
      : s.current_scan !== 'ON' ? 'OFF'
      : s.current_zero !== 'READY' ? L('ZERO NEEDED','รอสอบศูนย์')
      : currentValid ? L('LIVE','สด') : L('WAITING','รอข้อมูล');
    $('currentMonitorDetail').textContent = app.currentSupported
      ? L('INA181A1 · 1 mΩ · nominal 3.3 V ADC scale','INA181A1 · 1 mΩ · สเกล ADC 3.3 V สมมติ')
      : L('Current telemetry is not in v33','v33 ยังไม่มีข้อมูลกระแสให้แอป');
    $('currentHint').textContent = app.currentSupported
      ? L('One LOW-side shunt per sample · nominal amperes; not RMS, Id/Iq or a current trip',
          'วัดชันต์เฟส LOW ทีละช่อง · แอมป์ยังเป็นค่าประมาณ ไม่ใช่ RMS, Id/Iq หรือวงจรตัดกระแส')
      : L('Flash v34 to enable current measurement; v33 motor control still works',
          'แฟลช v34 เพื่อวัดกระแส · v33 ยังใช้ควบคุมมอเตอร์ได้');
    $('currentScanBtn').disabled = !app.currentSupported || !fresh || running || s.current_zero === 'BUSY';
    $('currentScanBtn').textContent = s?.current_scan === 'ON'
      ? L('Disable current','ปิดวัดกระแส') : L('Enable current','เปิดวัดกระแส');
    $('currentZeroBtn').disabled = !app.currentSupported || !fresh || running ||
      s.current_scan !== 'ON' || s.current_monitor_ok !== '1' || s.current_zero === 'BUSY';
    $('currentZeroBtn').textContent = s?.current_zero === 'BUSY'
      ? L('Zeroing…','กำลังสอบศูนย์…') : L('Zero calibrate','สอบศูนย์');
    $('healthValue').textContent = s ? [s.init_ok, s.timer_ok, s.iwdg_ok].every((x) => x === '1') ? 'READY' : 'CHECK' : '—';
    $('heartbeatValue').textContent = s?.heartbeat || '—';
    $('armBtn').disabled = !fresh || running || s.state === 'ARMED_IDLE';
    $('armBtn').textContent = s?.state === 'FAULT' ? 'RESET / ARM' : 'ARM';
    $('armHelp').textContent = !app.port ? L('Connect the board to enable ARM','เชื่อมต่อบอร์ดเพื่อเปิดปุ่ม ARM')
      : !fresh ? L('Waiting for live board status','รอสถานะสดจากบอร์ด')
      : running ? L('Motor active · STOP before arming again','มอเตอร์ทำงานอยู่ · กด STOP ก่อน ARM อีกครั้ง')
      : s.state === 'ARMED_IDLE' ? L('Board armed · press RUN to start','บอร์ด ARM แล้ว · กด RUN เพื่อเริ่ม')
      : L('Press ARM, then RUN','กด ARM แล้วจึงกด RUN');
    $('runBtn').disabled = !fresh || s.state !== 'ARMED_IDLE';
    $('stopBtn').disabled = !app.writer;
    $('stopQuickBtn').disabled = !app.writer;
    $('setDutyBtn').disabled = !fresh;
    $('dutySlider').disabled = false;
    renderControlGauge();
    const idle = fresh && !running;
    $('setPeriodBtn').disabled = !idle;
    $('setStartBtn').disabled = !idle || !app.startupProfileSupported;
    $('setRampBtn').disabled = !idle || !app.startupProfileSupported;
    $('setResponseBtn').disabled = !idle || !app.dutyResponseSupported;
    $('forwardBtn').disabled = !idle;
    $('reverseBtn').disabled = !idle;
    for (const [overview, original] of [
      ['overviewArmBtn','armBtn'], ['overviewRunBtn','runBtn'], ['overviewStopBtn','stopBtn'],
      ['overviewSetDutyBtn','setDutyBtn'], ['overviewSetPeriodBtn','setPeriodBtn'],
      ['overviewSetStartBtn','setStartBtn'], ['overviewSetRampBtn','setRampBtn'],
      ['overviewSetResponseBtn','setResponseBtn'],
      ['overviewForwardBtn','forwardBtn'], ['overviewReverseBtn','reverseBtn']
    ]) $(overview).disabled = $(original).disabled;
    $('overviewArmBtn').textContent = $('armBtn').textContent;
    $('overviewControlState').textContent = !app.port ? L('Connect board to enable controls','เชื่อมต่อบอร์ดเพื่อใช้การควบคุม')
      : !fresh ? L('Waiting for live board status','รอสถานะสดจากบอร์ด')
      : s.state === 'ARMED_IDLE' ? L('Armed · press RUN','ARM แล้ว · กด RUN')
      : running ? L('Motor active · STOP before reversing','มอเตอร์ทำงาน · หยุดก่อนกลับทิศ')
      : L('Board ready for commands','บอร์ดพร้อมรับคำสั่ง');
    const initial = integer(s?.startup_initial_period_us), ramp = integer(s?.startup_ramp_divisor);
    const response = integer(s?.duty_ramp_pct_per_s);
    $('overviewProfileApplied').textContent = app.startupProfileSupported && initial !== null && ramp !== null
      ? L(`BOARD STARTUP: initial ${initial.toLocaleString()} µs/step · ramp divisor ${ramp} (lower = faster) · duty response ${response ?? '—'}%/s`,
          `ค่าที่บอร์ดใช้: เริ่ม ${initial.toLocaleString()} µs/สเต็ป · ตัวหารการเร่ง ${ramp} (น้อย = เร็วขึ้น) · duty ${response ?? '—'}%/วินาที`)
      : L('BOARD STARTUP: update firmware to enable tuning','ค่าสตาร์ท: อัปเดตเฟิร์มแวร์เพื่อปรับได้');
    $('statusBtn').disabled = !app.writer;
    $('diagBtn').disabled = !idle;
    $('consoleInput').disabled = !app.writer;
    $('consoleSendBtn').disabled = !app.writer;
    renderScopeState();
  }

  function showPage(name) {
    if (!['dashboard', 'control', 'rt', 'phases', 'scope', 'diagnostics'].includes(name)) return;
    app.page = name;
    document.querySelectorAll('.page').forEach((page) => page.classList.toggle('active', page.id === `page-${name}`));
    document.querySelectorAll('.nav-item').forEach((button) => button.classList.toggle('active', button.dataset.page === name));
    window.SATCharts.setActive(name === 'rt' || name === 'scope');
    if (name === 'scope') window.SATScopePlot.draw(app.scopeSamples);
  }

  async function action(command, kind) {
    try {
      const line = await request(command, kind);
      note(line);
      if (kind !== 'STOP') await request('STATUS', 'STATUS', 2800);
      else void request('STATUS', 'STATUS', 2800).catch(() => {});
    } catch (error) { note(error.message, 'error'); }
  }

  function validNumber(input, minimum, maximum) {
    const number = Number(input.value);
    return Number.isInteger(number) && number >= minimum && number <= maximum ? number : null;
  }

  function exportCsv() {
    const lines = ['source,board_t_us,elapsed_s,wall_iso,u_adc_counts,v_adc_counts,w_adc_counts,phase_valid,phase_age_us,floating_phase,bemf_error_counts_x3'];
    for (const p of app.phaseSamples)
      lines.push(`F405_v33,${p.boardUs},${p.t.toFixed(6)},${p.wall},${p.u},${p.v},${p.w},${p.valid ? 1 : 0},${p.age},${p.floating ?? ''},${p.bemfError ?? ''}`);
    const blob = new Blob([lines.join('\r\n')], { type: 'text/csv;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a'); link.href = url; link.download = `SAT-motor-pilot-phase-${Date.now()}.csv`; link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  $('connectBtn').addEventListener('click', connect);
  $('refreshPortsBtn').addEventListener('click', () => { void refreshPorts(); });
  $('disconnectBtn').addEventListener('click', () => void disconnect(true));
  document.querySelectorAll('.nav-item').forEach((button) => button.addEventListener('click', () => showPage(button.dataset.page)));
  $('armBtn').addEventListener('click', async () => {
    try { await request('HEARTBEAT', 'HEARTBEAT'); app.lastHeartbeatAt = performance.now(); await action('ARM', 'ARM'); }
    catch (error) { note(error.message, 'error'); }
  });
  $('runBtn').addEventListener('click', () => void action('RUN', 'RUN'));
  $('stopBtn').addEventListener('click', () => void action('STOP', 'STOP'));
  $('stopQuickBtn').addEventListener('click', () => void action('STOP', 'STOP'));
  $('currentScanBtn').addEventListener('click', () => {
    if (!app.currentSupported || ACTIVE.has(app.status?.state)) return;
    void action(app.status?.current_scan === 'ON' ? 'CSCAN OFF' : 'CSCAN ON', 'CSCAN');
  });
  $('currentZeroBtn').addEventListener('click', () => {
    if (!app.currentSupported || ACTIVE.has(app.status?.state) || app.status?.current_scan !== 'ON') return;
    void action('CZERO', 'CZERO');
  });
  for (const [overview, original] of [
    ['overviewArmBtn','armBtn'], ['overviewRunBtn','runBtn'], ['overviewStopBtn','stopBtn'],
    ['overviewSetDutyBtn','setDutyBtn'], ['overviewSetPeriodBtn','setPeriodBtn'],
    ['overviewSetStartBtn','setStartBtn'], ['overviewSetRampBtn','setRampBtn'],
    ['overviewSetResponseBtn','setResponseBtn'],
    ['overviewForwardBtn','forwardBtn'], ['overviewReverseBtn','reverseBtn']
  ]) $(overview).addEventListener('click', () => $(original).click());
  $('overviewDutySlider').addEventListener('input', () => setDraft(Number($('overviewDutySlider').value)));
  $('overviewDutyInput').addEventListener('input', () => {
    const value = validNumber($('overviewDutyInput'), -MAX_DUTY, MAX_DUTY);
    $('dutyInput').value = $('overviewDutyInput').value;
    if (value !== null) {
      $('dutySlider').value = value;
      $('overviewDutySlider').value = value;
    }
    renderControlGauge();
  });
  $('overviewPeriodInput').addEventListener('input', () => { $('periodInput').value = $('overviewPeriodInput').value; });
  $('periodInput').addEventListener('input', () => { $('overviewPeriodInput').value = $('periodInput').value; });
  $('overviewStartInput').addEventListener('input', () => { $('startInput').value = $('overviewStartInput').value; });
  $('startInput').addEventListener('input', () => { $('overviewStartInput').value = $('startInput').value; });
  $('overviewRampSelect').addEventListener('change', () => { $('rampSelect').value = $('overviewRampSelect').value; });
  $('rampSelect').addEventListener('change', () => { $('overviewRampSelect').value = $('rampSelect').value; });
  $('overviewResponseSelect').addEventListener('change', () => { $('responseSelect').value = $('overviewResponseSelect').value; });
  $('responseSelect').addEventListener('change', () => { $('overviewResponseSelect').value = $('responseSelect').value; });
  $('setDutyBtn').addEventListener('click', () => {
    const value = validNumber($('dutyInput'), -MAX_DUTY, MAX_DUTY);
    if (value === null || (value !== 0 && Math.abs(value) < 5)) return note(L('Duty must be ±5 to ±80%, or 0 to STOP','Duty ใช้ ±5 ถึง ±80% หรือ 0 เพื่อ STOP'), 'warn');
    if (value === 0) return void action('STOP', 'STOP');
    if (ACTIVE.has(app.status?.state) && (value > 0) !== (app.status.direction === 'fwd')) return note(L('Press STOP before reversing direction','กด STOP ก่อนเปลี่ยนทิศหมุน'), 'warn');
    void action(`DUTY ${signedText(value)}`, 'DUTY');
  });
  $('dutySlider').addEventListener('input', () => setDraft(Number($('dutySlider').value)));
  $('dutyInput').addEventListener('input', () => {
    const value = validNumber($('dutyInput'), -MAX_DUTY, MAX_DUTY);
    if (value !== null) {
      $('dutySlider').value = value;
      $('overviewDutyInput').value = value;
      $('overviewDutySlider').value = value;
    }
    renderControlGauge();
  });
  $('setPeriodBtn').addEventListener('click', () => {
    const value = validNumber($('periodInput'), 600, 12000);
    if (value === null) return note(L('Startup period must be an integer from 600 to 12000 µs','Startup period ต้องเป็นจำนวนเต็ม 600–12000 µs'), 'warn');
    void action(`PERIOD ${value}`, 'PARAM');
  });
  $('setStartBtn').addEventListener('click', () => {
    const value = validNumber($('startInput'), 12000, 30000);
    if (value === null) return note(L('Initial step period must be 12000–30000 µs','คาบสเต็ปเริ่มต้นต้องอยู่ในช่วง 12000–30000 µs'), 'warn');
    void action(`STARTPERIOD ${value}`, 'PARAM');
  });
  $('setRampBtn').addEventListener('click', () => {
    const value = Number($('rampSelect').value);
    if (![150,200,300].includes(value)) return;
    void action(`RAMP ${value}`, 'PARAM');
  });
  $('setResponseBtn').addEventListener('click', () => {
    const value = Number($('responseSelect').value);
    if (![10,20,30].includes(value)) return;
    void action(`RESPONSE ${value}`, 'PARAM');
  });
  $('forwardBtn').addEventListener('click', () => void action('DIR FWD', 'PARAM'));
  $('reverseBtn').addEventListener('click', () => void action('DIR REV', 'PARAM'));
  $('statusBtn').addEventListener('click', () => void request('STATUS', 'STATUS').then((line) => logLine(line)).catch((error) => note(error.message, 'error')));
  $('diagBtn').addEventListener('click', () => void transmit('DIAG').catch((error) => note(error.message, 'error')));
  $('consoleForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const command = $('consoleInput').value.trim();
    if (!command) return;
    if (/[\r\n]/.test(command) || /[^\x20-\x7E]/.test(command) || encoder.encode(command).length > 63) {
      return note(L('Use one ASCII command, up to 63 characters','ใช้คำสั่งภาษาอังกฤษหนึ่งบรรทัด ไม่เกิน 63 ตัวอักษร'), 'warn');
    }
    $('consoleInput').value = '';
    const initialUpper = command.toUpperCase();
    const dutyMatch = initialUpper.match(/^(?:DUTY\s+([+-]?\d+)|([+-]\d+))$/);
    const dutyValue = dutyMatch ? Number(dutyMatch[1] ?? dutyMatch[2]) : null;
    if (dutyMatch && (!Number.isInteger(dutyValue) || (dutyValue !== 0 && (Math.abs(dutyValue) < 5 || Math.abs(dutyValue) > MAX_DUTY)))) {
      return note(L('Duty must be ±5 to ±80%, or 0 to STOP','Duty ใช้ ±5 ถึง ±80% หรือ 0 เพื่อ STOP'), 'warn');
    }
    const upper = dutyMatch && dutyValue === 0 ? 'STOP' : initialUpper;
    try {
      if (upper === 'STATUS' || upper === 'HELLO' || upper === 'HEARTBEAT') {
        const line = await request(upper, upper);
        if (upper === 'STATUS' || upper === 'HEARTBEAT') logLine(line);
        if (upper === 'HEARTBEAT') app.lastHeartbeatAt = performance.now();
      } else {
        await transmit(command);
      }
      $('consoleInput').focus();
    } catch (error) { note(error.message, 'error'); }
  });
  $('clearLogBtn').addEventListener('click', () => { app.log.length = 0; $('serialLog').textContent = ''; });
  $('clearBtn').addEventListener('click', () => {
    app.phaseSamples.length = 0;
    app.lastPhaseUs = null; app.phaseTimeUs = 0;
    $('sampleCount').textContent = '0 samples'; $('csvBtn').disabled = true;
  });
  $('csvBtn').addEventListener('click', exportCsv);
  $('scopeCaptureBtn').addEventListener('click', async () => {
    try {
      const reply = fields(await request('SCOPE CAPTURE','SCOPE',2800));
      if (reply.state !== 'CAPTURING') throw new Error('SCOPE: unexpected board reply');
      app.scopeCapturing=true; app.scopeReady=false; app.scopeExpected=0;
      app.scopeLiveFrames=0; app.scopeLastLiveAt=0;
      app.scopeSamples.length=0; window.SATScopePlot.draw([]);
      renderScopeState();
      clearTimeout(app.scopePollTimer);
      app.scopePollTimer=setTimeout(pollScope,300);
    } catch (error) { note(error.message,'error'); }
  });
  $('scopeLiveBtn').addEventListener('click', async () => {
    const wasLive = app.scopeLive;
    try {
      const reply = fields(await request(wasLive ? 'SCOPE LIVE OFF' : 'SCOPE LIVE ON', 'SCOPE', 2800));
      if (reply.state !== (wasLive ? 'LIVE_OFF' : 'LIVE_ON')) throw new Error('SCOPE: unexpected board reply');
      app.scopeLive = !wasLive; app.scopeLiveFrame = false;
      if (!wasLive) {
        app.scopeLiveFrames = 0;
        app.scopeLastLiveAt = 0;
        app.scopeSamples.length = 0;
        clearTimeout(app.scopePollTimer);
        app.scopePollTimer = setTimeout(pollScope, 500);
      }
      renderScopeState();
    } catch (error) { note(error.message, 'error'); }
  });
  async function downloadScopeCapture() {
    app.scopeSamples.length=0; app.scopeReading=true; renderScopeState();
    clearTimeout(app.scopeReadTimer);
    app.scopeReadTimer=setTimeout(() => {
      if (app.scopeReading) { app.scopeReading=false; app.scopeSamples.length=0;
        note(L('Scope transfer timed out','การรับข้อมูล Scope หมดเวลา'),'warn'); renderScopeState(); }
    },10000);
    try {
      const reply=fields(await request('SCOPE READ','SCOPE',3000));
      if (reply.state!=='READING') throw new Error('SCOPE: unexpected board reply');
      app.scopeExpected=integer(reply.count)??0;
    } catch (error) {
      clearTimeout(app.scopeReadTimer);app.scopeReading=false;
      note(error.message,'error');renderScopeState();
    }
  }
  $('scopeReadBtn').addEventListener('click', () => void downloadScopeCapture());
  $('scopeStopViewBtn').addEventListener('click', async () => {
    if (!app.scopeReady || !app.ready || !ACTIVE.has(app.status?.state)) return;
    app.scopeStopping = true; renderScopeState();
    try {
      const stopped = fields(await request('STOP', 'STOP', 2800));
      if (stopped.gates !== 'OFF' || ACTIVE.has(stopped.state))
        throw new Error(L('STOP was not confirmed by the board','บอร์ดยังไม่ยืนยันการหยุด'));
      let status = fields(await request('STATUS', 'STATUS', 2800));
      if (ACTIVE.has(status.state)) status = fields(await request('STATUS', 'STATUS', 2800));
      if (ACTIVE.has(status.state)) throw new Error(L('Board is still active · capture not downloaded','บอร์ดยังทำงานอยู่ · ยังไม่ดาวน์โหลด Scope'));
      app.scopeStopping = false;
      await downloadScopeCapture();
    } catch (error) {
      note(error.message, 'error');
    } finally {
      app.scopeStopping = false; renderScopeState();
    }
  });
  document.addEventListener('visibilitychange', () => {
    if (document.hidden && app.writer && ACTIVE.has(app.status?.state)) {
      void action('STOP', 'STOP');
    }
    render();
    window.SATCharts.setActive(app.page === 'rt' || app.page === 'scope');
  });
  window.addEventListener('sat-language-change', () => { render(); renderPhase(); void refreshPorts(); });
  if ('serial' in navigator) navigator.serial.addEventListener('disconnect', (event) => {
    void refreshPorts();
    if (app.port && (!event.port || event.port === app.port)) {
      note(L('Board unplugged or USB lost · firmware heartbeat timeout will stop the drive','บอร์ดถูกถอดหรือ USB หลุด · รอ heartbeat timeout บนเฟิร์มแวร์'), 'error');
      void disconnect(false);
    }
  });
  if ('serial' in navigator) {
    void refreshPorts();
    navigator.serial.addEventListener('connect', () => { void refreshPorts(); });
  }
  $('portSelect').addEventListener('change', () => {
    if (window.satNative) return;
    const index = $('portSelect').value;
    if (index !== '' && !app.availablePorts[Number(index)]) void refreshPorts();
  });
  setInterval(render, 250);
  initControlGauge(); render(); renderPhase();
})();
