'use strict';
// Coordinates and footprints are in cells. Items never stack or rotate.
class GridInventory {
  constructor(){this.items=[];this.serial=0;this.selected=null;}
  definition(kind){
    return {
      tank:{name:'산소통',w:2,h:2},
      flashlight:{name:'손전등',w:3,h:1},
      power:{name:'전력 전지',w:1,h:1},
      hull:{name:'선체 합금',w:1,h:1},
      drive:{name:'추진 부품',w:1,h:1}
    }[kind];
  }
  fits(item,x,y,ignore=item.id){
    return Number.isInteger(x)&&Number.isInteger(y)&&x>=0&&y>=0&&x+item.w<=7&&y+item.h<=7&&
      !this.items.some(other=>other.id!==ignore&&x<other.x+other.w&&x+item.w>other.x&&y<other.y+other.h&&y+item.h>other.y);
  }
  add(kind){
    const item={...this.definition(kind),kind,id:'gear-'+this.serial++};
    for(let y=0;y<7;y++)for(let x=0;x<7;x++)if(this.fits(item,x,y)){this.items.push({...item,x,y});return true;}
    return false;
  }
  move(id,x,y){const item=this.items.find(i=>i.id===id);if(!item||!this.fits(item,x,y))return false;item.x=x;item.y=y;return true;}
  sync(wanted){
    let complete=true;
    for(const [kind,count] of Object.entries(wanted)){
      let existing=this.items.filter(i=>i.kind===kind);
      while(existing.length>count){const removed=existing.pop();this.items=this.items.filter(i=>i.id!==removed.id);}
    }
    for(const [kind,count] of Object.entries(wanted))while(this.items.filter(i=>i.kind===kind).length<count){if(!this.add(kind)){complete=false;break;}}
    if(!this.items.some(i=>i.id===this.selected))this.selected=null;
    return complete;
  }
  count(kind){return this.items.filter(i=>i.kind===kind).length;}
  get used(){return this.items.reduce((total,item)=>total+item.w*item.h,0);}
}
