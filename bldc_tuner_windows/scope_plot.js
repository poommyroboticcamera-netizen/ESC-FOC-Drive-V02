(() => {
  'use strict';
  const colors=['#35b7ff','#ff605b','#f1d84c'];
  let snapshot=[];
  function drawOne(canvasId, bemf) {
    const canvas=document.getElementById(canvasId), width=canvas.clientWidth, height=canvas.clientHeight;
    if (!width || !height) return;
    const dpr=Math.min(window.devicePixelRatio||1,2);
    canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);
    const ctx=canvas.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);
    ctx.fillStyle='#0b1219';ctx.fillRect(0,0,width,height);
    const left=45,right=9,top=10,bottom=23, w=Math.max(1,width-left-right),h=Math.max(1,height-top-bottom);
    const scale=window.SATChartScale.get(canvasId);
    let dataMin=Infinity,dataMax=-Infinity;
    for(const p of snapshot){
      if(bemf){
        const phase='UVW'.indexOf(p.f);
        if(phase<0)continue;
        const value=3*p['uvw'[phase]]-p.u-p.v-p.w;
        if(Number.isFinite(value)){dataMin=Math.min(dataMin,value);dataMax=Math.max(dataMax,value);}
      }else{
        for(const phase of ['u','v','w']){
          const value=p[phase];
          if(Number.isFinite(value)){dataMin=Math.min(dataMin,value);dataMax=Math.max(dataMax,value);}
        }
      }
    }
    if(!Number.isFinite(dataMin)||!Number.isFinite(dataMax)){dataMin=bemf?-8190:0;dataMax=bemf?8190:4095;}
    let mid,baseHalf;
    if(bemf){
      mid=0;
      baseHalf=Math.max(Math.abs(dataMin),Math.abs(dataMax),50)*1.12;
    }else{
      mid=(dataMin+dataMax)/2;
      const observedHalf=Math.max((dataMax-dataMin)/2,8);
      baseHalf=Math.min(Math.max(mid,4095-mid),observedHalf*1.12);
    }
    const half=baseHalf*scale.y;
    const min=mid-half,max=mid+half;
    ctx.font='10px Consolas,monospace';ctx.textAlign='right';ctx.textBaseline='middle';
    for(let i=0;i<=4;i++){
      const y=top+h*i/4;
      ctx.strokeStyle='#263542';ctx.beginPath();ctx.moveTo(left,y+.5);ctx.lineTo(left+w,y+.5);ctx.stroke();
      ctx.fillStyle='#7991a1';ctx.fillText(String(Math.round(max-(max-min)*i/4)),left-5,y);
    }
    if(!snapshot.length){ctx.fillStyle='#7590a4';ctx.textAlign='center';ctx.fillText('NO CAPTURE',left+w/2,top+h/2);return;}
    const origin=snapshot[0].tUs, times=snapshot.map((p)=>(p.tUs-origin)>>>0);
    const totalDuration=Math.max(1,times.at(-1));
    const duration=totalDuration/scale.x;
    const start=(totalDuration-duration)/2;
    ctx.textAlign='center';ctx.textBaseline='top';
    for(let i=0;i<=4;i++){
      const x=left+w*i/4;
      ctx.strokeStyle='#21303b';ctx.beginPath();ctx.moveTo(x+.5,top);ctx.lineTo(x+.5,top+h);ctx.stroke();
      ctx.fillStyle='#7991a1';ctx.fillText(`${((start+duration*i/4)/1000).toFixed(1)} ms`,x,top+h+5);
    }
    ctx.save();ctx.beginPath();ctx.rect(left,top,w,h);ctx.clip();
    for(let phase=0;phase<3;phase++){
      ctx.strokeStyle=colors[phase];ctx.fillStyle=colors[phase];ctx.lineWidth=1.4;ctx.beginPath();
      let previous=null;
      for(let i=0;i<snapshot.length;i++){
        const p=snapshot[i], x=left+w*(times[i]-start)/duration;
        let value;
        if(bemf){
          if(p.f!=='UVW'[phase]) continue;
          value=3*p['uvw'[phase]]-p.u-p.v-p.w;
        } else value=p['uvw'[phase]];
        const y=top+h*(max-value)/(max-min);
        if(bemf){ctx.fillRect(x-2,y-2,4,4);continue;}
        if(previous!==null && times[i]-times[previous]<=500) ctx.lineTo(x,y);
        else {ctx.moveTo(x,y);ctx.fillRect(x-1.5,y-1.5,3,3);}
        previous=i;
      }
      if(!bemf)ctx.stroke();
    }
    ctx.restore();
  }
  function draw(samples){snapshot=samples.slice();drawOne('scopePhaseChart',false);drawOne('scopeBemfChart',true);}
  window.addEventListener('resize',()=>{if(document.getElementById('page-scope').classList.contains('active'))draw(snapshot);});
  window.addEventListener('sat-chart-scale',()=>{if(document.getElementById('page-scope').classList.contains('active'))draw(snapshot);});
  window.SATScopePlot={draw};
})();
