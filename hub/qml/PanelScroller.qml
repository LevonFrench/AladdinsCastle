// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
// Stick scrolling for the SteamVR panel: whole rows at a time, never a part of one.
// SteamVR sends a held stick as a stream of small smooth-scroll packets; a mouse wheel sends notches.
// Both arrive here as wheel angles (120 = one notch). The rules:
//  - Dead zone: packets weaker than deadZone are ignored, so a resting stick never drifts the page.
//  - The first row moves once a push adds up to one notch: at once for a wheel notch after a pause,
//    or after two packets in the same direction for a stick, so stick noise flipping between up and
//    down never moves the page (no jitter).
//  - Held, it repeats: slowly at first, then faster the longer it is held (acceleration).
//  - Every move animates contentY to a row boundary; a new row while moving retargets from there.
QtObject {
 id:scroller
 property Item flick:null
 property real rowHeight:100
 property bool reduceMotion:false
 property int deadZone:24
 property int notch:120
 // Gap after which a new push counts as fresh.
 property int pauseMs:180
 property int firstRepeatMs:320
 property int fastestRepeatMs:75
 // How many rows the stick has moved, for checks.
 property int steps:0
 property int targetRow:0
 // Stream state.
 property int holdDir:0
 property real holdStart:0
 property real lastEvent:-100000
 property real lastStep:-100000
 property real acc:0
 property int runPackets:0
 property int rowsThisHold:0
 readonly property real minY:flick?flick.originY-flick.topMargin:0
 readonly property real maxY:flick?Math.max(minY,flick.originY+flick.contentHeight+flick.bottomMargin-flick.height):0
 readonly property int lastRow:rowHeight>0?Math.ceil((maxY-minY)/rowHeight-0.001):0
 property QtObject motion:NumberAnimation {target:scroller.flick;property:"contentY";duration:170;easing.type:Easing.OutCubic}
 function clampRow(row){return Math.max(0,Math.min(lastRow,row))}
 function rowAt(y){return clampRow(Math.round((y-minY)/rowHeight))}
 function yOf(row){return Math.max(minY,Math.min(maxY,minY+row*rowHeight))}
 // The row the page is at, or heading to while it moves.
 function currentRow(){return motion.running?targetRow:rowAt(flick?flick.contentY:0)}
 function scrollToRow(row){
  if(!flick)return
  targetRow=clampRow(row);const y=yOf(targetRow)
  motion.stop()
  if(reduceMotion||Math.abs(flick.contentY-y)<0.5){flick.contentY=y;return}
  motion.from=flick.contentY;motion.to=y;motion.start()
 }
 // Keeps a row fully in view, moving as little as possible, still on row boundaries.
 function ensureRowVisible(row){
  if(!flick)return
  const visibleRows=Math.max(1,Math.floor((flick.height+0.5)/rowHeight))
  const top=currentRow()
  if(row<top)scrollToRow(row)
  else if(row>top+visibleRows-1)scrollToRow(row-visibleRows+1)
 }
 // One wheel packet; returns whether it moved a row. dy > 0 is up (towards the first row).
 function wheel(dy,now){
  if(!flick||Math.abs(dy)<deadZone)return false
  const dir=dy>0?-1:1
  const fresh=now-lastEvent>pauseMs
  if(fresh||dir!==holdDir){
   // A push in the other direction while the stream runs starts over and must prove itself.
   holdDir=dir;holdStart=now;acc=0;runPackets=0;rowsThisHold=0;lastStep=-100000
  }
  const wasFresh=fresh
  lastEvent=now;acc+=Math.abs(dy);++runPackets
  let move=false
  if(rowsThisHold===0)move=acc>=notch&&(wasFresh&&runPackets===1||runPackets>=2)
  else{
   const held=now-holdStart
   const interval=rowsThisHold===1?firstRepeatMs:Math.max(fastestRepeatMs,220-held*0.12)
   move=now-lastStep>=interval
  }
  if(!move)return false
  const from=currentRow(),to=clampRow(from+dir)
  acc=0;lastStep=now;++rowsThisHold
  if(to===from)return false
  ++steps;scrollToRow(to);return true
 }
}
