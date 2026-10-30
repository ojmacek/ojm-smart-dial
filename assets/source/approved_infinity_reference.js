  const TAU=Math.PI*2;
  const xy=p=>[p.x,p.y];
  const soft=x=>{x=clamp(x);return x*x*x*(10+x*(-15+6*x));};
  const lerp=(a,b,t)=>a+(b-a)*t;
  function curve(a,b,c,d,p){
    const q=1-p;
    return [q*q*q*a[0]+3*q*q*p*b[0]+3*q*p*p*c[0]+p*p*p*d[0],
      q*q*q*a[1]+3*q*q*p*b[1]+3*q*p*p*c[1]+p*p*p*d[1]];
  }
  const cubic=(a,b,c,d)=>Array.from({length:72},(_,i)=>curve(a,b,c,d,i/71));
  function worldPath(points){
    const length=[0];
    for(let i=1;i<points.length;i++)length.push(length[i-1]+Math.hypot(points[i][0]-points[i-1][0],points[i][1]-points[i-1][1]));
    return {p:points,length,total:length[length.length-1]};
  }
  function point(path,p){return xy(pathPoint(path,clamp(p)));}
  function joined(segments){
    const all=[],inks=[];let distance=0;
    segments.forEach(segment=>{
      const points=segment.points;
      const begin=distance;
      if(all.length&&Math.hypot(all[all.length-1][0]-points[0][0],all[all.length-1][1]-points[0][1])>.05)
        throw new Error('Disconnected signature segment');
      points.forEach((p,i)=>{
        if(i>0)distance+=Math.hypot(p[0]-points[i-1][0],p[1]-points[i-1][1]);
        if(i>0||!all.length)all.push(p);
      });
      if(segment.letter!==undefined)inks.push({letter:segment.letter,begin,end:distance,weight:segment.weight??26});
    });
    return {...worldPath(all),inks};
  }
  function star(ctx,p,alpha=1,size=3.15){
    if(alpha<=0)return;
    dot(ctx,...p,size*2.5,white,.038*alpha*settings.impact);
    dot(ctx,...p,size*1.65,white,.075*alpha*settings.impact);
    dot(ctx,...p,size,white,alpha);
    dot(ctx,p[0]-.8,p[1]-.9,size*.31,white,alpha);
  }
  function inkAlong(ctx,track,travel,alpha=1){
    const distance=clamp(travel)*track.total;
    maskedInk(ctx,track.inks.map(ink=>({path:penPaths[ink.letter],
      p:clamp((distance-ink.begin)/(ink.end-ink.begin)),weight:26,alpha})));
  }
  // A ribbon is a tapered strip with a bright edge, not a blur effect.
  // Depth is sorted explicitly, so the front of a crossing stays crisp.
  function ribbon(ctx,points,depths,options={}){
    const alpha=options.alpha??1,band=options.width??5,grow=clamp(options.grow??1);
    if(alpha<=0||grow<=0)return;
    const pieces=[],last=Math.min(points.length-1,Math.ceil(grow*(points.length-1)));
    for(let i=1;i<=last;i++){
      const a=points[i-1],raw=points[i],fraction=i===last?clamp(grow*(points.length-1)-(i-1)):1;
      const b=[lerp(a[0],raw[0],fraction),lerp(a[1],raw[1],fraction)];
      const dx=b[0]-a[0],dy=b[1]-a[1],length=Math.hypot(dx,dy);
      if(length<.001)continue;
      const z=clamp(depths[i]??.5),normal=[-dy/length,dx/length];
      const w=band*(.23+.77*z),taper=options.taper?Math.pow(i/last,.72):1;
      pieces.push({a,b,z,normal,w:w*taper});
    }
    pieces.sort((a,b)=>a.z-b.z);
    ctx.save();ctx.strokeStyle=white;ctx.lineCap='round';ctx.lineJoin='round';
    pieces.forEach(({a,b,z,normal,w})=>{
      ctx.globalAlpha=clamp(alpha*(.18+.52*z));ctx.lineWidth=Math.max(.4,w);
      ctx.beginPath();ctx.moveTo(...a);ctx.lineTo(...b);ctx.stroke();
      ctx.globalAlpha=clamp(alpha*(.35+.6*z));ctx.lineWidth=.7;
      ctx.beginPath();ctx.moveTo(a[0]+normal[0]*w*.42,a[1]+normal[1]*w*.42);
      ctx.lineTo(b[0]+normal[0]*w*.42,b[1]+normal[1]*w*.42);ctx.stroke();
      if(w>3){
        ctx.globalAlpha=alpha*.12;ctx.lineWidth=.5;
        ctx.beginPath();ctx.moveTo(a[0]-normal[0]*w*.42,a[1]-normal[1]*w*.42);
        ctx.lineTo(b[0]-normal[0]*w*.42,b[1]-normal[1]*w*.42);ctx.stroke();
      }
    });ctx.restore();
  }
  function ribbonTail(ctx,track,travel,span,alpha=1,band=5,twist=0){
    if(alpha<=0)return;
    const points=[],depths=[],begin=Math.max(0,travel-span),count=88;
    for(let i=0;i<count;i++){
      const p=lerp(begin,travel,i/(count-1));points.push(point(track,p));
      depths.push(.5+.5*Math.sin(p*TAU*4+twist));
    }
    ribbon(ctx,points,depths,{alpha,width:band,taper:true});
    star(ctx,point(track,travel),alpha,3.05);
  }
  function systemSweep(ctx,p,start=.82,end=1){
    const q=soft(phase(p,start,end));if(q<=0)return;
    const y=top+180*scale,h=top+height-y,edge=left-18+(width+48)*q;
    // Draw the original pixels directly, including at the final handoff.
    cropped(ctx,left,y,Math.max(0,Math.min(width,edge-25-left)),h);
    const strips=24,step=33/strips;
    for(let i=0;i<strips;i++){
      const x=edge-25+i*step,a=Math.max(left,x),b=Math.min(left+width,x+step);
      if(b>a)cropped(ctx,a,y,b-a,h,1-soft((i+.5)/strips));
    }
    if(q<.98)line(ctx,left+(width+28)*q-12,top+height+8,left+(width+28)*q+3,top+height+8,white,.8,
      Math.sin(Math.PI*q)*.6);
  }
  function settle(ctx,p,start=.92){
    cropped(ctx,left,top,width,assets.top[1]*scale,soft(phase(p,start,1)));
  }
  function orbitApproach(letter,turns=1.7,startAngle=-Math.PI/2,tilt=-.18){
    const destination=point(penPaths[letter],0);
    const rawAt=p=>{
      const a=startAngle+turns*TAU*p,capture=soft(phase(p,.26,1));
      const x=168*Math.cos(a),y=53*Math.sin(a);
      const raw=[C+x*Math.cos(tilt)-y*Math.sin(tilt),C+x*Math.sin(tilt)+y*Math.cos(tilt)];
      return raw.map((v,j)=>lerp(v,destination[j],capture));
    };
    const cut=.77,from=rawAt(cut),before=rawAt(cut-.0001);
    const gap=Math.hypot(destination[0]-from[0],destination[1]-from[1]);
    const dx=from[0]-before[0],dy=from[1]-before[1],length=Math.hypot(dx,dy)||1;
    const a=[from[0]+dx/length*gap*.42,from[1]+dy/length*gap*.42];
    const tangent=letter===1?[0,1]:letter===2?[0,-1]:[1,0];
    const b=destination.map((v,j)=>v-tangent[j]*gap*.35);
    return Array.from({length:360},(_,i)=>{
      const p=i/359;
      return p<=cut?rawAt(p):curve(from,a,b,destination,phase(p,cut,1));
    });
  }
  // A smooth spiral arrives tangent to O, then a single uninterrupted pen
  // crosses above J and below the baseline before climbing into M.
  const orbitO=(()=>{
    const dest=point(penPaths[0],0),cx=dest[0],cy=dest[1]+60*scale;
    return Array.from({length:420},(_,i)=>{
      const p=i/419,q=soft(phase(p,.15,1)),a=-Math.PI/2+TAU*2*p;
      const x=lerp(C,cx,q)+lerp(171,80*scale,q)*Math.cos(a);
      const y=lerp(C,cy,q)+lerp(59,60*scale,q)*Math.sin(a)+15*Math.cos(a)*(1-q);
      return [x,y];
    });
  })();
  const oEnd=point(penPaths[0],1),jStart=point(penPaths[1],0),jEnd=point(penPaths[1],1);
  const mStart=point(penPaths[2],0),mEnd=point(penPaths[2],1);
  const calligraphy=joined([
    {points:orbitO},{points:penPaths[0].p,letter:0},
    {points:cubic(oEnd,[oEnd[0]+48,oEnd[1]],[jStart[0],jStart[1]-42],jStart)},
    {points:penPaths[1].p,letter:1},
    {points:cubic(jEnd,[jEnd[0],jEnd[1]-42],[mStart[0],mStart[1]+55],mStart)},
    {points:penPaths[2].p,letter:2}
  ]);
  function orbitalCalligraphy(ctx,p){
    const travel=soft(phase(p,0,.94)),appear=soft(phase(p,0,.035)),fade=1-soft(phase(p,.93,1));
    inkAlong(ctx,calligraphy,travel);
    ribbonTail(ctx,calligraphy,travel,.105,appear*fade,5.2,p*4);
    // One fine echo follows the same physical curve, never a separate orbit.
    if(p<.56){
      const past=Math.max(0,travel-.022),pts=Array.from({length:70},(_,i)=>point(calligraphy,lerp(Math.max(0,past-.12),past,i/69)));
      const q=1-soft(phase(p,.43,.56));
      ctx.save();ctx.globalAlpha=.12*q;ctx.strokeStyle=white;ctx.lineWidth=.55;ctx.lineCap='round';
      ctx.beginPath();pts.forEach((v,i)=>i?ctx.lineTo(...v):ctx.moveTo(...v));ctx.stroke();ctx.restore();
    }
    settle(ctx,p,.94);systemSweep(ctx,p,.82,1);
  }

  const SAMPLES=220;
  const targetLines=penPaths.map(path=>Array.from({length:SAMPLES},(_,i)=>point(path,i/(SAMPLES-1))));
  function satinShape(p,index){
    const split=soft(phase(p,.29+index*.015,.77+index*.025)),turn=p*5.4*(1-split);
    return targetLines[index].map((target,n)=>{
      const t=n/(SAMPLES-1),a=TAU*t+turn;
      const theta=-.20+.15*Math.sin(p*4),x=153*Math.sin(a),y=53*Math.sin(2*a);
      const source=[C+x*Math.cos(theta)-y*Math.sin(theta),C+x*Math.sin(theta)+y*Math.cos(theta)+13*Math.cos(a)];
      return source.map((v,j)=>lerp(v,target[j],split));
    });
  }
  function satinOrbit(ctx,p){
    const appear=soft(phase(p,0,.045)),fade=1-soft(phase(p,.79,.94));
    const grow=soft(phase(p,0,.25));
    for(let index=2;index>=0;index--){
      const points=satinShape(p,index),depth=points.map((_,n)=>.5+.5*Math.cos(TAU*n/(SAMPLES-1)+p*5.4));
      ribbon(ctx,points,depth,{alpha:appear*fade*(index===0?1:.6),width:8.2,grow});
    }
    if(p<.71){
      const points=satinShape(p,0),q=clamp(grow*(SAMPLES-1)),i=Math.min(SAMPLES-2,Math.floor(q)),f=q-i;
      star(ctx,[lerp(points[i][0],points[i+1][0],f),lerp(points[i][1],points[i+1][1],f)],appear*(1-soft(phase(p,.58,.71))),2.8);
    }
    const ink=soft(phase(p,.69,.94));
    maskedInk(ctx,penPaths.map(path=>({path,p:1,weight:26*ink,alpha:ink})));
    settle(ctx,p,.94);systemSweep(ctx,p,.82,1);
  }

  const infinityLoop=Array.from({length:460},(_,i)=>{
    const f=i/459,a=TAU*1.5*f,q=soft(phase(f,.54,1));
    const r=lerp(156,88,q),x=r*Math.sin(a),y=55*Math.sin(2*a);
    return [C+x,C-9+y+8*Math.sin(a)*(1-q)];
  });
  const infinityTrace=worldPath(infinityLoop);
  const infinityEnd=infinityLoop[infinityLoop.length-1];
  const infinitySignature=joined([
    {points:infinityLoop},
    {points:cubic(infinityEnd,[infinityEnd[0]-42,infinityEnd[1]+42],[oEnd[0]-38,oEnd[1]],oEnd)},
    {points:penPaths[0].p,letter:0},
    {points:cubic(oEnd,[oEnd[0]+48,oEnd[1]],[jStart[0],jStart[1]-42],jStart)},
    {points:penPaths[1].p,letter:1},
    {points:cubic(jEnd,[jEnd[0],jEnd[1]-42],[mStart[0],mStart[1]+55],mStart)},
    // A wider masked brush covers M's broad upper shoulders during the
    // moving stroke itself. It never changes the supplied logo silhouette.
    {points:penPaths[2].p,letter:2,weight:40}
  ]);
  function exactSignatureInk(ctx,travel){
    const clip=new Path2D(),distance=travel*infinitySignature.total;
    infinitySignature.inks.forEach(ink=>{
      const p=clamp((distance-ink.begin)/(ink.end-ink.begin));if(p<=0)return;
      const path=penPaths[ink.letter],head=pathPoint(path,p),r=ink.weight/2;
      const points=path.p.slice(0,head.n);points.push([head.x,head.y]);
      for(let i=0;i<points.length;i++){
        const a=points[i];clip.moveTo(a[0]+r,a[1]);clip.arc(a[0],a[1],r,0,TAU);clip.closePath();
        if(i===0)continue;
        const b=points[i-1],dx=a[0]-b[0],dy=a[1]-b[1],length=Math.hypot(dx,dy);
        if(length<.00001)continue;
        const nx=-dy/length*r,ny=dx/length*r;
        clip.moveTo(b[0]-nx,b[1]-ny);clip.lineTo(a[0]-nx,a[1]-ny);
        clip.lineTo(a[0]+nx,a[1]+ny);clip.lineTo(b[0]+nx,b[1]+ny);clip.closePath();
      }
    });
    ctx.save();ctx.clip(clip);image(ctx);ctx.restore();
  }
  function infinityInk(ctx,p){
    const travel=soft(phase(p,0,.945)),alpha=soft(phase(p,0,.035))*(1-soft(phase(p,.93,1)));
    const trace=clamp(travel*infinitySignature.total/infinityTrace.total);
    strokePath(ctx,infinityTrace,trace,.85,white,soft(phase(p,0,.04))*(1-soft(phase(p,.51,.69)))*.43);
    exactSignatureInk(ctx,travel);
    ribbonTail(ctx,infinitySignature,travel,.135,alpha,7.0,-p*5);
    // No final logo overlay: the moving brush has already drawn every pixel.
    systemSweep(ctx,p,.825,1);
  }

  function wovenShape(p,index){
    const capture=soft(phase(p,.28+index*.045,.76+index*.035));
    const target=targetLines[index===0?0:2],turn=p*5.8*(1-capture),phaseOffset=index*Math.PI;
    return target.map((dest,n)=>{
      const t=n/(SAMPLES-1),a=TAU*t+turn+phaseOffset;
      // Two distinct surfaces weave around one shared loop.
      const x=149*Math.sin(a),y=56*Math.cos(a)+16*Math.sin(2*a+index*Math.PI);
      const source=[C+x,C+y+18*Math.sin(a)*Math.sin(p*3)];
      return source.map((v,j)=>lerp(v,dest[j],capture));
    });
  }
  function wovenMonogram(ctx,p){
    const alpha=soft(phase(p,0,.04))*(1-soft(phase(p,.80,.94)));
    const grow=soft(phase(p,0,.24));
    for(let index=0;index<2;index++){
      const points=wovenShape(p,index),depth=points.map((_,n)=>.5+.5*Math.cos(TAU*n/(SAMPLES-1)+p*5.8+index*Math.PI));
      ribbon(ctx,points,depth,{alpha,width:5.7,grow});
      if(p<.65){
        const at=Math.min(SAMPLES-1,Math.floor(grow*(SAMPLES-1)));
        star(ctx,points[at],alpha*(1-soft(phase(p,.50,.65))),2.65);
      }
    }
    const fill=soft(phase(p,.67,.93)),j=soft(phase(p,.65,.87));
    maskedInk(ctx,[{path:penPaths[0],p:1,weight:26*fill,alpha:fill},
      {path:penPaths[2],p:1,weight:26*fill,alpha:fill},{path:penPaths[1],p:j,weight:26}]);
    if(p>.65&&p<.91)star(ctx,point(penPaths[1],j),1-soft(phase(p,.87,.91)),2.7);
    settle(ctx,p,.94);systemSweep(ctx,p,.82,1);
  }

  // Three lights first share one orbit. Each leaves it with zero capture
  // acceleration, takes its own tangent, and writes one original letter.
  const balletTracks=[0,1,2].map(index=>joined([
    {points:orbitApproach(index,1.5+index*.12,-Math.PI/2+index*.05,-.18)},
    {points:penPaths[index].p,letter:index}
  ]));
  function orbitalBallet(ctx,p){
    for(let index=2;index>=0;index--){
      const travel=soft(phase(p,index*.055,.87+index*.045));
      const alpha=soft(phase(p,index*.055,index*.055+.035))*(1-soft(phase(p,.92+index*.012,1)));
      inkAlong(ctx,balletTracks[index],travel);
      ribbonTail(ctx,balletTracks[index],travel,.14,alpha,index===0?6.2:4.3,p*5+index*.8);
    }
    settle(ctx,p,.94);systemSweep(ctx,p,.825,1);
  }
