// Model bench: an interactive panel showing the project's own models one at a time.
// Starts only when the panel scrolls into view; drag to turn, Fire to see the moving parts work.
import * as THREE from 'three';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { catalog, loadModel } from './models.js';

const panel = document.getElementById('bench');
if (panel) {
  const start = () => init().catch(err => { console.warn('model bench', err); panel.classList.add('bench-off'); });
  if ('IntersectionObserver' in window) {
    const io = new IntersectionObserver(es => { if (es.some(e => e.isIntersecting)) { io.disconnect(); start(); } }, { rootMargin: '300px' });
    io.observe(panel);
  } else start();
}

async function init() {
  const canvas = panel.querySelector('canvas');
  const chips = panel.querySelector('.bench-chips');
  const info = panel.querySelector('.bench-info');
  const fireBtn = panel.querySelector('.bench-fire');
  const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;

  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  const env = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(renderer), 0.04).texture;
  const scene = new THREE.Scene();
  scene.add(new THREE.HemisphereLight('#e8e4ff', '#2a1a3a', 1.1));
  const key = new THREE.DirectionalLight('#ffffff', 2.2); key.position.set(0.8, 1.4, 1.2); scene.add(key);
  const rim = new THREE.DirectionalLight('#4ff3ff', 1.2); rim.position.set(-1.2, 0.6, -1); scene.add(rim);
  const camera = new THREE.PerspectiveCamera(30, 1, 0.01, 20);
  const stage = new THREE.Group(); scene.add(stage);

  const cat = await catalog();
  const show = cat.models.filter(m => m.kind !== 'hand');
  const label = { gun: 'Gun', controller: 'Controller', control: 'Cabinet control' };
  // start side-on: models face -Z, so a quarter turn shows the profile
  let current = null, token = 0, yaw = -Math.PI / 2 + 0.35, pitch = 0.12, dragging = null, lastShot = -9;

  show.forEach((m, i) => {
    const b = document.createElement('button');
    b.type = 'button'; b.textContent = m.name.replace(/^The /, '');
    b.dataset.id = m.id;
    b.addEventListener('click', () => pick(m.id));
    chips.appendChild(b);
    if (i === 0) b.classList.add('on');
  });

  async function pick(id) {
    const my = ++token;
    chips.querySelectorAll('button').forEach(b => b.classList.toggle('on', b.dataset.id === id));
    const entry = show.find(m => m.id === id);
    const m = await loadModel(id, { envMap: env, hands: false });
    if (my !== token) return;
    if (current) stage.remove(current.root);
    current = m;
    // centre and frame the model at its real size
    const box = new THREE.Box3().setFromObject(m.root);
    const size = box.getSize(new THREE.Vector3()), mid = box.getCenter(new THREE.Vector3());
    m.root.position.sub(mid);
    stage.add(m.root);
    const radius = Math.max(size.x, size.y, size.z) * 0.62;
    camera.position.set(0, 0, radius / Math.tan(THREE.MathUtils.degToRad(15)) * 1.05);
    camera.lookAt(0, 0, 0);
    const moving = Object.keys(entry.motion || {}).length;
    info.innerHTML = '';
    const h = document.createElement('h3'); h.textContent = entry.name; info.appendChild(h);
    const meta = document.createElement('p'); meta.className = 'bench-meta';
    const line = [label[entry.kind] || entry.kind, entry.family, entry.hold && entry.hold.replace('-', ' '), entry.length_mm && entry.length_mm + ' mm', moving ? moving + ' moving parts' : ''].filter(Boolean).join(' · ');
    meta.textContent = line.charAt(0).toUpperCase() + line.slice(1);
    info.appendChild(meta);
    if (entry.shape) { const p = document.createElement('p'); p.textContent = entry.shape; info.appendChild(p); }
    fireBtn.hidden = !(entry.motion && (entry.motion.trigger || entry.motion.recoil));
  }

  fireBtn.addEventListener('click', () => { lastShot = clock.elapsedTime; });
  canvas.addEventListener('pointerdown', e => { dragging = { x: e.clientX, y: e.clientY, yaw, pitch }; canvas.setPointerCapture(e.pointerId); });
  canvas.addEventListener('pointermove', e => {
    if (!dragging) return;
    yaw = dragging.yaw + (e.clientX - dragging.x) * 0.01;
    pitch = Math.max(-0.8, Math.min(0.8, dragging.pitch + (e.clientY - dragging.y) * 0.006));
  });
  canvas.addEventListener('pointerup', () => { dragging = null; });

  function resize() {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (canvas.width !== Math.floor(w * renderer.getPixelRatio()) || canvas.height !== Math.floor(h * renderer.getPixelRatio())) {
      renderer.setSize(w, h, false);
      camera.aspect = w / h; camera.updateProjectionMatrix();
    }
  }
  const clock = new THREE.Clock();
  let visible = true;
  new IntersectionObserver(es => { visible = es.some(e => e.isIntersecting); }).observe(panel);
  renderer.setAnimationLoop(() => {
    const dt = clock.getDelta();
    if (!visible) return;
    resize();
    if (!dragging && !reduced) yaw += dt * 0.35;
    stage.rotation.set(pitch, yaw, 0);
    if (current) {
      const since = clock.elapsedTime - lastShot;
      const kick = Math.max(0, 1 - since * 6);
      current.pull(since < 0.25 ? 1 : 0);
      current.shoot(kick);
    }
    renderer.render(scene, camera);
  });
  await pick(show[0].id);
  panel.classList.add('bench-ready');
}
