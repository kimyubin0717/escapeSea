'use strict';
const $ = id => document.getElementById(id);
const screen = $('screen');
const base = () => ({oxygen:100,battery:120,health:100,tanks:2,parts:[0,0,0],repaired:[false,false,false],elapsed:0,inventory:false,hidden:false,light:true,inside:false,sprint:false,alert:false,chasing:false,notice:'',prompt:'',repair:0,salvage:false,ended:false,won:false,reason:''});
const scenarios = {
  menu:['시작화면',{},'시작화면 · 새로하기 / 이어하기 / 게임 종료 / 조작안내'],
  normal:['기본 HUD',{},'왼쪽 상단에는 현재 목표 하나만 표시합니다. 전력 → 선체 → 추진 순서로 수리를 완료하면 다음 목표로 넘어갑니다. 산소·후레쉬·체력은 TAB 인벤토리에서 확인합니다.'],
  intro:['시작 안내',{notice:'THE WORLD IS SUBMERGED. Salvage 2 of each part. Repair NAUTILUS. Escape.'},'플레이 시작 알림.'],
  equipment:['장비창',{inventory:true,oxygen:76,battery:84,parts:[2,1,0]},'7×7 격자 · 산소통 2×2 · 후레쉬 3×1 · 부품 1×1. 드래그 또는 아이템 선택 후 빈칸 클릭으로 이동합니다.'],
  critical:['자원 부족 경고',{oxygen:24,battery:25,health:20,tanks:0,inventory:true},'산소·배터리 30% 이하 점멸과 예비 산소통 없음 경고.'],
  suffocating:['산소 고갈 · 체력 감소',{oxygen:0,tanks:1,inventory:true},'주황색 화면 테두리 점멸 · 체력 초당 5 감소. Q로 산소를 보충하면 멈춥니다.'],
  missing:['수리 재료 부족',{inventory:true,parts:[1,0,0],notice:'Not enough parts. Collect 2 POWER CELL.',missingPart:0},'부족한 재료 강조와 알림.'],
  ready:['수리 재료 준비',{inventory:true,parts:[2,2,2]},'모든 재료를 모아 수리할 수 있는 상태입니다.'],
  pickup:['부품 회수 상호작용',{prompt:'[E] SALVAGE POWER CELL / timing check'},'가까운 부품의 상호작용 안내.'],
  salvage:['회수 타이밍 이벤트',{salvage:true},'2.4초 안에 녹색 구간에서 SPACE. 실패하면 추적 경고가 표시됩니다.'],
  salvageSuccess:['회수 성공',{parts:[1,0,0],notice:'POWER CELL secured.'},'성공 시 부품 추가와 안내.'],
  crafted:['제작 완료 알림',{parts:[0,1,0],notice:'HULL ALLOY crafted.'},'제작 완료 메시지 미리보기 · 수집목표 아래에서 5초 후 사라집니다.'],
  salvageFail:['회수 실패',{alert:true,chasing:true,notice:'SALVAGE FAILED / noise alerted the stalkers.'},'실패·시간 초과·이탈 시 소음 알림.'],
  oxygenPickup:['산소통 상호작용',{prompt:'[E] COLLECT OXYGEN TANK'},'산소통 수집 안내.'],
  kelp:['해초 은신 상호작용',{prompt:'[E] HIDE IN KELP / light off'},'은신 가능한 해초 근처 안내.'],
  hidden:['해초 은신',{hidden:true,prompt:'[E] LEAVE KELP'},'은신 상태와 해제 안내.'],
  lightOff:['손전등 꺼짐',{light:false,battery:89},'손전등을 끈 상태입니다.'],
  batteryEmpty:['배터리 소진',{battery:0,light:false},'배터리 소진 및 손전등 비활성화.'],
  shelter:['건물 내부',{inside:true},'침수 대피소 내부 표시.'],
  sprint:['빠른 이동',{sprint:true,oxygen:68},'산소 소비 3배 안내.'],
  proximity:['접근 경고',{alert:true},'주변 접근 상태 예시 · 경고 및 지도는 표시하지 않습니다.'],
  chase:['추적 경고',{alert:true,chasing:true,health:65},'시야에서 벗어나라는 추적 경고.'],
  repair:['수리 진행',{parts:[2,2,2],repair:2,prompt:'REPAIRING / stay near the terminal'},'4초 수리 진행. E로 현재 단계를 완료하고 다음 단계를 확인할 수 있습니다.'],
  stage2:['전력 수리 완료',{repaired:[true,false,false],parts:[0,2,2],prompt:'[E] UNLOCK REPAIR STAGE 2 / HULL ALLOY (2/2)'},'전력 온라인, 선체 단계 활성화.'],
  stage3:['선체 수리 완료',{repaired:[true,true,false],parts:[0,0,2],prompt:'[E] UNLOCK REPAIR STAGE 3 / DRIVE PART (2/2)'},'전력·선체 온라인, 추진 단계 활성화.'],
  locked:['탈출구 잠김',{prompt:'HATCH LOCKED / repair all 3 systems'},'세 시스템 수리 전 탈출구 안내.'],
  escape:['탈출 준비',{repaired:[true,true,true],prompt:'[E] BOARD SUBMARINE / ESCAPE',notice:'All systems ONLINE. Board the escape hatch!'},'모든 시스템 온라인. E로 탈출 성공 화면.'],
  win:['탈출 성공',{ended:true,won:true,elapsed:284,reason:'NAUTILUS RESTORED. You escaped the drowned city.'},'성공 종료 화면과 잠수 시간, R 재시작.'],
  oxygenDeath:['산소 고갈 실패',{ended:true,oxygen:0,health:0,elapsed:153,reason:'OXYGEN DEPLETED. The deep takes another diver.'},'산소 고갈 종료 화면.'],
  death:['체력 소진 실패',{ended:true,health:0,elapsed:196,reason:'DIVER LOST / health depleted.'},'사망 종료 화면.']
};
let state = base(), salvageStart = 0, phase = 0, clockStart = 0, clockOffset = 0;
let bag = new GridInventory(), bagMessage = '';
let eventToasts=[],toastSerial=0;
let oxygenDamageStart=null;
const OXYGEN_DAMAGE_PER_SECOND=5;
function notifyToast(message,kind='success'){
  eventToasts.push({id:++toastSerial,message,kind,expires:performance.now()+5000});
  eventToasts=eventToasts.slice(-3);
}
function toastHTML(){
  return `<div class="event-toasts" aria-live="polite" aria-relevant="additions">${eventToasts.map(t=>`<div id="event-toast-${t.id}" class="event-toast ${t.kind}"><span class="event-toast-icon" aria-hidden="true">✓</span><div><small>${t.kind==='craft'?'제작 완료':t.kind==='repair'?'수리 완료':'회수 성공'}</small><p>${esc(t.message)}</p></div></div>`).join('')}</div>`;
}
const itemKinds=['power','hull','drive'];
function syncBag(){
  const complete=bag.sync({flashlight:1,tank:state.tanks,power:state.parts[0],hull:state.parts[1],drive:state.parts[2]});
  if(!complete)bagMessage='공간 부족 · 아이템이 들어갈 연속된 빈칸이 필요합니다.';
  state.tanks=bag.count('tank');state.parts=itemKinds.map(kind=>bag.count(kind));
}
function itemIcon(kind){
  const shapes={tank:'<rect x="29" y="18" width="42" height="68" rx="17"/><path d="M42 18V9h16v9M29 45h42M29 66h42M46 9V4h17"/>',flashlight:'<path d="M12 37h47l16-12h13v50H75L59 63H12zM59 37v26M75 25v50M22 37v26M32 37v26M88 32l9-7M88 50h12M88 68l9 7"/>',power:'<path d="M31 20h38v65H31zM42 20V12h16v8M54 30L40 55h14l-7 20 18-28H51z"/>',hull:'<path d="M19 28h62v48H19zM19 28l15-12h62v48L81 76M34 16v47M19 76l15-13h62"/>',drive:'<circle cx="50" cy="50" r="15"/><path d="M48 35C20 4 10 33 35 49M64 45c40-8 26-37 1-15M47 65c-12 39 22 41 20 8M35 58C0 79 25 99 41 74"/>'};
  return `<svg viewBox="0 0 100 100" aria-hidden="true">${shapes[kind]}</svg>`;
}
function inventoryVitals(){
  const oxygen=state.oxygen,battery=Math.ceil(state.battery/120*100);
  return `<div class="cargo-vitals inventory-telemetry"><div class="oxygen-inventory"><div class="oxygen-dial ${oxygen<=30?'danger':''}" style="--gauge-color:${color(oxygen/100)}" role="meter" aria-label="산소 잔량" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${oxygen}"><svg viewBox="0 0 88 88" aria-hidden="true"><circle class="dial-track" cx="44" cy="44" r="35"/><circle class="dial-fill" cx="44" cy="44" r="35" pathLength="100" stroke-dasharray="100" stroke-dashoffset="${100-oxygen}"/></svg><strong>${oxygen}<small>%</small></strong></div><div class="oxygen-description"><span>산소</span><b>산소 잔량</b><small>예비 산소통 ×${state.tanks} · [Q]</small></div></div><div class="flashlight-inventory" style="--gauge-color:${color(state.battery/120)}"><div class="flashlight-label"><span>손전등 배터리</span><b>${battery}% <small> / ${state.battery}초</small></b></div><div role="meter" aria-label="후레쉬 배터리" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${battery}">${meter(state.battery/120,color(state.battery/120))}</div><div class="inventory-light-state">${state.battery<=0?'배터리 소진':state.light?'손전등 켜짐':'손전등 꺼짐'} · [F]</div></div><div class="inventory-health">체력 <span class="${state.health<=30?'danger':''}">${state.health} / 100</span></div></div>`;
}
function inventoryHTML(){
  const selected=bag.items.find(i=>i.id===bag.selected);
  return `<div class="title">인벤토리 · 장비 보관함</div><div class="bag-subtitle">잠수부 07 · 홀로그램 보관함 · 7 × 7</div><div class="bag-capacity">${bag.used} / 49칸</div>
    <div class="cargo-grid" role="group" aria-label="7행 7열 인벤토리">${Array.from({length:49},(_,i)=>`<button class="cargo-cell" style="grid-column:${i%7+1};grid-row:${Math.floor(i/7)+1}" data-x="${i%7}" data-y="${Math.floor(i/7)}" aria-label="${Math.floor(i/7)+1}행 ${i%7+1}열로 선택 아이템 이동"></button>`).join('')}${bag.items.map(i=>`<button draggable="true" class="cargo-item ${i.kind} ${bag.selected===i.id?'selected':''}" data-item="${i.id}" style="grid-column:${i.x+1}/span ${i.w};grid-row:${i.y+1}/span ${i.h}" aria-label="${ko(i.name)}, ${i.w} × ${i.h}, ${i.y+1}행 ${i.x+1}열" aria-pressed="${bag.selected===i.id}">${itemIcon(i.kind)}<span>${ko(i.name)}</span><small>${i.w} × ${i.h}</small></button>`).join('')}</div>
    <div class="cargo-details"><div class="cargo-eyebrow">선택한 아이템</div><div class="cargo-inspect">${selected?itemIcon(selected.kind):itemIcon('tank')}</div><h2>${selected?ko(selected.name):'보관함 · 총 49칸'}</h2><p>${selected?`${selected.w} × ${selected.h} · ${selected.w*selected.h}칸 차지`:'아이템 선택 후 빈칸을 클릭하거나<br>드래그해서 배치하세요.'}</p>${inventoryVitals()}<div class="cargo-manifest">${items.map((name,i)=>`<div class="${state.repaired[i]?'teal':state.parts[i]<2?'cargo-missing':''}">${name}<span>${state.parts[i]} / 2 · ${state.repaired[i]?'수리 완료':state.parts[i]<2?'재료 부족':'수리 가능'}</span></div>`).join('')}</div><p class="cargo-footprints">산소통 2 × 2 · 후레쉬 3 × 1<br>수리 부품 1 × 1 · 개별 보관</p></div>
    <div class="cargo-feedback" role="status">${esc(bagMessage||state.notice||'각 아이템은 독립된 공간을 차지합니다. 겹침과 경계 밖 배치는 불가능합니다.')}</div><footer>[TAB] 닫기　 [Q] 산소통 사용　 [F] 손전등 <span>${state.tanks===0?'! 예비 산소통이 없습니다':'드래그로 이동 / 선택 후 빈칸 클릭'}</span></footer>`;
}
const items=['전력 전지','선체 합금','추진 부품'];
const systems=['전력 장치','선체','추진 장치'];
const locations=['북서쪽 · 침수 연구소','남서쪽 · 난파선 묘지','북동쪽 · 펌프장'];
const esc = text => ko(text).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const time = seconds => `${String(Math.floor(seconds/60)).padStart(2,'0')}:${String(Math.floor(seconds%60)).padStart(2,'0')}`;
const color = ratio => ratio<=.3?'var(--red)':'var(--teal)';
const meter = (ratio,c='var(--teal)') => `<div class="meter" style="--color:${c}"><div class="fill" style="width:${Math.max(0,Math.min(100,ratio*100))}%"></div></div>`;
const frame = (cls,html) => `<section class="frame ${cls}">${html}</section>`;
function render(){
  if(view!=='game'||state.ended||state.oxygen>0)oxygenDamageStart=null;
  for(const key of ['oxygen','battery','health','tanks','part0','part1','part2','equipment','restart'])$(key).disabled=view==='menu';
  if(view==='menu'){screen.className='title-screen';screen.innerHTML=menuHTML();return;}
  syncBag();
  const s=state, next=s.repaired.findIndex(v=>!v);
  screen.className=$('background').checked?'scenery':'';
  let html=`<div class="world" ${$('background').checked?'':'hidden'}></div>`;
  html+=frame('restore',`<div class="title">현재 목표 · 잠수함 수리</div><div class="goal-summary">${next<0?'모든 장치 수리 완료':`${next+1}단계 · ${systems[next]} 복구`}</div><div class="system active"><div class="goal-row"><strong>${next<0?'잠수함 탑승':items[next]}</strong><span>${next<0?'탈출 준비':`${s.parts[next]} / 2개`}</span></div><small>${next<0?'탈출구로 이동하세요':`${locations[next]} · ${systems[next]}`}</small></div><p>${next<0?'탈출구에서 [E]를 눌러 탈출하세요.':s.parts[next]<2?`${items[next]} 2개를 모으세요.`:`단말기에서 [E]를 눌러 ${systems[next]} 수리`}</p>`);
  html+=`<div class="timer"><strong id="diveTime" aria-label="잠수 경과 시간">${time(s.elapsed)}</strong></div>`;
  if(s.prompt&&!s.ended)html+=frame('prompt',esc(s.prompt));
  if(s.repair>0)html+=`<div class="repair">잠수함 장치 수리 중${meter(s.repair/4,'var(--neon)')}</div>`;
  if(s.inventory&&!s.ended){
    html+='<div class="shade"></div>'+frame('equipment',inventoryHTML());
  }
  if(s.salvage&&!s.ended)html+=frame('salvage','<div class="title">부품 회수 · 타이밍 맞추기</div><div class="muted">흰 선이 초록색 구간에 들어오면 [SPACE]를 누르세요</div><div class="track"><div class="zone"></div><div id="marker" class="marker"></div></div><div class="warning">판정 실패 · 시간 초과 · 이탈 시 소음으로 추적자가 접근합니다</div>');
  if(s.ended)html+='<div class="shade ending-shade"></div>'+frame('ending',`<div class="title ${s.won?'teal':'danger'}">${s.won?'탈출 성공':'잠수 실패'}</div><p>${esc(s.reason)}</p><footer>잠수 시간 ${time(s.elapsed)} · [R] 다시 시작</footer>`);
  if(!s.ended)html+=toastHTML();
  if(s.oxygen<=0&&!s.ended)html+='<div class="oxygen-depleted-edge" aria-hidden="true"></div>';
  screen.innerHTML=html;
  for(const key of ['oxygen','battery','health','tanks'])$(key).value=s[key];
  for(let i=0;i<3;i++)$('part'+i).value=s.parts[i];
  $('oxygenValue').value=s.oxygen+'%';$('batteryValue').value=s.battery+'초 / '+Math.ceil(s.battery/120*100)+'%';$('healthValue').value=s.health+' / 100';
  saveRun();
}
function choose(key){
  oxygenDamageStart=null;
  if(key==='menu'){showMenu();return;}
  if(runActive)saveRun();
  view='game';runActive=false;menuPanel='';
  eventToasts=[];
  if(key==='salvageSuccess')notifyToast('POWER CELL +1');
  if(key==='crafted')notifyToast('HULL ALLOY 제작 완료','craft');
  if(key==='escape')notifyToast('모든 잠수함 시스템 복구 완료','repair');
  const preset=scenarios[key];state={...base(),...structuredClone(preset[1])};bag=new GridInventory();bagMessage='';phase=0;salvageStart=performance.now();clockStart=salvageStart;clockOffset=state.elapsed;$('scenario').value=key;$('description').textContent=preset[2];render();
}
function salvageResult(){
  state.salvage=false;
  if(phase>=.45&&phase<=.62){
    if(bag.add('power')){state.parts[0]++;state.notice='POWER CELL secured.';notifyToast('POWER CELL +1');}
    else{state.notice='INVENTORY FULL / clear space before collecting.';bagMessage=state.notice;}
  }
  else{state.alert=true;state.chasing=true;state.notice='SALVAGE FAILED / noise alerted the stalkers.';}
  render();
}
function interact(){
  if(state.ended||state.salvage)return;
  if(state.hidden){state.hidden=false;state.prompt='[E] HIDE IN KELP';}
  else if(state.prompt.includes('HIDE IN KELP')){state.hidden=true;state.prompt='[E] LEAVE KELP';}
  else if(state.prompt.includes('OXYGEN TANK')){
    if(bag.add('tank')){state.tanks++;state.notice='OXYGEN TANK stored. [Q] to use.';state.prompt='';notifyToast('OXYGEN TANK +1');}
    else{state.notice='INVENTORY FULL / 2 x 2 cells required.';bagMessage=state.notice;}
  }
  else if(state.prompt.includes('SALVAGE')){state.salvage=true;phase=0;salvageStart=performance.now();}
  else if(state.repaired.every(Boolean)){state.ended=true;state.won=true;state.reason='NAUTILUS RESTORED. You escaped the drowned city.';}
  else if(state.inventory||state.repair>0||state.prompt.includes('STAGE')){
    const k=state.repaired.findIndex(v=>!v);
    if(state.parts[k]<2){state.inventory=true;state.missingPart=k;state.notice=`Collect 2 ${items[k]}.`;}
    else{state.parts[k]-=2;state.repaired[k]=true;state.repair=0;state.notice=`${items[k]} installed. Subsystem restored.`;notifyToast(`${items[k]} 설치 완료`,'repair');state.prompt=state.repaired.every(Boolean)?'[E] BOARD SUBMARINE / ESCAPE':`[E] UNLOCK REPAIR STAGE ${k+2} / ${items[k+1]}`;}
  }
  render();
}
function action(key){
  if(view==='menu')return;
  if(key==='r'){if(runActive)startNewRun();else choose('normal');return;}
  if(state.ended)return;
  if(key==='tab')state.inventory=!state.inventory;
  if(key==='q'){
    if(state.tanks>0&&state.oxygen>=99){state.notice='Oxygen is full. Save your tank.';}
    else if(state.tanks>0){state.tanks--;state.oxygen=Math.min(100,state.oxygen+55);state.notice='Oxygen tank used. +55 oxygen.';}
    else state.notice='No oxygen tanks. Find green canisters in the city.';
  }
  if(key==='f'){if(state.battery>0)state.light=!state.light;else state.notice='Battery depleted. Flashlight disabled.';}
  if(key===' '&&state.salvage){salvageResult();return;}
  if(key==='e'){interact();return;}
  render();
}
for(const [key,preset] of Object.entries(scenarios)){$('scenario').add(new Option(preset[0],key));}
$('scenario').addEventListener('change',e=>choose(e.target.value));
$('background').addEventListener('change',render);
$('equipment').addEventListener('click',()=>action('tab'));
$('restart').addEventListener('click',()=>choose('normal'));
$('home').addEventListener('click',showMenu);
function moveCargo(id,x,y){
  bagMessage=bag.move(id,x,y)?'아이템 위치를 변경했습니다.':'배치 불가 · 다른 아이템과 겹치거나 격자를 벗어납니다.';
  render();
}
screen.addEventListener('click',e=>{
  const menu=e.target.closest('[data-menu]');
  if(menu){menuAction(menu.dataset.menu);return;}
  const item=e.target.closest('[data-item]'),cell=e.target.closest('.cargo-cell');
  if(item){bag.selected=bag.selected===item.dataset.item?null:item.dataset.item;bagMessage='';render();}
  else if(cell&&bag.selected)moveCargo(bag.selected,Number(cell.dataset.x),Number(cell.dataset.y));
});
let dragAnchor={x:0,y:0};
screen.addEventListener('dragstart',e=>{
  const target=e.target.closest('[data-item]');if(!target)return;
  bag.selected=target.dataset.item;
  const rect=target.getBoundingClientRect(),scale=screen.getBoundingClientRect().width/1600;
  dragAnchor={x:Math.floor((e.clientX-rect.left)/(74*scale)),y:Math.floor((e.clientY-rect.top)/(74*scale))};
  e.dataTransfer.setData('text/plain',bag.selected);e.dataTransfer.effectAllowed='move';target.classList.add('dragging');
});
screen.addEventListener('dragover',e=>{if(e.target.closest('.cargo-grid')){e.preventDefault();e.dataTransfer.dropEffect='move';}});
screen.addEventListener('drop',e=>{
  const grid=e.target.closest('.cargo-grid');if(!grid)return;e.preventDefault();
  const rect=grid.getBoundingClientRect(),cell=(rect.width+4*rect.width/518)/7;
  const x=Math.floor((e.clientX-rect.left)/cell)-dragAnchor.x,y=Math.floor((e.clientY-rect.top)/cell)-dragAnchor.y;
  moveCargo(e.dataTransfer.getData('text/plain'),x,y);
});
screen.addEventListener('dragend',()=>{const dragging=screen.querySelector('.dragging');if(dragging)dragging.classList.remove('dragging');});
for(const key of ['oxygen','battery','health','tanks'])$(key).addEventListener('input',e=>{const max=key==='battery'?120:key==='tanks'?99:100;state[key]=Math.max(0,Math.min(max,Number(e.target.value)||0));render();});
for(let i=0;i<3;i++)$('part'+i).addEventListener('input',e=>{state.parts[i]=Math.max(0,Math.min(99,Math.trunc(Number(e.target.value)||0)));render();});
document.addEventListener('keydown',e=>{
  if(e.key==='Escape'){e.preventDefault();if(view==='menu'){if(menuPanel){menuPanel='';render();}}else showMenu();return;}
  if(['INPUT','SELECT','TEXTAREA','BUTTON'].includes(e.target.tagName))return;
  const key=e.key.toLowerCase();
  if(['tab','q','f','e',' ','r'].includes(key)){e.preventDefault();if(!e.repeat)action(key);}
});
new ResizeObserver(()=>{screen.style.transform=`scale($('viewport').clientWidth/1600)`;}).observe($('viewport'));
function updateOxygenDepletion(now){
  if(view!=='game'||state.ended||state.oxygen>0){oxygenDamageStart=null;return;}
  if(oxygenDamageStart===null)oxygenDamageStart=now;
  const seconds=Math.floor((now-oxygenDamageStart)/1000);
  if(seconds<=0&&state.health>0)return;
  oxygenDamageStart+=seconds*1000;
  state.health=Math.max(0,state.health-seconds*OXYGEN_DAMAGE_PER_SECOND);
  if(state.health===0){
    state.ended=true;state.won=false;state.salvage=false;state.repair=0;
    state.reason='OXYGEN DEPLETED. The deep takes another diver.';
  }
  render();
}
function animate(now){
  const expired=eventToasts.filter(t=>t.expires<=now);
  if(expired.length){for(const t of expired)$('event-toast-'+t.id)?.remove();eventToasts=eventToasts.filter(t=>t.expires>now);}
  if(view==='game'&&!state.ended){state.elapsed=clockOffset+(now-clockStart)/1000;const timer=$('diveTime');if(timer)timer.textContent=time(state.elapsed);}
  updateOxygenDepletion(now);
  if(view==='game'&&state.salvage){phase=Math.min(1,(now-salvageStart)/2400);const marker=$('marker');if(marker)marker.style.left=phase*100+'%';if(phase>=1)salvageResult();}
  if(runActive&&now-lastSavedAt>=5000){saveRun();lastSavedAt=now;}
  requestAnimationFrame(animate);
}
window.addEventListener('pagehide',saveRun);
choose('menu');requestAnimationFrame(now=>animate(now));


