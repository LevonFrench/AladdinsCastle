// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
// Stick scrolling for the SteamVR panel, after Steam's: the page glides while the stick is held, as fast as the
// stick is pushed but never faster than a set cap, carries on a little when the stick is let go, and always comes
// to rest on a row, never part-way through one.
// SteamVR sends a held stick as a stream of small smooth-scroll packets, one per headset frame, each as large as
// the push; a mouse wheel sends whole notches (120). Both arrive here as wheel angles. The rules:
//  - A stream must prove itself: the page starts to glide on the second packet in the same direction, so stick
//    noise flipping between up and down never moves it (no jitter). Packets far weaker than the strongest pushes
//    seen are a resting thumb and are ignored (dead zone).
//  - How hard the stick is pushed sets the speed, between minRowsPerSecond and the cap, maxRowsPerSecond. The
//    speed rises and falls gently (rampSeconds from rest to the cap), so a push never jerks the page.
//    The packet size of a full push is learned from the pushes themselves, so the feel does not hang on SteamVR's
//    packet scale.
//  - Let go (no packet for releaseMs) and the page coasts on, slowing, to the next row in the direction it was
//    going, past where it would have stopped by itself. A short nudge moves exactly one row.
//  - A single whole wheel notch after a pause moves one row per notch, as on the desktop.
// The clock is the wall clock; checks set manualClock and drive wheel() and tick() with their own times.
QtObject {
 id:scroller
 property Item flick:null
 property real rowHeight:100
 // Optional rest positions (content offsets from the top, ascending) instead of one every rowHeight: a page with
 // a header above its rows lists the top, the header's end and then each row.
 property var stops:[]
 property bool reduceMotion:false
 property int notch:120
 // A gap this long between packets starts a new push.
 property int pauseMs:180
 // No packet for this long while gliding: the stick was let go.
 property int releaseMs:110
 // Packets weaker than this share of a full push are ignored.
 property real deadZone:0.12
 property real minRowsPerSecond:1.2
 property real maxRowsPerSecond:5
 property real rampSeconds:0.35
 // How briskly the page moves to a row from rest (a keyboard step, a wheel notch, a jump to the top): a
 // critically damped spring, per second.
 property real settleRate:13
 property bool manualClock:false
 // For checks and the input log: what the scroller is doing ("idle", "glide" or "settle"), its speed in pixels
 // a second (down is positive), the fastest it went in this push, how many pushes or notches moved the page, and
 // the rest position it is heading to or resting at.
 property string phase:"idle"
 property real velocity:0
 property real peakSpeed:0
 property int steps:0
 property int targetRow:0
 // Stream state.
 property int holdDir:0
 property int runPackets:0
 property real lastEvent:-100000
 property real lastTick:-1
 property real push:0
 property real streamPeak:0
 property real fullPush:0
 property real settleY:0
 // The fastest a settle may go: the cap after a push or a notch; a jump (keyboard, Home) is not held to it.
 property real settleCap:0
 readonly property real minY:flick?flick.originY-flick.topMargin:0
 readonly property real maxY:flick?Math.max(minY,flick.originY+flick.contentHeight+flick.bottomMargin-flick.height):0
 readonly property real maxSpeed:rowHeight*maxRowsPerSecond
 readonly property real minSpeed:rowHeight*minRowsPerSecond
 // Every rest position, in contentY, from the top to the bottom of the page.
 readonly property var restYs:{
  const list=[]
  if(!flick)return [0]
  if(stops&&stops.length){for(const s of stops){const y=Math.min(maxY,minY+s);if(!list.length||y>list[list.length-1]+0.5)list.push(y)}}
  else if(rowHeight>0){for(let y=minY;y<maxY-0.5;y+=rowHeight)list.push(y)}
  if(!list.length||list[list.length-1]<maxY-0.5)list.push(maxY)
  return list
 }
 readonly property int lastRow:restYs.length-1
 property QtObject frames:FrameAnimation {running:scroller.phase!=="idle"&&!scroller.manualClock;onTriggered:scroller.tick(Date.now())}
 function clampRow(row){return Math.max(0,Math.min(lastRow,row))}
 function yOf(row){return restYs[clampRow(row)]}
 function rowAt(y){let best=0;for(let i=1;i<restYs.length;++i)if(Math.abs(restYs[i]-y)<Math.abs(restYs[best]-y))best=i;return best}
 // The row the page rests at, or is heading to while it settles.
 function currentRow(){return phase==="settle"?targetRow:rowAt(flick?flick.contentY:0)}
 function stopAll(){phase="idle";velocity=0;lastTick=-1}
 function settleTo(row,cap){
  if(!flick)return
  targetRow=clampRow(row);settleY=yOf(targetRow);settleCap=cap||0
  if(reduceMotion||Math.abs(flick.contentY-settleY)<0.5&&Math.abs(velocity)<1){flick.contentY=settleY;stopAll();return}
  if(phase==="idle")lastTick=-1
  phase="settle"
 }
 function scrollToRow(row){holdDir=0;runPackets=0;velocity=0;settleTo(row,0)}
 // Keeps a row fully in view, moving as little as possible, still resting on rows.
 function ensureRowVisible(row){
  if(!flick)return
  const visibleRows=Math.max(1,Math.floor((flick.height+0.5)/Math.max(1,rowHeight)))
  const top=currentRow()
  if(row<top)scrollToRow(row)
  else if(row>top+visibleRows-1)scrollToRow(row-visibleRows+1)
 }
 // The let-go: on to the first row at or past where the page would coast to, in the direction it was going.
 function release(){
  if(!flick)return
  const dir=velocity>0?1:velocity<0?-1:holdDir
  const decel=maxSpeed/Math.max(0.05,rampSeconds)
  const coast=flick.contentY+velocity*Math.abs(velocity)/(2*decel)
  let row=rowAt(coast)
  if(dir>0)while(row<lastRow&&yOf(row)<coast-0.5)++row
  if(dir<0)while(row>0&&yOf(row)>coast+0.5)--row
  if(streamPeak>0)fullPush=Math.max(streamPeak,fullPush*0.98)
  settleTo(row,maxSpeed)
 }
 function speedFor(strength){
  const n=fullPush>0?Math.min(1,strength/fullPush):1
  return Math.min(maxSpeed,minSpeed+(maxSpeed-minSpeed)*Math.pow(n,1.5))
 }
 function moveBy(dy){
  const y=Math.max(minY,Math.min(maxY,flick.contentY+dy))
  if(y!==flick.contentY+dy)velocity=0
  flick.contentY=y
 }
 // One wheel packet; returns whether it moved or started to move the page. dy > 0 is up (towards the top).
 function wheel(dy,now){
  if(!flick||!dy)return false
  const size=Math.abs(dy)
  if(fullPush>0&&size<deadZone*fullPush)return false
  const dir=dy>0?-1:1
  const fresh=now-lastEvent>pauseMs
  if(fresh||dir!==holdDir){
   // A new push, or one in the other direction: it starts over and must prove itself.
   if(phase==="glide")release()
   holdDir=dir;runPackets=0;push=size;streamPeak=0
  }
  lastEvent=now;++runPackets
  push=push*0.6+size*0.4;streamPeak=Math.max(streamPeak,push)
  // A whole wheel notch after a pause moves a row per notch at once.
  if(fresh&&runPackets===1&&size%notch===0){
   const from=currentRow(),to=clampRow(from+dir*Math.round(size/notch))
   if(to===from)return false
   ++steps;velocity=0;settleTo(to,maxSpeed);return true
  }
  if(runPackets<2)return false
  if(phase!=="glide"){
   // The page is at the top or bottom already and the stick pushes further: nothing to do.
   if(dir>0&&flick.contentY>=maxY-0.5||dir<0&&flick.contentY<=minY+0.5)return false
   if(phase==="idle")lastTick=-1
   phase="glide";++steps;peakSpeed=0
  }
  if(manualClock||lastTick<0)tick(now)
  return true
 }
 // One frame: glide at the stick's speed while it is held, else settle on the chosen row.
 function tick(now){
  if(!flick||phase==="idle")return
  const dt=lastTick<0?0:Math.min(0.05,Math.max(0,(now-lastTick)/1000));lastTick=now
  if(phase==="glide"){
   if(now-lastEvent>releaseMs)release()
   else{
    const target=holdDir*speedFor(push),step=maxSpeed/Math.max(0.05,rampSeconds)*dt
    velocity+=Math.max(-step,Math.min(step,target-velocity))
    velocity=Math.max(-maxSpeed,Math.min(maxSpeed,velocity))
    peakSpeed=Math.max(peakSpeed,Math.abs(velocity))
    moveBy(velocity*dt)
    return
   }
  }
  if(phase!=="settle")return
  const dist=settleY-flick.contentY,dir=dist>0?1:-1
  if(Math.abs(dist)<0.5){flick.contentY=settleY;stopAll();return}
  // Coasting in from a push: slow down evenly to rest exactly on the row. It only ever slows.
  const decel=velocity*dir>0?velocity*velocity/(2*Math.abs(dist)):0
  if(decel>=maxSpeed/Math.max(0.05,rampSeconds)/4){
   const v1=velocity-dir*decel*dt
   if(v1*dir<=0){flick.contentY=settleY;stopAll();return}
   const y=flick.contentY+(velocity+v1)/2*dt
   if((settleY-y)*dir<0.5){flick.contentY=settleY;stopAll();return}
   velocity=v1;flick.contentY=y;return
  }
  // From rest, or too slow to coast there: an exact critically damped spring, x(t)=(x0+(v0+w x0)t)e^-wt.
  // It never passes the row; after a push or a notch it never goes faster than the cap.
  const w=settleRate,x0=-dist,v0=velocity,e=Math.exp(-w*dt)
  let x=(x0+(v0+w*x0)*dt)*e
  velocity=(v0-w*(v0+w*x0)*dt)*e
  if(settleCap>0&&Math.abs(velocity)>settleCap){velocity=Math.sign(velocity)*settleCap;x=x0+velocity*dt}
  if(Math.sign(x)!==Math.sign(x0)||Math.abs(x)<0.5&&Math.abs(velocity)<40){flick.contentY=settleY;stopAll();return}
  flick.contentY=settleY+x
 }
}
