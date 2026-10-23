// Background music: plays from 0:00 (the track has its own fade-in) and loops,
// and a bar at the bottom has play/pause, stop, the title (a seek bar on hover), a timer and a
// volume knob (starts at -6 dB). Browsers only allow sound after a click or key press, so the
// music starts on the first one.
(() => {
  const SRC = 'audio/l-von-theme.mp3?v=20261023m';
  const TITLE = 'L-VON · Theme From Aladdin’s Castle';
  const START = 0;             // a fresh start plays from the top; the track fades itself in
  const QUICK_FADE = 0.35;     // seconds, pause/resume/stop, so they do not click
  const DB_MIN = -40, DB_MAX = 0, DB_DEFAULT = -6;
  const store = {
    get(k) { try { return sessionStorage.getItem(k); } catch { return null; } },
    set(k, v) { try { sessionStorage.setItem(k, v); } catch { /* private window */ } },
    getLocal(k) { try { return localStorage.getItem(k); } catch { return null; } },
    setLocal(k, v) { try { localStorage.setItem(k, v); } catch { /* private window */ } },
  };

  const el = new Audio();
  el.preload = 'auto';
  if (/[?&]mute\b/.test(location.search)) el.muted = true;  // silent test runs
  let ctx = null, fadeGain = null, volGain = null;
  let db = Number(store.getLocal('acvr-music-db'));
  if (!Number.isFinite(db) || store.getLocal('acvr-music-db') === null) db = DB_DEFAULT;
  db = Math.min(DB_MAX, Math.max(DB_MIN, db));
  // where to pick up: a page change keeps the position, a first visit starts at the top
  let resumeAt = Number(store.get('acvr-music-at'));
  if (!Number.isFinite(resumeAt) || store.get('acvr-music-at') === null) resumeAt = START;
  let fresh = store.get('acvr-music-at') === null;
  let userStopped = store.get('acvr-music-off') === '1';

  // The host (Cloudflare Pages) ignores range requests, and a streamed file without them cannot
  // seek. So the track is fetched whole (cached for a day) and played from a local copy.
  let loading = null, loaded = false;
  function load() {
    if (!loading) {
      loading = fetch(SRC)
        .then(r => { if (!r.ok) throw new Error('music ' + r.status); return r.blob(); })
        .then(blob => new Promise((resolve, reject) => {
          el.addEventListener('loadedmetadata', resolve, { once: true });
          el.addEventListener('error', reject, { once: true });
          el.src = URL.createObjectURL(blob);
        }))
        .then(() => { loaded = true; });
      loading.catch(() => { loading = null; hideBar(); });
    }
    return loading;
  }

  const gainOf = d => (d <= DB_MIN ? 0 : Math.pow(10, d / 20));
  function graph() {
    if (ctx) return;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;  // no Web Audio: plain element volume, no fades
    ctx = new AC();
    const src = ctx.createMediaElementSource(el);
    fadeGain = ctx.createGain(); fadeGain.gain.value = 0;
    volGain = ctx.createGain(); volGain.gain.value = gainOf(db);
    src.connect(fadeGain).connect(volGain).connect(ctx.destination);
  }
  function fadeTo(target, seconds) {
    if (!ctx) { el.volume = target * gainOf(db); return; }
    const g = fadeGain.gain, now = ctx.currentTime;
    g.cancelScheduledValues(now);
    g.setValueAtTime(g.value, now);
    if (seconds > 0) g.linearRampToValueAtTime(target, now + seconds);
    else g.setValueAtTime(target, now);
  }

  // ---- bar ----
  const bar = document.createElement('div');
  bar.className = 'music';
  bar.setAttribute('role', 'group');
  bar.setAttribute('aria-label', 'Background music');
  bar.innerHTML = `
    <button type="button" class="music-btn music-play" aria-label="Play music">
      <svg viewBox="0 0 16 16" aria-hidden="true"><path class="i-play" d="M4 2.5v11l9-5.5z"/><path class="i-pause" d="M3.5 2.5h3v11h-3zM9.5 2.5h3v11h-3z"/></svg>
    </button>
    <button type="button" class="music-btn music-stop" aria-label="Stop music">
      <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 3.5h9v9h-9z"/></svg>
    </button>
    <span class="music-track">
      <span class="music-title">${TITLE}</span>
      <input class="music-seek" type="range" min="0" max="1" step="0.1" value="${START}" aria-label="Seek in ${TITLE}">
    </span>
    <span class="music-time" aria-label="Track position"><b>0:00</b> / <span>-:--</span></span>
    <div class="music-knob" role="slider" tabindex="0" aria-label="Volume"
         aria-valuemin="${DB_MIN}" aria-valuemax="${DB_MAX}">
      <i class="music-dial"><i></i></i>
    </div>
    <span class="music-db" aria-hidden="true"></span>`;
  document.body.appendChild(bar);
  document.body.classList.add('has-music');
  const playBtn = bar.querySelector('.music-play');
  const stopBtn = bar.querySelector('.music-stop');
  const seek = bar.querySelector('.music-seek');
  const timeNow = bar.querySelector('.music-time b');
  const timeAll = bar.querySelector('.music-time span');
  const knob = bar.querySelector('.music-knob');
  const dial = bar.querySelector('.music-dial');
  const dbText = bar.querySelector('.music-db');
  function hideBar() { bar.hidden = true; document.body.classList.remove('has-music'); }  // no track, no bar

  const clock = s => { s = Math.max(0, Math.floor(s || 0)); return Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0'); };
  let seeking = false;
  function showTime(at = playing ? el.currentTime : resumeAt) {
    timeNow.textContent = clock(at);
    if (!seeking) {
      seek.value = String(at);
      seek.style.setProperty('--fill', `${(100 * at / (Number(seek.max) || 1)).toFixed(2)}%`);
    }
    seek.setAttribute('aria-valuetext', clock(at));
  }
  function showVolume() {
    const t = (db - DB_MIN) / (DB_MAX - DB_MIN);       // 0..1
    dial.style.transform = `rotate(${-135 + 270 * t}deg)`;
    const label = db <= DB_MIN ? 'Off' : (db === 0 ? '0 dB' : `${db.toFixed(0)} dB`);
    dbText.textContent = label;
    knob.setAttribute('aria-valuenow', String(Math.round(db)));
    knob.setAttribute('aria-valuetext', label);
  }
  function setDb(d) {
    db = Math.min(DB_MAX, Math.max(DB_MIN, Math.round(d)));
    if (volGain) volGain.gain.setTargetAtTime(gainOf(db), ctx.currentTime, 0.03);
    else if (playing) el.volume = gainOf(db);
    store.setLocal('acvr-music-db', String(db));
    showVolume();
  }

  // ---- transport ----
  let playing = false, pauseTimer = 0;
  function showState() {
    bar.classList.toggle('is-playing', playing);
    playBtn.setAttribute('aria-label', playing ? 'Pause music' : 'Play music');
  }
  async function play() {
    graph();  // inside the click, so the browser lets the audio start
    if (ctx && ctx.state === 'suspended') ctx.resume().catch(() => {});
    clearTimeout(pauseTimer);
    if (!loaded) { try { await load(); } catch { return false; } }
    if (el.paused) {
      el.currentTime = resumeAt;
      try { await el.play(); } catch { return false; }  // still blocked: wait for a gesture
    }
    playing = true; userStopped = false; store.set('acvr-music-off', '0');
    fadeTo(1, fresh ? 0 : QUICK_FADE);  // straight in from the top, a short ramp on resume
    fresh = false;
    showState();
    return true;
  }
  function pause() {
    playing = false;
    resumeAt = el.currentTime || resumeAt;
    fadeTo(0, QUICK_FADE);
    clearTimeout(pauseTimer);
    pauseTimer = setTimeout(() => el.pause(), QUICK_FADE * 1000 + 30);
    showState(); showTime();
  }
  function stop() {
    pause();
    resumeAt = START; fresh = true;
    userStopped = true; store.set('acvr-music-off', '1');
    store.set('acvr-music-at', String(START));
    showTime();
  }
  playBtn.addEventListener('click', () => { if (playing) { pause(); userStopped = true; store.set('acvr-music-off', '1'); } else play(); });
  stopBtn.addEventListener('click', stop);
  // loop from the top of the track once it ends
  el.addEventListener('ended', () => { el.currentTime = 0; el.play().catch(() => {}); });
  el.addEventListener('timeupdate', () => { if (playing) { showTime(el.currentTime); store.set('acvr-music-at', String(el.currentTime)); } });
  el.addEventListener('loadedmetadata', () => { timeAll.textContent = clock(el.duration); seek.max = String(el.duration); showTime(); });
  addEventListener('pagehide', () => { if (playing) store.set('acvr-music-at', String(el.currentTime)); });

  // ---- seek bar (the title turns into it on hover or focus) ----
  seek.addEventListener('input', () => {
    seeking = true;
    const at = Number(seek.value);
    seek.style.setProperty('--fill', `${(100 * at / (Number(seek.max) || 1)).toFixed(2)}%`);
    timeNow.textContent = clock(at);
  });
  seek.addEventListener('change', () => {
    seeking = false;
    const at = Number(seek.value);
    resumeAt = at; fresh = false;
    store.set('acvr-music-at', String(at));
    if (loaded) el.currentTime = at;
    showTime(at);
  });

  // ---- knob: drag up/down, wheel, or arrow keys ----
  let drag = null;
  knob.addEventListener('pointerdown', e => {
    drag = { y: e.clientY, db };
    knob.setPointerCapture(e.pointerId);
    e.preventDefault();
    knob.focus();  // preventDefault skips the browser's own focus; keys should work after a click
  });
  knob.addEventListener('pointermove', e => { if (drag) setDb(drag.db + (drag.y - e.clientY) / 4); });
  const endDrag = () => { drag = null; };
  knob.addEventListener('pointerup', endDrag);
  knob.addEventListener('pointercancel', endDrag);
  knob.addEventListener('wheel', e => { e.preventDefault(); setDb(db + (e.deltaY < 0 ? 1 : -1)); }, { passive: false });
  knob.addEventListener('keydown', e => {
    const step = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: 6, PageDown: -6 }[e.key];
    if (step) { setDb(db + step); e.preventDefault(); e.stopPropagation(); }
    else if (e.key === 'Home') { setDb(DB_MIN); e.preventDefault(); }
    else if (e.key === 'End') { setDb(DB_MAX); e.preventDefault(); }
  });
  knob.addEventListener('dblclick', () => setDb(DB_DEFAULT));
  // the arcade walks on arrow keys; keep the bar's own keys to itself
  seek.addEventListener('keydown', e => e.stopPropagation());

  // ---- start: right away if the browser allows it, otherwise on the first click or key ----
  function firstGesture(e) {
    if (e && bar.contains(e.target)) return;  // the bar's own controls decide for themselves
    removeEventListener('pointerup', firstGesture, true);
    removeEventListener('keydown', firstGesture, true);
    if (!userStopped) play();
  }
  showVolume(); showTime(); showState();
  const idle = window.requestIdleCallback || (fn => setTimeout(fn, 1500));
  idle(() => load().catch(() => {}), { timeout: 4000 });  // ready by the first click
  if (!userStopped) {
    addEventListener('pointerup', firstGesture, true);
    addEventListener('keydown', firstGesture, true);
    // a direct attempt works where the browser already allows sound for this site
    if (navigator.userActivation ? navigator.userActivation.hasBeenActive : false) play();
  }

  if (/[?&]debug\b/.test(location.search)) window.__music = { el, get ctx() { return ctx; }, get db() { return db; }, setDb, play, pause, stop };
})();
