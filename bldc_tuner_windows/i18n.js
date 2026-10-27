(() => {
  'use strict';
  const dictionary = {
    'USB Serial ยังไม่เชื่อมต่อ':'USB Serial disconnected',
    'เชื่อมต่อบอร์ด':'Connect board', 'ตัดการเชื่อมต่อ':'Disconnect',
    'ภาพรวม':'Overview', 'ควบคุมมอเตอร์':'Motor control', 'RT / กราฟสด':'RT / Live charts',
    'ค่าสามเฟส':'Three phases', 'วินิจฉัย':'Diagnostics',
    'จับข้อมูลดิบจาก ADC ขณะมอเตอร์หมุน แล้วให้คอมวาดรูปคลื่น':'Capture raw ADC samples while the motor runs; the PC plots the waveform.',
    '3 × (เฟสลอย − ค่าเฉลี่ยสามเฟส) · ADC counts · 10 Hz':'3 × (floating phase − three-phase mean) · ADC counts · 10 Hz',
    'ใช้ Edge หรือ Chrome บนเครื่องที่ต่อ STM32F405 ผ่าน USB แล้วกดเชื่อมต่อบอร์ด':'Use Edge or Chrome on the PC connected to the STM32F405, then select Connect board.',
    'ข้อมูลจากเฟิร์มแวร์ v33/v34 ผ่าน USB CDC':'Live data from firmware v33/v34 over USB CDC',
    'ค่าที่บอร์ดใช้จริง':'Board reported output', 'รอข้อมูล':'Waiting for data',
    'ยังไม่มีค่าที่สอบเทียบแล้ว':'Calibrated values unavailable',
    'แรงดันบัส · กระแส · RPM · อุณหภูมิ':'Bus voltage · current · RPM · temperature',
    'Id / กระแสแกน d':'Id / D-AXIS CURRENT',
    'Iq / กระแสแกน q':'Iq / Q-AXIS CURRENT',
    'ยังวัดไม่ได้ · ต้องสอบเทียบกระแสเฟสและทราบมุมโรเตอร์':'Unmeasured · needs calibrated phase current and rotor angle',
    'กระแสชันต์เฟส LOW':'LOW-SIDE SHUNT CURRENT',
    'สถานะการวัดกระแส':'CURRENT MONITOR',
    'รอการสอบศูนย์':'Waiting for zero calibration',
    'กระแสที่แสดงเป็นค่าประมาณหลังสอบศูนย์':'Current shown is a nominal estimate after zero calibration',
    'ต้องเทียบกับกระแสอ้างอิงก่อนใช้ตัดสินใจ · ยังไม่มี current trip':'Compare with a known current before relying on this value · no current trip',
    'เปิดวัดกระแส':'Enable current',
    'สอบศูนย์':'Zero calibrate',
    'SET DUTY ยืนยันค่าที่ปรับ · 0 ส่ง STOP · บอร์ดตรวจขอบเขตค่าเอง · เปลี่ยนทิศและช่วงเริ่มหมุนได้เมื่อหยุดแล้ว':'SET DUTY confirms the draft; 0 sends STOP. The board validates each value. Change direction and startup period only while stopped.',
    '12,000–30,000 µs · ลดลงเมื่อมอเตอร์รับการสตาร์ทที่เร็วขึ้นได้':'12,000–30,000 µs · decrease only after the motor accepts faster starts',
    'Faster ไล่สเต็ปเร็วขึ้น · ตั้งได้เมื่อหยุดเท่านั้น':'Faster advances steps more quickly · set only while stopped',
    '10–30 percentage points/s · ตั้งได้เมื่อหยุดเท่านั้น':'10–30 percentage points/s · set only while stopped',
    'ตั้งค่าเฉพาะที่เฟิร์มแวร์รองรับ แล้วอ่านค่าที่บอร์ดรับจริง':'Set supported values and read back what the board accepted.',
    'บอร์ดเป็นผู้ตรวจขอบเขตค่าทุกคำสั่ง':'The board validates every command.',
    'คำสั่ง: HELP · HELLO · STATUS · DIAG · ARM · RUN · STOP · DUTY <±5–80%> · DIR FWD/REV · STARTPERIOD <µs> · RAMP <ค่า> · RESPONSE <%/s> · SCOPE …':'Commands: HELP · HELLO · STATUS · DIAG · ARM · RUN · STOP · DUTY <±5–80%> · DIR FWD/REV · STARTPERIOD <µs> · RAMP <value> · RESPONSE <%/s> · SCOPE …',
    'ARM / RUN / DUTY สั่งการมอเตอร์โดยตรง บอร์ดเป็นผู้ตรวจสอบและปฏิเสธคำสั่งที่ไม่ผ่านเงื่อนไข':'ARM / RUN / DUTY directly control the motor. The board validates and rejects commands that do not meet its conditions.',
    'พิมพ์คำสั่งบอร์ด':'Type a board command',
    'ส่งคำสั่ง':'SEND',
    '% จากบอร์ด':'% from board', 'ค่าที่กำลังปรับ':'Draft target',
    '● ค่าที่ปรับ':'● Draft target', '● Effective จากบอร์ด':'● Board effective',
    'เป้าหมายที่บอร์ดรับ: —':'Board target: —',
    'ลากรอบเกจหรือใช้ปุ่มลูกศร แล้วกด SET DUTY':'Drag the gauge or use arrow keys, then press SET DUTY.',
    '−80 ถึง −5 / 0 STOP / +5 ถึง +80%':'−80 to −5 / 0 STOP / +5 to +80%',
    '600–12000 µs · หยุดอยู่เท่านั้น':'600–12000 µs · while stopped',
    'เปลี่ยนได้เมื่อหยุดแล้ว':'Change only when stopped',
    'ARM → RUN เริ่ม ALIGN และ STARTUP':'ARM → RUN starts ALIGN and STARTUP',
    'เชื่อมต่อบอร์ดเพื่อเปิดปุ่ม ARM':'Connect the board to enable ARM',
    'STOP ส่งคำสั่งหยุดทันทีเมื่อ USB ยังเชื่อมต่ออยู่ หากสายหลุด เฟิร์มแวร์จะหยุดเมื่อ heartbeat หมดอายุ 1.5 วินาที พฤติกรรมหยุดจริงต้องยืนยันบนบอร์ดก่อนใช้งานกับมอเตอร์':'STOP requests an immediate stop while USB is connected. After a cable loss, the firmware stops on the 1.5 s heartbeat timeout. Verify physical gate-off on the board before running a motor.',
    'กราฟจากค่าที่บอร์ดส่งจริง · หน้าจอวาดบนคอมพิวเตอร์':'Charts use board data; the PC renders and stores the history.',
    '01 / ADC สามเฟส':'01 / Three-phase ADC',
    'ADC counts · ยังไม่สอบเทียบเป็นโวลต์':'ADC counts · voltage conversion unverified',
    'รอข้อมูลจากบอร์ด':'Waiting for board data',
    '3 × (เฟสลอย − ค่าเฉลี่ยสามเฟส) · ADC counts':'3 × (floating phase − three-phase mean) · ADC counts',
    'แสดงเส้น U/V/W แยกสี โดยเชื่อมตัวอย่างจริงของเฟสเดียวกัน · ข้อมูล 10 Hz ใช้ดูแนวโน้ม ไม่ใช่รูปคลื่น BEMF ความถี่สูงหรือค่าโวลต์ที่สอบเทียบ':'Separate colored U/V/W lines connect actual samples from the same phase. The 10 Hz stream shows trends, not the high-frequency BEMF waveform or calibrated volts.',
    'ข้อมูลดิบ U/V/W ระหว่าง CLOSED_LOOP · เฟรมสด 256 จุด หรือภาพจับ 512 จุดหลัง STOP':'Raw U/V/W in CLOSED_LOOP · 256 point live frames or 512 point snapshots after STOP',
    'รอเชื่อมต่อบอร์ด · จับข้อมูลได้เมื่อมอเตอร์อยู่ใน CLOSED_LOOP':'Connect the board · capture is available in CLOSED_LOOP',
    'กด START LIVE VIEW ระหว่าง CLOSED_LOOP เพื่อดูเฟรมที่บอร์ดวัดจริงต่อเนื่อง ปุ่ม PAUSE LIVE VIEW หยุดส่งกราฟเท่านั้น มอเตอร์ยังทำงาน · โหมด CAPTURE แยกต่างหาก: จับข้อมูล → STOP & SHOW WAVEFORM · เฟรมสดอาจข้ามได้เมื่อ USB ส่งไม่ทัน ค่ายังไม่สอบเทียบเป็นโวลต์':'Press START LIVE VIEW in CLOSED_LOOP for repeated real board frames. PAUSE LIVE VIEW stops plotting only; the motor keeps running. Separate CAPTURE mode: capture → STOP & SHOW WAVEFORM. Live frames may be skipped when USB cannot keep up. Values are not calibrated volts.',
    '03 / คาบการสับเฟส':'03 / Commutation period',
    'ยังไม่มีข้อมูลสด':'No live data yet',
    'ช่วงเวลา 30 วินาที · ช่องว่างคือข้อมูลที่ขาดหรือใช้ไม่ได้':'30 s window · gaps mark missing or invalid samples',
    'ค่าดิบจาก ADC สามเฟส พร้อมเวลาและสถานะ sample สำหรับทำกราฟภายหลัง':'Raw phase ADC values with timestamp and sample quality for CSV export.',
    'ค่า ADC ดิบที่บอร์ดส่งทุก 100 ms · ไม่ใช่แรงดันที่สอบเทียบ':'Raw ADC sample every 100 ms · not calibrated voltage',
    'ข้อมูลพร้อมสำหรับทำกราฟ':'Graph-ready data',
    'แต่ละชุดมี timestamp เดียวสำหรับ U/V/W พร้อม phase_valid และ phase_age_us เก็บในหน่วยความจำสูงสุด 1,800 ตัวอย่าง และส่งออก CSV ได้':'Each U/V/W sample shares a timestamp, phase_valid, and phase_age_us. Up to 1,800 samples are kept in PC memory and can be exported as CSV.',
    'ตรวจการเปลี่ยนสถานะและคำตอบจากบอร์ด':'Inspect state changes and board replies.',
    'ล่าสุด 8 รายการ':'Latest 8 events', 'รอข้อมูลสถานะ':'Waiting for status',
    'ปรับ Duty ลบหรือบวก ศูนย์คือหยุด กด SET DUTY เพื่อยืนยัน':'Adjust signed duty. Zero stops. Press SET DUTY to confirm.'
  };
  const staticNodes = [];
  const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
  while (walker.nextNode()) {
    const node=walker.currentNode, original=node.nodeValue, trimmed=original.trim();
    if (Object.hasOwn(dictionary,trimmed)) staticNodes.push({node,original,english:dictionary[trimmed]});
  }
  const gauge=document.getElementById('controlGauge');
  const originalGaugeLabel=gauge.getAttribute('aria-label');
  const ariaLabels={
    'หน้าหลัก':'Main pages',
    'กราฟ ADC สามเฟส U V W':'Three-phase ADC chart U V W',
    'กราฟ BEMF เฉพาะเฟสลอย U V W':'Floating-phase BEMF chart U V W',
    'กราฟสโคปแรงดันปลายเฟส U V W':'Oscilloscope capture of U V W phase terminal voltage',
    'กราฟสโคป back EMF เฉพาะช่วงเฟสลอย':'Oscilloscope capture of floating-phase back EMF',
    'กราฟ Duty เป้าหมาย ค่าที่บอร์ดใช้ และพัลส์':'Target, effective, and pulse duty chart',
    'กราฟคาบการสับเฟส':'Commutation period chart',
    'กราฟจำนวน BEMF lock และค่าที่ต้องการ':'BEMF lock and required count chart'
  };
  const labelled=Array.from(document.querySelectorAll('[aria-label]'))
    .map((element)=>({element,original:element.getAttribute('aria-label')}))
    .filter((item)=>Object.hasOwn(ariaLabels,item.original));
  let language=localStorage.getItem('sat-language')==='th'?'th':'en';
  const pick=(english,thai)=>language==='th'?thai:english;
  function apply() {
    document.documentElement.lang=language;
    for (const item of staticNodes) {
      const original=item.original, lead=original.match(/^\s*/)[0], tail=original.match(/\s*$/)[0];
      item.node.nodeValue=language==='th'?original:lead+item.english+tail;
    }
    gauge.setAttribute('aria-label',language==='th'?originalGaugeLabel:dictionary[originalGaugeLabel]);
    for (const item of labelled) item.element.setAttribute('aria-label',language==='th'?item.original:ariaLabels[item.original]);
    document.getElementById('languageBtn').textContent=language==='en'?'ไทย':'English';
    document.getElementById('consoleInput').placeholder=pick('Type a command, e.g. HELP or STATUS','พิมพ์คำสั่ง เช่น HELP หรือ STATUS');
    document.getElementById('consoleInput').setAttribute('aria-label',pick('Serial command','คำสั่ง Serial'));
    document.getElementById('languageBtn').setAttribute('aria-label',pick('Switch to Thai','เปลี่ยนเป็นภาษาอังกฤษ'));
    window.dispatchEvent(new Event('sat-language-change'));
  }
  document.getElementById('languageBtn').addEventListener('click',()=>{
    language=language==='en'?'th':'en';localStorage.setItem('sat-language',language);apply();
  });
  window.SATLang={pick, get language(){return language;}, t(key,args={}){
    if (key==='rtData') return pick(`${args.phase} phase / ${args.status} status samples`,`${args.phase} ตัวอย่างเฟส / ${args.status} ตัวอย่างสถานะ`);
    return key;
  }};
  apply();
})();
