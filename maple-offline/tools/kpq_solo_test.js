class Rect { constructor(x,y,w,h){this.x=x;this.y=y;this.w=w;this.h=h;} contains(p){return p.x>=this.x&&p.x<this.x+this.w&&p.y>=this.y&&p.y<this.y+this.h;} }
const fs=require('fs');
let src=fs.readFileSync('npc/9020001.js','utf8');
const start=src.indexOf('function rectangleStages'); const end=src.indexOf('var status = -1;');
eval(src.substring(start,end));
const rects=[new Rect(-755,-132,4,218),new Rect(-721,-340,4,166),new Rect(-586,-326,4,150),new Rect(-483,-181,4,222)];
const combos=[[0,1,1,1],[1,0,1,1],[1,1,0,1],[1,1,1,0]];
function eim(players, c){ const props={p:String(c)}; return {getProperty:k=>props[k]??null,setProperty:(k,v)=>props[k]=v,getPlayers:()=>({get:i=>({getPosition:()=>players[i]})}),getPlayerCount:()=>players.length}; }
const onRope=i=>({x:rects[i].x+1,y:rects[i].y+50});
for (let c=0;c<4;c++) for (let r=0;r<4;r++) console.log('combo',c,'rope',r,'solo accept',rectangleStages(eim([onRope(r)],c),'p',combos,rects),'expected',combos[c][r]==1);
console.log('solo off rope', rectangleStages(eim([{x:0,y:0}],0),'p',combos,rects));
console.log('3 players correct', rectangleStages(eim([onRope(1),onRope(2),onRope(3)],0),'p',combos,rects));
console.log('3 players wrong', rectangleStages(eim([onRope(0),onRope(2),onRope(3)],0),'p',combos,rects));
console.log('2 players both correct', rectangleStages(eim([onRope(1),onRope(2)],0),'p',combos,rects));
console.log('2 players same rope', rectangleStages(eim([onRope(1),onRope(1)],0),'p',combos,rects));
