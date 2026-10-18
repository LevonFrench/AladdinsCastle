// AC VR Hub site: a scroll-driven walk from the mall corridor into an arcade.
// Everything is built in code (no photos or third-party art): boxes, canvas textures, emissive neon.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { ShaderPass } from 'three/addons/postprocessing/ShaderPass.js';
import { FXAAShader } from 'three/addons/shaders/FXAAShader.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

const canvas = document.getElementById('scene');
const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)').matches;
const isSmall = matchMedia('(max-width: 720px)').matches;
let portrait = innerWidth < innerHeight;

let renderer;
try {
  // antialias is lost under the composer; FXAA runs as the last pass instead.
  // A software or blocklisted GPU falls back to the text page rather than crawling.
  renderer = new THREE.WebGLRenderer({ canvas, antialias: false, powerPreference: 'high-performance', failIfMajorPerformanceCaveat: true });
} catch (err) {
  console.warn('WebGL unavailable, showing the text version', err);
  throw err; // body never gets .js, so the static layout stays
}
document.body.classList.add('js');

// a short curtain while the fonts arrive, so canvas signs draw in the right face
const curtain = document.createElement('div');
curtain.className = 'loading';
curtain.textContent = 'INSERT TOKEN…';
document.body.appendChild(curtain);

// Anything that breaks after this point drops back to the readable text page instead of
// leaving the curtain up or a frozen canvas.
let stopped = false;
function fallBackToText(reason) {
  if (stopped) return;
  stopped = true;
  console.warn('Arcade walk stopped, showing the text version:', reason);
  document.body.classList.remove('js');
  curtain.remove();
  document.querySelectorAll('.card').forEach(c => { c.style.removeProperty('--vis'); c.removeAttribute('data-hidden'); });
}
addEventListener('error', e => fallBackToText(e.error || e.message));
addEventListener('unhandledrejection', e => fallBackToText(e.reason));
canvas.addEventListener('webglcontextlost', e => { e.preventDefault(); fallBackToText('WebGL context lost'); });
await Promise.race([
  Promise.all([
    document.fonts.load('28px "Press Start 2P"'),
    document.fonts.load('900 120px "Cinzel Decorative"'),
  ]),
  new Promise(r => setTimeout(r, 3000)),
]).catch(() => {});

renderer.setPixelRatio(Math.min(devicePixelRatio, isSmall ? 1.5 : 1.75));
renderer.setSize(innerWidth, innerHeight);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;
renderer.outputColorSpace = THREE.SRGBColorSpace;

const scene = new THREE.Scene();
scene.background = new THREE.Color('#0d0818');
scene.fog = new THREE.Fog('#0d0818', 18, 46);

const camera = new THREE.PerspectiveCamera(58, innerWidth / innerHeight, 0.05, 80);

const pmrem = new THREE.PMREMGenerator(renderer);
const envTex = pmrem.fromScene(new RoomEnvironment(renderer), 0.04).texture;

const maxAniso = renderer.capabilities.getMaxAnisotropy();
const clickables = [];
const animated = [];   // every frame
const screens = [];    // canvas redraws, throttled

// ---------------------------------------------------------------- helpers
function makeCanvas(w, h) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  return [c, c.getContext('2d')];
}
function tex(c, { repeat, srgb = true } = {}) {
  const t = new THREE.CanvasTexture(c);
  if (srgb) t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = maxAniso;
  if (repeat) { t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(...repeat); }
  return t;
}
// Materials and geometries are shared by value: the scene builds ~800 meshes from a few dozen of each.
const matCache = new Map(), geoCache = new Map();
const cacheKey = o => Object.keys(o).sort().map(k => {
  const val = o[k];
  return k + ':' + (val && val.isTexture ? val.uuid : val && val.isColor ? val.getHexString() : String(val));
}).join('|');
function std(color, opts = {}) {
  const k = 's' + color + cacheKey(opts);
  if (!matCache.has(k)) matCache.set(k, new THREE.MeshStandardMaterial({ color, roughness: 0.6, metalness: 0.05, ...opts }));
  return matCache.get(k);
}
function glow(color, intensity = 2) {
  const k = 'g' + color + intensity;
  if (!matCache.has(k)) matCache.set(k, new THREE.MeshStandardMaterial({ color: '#000', emissive: color, emissiveIntensity: intensity, roughness: 1 }));
  return matCache.get(k);
}
function basicMat(map) {  // one unlit material per shared screen texture
  const k = 'm' + map.uuid;
  if (!matCache.has(k)) matCache.set(k, new THREE.MeshBasicMaterial({ map }));
  return matCache.get(k);
}
function boxGeo(w, h, d) {
  const k = `b${w},${h},${d}`;
  if (!geoCache.has(k)) geoCache.set(k, new THREE.BoxGeometry(w, h, d));
  return geoCache.get(k);
}
function planeGeo(w, h) {
  const k = `p${w},${h}`;
  if (!geoCache.has(k)) geoCache.set(k, new THREE.PlaneGeometry(w, h));
  return geoCache.get(k);
}
function box(w, h, d, mat, x = 0, y = 0, z = 0, parent = scene) {
  const m = new THREE.Mesh(boxGeo(w, h, d), mat);
  m.position.set(x, y, z);
  parent.add(m);
  return m;
}
function plane(w, h, mat, x = 0, y = 0, z = 0, parent = scene) {
  const m = new THREE.Mesh(planeGeo(w, h), mat);
  m.position.set(x, y, z);
  parent.add(m);
  return m;
}
const v = (x, y, z) => new THREE.Vector3(x, y, z);
const smooth = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
const pix = (g, size) => { g.font = `${size}px "Press Start 2P", monospace`; g.textAlign = 'center'; g.textBaseline = 'middle'; };
function rng(seed) {
  return () => ((seed = (seed * 16807) % 2147483647) - 1) / 2147483646;
}
function markClickable(obj, stop) {
  obj.traverse(o => { if (o.isMesh) { o.userData.stop = stop; clickables.push(o); } });
}

const PAL = {
  red: '#e0393e', yellow: '#f6c744', teal: '#19a7a0', purple: '#6b4fbb',
  magenta: '#e2468f', blue: '#2d6cdf', orange: '#f28a2e', lime: '#9bdc3c',
};
const muralColors = [PAL.red, PAL.yellow, PAL.teal, PAL.purple, PAL.magenta, PAL.blue, PAL.orange];

// ---------------------------------------------------------------- textures
function mallTileTex() {
  const [c, g] = makeCanvas(512, 512);
  g.fillStyle = '#e8e0cf'; g.fillRect(0, 0, 512, 512);
  const r = rng(7);
  for (let i = 0; i < 4; i++) for (let j = 0; j < 4; j++) {
    const v = 226 + Math.floor(r() * 14);
    g.fillStyle = `rgb(${v},${v - 6},${v - 20})`;
    g.fillRect(i * 128 + 3, j * 128 + 3, 122, 122);
  }
  for (let k = 0; k < 2500; k++) {
    g.fillStyle = `rgba(120,100,80,${r() * 0.08})`;
    g.fillRect(r() * 512, r() * 512, 2, 2);
  }
  return tex(c, { repeat: [12, 9] });
}

function checkerTex(cols, rows, a = '#2c6b66', b = '#eeeae0') {
  const s = 32;
  const [c, g] = makeCanvas(cols * s, rows * s);
  for (let i = 0; i < cols; i++) for (let j = 0; j < rows; j++) {
    g.fillStyle = (i + j) % 2 ? a : b;
    g.fillRect(i * s, j * s, s, s);
    g.strokeStyle = 'rgba(0,0,0,0.12)';
    g.strokeRect(i * s + 0.5, j * s + 0.5, s - 1, s - 1);
  }
  return tex(c);
}

function carpetTex() {
  const [c, g] = makeCanvas(1024, 1024);
  g.fillStyle = '#100a1e'; g.fillRect(0, 0, 1024, 1024);
  const r = rng(42);
  const cols = ['#33e0ff', '#ff4fa8', '#f6d044', '#8de04a', '#a46bff'];
  g.lineWidth = 7; g.lineCap = 'round';
  for (let i = 0; i < 70; i++) {
    const col = cols[Math.floor(r() * cols.length)];
    g.strokeStyle = col; g.fillStyle = col;
    const x = r() * 1024, y = r() * 1024, kind = r();
    g.save(); g.translate(x, y); g.rotate(r() * Math.PI * 2);
    if (kind < 0.3) { // squiggle
      g.beginPath(); g.moveTo(-40, 0);
      for (let k = -40; k <= 40; k += 4) g.lineTo(k, Math.sin(k / 6) * 9);
      g.stroke();
    } else if (kind < 0.5) { // ring
      g.beginPath(); g.arc(0, 0, 10 + r() * 14, 0, Math.PI * 2); g.stroke();
    } else if (kind < 0.7) { // triangle outline
      g.beginPath(); g.moveTo(0, -18); g.lineTo(16, 12); g.lineTo(-16, 12); g.closePath(); g.stroke();
    } else if (kind < 0.85) { // zigzag
      g.beginPath(); g.moveTo(-30, 0);
      for (let k = 0; k < 6; k++) g.lineTo(-30 + (k + 1) * 10, k % 2 ? 0 : -12);
      g.stroke();
    } else { // dot cluster
      for (let k = 0; k < 5; k++) { g.beginPath(); g.arc(r() * 30 - 15, r() * 30 - 15, 4, 0, 7); g.fill(); }
    }
    g.restore();
  }
  // faint long racing lines, like the tape lines in the photos
  g.globalAlpha = 0.5; g.lineWidth = 4;
  g.strokeStyle = '#ff5b4f'; g.beginPath(); g.moveTo(0, 300); g.lineTo(1024, 760); g.stroke();
  g.strokeStyle = '#6fb6ff'; g.beginPath(); g.moveTo(300, 1024); g.lineTo(760, 0); g.stroke();
  g.globalAlpha = 1;
  return tex(c, { repeat: [5, 8] });
}

function muralTex() {
  // the bright 80s/90s geometric band: rows of triangles over stripes
  const [c, g] = makeCanvas(1024, 256);
  const r = rng(11);
  const tw = 64, th = 64;
  for (let row = 0; row < 3; row++) for (let i = -1; i < 1024 / tw + 1; i++) {
    const x = i * tw + (row % 2 ? tw / 2 : 0), y = 40 + row * th * 0.9;
    g.fillStyle = muralColors[Math.floor(r() * muralColors.length)];
    g.beginPath(); g.moveTo(x, y + th); g.lineTo(x + tw / 2, y); g.lineTo(x + tw, y + th); g.fill();
    g.fillStyle = muralColors[Math.floor(r() * muralColors.length)];
    g.beginPath(); g.moveTo(x + tw / 2, y); g.lineTo(x + tw * 1.5, y); g.lineTo(x + tw, y + th); g.fill();
  }
  g.fillStyle = PAL.yellow; g.fillRect(0, 0, 1024, 14);
  g.fillStyle = PAL.teal; g.fillRect(0, 14, 1024, 12);
  g.fillStyle = PAL.magenta; g.fillRect(0, 26, 1024, 14);
  g.fillStyle = PAL.yellow; g.fillRect(0, 232, 1024, 10);
  g.fillStyle = PAL.teal; g.fillRect(0, 242, 1024, 14);
  return tex(c, { repeat: [3, 1] });
}

function ceilingTex() {
  const [c, g] = makeCanvas(256, 256);
  g.fillStyle = '#d9d4ca'; g.fillRect(0, 0, 256, 256);
  const r = rng(5);
  for (let k = 0; k < 900; k++) { g.fillStyle = `rgba(90,80,70,${r() * 0.15})`; g.fillRect(r() * 256, r() * 256, 1.5, 1.5); }
  g.strokeStyle = '#a9a398'; g.lineWidth = 6; g.strokeRect(0, 0, 256, 256);
  return tex(c, { repeat: [16, 12] });
}

function makeSign() {
  // Storefront sign: ARCADE lights up, R-A-D-E die like old neon, A and C slide together, VR HUB flickers on.
  const W = 2048, H = 360;
  const [c, g] = makeCanvas(W, H);
  const [bc, bg] = makeCanvas(W, H);
  bg.fillStyle = '#4c5d58'; bg.fillRect(0, 0, W, H);
  const s = 22;
  for (let i = 0; i < W / s; i++) for (const y of [0, H - s]) { bg.fillStyle = i % 2 ? '#20302d' : '#9fb0aa'; bg.fillRect(i * s, y, s, s); }
  for (let j = 1; j < H / s - 1; j++) for (const x of [0, W - s]) { bg.fillStyle = j % 2 ? '#20302d' : '#9fb0aa'; bg.fillRect(x, j * s, s, s); }
  const font = size => `900 ${size}px "Cinzel Decorative", Georgia, serif`;
  let size = 200;
  g.font = font(size);
  while (g.measureText('AC VR HUB').width > W - 220 && size > 60) { size -= 6; g.font = font(size); }
  const layout = text => {
    g.font = font(size);
    const total = g.measureText(text).width;
    let x = W / 2 - total / 2;
    return [...text].map(ch => { const w = g.measureText(ch).width; const o = { ch, x: x + w / 2 }; x += w; return o; });
  };
  const from = layout('ARCADE'), to = layout('AC VR HUB');
  const keep = [[from[0], to[0]], [from[2], to[1]]];
  const dying = [from[1], from[3], from[4], from[5]];
  const arriving = to.slice(2).filter(o => o.ch !== ' ');
  const grd = g.createLinearGradient(0, 70, 0, 290);
  grd.addColorStop(0, '#fff6d6'); grd.addColorStop(1, '#ffc65c');
  const flick = (t, k) => Math.sin(t * 41 + k * 13.7) + Math.sin(t * 23 + k * 5.3) > 0.3;
  const ease = x => x < 0 ? 0 : x > 1 ? 1 : x * x * (3 - 2 * x);
  function letter(ch, x, lit) {
    const y = H / 2 + 10;
    g.textAlign = 'center'; g.textBaseline = 'middle';
    if (lit <= 0) return;
    if (lit < 0.5) { g.shadowBlur = 0; g.fillStyle = 'rgba(70,58,44,0.9)'; g.fillText(ch, x, y); return; } // unlit tube
    g.fillStyle = grd;
    g.shadowColor = 'rgba(255,160,40,0.95)'; g.shadowBlur = 40; g.fillText(ch, x, y);
    g.shadowBlur = 8; g.fillText(ch, x, y);
  }
  let t0 = -1e9, done = false;
  const texture = tex(c);
  function draw(T) {
    g.drawImage(bc, 0, 0);
    g.font = font(size);
    // R, A, D, E: steady, then flicker to an unlit tube, then gone as A and C close the gap
    dying.forEach((o, k) => {
      let lit = 1;
      if (T > 1.2) lit = T > 2.3 ? 0.3 : (flick(T, k) && T < 2.3 - k * 0.15) ? 1 : 0.3;
      if (T > 2.5) lit = 0;
      letter(o.ch, o.x, lit);
    });
    const m = ease((T - 2.5) / 0.8);
    keep.forEach(([a, b]) => letter(a.ch, a.x + (b.x - a.x) * m, 1));
    arriving.forEach((o, k) => {
      const start = 3.4 + k * 0.08;
      const lit = T < start ? 0 : (T > 4.3 || flick(T, k + 9)) ? 1 : 0.3;
      letter(o.ch, o.x, lit);
    });
    texture.needsUpdate = true;
  }
  draw(reducedMotion ? 99 : 0);
  return {
    texture,
    restart(t) { if (!reducedMotion) { t0 = t; done = false; } },
    update(t) {
      if (done) return;
      const T = t - t0;
      if (T < 0) return;
      draw(T);
      if (T > 4.6) done = true;
    },
  };
}

function marqueeTex(seed, bg = PAL.purple) {
  // blank lit marquee: a seeded multicoloured pattern, no words or logos
  const [c, g] = makeCanvas(512, 128);
  let h = 7;
  for (const ch of String(seed)) h = (h * 31 + ch.charCodeAt(0)) % 2147483646;
  const r = rng(h + 1);
  const pick = () => muralColors[Math.floor(r() * muralColors.length)];
  g.fillStyle = bg; g.fillRect(0, 0, 512, 128);
  const style = h % 4;
  if (style === 0) {
    for (let x = -140; x < 660; x += 44) {
      g.fillStyle = pick();
      g.beginPath(); g.moveTo(x, 0); g.lineTo(x + 26, 0); g.lineTo(x - 102, 128); g.lineTo(x - 128, 128); g.fill();
    }
  } else if (style === 1) {
    for (let i = -1; i < 17; i++) for (let row = 0; row < 2; row++) {
      const x = i * 32 + (row ? 16 : 0), y = row * 64;
      g.fillStyle = pick();
      g.beginPath(); g.moveTo(x, y + 64); g.lineTo(x + 16, y); g.lineTo(x + 32, y + 64); g.fill();
    }
  } else if (style === 2) {
    const bands = 6;
    for (let i = 0; i < bands; i++) { g.fillStyle = pick(); g.fillRect(0, i * (128 / bands), 512, 128 / bands + 1); }
    for (let i = 0; i < 5; i++) { g.fillStyle = pick(); g.beginPath(); g.arc(60 + i * 98, 64, 30, 0, 7); g.fill(); }
  } else {
    for (let i = 0; i < 9; i++) {
      g.fillStyle = pick();
      g.beginPath(); g.moveTo(i * 64 - 32, 128); g.lineTo(i * 64, 20); g.lineTo(i * 64 + 32, 128); g.fill();
    }
    g.fillStyle = pick(); g.fillRect(0, 0, 512, 18);
  }
  g.strokeStyle = 'rgba(255,255,255,0.85)'; g.lineWidth = 6; g.strokeRect(5, 5, 502, 118);
  return tex(c);
}

function textSign(text, bg, fg) {
  const [c, g] = makeCanvas(256, 96);
  g.fillStyle = bg; g.fillRect(0, 0, 256, 96);
  pix(g, 28); g.shadowColor = fg; g.shadowBlur = 12; g.fillStyle = fg; g.fillText(text, 128, 50);
  return tex(c);
}

function tokenFaceTex(front) {
  const [c, g] = makeCanvas(512, 512);
  const grd = g.createRadialGradient(200, 180, 30, 256, 256, 260);
  grd.addColorStop(0, '#f7dc8a'); grd.addColorStop(0.6, '#cfa340'); grd.addColorStop(1, '#8a6519');
  g.fillStyle = grd; g.fillRect(0, 0, 512, 512);
  g.strokeStyle = '#7a5614'; g.lineWidth = 10;
  g.beginPath(); g.arc(256, 256, 226, 0, 7); g.stroke();
  g.fillStyle = '#6a4a10'; g.textAlign = 'center'; g.textBaseline = 'middle';
  if (front) {
    g.font = '900 78px "Cinzel Decorative", Georgia, serif';
    g.save(); g.translate(256, 256); g.rotate(-0.35);
    g.fillText('AC VR', 0, -46); g.fillText('HUB', 0, 46);
    g.restore();
  } else {
    // embossed lamp, echoing the old brass tokens without copying one
    g.save(); g.translate(256, 270); g.scale(5.2, 5.2); g.translate(-32, -34);
    g.lineWidth = 1.6; g.strokeStyle = '#5e410c'; g.fillStyle = '#a77a26';
    const p = new Path2D('M15 37c0-6 7-8 15-7.5 6 .4 10 2 14 1l9.5-5c1.2-.6 2.2.6 1.2 1.6L46 36c-4 4-10 6-18 6-8 0-13-1.5-13-5z');
    g.fill(p); g.stroke(p);
    g.stroke(new Path2D('M24 42l-2.5 4.5h15L34 42'));
    const lid = new Path2D('M25.5 30c0-4 2-6.5 4.5-6.5s4.5 2.5 4.5 6.5'); g.fill(lid); g.stroke(lid);
    g.stroke(new Path2D('M16 36c-7 .5-7.5-8-1.5-6.5'));
    g.restore();
    g.font = '30px "Press Start 2P", monospace';
    g.fillText('TRUE 3D VR', 256, 92);
    g.fillText('INSERT TOKEN', 256, 430);
  }
  return tex(c);
}

// ---------------------------------------------------------------- attract-mode screens
function screen(draw, w = 256, h = 192) {
  const [c, g] = makeCanvas(w, h);
  const t = tex(c);
  const s = { c, g, t, draw, w, h };
  screens.push(time => { draw(g, w, h, time); t.needsUpdate = true; });
  draw(g, w, h, 0);
  return t;
}

const screenGun = screen((g, w, h, t) => {
  // dusk skyline scrolling, crosshair hunting targets
  const sky = g.createLinearGradient(0, 0, 0, h);
  sky.addColorStop(0, '#2a1a5e'); sky.addColorStop(0.6, '#e2468f'); sky.addColorStop(1, '#f6c744');
  g.fillStyle = sky; g.fillRect(0, 0, w, h);
  g.fillStyle = '#140b26';
  for (let i = 0; i < 12; i++) {
    const bx = ((i * 37 - t * 20) % (w + 40) + w + 40) % (w + 40) - 40;
    const bh = 50 + ((i * 53) % 70);
    g.fillRect(bx, h - bh, 30, bh);
    g.fillStyle = '#f6c744';
    for (let k = 0; k < 4; k++) if ((i + k) % 3) g.fillRect(bx + 6 + (k % 2) * 12, h - bh + 10 + Math.floor(k / 2) * 14, 5, 6);
    g.fillStyle = '#140b26';
  }
  const cx = w / 2 + Math.sin(t * 1.3) * 70, cy = h / 2 + Math.sin(t * 2.1) * 35;
  g.strokeStyle = '#4ff3ff'; g.lineWidth = 2;
  g.beginPath(); g.arc(cx, cy, 12, 0, 7); g.moveTo(cx - 20, cy); g.lineTo(cx + 20, cy); g.moveTo(cx, cy - 20); g.lineTo(cx, cy + 20); g.stroke();
  for (let k = 0; k < 6; k++) {
    g.fillStyle = k < 6 - (Math.floor(t * 1.5) % 7) ? '#4ff3ff' : 'rgba(79,243,255,0.25)';
    g.beginPath(); g.arc(12 + k * 11, h - 10, 4, 0, 7); g.fill();
  }
});

const screenRace = screen((g, w, h, t) => {
  // pseudo-3D road, an original attract loop
  const hz = h * 0.45;
  const sky = g.createLinearGradient(0, 0, 0, hz);
  sky.addColorStop(0, '#1b4fb8'); sky.addColorStop(1, '#ff9a5c');
  g.fillStyle = sky; g.fillRect(0, 0, w, hz);
  g.fillStyle = '#3d2a6b';
  g.beginPath(); g.moveTo(0, hz);
  for (let x = 0; x <= w; x += 16) g.lineTo(x, hz - 12 - Math.abs(Math.sin(x * 0.03 + 1)) * 22);
  g.lineTo(w, hz); g.fill();
  const curve = Math.sin(t * 0.5) * 50;
  for (let y = hz; y < h; y += 2) {
    const z = (y - hz) / (h - hz);
    const half = 8 + z * w * 0.55;
    const center = w / 2 + curve * (1 - z) * (1 - z);
    const stripe = Math.floor((1 / (z + 0.05)) * 3 - t * 14) % 2 === 0;
    g.fillStyle = stripe ? '#2f9e44' : '#3fb950'; g.fillRect(0, y, w, 2);
    g.fillStyle = stripe ? '#555' : '#5f5f5f'; g.fillRect(center - half, y, half * 2, 2);
    g.fillStyle = stripe ? '#e0393e' : '#fff'; g.fillRect(center - half - 6 * z - 2, y, 6 * z + 2, 2); g.fillRect(center + half, y, 6 * z + 2, 2);
    if (stripe) { g.fillStyle = '#fff'; g.fillRect(center - 1 - z * 2, y, 2 + z * 4, 2); }
  }
  g.fillStyle = '#e0393e'; g.fillRect(w / 2 - 22, h - 34, 44, 18);
  g.fillStyle = '#111'; g.fillRect(w / 2 - 26, h - 20, 12, 8); g.fillRect(w / 2 + 14, h - 20, 12, 8);
  g.fillStyle = '#f6c744'; g.fillRect(w / 2 - 40, 8, 80 * (1 - (t % 20) / 20), 6);
});

const screenHub = screen((g, w, h, t) => {
  g.fillStyle = '#15111f'; g.fillRect(0, 0, w, h);
  g.fillStyle = '#221b33'; g.fillRect(0, 0, w, 20);
  g.fillStyle = '#e2b955'; g.fillRect(8, 8, 40, 5);
  const cols = [PAL.red, PAL.teal, PAL.purple, PAL.orange, PAL.blue, PAL.magenta, PAL.yellow, PAL.lime];
  const sel = Math.floor(t * 0.8) % 8;
  for (let i = 0; i < 8; i++) {
    const x = 8 + (i % 4) * 61, y = 28 + Math.floor(i / 4) * 80;
    g.fillStyle = cols[i]; g.fillRect(x, y, 54, 58);
    g.fillStyle = 'rgba(0,0,0,0.35)'; g.fillRect(x, y + 44, 54, 14);
    g.fillStyle = i % 3 === 0 ? '#9bdc3c' : '#e2b955'; g.fillRect(x + 4, y + 49, 6, 4);
    g.fillStyle = '#fff'; g.fillRect(x + 13, y + 49, 30, 4);
    if (i === sel) { g.strokeStyle = '#4ff3ff'; g.lineWidth = 3; g.strokeRect(x - 2, y - 2, 58, 62); }
  }
  g.fillStyle = sel % 3 === 0 ? '#9bdc3c' : '#e2b955';
  g.fillRect(w / 2 - 50, h - 11, 100 * (sel % 3 === 0 ? 1 : 0.5 + 0.5 * Math.sin(t * 2) ** 2), 5);
});

const screenAttract = screen((g, w, h, t) => {
  g.fillStyle = '#05030d'; g.fillRect(0, 0, w, h);
  for (let i = 0; i < 60; i++) {
    const z = ((i * 97 + t * 40) % 200) / 200;
    const a = i * 2.39996;
    const x = w / 2 + Math.cos(a) * z * w * 0.7, y = h / 2 + Math.sin(a) * z * h * 0.7;
    g.fillStyle = `rgba(255,255,255,${z})`; g.fillRect(x, y, 1 + z * 2, 1 + z * 2);
  }
  for (let i = 0; i < 3; i++) {
    const a = t * (0.8 + i * 0.3) + i * 2;
    g.fillStyle = [PAL.yellow, PAL.magenta, PAL.teal][i];
    g.save(); g.translate(w / 2 + Math.cos(a) * 50, h / 2 + Math.sin(a * 1.3) * 40); g.rotate(a);
    g.beginPath(); g.moveTo(0, -16); g.lineTo(14, 10); g.lineTo(-14, 10); g.fill(); g.restore();
  }
});

const screenCorridor = screen((g, w, h, t) => {
  // one-point corridor with targets popping out of doors
  g.fillStyle = '#0d0a14'; g.fillRect(0, 0, w, h);
  const vx = w / 2 + Math.sin(t * 0.4) * 10, vy = h * 0.46;
  for (let i = 8; i > 0; i--) {
    const k = ((i - (t * 1.2) % 1) / 8);
    const hw = k * w * 0.62, hh = k * h * 0.62;
    g.strokeStyle = `rgba(155,140,200,${0.15 + k * 0.6})`; g.lineWidth = 1 + k * 2;
    g.strokeRect(vx - hw, vy - hh, hw * 2, hh * 2);
  }
  g.strokeStyle = 'rgba(155,140,200,0.5)'; g.lineWidth = 1;
  for (const [x, y] of [[0, 0], [w, 0], [0, h], [w, h]]) { g.beginPath(); g.moveTo(vx, vy); g.lineTo(x, y); g.stroke(); }
  for (let i = 0; i < 3; i++) {
    const ph = (t * 0.7 + i * 0.33) % 1;
    if (ph > 0.75) continue;
    const s = 10 + ph * 34, x = vx + (i - 1) * (20 + ph * 70), y = vy + ph * 30;
    g.fillStyle = [PAL.red, PAL.lime, PAL.orange][i];
    g.beginPath(); g.arc(x, y - s * 0.6, s * 0.35, 0, 7); g.fill();
    g.fillRect(x - s * 0.3, y - s * 0.3, s * 0.6, s);
  }
  const cx = w / 2 + Math.sin(t * 1.7) * 60, cy = h / 2 + Math.cos(t * 1.1) * 30;
  g.strokeStyle = '#4ff3ff'; g.lineWidth = 2;
  g.beginPath(); g.arc(cx, cy, 10, 0, 7); g.moveTo(cx - 16, cy); g.lineTo(cx + 16, cy); g.moveTo(cx, cy - 16); g.lineTo(cx, cy + 16); g.stroke();
});

const screenBoard = screen((g, w, h, t) => {
  // roadmap as a high-score table (docs/roadmap.md, status from README)
  g.fillStyle = '#05030d'; g.fillRect(0, 0, w, h);
  pix(g, 22);
  const hue = (t * 40) % 360;
  g.fillStyle = `hsl(${hue},90%,65%)`; g.fillText('ROADMAP', w / 2, 34);
  const rows = [
    ['M0', 'PLAY NOW', 'READY', '#9bdc3c'],
    ['M1', 'HUB + FLAT LAUNCH', 'BUILDS', '#9bdc3c'],
    ['M2', 'FIRST TRUE 3D', 'NOW', '#f6c744'],
    ['M3', 'THEATRE + INPUT', '', '#8b86a0'],
    ['M4', 'SUPERMODEL-VR', '', '#8b86a0'],
    ['M5', 'SPRITE DEPTH', '', '#8b86a0'],
    ['M6', 'PS2 TRUE 3D', '', '#8b86a0'],
    ['M7', 'STANDALONE', '', '#8b86a0'],
  ];
  pix(g, 11);
  rows.forEach(([m, name, st, col], i) => {
    const y = 76 + i * 23;
    g.textAlign = 'left'; g.fillStyle = '#4ff3ff'; g.fillText(m, 22, y);
    g.fillStyle = col; g.fillText(name, 70, y);
    g.textAlign = 'right';
    if (st && !(st === 'NOW' && Math.floor(t * 2) % 2)) g.fillText(st, w - 22, y);
  });
}, 512, 280);

// ---------------------------------------------------------------- lighting
scene.add(new THREE.HemisphereLight('#bfb6ff', '#2a1a2e', 0.35));
const mallLight = new THREE.PointLight('#fff1d8', 7, 26, 1.4); mallLight.position.set(0, 3.3, 7); scene.add(mallLight);
const mallLight2 = new THREE.PointLight('#fff1d8', 5, 20, 1.4); mallLight2.position.set(0, 3.3, 15); scene.add(mallLight2);
[[-3.5, -5, '#ff5fb8'], [3.5, -11, '#4ff3ff'], [-2.5, -17, '#ffb45c'], [3, -22, '#b98cff'], [0, -2, '#fff0d0']].forEach(([x, z, col]) => {
  const l = new THREE.PointLight(col, 14, 11, 1.4); l.position.set(x, 3.0, z); scene.add(l);
});

// ---------------------------------------------------------------- mall corridor (z > 0)
const ROOM = { x: 8, depth: 26, h: 3.4 };
const floorMall = plane(28, 20, std('#b9b2a4', { map: mallTileTex(), roughness: 0.45, envMap: envTex, envMapIntensity: 0.15 }), 0, 0, 10);
floorMall.rotation.x = -Math.PI / 2;
// teal stripe bands across the tile, like the mall floors in the photos
for (const z of [1.8, 9.5, 17]) {
  const band = plane(28, 0.35, std('#2e7a72', { roughness: 0.35 }), 0, 0.002, z); band.rotation.x = -Math.PI / 2;
}
// threshold: dark band with light stripes
for (let i = 0; i < 3; i++) {
  const b = plane(6.4, 0.22, std(i % 2 ? '#e9e4d6' : '#1d4b46', { roughness: 0.4 }), 0, 0.003, 0.35 + i * 0.22);
  b.rotation.x = -Math.PI / 2;
}
const ceilMall = plane(28, 20, std('#ffffff', { map: ceilingTex() }), 0, 4.6, 10); ceilMall.rotation.x = Math.PI / 2;
for (const [x, z] of [[-4, 6], [4, 6], [-4, 12], [4, 12], [0, 3], [0, 9], [0, 15]]) {
  const p = plane(1.2, 1.2, new THREE.MeshBasicMaterial({ color: '#d9d2c2' }), x, 4.58, z); p.rotation.x = Math.PI / 2;
}

// ---------------------------------------------------------------- storefront (z = 0)
const upperWall = std('#56706b', { roughness: 0.8 });
box(28, 1.6, 0.4, upperWall, 0, 3.8, 0.0);                  // band above the shop
box(10, 0.9, 0.3, std('#405652'), -9, 4.15, 0.25);          // neighbours' fascia
box(10, 0.9, 0.3, std('#405652'), 9, 4.15, 0.25);
// red sign frame + panel
box(10.2, 1.7, 0.35, std('#b81f35', { roughness: 0.45, envMap: envTex, envMapIntensity: 0.4 }), 0, 3.85, 0.38);
const sign = makeSign();
const signMesh = plane(9.5, 1.67, new THREE.MeshBasicMaterial({ color: '#ffffff', map: sign.texture, toneMapped: false }), 0, 3.85, 0.57);
markClickable(signMesh, 0);
// red header over the opening
box(7.2, 0.25, 0.5, std('#b81f35'), 0, 2.95, 0.15);
// checker pillars
for (const x of [-3.6, 3.6, -7.9, 7.9]) {
  const pt = checkerTex(4, 14);
  const mat = std('#ffffff', { map: pt, roughness: 0.3 });
  box(0.8, 2.95, 0.8, mat, x, 1.475, 0.2);
}
// display windows with checker kick plates
for (const side of [-1, 1]) {
  const cx = side * 5.75;
  const kick = checkerTex(14, 2);
  box(3.5, 0.5, 0.3, std('#ffffff', { map: kick, roughness: 0.3 }), cx, 0.25, 0.25);
  const glass = new THREE.MeshPhysicalMaterial({ color: '#9fd6ff', transparent: true, opacity: 0.12, roughness: 0.05, metalness: 0, envMap: envTex, envMapIntensity: 0.9 });
  plane(3.5, 2.45, glass, cx, 1.725, 0.3);
  box(3.5, 0.05, 0.08, std('#b9b9b9', { metalness: 0.8, roughness: 0.3 }), cx, 2.95, 0.3);
}
// neighbouring shops, dim
for (const side of [-1, 1]) {
  box(5.8, 3, 0.2, std('#3a3a44'), side * 11.2, 1.5, 0.1);
  plane(4.6, 2.2, glow('#2a3c5a', 0.6), side * 11.2, 1.5, 0.21);
}
// sidewalk sandwich board by the door (original artwork)
{
  const [c, g] = makeCanvas(256, 384);
  g.fillStyle = '#141018'; g.fillRect(0, 0, 256, 384);
  pix(g, 18); g.fillStyle = '#f6c744'; g.fillText('TRUE 3D', 128, 60); g.fillText('VR', 128, 92);
  g.fillStyle = '#4ff3ff'; pix(g, 12); g.fillText('NOW BUILDING', 128, 140);
  g.drawImage(tokenFaceTex(false).image, 48, 170, 160, 160);
  g.fillStyle = '#ff4fa8'; pix(g, 10); g.fillText('OPEN SOURCE', 128, 356);
  const board = new THREE.Group();
  const face = plane(0.62, 0.95, std('#ffffff', { map: tex(c), roughness: 0.5 }), 0, 0.55, 0.01, board);
  face.rotation.x = -0.12;
  box(0.7, 1.05, 0.03, std('#222'), 0, 0.53, -0.03, board).rotation.x = -0.12;
  board.position.set(3.0, 0, 1.4); board.rotation.y = -0.35;
  scene.add(board);
}

// ---------------------------------------------------------------- arcade room (z < 0)
const carpet = plane(ROOM.x * 2, ROOM.depth, std('#ffffff', { map: carpetTex(), roughness: 0.95 }), 0, 0, -ROOM.depth / 2);
carpet.rotation.x = -Math.PI / 2;
const ceil = plane(ROOM.x * 2, ROOM.depth, std('#5a556a', { map: ceilingTex(), roughness: 0.95 }), 0, ROOM.h, -ROOM.depth / 2);
ceil.rotation.x = Math.PI / 2;
for (let z = -3; z > -ROOM.depth; z -= 4) for (const x of [-4.5, 0, 4.5]) {
  const p = plane(1.1, 0.5, new THREE.MeshBasicMaterial({ color: '#ffe9c9' }), x, ROOM.h - 0.01, z); p.rotation.x = Math.PI / 2;
}
const lowerWall = std('#b3283c', { roughness: 0.7 });
const mural = std('#ffffff', { map: muralTex(), roughness: 0.8 });
function wall(len, x, z, rotY) {
  const g = new THREE.Group();
  box(len, 1.25, 0.1, lowerWall, 0, 0.625, 0, g);
  const m = plane(len, 1.1, mural, 0, 1.8, 0.06, g);
  m.material = mural.clone(); m.material.map = mural.map.clone(); m.material.map.repeat.set(len / 5, 1); m.material.map.needsUpdate = true;
  box(len, ROOM.h - 2.35, 0.1, std('#2c1f45'), 0, 2.35 + (ROOM.h - 2.35) / 2, 0, g);
  // cyan neon strip along the top, as in the photos
  box(len, 0.05, 0.05, glow('#4ff3ff', 1.6), 0, ROOM.h - 0.25, 0.08, g);
  g.position.set(x, 0, z); g.rotation.y = rotY;
  scene.add(g);
}
wall(ROOM.depth, -ROOM.x, -ROOM.depth / 2, Math.PI / 2);
wall(ROOM.depth, ROOM.x, -ROOM.depth / 2, -Math.PI / 2);
wall(ROOM.x * 2, 0, -ROOM.depth, 0);

// ---------------------------------------------------------------- cabinets
function cabinetBody(group, w, h, d, color) {
  const side = std(color, { roughness: 0.45, envMap: envTex, envMapIntensity: 0.35 });
  const black = std('#1d1a27', { roughness: 0.5 });
  box(0.05, h, d, side, -w / 2 + 0.025, h / 2, 0, group);
  box(0.05, h, d, side, w / 2 - 0.025, h / 2, 0, group);
  box(w - 0.1, h, d - 0.04, black, 0, h / 2, -0.02, group);
  // T-molding glow
  box(0.012, h, 0.012, glow(color, 0.8), -w / 2, h / 2, d / 2, group);
  box(0.012, h, 0.012, glow(color, 0.8), w / 2, h / 2, d / 2, group);
  // coin door with two lit slots
  box(0.34, 0.42, 0.02, std('#2a2a30', { metalness: 0.6, roughness: 0.4 }), 0, 0.52, d / 2, group);
  box(0.05, 0.08, 0.01, glow('#ff7b2e', 2.2), -0.07, 0.6, d / 2 + 0.015, group);
  box(0.05, 0.08, 0.01, glow('#ff7b2e', 2.2), 0.07, 0.6, d / 2 + 0.015, group);
}

function upright({ x, z, rotY = 0, color, marquee, sub, screenTex, w = 0.8, h = 1.95, d = 0.85, guns = false, stop }) {
  const g = new THREE.Group();
  cabinetBody(g, w, h, d, color);
  // screen, tilted back
  const sw = w - 0.2, sh = sw * 0.78;
  const bezel = box(w - 0.1, sh + 0.12, 0.02, std('#050507'), 0, 1.38, d / 2 + 0.04, g);
  bezel.rotation.x = -0.18;
  const scr = plane(sw, sh, basicMat(screenTex), 0, 1.38, d / 2 + 0.052, g);
  scr.rotation.x = -0.18;
  // control panel
  const cp = box(w - 0.06, 0.06, 0.32, std('#1c1924'), 0, 1.0, d / 2 + 0.1, g);
  cp.rotation.x = 0.35;
  if (!guns) {
    const stick = new THREE.Mesh(new THREE.CylinderGeometry(0.008, 0.008, 0.08), std('#111'));
    stick.position.set(-0.15, 1.07, d / 2 + 0.1); g.add(stick);
    const ball = new THREE.Mesh(new THREE.SphereGeometry(0.025, 12, 8), std(PAL.red, { roughness: 0.3 }));
    ball.position.set(-0.15, 1.11, d / 2 + 0.1); g.add(ball);
    for (let i = 0; i < 3; i++) {
      const b = new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.02, 0.015, 16), glow([PAL.yellow, PAL.teal, PAL.magenta][i], 1.5));
      b.position.set(0.02 + i * 0.07, 1.05, d / 2 + 0.11); b.rotation.x = 0.35; g.add(b);
    }
  } else {
    // two holstered lookalike pistols and a floor pedal (generic shapes, no real peripheral copied)
    for (const [sx, col] of [[-1, PAL.blue], [1, PAL.red]]) {
      const gun = new THREE.Group();
      box(0.05, 0.13, 0.05, std(col, { roughness: 0.35 }), 0, 0, 0, gun).rotation.x = 0.3;
      box(0.05, 0.06, 0.2, std(col, { roughness: 0.35 }), 0, 0.07, 0.07, gun);
      box(0.02, 0.02, 0.02, glow('#ff3030', 2), 0, 0.07, 0.175, gun);
      gun.position.set(sx * (w / 2 - 0.12), 1.12, d / 2 + 0.08);
      gun.rotation.y = sx * 0.4;
      g.add(gun);
    }
    box(0.36, 0.06, 0.3, std('#26222c', { metalness: 0.5 }), 0, 0.03, d / 2 + 0.45, g);
    box(0.28, 0.02, 0.22, std('#ffcc33'), 0, 0.07, d / 2 + 0.45, g).rotation.x = -0.15;
  }
  // lit marquee
  box(w, 0.3, 0.3, std('#1d1a27'), 0, h + 0.15, d / 2 - 0.17, g);
  plane(w - 0.04, 0.26, new THREE.MeshBasicMaterial({ map: marqueeTex(marquee, color, '#fff4c2', sub), toneMapped: false }), 0, h + 0.15, d / 2 - 0.015, g);
  g.position.set(x, 0, z); g.rotation.y = rotY;
  scene.add(g);
  if (stop !== undefined) markClickable(g, stop);
  return g;
}

function sitdown({ x, z, rotY = 0, color, marquee, sub, screenTex, ghost = false, stop }) {
  const g = new THREE.Group();
  const side = std(color, { roughness: 0.4, envMap: envTex, envMapIntensity: 0.35 });
  box(1.0, 0.14, 2.0, std('#1b1820'), 0, 0.07, 0.4, g);                   // base
  box(1.0, 1.35, 0.7, side, 0, 0.82, -0.35, g);                            // front tower
  const bz = box(0.86, 0.66, 0.02, std('#050507'), 0, 1.12, 0.07, g); bz.rotation.x = -0.22;
  const scr = plane(0.76, 0.56, basicMat(screenTex), 0, 1.12, 0.082, g); scr.rotation.x = -0.22;
  box(1.0, 0.28, 0.5, std('#1d1a27'), 0, 1.64, -0.4, g);
  plane(0.96, 0.24, new THREE.MeshBasicMaterial({ map: marqueeTex(marquee, color, '#fff4c2', sub), toneMapped: false }), 0, 1.64, -0.149, g);
  // dash, wheel, shifter
  box(1.0, 0.18, 0.3, side, 0, 0.62, 0.15, g).rotation.x = 0.4;
  const column = new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.02, 0.25), std('#222'));
  column.rotation.x = Math.PI / 2 - 0.5; column.position.set(0, 0.74, 0.32); g.add(column);
  const wheel = new THREE.Mesh(new THREE.TorusGeometry(0.17, 0.022, 10, 32), std('#191919', { roughness: 0.4 }));
  wheel.position.set(0, 0.8, 0.43); wheel.rotation.x = -0.5; g.add(wheel);
  const hub = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.05, 0.03, 16), std(PAL.red));
  hub.rotation.x = Math.PI / 2 - 0.5; hub.position.copy(wheel.position); g.add(hub);
  const shifter = new THREE.Mesh(new THREE.CylinderGeometry(0.012, 0.012, 0.2), std('#aaa', { metalness: 0.8, roughness: 0.3 }));
  shifter.position.set(0.32, 0.6, 0.6); g.add(shifter);
  const knob = new THREE.Mesh(new THREE.SphereGeometry(0.035, 12, 8), std('#111')); knob.position.set(0.32, 0.71, 0.6); g.add(knob);
  // seat
  box(0.55, 0.12, 0.5, std('#1b1b22', { roughness: 0.8 }), 0, 0.45, 1.05, g);
  box(0.55, 0.65, 0.12, std('#1b1b22', { roughness: 0.8 }), 0, 0.8, 1.32, g).rotation.x = -0.15;
  box(0.6, 0.3, 0.55, side, 0, 0.27, 1.05, g);
  // under-glow
  box(0.98, 0.02, 1.98, glow(color, 1.2), 0, 0.005, 0.4, g);
  if (ghost) {
    // ghost controls: see-through wheel and lever hovering where your hands go
    const gm = new THREE.MeshBasicMaterial({ color: new THREE.Color('#7ff8ff').multiplyScalar(2.2), transparent: true, opacity: 0.55, blending: THREE.AdditiveBlending, depthWrite: false });
    const gw = new THREE.Mesh(new THREE.TorusGeometry(0.2, 0.018, 8, 40), gm);
    gw.position.set(0, 0.9, 0.62); gw.rotation.x = -1.05; g.add(gw);
    const spokes = new THREE.Mesh(new THREE.BoxGeometry(0.38, 0.015, 0.015), gm); spokes.position.copy(gw.position); spokes.rotation.x = -1.05; g.add(spokes);
    const gl = new THREE.Mesh(new THREE.CylinderGeometry(0.012, 0.012, 0.22), gm); gl.position.set(0.36, 0.72, 0.8); gl.rotation.x = 0.35; g.add(gl);
    const gk = new THREE.Mesh(new THREE.SphereGeometry(0.04, 12, 8), gm); gk.position.set(0.36, 0.82, 0.84); g.add(gk);
    animated.push(t => {
      gm.opacity = reducedMotion ? 0.55 : 0.55 + Math.sin(t * 2.4) * 0.1;
      gw.rotation.z = Math.sin(t * 0.9) * 0.35; spokes.rotation.z = gw.rotation.z;
      const bob = reducedMotion ? 0 : Math.sin(t * 1.2) * 0.02;
      gw.position.y = spokes.position.y = 0.9 + bob;
    });
  }
  g.position.set(x, 0, z); g.rotation.y = rotY;
  scene.add(g);
  if (stop !== undefined) markClickable(g, stop);
  return g;
}

function claw(x, z, color, seed, rotY = 0) {
  const g = new THREE.Group();
  const r = rng(seed);
  const side = std(color, { roughness: 0.4, envMap: envTex, envMapIntensity: 0.3 });
  box(0.95, 0.9, 0.95, side, 0, 0.45, 0, g);
  box(0.5, 0.06, 0.08, std('#151318'), 0, 0.95, 0.5, g);
  const glass = new THREE.MeshStandardMaterial({ color: '#cfefff', transparent: true, opacity: 0.12, roughness: 0.05, envMap: envTex, envMapIntensity: 1, depthWrite: false });
  box(0.93, 1.0, 0.93, glass, 0, 1.4, 0, g);
  const plushCols = [PAL.yellow, PAL.magenta, PAL.lime, '#ffffff', PAL.blue, PAL.orange, '#b98cff'];
  const geo = new THREE.IcosahedronGeometry(0.09, 1);
  for (let i = 0; i < 26; i++) {
    const m = new THREE.Mesh(geo, std(plushCols[Math.floor(r() * plushCols.length)], { roughness: 0.95 }));
    m.position.set((r() - 0.5) * 0.75, 0.97 + r() * 0.22, (r() - 0.5) * 0.75);
    m.scale.setScalar(0.7 + r() * 0.6);
    g.add(m);
  }
  box(0.95, 0.32, 0.95, std('#151318'), 0, 2.06, 0, g);
  plane(0.9, 0.26, new THREE.MeshBasicMaterial({ map: marqueeTex('PRIZES', color), toneMapped: false }), 0, 2.06, 0.477, g);
  box(0.9, 0.03, 0.9, glow('#fff3d6', 1.6), 0, 1.89, 0, g);
  const clawG = new THREE.Group();
  const rod = new THREE.Mesh(new THREE.CylinderGeometry(0.006, 0.006, 0.4), std('#ccc', { metalness: 0.9, roughness: 0.2 }));
  rod.position.y = 0.2; clawG.add(rod);
  for (let k = 0; k < 3; k++) {
    const f = box(0.012, 0.12, 0.012, std('#ddd', { metalness: 0.9, roughness: 0.2 }), Math.cos(k * 2.09) * 0.04, -0.04, Math.sin(k * 2.09) * 0.04, clawG);
    f.rotation.z = Math.cos(k * 2.09) * 0.4; f.rotation.x = -Math.sin(k * 2.09) * 0.4;
  }
  clawG.position.set(0, 1.45, 0); clawG.userData.dynamic = true; g.add(clawG);
  const ph = r() * 6;
  animated.push(t => {
    if (reducedMotion) return;
    clawG.position.x = Math.sin(t * 0.5 + ph) * 0.28;
    clawG.position.z = Math.cos(t * 0.37 + ph) * 0.28;
  });
  g.position.set(x, 0, z); g.rotation.y = rotY;
  scene.add(g);
  return g;
}

function wheelGame(x, z, rotY, seed) {
  // ticket wheel: a lit disc that spins
  const g = new THREE.Group();
  cabinetBody(g, 0.9, 1.0, 0.7, PAL.purple);
  const [c, ctx] = makeCanvas(512, 512);
  const segs = 16;
  for (let i = 0; i < segs; i++) {
    ctx.fillStyle = muralColors[(i + seed) % muralColors.length];
    ctx.beginPath(); ctx.moveTo(256, 256); ctx.arc(256, 256, 250, (i / segs) * Math.PI * 2, ((i + 1) / segs) * Math.PI * 2); ctx.fill();
    ctx.save(); ctx.translate(256, 256); ctx.rotate(((i + 0.5) / segs) * Math.PI * 2);
    pix(ctx, 18); ctx.fillStyle = '#fff'; ctx.fillText(String([5, 10, 25, 50, 100][i % 5]), 170, 0); ctx.restore();
  }
  ctx.fillStyle = '#1a0f2e'; ctx.beginPath(); ctx.arc(256, 256, 50, 0, 7); ctx.fill();
  const disc = new THREE.Mesh(new THREE.CircleGeometry(0.42, 48), new THREE.MeshBasicMaterial({ map: tex(c), toneMapped: false, color: '#d8d8d8' }));
  disc.position.set(0, 1.55, 0.2); disc.userData.dynamic = true; g.add(disc);
  const ring = new THREE.Mesh(new THREE.TorusGeometry(0.45, 0.025, 8, 48), glow('#ffd34f', 2.5)); ring.position.copy(disc.position); g.add(ring);
  box(0.06, 0.1, 0.03, glow('#ffffff', 2), 0, 2.02, 0.22, g);
  box(0.9, 0.9, 0.2, std('#191522'), 0, 1.55, 0.05, g);
  animated.push((t, dt) => { disc.rotation.z -= dt * (reducedMotion ? 0.2 : 0.9 + Math.sin(t * 0.3 + seed) * 0.6); });
  g.position.set(x, 0, z); g.rotation.y = rotY;
  scene.add(g);
}

function hoops(x, z, rotY) {
  const g = new THREE.Group();
  box(1.1, 0.8, 2.4, std(PAL.blue, { roughness: 0.4 }), 0, 0.4, 0, g);
  box(1.1, 0.04, 2.3, std('#c98a4b', { roughness: 0.5 }), 0, 0.82, 0.02, g).rotation.x = 0.12;
  box(1.1, 1.6, 0.1, std('#151318'), 0, 1.6, -1.15, g);
  const net = new THREE.Mesh(new THREE.TorusGeometry(0.16, 0.012, 8, 32), glow(PAL.orange, 2.5));
  net.rotation.x = Math.PI / 2; net.position.set(0, 1.55, -0.95); g.add(net);
  plane(0.9, 0.5, new THREE.MeshBasicMaterial({ map: marqueeTex('HOOPS', PAL.orange, '#fff4c2', 'TICKETS'), toneMapped: false }), 0, 2.15, -1.09, g);
  const ball = new THREE.Mesh(new THREE.SphereGeometry(0.09, 16, 12), std(PAL.orange, { roughness: 0.8 }));
  ball.position.set(0.25, 0.9, 0.9); g.add(ball);
  g.position.set(x, 0, z); g.rotation.y = rotY;
  scene.add(g);
}

// entrance: claw machines and a token changer
claw(-6.6, -1.4, PAL.magenta, 3);
claw(-5.5, -1.4, PAL.teal, 9);
claw(5.5, -1.4, PAL.yellow, 21);
claw(6.6, -1.4, PAL.blue, 33);

// token changer + floating token
{
  const g = new THREE.Group();
  box(0.8, 1.8, 0.6, std('#2b2533', { metalness: 0.4, roughness: 0.4, envMap: envTex, envMapIntensity: 0.5 }), 0, 0.9, 0, g);
  plane(0.7, 0.36, new THREE.MeshBasicMaterial({ map: marqueeTex('TOKENS', PAL.red, '#ffe9a8', 'CHANGE'), toneMapped: false }), 0, 1.55, 0.301, g);
  box(0.3, 0.12, 0.05, glow('#4ff3ff', 1.6), 0, 1.12, 0.31, g);
  box(0.24, 0.1, 0.12, std('#888', { metalness: 0.9, roughness: 0.25, envMap: envTex }), 0, 0.55, 0.33, g);
  g.position.set(-2.3, 0, -3.1); g.rotation.y = 0.55;
  scene.add(g);
  markClickable(g, 1);
}
const brass = new THREE.MeshStandardMaterial({ color: '#c99c3e', metalness: 1, roughness: 0.38, envMap: envTex, envMapIntensity: 0.7 });
const token = new THREE.Mesh(new THREE.CylinderGeometry(0.32, 0.32, 0.045, 64), [
  brass,
  new THREE.MeshStandardMaterial({ map: tokenFaceTex(true), metalness: 0.85, roughness: 0.4, envMap: envTex, envMapIntensity: 0.6 }),
  new THREE.MeshStandardMaterial({ map: tokenFaceTex(false), metalness: 0.85, roughness: 0.4, envMap: envTex, envMapIntensity: 0.6 }),
]);
const tokenPivot = new THREE.Group();
token.rotation.x = Math.PI / 2;
tokenPivot.add(token);
tokenPivot.position.set(-2.1, 2.3, -2.8);
tokenPivot.userData.dynamic = true;
scene.add(tokenPivot);
markClickable(tokenPivot, 1);
animated.push((t, dt) => {
  tokenPivot.rotation.y += dt * (reducedMotion ? 0.3 : 1.1);
  if (!reducedMotion) tokenPivot.position.y = 2.3 + Math.sin(t * 1.5) * 0.05;
});

// light gun row (left wall, facing +x)
upright({ x: -7.2, z: -5.0, rotY: Math.PI / 2, color: PAL.blue, marquee: 'LIGHT GUNTRUE 3D', screenTex: screenGun, w: 1.15, h: 2.15, d: 1.0, guns: true, stop: 2 });
upright({ x: -7.2, z: -6.5, rotY: Math.PI / 2, color: PAL.red, marquee: 'TAKE COVER2 PLAYER', screenTex: screenGun, w: 1.15, h: 2.15, d: 1.0, guns: true, stop: 2 });
upright({ x: -7.2, z: -8.0, rotY: Math.PI / 2, color: PAL.teal, marquee: 'RELOADPOINT OFF SCREEN', screenTex: screenGun, w: 1.15, h: 2.15, d: 1.0, guns: true, stop: 2 });
// center island of classic uprights, back to back
for (let i = 0; i < 3; i++) {
  upright({ x: -1.4, z: -6.0 - i * 0.85, rotY: -Math.PI / 2, color: [PAL.purple, PAL.orange, PAL.magenta][i], marquee: ['INSERT TOKEN', 'ATTRACT', 'HI SCORE'][i], screenTex: screenAttract });
  upright({ x: -0.55, z: -6.0 - i * 0.85, rotY: Math.PI / 2, color: [PAL.teal, PAL.blue, PAL.red][i], marquee: ['PLAYER 1', 'CONTINUE?', 'GAME OVER'][i], screenTex: [screenAttract, screenRace, screenGun][i] });
}

// racing row (right wall, facing -x)
sitdown({ x: 6.6, z: -9.6, rotY: -Math.PI / 2, color: PAL.red, marquee: 'GHOST WHEELGRAB TO DRIVE', screenTex: screenRace, ghost: true, stop: 3 });
sitdown({ x: 6.6, z: -11.0, rotY: -Math.PI / 2, color: PAL.yellow, marquee: 'TWIN RACERTRIGGER = PEDAL', screenTex: screenRace, ghost: true, stop: 3 });
sitdown({ x: 6.6, z: -12.4, rotY: -Math.PI / 2, color: PAL.blue, marquee: 'TWIN RACERLINK PLAY', screenTex: screenRace, stop: 3 });

// ticket games along the right wall near the front
wheelGame(7.3, -4.4, -Math.PI / 2, 1);
wheelGame(7.3, -5.6, -Math.PI / 2, 4);
hoops(6.8, -7.4, -Math.PI / 2 + 0.0001);

// the Hub cabinet, back left
upright({ x: -4.2, z: -18.6, rotY: 0.45, color: '#e2b955', marquee: 'THE HUBLIBRARY', screenTex: screenHub, w: 1.0, h: 2.05, d: 0.9, stop: 8 });
upright({ x: -5.5, z: -18.0, rotY: 0.6, color: PAL.purple, marquee: 'ONE CLICKINSTALL', screenTex: screenHub, stop: 8 });
upright({ x: -2.9, z: -19.0, rotY: 0.3, color: PAL.teal, marquee: 'THEATREMODE', screenTex: screenAttract, stop: 8 });

// prize counter, back right
{
  const g = new THREE.Group();
  const glass = new THREE.MeshStandardMaterial({ color: '#d8f3ff', transparent: true, opacity: 0.16, roughness: 0.05, envMap: envTex, envMapIntensity: 1.2, depthWrite: false });
  for (let i = 0; i < 3; i++) {
    const cx = (i - 1) * 1.25;
    box(1.2, 0.5, 0.7, std(PAL.purple, { roughness: 0.5 }), cx, 0.25, 0, g);
    box(1.2, 0.55, 0.7, glass, cx, 0.78, 0, g);
    box(1.18, 0.02, 0.68, glow('#fff0d0', 0.5), cx, 1.04, 0, g);
    const r = rng(100 + i);
    for (let k = 0; k < 8; k++) {
      const m = new THREE.Mesh(k % 2 ? new THREE.BoxGeometry(0.1, 0.1, 0.1) : new THREE.SphereGeometry(0.06, 12, 8),
        std(muralColors[Math.floor(r() * muralColors.length)], { roughness: 0.4 }));
      m.position.set(cx + (r() - 0.5) * 0.9, 0.58, (r() - 0.5) * 0.4); m.rotation.y = r() * 3; g.add(m);
    }
  }
  // back shelf of plush and files ("everything is a file")
  box(4.2, 2.2, 0.4, std('#2c1f45'), 0, 1.1, -1.5, g);
  for (let s = 0; s < 3; s++) {
    box(4.0, 0.04, 0.35, std('#d9cfc0'), 0, 0.7 + s * 0.6, -1.3, g);
    const r = rng(200 + s);
    for (let k = 0; k < 9; k++) {
      const isFile = (k + s) % 3 === 0;
      const m = isFile
        ? new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.24, 0.03), std('#f4ecdc', { roughness: 0.7 }))
        : new THREE.Mesh(new THREE.IcosahedronGeometry(0.12, 1), std(muralColors[Math.floor(r() * muralColors.length)], { roughness: 0.95 }));
      m.position.set(-1.8 + k * 0.45, 0.84 + s * 0.6, -1.3); if (isFile) m.rotation.y = 0.3;
      g.add(m);
    }
  }
  plane(2.6, 0.55, new THREE.MeshBasicMaterial({ map: marqueeTex('prize-sign', PAL.magenta), toneMapped: false }), 0, 2.55, -1.28, g);
  g.position.set(4.2, 0, -21.5); g.rotation.y = -0.35;
  scene.add(g);
  markClickable(g, 7);
}

// scoreboard on the back wall
{
  const board = plane(4.2, 2.3, new THREE.MeshBasicMaterial({ map: screenBoard, toneMapped: false }), -0.4, 2.0, -25.9);
  box(4.5, 2.6, 0.1, std('#1d1a27'), -0.4, 2.0, -25.97);
  box(4.5, 0.05, 0.05, glow('#ff4fa8', 3), -0.4, 3.32, -25.9);
  box(4.5, 0.05, 0.05, glow('#ff4fa8', 3), -0.4, 0.68, -25.9);
  markClickable(board, 9);
}
// filler uprights along the back wall
[[-6.8, PAL.orange], [-5.9, PAL.teal], [3.0, PAL.red], [3.9, PAL.blue]].forEach(([x, col], i) =>
  upright({ x, z: -25.3, rotY: 0, color: col, marquee: ['BONUS', 'EXTRA LIFE', 'STAGE 2', 'NEW RECORD'][i], screenTex: [screenRace, screenAttract, screenGun, screenAttract][i] }));

// ---------------------------------------------------------------- motion controls and gun models
// Lookalike silhouettes only (docs/guns.md §2.2): no logos, no copied shapes. Forward is -z, grip at the origin.
const additive = (color, opacity) => new THREE.MeshBasicMaterial({ color, transparent: true, opacity, blending: THREE.AdditiveBlending, depthWrite: false, toneMapped: false });

// Side profiles, x = forward (muzzle), y = up, grip at the origin; the trigger guard is a hole.
// Each pistol is one extruded silhouette with a small bevel; moving parts stay separate meshes.
const GUN_PROFILES = {
  slide: { width: 0.036,
    outer: [[-0.035, -0.115], [-0.012, -0.115], [0.005, -0.004], [0.05, -0.004], [0.062, 0.03], [0.14, 0.03], [0.14, 0.06], [-0.045, 0.06], [-0.045, 0.036], [-0.03, 0.03]],
    hole: [[0.012, 0.003], [0.045, 0.003], [0.053, 0.025], [0.012, 0.025]] },
  twin: { width: 0.046,
    outer: [[-0.04, -0.12], [-0.012, -0.12], [0.005, -0.004], [0.05, -0.004], [0.062, 0.03], [0.09, 0.03], [0.09, 0.09], [-0.05, 0.09], [-0.05, 0.04], [-0.032, 0.03]],
    hole: [[0.012, 0.003], [0.045, 0.003], [0.053, 0.025], [0.012, 0.025]] },
  revolver: { width: 0.03,
    outer: [[-0.045, -0.105], [-0.02, -0.105], [0.0, -0.002], [0.035, -0.004], [0.046, 0.028], [0.06, 0.032], [0.06, 0.075], [-0.028, 0.08], [-0.04, 0.062], [-0.03, 0.032]],
    hole: [[0.007, 0.003], [0.031, 0.003], [0.038, 0.023], [0.007, 0.023]] },
  dpad: { width: 0.04,
    outer: [[-0.04, -0.11], [-0.015, -0.11], [0.003, -0.004], [0.05, -0.004], [0.062, 0.025], [0.17, 0.025], [0.17, 0.075], [-0.035, 0.08], [-0.046, 0.06], [-0.03, 0.03]],
    hole: [[0.01, 0.003], [0.045, 0.003], [0.053, 0.021], [0.01, 0.021]] },
  shell: { width: 0.045,
    outer: [[-0.04, -0.1], [-0.015, -0.1], [0.0, -0.004], [0.05, -0.004], [0.062, 0.02], [0.2, 0.02], [0.208, -0.08], [0.235, -0.08], [0.235, 0.02], [0.26, 0.02], [0.26, 0.07], [-0.08, 0.07], [-0.19, 0.05], [-0.19, 0.0], [-0.09, 0.012], [-0.03, 0.026]],
    hole: [[0.01, 0.003], [0.045, 0.003], [0.053, 0.016], [0.01, 0.016]] },
};
const profileGeoCache = new Map();
function gunProfileGeo(kind) {
  if (profileGeoCache.has(kind)) return profileGeoCache.get(kind);
  const { width, outer, hole } = GUN_PROFILES[kind];
  const shape = new THREE.Shape(outer.map(([x, y]) => new THREE.Vector2(x, y)));
  shape.holes.push(new THREE.Path(hole.map(([x, y]) => new THREE.Vector2(x, y))));
  const bevel = 0.003;
  const geo = new THREE.ExtrudeGeometry(shape, { depth: width - bevel * 2, bevelEnabled: true, bevelThickness: bevel, bevelSize: bevel, bevelSegments: 1, curveSegments: 4 });
  geo.translate(0, 0, -(width - bevel * 2) / 2);
  geo.rotateY(Math.PI / 2);  // shape +x becomes -z (forward), extrusion becomes width along x
  profileGeoCache.set(kind, geo);
  return geo;
}

function gunModel(kind, main, accent) {
  const g = new THREE.Group();
  const body = std(main, { roughness: 0.35, envMap: envTex, envMapIntensity: 0.4 });
  const trim = std(accent, { roughness: 0.4, envMap: envTex, envMapIntensity: 0.3, emissive: accent, emissiveIntensity: 0.05 });
  const dark = std('#2a2733', { roughness: 0.5, envMap: envTex, envMapIntensity: 0.3 });
  let slide = null, drum = null, muzzleZ = -0.2, muzzleY = 0.07;
  g.add(new THREE.Mesh(gunProfileGeo(kind), body));
  const trigger = box(0.008, 0.02, 0.008, dark, 0, 0.012, -0.022, g);
  trigger.rotation.x = -0.3;
  if (kind === 'slide') {            // cabinet pistol, slide travels back on a shot
    slide = box(0.04, 0.03, 0.17, trim, 0, 0.076, -0.06, g);
    muzzleZ = -0.15; muzzleY = 0.075;
  } else if (kind === 'twin') {      // chunky cabinet pistol with a round barrel
    const bar = new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.023, 0.13, 16), body);
    bar.rotation.x = Math.PI / 2; bar.position.set(0, 0.065, -0.155); g.add(bar);
    slide = box(0.03, 0.012, 0.1, trim, 0, 0.098, -0.02, g);
    muzzleZ = -0.23; muzzleY = 0.065;
  } else if (kind === 'revolver') {  // long-barrel revolver: hammer falls, cylinder turns
    drum = new THREE.Mesh(new THREE.CylinderGeometry(0.026, 0.026, 0.045, 12), trim);
    drum.rotation.x = Math.PI / 2; drum.position.set(0, 0.056, -0.045); g.add(drum);
    const bar = new THREE.Mesh(new THREE.CylinderGeometry(0.009, 0.009, 0.18, 12), body);
    bar.rotation.x = Math.PI / 2; bar.position.set(0, 0.068, -0.15); g.add(bar);
    slide = box(0.012, 0.03, 0.015, dark, 0, 0.088, 0.03, g);
    muzzleZ = -0.24; muzzleY = 0.068;
  } else if (kind === 'dpad') {      // console pistol with a pad at the rear
    box(0.042, 0.016, 0.06, trim, 0, 0.083, 0.0, g);
    const pad = new THREE.Mesh(new THREE.CylinderGeometry(0.012, 0.012, 0.006, 16), trim);
    pad.rotation.x = Math.PI / 2; pad.position.set(0, 0.058, 0.05); g.add(pad);
    for (const x of [-0.023, 0.023]) box(0.006, 0.012, 0.012, std(PAL.blue), x, 0.06, -0.09, g);
    slide = box(0.03, 0.01, 0.08, trim, 0, 0.08, -0.11, g);
    muzzleZ = -0.18; muzzleY = 0.05;
  } else if (kind === 'shell') {     // two-hand shell with a remote riding in the barrel
    slide = box(0.034, 0.03, 0.15, trim, 0, 0.085, -0.17, g);
    muzzleZ = -0.26; muzzleY = 0.085;
  }
  const flash = new THREE.Mesh(new THREE.SphereGeometry(0.03, 10, 8), additive('#ffd27a', 0));
  flash.position.set(0, muzzleY, muzzleZ - 0.03); g.add(flash);
  const ray = new THREE.Mesh(new THREE.CylinderGeometry(0.0025, 0.0025, 1.2, 6), additive('#4ff3ff', 0));
  ray.rotation.x = Math.PI / 2; ray.position.set(0, muzzleY, muzzleZ - 0.6); g.add(ray);
  g.userData.gun = { kind, slide, slideZ: slide ? slide.position.z : 0, drum, flash, ray };
  return g;
}

// one shot every `period` seconds: slide or hammer kicks, muzzle flashes, aim ray pulses
function animateShot(gun, t, period, offset) {
  const u = gun.userData.gun;
  const n = Math.floor((t + offset) / period);
  const ph = ((t + offset) % period) / period;
  const kick = reducedMotion ? 0 : Math.max(0, 1 - ph * 9);
  if (u.kind === 'revolver') u.slide.rotation.x = -kick * 0.7;
  else if (u.slide) u.slide.position.z = u.slideZ + kick * 0.035;
  if (u.drum) u.drum.rotation.y = n * (Math.PI / 3);
  u.flash.material.opacity = kick > 0.55 ? 0.7 : 0;
  u.flash.scale.setScalar(0.4 + kick * 0.5);
  u.ray.material.opacity = 0.08 + kick * 0.35;
  return kick;
}

function controllerModel(color) {
  const g = new THREE.Group();
  const shell = std('#3c3a48', { roughness: 0.4, envMap: envTex, envMapIntensity: 0.5, emissive: '#2a2836', emissiveIntensity: 0.6 });
  const grip = new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.026, 0.12, 16), shell);
  grip.position.set(0, -0.045, 0.02); grip.rotation.x = 0.35; g.add(grip);
  const head = new THREE.Mesh(new THREE.CylinderGeometry(0.032, 0.03, 0.035, 20), shell);
  head.position.set(0, 0.022, -0.005); g.add(head);
  const stick = new THREE.Mesh(new THREE.CylinderGeometry(0.008, 0.008, 0.014, 12), std('#111'));
  stick.position.set(-0.009, 0.045, 0.0); g.add(stick);
  box(0.009, 0.004, 0.009, glow(color, 1.5), 0.013, 0.041, -0.012, g);
  const ring = new THREE.Mesh(new THREE.TorusGeometry(0.055, 0.007, 8, 40), shell);
  ring.position.set(0, 0.03, -0.01); ring.rotation.x = Math.PI / 2 - 0.5; g.add(ring);
  const accent = new THREE.Mesh(new THREE.TorusGeometry(0.055, 0.0025, 6, 40), glow(color, 2.2));
  accent.position.set(0, 0.034, -0.01); accent.rotation.copy(ring.rotation); g.add(accent);
  box(0.014, 0.03, 0.02, std('#111'), 0, -0.005, -0.035, g);
  const ray = new THREE.Mesh(new THREE.CylinderGeometry(0.002, 0.002, 1.4, 6), additive(color, 0.5));
  ray.rotation.x = Math.PI / 2; ray.position.set(0, 0.02, -0.72); g.add(ray);
  return g;
}

function floatingRing(x, y, z, r = 0.15, parent = scene) {
  const ring = new THREE.Mesh(new THREE.TorusGeometry(r, 0.006, 8, 48), glow('#4ff3ff', 0.9));
  ring.rotation.x = Math.PI / 2; ring.position.set(x, y, z); parent.add(ring);
  return ring;
}

// Exhibits spread out on wide screens and gather on narrow ones. Each registers a wide and a
// narrow spot; applyLayout() blends between them by aspect ratio (at start and on resize).
const layouts = [];
function exhibitHolder(wide, narrow) {
  const holder = new THREE.Group();
  holder.userData.dynamic = true;  // keeps it out of static batching so it can move
  scene.add(holder);
  layouts.push({ holder, wide, narrow });
  return holder;
}
function applyLayout() {
  const k = smooth(1.45, 2.0, camera.aspect);  // 16:10 laptop stays gathered, 2:1 and wider spreads
  for (const { holder, wide, narrow } of layouts) holder.position.lerpVectors(narrow, wide, k);
}

// turntable display for a gun: spins, fires, bobs
function gunOnTurntable(gun, wide, narrow, scale, stop, i) {
  const holder = exhibitHolder(wide, narrow);
  const pivot = new THREE.Group();
  gun.scale.setScalar(scale); pivot.add(gun); holder.add(pivot);
  floatingRing(0, -0.32, 0, 0.15, holder);
  if (stop !== undefined) markClickable(holder, stop);
  animated.push(t => {
    if (!reducedMotion) {
      pivot.rotation.y = t * 0.45 + i * 1.3;
      pivot.position.y = Math.sin(t * 1.1 + i) * 0.02;
    }
    gun.rotation.x = animateShot(gun, t, 2.4, i * 0.47) * 0.3;
  });
  return holder;
}

// stop 1: two controllers that turn into guns and back. Wide screens (2:1 and up): at about 22%
// and 42% of the width from the stop-1 camera, so both stay left of the card. Narrow screens: both gathered left of centre. The camera
// path curves to the right of both (1.4 m or more).
[
  { wide: v(-1.46, 1.5, -1.29), narrow: v(-1.9, 1.55, -2.3), col: '#4ff3ff', kind: 'slide', main: PAL.blue, acc: '#e8e8ee', i: 0 },
  { wide: v(-1.4, 1.35, -2.41), narrow: v(-1.6, 1.3, -2.9), col: '#ff4fa8', kind: 'twin', main: PAL.red, acc: PAL.yellow, i: 1 },
].forEach(({ wide, narrow, col, kind, main, acc, i }) => {
  const holder = exhibitHolder(wide, narrow);
  const pivot = new THREE.Group();
  const ctrl = controllerModel(col);
  const gun = gunModel(kind, main, acc);
  pivot.add(ctrl, gun);
  holder.add(pivot);
  floatingRing(0, -0.32, 0, 0.17, holder);
  markClickable(holder, 1);
  const S = 3.4;
  animated.push(t => {
    const cyc = ((t + i * 0.8) % 7) / 7;
    const g = smooth(0.42, 0.52, cyc) - smooth(0.9, 1.0, cyc);
    ctrl.scale.setScalar(S * (1 - g) + 1e-4);
    gun.scale.setScalar(S * g + 1e-4);
    const sway = reducedMotion ? 0 : Math.sin(t * 0.9 + i * 2) * 0.35;
    pivot.rotation.set(reducedMotion ? 0 : Math.sin(t * 1.3 + i) * 0.12, -0.8 + sway, 0);
    if (g > 0.5) gun.rotation.x = animateShot(gun, t, 1.4, i * 0.6) * 0.3;
  });
});

// stop 2: one gun in the aisle that becomes the next model every few seconds
{
  const models = [['slide', PAL.blue, '#e8e8ee'], ['revolver', PAL.teal, '#c9c9d2'], ['twin', PAL.red, PAL.yellow], ['dpad', PAL.orange, '#1b1b20'], ['shell', '#a7acb8', '#4fd2ff']]
    .map(([kind, main, acc]) => gunModel(kind, main, acc));
  const S = 2.5, HOLD = 4.5;
  const holder = exhibitHolder(v(-5.0, 1.45, -6.5), v(-5.0, 1.45, -6.5));  // one gun: same spot at every width
  const pivot = new THREE.Group();
  models.forEach(m => { m.scale.setScalar(1e-4); pivot.add(m); });
  holder.add(pivot);
  floatingRing(0, -0.32, 0, 0.15, holder);
  markClickable(holder, 2);
  animated.push(t => {
    const n = Math.floor(t / HOLD), ph = (t % HOLD) / HOLD;
    const grow = smooth(0, 0.08, ph) * (1 - smooth(0.92, 1, ph));
    models.forEach((m, k) => m.scale.setScalar(k === n % models.length ? S * grow + 1e-4 : 1e-4));
    const gun = models[n % models.length];
    if (!reducedMotion) {
      pivot.rotation.y = t * 0.45;
      pivot.position.y = Math.sin(t * 1.1) * 0.02;
    }
    gun.rotation.x = animateShot(gun, t, 1.5, 0.6) * 0.3;
  });
}

// ---------------------------------------------------------------- systems
// stop 4: Model 3 corner, blank cabs on the left wall
upright({ x: -7.2, z: -10.7, rotY: Math.PI / 2, color: PAL.orange, marquee: 'm3-a', screenTex: screenGun, w: 1.05, h: 2.1, d: 0.95, guns: true, stop: 4 });
upright({ x: -7.2, z: -11.9, rotY: Math.PI / 2, color: PAL.lime, marquee: 'm3-b', screenTex: screenCorridor, w: 1.05, h: 2.1, d: 0.95, guns: true, stop: 4 });
sitdown({ x: -6.6, z: -13.3, rotY: Math.PI / 2, color: PAL.purple, marquee: 'm3-c', screenTex: screenRace, stop: 4 });

// stops 5 and 6: console kiosks on the right wall, each with its own gun
function kiosk({ x, z, consoleKind, screenTex, gun, stop, i, gunDzWide = 0, gunDzNarrow = 0 }) {
  const g = new THREE.Group();
  box(1.5, 0.72, 0.55, std('#3a2752', { roughness: 0.6 }), 0, 0.36, 0, g);
  box(1.5, 0.03, 0.03, glow(consoleKind === 'tall' ? '#4fd2ff' : PAL.orange, 2), 0, 0.72, 0.28, g);
  box(1.25, 0.76, 0.1, std('#0c0b10', { roughness: 0.35 }), 0, 1.22, -0.12, g);
  plane(1.15, 0.66, basicMat(screenTex), 0, 1.22, -0.068, g);
  box(0.3, 0.06, 0.2, std('#0c0b10'), 0, 0.78, -0.12, g);
  // generic console shapes, about 1.6x so they read from the aisle; lit edges, no logos
  if (consoleKind === 'tall') {
    box(0.08, 0.27, 0.32, std('#c9ccd6', { roughness: 0.3 }), 0.5, 0.735 + 0.135, 0.06, g);
    box(0.006, 0.1, 0.006, glow('#4fb8ff', 2.5), 0.5, 0.92, 0.222, g);
    box(0.082, 0.006, 0.322, glow('#4fb8ff', 1.2), 0.5, 0.738, 0.06, g);
  } else {
    box(0.5, 0.11, 0.38, std('#262532', { roughness: 0.4, envMap: envTex, envMapIntensity: 0.4 }), 0.42, 0.735 + 0.055, 0.04, g);
    box(0.5, 0.006, 0.006, glow('#4f7bff', 2), 0.42, 0.79, 0.232, g);
    box(0.006, 0.11, 0.006, glow('#4f7bff', 1.2), 0.17, 0.79, 0.232, g);
  }
  g.position.set(x, 0, z); g.rotation.y = -Math.PI / 2;
  scene.add(g);
  markClickable(g, stop);
  // the gun floats on the kiosk's outer side, away from its neighbour: further out on wide
  // screens, tucked closer to the TV on narrow ones
  gunOnTurntable(gun, v(x - 1.05, 1.3, z + gunDzWide), v(x - 1.05, 1.3, z + gunDzNarrow), 2.4, stop, i);
}
kiosk({ x: 7.35, z: -14.6, consoleKind: 'tall', screenTex: screenCorridor, gun: gunModel('shell', '#a7acb8', '#4fd2ff'), stop: 5, i: 5, gunDzWide: 1.05, gunDzNarrow: 0.55 });
kiosk({ x: 7.35, z: -17.2, consoleKind: 'slab', screenTex: screenGun, gun: gunModel('dpad', PAL.orange, '#1b1b20'), stop: 6, i: 6, gunDzWide: -1.05, gunDzNarrow: -0.55 });

// exit sign over the door, inside
plane(0.5, 0.18, new THREE.MeshBasicMaterial({ map: textSign('EXIT', '#300', '#ff4040'), toneMapped: false }), 0, 3.1, -0.3).rotation.y = Math.PI;

// ---------------------------------------------------------------- static batching
// Meshes that never move and share a material (and click target) are merged into one draw.
// Anything under a userData.dynamic ancestor, transparent, or multi-material stays as is.
{
  scene.updateMatrixWorld(true);
  const isDynamic = o => { for (let a = o; a; a = a.parent) if (a.userData.dynamic) return true; return false; };
  const buckets = new Map();
  scene.traverse(o => {
    if (!o.isMesh || Array.isArray(o.material) || o.material.transparent || isDynamic(o)) return;
    const key = `${o.material.uuid}|${o.userData.stop ?? ''}|${o.geometry.index ? 'i' : 'n'}`;
    if (!buckets.has(key)) buckets.set(key, []);
    buckets.get(key).push(o);
  });
  for (const list of buckets.values()) {
    if (list.length < 2) continue;
    const merged = mergeGeometries(list.map(m => m.geometry.clone().applyMatrix4(m.matrixWorld)), false);
    if (!merged) continue;
    const mesh = new THREE.Mesh(merged, list[0].material);
    mesh.userData.stop = list[0].userData.stop;
    scene.add(mesh);
    list.forEach(m => m.removeFromParent());
  }
  clickables.length = 0;
  scene.traverse(o => { if (o.isMesh && o.userData.stop !== undefined) clickables.push(o); });
}

// ---------------------------------------------------------------- post-processing
const composer = new EffectComposer(renderer);
composer.addPass(new RenderPass(scene, camera));
// Bloom only catches real emitters (linear > 1.1). Lit props never reach it.
const bloom = new UnrealBloomPass(new THREE.Vector2(innerWidth, innerHeight), 0.45, 0.35, 1.1);
composer.addPass(bloom);
composer.addPass(new OutputPass());
const fxaa = new ShaderPass(FXAAShader);
composer.addPass(fxaa);
function sizeFxaa() {
  const pr = renderer.getPixelRatio();
  fxaa.material.uniforms.resolution.value.set(1 / (innerWidth * pr), 1 / (innerHeight * pr));
}
sizeFxaa();

// Emitters get real HDR level. Under the composer, toneMapped:false is ignored (OutputPass
// tone-maps every pixel), so screens, marquees and the sign are lifted above 1.0 instead.
{
  const seen = new Set();
  scene.traverse(o => {
    const m = o.material;
    if (!o.isMesh || !m || !m.isMeshBasicMaterial || seen.has(m) || m.blending === THREE.AdditiveBlending) return;
    seen.add(m);
    if (m === signMesh.material) m.color.setScalar(1.6);
    else if (m.map) m.color.multiplyScalar(1.4);
    else m.color.multiplyScalar(1.2);  // ceiling light panels
  });
}

// ---------------------------------------------------------------- the walk
const STOPS = [
  { pos: v(0, 1.62, 9.8), look: v(0, 2.55, 0), fov: 58 },            // mall, facing the sign
  { pos: v(0.9, 1.6, -0.2), look: v(-2.76, 1.49, -4.95), fov: 50, subject: v(-1.75, 1.42, -2.6) },  // motion controls
  { pos: v(-2.9, 1.6, -6.4), look: v(-8.53, 1.27, -8.46), fov: 46, subject: v(-5.0, 1.45, -6.5) }, // the gun
  { pos: v(4.6, 1.8, -11.45), look: v(10.04, -0.71, -11.72), fov: 52, subject: v(6.2, 1.0, -11.0) }, // racing, over the seat
  { pos: v(-3.3, 1.6, -12.0), look: v(-9.16, 1.34, -13.25), fov: 58, subject: v(-7.0, 1.4, -11.9) }, // Model 3
  { pos: v(3.2, 1.6, -15.5), look: v(9.19, 1.2, -15.55), fov: 46, subject: v(7.0, 1.2, -14.8) },  // Wii
  { pos: v(3.2, 1.6, -16.3), look: v(9.18, 1.19, -16.08), fov: 46, subject: v(7.0, 1.2, -17.0) }, // PS2
  { pos: v(1.2, 1.6, -18.3), look: v(3.7, 1.32, -23.75), fov: 58, subject: v(4.0, 1.1, -21.6) },  // prize counter
  { pos: v(-1.8, 1.6, -16.9), look: v(-5.51, 1.18, -21.59), fov: 52, subject: v(-4.2, 1.45, -18.6) }, // the Hub
  { pos: v(0.6, 1.65, -19.4), look: v(1.82, 1.92, -25.27), fov: 58, subject: v(-0.4, 2.0, -25.9) }, // scoreboard
];
// in-between waypoints keep the path at least 0.5 m off the cabinets
const PATH = [STOPS[0].pos, v(0, 1.61, 5.2), v(0, 1.6, 2.0), STOPS[1].pos, v(0.2, 1.6, -4.2), v(-2.6, 1.6, -5.0), STOPS[2].pos,
  v(-2.9, 1.6, -9.0), v(0.8, 1.6, -10.0), STOPS[3].pos, v(0.4, 1.6, -11.6), STOPS[4].pos, v(0.2, 1.6, -13.4),
  STOPS[5].pos, STOPS[6].pos, STOPS[7].pos, v(0.0, 1.6, -17.4), STOPS[8].pos, v(-0.2, 1.6, -18.4), STOPS[9].pos];
const STOP_IDX = STOPS.map(st => PATH.indexOf(st.pos));
const curve = new THREE.CatmullRomCurve3(PATH, false, 'centripetal');
const N = STOPS.length;
// Each stop's view as yaw and pitch, so moves between opposite walls turn the head instead of
// swinging the look point through the camera.
function aimStops() {
  STOPS[0].pos.set(0, 1.62, portrait ? 16.5 : 9.8);
  for (const st of STOPS) {
    // portrait: the card covers the lower half, so aim a little under the subject to lift it into view
    const target = portrait && st.subject ? st.subject.clone().add(v(0, -0.55, 0)) : st.look;
    const d = target.clone().sub(st.pos);
    st.yaw = Math.atan2(d.x, -d.z);
    st.pitch = Math.atan2(d.y, Math.hypot(d.x, d.z));
  }
}
aimStops();
applyLayout();
const wrapAngle = a => Math.atan2(Math.sin(a), Math.cos(a));

const sections = [...document.querySelectorAll('.stop')];
const cards = sections.map(s => s.querySelector('.card'));
const navLinks = [...document.querySelectorAll('#stop-nav a')];
const creditFill = document.getElementById('credit-fill');
const creditNum = document.getElementById('credit-num');

function scrollToStop(i) {
  sections[i].scrollIntoView({ behavior: reducedMotion ? 'auto' : 'smooth', block: 'start' });
}
cards.forEach((c, i) => c.addEventListener('focusin', () => { if (Math.round(p) !== i) scrollToStop(i); }));
navLinks.forEach(a => a.addEventListener('click', e => { e.preventDefault(); scrollToStop(+a.dataset.stop); history.replaceState(null, '', a.getAttribute('href')); }));
document.querySelector('.brand').addEventListener('click', e => { e.preventDefault(); scrollToStop(0); });

let tops = [];
function measure() { tops = sections.map(s => s.getBoundingClientRect().top + scrollY); }
measure();
function progress() {
  // stop i sits where section i's top meets the viewport top
  const y = scrollY;
  for (let i = N - 1; i >= 0; i--) {
    if (y >= tops[i] - 1) {
      if (i === N - 1) return N - 1;
      return i + Math.min(1, (y - tops[i]) / (tops[i + 1] - tops[i]));
    }
  }
  return 0;
}

const camPos = new THREE.Vector3(), camLook = new THREE.Vector3();
const lookTmp = new THREE.Vector3();
let p = progress(), pShown = p;
let pointer = { x: 0, y: 0 }, pointerEase = { x: 0, y: 0 };
let drag = null, dragYaw = 0, dragPitch = 0;
let lastActive = -1;

function poseAt(q, outPos, outLook) {
  const i = Math.min(N - 2, Math.floor(q));
  const t = q - i;
  // dwell near stops, glide between them
  const te = smooth(0.12, 0.88, t);
  const a = STOP_IDX[i] / (PATH.length - 1), b = STOP_IDX[i + 1] / (PATH.length - 1);
  curve.getPoint(a + (b - a) * te, outPos);
  const A = STOPS[i], B = STOPS[i + 1], k = smooth(0.2, 0.85, t);
  const yaw = A.yaw + wrapAngle(B.yaw - A.yaw) * k, pitch = A.pitch + (B.pitch - A.pitch) * k;
  outLook.set(outPos.x + Math.sin(yaw) * Math.cos(pitch), outPos.y + Math.sin(pitch), outPos.z - Math.cos(yaw) * Math.cos(pitch));
  // per-stop lens; portrait phones get 16 degrees more so the subject fits above the card
  const fov = A.fov + (B.fov - A.fov) * k + (portrait ? 16 : 0);
  if (Math.abs(camera.fov - fov) > 0.01) { camera.fov = fov; camera.updateProjectionMatrix(); }
  return te;
}

addEventListener('scroll', () => { p = progress(); }, { passive: true });
addEventListener('pointermove', e => {
  pointer.x = (e.clientX / innerWidth) * 2 - 1;
  pointer.y = (e.clientY / innerHeight) * 2 - 1;
  if (drag && drag.look) {
    dragYaw = Math.max(-1.2, Math.min(1.2, drag.yaw - (e.clientX - drag.x) * 0.004));
    dragPitch = Math.max(-0.5, Math.min(0.5, drag.pitch - (e.clientY - drag.y) * 0.003));
  }
  hover(e);
});
canvas.parentElement.addEventListener('pointerdown', e => {
  if (e.target !== canvas && !e.target.classList.contains('stop')) return;
  drag = { x: e.clientX, y: e.clientY, yaw: dragYaw, pitch: dragPitch, look: e.pointerType === 'mouse' };
});
addEventListener('pointerup', e => {
  if (drag && Math.hypot(e.clientX - drag.x, e.clientY - drag.y) < 6) pick(e);
  drag = null;
});

const raycaster = new THREE.Raycaster();
const ndc = new THREE.Vector2();
function hitStop(e) {
  if (e.target !== canvas && !(e.target.classList && e.target.classList.contains('stop'))) return undefined;
  ndc.set((e.clientX / innerWidth) * 2 - 1, -(e.clientY / innerHeight) * 2 + 1);
  raycaster.setFromCamera(ndc, camera);
  const hit = raycaster.intersectObjects(clickables, false)[0];
  return hit && hit.distance < 14 ? hit.object.userData.stop : undefined;
}
function hover(e) {
  if (drag) return;
  document.body.style.cursor = hitStop(e) !== undefined ? 'pointer' : '';
}
function pick(e) {
  const s = hitStop(e);
  if (s !== undefined) scrollToStop(s);
}

addEventListener('keydown', e => {
  if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) return;  // leave browser shortcuts alone
  if (e.target.closest && e.target.closest('a, button, input')) return;
  const cur = Math.round(p);
  if (e.key === 'ArrowRight' || e.key === 'n') { scrollToStop(Math.min(N - 1, cur + 1)); e.preventDefault(); }
  if (e.key === 'ArrowLeft' || e.key === 'p') { scrollToStop(Math.max(0, cur - 1)); e.preventDefault(); }
});

function resize() {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
  composer.setSize(innerWidth, innerHeight);
  sizeFxaa();
  const nowPortrait = innerWidth < innerHeight;
  if (nowPortrait !== portrait) { portrait = nowPortrait; aimStops(); }
  applyLayout();
  measure();
  p = progress();
}
// debounced: phone toolbars resize the viewport on every scroll direction change
let resizeTimer = 0;
addEventListener('resize', () => { clearTimeout(resizeTimer); resizeTimer = setTimeout(resize, 150); });

// ---------------------------------------------------------------- loop
const clock = new THREE.Clock();
let screenAcc = 0, walked = 0, lastPShown = pShown, signArmed = false;
sign.restart(0.9);
// Quality governor: hold ~60 fps on fast displays, and step resolution, then bloom, down
// when frames run long. It never steps back up within a visit.
const FRAME_MS = 1000 / 60;
let lastFrame = 0, lastRaf = 0, minRaf = 1e9, slowAcc = 0, slowCount = 0, dprCap = renderer.getPixelRatio();
const govStart = performance.now() + 3000;  // ignore shader compiles and font swaps at load
function govern(ms) {
  if (ms > 100 || performance.now() < govStart) return;  // tab hidden, or still warming up
  slowAcc += ms; slowCount++;
  if (slowCount < 90) return;
  const avg = slowAcc / slowCount; slowAcc = 0; slowCount = 0;
  if (avg < Math.max(28, minRaf * 1.8)) return;  // well below what this display can show
  if (dprCap > 1) {
    dprCap = Math.max(1, dprCap - 0.25);
    renderer.setPixelRatio(dprCap); composer.setPixelRatio(dprCap);
    renderer.setSize(innerWidth, innerHeight); composer.setSize(innerWidth, innerHeight); sizeFxaa();
  } else if (bloom.enabled) {
    bloom.enabled = false;
  }
}
function frame(now = 0) {
  if (stopped) return;
  requestAnimationFrame(frame);
  if (lastRaf && now - lastRaf > 1) minRaf = Math.min(minRaf, now - lastRaf);
  lastRaf = now;
  // skip a frame only on displays well above 60 Hz (120/144/240), so 60, 72 and 75 Hz render every frame
  if (now - lastFrame < FRAME_MS * 0.75) return;
  govern(now - lastFrame);
  lastFrame = now;
  if (renderer.info.autoReset === false) renderer.info.reset();
  const rawDt = clock.getDelta();
  const dt = Math.min(rawDt, 0.05);  // animation step
  const t = clock.elapsedTime;

  // the walk eases on real time, so a slow frame rate does not leave the camera a stop behind
  pShown += (p - pShown) * (reducedMotion ? 1 : 1 - Math.exp(-Math.min(rawDt, 0.5) * 6));
  if (Math.abs(p - pShown) < 1e-4) pShown = p;
  poseAt(pShown, camPos, camLook);

  // gentle head bob while actually moving
  walked += Math.abs(pShown - lastPShown) * 9; lastPShown = pShown;
  const bob = reducedMotion ? 0 : Math.sin(walked * 2.2) * 0.025 * Math.min(1, Math.abs(p - pShown) * 6);
  camera.position.set(camPos.x, camPos.y + bob, camPos.z);

  // pointer parallax + drag-to-look
  pointerEase.x += (pointer.x - pointerEase.x) * (1 - Math.exp(-dt * 3));
  pointerEase.y += (pointer.y - pointerEase.y) * (1 - Math.exp(-dt * 3));
  if (!drag || !drag.look) { dragYaw *= Math.exp(-dt * 0.6); dragPitch *= Math.exp(-dt * 0.6); }
  lookTmp.subVectors(camLook, camPos);
  // parallax shrinks with the lens on narrow screens so it never pushes the subject out of frame
  const sway = Math.min(1, camera.aspect);
  const yaw = -pointerEase.x * 0.12 * sway + dragYaw, pitch = -pointerEase.y * 0.06 * sway + dragPitch;
  lookTmp.applyAxisAngle(new THREE.Vector3(0, 1, 0), yaw);
  const right = new THREE.Vector3().crossVectors(lookTmp, camera.up).normalize();
  lookTmp.applyAxisAngle(right, pitch);
  camera.lookAt(camPos.x + lookTmp.x, camPos.y + bob + lookTmp.y, camPos.z + lookTmp.z);

  // attract screens at ~20 fps
  screenAcc += dt;
  for (const fn of animated) fn(t, dt);
  // the sign replays its ARCADE to AC routine whenever you walk back out to the mall
  if (p > 0.6) signArmed = true;
  if (signArmed && p < 0.05) { sign.restart(t + 0.3); signArmed = false; }
  sign.update(t);
  if (screenAcc > 0.05) { for (const fn of screens) fn(t); screenAcc = 0; }

  // cards and HUD follow the scroll, not the eased camera, so text never lags
  cards.forEach((c, i) => {
    const vis = 1 - smooth(0.22, 0.5, Math.abs(p - i));
    c.style.setProperty('--vis', vis.toFixed(3));
    if (vis < 0.02) c.setAttribute('data-hidden', ''); else c.removeAttribute('data-hidden');
  });
  const active = Math.round(p);
  if (active !== lastActive) {
    navLinks.forEach((a, i) => a.classList.toggle('active', i === active));
    lastActive = active;
  }
  creditFill.style.width = `${(p / (N - 1)) * 100}%`;
  creditNum.textContent = String(Math.round(p)).padStart(2, '0');

  composer.render(dt);
}

// deep links (#stop-3) land on their stop without a walk from the mall
{
  const i = sections.findIndex(sec => '#' + sec.id === location.hash);
  if (i > 0) { scrollTo({ top: tops[i], behavior: 'instant' }); p = pShown = lastPShown = i; }
}
// ?debug exposes render stats for checking draw calls and quality steps
const debug = new URLSearchParams(location.search).has('debug');
if (debug) renderer.info.autoReset = false;
if (debug) window.__acvr = { renderer, scene, bloom, get dpr() { return renderer.getPixelRatio(); }, get p() { return p; } };
requestAnimationFrame(frame);
requestAnimationFrame(() => curtain.classList.add('done'));
setTimeout(() => curtain.remove(), 900);
