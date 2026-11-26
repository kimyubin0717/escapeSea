'use strict';
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const code=fs.readFileSync(path.join(root,'Content/UI/scripts/runtime.js'),'utf8');
function setup(runtime){
  const messages=[],nodes=new Map();
  const node=id=>{if(!nodes.has(id))nodes.set(id,{checked:true,style:{}});return nodes.get(id);};
  const ctx=vm.createContext({URLSearchParams,location:{search:runtime?'?runtime=1':''},console:{log:s=>messages.push(s)},document:{body:{classList:{add(){}}}},requestAnimationFrame(){},performance:{now:()=>0},render(){},notifyToast(){}});
  vm.runInContext(`
    let snapshotForTest=null;
    let state={oxygen:100},phase=0,view='menu',runActive=false,eventToasts=[],bag={selected:'a',items:[],definition:()=>({name:'item'})};
    const $=id=>getNode(id),screen={querySelector:()=>getNode('zone')};
    const base=()=>({oxygen:100});
    let localCalls=0;
    function menuAction(){localCalls++;} function syncBag(){state.oxygen=-10;}
    function saveRun(){state.oxygen=-20;} function readSavedRun(){return null;}
    function showMenu(){} function startNewRun(){} function continueRun(){}
    function action(){} function moveCargo(){} function animate(){state.oxygen=-30;}
  `,Object.assign(ctx,{getNode:node}));
  ctx.window=ctx;vm.runInContext(code,ctx);
  return {ctx,messages,node,run:s=>vm.runInContext(s,ctx)};
}
const preview=setup(false);
assert.equal(preview.ctx.abyssRuntime,undefined,'Preview must not install runtime bridge');
assert.equal(preview.messages.length,0);
const rt=setup(true);
assert.equal(rt.messages[0],'ABYSS_UI_READY');
rt.ctx.abyssRuntime.receive({oxygen:37,health:64,battery:9,elapsed:123,menu:false,hasSave:true,phase:.6,salvageTarget:.4,salvageWindow:.17},[{id:'a',kind:'tank',x:1,y:2}]);
assert.equal(rt.run('state.oxygen'),37);
assert.equal(rt.run('view'),'game');
assert.equal(rt.run('bag.selected'),'a');
assert.equal(rt.node('marker').style.left,'60%');
assert.equal(rt.node('zone').style.left,'40%');
assert.equal(rt.node('zone').style.width,'17%');
rt.run('syncBag();saveRun();animate(10)');
assert.equal(rt.run('state.oxygen'),37,'Preview simulation must not modify authoritative state');
assert.equal(rt.run('readSavedRun().state.elapsed'),123);
rt.run("action(' ');action('q');moveCargo('a',2,3);startNewRun();continueRun();showMenu();menuAction('help');menuAction('quit')");
const sent=rt.messages.slice(1).map(s=>JSON.parse(s.slice('ABYSS_ACTION:'.length)));
assert.deepEqual(sent.map(x=>x.action),['space','q','cargo','new','continue','menu','quit']);
assert.deepEqual(JSON.parse(sent[2].payload),{id:'a',x:2,y:3});
assert.equal(rt.run('localCalls'),1,'Help remains local; game actions go to C++');
rt.ctx.abyssRuntime.receive({oxygen:35,menu:true,hasSave:false,phase:0,salvageTarget:.4,salvageWindow:.17},[]);
assert.equal(rt.run('bag.selected'),null);
assert.equal(rt.run('readSavedRun()'),null);
assert.equal(rt.run('view'),'menu');
const rawState={oxygen:88.4624,battery:102.2495,health:64.9,menu:false,phase:0,salvageTarget:.4,salvageWindow:.17};
rt.ctx.abyssRuntime.receive(rawState,[]);
assert.equal(rt.run('state.oxygen'),89);
assert.equal(rt.run('state.battery'),103);
assert.equal(rt.run('state.health'),65);
assert.equal(rawState.oxygen,88.4624,'Display formatting must preserve incoming game data');
console.log('PASS: preview isolation, C++ state, salvage bounds, inventory, input, menu, save routing and integer display');

