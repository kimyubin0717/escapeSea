'use strict';
let view='game',runActive=false,menuPanel='',saveProblem='',lastSavedAt=0;
const SAVE_KEY='abyss-html-run-v1';
function readSavedRun(){
  try{
    const data=JSON.parse(localStorage.getItem(SAVE_KEY)||'null');
    if(!data||data.version!==1||!data.state||!Array.isArray(data.cargo))return null;
    const s=data.state;
    if(s.ended||!['oxygen','battery','health','tanks','elapsed'].every(k=>Number.isFinite(s[k])&&s[k]>=0))return null;
    if(s.oxygen>100||s.health>100||s.battery>120||s.tanks>99||s.elapsed>1e8)return null;
    if(!Array.isArray(s.parts)||s.parts.length!==3||!s.parts.every(v=>Number.isInteger(v)&&v>=0&&v<=99))return null;
    if(!Array.isArray(s.repaired)||s.repaired.length!==3||!s.repaired.every(v=>typeof v==='boolean'))return null;
    const checkBag=new GridInventory();
    for(const item of data.cargo){
      const def=checkBag.definition(item.kind);
      if(!def||typeof item.id!=='string'||checkBag.items.some(i=>i.id===item.id)||!checkBag.fits({...def,id:item.id},item.x,item.y))return null;
      checkBag.items.push({...def,id:item.id,kind:item.kind,x:item.x,y:item.y});
    }
    if(checkBag.count('flashlight')!==1||checkBag.count('tank')!==s.tanks||['power','hull','drive'].some((k,i)=>checkBag.count(k)!==s.parts[i]))return null;
    return data;
  }catch{return null;}
}
function saveRun(){
  if(!runActive||state.ended)return false;
  try{
    localStorage.setItem(SAVE_KEY,JSON.stringify({version:1,state:{...state,inventory:false,salvage:false,repair:0},cargo:bag.items,savedAt:Date.now()}));
    saveProblem='';return true;
  }catch{saveProblem='이 브라우저에서는 저장할 수 없습니다. 저장소 접근 설정을 확인해주세요.';return false;}
}
function oceanHTML(){
  return `<div class="title-ocean" aria-hidden="true"><div class="ocean-rays"></div><div class="ocean-haze"></div><svg class="leviathan leviathan-near" viewBox="0 0 1300 400"><path d="M60 193C170 174 220 81 465 97C590 107 673 164 767 180L956 147L1119 39L1080 169L1250 187L1090 216L1146 347L958 240L763 219C691 236 608 287 466 293C252 311 165 233 60 218L10 207Z"/><path d="M391 113L485 9L560 116M406 278L504 395L582 268M666 161L774 72L753 185"/></svg><svg class="leviathan leviathan-far" viewBox="0 0 1300 400"><path d="M60 193C170 174 220 81 465 97C590 107 673 164 767 180L956 147L1119 39L1080 169L1250 187L1090 216L1146 347L958 240L763 219C691 236 608 287 466 293C252 311 165 233 60 218L10 207Z"/></svg><div class="ocean-dust"></div><div class="ocean-vignette"></div></div>`;
}
function menuHTML(){
  const saved=readSavedRun();
  return `${oceanHTML()}<section class="main-menu" aria-label="시작 메뉴" ${menuPanel?"inert":""} ><div class="menu-coordinate">수심 84m · 지상 신호 없음</div><h1>ABYSS</h1><div class="menu-subtitle">마 지 막　빛</div><div class="menu-rule"></div><nav aria-label="게임 메뉴"><button data-menu="new"><span>새로하기</span><small>새 잠수 시작</small></button><button data-menu="continue" ${saved?'':'disabled'}><span>이어하기</span><small>${saved?time(saved.state.elapsed)+' / 잠수 계속하기':'저장 기록 없음'}</small></button><button data-menu="help"><span>조작안내</span><small>생존 지침</small></button><button data-menu="quit"><span>게임 종료</span><small>잠수 종료</small></button></nav><p class="menu-save-note">${esc(saveProblem||(saved?'이 브라우저에 저장된 잠수 기록이 있습니다.':'새 잠수를 시작하면 진행 상태가 자동 저장됩니다.'))}</p></section><div class="menu-bottom"><span>노틸러스 / 07</span><span>어둠 속에서 무언가 움직이고 있습니다.</span></div>${menuPanelHTML()}`;
}
function menuPanelHTML(){
  if(menuPanel==='help')return `<div class="menu-modal-shade"></div><section class="frame menu-modal" role="dialog" aria-modal="true" aria-label="조작안내"><div class="title">조작안내 · 생존 지침</div><p class="manual-objective">부품을 종류별로 2개씩 회수하고 잠수함의 전력·선체·추진 장치를 수리해 탈출하세요.</p><dl>${[['W A S D','이동'],['마우스','바라보는 방향 / 손전등 조준'],['SHIFT','빠르게 이동 · 산소 소모 3배'],['E','부품·산소통 회수 / 수리 / 은신 / 탈출'],['SPACE','회수 타이밍 판정'],['TAB','7×7 인벤토리 열기 / 닫기'],['Q','예비 산소통 사용 · 산소 +55'],['F','후레쉬 켜기 / 끄기'],['드래그 / 클릭','인벤토리 아이템 배치'],['ESC','시작 메뉴 / 안내 닫기'],['R','잠수 다시 시작']].map(([key,desc])=>`<div><dt>${key}</dt><dd>${desc}</dd></div>`).join('')}</dl><p class="manual-note">HTML 미리보기에서는 이동·조준 대신 상단의 화면 상태를 선택합니다.<br>산소통 2×2칸 / 후레쉬 3×1칸 / 수리 부품 1×1칸</p><button id="menuBack" data-menu="back">돌아가기</button></section>`;
  if(menuPanel==='quit')return `<div class="menu-modal-shade"></div><section class="frame menu-modal quit-modal" role="dialog" aria-modal="true" aria-label="게임 종료"><div class="title">SIGNAL 잠수 종료ED</div><h2>잠수를 종료했습니다.</h2><p>${esc(saveProblem||(readSavedRun()?'저장된 잠수 기록은 이어하기로 복원할 수 있습니다.':'시작화면으로 돌아가 새 잠수를 시작할 수 있습니다.'))}</p><p class="quit-note">브라우저에서 창 닫기를 허용하지 않으면<br>이 탭을 직접 닫아주세요.</p><button id="menuBack" data-menu="back">시작화면으로</button></section>`;
  return '';
}
function showMenu(){
  if(view==='game')saveRun();
  eventToasts=[];
  view='menu';runActive=false;menuPanel='';$('scenario').value='menu';$('description').textContent='시작화면 · 새로하기 / 이어하기 / 게임 종료 / 조작안내';render();
}
function startNewRun(){
  choose('intro');runActive=true;saveRun();
}
function continueRun(){
  eventToasts=[];
  const saved=readSavedRun();if(!saved){showMenu();return;}
  state=base();
  for(const key of Object.keys(state))if(typeof saved.state[key]===typeof state[key])state[key]=saved.state[key];
  state.inventory=false;state.salvage=false;state.repair=0;
  bag=new GridInventory();bag.items=saved.cargo.map(i=>({...bag.definition(i.kind),kind:i.kind,id:i.id,x:i.x,y:i.y}));bag.serial=Date.now();bagMessage='';
  view='game';runActive=true;clockOffset=state.elapsed;clockStart=performance.now();$('scenario').value='normal';$('description').textContent='저장된 잠수와 인벤토리 배치를 복원했습니다.';render();
}
function menuAction(action){
  if(action==='home'){showMenu();return;}
  if(action==='new'){startNewRun();return;}
  if(action==='continue'){continueRun();return;}
  if(action==='help'){menuPanel='help';render();$('menuBack')?.focus();return;}
  if(action==='back'){menuPanel='';render();return;}
  if(action==='quit'){
    menuPanel='quit';render();
    // Browsers only allow closing windows they opened themselves.
    if(window.opener)window.close();
  }
}

