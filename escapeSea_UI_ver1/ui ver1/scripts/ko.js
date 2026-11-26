'use strict';
// Keep saved event identifiers in English; translate only their visible text.
const KO_TEXT={
  'POWER CELL':'전력 전지','HULL ALLOY':'선체 합금','DRIVE PART':'추진 부품','OXYGEN TANK':'산소통','FLASHLIGHT':'손전등',
  'THE WORLD IS SUBMERGED. Salvage 2 of each part. Repair NAUTILUS. Escape.':'물에 잠긴 도시에서 부품을 종류별로 2개씩 모아 잠수함을 수리하고 탈출하세요.',
  'Not enough parts. Collect 2 POWER CELL.':'전력 전지가 부족합니다. 2개를 모아주세요.',
  'POWER CELL secured.':'전력 전지를 회수했습니다.',
  'HULL ALLOY crafted.':'선체 합금을 제작했습니다.',
  'SALVAGE FAILED / noise alerted the stalkers.':'회수에 실패했습니다. 소음을 들은 추적자가 다가옵니다.',
  '[E] SALVAGE POWER CELL / timing check':'[E] 전력 전지 회수 · 타이밍 판정',
  '[E] COLLECT OXYGEN TANK':'[E] 산소통 줍기',
  '[E] HIDE IN KELP / light off':'[E] 해초에 숨기 · 손전등 끄기',
  '[E] HIDE IN KELP':'[E] 해초에 숨기',
  '[E] LEAVE KELP':'[E] 은신 해제',
  'REPAIRING / stay near the terminal':'수리 중 · 단말기 근처에서 기다리세요',
  'HATCH LOCKED / repair all 3 systems':'탈출구 잠김 · 세 장치를 모두 수리하세요',
  '[E] BOARD SUBMARINE / ESCAPE':'[E] 잠수함에 탑승하여 탈출',
  'All systems ONLINE. Board the escape hatch!':'모든 장치의 수리가 끝났습니다. 탈출구로 이동하세요!',
  'NAUTILUS RESTORED. You escaped the drowned city.':'잠수함을 수리하고 물에 잠긴 도시에서 탈출했습니다.',
  'OXYGEN DEPLETED. The deep takes another diver.':'산소가 고갈되어 잠수를 마치지 못했습니다.',
  'DIVER LOST / health depleted.':'체력이 소진되어 잠수를 마치지 못했습니다.',
  'INVENTORY FULL / clear space before collecting.':'인벤토리가 가득 찼습니다. 빈칸을 확보한 후 회수하세요.',
  'INVENTORY FULL / 2 x 2 cells required.':'산소통을 넣을 2×2 크기의 빈 공간이 필요합니다.',
  'OXYGEN TANK stored. [Q] to use.':'산소통을 보관했습니다. [Q]로 사용할 수 있습니다.',
  'Oxygen is full. Save your tank.':'산소가 충분합니다. 산소통을 아껴두세요.',
  'Oxygen tank used. +55 oxygen.':'산소통을 사용해 산소를 55 회복했습니다.',
  'No oxygen tanks. Find green canisters in the city.':'예비 산소통이 없습니다. 도시에서 초록색 산소통을 찾아보세요.',
  'Battery depleted. Flashlight disabled.':'배터리가 소진되어 손전등을 켤 수 없습니다.'
};
function ko(value){
  let text=String(value);
  if(KO_TEXT[text])return KO_TEXT[text];
  text=text.replace(/^\[E\] UNLOCK REPAIR STAGE (\d+) \/ (.+?)(?: \((\d+)\/2\))?$/,(_,step,item,count)=>`[E] ${step}단계 수리 시작 · ${ko(item)}${count!==undefined?` (${count}/2개)`:''}`)
    .replace(/^Collect 2 (.+)\.$/,(_,item)=>`${ko(item)} 2개가 필요합니다.`)
    .replace(/^(.+) installed\. Subsystem restored\.$/,(_,item)=>`${ko(item)} 설치 완료 · 장치를 복구했습니다.`);
  for(const key of ['POWER CELL','HULL ALLOY','DRIVE PART','OXYGEN TANK','FLASHLIGHT'])text=text.replaceAll(key,KO_TEXT[key]);
  return text;
}
