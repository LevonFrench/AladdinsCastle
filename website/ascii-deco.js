// Animated ASCII on the flow pages, from ascii-magic-art (MIT, copied into ascii/ by
// tools/sync_ascii.py): a divider between sections, and every piece sized to fit its box.
// Each <ascii-art> plays only while on screen and holds still for reduced motion.
const RULES = ['wave-divider', 'circuit-trace', 'stars-divider', 'pulse-line'];
const CELL_W = 0.602;  // a monospace cell is this wide per pixel of font size

document.querySelectorAll('.flow-main > .pipe').forEach((section, i) => {
  if (i === 0) return;
  const rule = document.createElement('div');
  rule.className = 'ascii-rule';
  rule.setAttribute('aria-hidden', 'true');
  rule.innerHTML = `<ascii-art piece="${RULES[(i - 1) % RULES.length]}" data-cols="96" data-max="9"></ascii-art>`;
  section.before(rule);
});

// font size from the box width, in half-pixel steps so small resizes do not restart a piece
function fit() {
  for (const art of document.querySelectorAll('ascii-art[data-cols]')) {
    const box = art.parentElement, cs = getComputedStyle(box);
    const width = box.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight);
    const px = Math.max(3, Math.min(Number(art.dataset.max) || 12, Math.floor((width / (Number(art.dataset.cols) * CELL_W)) * 2) / 2));
    if (art.getAttribute('size') !== String(px)) art.setAttribute('size', String(px));
  }
}
fit();  // before the element is defined, so each piece starts at its final size
import('./ascii/runtime/ascii.js?v=20261025a');
let pending = 0;
addEventListener('resize', () => { clearTimeout(pending); pending = setTimeout(fit, 200); });
