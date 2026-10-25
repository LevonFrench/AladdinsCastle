// Background music: plays from 0:00 (the track has its own fade-in) and loops,
// and a bar at the bottom has play/pause, stop, the title (a seek bar on hover), a timer and a
// volume knob (starts at -6 dB). Browsers only allow sound after a gesture, so the music starts on
// the first pointer press or Enter/Space (never Tab or other keys), and the 7 MB track is not
// fetched until then (or until Play).
(() => {
  const SRC = '/audio/l-von-theme.mp3?v=20261023m';  // root-absolute: the 404 page loads this from any depth
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
  // silent test runs: the output is cut after the meter, so the EQ still moves (no Web Audio: the element is muted)
  const MUTE = /[?&]mute\b/.test(location.search);
  if (MUTE) el.muted = true;
  let ctx = null, fadeGain = null, volGain = null, meter = null;
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
        .then(() => { loaded = true; showFailed(false); });
      // a failed fetch keeps the bar: Play tries again
      loading.catch(() => { loading = null; showFailed(true); });
    }
    return loading;
  }

  const gainOf = d => (d <= DB_MIN ? 0 : Math.pow(10, d / 20));
  function graph() {
    if (ctx) return;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;  // no Web Audio: plain element volume, no fades
    try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch { /* Safari 16.4+: sound even with the ringer switch on */ }
    ctx = new AC();
    ctx.addEventListener('statechange', wake);
    const src = ctx.createMediaElementSource(el);
    fadeGain = ctx.createGain(); fadeGain.gain.value = 0;
    volGain = ctx.createGain(); volGain.gain.value = gainOf(db);
    const out = ctx.createGain(); out.gain.value = MUTE ? 0 : 1;
    meter = ctx.createAnalyser(); meter.fftSize = 512; meter.smoothingTimeConstant = 0.6;
    meter.minDecibels = -70; meter.maxDecibels = -20;  // headroom so loud bands are not pinned at the top
    src.connect(fadeGain).connect(volGain).connect(out).connect(ctx.destination);
    fadeGain.connect(meter);  // before the volume knob, so the EQ shows the music, not the level
    el.muted = false;          // the graph carries the sound now (and cuts it when MUTE)
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
    <span class="music-eq" aria-hidden="true">▁▁▁▁▁▁▁▁</span>
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
  const eq = bar.querySelector('.music-eq');
  const title = bar.querySelector('.music-title');
  function showFailed(on) {
    bar.classList.toggle('is-failed', on);
    title.textContent = on ? 'Music did not load · press Play to retry' : TITLE;
  }

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

  // ---- EQ: eight bands of the track, drawn in block characters ----
  const BARS = ' ▁▂▃▄▅▆▇█', EDGES = [1, 2, 4, 7, 12, 20, 34, 58, 100];  // FFT bins, ~94 Hz each
  const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
  let eqTimer = 0, bins = null;
  function drawEq() {
    bins = bins || new Uint8Array(meter.frequencyBinCount);
    meter.getByteFrequencyData(bins);
    let line = '';
    for (let b = 0; b < 8; b++) {
      let peak = 0;
      for (let i = EDGES[b]; i < EDGES[b + 1]; i++) peak = Math.max(peak, bins[i]);
      line += BARS[Math.max(1, Math.round((peak / 255) * 8))];
    }
    eq.textContent = line;
  }
  function eqRun(on) {
    clearInterval(eqTimer);
    if (on && meter && !reduce) eqTimer = setInterval(drawEq, 50);
    else eq.textContent = '▁▁▁▁▁▁▁▁';
  }

  // ---- transport ----
  let playing = false, pauseTimer = 0;
  function showState() {
    bar.classList.toggle('is-playing', playing);
    playBtn.setAttribute('aria-label', playing ? 'Pause music' : 'Play music');
  }
  async function play() {
    graph();  // inside the click, so the browser lets the audio start
    if (ctx && ctx.state !== 'running') ctx.resume().catch(() => {});  // 'suspended', or 'interrupted' on iOS
    clearTimeout(pauseTimer);
    if (!loaded) { try { await load(); } catch { return false; } }
    if (el.paused) {
      el.currentTime = resumeAt;
      // after the download the gesture may be spent (iOS): the caller re-arms the gesture listeners
      try { await el.play(); } catch { return false; }
    }
    playing = true; userStopped = false; store.set('acvr-music-off', '0');
    disarm();
    fadeTo(1, fresh ? 0 : QUICK_FADE);  // straight in from the top, a short ramp on resume
    fresh = false;
    showState(); eqRun(true);
    return true;
  }
  function pause() {
    playing = false;
    resumeAt = el.currentTime || resumeAt;
    fadeTo(0, QUICK_FADE);
    clearTimeout(pauseTimer);
    pauseTimer = setTimeout(() => { el.pause(); eqRun(false); }, QUICK_FADE * 1000 + 30);
    showState(); showTime();
  }
  function stop() {
    pause();
    resumeAt = START; fresh = true;
    userStopped = true; store.set('acvr-music-off', '1');
    store.set('acvr-music-at', String(START));
    showTime();
  }
  playBtn.addEventListener('click', () => {
    if (playing) { pause(); userStopped = true; store.set('acvr-music-off', '1'); }
    else play().then(ok => { if (!ok) arm(); });  // blocked after the load: the next gesture plays from the local copy
  });
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

  // ---- start: on the first pointer press or Enter/Space, never Tab or other keys ----
  // A phone or screen-reader user tabbing to the skip link must not get music over the speech.
  function firstGesture(e) {
    if (e && bar.contains(e.target)) return;  // the bar's own controls decide for themselves
    if (e && e.type === 'keydown' && (e.repeat || e.ctrlKey || e.altKey || e.metaKey || (e.key !== 'Enter' && e.key !== ' '))) return;
    disarm();
    if (userStopped) return;
    // listeners stay off only if the music really started; if it did not (slow load, iOS dropped
    // the activation across the download), the next gesture plays from the finished local copy
    play().then(ok => { if (!ok && !userStopped) arm(); });
  }
  let isArmed = false;
  function arm() {
    if (isArmed) return;
    isArmed = true;
    addEventListener('pointerup', firstGesture, true);
    addEventListener('keydown', firstGesture, true);
  }
  function disarm() {
    isArmed = false;
    removeEventListener('pointerup', firstGesture, true);
    removeEventListener('keydown', firstGesture, true);
  }
  // the audio context can stop under us (phone call, lock, tab switch: 'interrupted' on iOS)
  function wake() {
    if (ctx && playing && ctx.state !== 'running') ctx.resume().catch(() => {});
  }
  document.addEventListener('visibilitychange', () => { if (!document.hidden) wake(); });
  addEventListener('pointerup', wake, true);  // a resume refused outside a gesture succeeds inside one
  showVolume(); showTime(); showState();
  if (!userStopped) {
    arm();
    // already playing in this tab (page change) and the browser keeps the site's activation: carry on
    if (store.get('acvr-music-off') === '0' && navigator.userActivation && navigator.userActivation.hasBeenActive) {
      play();
    }
  }

  if (/[?&]debug\b/.test(location.search)) window.__music = { el, get ctx() { return ctx; }, get eq() { return eq.textContent; }, get db() { return db; }, setDb, play, pause, stop };
})();
