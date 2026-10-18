// Pixel sprites for the How It Works page: each is a small character grid drawn as SVG rects.
(function () {
  const C = {
    Y: '#f6c744', y: '#b8862a', W: '#ffffff', K: '#1a1424', C: '#4ff3ff', c: '#1f8a94', P: '#ff4fa8',
    G: '#9bdc3c', R: '#e0393e', B: '#2d6cdf', O: '#f28a2e', V: '#8b6cff', S: '#c9c9d2', s: '#7d7a8c',
  };
  const S = {
    folder: [
      '............', '.YYYY.......', '.YyyyY......', '.YYYYYYYYYY.', '.YyyyyyyyyY.', '.YyyyyyyyyY.',
      '.YyyyyyyyyY.', '.YyyyyyyyyY.', '.YyyyyyyyyY.', '.YYYYYYYYYY.', '............', '............'],
    scroll: [
      '..WWWWWWWW..', '.WSSSSSSSSW.', '.WSKKKKKKSW.', '.WSSSSSSSSW.', '.WSKKKKKSSW.', '.WSSSSSSSSW.',
      '.WSKKKKKKSW.', '.WSSSSSSSSW.', '.WSKKKSSSSW.', '.WSSSSSSSSW.', '..WWWWWWWW..', '............'],
    shield: [
      '..CCCCCCCC..', '.CccccccccC.', '.CccccccGcC.', '.CcccccGGcC.', '.CcGccGGccC.', '.CcGGGGcccC.',
      '.CccGGccccC.', '..CccccccC..', '...CccccC...', '....CccC....', '.....CC.....', '............'],
    play: [
      '............', '...GG.......', '...GGGG.....', '...GGGGGG...', '...GGGGGGGG.', '...GGGGGGGG.',
      '...GGGGGG...', '...GGGG.....', '...GG.......', '............', '............', '............'],
    headset: [
      '............', '............', '..VVVVVVVV..', '.VVVVVVVVVV.', 'VVCCCVVCCCVV', 'VVCWCVVCWCVV',
      'VVCCCVVCCCVV', '.VVVV..VVVV.', '..VV....VV..', '............', '............', '............'],
    window: [
      '.BBBBBBBBBB.', '.BWWWWWWPWB.', '.BBBBBBBBBB.', '.BKYYKCCKPB.', '.BKYYKCCKPB.', '.BKKKKKKKKB.',
      '.BKGGKRRKOB.', '.BKGGKRRKOB.', '.BBBBBBBBBB.', '............', '............', '............'],
    gear: [
      '.....SS.....', '..S.SSSS.S..', '.SSSSSSSSSS.', '..SSSKKSSS..', '.SSSK..KSSS.', 'SSSK....KSSS',
      'SSSK....KSSS', '.SSSK..KSSS.', '..SSSKKSSS..', '.SSSSSSSSSS.', '..S.SSSS.S..', '.....SS.....'],
    laser: [
      'CC..........', '.CC.........', '..CC........', '...CC.......', '....CSS.....', '.....SSSS...',
      '.....SKSSs..', '......SSSs..', '.......SSs..', '........Ss..', '............', '............'],
    pinball: [
      '............', '....WWWW....', '...WSSSSW...', '..WSSWSSSW..', '..WSSSSSSW..', '...WSSSSW...',
      '....WWWW....', '............', '.PPP....PPP.', '..PPPP.PPPP.', '....PP.PP...', '............'],
    pad: [
      '............', '..BBBBBBBB..', '.BBBBBBBBBB.', 'BBWBBBBBBPBB', 'BWWWBBBBPBPB', 'BBWBBBBBBPBB',
      'BBBBBBBBBBBB', 'BBBB....BBBB', 'BBB......BBB', '............', '............', '............'],
    chip: [
      '..S.S.S.S...', '.KKKKKKKKK..', 'SKVVVVVVVKS.', '.KVCCCCCVK..', 'SKVCKKKCVKS.', '.KVCKKKCVK..',
      'SKVCCCCCVKS.', '.KVVVVVVVK..', '.KKKKKKKKK..', '..S.S.S.S...', '............', '............'],
    link: [
      '............', '..OOOO......', '.O....O.....', '.O....OOOO..', '.O....O...O.', '..OOOOO...O.',
      '......O...O.', '......O...O.', '.......OOO..', '............', '............', '............'],
  };
  function svg(name) {
    const rows = S[name];
    if (!rows) return '';
    const w = Math.max(...rows.map(r => r.length)), h = rows.length;
    let rects = '';
    rows.forEach((row, y) => [...row].forEach((ch, x) => {
      if (C[ch]) rects += `<rect x="${x}" y="${y}" width="1.02" height="1.02" fill="${C[ch]}"/>`;
    }));
    return `<svg viewBox="0 0 ${w} ${h}" shape-rendering="crispEdges" aria-hidden="true">${rects}</svg>`;
  }
  document.querySelectorAll('[data-sprite]').forEach(el => { el.innerHTML = svg(el.dataset.sprite); });
})();
