// The project's own models (copied into models/ by tools/sync_models.py) for the site's scenes.
// Each GLB uses four named materials (body, accent, dark, glass); tints come from data/guns.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

const loader = new GLTFLoader();
const files = new Map();     // file -> Promise<gltf>
// Bump with the ?v= tags in the pages when models/ changes.
const VER = '20261023d';
let catalogPromise = null;

export function catalog() {
  if (!catalogPromise) {
    catalogPromise = fetch('models/models.json?v=' + VER).then(r => { if (!r.ok) throw new Error('models.json ' + r.status); return r.json(); });
    catalogPromise.catch(() => { catalogPromise = null; });  // a failed fetch can be retried
  }
  return catalogPromise;
}

function loadFile(file) {
  if (!files.has(file)) {
    const p = loader.loadAsync(file + '?v=' + VER);
    p.catch(() => files.delete(file));  // never cache a failure
    files.set(file, p);
  }
  return files.get(file);
}

const DARK = '#2c2a33', GLASS = '#1d4a5c';

// style: 'solid' (lit PBR, tinted, for the arcade scene), 'runtime' (flat Lambert, the hosts' own
// lighting from assets/guns/runtime-lighting.json) or 'ghost' (see-through cyan ghost controls)
function restyle(root, entry, style, envMap) {
  // tints come from data/guns (guns) or assets/hands (gloves); models without one, like the
  // cabinet controls, keep the colours authored in the GLB
  const tint = entry.tint || {};
  const solid = new Map();
  const solidFor = (name, original) => {
    if (solid.has(name)) return solid.get(name);
    const authored = original && original.color ? '#' + original.color.getHexString() : '#8c8f98';
    const Lit = style === 'runtime' ? THREE.MeshLambertMaterial : THREE.MeshStandardMaterial;
    const pbr = style === 'runtime' ? () => ({}) : extra => extra;
    const make = {
      body: () => new Lit({ color: tint.body || authored, ...pbr({ roughness: 0.42, metalness: 0.08, envMap, envMapIntensity: 0.5 }) }),
      accent: () => new Lit({ color: tint.accent || authored, ...pbr({ roughness: 0.5, metalness: 0.15, envMap, envMapIntensity: 0.4 }) }),
      dark: () => new Lit({ color: entry.tint ? DARK : authored, ...pbr({ roughness: 0.6, metalness: 0.05, envMap, envMapIntensity: 0.3 }) }),
      glass: () => new Lit({ color: GLASS, ...pbr({ roughness: 0.1, metalness: 0.2, emissive: '#2fd2ff', emissiveIntensity: 0.25, envMap, envMapIntensity: 1 }) }),
    }[name] || (() => new Lit({ color: tint.body || authored, ...pbr({ roughness: 0.45, envMap, envMapIntensity: 0.4 }) }));
    solid.set(name, make());
    return solid.get(name);
  };
  const ghost = new THREE.MeshBasicMaterial({
    color: new THREE.Color('#7ff8ff').multiplyScalar(1.6), transparent: true, opacity: 0.32,
    blending: THREE.AdditiveBlending, depthWrite: false,
  });
  // ghost: draw only the nearest see-through surface, so stacked layers don't add up to white
  const depthOnly = new THREE.MeshBasicMaterial({ colorWrite: false });
  const ghosts = [];
  root.traverse(o => {
    // LOD1 copies are for distance in the headset; the site always shows LOD0
    if (/(^|_)lod1$/i.test(o.name) || o.name === 'LOD1') o.visible = false;
    if (!o.isMesh) return;
    // exported names can carry Blender's .001-style suffixes (body.006)
    const name = ((o.material && o.material.name) || 'body').replace(/\.\d+$/, '');
    o.material = style === 'ghost' ? ghost : solidFor(name, o.material);
    if (style === 'ghost') ghosts.push(o);
  });
  for (const o of ghosts) {
    const pre = new THREE.Mesh(o.geometry, depthOnly);
    pre.renderOrder = 1; o.renderOrder = 2;
    o.add(pre);
  }
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
    // a missing hand file leaves the gun without a hand rather than dropping the gun
    const hand = await loadModel('grip-hand-right', { style, envMap }).catch(() => null);
    if (hand) {
      hand.root.position.fromArray(bind.translation || [0, 0, 0]);
      hand.root.quaternion.fromArray(bind.rotation_xyzw || [0, 0, 0, 1]);
      mount.add(hand.root);
    }
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
