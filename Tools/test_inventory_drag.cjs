'use strict';
const fs=require('node:fs');
const vm=require('node:vm');
const assert=require('node:assert/strict');
const source=fs.readFileSync(require('node:path').join(__dirname,'../Content/UI/scripts/app.js'),'utf8');
const handlers={},windowHandlers={},moves=[];
const grid={clientWidth:518,getBoundingClientRect:()=>({left:0,top:0,right:414.4,bottom:414.4,width:414.4})};
const classes=new Set();
const item={dataset:{item:'tank'},style:{},classList:{add:k=>classes.add(k),remove:k=>classes.delete(k)},
  closest:selector=>selector==='.cargo-grid'?grid:null,setPointerCapture(){},hasPointerCapture:()=>false};
const context=vm.createContext({Math,parseFloat,getComputedStyle:()=>({columnGap:'4px'}),setTimeout:fn=>fn(),
  screen:{addEventListener:(name,fn)=>{handlers[name]=fn;},getBoundingClientRect:()=>({width:1280})},
  window:{addEventListener:(name,fn)=>{windowHandlers[name]=fn;}},
  bag:{items:[{id:'tank',x:0,y:0,w:2,h:2}],selected:null},bagMessage:'',render(){},
  moveCargo:(...args)=>moves.push(args)});
const dragStart=source.indexOf('let cargoDrag=');
vm.runInContext(source.slice(dragStart,source.indexOf("for(const key of ['oxygen','battery'",dragStart)),context);
const event=(x,y,id=1)=>({pointerId:id,button:0,clientX:x,clientY:y,preventDefault(){},target:{closest:()=>item}});
handlers.pointerdown(event(20,20));
assert.ok(classes.has('dragging'),'The node survives incoming state from the initial press');
handlers.pointermove(event(200,200));
handlers.pointerup(event(200,200));
assert.deepEqual(moves.pop(),['tank',3,3],'Drag maps scaled coordinates to the right cell');
assert.ok(!classes.has('dragging'));
handlers.pointerdown(event(20,20));handlers.pointerup(event(20,20));
assert.equal(context.bag.selected,null,'A click toggles the already selected item without moving it');
assert.equal(moves.length,0);
handlers.pointerdown(event(20,20));handlers.pointerup(event(200,200));
assert.deepEqual(moves.pop(),['tank',3,3],'CEF release coordinates work when pointermove is missing');
handlers.pointerdown(event(20,20));handlers.pointermove(event(500,500));handlers.pointerup(event(500,500));
assert.equal(moves.length,0,'Dropping outside the grid does not move cargo');
handlers.pointerdown(event(20,20));handlers.pointermove(event(200,200));
handlers.pointerup(event(200,200,2));
assert.ok(classes.has('dragging'),'Other pointers cannot finish a drag');
windowHandlers.blur({});
assert.ok(!classes.has('dragging'),'Losing focus cancels the drag');
assert.equal(moves.length,0);
console.log('PASS: scaled drag, click selection, outside drop, pointer identity and focus cancellation');
