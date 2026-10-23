// The project's own models (copied into models/ by tools/sync_models.py) for the site's scenes.
// Each GLB uses four named materials (body, accent, dark, glass); tints come from data/guns.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

const loader = new GLTFLoader();
const files = new Map();     // file -> Promise<gltf>
let catalogPromise = null;

export function catalog() {
  if (!catalogPromise) catalogPromise = fetch('models/models.json').then(r => r.json());
  return catalogPromise;
}

function loadFile(file) {
  if (!files.has(file)) files.set(file, loader.loadAsync(file));
  return files.get(file);
}

const DARK = '#2c2a33', GLASS = '#1d4a5c';

// style: 'solid' (lit, tinted) or 'ghost' (see-through cyan, like the ghost controls in the headset)
function restyle(root, entry, style, envMap) {
  // hands have no tint table in data/guns: a dark glove with a lighter cuff
  const tint = entry.tint || (entry.kind === 'hand' ? { body: '#3b4150', accent: '#6b7282' } : {});
  const solid = {
    body: new THREE.MeshStandardMaterial({ color: tint.body || '#8c8f98', roughness: 0.42, metalness: 0.08, envMap, envMapIntensity: 0.5 }),
    accent: new THREE.MeshStandardMaterial({ color: tint.accent || '#5a5d66', roughness: 0.5, metalness: 0.15, envMap, envMapIntensity: 0.4 }),
    dark: new THREE.MeshStandardMaterial({ color: DARK, roughness: 0.6, metalness: 0.05, envMap, envMapIntensity: 0.3 }),
    glass: new THREE.MeshStandardMaterial({ color: GLASS, roughness: 0.1, metalness: 0.2, emissive: '#2fd2ff', emissiveIntensity: 0.25, envMap, envMapIntensity: 1 }),
  };
  const ghost = new THREE.MeshBasicMaterial({
    color: new THREE.Color('#7ff8ff').multiplyScalar(1.6), transparent: true, opacity: 0.32,
    blending: THREE.AdditiveBlending, depthWrite: false,
  });
  root.traverse(o => {
    // LOD1 copies are for distance in the headset; the site always shows LOD0
    if (/(^|_)lod1$/i.test(o.name) || o.name === 'LOD1') o.visible = false;
    if (!o.isMesh) return;
    const name = (o.material && o.material.name) || 'body';
    o.material = style === 'ghost' ? ghost : (solid[name] || solid.body);
  });
}

// Returns { root, entry, node(name), shoot(kick 0..1), pull(0..1) }. Units are metres; -Z is forward.
export async function loadModel(id, { style = 'solid', envMap = null, hands = false } = {}) {
  const cat = await catalog();
  const entry = cat.models.find(m => m.id === id);
  if (!entry) throw new Error('unknown model ' + id);
  const gltf = await loadFile(entry.file);
  const root = gltf.scene.clone(true);
  restyle(root, entry, style, envMap);
  const byName = new Map();
  root.traverse(o => { if (o.name && !byName.has(o.name)) byName.set(o.name, o); });
  const node = n => byName.get(n);

  if (hands && entry.hands && entry.hands.right) {
    const bind = entry.hands.right;
    const mount = node(bind.node) || root;
    const hand = await loadModel('grip-hand-right', { style, envMap });
    hand.root.position.fromArray(bind.translation || [0, 0, 0]);
    hand.root.quaternion.fromArray(bind.rotation_xyzw || [0, 0, 0, 1]);
    mount.add(hand.root);
  }

  // moving parts from data/guns motion tables
  const parts = [];
  for (const m of Object.values(entry.motion || {})) {
    if (m.drive !== 'trigger' && m.drive !== 'recoil') continue;
    const o = node(m.node);
    if (!o) continue;
    parts.push({ o, m, pos: o.position.clone(), quat: o.quaternion.clone() });
  }
  const axis = new THREE.Vector3(), q = new THREE.Quaternion();
  function drive(kind, amount) {
    for (const { o, m, pos, quat } of parts) {
      if (m.drive !== kind) continue;
      const [lo, hi] = m.range || [0, 0];
      const v = lo + (hi - lo) * amount;
      axis.fromArray(m.axis || [0, 0, 1]).normalize();
      if (m.kind === 'slide') o.position.copy(pos).addScaledVector(axis, v);
      else o.quaternion.copy(quat).multiply(q.setFromAxisAngle(axis, v));
    }
  }
  return { root, entry, node, pull: a => drive('trigger', a), shoot: a => drive('recoil', a) };
}
