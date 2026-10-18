'use strict';

document.documentElement.classList.add('js');
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
if ('IntersectionObserver' in window) {
  const observer = new IntersectionObserver(entries => {
    for (const entry of entries) if (entry.isIntersecting) {
      entry.target.classList.add('in-view');
      observer.unobserve(entry.target);
    }
  }, { threshold: .08 });
  document.querySelectorAll('.reveal').forEach(element => observer.observe(element));
} else {
  document.querySelectorAll('.reveal').forEach(element => element.classList.add('in-view'));
}

const initial = [
  [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
  [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0],
  [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
];
let pattern = initial.map(row => [...row]);
const names = ['Бочка', 'Малый', 'Хэт'];
const cells = [];
const sequence = document.getElementById('sequence');
for (let channel = 0; channel < 3; channel++) {
  const lane = document.createElement('div');
  lane.className = 'lane';
  lane.setAttribute('role', 'group');
  lane.setAttribute('aria-label', names[channel]);
  const name = document.createElement('span');
  name.className = 'lane-name';
  name.textContent = names[channel];
  lane.append(name);
  cells[channel] = [];
  for (let step = 0; step < 16; step++) {
    const button = document.createElement('button');
    button.className = 'step';
    button.type = 'button';
    button.setAttribute('aria-label', `${names[channel]}, шаг ${step + 1}`);
    button.setAttribute('aria-pressed', String(!!pattern[channel][step]));
    button.addEventListener('click', () => {
      pattern[channel][step] = pattern[channel][step] ? 0 : 1;
      button.setAttribute('aria-pressed', String(!!pattern[channel][step]));
    });
    button.addEventListener('keydown', event => {
      const movement = { ArrowLeft: [channel, (step + 15) % 16], ArrowRight: [channel, (step + 1) % 16],
        ArrowUp: [(channel + 2) % 3, step], ArrowDown: [(channel + 1) % 3, step] }[event.key];
      if (movement) {
        event.preventDefault();
        cells[movement[0]][movement[1]].focus();
      }
    });
    cells[channel][step] = button;
    lane.append(button);
  }
  sequence.append(lane);
}

const play = document.getElementById('play');
const tempo = document.getElementById('tempo');
const status = document.getElementById('demo-status');
const nativeAudio = document.getElementById('native-demo');
const listen = document.getElementById('listen');
const listenLabel = document.getElementById('listen-label');
let context, master, noise;
let playing = false, starting = false, timer = null, nextTime = 0, nextStep = 0;
let visuals = [], highlightedStep = -1;
let startRequest = 0;

tempo.addEventListener('input', () => { document.getElementById('bpm').value = tempo.value; });
document.getElementById('reset').addEventListener('click', () => {
  pattern = initial.map(row => [...row]);
  cells.forEach((row, channel) => row.forEach((button, step) =>
    button.setAttribute('aria-pressed', String(!!pattern[channel][step]))));
  status.textContent = playing ? 'Ритм вернулся к началу. Продолжай менять его.' : 'Начальный ритм восстановлен. Нажми Play.';
});

async function prepareAudio() {
  if (!context) {
    const Audio = window.AudioContext || window.webkitAudioContext;
    if (!Audio) throw new Error('Web Audio unavailable');
    context = new Audio();
    master = context.createGain();
    master.gain.value = .22;
    master.connect(context.destination);
    noise = context.createBuffer(1, context.sampleRate, context.sampleRate);
    const data = noise.getChannelData(0);
    for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  }
  await context.resume();
  if (context.state !== 'running') throw new Error('Audio context is suspended');
}
function tone(time, frequency, endFrequency, level, duration, type = 'sine') {
  const oscillator = context.createOscillator();
  const gain = context.createGain();
  oscillator.type = type;
  oscillator.frequency.setValueAtTime(frequency, time);
  oscillator.frequency.exponentialRampToValueAtTime(endFrequency, time + Math.min(duration, .12));
  gain.gain.setValueAtTime(.0001, time);
  gain.gain.exponentialRampToValueAtTime(level, time + .003);
  gain.gain.exponentialRampToValueAtTime(.0001, time + duration);
  oscillator.connect(gain).connect(master);
  oscillator.start(time);
  oscillator.stop(time + duration + .01);
  oscillator.onended = () => { oscillator.disconnect(); gain.disconnect(); };
}
function noiseHit(time, cutoff, level, duration, filterType) {
  const source = context.createBufferSource();
  const filter = context.createBiquadFilter();
  const gain = context.createGain();
  source.buffer = noise;
  filter.type = filterType;
  filter.frequency.value = cutoff;
  gain.gain.setValueAtTime(.0001, time);
  gain.gain.exponentialRampToValueAtTime(level, time + .002);
  gain.gain.exponentialRampToValueAtTime(.0001, time + duration);
  source.connect(filter).connect(gain).connect(master);
  source.start(time);
  source.stop(time + duration + .01);
  source.onended = () => { source.disconnect(); filter.disconnect(); gain.disconnect(); };
}
function hit(channel, time, step) {
  if (channel === 0) tone(time, 150, 43, .85, .34);
  if (channel === 1) { noiseHit(time, 1600, .34, .17, 'highpass'); tone(time, 185, 160, .2, .13, 'triangle'); }
  if (channel === 2) noiseHit(time, 6500, step % 4 === 0 ? .17 : .1, .07, 'highpass');
}
function highlight(step) {
  if (step === highlightedStep) return;
  if (highlightedStep >= 0) cells.forEach(row => row[highlightedStep].classList.remove('is-current'));
  if (step >= 0) cells.forEach(row => row[step].classList.add('is-current'));
  highlightedStep = step;
}
function schedule() {
  if (!playing || context.state !== 'running') return;
  const now = context.currentTime;
  // A backgrounded or suspended page never queues a burst of late notes.
  if (nextTime < now - .1) { nextTime = now + .03; visuals = []; }
  while (nextTime < now + .09) {
    pattern.forEach((row, channel) => { if (row[nextStep]) hit(channel, nextTime, nextStep); });
    visuals.push({ step: nextStep, time: nextTime });
    nextTime += 60 / Number(tempo.value) / 4;
    nextStep = (nextStep + 1) % 16;
  }
  while (visuals.length && visuals[0].time <= now) highlight(visuals.shift().step);
}
function stop() {
  ++startRequest;
  playing = false;
  clearInterval(timer);
  timer = null;
  visuals = [];
  highlight(-1);
  if (context) {
    master.gain.cancelScheduledValues(context.currentTime);
    master.gain.setTargetAtTime(.0001, context.currentTime, .008);
    setTimeout(() => { if (!playing && !starting && context.state === 'running') context.suspend().catch(() => {}); }, 80);
  }
  play.setAttribute('aria-pressed', 'false');
  play.setAttribute('aria-label', 'Включить ритм');
  play.querySelector('path').setAttribute('d', 'M9 5L20 12L9 19Z');
  status.textContent = 'Пауза. Твои шаги остаются на месте.';
}
play.addEventListener('click', async () => {
  if (playing) { stop(); return; }
  if (starting) return;
  starting = true;
  const request = ++startRequest;
  play.disabled = true;
  try {
    nativeAudio.pause();
    await prepareAudio();
    if (request !== startRequest || document.hidden) { stop(); return; }
    playing = true;
    master.gain.cancelScheduledValues(context.currentTime);
    master.gain.setTargetAtTime(.22, context.currentTime, .01);
    nextTime = context.currentTime + .05;
    nextStep = 0;
    visuals = [];
    play.setAttribute('aria-pressed', 'true');
    play.setAttribute('aria-label', 'Остановить ритм');
    play.querySelector('path').setAttribute('d', 'M7 5H11V19H7ZM15 5H19V19H15Z');
    status.textContent = 'Звучит. Нажимай на шаги — изменения слышны сразу.';
    schedule();
    timer = setInterval(schedule, 25);
  } catch {
    stop();
    status.textContent = 'Браузер не включил звук. Проверь разрешение на воспроизведение.';
  } finally { starting = false; play.disabled = false; }
});
document.addEventListener('visibilitychange', () => {
  if (document.hidden) { if (playing || starting) stop(); nativeAudio.pause(); }
});
window.addEventListener('pagehide', () => { if (playing || starting) stop(); nativeAudio.pause(); });

listen.addEventListener('click', async () => {
  if (!nativeAudio.paused) { nativeAudio.pause(); return; }
  if (playing || starting) stop();
  listen.disabled = true;
  listenLabel.textContent = 'Включаем проект…';
  try { nativeAudio.volume = .65; await nativeAudio.play(); }
  catch { listenLabel.textContent = 'Не удалось включить запись'; }
  finally { listen.disabled = false; }
});
nativeAudio.addEventListener('play', () => {
  listen.setAttribute('aria-pressed', 'true');
  listenLabel.textContent = 'Остановить проект';
  listen.querySelector('.listen-icon').textContent = 'Ⅱ';
});
function pausedNative() {
  listen.setAttribute('aria-pressed', 'false');
  listenLabel.textContent = 'Послушать проект';
  listen.querySelector('.listen-icon').textContent = '▷';
}
nativeAudio.addEventListener('pause', pausedNative);
nativeAudio.addEventListener('ended', () => { nativeAudio.currentTime = 0; pausedNative(); });
nativeAudio.addEventListener('timeupdate', () => {
  document.getElementById('audio-progress').style.width =
    `${Number.isFinite(nativeAudio.duration) && nativeAudio.duration > 0 ? nativeAudio.currentTime / nativeAudio.duration * 100 : 0}%`;
});

const views = {
  steps: { number: '01', file: 'studio-steps.png', alt: 'OpenDAW: аранжировка и шаговый секвенсор с пятью инструментами', caption: 'Поставь ударные в сетке. Измени акценты. Собери свою основу.' },
  piano: { number: '02', file: 'studio-piano.png', alt: 'OpenDAW: аккорды Velvet keys в piano roll и пять дорожек аранжировки', caption: 'Мелодия и аккорды в piano roll. Ноты, длительность, velocity — всё перед глазами.' },
  mixer: { number: '03', file: 'studio-mixer.png', alt: 'OpenDAW: микшер с пятью каналами, панорамой, low-pass и громкостью', caption: 'Найди баланс. Настрой панораму и громкость, убери лишние частоты фильтром.' },
};
const tabs = [...document.querySelectorAll('[data-view]')];
let viewRequest = 0, currentTransition;
async function showView(button) {
  const view = views[button.dataset.view];
  const request = ++viewRequest;
  const preload = new Image();
  preload.src = `assets/${view.file}`;
  try { await preload.decode(); } catch { return; }
  if (request !== viewRequest) return;
  const update = () => {
    tabs.forEach(tab => { tab.setAttribute('aria-selected', String(tab === button)); tab.tabIndex = tab === button ? 0 : -1; });
    const image = document.getElementById('studio-image');
    image.src = preload.src;
    image.alt = view.alt;
    document.getElementById('studio-panel').setAttribute('aria-labelledby', button.id);
    document.getElementById('caption-number').textContent = view.number;
    document.getElementById('studio-caption').textContent = view.caption;
  };
  if (document.startViewTransition && !reducedMotion.matches) {
    currentTransition?.skipTransition();
    currentTransition = document.startViewTransition(update);
  } else update();
}
tabs.forEach((button, index) => {
  button.addEventListener('click', () => showView(button));
  button.addEventListener('keydown', event => {
    const indexForKey = { ArrowLeft: (index + tabs.length - 1) % tabs.length, ArrowRight: (index + 1) % tabs.length, Home: 0, End: tabs.length - 1 };
    if (event.key in indexForKey) {
      event.preventDefault();
      const next = tabs[indexForKey[event.key]];
      next.focus();
      showView(next);
    }
  });
});
