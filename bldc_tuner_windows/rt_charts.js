(() => {
  'use strict';
  const $ = (id) => document.getElementById(id);
  const WINDOW_MS = 30000;
  const MAX_POINTS = 1800;
  const specs = [
    { id: 'Phase', source: 'phase', keys: ['u','v','w'], colors: ['#35b7ff','#ff605b','#f1d84c'], max: 4095, gap: 260 },
    { id: 'Emf', source: 'phase', keys: ['bemfU','bemfV','bemfW'], colors: ['#35b7ff','#ff605b','#f1d84c'], min: -8190, max: 8190, gap: 700 },
    { id: 'Duty', source: 'status', keys: ['target','effective','pulse'], colors: ['#ffac50','#35b7ff','#a998f8'], min: -100, max: 100, gap: 1250 },
    { id: 'Period', source: 'status', keys: ['target','forced','measured'], colors: ['#ffac50','#a998f8','#35b7ff'], max: 12000, gap: 1250 },
    { id: 'Bemf', source: 'status', keys: ['lock','required'], colors: ['#80d98a','#ffac50'], max: 6, gap: 1250 }
  ];
  const phase = [], status = [];
  let phaseOrigin = null, phaseHostOrigin = null, raf = 0, lastPaint = 0;
  const num = (value) => typeof value === 'string' && /^\d+$/.test(value) ? Number(value) : null;
  function addPhase(sample, elapsedMs) {
    if (phaseOrigin === null) { phaseOrigin = elapsedMs; phaseHostOrigin = performance.now(); }
    const t = phaseHostOrigin + elapsedMs - phaseOrigin;
    const error = sample.valid && sample.floating ? 3 * ({U:sample.u,V:sample.v,W:sample.w}[sample.floating]) - sample.u - sample.v - sample.w : null;
    phase.push({ t, u:sample.valid ? sample.u : null, v:sample.valid ? sample.v : null,
      w:sample.valid ? sample.w : null,
      bemfU:sample.floating==='U'?error:null,
      bemfV:sample.floating==='V'?error:null,
      bemfW:sample.floating==='W'?error:null,
      valid:sample.valid, floating:sample.floating, age:sample.age });
    if (phase.length > MAX_POINTS) phase.shift();
  }
  function addStatus(s) {
    const sign = s.direction === 'rev' ? -1 : 1;
    const target = num(s.duty_target_pct), effective = num(s.duty_effective_pct_tenths), pulse = num(s.duty_pulse_pct_tenths);
    status.push({ t:performance.now(), target:target === null ? null : sign * target,
      effective:effective === null ? null : sign * effective / 10,
      pulse:pulse === null ? null : sign * pulse / 10,
      periodTarget:num(s.target_period_us), forced:num(s.forced_period_us), measured:num(s.measured_period_us),
      lock:num(s.bemf_lock_count), required:num(s.bemf_lock_required) });
    if (status.length > MAX_POINTS) status.shift();
  }
  function reset() { phase.length = 0; status.length = 0; phaseOrigin = null; phaseHostOrigin = null; }
  function paint(spec, now) {
    const canvas = $(`rt${spec.id}Chart`), empty = $(`rt${spec.id}Empty`);
    const width = canvas.clientWidth, height = canvas.clientHeight;
    if (!width || !height) return;
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    if (canvas.width !== Math.round(width*dpr) || canvas.height !== Math.round(height*dpr)) {
      canvas.width = Math.round(width*dpr); canvas.height = Math.round(height*dpr);
    }
    const ctx = canvas.getContext('2d'); ctx.setTransform(dpr,0,0,dpr,0,0);
    ctx.clearRect(0,0,width,height);
    const left=36, right=8, top=9, bottom=19, plotW=Math.max(1,width-left-right), plotH=Math.max(1,height-top-bottom);
    const scale=window.SATChartScale.get(canvas.id);
    const windowMs=WINDOW_MS/scale.x;
    const rows=(spec.source==='phase'?phase:status).filter((p)=>p.t>=now-windowMs-1500 && p.t<=now+250);
    empty.hidden=rows.some((row)=>spec.keys.some((key)=>Number.isFinite(row[key])));
    let yMin=spec.min ?? 0, yMax=spec.max;
    if (spec.id==='Emf') {
      const values=rows.flatMap((row)=>spec.keys.map((key)=>row[key]).filter(Number.isFinite));
      if (values.length) {
        const extent=Math.max(50,...values.map((value)=>Math.abs(value)))*1.2;
        yMin=-extent; yMax=extent;
      }
    } else if (spec.id==='Period') {
      const high=Math.max(12000,...rows.flatMap((p)=>[p.periodTarget,p.forced,p.measured].filter(Number.isFinite)));
      yMax=Math.ceil(high/3000)*3000;
    } else if (spec.id==='Bemf') {
      yMax=Math.max(6,...rows.map((p)=>p.required || 0));
    }
    const yMid=(yMin+yMax)/2, yHalf=(yMax-yMin)*scale.y/2;
    yMin=yMid-yHalf; yMax=yMid+yHalf;
    ctx.font='10px Consolas,monospace'; ctx.textAlign='right'; ctx.textBaseline='middle';
    for (let tick=0;tick<=4;tick++) {
      const y=top+plotH*tick/4;
      ctx.strokeStyle=tick===4?'#526271':'#25313d';ctx.lineWidth=1;
      ctx.beginPath();ctx.moveTo(left,y+.5);ctx.lineTo(left+plotW,y+.5);ctx.stroke();
      ctx.fillStyle='#698093';ctx.fillText(String(Math.round(yMax-(yMax-yMin)*tick/4)),left-5,y);
    }
    ctx.textBaseline='top';ctx.textAlign='center';
    for (let tick=0;tick<=3;tick++) {
      const x=left+plotW*tick/3;
      ctx.strokeStyle='#20303c';ctx.beginPath();ctx.moveTo(x+.5,top);ctx.lineTo(x+.5,top+plotH);ctx.stroke();
      ctx.fillStyle='#698093';ctx.fillText(`${Math.round((-windowMs+windowMs*tick/3)/1000)}s`,x,top+plotH+5);
    }
    for (let k=0;k<spec.keys.length;k++) {
      const key=spec.id==='Period'?['periodTarget','forced','measured'][k]:spec.keys[k];
      ctx.strokeStyle=spec.colors[k];ctx.lineWidth=1.8;ctx.beginPath();
      let previous=null, drawn=false;
      for (const row of rows) {
        const value=row[key];
        if (!Number.isFinite(value)) {
          if (spec.id!=='Emf') previous=null;
          continue;
        }
        const x=left+(row.t-(now-windowMs))/windowMs*plotW;
        const y=top+(yMax-value)/(yMax-yMin)*plotH;
        if (y<top || y>top+plotH) { previous=null; continue; }
        if (previous && row.t-previous.t<=spec.gap) ctx.lineTo(x,y);
        else ctx.moveTo(x,y);
        previous=row;drawn=true;
      }
      if (drawn) ctx.stroke();
    }
  }
  function updateLabels(now) {
    const p=phase.at(-1), s=status.at(-1), phaseAge=p?now-p.t:Infinity, statusAge=s?now-s.t:Infinity;
    const livePhase=phaseAge<600 && Number.isFinite(p?.u), liveStatus=statusAge<1600;
    $('rtPhaseValue').textContent=livePhase?`${p.u} / ${p.v} / ${p.w}`:'—';
    const bemfKey=p?.floating?`bemf${p.floating}`:null;
    const bemfValue=bemfKey?p[bemfKey]:null;
    $('rtEmfValue').textContent=phaseAge<600 && Number.isFinite(bemfValue)?`${p.floating}: ${bemfValue>0?'+':''}${bemfValue}`:'—';
    $('rtEmfState').textContent=!p?window.SATLang.pick('WAITING','รอข้อมูล'):
      phaseAge>=600?window.SATLang.pick('STALE','ข้อมูลเก่า'):
      !p.floating?window.SATLang.pick('FIRMWARE UPDATE NEEDED','ต้องอัปเดตเฟิร์มแวร์'):
      p.valid?window.SATLang.pick('FLOATING PHASE','เฟสลอย'):window.SATLang.pick('INVALID','ใช้ไม่ได้');
    $('rtDutyValue').textContent=liveStatus && s.effective!==null?`${s.effective>0?'+':''}${s.effective.toFixed(1)}%`:'—';
    $('rtPeriodValue').textContent=liveStatus && s.measured!==null?`${s.measured.toLocaleString()} µs`:'—';
    $('rtBemfValue').textContent=liveStatus && s.lock!==null?`${s.lock} / ${s.required ?? '—'}`:'—';
    const label=(en,th)=>window.SATLang.pick(en,th);
    $('rtPhaseState').textContent=!p?label('WAITING','รอข้อมูล'):phaseAge>=600?label('STALE','ข้อมูลเก่า'):p.valid?label('VALID','ใช้ได้'):label('INVALID','ใช้ไม่ได้');
    for (const id of ['Duty','Period','Bemf']) $( `rt${id}State`).textContent=!s?label('WAITING','รอข้อมูล'):liveStatus?label('LIVE','สด'):label('STALE','ข้อมูลเก่า');
    $('rtDataState').textContent=window.SATLang?.t ? window.SATLang.t('rtData', {phase:phase.length,status:status.length}) : `${phase.length} phase / ${status.length} status samples`;
  }
  function frame(now) {
    if (now-lastPaint>=32) { specs.forEach((spec)=>paint(spec,now)); updateLabels(now); lastPaint=now; }
    raf=requestAnimationFrame(frame);
  }
  function setActive(active) {
    if (active && !raf && !document.hidden) raf=requestAnimationFrame(frame);
    if ((!active || document.hidden) && raf) {cancelAnimationFrame(raf);raf=0;}
  }
  window.SATCharts={addPhase,addStatus,reset,setActive};
})();
