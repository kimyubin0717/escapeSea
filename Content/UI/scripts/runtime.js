 'use strict';
(() => {
  if (new URLSearchParams(location.search).get('runtime') !== '1') return;
  const send=(action,payload='')=>console.log('ABYSS_ACTION:'+JSON.stringify({action,payload}));
  const localMenuAction=menuAction;
  let snapshot=null,noticeSerial=-1;
  document.body.classList.add('runtime');
  $('background').checked=false;
  syncBag=()=>{};
  saveRun=()=>false;
  readSavedRun=()=>snapshot?.hasSave ? {state:snapshot} : null;
  showMenu=()=>send('menu');
  startNewRun=()=>send('new');
  continueRun=()=>send('continue');
  menuAction=action=>{
    if(['help','back'].includes(action)){localMenuAction(action);return;}
    send(action==='home'?'menu':action);
  };
  action=key=>send(key===' '?'space':key);
  moveCargo=(id,x,y)=>send('cargo',JSON.stringify({id,x,y}));
  // Runtime state is owned by C++; the HTML preview must never drain oxygen,
  // award parts, finish salvage, or save a separate browser game.
  animate=now=>{
    eventToasts=eventToasts.filter(t=>t.expires>now);
    requestAnimationFrame(animate);
  };
  window.abyssRuntime=Object.freeze({receive(next,cargo){
    if(!next||!Array.isArray(cargo))return;
    snapshot=next;
    state={...base(),...next};phase=next.phase||0;
    // Display whole units without changing authoritative C++ values.
    for(const key of ["oxygen","battery","health"])if(Number.isFinite(state[key]))state[key]=Math.ceil(state[key]);
    view=next.menu?'menu':'game';runActive=false;
    const selected=bag.selected;
    bag.items=cargo.map(i=>({...bag.definition(i.kind),...i}));
    bag.selected=cargo.some(i=>i.id===selected)?selected:null;
    if(next.notice && next.noticeSerial!==noticeSerial){
      noticeSerial=next.noticeSerial;
      if(next.noticeKind)notifyToast(next.notice,next.noticeKind);
    }
    // Keep the captured pointer's element alive until drop or cancellation.
    if(!screen.querySelector('.dragging')||next.menu||!next.inventory)render();
    const marker=$('marker');if(marker)marker.style.left=Math.max(0,Math.min(1,phase))*100+'%';
    const zone=screen.querySelector('.zone');if(zone){zone.style.left=next.salvageTarget*100+'%';zone.style.width=next.salvageWindow*100+'%';}
  }});
  console.log('ABYSS_UI_READY');
})();

